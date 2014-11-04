#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/firmware.h>
#include <linux/inv_sensors.h>

#include "../../iio.h"
#include "../../buffer.h"
#include "../../kfifo_buf.h"
#include "../../sysfs.h"


#include "inv_sensors_sm.h"
#include "inv_sensors_common.h"

#include "inv_dmp_k520s.h"

#define FIRMWARE_FILENAME "dfk520-5400.bin"
#define DMP_START_ADDRESS 0x400

#define mem_w(dctl, addr, len, data) \
	invdmp_write_memory(addr, len, data, dctl->parent_data)

#define mem_r(dctl, addr, len, data) \
	invdmp_read_memory(addr, len, data, dctl->parent_data)

#define reg_w(dctl, addr, len, data) \
        invdmp_write_register(addr, len, data, dctl->parent_data)

#define reg_r(dctl, addr, len, data) \
        invdmp_read_register(addr, len, data, dctl->parent_data)

#define AXIS_NUM 3
#define AXIS_ADC_BYTE 2
#define QUAT_AXIS_BYTE 4
#define QUAT_AXIS_NUM 3
#define PACKET_SIZE_PER_SENSOR (AXIS_NUM * AXIS_ADC_BYTE)
#define PACKET_SIZE_PER_QUAT (QUAT_AXIS_BYTE*QUAT_AXIS_NUM)
#define PACKET_SIZE_PER_STED (4)
#define PACKET_SIZE_PER_SMD  (0)

/*
	protocol definition between kenel and chip
*/

// Sensor update rate
#define DMP_ACCEL_RATE 772      // (16*48 + 4)
#define DMP_GYRO_RATE 774       // (16*48 + 6)
#define DMP_GYRO_COUNTER 408    // (16*25 + 8)

#define DMP_ACCEL_COUNT 768    // (16*48)rocky
#define DMP_GYRO_COUNT	770	// (16*48)+2rocky

// Z-Axis stiction test mode
#define DMP_STICTION_TEST	716

// Header
#define HEADER_BASE_VALUE 0x8000
#define HEADER_BIT_FSYNC 0x01
#define HEADER_BIT_ACCEL 0x02
#define HEADER_BIT_GYRO 0x04



struct dmp_func_match_t {
	int func;
	int feat_mask;
};

static struct dmp_func_match_t dmp_match_table[] = {
	{INV_FUNC_ACCEL, (1 << INV_DFEAT_ACCEL)},
	{INV_FUNC_GYRO, (1 << INV_DFEAT_GYRO)},
	{INV_FUNC_BATCH, 0}, 
	{INV_FUNC_FLUSH, 0},
};


#define FUNC_MATCH_TABLE_NUM \
    sizeof(dmp_match_table) / sizeof(dmp_match_table[0])



struct dmp_dctl_t {
	struct {
		int (*enable) (struct dmp_dctl_t * dctl, bool en, long delay);
	} feat_set[INV_DFEAT_MAX];

	long feat_delay[INV_DFEAT_MAX];

	struct invsens_sm_ctrl_t sm_ctrl;
	struct invsens_platform_data_t *platform_data;

	void *parent_data;
	u32 enabled_mask;
};

static int dmp_write_2bytes(struct dmp_dctl_t *dctl, u16 addr, int data)
{
	u8 d[2];

	if (data < 0 || data > USHRT_MAX)
		return -EINVAL;
	d[0] = (u8) ((data >> 8) & 0xff);
	d[1] = (u8) (data & 0xff);
	return mem_w(dctl, addr, ARRAY_SIZE(d), d);
}

static int dmp_read_2bytes(struct dmp_dctl_t *dctl, u16 addr, int *ret)
{
	u8 d[2];
	int result = 0;

	result = mem_r(dctl, addr, ARRAY_SIZE(d), d);

	*ret = ((d[0] & 0xff) << 8) + (d[1] & 0xff);

	return result;
}

/**
* int dmp_stiction_test_mode(struct dmp_dctl_t *dctl, bool en, long delay)
* en = 1 ; enable
* en = 0 ; enable
*/
static int dmp_set_stiction_test_mode(struct dmp_dctl_t *dctl, bool en)
{
	int val = 0;

	INV_DBG_FUNC_NAME;

	val = en ? 1 : 0;

	INVSENS_LOGD("[INV] %s en=%d val=%d\n", __func__, en, val);
	
	return dmp_write_2bytes(dctl, DMP_STICTION_TEST, val);

}

#if 1 //rocky 
static u16 dmp_address = 0;


static int dmp_set_dmp_address(struct dmp_dctl_t *dctl, u16 dmp_addr)
{
	int val = 0;
	INV_DBG_FUNC_NAME;
	dmp_address = dmp_addr;
	return 0;	
}

static int dmp_set_dmp_data(struct dmp_dctl_t *dctl, int dmp_data)
{

	INV_DBG_FUNC_NAME;
	pr_info("[INV] %s dmp_addr=%d dmp_data=%d\n", __func__, dmp_address, dmp_data);
	return dmp_write_2bytes(dctl, dmp_address, dmp_data);
}

static int dmp_get_dmp_data(struct dmp_dctl_t *dctl, int *ret)
{
	INV_DBG_FUNC_NAME;
	pr_info("[INV] %s dmp_addr=%d\n", __func__, dmp_address);
	return dmp_read_2bytes(dctl, dmp_address, ret);	
}


static int dmp_get_accel_rate(struct dmp_dctl_t *dctl, int *ret)
{
	INV_DBG_FUNC_NAME;
	
	return dmp_read_2bytes(dctl, DMP_ACCEL_RATE, ret);	
}
static int dmp_get_gyro_rate(struct dmp_dctl_t *dctl, int *ret)
{
	INV_DBG_FUNC_NAME;
	
	return dmp_read_2bytes(dctl, DMP_GYRO_RATE, ret);	
}

static int dmp_get_accel_count(struct dmp_dctl_t *dctl, int *ret)
{
	INV_DBG_FUNC_NAME;
	
	return dmp_read_2bytes(dctl, DMP_ACCEL_COUNT, ret);	
}
static int dmp_get_gyro_count(struct dmp_dctl_t *dctl, int *ret)
{
	INV_DBG_FUNC_NAME;
	
	return dmp_read_2bytes(dctl, DMP_GYRO_COUNT, ret);	
}

static int dmp_get_gyro_counter(struct dmp_dctl_t *dctl, int *ret)
{
	INV_DBG_FUNC_NAME;
	
	return dmp_read_2bytes(dctl, DMP_GYRO_COUNTER, ret);	
}

#endif

static int dmp_get_stiction_test_mode(struct dmp_dctl_t *dctl, int *ret)
{
	INV_DBG_FUNC_NAME;
	
	return dmp_read_2bytes(dctl, DMP_STICTION_TEST, ret);	
}


static int dmp_enable_accel(struct dmp_dctl_t *dctl, bool en, long delay)
{
	int rate;

	INV_DBG_FUNC_NAME;

	rate = (int) (delay / 5000000L);

	if(!en)
		rate = 0;

	INVSENS_LOGD("[INV] %s enable=%d rate = %d\n", __func__, en, rate);
	
	return dmp_write_2bytes(dctl, DMP_ACCEL_RATE, rate);
}


static int dmp_enable_gyro(struct dmp_dctl_t *dctl, bool en, long delay)
{
	int rate;

	INV_DBG_FUNC_NAME;

	rate = (int) (delay / 5000000L);

	if(!en)
		rate = 0;

	INVSENS_LOGD("[INV] %s enable=%d rate = %d\n", __func__, en, rate);

	return dmp_write_2bytes(dctl, DMP_GYRO_RATE, rate);
}


static int dmp_trans_func2feat(struct dmp_dctl_t *dctl,
				u32 func_mask, u32 * feat_mask)
{
	int res = 0;
	int i = 0, j = 0;
	u32 mask = 0;

	INV_DBG_FUNC_NAME;

	for (i = 0; i < INV_FUNC_NUM; i++)
		if (func_mask & (1 << i))
			for (j = 0; j < FUNC_MATCH_TABLE_NUM; j++)
				if (dmp_match_table[j].func == i)
					mask |= dmp_match_table[j].feat_mask;

	*feat_mask = mask;
	return res;
}

static void dmp_update_feat_delays(struct dmp_dctl_t *dctl, u32 feat_mask)
{
	long accel_delay = 200000000L;
	long gyro_delay = 200000000L;
	u32 func = 0;

	/*-----------------------------------------*/
	/* decide the delays of accel, gyro and compass        */
	/*-----------------------------------------*/
	/* 1. check accel */
	func = INV_FUNC_ACCEL;
	if (dctl->sm_ctrl.enable_mask & (1 << func)) {
		accel_delay = dctl->sm_ctrl.delays[func];
	}

	/* 2. check gyro */
	func = INV_FUNC_GYRO;
	if (dctl->sm_ctrl.enable_mask & (1 << func)) {
		gyro_delay = dctl->sm_ctrl.delays[func];
	}

	dctl->feat_delay[INV_DFEAT_ACCEL] = accel_delay;
	dctl->feat_delay[INV_DFEAT_GYRO] = gyro_delay;
}

static long dmp_get_delay(struct dmp_dctl_t *dctl, int feat)
{
	long delay = dctl->feat_delay[feat];

	INVSENS_LOGD("%s : delay = %ld\n", __func__, delay);
	return delay;
}

static int dmp_enable_features(struct dmp_dctl_t *dctl, u32 feat_mask)
{
	int res = 0;
	int i = 0;
	bool enable;

	INV_DBG_FUNC_NAME;

	INVSENS_LOGD("%s : feat_mask = 0x%x\n", __func__, feat_mask);

	dmp_update_feat_delays(dctl, feat_mask);

	for (i = 0; i < INV_DFEAT_MAX; i++) {
		if (dctl->feat_set[i].enable) {
			if (feat_mask & (1 << i))
				enable = true;

			else
				enable = false;
			res = dctl->feat_set[i].enable(dctl, enable, dmp_get_delay(dctl, i) );
		}
	}
	return res;
}


static int dmp_apply_mpu_orientation(struct dmp_dctl_t *dctl,
				     const u8 src[], u8 dest[])
{
	short tmp[AXIS_NUM];
	int oriented[AXIS_NUM];

	int i = 0;

	tmp[0] = ((short) src[0] << 8) + src[1];
	tmp[1] = ((short) src[2] << 8) + src[3];
	tmp[2] = ((short) src[4] << 8) + src[5];

	for (i = 0; i < AXIS_NUM; i++) {
		oriented[i] = tmp[0] * dctl->platform_data->orientation[i * 3 + 0]
		    + tmp[1] * dctl->platform_data->orientation[i * 3 + 1]
		    + tmp[2] * dctl->platform_data->orientation[i * 3 + 2];

		/*handle overflow */
		if (oriented[i] > 32767)
			oriented[i] = 32767;
	}

	for (i = 0; i < AXIS_NUM; i++)
		tmp[i] = (short) oriented[i];

	memcpy((void *) dest, (void *) tmp, sizeof(tmp));

	return 0;
}


static int dmp_parse_fifo(struct dmp_dctl_t *dctl,
			      int len, const char *data,
			      struct invsens_data_list_t *buffer)
{
	u8 *p = (u8 *) data;
	u16 header;
	int res = SM_SUCCESS;

	INV_DBG_FUNC_NAME;

	while (p && p < (u8 *) (data + len)) {
		if (buffer->count >= INVSENS_DATA_ITEM_MAX) {
   			pr_info("%s, ::special***\n", __func__);
			return res;
		}

		/*find header */
		header = (u16) p[0] << 8 | p[1];

		p += sizeof(u16);

		if((header & 0xff00)	== HEADER_BASE_VALUE) {
			u8 mask = (u8)(header & 0xff);
			if(mask & HEADER_BIT_ACCEL) {
				struct invsens_data_t *d =
				    &buffer->items[buffer->count++];
				d->hdr = INV_DATA_HDR_ACCEL;
				d->length = PACKET_SIZE_PER_SENSOR;
				dmp_apply_mpu_orientation(dctl, p,
							  &d->data[0]);
				INVSENS_LOGD("ACCEL_HDR\n");
				p += d->length;
			}
			if(mask & HEADER_BIT_GYRO) {
				struct invsens_data_t *d =
				    &buffer->items[buffer->count++];
				d->hdr = INV_DATA_HDR_GYRO;
				d->length = PACKET_SIZE_PER_SENSOR;
				dmp_apply_mpu_orientation(dctl, p,
							  &d->data[0]);
				INVSENS_LOGD("GYRO_HDR\n");
				p += d->length;
			}
			if(mask & HEADER_BIT_FSYNC) {

			}
		}

	};

	return res;
}



static int dmp_k520s_enable(struct invdmp_driver_t *drv,
							u32 func_mask, bool enabled, void *sm_ctrl)
{
	int res = 0;
	struct dmp_dctl_t *dctl = NULL;
	u32 enable_mask = 0;
	u32 feat_mask = 0;

	INV_DBG_FUNC_NAME;
	dctl = INVSENS_DRV_USER_DATA(drv);
	if (sm_ctrl)
		memcpy(&dctl->sm_ctrl, sm_ctrl, sizeof(struct invsens_sm_ctrl_t));

	enable_mask = dctl->sm_ctrl.enable_mask;
	INVSENS_LOGD("%s : enable_mask = 0x%x\n", __func__, enable_mask);

	res = dmp_trans_func2feat(dctl, enable_mask, &feat_mask);
	if (res)
		goto error_case;

	res = dmp_enable_features(dctl, feat_mask);
	if (res)
		goto error_case;
	dctl->enabled_mask = enable_mask;

 error_case:
 	return res;
}

static int dmp_k520s_terminate(struct invdmp_driver_t *drv)
{
	int res = 0;
	struct dmp_dctl_t *dctl = NULL;

	INV_DBG_FUNC_NAME;
	dctl = INVSENS_DRV_USER_DATA(drv);
	kzfree(dctl);
	INVSENS_DRV_USER_DATA(drv) = NULL;
	return res;
}

static int dmp_k520s_read(struct invdmp_driver_t *drv,
						  struct invsens_data_list_t *buffer)
{
	int res = SM_SUCCESS;
	struct dmp_dctl_t *dctl = NULL;
	u8 user_ctrl;

	INV_DBG_FUNC_NAME;

	dctl = INVSENS_DRV_USER_DATA(drv);

	INVSENS_LOGD("[INV] %s is_dmp_on %d | enable_mask %d | is_fifo_data_copied %d\n",
		__func__, dctl->sm_ctrl.is_dmp_on, dctl->sm_ctrl.enable_mask,
		buffer->is_fifo_data_copied);
	if(dctl->sm_ctrl.is_dmp_on
		&& dctl->sm_ctrl.enable_mask
		&& (buffer->is_fifo_data_copied))
	{
		INVSENS_LOGD("[INV] %s DMP is on. dmp_int_status = %d.\n", __func__, buffer->dmp_int_status);
		dmp_parse_fifo(dctl, buffer->fifo_data_length,
				buffer->copy_buffer,buffer);

	}

	/* "2" means that after 2 DMPcycle, DMP will start using
	   gyro data for estimating accel Z axis */
	if(buffer->dmp_int_status & 0x2)
	{
		res=reg_r(dctl,0x6A,1,&user_ctrl);
           	user_ctrl&=~0x80/*BIT_DMP_EN*/;
		res=reg_w(dctl,0x6A,1,user_ctrl);
		msleep(5);
		res = dmp_write_2bytes(dctl, DMP_GYRO_COUNTER, 2);
		if(res)
			INVSENS_LOGE("DMP_GYRO_COUNTER writing is failed.\n");
                user_ctrl|=0x08/*BIT_DMP_RST*/;
                res=reg_w(dctl,0x6A,1,user_ctrl);
		user_ctrl|=0x80/*BIT_DMP_EN*/;
		res=reg_w(dctl,0x6A,1,user_ctrl);
	}

	return res;
}

static int dmp_init_config(struct dmp_dctl_t *dctl)
{
	int res = SM_SUCCESS;

	memset((void *) dctl->feat_set, 0x0, sizeof(dctl->feat_set));
	dctl->feat_set[INV_DFEAT_ACCEL].enable = dmp_enable_accel;
	dctl->feat_set[INV_DFEAT_GYRO].enable = dmp_enable_gyro;
	return res;
}



static int dmp_k520s_init(struct invdmp_driver_t *drv,
			const signed char orientation[9])
{
	int res = 0;
	struct dmp_dctl_t *dctl = NULL;

	INV_DBG_FUNC_NAME;
	
	dctl = INVSENS_DRV_USER_DATA(drv);
	if (dctl) {
		res = invdmp_load_firmware(FIRMWARE_FILENAME, 0x20,
					 DMP_START_ADDRESS, dctl->parent_data);
		if (res) {
			INVSENS_LOGE("firmware error = %d !!!!\n", res);
			goto error_case;
		}
	}
	res = dmp_init_config(dctl);
	if (res)
		INVSENS_LOGI("%s configuration error\n", drv->version);

	return SM_SUCCESS;

 error_case:
  	return res;
}

static int dmp_k520s_ioctl(struct invdmp_driver_t *drv, u32 cmd,
			     long lparam, void *vparm)
{
	struct invsens_ioctl_param_t *data = vparm;
	struct dmp_dctl_t *dctl = NULL;

	INV_DBG_FUNC_NAME;

	dctl = INVSENS_DRV_USER_DATA(drv);

	switch(cmd) {
	case INV_IOCTL_GET_START_ADDR:
		{
			data->u32d[0] = DMP_START_ADDRESS;
		}
		return SM_IOCTL_HANDLED;

	case INV_IOCTL_SET_STICTION_TESTMODE:
		{
			dmp_set_stiction_test_mode(dctl, (int)lparam);
		}
		return SM_IOCTL_HANDLED;
#if 1 //rocky
	case INV_IOCTL_SET_DMP_ADDRESS:
		{
			dmp_set_dmp_address(dctl, (int)lparam);
		}
		return SM_IOCTL_HANDLED;
	case INV_IOCTL_SET_DMP_DATA:
		{
			dmp_set_dmp_data(dctl, (int)lparam);
		}
		return SM_IOCTL_HANDLED;
	case INV_IOCTL_GET_DMP_ADDRESS:
		{
			data->u32d[0] = dmp_address;
		}
		return SM_IOCTL_HANDLED;
	case INV_IOCTL_GET_DMP_DATA:
		{
			dmp_get_dmp_data(dctl, (int *) &data->u32d[0]);
		}
		return SM_IOCTL_HANDLED;
#endif

	case INV_IOCTL_GET_STICTION_TESTMODE:
		{
			dmp_get_stiction_test_mode(dctl, (int *) &data->u32d[0]);
#if 1 	//rocky
			dmp_get_accel_rate(dctl, (int *) &data->u32d[1]);
			dmp_get_gyro_rate(dctl, (int *) &data->u32d[2]);
			dmp_get_gyro_counter(dctl, (int *) &data->u32d[3]);
			dmp_get_accel_count(dctl, (int *) &data->u32d[4]);
			dmp_get_gyro_count(dctl, (int *) &data->u32d[5]);
#endif
		}
		return SM_IOCTL_HANDLED;
	}

	return SM_IOCTL_NOTHANDLED;
}


struct invdmp_driver_t dmp_k520s_driver = {
	.version = {"DMPv2 K520-5400"},
	.init = dmp_k520s_init,
	.enable_func = dmp_k520s_enable,
	.read =dmp_k520s_read,
	.terminate = dmp_k520s_terminate,
	.ioctl = dmp_k520s_ioctl,
};

int invdmp_load_module(struct invdmp_driver_t *drv,
	struct invsens_board_cfg_t *board_cfg, void *parent_data)
{
	int res = 0;
	struct dmp_dctl_t *dctl = NULL;

	INV_DBG_FUNC_NAME;
	if (!drv) {
		res = -EINVAL;
		goto error_case;
	}
	memcpy(drv, &dmp_k520s_driver, sizeof(struct invdmp_driver_t));

	dctl = kzalloc(sizeof(struct dmp_dctl_t), GFP_KERNEL);
	if (!dctl) {
		res = -ENOMEM;
		goto error_case;
	}
	dctl->parent_data = parent_data;
	dctl->platform_data = board_cfg->platform_data;

	drv->user_data = (void *) dctl;
	return 0;

 error_case:
  	return res;
}
