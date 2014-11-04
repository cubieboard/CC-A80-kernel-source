#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/semaphore.h>
#include <linux/init-input.h>

#include <linux/inv_sensors.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND) || defined(CONFIG_PM)
#include <linux/pm.h>
#endif

#include "../../iio.h"
#include "../../buffer.h"
#include "../../trigger.h"
#include "../../kfifo_buf.h"
#include "../../sysfs.h"

#include "inv_sensors_common.h"
#include "inv_sensors_control.h"
#include "inv_sensors_sm.h"
#include "inv_sensors_dts.h"
#include "inv_sensors_buffer.h"


#define INVSENS_SELFTEST_BUF_LEN 256
#define INVSENS_IIO_BUFFER_DATUM 8

#define INVSENS_ACCEL_CAL_FILENAME "/data/accel_bias.dat"


struct invsens_iio_data {
	struct iio_dev *indio_dev;

	int irq;
	const char *device_name;
	struct invsens_i2c_t i2c_handler;

	struct invsens_platform_data_t *platform_data;
	struct invsens_sm_data_t sm_data;
	struct invsens_data_list_t data_list;

	/*variables for factory mode*/
	struct delayed_work fso_work;
	bool fso_enabled;

	short accel_cache[INVSENS_AXIS_NUM];
	short gyro_cache[INVSENS_AXIS_NUM];

	bool accel_is_calibrated;
	short accel_bias[INVSENS_AXIS_NUM];

	bool motion_alert_is_occured;

	/*log level*/
	u32 log_level;
	u8  suspend_data;

	struct delayed_work sync_work;	
	struct work_struct  work;
	struct semaphore sema;
	
	
	
#ifdef CONFIG_HAS_EARLYSUSPEND	
	struct early_suspend early_suspend;
#endif
};

static struct workqueue_struct *inv_wq;
static unsigned short normal_i2c[] = { 0x68, I2C_CLIENT_END };
static unsigned short i2c_address[2] = {0x68, 0x69};
static unsigned short addr = 0;
static struct sensor_config_info sensor_info = {
	.input_type = GSENSOR_TYPE,
	.int_number = 0,
	.ldo = NULL,
};
bool suspend_flag = false;
struct mutex suspend_mutex;

static int invsens_store_accel_cal(const short accel_bias[])
{
	int res = SM_SUCCESS;
	struct file *fp;
	mm_segment_t old_fs;
	loff_t pos = 0;

	INV_DBG_FUNC_NAME;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open file to write */
	fp = filp_open(INVSENS_ACCEL_CAL_FILENAME, O_WRONLY|O_CREAT,
		S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(fp)) {
		INVSENS_LOGE("%s: open file error\n", __FUNCTION__);
		res = -1;
		goto exit;
	}

	/* Write buf to file */
	vfs_write(fp, (void *)accel_bias, sizeof(short) * INVSENS_AXIS_NUM, &pos);

exit:

	/* close file before return */
	if (fp)
		filp_close(fp, NULL);
	/* restore previous address limit */
	set_fs(old_fs);

	return res;
}

static int invsens_load_accel_cal(short accel_bias[])
{
	int res = SM_SUCCESS;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	//loff_t pos = 0;

	INV_DBG_FUNC_NAME;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	/* open file to read */
	fp = filp_open(INVSENS_ACCEL_CAL_FILENAME, O_RDONLY,
		S_IRUGO | S_IWUSR | S_IWGRP);
	if (IS_ERR(fp)) {
		INVSENS_LOGE("invsens_load_accel_cal : fp = %d\n", (int) -PTR_ERR(fp));
		res = -1;
		fp = NULL;
		goto exit;
	}

	/* Write buf to file */
	vfs_read(fp, (void *)accel_bias,
			sizeof(short) * INVSENS_AXIS_NUM, &fp->f_pos);

	INVSENS_LOGD("accel bias : %d %d %d res=%d\n", accel_bias[0],
		accel_bias[1], accel_bias[2], res);

exit:
	/* close file before return */
	if (fp)
		filp_close(fp, NULL);

	/* restore previous address limit */
	set_fs(old_fs);

	return res;
}


static int invsens_execute_ccfg(struct invsens_iio_data *iio_data,
	struct invsens_control_data_t *cmd_data)
{
	int res = SM_SUCCESS;
	struct invsens_ioctl_param_t ioctl_data;

	INV_DBG_FUNC_NAME;


	if(cmd_data->config == INV_CCFG_WDGB) {
		ioctl_data.wdgb.bias[0] = cmd_data->params[0];
		ioctl_data.wdgb.bias[1] = cmd_data->params[1];
		ioctl_data.wdgb.bias[2] = cmd_data->params[2];
		res = invsens_sm_ioctl(&iio_data->sm_data,
			INV_IOCTL_SET_GYRO_DMP_BIAS, 0, &ioctl_data);
	} else if(cmd_data->config == INV_CCFG_WDAB) {
		ioctl_data.wdab.bias[0] = cmd_data->params[0];
		ioctl_data.wdab.bias[1] = cmd_data->params[1];
		ioctl_data.wdab.bias[2] = cmd_data->params[2];
		res = invsens_sm_ioctl(&iio_data->sm_data,
			INV_IOCTL_SET_ACCEL_DMP_BIAS, 0, &ioctl_data);
	}

	return res;
}

/**
	command calback
*/
static int invsens_cmd_callback(void *user_data,
				struct invsens_control_data_t *cmd_data)
{
	int result = 0;
	struct invsens_iio_data *iio_data =
	    (struct invsens_iio_data *) user_data;

	INV_DBG_FUNC_NAME;

	switch (cmd_data->control_type) {
	case INV_CONTROL_FLUSH:
		break;

	case INV_CONTROL_SYNC:
		down(&iio_data->sema);
		result =
		    invsens_sm_sync(&iio_data->sm_data, cmd_data->key);
		up(&iio_data->sema);
		
		/*load bias from calibration file*/
		if(invsens_load_accel_cal(iio_data->accel_bias)==SM_SUCCESS)
			iio_data->accel_is_calibrated = true;
			
		break;				
	case INV_CONTROL_ENABLE_SENSOR:
		down(&iio_data->sema);
		result =
		    invsens_sm_enable_sensor(&iio_data->sm_data,
			     cmd_data->func, (cmd_data->delay == 0) ? false : true,
			     cmd_data->delay);
		up(&iio_data->sema);
		break;

	case INV_CONTROL_BATCH:
		down(&iio_data->sema);
		result =
		    invsens_sm_batch(&iio_data->sm_data, cmd_data->func,
				     cmd_data->delay, cmd_data->timeout);
		up(&iio_data->sema);
		break;
	case INV_CONTROL_CONFIG:
		down(&iio_data->sema);
		result = invsens_execute_ccfg(iio_data, cmd_data);
		up(&iio_data->sema);
		break;
	default:
		break;

	}
	return 0;
}


static ssize_t invsens_control_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int result = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	INVSENS_LOGD("%s : %s\n", __func__, buf);

	result = invsens_parse_control(buf, iio_data, invsens_cmd_callback);

	return strlen(buf);
}


static ssize_t invsens_log_level_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", invsens_get_log_mask());
}

static ssize_t invsens_log_level_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int result = 0;
	long log_mask;


	result = kstrtol(buf, 0, &log_mask);

	invsens_set_log_mask((u32)log_mask);

	return strlen(buf);
}

static ssize_t invsens_reg_dump_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	u16 i;

	char tmp[5] = { 0 };
	u8 d;
	down(&iio_data->sema);
	for (i = 0; i < 0x80; i++) {
		if (i == 0x74)
			d = 0;
		else
			inv_i2c_read(&iio_data->i2c_handler,
				     iio_data->sm_data.board_cfg.i2c_addr, i, 1,
				     &d);

		sprintf(tmp, "%02X,", d);
		strcat(buf, tmp);
		if (!((i + 1) % 16))
			strcat(buf, "\n");
	}
	up(&iio_data->sema);
	return strlen(buf);
}



static ssize_t invsens_swst_gyro_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int res = 0, value = 0;

	down(&iio_data->sema);
	res =
	    invsens_sm_ioctl(&iio_data->sm_data,
			     INV_IOCTL_GYRO_SW_SELFTEST, 0, (void *) &ioctl_data);
	up(&iio_data->sema);


	if (res == SM_IOCTL_HANDLED) {
		value = 1;
		sprintf(buf, "%d,%ld,%ld,%ld,%ld,%ld,%ld,%d\n",
			ioctl_data.swst_gyro.result, ioctl_data.swst_gyro.bias[0], ioctl_data.swst_gyro.bias[1],
			ioctl_data.swst_gyro.bias[2], ioctl_data.swst_gyro.rms[0], ioctl_data.swst_gyro.rms[1],
			ioctl_data.swst_gyro.rms[2], ioctl_data.swst_gyro.packet_cnt);
	} else
		sprintf(buf, "0");


	return strlen(buf);

}

static ssize_t invsens_hwst_accel_show(struct device *dev, struct device_attribute
					      *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int res = 0, value = 0;

	down(&iio_data->sema);
	res =
	    invsens_sm_ioctl(&iio_data->sm_data,
			     INV_IOCTL_ACCEL_HW_SELFTEST, 0, (void *) &ioctl_data);

	up(&iio_data->sema);

	if (res == SM_IOCTL_HANDLED) {
		value = 1;
		sprintf(buf, "%d,%d,%d,%d\n",
			ioctl_data.hwst_accel.result, ioctl_data.hwst_accel.ratio[0],
			ioctl_data.hwst_accel.ratio[1], ioctl_data.hwst_accel.ratio[2]);
	} else
		sprintf(buf, "0");


	return strlen(buf);
}

static ssize_t invsens_hwst_gyro_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int res = 0, value = 0;

	down(&iio_data->sema);
	res =
	    invsens_sm_ioctl(&iio_data->sm_data,
			     INV_IOCTL_GYRO_HW_SELFTEST, 0, (void *) &ioctl_data);
	up(&iio_data->sema);


	if (res == SM_IOCTL_HANDLED) {
		value = 1;
		sprintf(buf, "%d,%d,%d,%d\n",
			ioctl_data.hwst_gyro.result, ioctl_data.hwst_gyro.ratio[0],
			ioctl_data.hwst_gyro.ratio[1], ioctl_data.hwst_gyro.ratio[2]);
	} else
		sprintf(buf, "0");

	return strlen(buf);

}


static ssize_t invsens_swst_compass_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int res = 0, value = 0;

	down(&iio_data->sema);

	res =
	    invsens_sm_ioctl(&iio_data->sm_data,
			     INV_IOCTL_COMPASS_SELFTEST, 0, (void *) &ioctl_data);
	if (res == SM_IOCTL_HANDLED)
		value = 1;

	up(&iio_data->sema);

	return sprintf(buf, "%d\n", value);
}


static ssize_t invsens_temperature_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	int res;
	struct invsens_ioctl_param_t ioctl_data;

	down(&iio_data->sema);
	res =
	    invsens_sm_ioctl(&iio_data->sm_data,
			     INV_IOCTL_GET_TEMP, 0, (void *) &ioctl_data);
	if(res == SM_IOCTL_HANDLED)
		sprintf(buf, "%d\n", ioctl_data.temperature.temp);

	up(&iio_data->sema);
	return strlen(buf);
}


static ssize_t invsens_gyro_is_on_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	bool is_on;

	down(&iio_data->sema);
	is_on =
 	    invsens_sm_is_func_enabled(&iio_data->sm_data, INV_FUNC_GYRO);
	sprintf(buf, "%d\n", is_on);

	up(&iio_data->sema);

	return strlen(buf);
}

static ssize_t invsens_stiction_test_mode_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int stiction_mode = 1;
	int res = 0;

	pr_info("[INV] %s stiction test mode on = %d.\n", __func__, stiction_mode);
	down(&iio_data->sema);
	res = invsens_sm_ioctl(&iio_data->sm_data,
				INV_IOCTL_GET_STICTION_TESTMODE,
				0, (void *) &ioctl_data);

	pr_info("[INV] %s node data = %d\n", __func__, ioctl_data.u32d[0]);
#if 1 //rocky	
	pr_info("[INV] %s node data = %d,accel_rate=%d,gyro_rate=%d,gyro_counter=%d\n", __func__, ioctl_data.u32d[0],
		ioctl_data.u32d[1],ioctl_data.u32d[2],ioctl_data.u32d[3]);
#endif	

	up(&iio_data->sema);
	

		sprintf(buf,"testmode =%d, accel_rate = %d, gyro_rate = %d, gyro_counter = %d, accel_count=%d,gyro_count=%d\n",
		ioctl_data.u32d[0],ioctl_data.u32d[1],ioctl_data.u32d[2],ioctl_data.u32d[3],ioctl_data.u32d[4],ioctl_data.u32d[5]);

	return strlen(buf);
}

static ssize_t invsens_stiction_test_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int result = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int stiction_mode = 0;
	int res = 0;

	down(&iio_data->sema);
	result = kstrtoint(buf, 0, &stiction_mode);
	pr_info("[INV] %s stiction_mode = %d.\n", __func__, stiction_mode);
	res = invsens_sm_ioctl(&iio_data->sm_data,
				INV_IOCTL_SET_STICTION_TESTMODE,
				stiction_mode, (void *) &ioctl_data);
	up(&iio_data->sema);

	return strlen(buf);
}

#if 1 //rocky


static ssize_t invsens_dmp_address_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	//int dmp_address = 1;
	int res = 0;

	down(&iio_data->sema);
	res = invsens_sm_ioctl(&iio_data->sm_data,
				INV_IOCTL_GET_DMP_ADDRESS,
				0, (void *) &ioctl_data);

	up(&iio_data->sema);
	sprintf(buf,"dmp_address =%d\n",ioctl_data.u32d[0]);
	return strlen(buf);
}

static ssize_t invsens_dmp_address_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int result = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int dmp_address = 0;
	int res = 0;

	down(&iio_data->sema);
	result = kstrtoint(buf, 0, &dmp_address);
	pr_info("[INV] %s dmp_address = %d.\n", __func__, dmp_address);
	res = invsens_sm_ioctl(&iio_data->sm_data,
				INV_IOCTL_SET_DMP_ADDRESS,
				dmp_address, (void *) &ioctl_data);
	up(&iio_data->sema);

	return strlen(buf);
}


static ssize_t invsens_dmp_data_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int res = 0;

	down(&iio_data->sema);
	res = invsens_sm_ioctl(&iio_data->sm_data,
				INV_IOCTL_GET_DMP_DATA,
				0, (void *) &ioctl_data);

	up(&iio_data->sema);
	sprintf(buf,"dmp_data =%d\n",ioctl_data.u32d[0]);
	return strlen(buf);
}

static ssize_t invsens_dmp_data_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int result = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	int dmp_data = 0;
	int res = 0;

	down(&iio_data->sema);
	result = kstrtoint(buf, 0, &dmp_data);
	pr_info("[INV] %s dmp_data = %d.\n", __func__, dmp_data);
	res = invsens_sm_ioctl(&iio_data->sm_data,
				INV_IOCTL_SET_DMP_DATA,
				dmp_data, (void *) &ioctl_data);
	up(&iio_data->sema);

	return strlen(buf);
}

#endif


static ssize_t invsens_s_gyro_selftest_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	int res;
	struct invsens_ioctl_param_t ioctl_data;

	down(&iio_data->sema);

	res =
	    invsens_sm_ioctl(&iio_data->sm_data,
			     INV_IOCTL_S_GYRO_SELFTEST, 0, (void *) &ioctl_data);
	up(&iio_data->sema);

	if (res == SM_IOCTL_HANDLED)
		sprintf(buf, "%d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%03d,%d.%03d,%d.%03d,"
			"%d.%01d,%d.%01d,%d.%01d,"
			"%d.%03d,%d.%03d,%d.%03d\n",
			ioctl_data.s_gyro_selftest.result,
			(int)abs(ioctl_data.s_gyro_selftest.gyro_bias[0] / 1000),
			(int)abs(ioctl_data.s_gyro_selftest.gyro_bias[0]) % 1000,
			(int)abs(ioctl_data.s_gyro_selftest.gyro_bias[1] / 1000),
			(int)abs(ioctl_data.s_gyro_selftest.gyro_bias[1]) % 1000,
			(int)abs(ioctl_data.s_gyro_selftest.gyro_bias[1] / 1000),
			(int)abs(ioctl_data.s_gyro_selftest.gyro_bias[1]) % 1000,
			(int)ioctl_data.s_gyro_selftest.gyro_rms[0]/1000,
			(int)ioctl_data.s_gyro_selftest.gyro_rms[0] % 1000,
			(int)ioctl_data.s_gyro_selftest.gyro_rms[1]/ 1000,
			(int)ioctl_data.s_gyro_selftest.gyro_rms[1] % 1000,
			(int)ioctl_data.s_gyro_selftest.gyro_rms[2]/1000,
			(int)ioctl_data.s_gyro_selftest.gyro_rms[2] % 1000,
			ioctl_data.s_gyro_selftest.gyro_ratio[0]/10,
			ioctl_data.s_gyro_selftest.gyro_ratio[0]%10,
			ioctl_data.s_gyro_selftest.gyro_ratio[1]/10,
			ioctl_data.s_gyro_selftest.gyro_ratio[1]%10,
			ioctl_data.s_gyro_selftest.gyro_ratio[2]/10,
			ioctl_data.s_gyro_selftest.gyro_ratio[2]%10,
			ioctl_data.s_gyro_selftest.packet_cnt,0,
			ioctl_data.s_gyro_selftest.packet_cnt,0,
			ioctl_data.s_gyro_selftest.packet_cnt,0
		);
	else
		sprintf(buf, "0\n");

	return strlen(buf);
}

static ssize_t invsens_s_accel_selftest_show(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	int res;
	struct invsens_ioctl_param_t ioctl_data;

	down(&iio_data->sema);

	res =
	    invsens_sm_ioctl(&iio_data->sm_data,
			     INV_IOCTL_S_ACCEL_SELFTEST, 0, (void *) &ioctl_data);

	up(&iio_data->sema);

	if (res == SM_IOCTL_HANDLED)
		sprintf(buf, "%d,"
			"%d.%01d,%d.%01d,%d.%01d\n",
			ioctl_data.s_accel_selftest.result,
			ioctl_data.s_accel_selftest.accel_ratio[0]/10,
			ioctl_data.s_accel_selftest.accel_ratio[0]%10,
			ioctl_data.s_accel_selftest.accel_ratio[1]/10,
			ioctl_data.s_accel_selftest.accel_ratio[1]%10,
			ioctl_data.s_accel_selftest.accel_ratio[2]/10,
			ioctl_data.s_accel_selftest.accel_ratio[2]%10
		);
	else
		sprintf(buf, "0\n");

	return strlen(buf);
}



static ssize_t invsens_s_accel_cal_start_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int res;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct invsens_ioctl_param_t ioctl_data;
	long value;

	res = kstrtol(buf, 0, &value);

	if(value == 1) {
		down(&iio_data->sema);
		res =
		    invsens_sm_ioctl(&iio_data->sm_data,
				     INV_IOCTL_DO_ACCEL_CAL, 0, (void *) &ioctl_data);
		up(&iio_data->sema);

		memcpy(iio_data->accel_bias, ioctl_data.accel_bias.bias,
			sizeof(ioctl_data.accel_bias.bias));
				
		invsens_store_accel_cal(iio_data->accel_bias);	
			
		iio_data->accel_is_calibrated = true;
	} else if(value == 0)
		iio_data->accel_is_calibrated = false;

	return strlen(buf);
}


static ssize_t invsens_s_accel_cal_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	if(iio_data->accel_is_calibrated)
		sprintf(buf, "%d,%d,%d\n", iio_data->accel_bias[0], iio_data->accel_bias[1],
			iio_data->accel_bias[2]);
	else
		sprintf(buf, "0,0,0\n");

	return strlen(buf);
}


static ssize_t invsens_motion_interrupt_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", iio_data->motion_alert_is_occured);
}

static ssize_t invsens_motion_interrupt_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int res = 0, enable;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	down(&iio_data->sema);
	res = kstrtoint(buf, 0, &enable);
	/*
		mode of motion interrupt
			0. disable motion interrupt
			1. normal motion interrupt
			2. probe interrupt pin
	*/

	if(enable > 0) {
		struct invsens_ioctl_param_t ioctl_data;

		if(enable == 1)
			ioctl_data.data[0] = 0xc;
		else if(enable == 2)
			ioctl_data.data[0] = 0x0;
		res =
		    invsens_sm_ioctl(&iio_data->sm_data,
				     INV_IOCTL_SET_WOM_THRESH, 0, (void *) &ioctl_data);		
	}

	res = invsens_sm_enable_sensor(&iio_data->sm_data,
		INV_FUNC_MOTION_INTERRUPT, (bool)enable, SM_DELAY_DEFAULT);

	up(&iio_data->sema);

	iio_data->motion_alert_is_occured = false;

	return strlen(buf);
}

static void invsens_delayed_work_fso(struct work_struct *work)
{
	struct invsens_iio_data *iio_data = container_of(work,
		struct invsens_iio_data, fso_work.work);
	//struct iio_dev *indio_dev = iio_data->indio_dev;

	int res;
	down(&iio_data->sema);

	res = invsens_sm_enable_sensor(&iio_data->sm_data,
			INV_FUNC_FT_SENSOR_ON, false, SM_DELAY_DEFAULT);
	iio_data->fso_enabled = false;
	up(&iio_data->sema);
}

static void invsens_delayed_work_sync(struct work_struct *work)
{
	bool is_on = false;
	struct invsens_iio_data *iio_data = container_of(work,
	struct invsens_iio_data,sync_work.work);
	
	if(usermodehelper_read_trylock()) {
		cancel_delayed_work(&iio_data->sync_work);
		schedule_delayed_work(&iio_data->sync_work, msecs_to_jiffies(200));
   		return ;
   	}else{
		usermodehelper_read_unlock();
   	}
	
	down(&iio_data->sema);

	invsens_sm_sync(&iio_data->sm_data, "y8u1poDy8i9ozu2ev2OthRNKw2898S82");
	is_on = invsens_sm_is_func_enabled(&iio_data->sm_data, INV_FUNC_GYRO);
	if(is_on)
		invsens_sm_enable_sensor(&iio_data->sm_data,INV_FUNC_GYRO, true, SM_DELAY_DEFAULT);
	is_on = invsens_sm_is_func_enabled(&iio_data->sm_data, INV_FUNC_ACCEL);
	if(is_on)
		invsens_sm_enable_sensor(&iio_data->sm_data,INV_FUNC_ACCEL, true, SM_DELAY_DEFAULT);
	
	up(&iio_data->sema);
}

static ssize_t invsens_accel_raw_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	int res = SM_SUCCESS;

	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	/*factory mode on*/
	if(!iio_data->fso_enabled) {
		down(&iio_data->sema);
		res = invsens_sm_enable_sensor(&iio_data->sm_data,
				INV_FUNC_FT_SENSOR_ON, true, SM_DELAY_FTMODE);
		iio_data->fso_enabled = true;
		up(&iio_data->sema);
	}

	if(!res) {
		cancel_delayed_work(&iio_data->fso_work);
		schedule_delayed_work(&iio_data->fso_work, msecs_to_jiffies(2000));

		/*read data from cache*/
		sprintf(buf, "%d,%d,%d\n", iio_data->accel_cache[0],
				iio_data->accel_cache[1], iio_data->accel_cache[2]);		
	}

	return strlen(buf);
}

static ssize_t invsens_gyro_raw_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	int res = SM_SUCCESS;

	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	//struct invsens_ioctl_param_t ioctl_data;

	if(!iio_data->fso_enabled) {
		down(&iio_data->sema);

		res = invsens_sm_enable_sensor(&iio_data->sm_data,
				INV_FUNC_FT_SENSOR_ON, true, SM_DELAY_DEFAULT);
		iio_data->fso_enabled = true;
		up(&iio_data->sema);
	}

	
	if(!res) {
		cancel_delayed_work(&iio_data->fso_work);
		schedule_delayed_work(&iio_data->fso_work, msecs_to_jiffies(2000));
		sprintf(buf, "%d,%d,%d\n", iio_data->gyro_cache[0],
				iio_data->gyro_cache[1], iio_data->gyro_cache[2]);
	}

	return strlen(buf);
}




static IIO_DEVICE_ATTR(control, S_IRUGO, NULL, invsens_control_store, 0);
static IIO_DEVICE_ATTR(reg_dump, S_IRUGO, invsens_reg_dump_show, NULL, 0);
static IIO_DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR,
		       invsens_log_level_show, invsens_log_level_store, 0);
static IIO_DEVICE_ATTR(swst_gyro, S_IRUGO | S_IWUSR,
		       invsens_swst_gyro_show, NULL, 0);
static IIO_DEVICE_ATTR(hwst_accel, S_IRUGO | S_IWUSR,
		       invsens_hwst_accel_show, NULL, 0);
static IIO_DEVICE_ATTR(hwst_gyro, S_IRUGO | S_IWUSR,
		       invsens_hwst_gyro_show, NULL, 0);
static IIO_DEVICE_ATTR(swst_compass, S_IRUGO | S_IWUSR,
		       invsens_swst_compass_show, NULL, 0);
static IIO_DEVICE_ATTR(motion_interrupt, S_IRUGO | S_IWUSR,
		       invsens_motion_interrupt_show,
		       invsens_motion_interrupt_store, 0);
static IIO_DEVICE_ATTR(accel_raw, S_IRUGO | S_IWUSR,
		       invsens_accel_raw_show, NULL, 0);
static IIO_DEVICE_ATTR(gyro_raw, S_IRUGO | S_IWUSR,
		       invsens_gyro_raw_show, NULL, 0);
static IIO_DEVICE_ATTR(temperature, S_IRUGO | S_IWUSR,
		       invsens_temperature_show, NULL, 0);
static IIO_DEVICE_ATTR(s_gyro_selftest, S_IRUGO | S_IWUSR,
		       invsens_s_gyro_selftest_show, NULL, 0);
static IIO_DEVICE_ATTR(s_accel_selftest, S_IRUGO | S_IWUSR,
		       invsens_s_accel_selftest_show, NULL, 0);
static IIO_DEVICE_ATTR(s_accel_cal, S_IRUGO | S_IWUSR,
		       invsens_s_accel_cal_show, NULL, 0);
static IIO_DEVICE_ATTR(s_accel_cal_start, S_IRUGO | S_IWUSR,
		       NULL, invsens_s_accel_cal_start_store, 0);
static IIO_DEVICE_ATTR(gyro_is_on, S_IRUGO | S_IWUSR,
		       invsens_gyro_is_on_show, NULL, 0);
static IIO_DEVICE_ATTR(stiction_test_mode, S_IRUGO | S_IWUSR,
		       invsens_stiction_test_mode_show,
		       invsens_stiction_test_mode_store, 0);
//static IIO_DEVICE_ATTR(false_z, S_IRUGO | S_IWUSR,
//			invsens_false_z_show, invsens_false_z_store, 0);


#if 1 //rocky
static IIO_DEVICE_ATTR(dmp_address, S_IRUGO | S_IWUSR,
		       invsens_dmp_address_show, invsens_dmp_address_store, 0);
static IIO_DEVICE_ATTR(dmp_data, S_IRUGO | S_IWUSR,
		       invsens_dmp_data_show,
		       invsens_dmp_data_store, 0);

#endif


static struct attribute *invsens_attributes[] = {
	&iio_dev_attr_control.dev_attr.attr,
	&iio_dev_attr_reg_dump.dev_attr.attr,
	&iio_dev_attr_log_level.dev_attr.attr,
	&iio_dev_attr_swst_gyro.dev_attr.attr,
	&iio_dev_attr_hwst_accel.dev_attr.attr,
	&iio_dev_attr_hwst_gyro.dev_attr.attr,
	&iio_dev_attr_swst_compass.dev_attr.attr,
	&iio_dev_attr_motion_interrupt.dev_attr.attr,
	&iio_dev_attr_accel_raw.dev_attr.attr,
	&iio_dev_attr_gyro_raw.dev_attr.attr,
	&iio_dev_attr_temperature.dev_attr.attr,
	&iio_dev_attr_s_gyro_selftest.dev_attr.attr,
	&iio_dev_attr_s_accel_selftest.dev_attr.attr,
	&iio_dev_attr_s_accel_cal_start.dev_attr.attr,
	&iio_dev_attr_s_accel_cal.dev_attr.attr,
	&iio_dev_attr_gyro_is_on.dev_attr.attr,
	&iio_dev_attr_stiction_test_mode.dev_attr.attr,
#if 1//rocky
	&iio_dev_attr_dmp_address.dev_attr.attr,
	&iio_dev_attr_dmp_data.dev_attr.attr,
#endif
//	&iio_dev_attr_false_z.dev_attr.attr,
	NULL,
};



static const struct attribute_group invsens_attr_group = {
	.attrs = invsens_attributes,
};

static const struct iio_info invsens_i2c_info = {
	.attrs = &invsens_attr_group,
	.driver_module = THIS_MODULE,
};

static const struct iio_chan_spec invsens_channels[] = {
	IIO_CHAN_SOFT_TIMESTAMP(8),
};

static int invsens_preenable(struct iio_dev *indio_dev)
{
	int result = 0;
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	struct iio_buffer *iio_buffer = NULL;

	INV_DBG_FUNC_NAME;

	iio_data->data_list.copy_buffer =
	    kzalloc(INVSENS_COPY_BUFFER_SIZE, GFP_KERNEL);

	if (!iio_data->data_list.copy_buffer)
		return -ENOMEM;
	iio_buffer = iio_data->indio_dev->buffer;
	iio_buffer->access->set_bytes_per_datum(iio_buffer,
				INVSENS_IIO_BUFFER_DATUM);

	return result;
}



static int invsens_predisable(struct iio_dev *indio_dev)
{
	int result = 0;
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	INV_DBG_FUNC_NAME;

	if (iio_data->data_list.copy_buffer) {
		kzfree(iio_data->data_list.copy_buffer);
		iio_data->data_list.copy_buffer = NULL;
	}

	return result;
}


static const struct iio_buffer_setup_ops invsens_ring_setup_ops = {
	.preenable = &invsens_preenable,
	.predisable = &invsens_predisable,
};




//static irqreturn_t invsens_irq_handler(int irq, void *user_data)
//{
//	struct invsens_iio_data *iio_data = user_data;
//
//	INV_DBG_FUNC_NAME;
//
//	iio_data->data_list.timestamp = iio_get_time_ns();
//
//	return IRQ_WAKE_THREAD;
//}

static void invsens_prefetch_data(struct invsens_iio_data *iio_data,
	bool *need_to_store)
{
	int i;
	struct invsens_data_t *p = NULL;

	if(!iio_data){
		printk("%d:iio_data is null!", __LINE__);
		return;
	}

	/*copy data to cache*/
	for( i = 0; i < iio_data->data_list.count; i ++) {
		p = &iio_data->data_list.items[i];

		if(p->hdr == INV_DATA_HDR_ACCEL) {
			memcpy(iio_data->accel_cache, p->data, p->length);
			if(iio_data->accel_is_calibrated) {
				iio_data->accel_cache[0] -= iio_data->accel_bias[0];
				iio_data->accel_cache[1] -= iio_data->accel_bias[1];
				iio_data->accel_cache[2] -= iio_data->accel_bias[2];
				memcpy(p->data, iio_data->accel_cache, p->length);
			}
		}

		if(p->hdr == INV_DATA_HDR_GYRO)
			memcpy(iio_data->gyro_cache, p->data, p->length);
	}

	*need_to_store = true;

	/*if only factory mode is on, it isn't neccessary to post buffer to hal*/
	if(!(iio_data->data_list.enable_mask & ~(1 << INV_FUNC_FT_SENSOR_ON)))
		*need_to_store = false;

	if(iio_data->data_list.event_motion_interrupt_notified) {
		INVSENS_LOGD("motion interrupt is delivered !!!!!\n");
		iio_data->motion_alert_is_occured = true;
	}
}



static void invsens_store_to_buffer(struct invsens_iio_data *iio_data)
{
	s64 timestamp;
	uint8_t *copy_buffer = NULL;
	struct iio_buffer *iio_buffer = NULL;
	struct invsens_data_t *p = NULL;
	int i = 0, ind = 0;

	timestamp = iio_data->data_list.timestamp;
	copy_buffer = iio_data->data_list.copy_buffer;

	if (copy_buffer && (iio_data->data_list.count > 0)) {
		int timestamp_size = sizeof(timestamp);
		u16 packet_header = INVSENS_PACKET_HEADER;
		u16 packet_size;
		u16 packet_size_offset;

		iio_buffer = iio_data->indio_dev->buffer;

		/*fill packet header */
		memcpy(&copy_buffer[ind], &packet_header, sizeof(packet_header));
		ind += sizeof(packet_header);
		packet_size_offset = ind;
		/*leave packet size empty, it will be filled out at the bottom*/
		ind += sizeof(packet_size);

		memcpy(&copy_buffer[ind], (void *) &timestamp,
		       timestamp_size);
		ind += timestamp_size;

		/*fill sensor data into copy buffer*/
		for (i = 0; i < iio_data->data_list.count; i++) {
			p = &iio_data->data_list.items[i];

			/*put header into buffer first*/
			memcpy(&copy_buffer[ind], &p->hdr, sizeof(p->hdr));
			ind += sizeof(p->hdr);

			/*put data into buffer*/
			memcpy(&copy_buffer[ind], p->data, p->length);
			ind += p->length;
		}

		/*fill packet size*/
		packet_size = (u16)ind;
		memcpy(&copy_buffer[packet_size_offset], &packet_size, sizeof(packet_size));

		iio_buffer->access->store_to(iio_buffer, copy_buffer,
					     timestamp);
	}

}


static void invsens_work_func(struct work_struct *work)
{
	
	struct invsens_iio_data *iio_data = container_of(work, struct invsens_iio_data, work);
	int result = 0;
	bool need_to_store = false;
	
	INV_DBG_FUNC_NAME;
	
	iio_data->data_list.timestamp = iio_get_time_ns();

 	down(&iio_data->sema);
	result = invsens_sm_read(&(iio_data->sm_data), &(iio_data->data_list));
	up(&iio_data->sema);

	invsens_prefetch_data(iio_data, &need_to_store);

	if((result == SM_SUCCESS) && need_to_store)
		invsens_store_to_buffer(iio_data);


	/*clear data list */
	iio_data->data_list.count = 0;
	iio_data->data_list.is_fifo_data_copied = false;
	iio_data->data_list.event_motion_interrupt_notified = false;
	iio_data->data_list.int_status = 0;
	iio_data->data_list.dmp_int_status = 0;

}

 irqreturn_t invsens_thread_handler(int irq, void *dev_id)
{
	
	struct invsens_iio_data *iio_data = (struct  invsens_iio_data * )dev_id;
	INV_DBG_FUNC_NAME;
	
	queue_work(inv_wq, &iio_data->work);
	
	return IRQ_HANDLED;
}

static int invsens_request_irq(struct invsens_iio_data *iio_data)
{
		
	script_item_u 	val;
	script_item_value_type_e  type;
	int irq_number = 0;
	int ret = 0;

	/*get irq number*/
	type = script_get_item("gsensor_para", "gsensor_int1", &val);	
	if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
		printk("%s: type err int1 = %d. \n", __func__, val.gpio.gpio);
		return 0;
	}
	sensor_info.irq_gpio = val.gpio;
	sensor_info.int_number = val.gpio.gpio;
		
	
	
	/*register irq*/
	irq_number = gpio_to_irq(sensor_info.int_number);
	if (IS_ERR_VALUE(irq_number)) {
			printk("map gpio [%d] to virq failed, errno = %d\n", 
		         	sensor_info.int_number, irq_number);
			return -EINVAL;
	}
	
	ret = devm_request_irq(sensor_info.dev, irq_number, invsens_thread_handler, 
			       IRQF_TRIGGER_FALLING, "INV_EINT", iio_data);
		if (IS_ERR_VALUE(ret)) {
			printk("[INV]:request virq %d failed, errno = %d\n", 
		         irq_number, ret);
			return -EINVAL;
		}
		
	return 0;	
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define BIT_SLEEP			(1<<6)

static void invsens_early_suspend(struct early_suspend *h)
{
	struct invsens_iio_data *iio_data = container_of(h, struct invsens_iio_data, early_suspend);
	u32 irq_number = 0;
	
	INV_DBG_FUNC_NAME;

	printk("[inv]: early suspend\n");
	
	mutex_lock(&suspend_mutex);			 			
	suspend_flag = true;/*set the early suspend flags*/
	mutex_unlock(&suspend_mutex);
	
	inv_i2c_read(&iio_data->i2c_handler,
				     iio_data->sm_data.board_cfg.i2c_addr, 0x6B, 1,
				     &iio_data->suspend_data);
	
	printk("[inv]:early suspend, reg(0x6B):0x%x\n", iio_data->suspend_data);
	
	/*all functions are disabled, chip can go to sleep*/
	/*0x6B set to 0x40*/	
	inv_i2c_single_write(&iio_data->i2c_handler, 
						  	iio_data->sm_data.board_cfg.i2c_addr,0x6B,
				 			iio_data->suspend_data | BIT_SLEEP);
	
	/*disable irq*/
	irq_number = gpio_to_irq(sensor_info.int_number);
	disable_irq(irq_number);
	
	/*cancel workqueue*/
	cancel_work_sync(&iio_data->work);
	flush_workqueue(inv_wq);
	
	return ;
}


static void invsens_late_resume(struct early_suspend *h)
{
	struct invsens_iio_data *iio_data = container_of(h, struct invsens_iio_data, early_suspend);
	u32 irq_number = 0;
	
	printk("[inv]: later resume\n");
	
	cancel_work_sync(&iio_data->work);
	flush_workqueue(inv_wq);
	
	irq_number = gpio_to_irq(sensor_info.int_number);
	enable_irq(irq_number);
	
	mutex_lock(&suspend_mutex);
	suspend_flag = false;/*set the later resume flags*/
	mutex_unlock(&suspend_mutex);
	
//	if(iio_data->suspend_data | BIT_SLEEP) {
//		iio_data->suspend_data &= ~BIT_SLEEP;
//	}
//	printk("[inv]:later resume, reg(0x6B):0x%x\n", iio_data->suspend_data);
//	/*resume 0x6B set to 0*/
//	inv_i2c_single_write(&iio_data->i2c_handler, 
//						  iio_data->sm_data.board_cfg.i2c_addr,0x6B,
//				 		  iio_data->suspend_data);
	
	return ;
}
#endif


static int invsens_configure_buffer(struct invsens_iio_data *iio_data)
{
	int result = 0;
	struct iio_buffer *buffer;

	buffer = invsens_kfifo_allocate(iio_data->indio_dev);
	if(buffer == NULL)
		goto exit_destruct_kfifo;

	iio_data->indio_dev->buffer = buffer;

	/* setup ring buffer */
	buffer->scan_timestamp = true;
	iio_data->indio_dev->setup_ops = &invsens_ring_setup_ops;
	iio_data->indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	
	sensor_info.dev =&( iio_data->indio_dev->dev);	
	result = invsens_request_irq(iio_data);

	if (result)  {
		printk("*********%d\n",  __LINE__);
		goto exit_unreg_irq;
	}


	return 0;

exit_destruct_kfifo:
exit_unreg_irq:
	invsens_kfifo_free(buffer);
	return result;
}


static int invsens_set_trigger_state(struct iio_trigger *trig, bool state)
{
	return 0;
}

static const struct iio_trigger_ops invsens_trriger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &invsens_set_trigger_state,
};


static int invsens_set_trigger(struct invsens_iio_data *iio_data)
{
	struct iio_trigger *trig;
	int result = 0;
	trig = iio_allocate_trigger("%s-dev%d",
				    iio_data->indio_dev->name,
				    iio_data->indio_dev->id);
	trig->private_data = iio_data;
	trig->ops = &invsens_trriger_ops;

	result = iio_trigger_register(trig);
	if (result) {
		iio_free_trigger(trig);
		return result;
	}

	iio_data->indio_dev->trig = trig;

	return result;
}


static struct invsens_platform_data_t mpu_gyro_data = {
    .int_config  = 0x10,
    .orientation = {   -1,  0,  0,
                       0,  1,  0,
                       0,  0,  -1},
    .level_shifter = 0,

};
static int invsens_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct invsens_iio_data *data = NULL;
	struct iio_dev *indio_dev;
	int result = 0;
#ifdef CONFIG_INV_SENSORS_DTS_SUPPORT
	static struct invsens_platform_data_t platform_data;
#endif

	client->addr = addr;
	
	indio_dev = iio_allocate_device(sizeof(struct invsens_iio_data));
	if (!indio_dev) {
		result = -ENOMEM;
		goto exit_free_data;
	}
	
	/* change the name of iio node from iio:devicex to invensense*/
	dev_set_name(&indio_dev->dev, "invensense");

	i2c_set_clientdata(client, indio_dev);

	data = iio_priv(indio_dev);

	invsens_set_log_mask(0x10);
	data->indio_dev = indio_dev;

	/*setup iio device nodes */
	data->indio_dev->info = &invsens_i2c_info;
	data->indio_dev->dev.parent = &client->dev;
	data->indio_dev->modes = 0x0;
	data->indio_dev->channels = invsens_channels;
	data->indio_dev->num_channels = ARRAY_SIZE(invsens_channels);
	data->indio_dev->name = client->name;

	sema_init(&data->sema, 1);
	/*set inital values for internal drivers */
	data->i2c_handler.client = client;

	data->irq = client->irq;
	data->device_name = client->name;

	/*set sm data */
	data->sm_data.board_cfg.i2c_handle = &data->i2c_handler;
	data->sm_data.board_cfg.i2c_addr = client->addr;
	data->sm_data.board_cfg.platform_data = data->platform_data;

	INIT_DELAYED_WORK(&data->fso_work, invsens_delayed_work_fso);
	data->fso_enabled = false;
	
	INIT_DELAYED_WORK(&data->sync_work, invsens_delayed_work_sync);

	/*cal info*/
	memset(data->accel_bias, 0x0, sizeof(data->accel_bias));
	data->accel_is_calibrated = false;


	/*set data list*/
	data->data_list.count = 0;
	data->data_list.is_fifo_data_copied = false;
	data->data_list.event_motion_interrupt_notified = false;

#ifdef CONFIG_INV_SENSORS_DTS_SUPPORT
	inv_sensors_parse_dt(&client->dev, &platform_data);
	data->sm_data.board_cfg.platform_data = &platform_data;
	/*Power on device. */
	if (platform_data.power_on) {
		result = platform_data.power_on(&platform_data);
		if (result < 0) {
			dev_err(&client->dev,
				"power_on failed: %d\n", result);
			return result;
		}
	}

	msleep(100);
#else
//	data->platform_data = (struct invsens_platform_data_t *)
//	    dev_get_platdata(&client->dev);
	data->platform_data = &mpu_gyro_data;
	data->sm_data.board_cfg.platform_data = data->platform_data;
#endif

	INIT_WORK(&data->work, invsens_work_func);
	inv_wq = create_singlethread_workqueue("inv_wq");
	if (!inv_wq) {
		printk(KERN_ALERT "Creat %s workqueue failed.\n", "mpu5400");
		return -ENOMEM;
		
	}
	flush_workqueue(inv_wq);
	/*configure iio fifo buffer */
	result = invsens_configure_buffer(data);
	if (result) {
		printk("*********%d\n",  __LINE__);
		goto exit_free_ring;
	}
	
	/*register iio buffer */
	result =
	    iio_buffer_register(data->indio_dev, data->indio_dev->channels,
				data->indio_dev->num_channels);
	if (result){
		printk("%d:iio_buffer_register fail!\n", __LINE__);
		goto exit_free_buffer;
	}
	
	result = invsens_set_trigger(data);
	if (result){
		printk("%d:invsens_set_trigger fail!\n", __LINE__);
		goto exit_unregister_iio;
	}
	
	result = invsens_sm_init(&data->sm_data);
	if (result) {
		printk("%d:invsens_sm_init fail!\n", __LINE__);
		goto exit_unregister_iio;
	}
	//queue_work(inv_init_wq, &data->init_work);	

	/*register iio device */
	result = iio_device_register(data->indio_dev);
	if (result)
		goto exit_free_iio;
		
#ifdef CONFIG_HAS_EARLYSUSPEND	
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;	
	data->early_suspend.suspend = invsens_early_suspend;
	data->early_suspend.resume= invsens_late_resume;
	register_early_suspend(&data->early_suspend);
#endif 

	mutex_init(&suspend_mutex);
	pr_info("[INV]invensense sensors probe OK!\n");
	return 0;

exit_free_ring:
exit_free_buffer:
	iio_buffer_unregister(data->indio_dev);
exit_unregister_iio:
	iio_device_unregister(data->indio_dev);
exit_free_iio:
	iio_free_device(data->indio_dev);
exit_free_data:
	if (data != NULL)
		kfree(data);

	return result;
}

static int invsens_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	invsens_sm_term(&iio_data->sm_data);

	kfree(iio_data);
	iio_device_unregister(indio_dev);
	flush_workqueue(inv_wq);
	
	if (inv_wq)
		destroy_workqueue(inv_wq);
	return 0;
}

static int invsens_suspend(struct i2c_client *client, pm_message_t mesg)
{
// struct iio_dev *indio_dev = i2c_get_clientdata(client);
//	struct invsens_iio_data *iio_data = iio_priv(indio_dev);

	return 0;
}

static int invsens_resume(struct i2c_client *client)
{


	
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct invsens_iio_data *iio_data = iio_priv(indio_dev);
	
	INV_DBG_FUNC_NAME;
	
	invsens_sm_init(&iio_data->sm_data);
	schedule_delayed_work(&iio_data->sync_work, msecs_to_jiffies(200));
	
	return 0;
}

/**
 * MPU6050C_detect - Device detection callback for automatic device creation
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
static int invsens_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;	
	int ret;
	int i = 0;
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	if(sensor_info.twi_id == adapter->nr){
		while( i < 2) {
			client->addr = i2c_address[i++];
			ret = i2c_smbus_read_byte_data(client, 0x75);
			if ((ret&0x00FF) == 0x7d) {
				printk( "%s: MPU5400(i2c addr:0x%x) is detected!\n ", __func__, client->addr);
				addr = client->addr;
				strlcpy(info->type, "MPU5400", I2C_NAME_SIZE);
				return 0;
			}
			printk("reg(0x75):0x%x, addr:0x%x. device is not find!\n", ret, client->addr);
		}
	}
	else {
		return -ENODEV;
	}
}

static const struct i2c_device_id invsens_ids[] = {
	{"ITG3500", 119},
	{"MPU3050", 63},
	{"MPU6050", 117},
	{"MPU9150", 118},
	{"MPU6500", 128},
	{"MPU9250", 128},
	{"MPU9350", 128},
	{"MPU6515", 128},
	{"MPU5400", 128},
	{"MPU6880", 128},
	{"mpu6515", 128},
	{"mpu5400", 128},
	{}
};

static struct i2c_driver invsens_driver = {
	.class 	= I2C_CLASS_HWMON,
	.driver = {
			.owner = THIS_MODULE,
		   	.name = "MPU5400",
		   },
	.id_table 		= invsens_ids,
	.probe 			= invsens_probe,
	.remove 		= invsens_remove,
	.address_list 	= normal_i2c,
	.detect	   		= invsens_detect,
	.suspend 		= invsens_suspend,
	.resume 		= invsens_resume,
};

static int __init invsens_init(void)
{
    INV_DBG_FUNC_NAME;
    
    if (input_fetch_sysconfig_para(&(sensor_info.input_type))) {
		printk("%s: ls_fetch_sysconfig_para err.\n", __func__);
		return 0;
	} 
	
	if (sensor_info.sensor_used == 0) {
		printk("*** gsensor_used set to 0 !\n");
		printk("*** if use gsensor_sensor,please put the sys_config.fex gsensor_used set to 1. \n");
		return 0;
	}
	
	return i2c_add_driver(&invsens_driver);
}

static void __exit invsens_exit(void)
{
	INV_DBG_FUNC_NAME;

	i2c_del_driver(&invsens_driver);
	input_free_platform_resource(&(sensor_info.input_type));
}

MODULE_AUTHOR("Invensense Korea Design Center");
MODULE_DESCRIPTION("Invensense Linux Driver");
MODULE_LICENSE("GPL");

module_init(invsens_init);
module_exit(invsens_exit);
