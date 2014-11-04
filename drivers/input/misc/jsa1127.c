/* drivers/input/misc/jsa1127.c
 *
 * Copyright (c) 2011 Solteamopto.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <linux/suspend.h>
#include <linux/init-input.h>
#include <mach/sys_config.h>
#ifdef CONFIG_SCENELOCK
#include <linux/power/scenelock.h>
#endif
#ifdef CONFIG_PM
#include <linux/pm.h>
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

//#include <linux/jsa1127.h>



#define I2C_RETRY_TIME 					5
#define PRODUCT_NAME 					"jsa1127"

#define MAX_DELAY  						2000 //2000 ms
#define MIN_DELAY  						200   //200 ms
#define delay_to_jiffies(d) 			((d)?msecs_to_jiffies(d):1)

/* Kernel has no float point, it would convert it by this*/
#define DEFAULT_RESOLUTION 				1670 //RINT = 100, specification page 12, lux/count = 1.67
#define BASE_VALUE 						1000
#define INVALID_COUNT      				0xFFFFFFFF

/* CMD definition */
#define CMD_SHUTDOWN_MODE 				0x8C //(1xxx_xxxx)
#define CMD_ACTIVATE_MODE 				0x0C
#define CMD_ACTIVATE_MODE_ONE_TIME 		0x04
#define CMD_START_INTEGRATION_ONE_TIME	0x08
#define CMD_STOP_INTEGRATION_ONE_TIME	0x30

static u32 debug_mask = 0;
#define dprintk(level_mask, fmt, arg...)	if (unlikely(debug_mask & level_mask)) \
	printk("*jsa1127:*" fmt , ## arg)
	

/* jsa1127 driver data struct */
struct jsa1127_drv_data
{
	struct delayed_work work;
	struct i2c_client *client;
	struct input_dev *input_dev;
//struct jsa1127_platform_data *pdev;
	struct mutex mutex;
	unsigned int delay;
	unsigned long lux;
	unsigned int enabled;
	unsigned long resolution;
};

static struct jsa1127_drv_data jsa1127_data = {
	.delay = MAX_DELAY,
	.lux = 0,
	.enabled = 0,
	.resolution = DEFAULT_RESOLUTION,
};


static struct sensor_config_info ls_sensor_info = {
	.input_type = LS_TYPE,
	.int_number = 0,
	.ldo = NULL,
};

enum {
	DEBUG_INIT = 1U << 0,
	DEBUG_DATA = 1U << 1,
	DEBUG_SUSP = 1U << 2,
	DEBUG_INFO = 1U << 3,
};

static const unsigned short normal_i2c[] = {0x39,I2C_CLIENT_END};
static const unsigned short i2c_address[] = {0x39, 0x29, 0x44};



static int jsa1127_cmd_send(unsigned char cmd)
{
	int ret = -EIO;
	unsigned char wbuf[5];
	int retry = I2C_RETRY_TIME;

	wbuf[0] = cmd;

	dprintk(DEBUG_INFO,"jsa1127_cmd_send now !\n");

	for (; retry > 0; retry --)
	{
		ret = i2c_master_send(jsa1127_data.client, wbuf, 1);
		if (ret < 0)
		{
			printk("write cmd[0x%2X] failed!, retry(%d)\n", cmd, (I2C_RETRY_TIME - retry));	
		}
		else
		{
			dprintk(DEBUG_INFO,"write cmd[0x%2X] success!\n", cmd);
			break;
		}	
	}
	
	return ret;
}

/* Using internal integration timing to read lux value */
static unsigned long jsa1127_read_lux(void)
{
	int ret = -EIO;
	int retry = I2C_RETRY_TIME;
	unsigned char rbuf[5];
	unsigned long lux = INVALID_COUNT;

	//start to read data of lux
	retry = I2C_RETRY_TIME;
	for(; retry > 0; retry --)
	{			
		ret = i2c_master_recv(jsa1127_data.client, rbuf, 2);
		if (ret < 0)
		{
			printk("read failed!, retry(%d)\n", (I2C_RETRY_TIME - retry));	
		}
		else
		{
			dprintk(DEBUG_INFO,"read success!\n");
			break;
		}
	}

//	DBG("rbuf[0] = 0x%x\n", rbuf[0]);
//	DBG("rbuf[1] = 0x%x\n", rbuf[1]);

	if (ret > 0)
	{
		if (rbuf[1] && 0x80)
		{
			lux = (unsigned long)((((int)rbuf[1]&0x7F) << 8) | (int)rbuf[0]);
			dprintk(DEBUG_INFO, "lux value is valid!, count = %ld\n", lux);
		}
		else
		{
			lux = INVALID_COUNT;
			dprintk(DEBUG_INFO, "lux value is invalid!\n");
		}
	}
	
	return lux;
}


/* Initial chips stage, check chip connect with board is fine, using external integration timing */
static int initial_jsa1127_chip(void)
{
	int ret = 0;

	ret = jsa1127_cmd_send(CMD_ACTIVATE_MODE);
	if (ret < 0)
	{
		printk("Send CMD activiate one time failed!\n");
		ret = -EIO;
		goto i2c_err;
	}

#if 0
	ret = jsa1127_cmd_send(CMD_ACTIVATE_MODE_ONE_TIME);
	if (ret < 0)
	{
		printk("Send CMD activiate one time failed!\n");
		ret = -EIO;
		goto i2c_err;
	}

	ret = jsa1127_cmd_send(CMD_START_INTEGRATION_ONE_TIME);
	if (ret < 0)
	{
		printk("Send CMD start command failed!\n");
		ret =  -EIO;
		goto i2c_err;
	}

	ret = jsa1127_cmd_send(CMD_STOP_INTEGRATION_ONE_TIME);
	if (ret < 0)
	{
		printk("Send CMD stop command failed!\n");
		ret =  -EIO;
		goto i2c_err;
	}

	ret = jsa1127_cmd_send(CMD_SHUTDOWN_MODE);
	if (ret < 0)
	{
		printk("Send CMD shutdown failed!\n");
		ret =  -EIO;
		goto i2c_err;
	}
#endif 
	
	if (ret > 0)
		dprintk(DEBUG_INFO, "initial chip success!\n");

i2c_err:
	return ret;
}

/* allocate input event to pass to user space*/
static int jsa1127_input_register(void)
{
	int ret = 0;
	
	/* Allocate Input Device */
	if (!jsa1127_data.input_dev)
    	jsa1127_data.input_dev = input_allocate_device();

	if (!jsa1127_data.input_dev)
    {
        printk("Allocate input device failed.\n");
        ret = -ENOMEM;
        return ret;
    }

	jsa1127_data.input_dev->name = PRODUCT_NAME;
	jsa1127_data.input_dev->id.bustype = BUS_I2C;
	input_set_abs_params(jsa1127_data.input_dev, ABS_MISC, 0, 65535, 0, 0);
	//set_bit(EV_ABS, jsa1127_data.input_dev->evbit);
	input_set_capability(jsa1127_data.input_dev, EV_ABS, ABS_MISC);

    /* Register Input Device */
    ret = input_register_device(jsa1127_data.input_dev);
    if (ret)
    {
       	printk("Register input device failed.\n");
		input_free_device(jsa1127_data.input_dev);
        ret = -EIO;
    }
	
	return ret;
}

/* work queue for update current light senosr lux*/
static void jsa1127_work_func(struct work_struct *work)
{
	unsigned long delay = delay_to_jiffies(jsa1127_data.delay);
	unsigned long lux;

	lux = jsa1127_read_lux();
	if (lux != INVALID_COUNT)
	{
		lux = (lux * jsa1127_data.resolution) / BASE_VALUE;
		jsa1127_data.lux = lux;
		dprintk(DEBUG_DATA, "udpdate lux: %ld\n", lux);
	}
	else
	{
		dprintk(DEBUG_DATA, "report the prevous lux value!\n");
		lux = jsa1127_data.lux;
	}

	if (jsa1127_data.input_dev)
	{
		dprintk(DEBUG_DATA, "report lux input abs_MISC event (%ld)\n", lux);
		input_report_abs(jsa1127_data.input_dev, ABS_MISC, lux);
        input_sync(jsa1127_data.input_dev);
	}
	
	if (jsa1127_data.enabled)
	{
		dprintk(DEBUG_DATA, "still scheduling light sensor workqueue!\n");
		schedule_delayed_work(&jsa1127_data.work, (delay + 1));
	}
}

static ssize_t sensor_data_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	printk("lux: %ld\n", jsa1127_data.lux);
	return snprintf(buf, sizeof(buf), "%ld\n", jsa1127_data.lux);
}

static ssize_t sensor_delay_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	printk("delay: %d", jsa1127_data.delay);
	return snprintf(buf, sizeof(buf), "%d\n", jsa1127_data.delay);
}

static ssize_t sensor_delay_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int value = simple_strtoul(buf, NULL, 10);

	if (value < MIN_DELAY)
		value = MIN_DELAY;

	if (value >= MAX_DELAY)
		value = MAX_DELAY;

	mutex_lock(&jsa1127_data.mutex);
	jsa1127_data.delay = value;
	mutex_unlock(&jsa1127_data.mutex);

	dprintk(DEBUG_INFO, "set delay time as %d ms\n", jsa1127_data.delay);

	return count;
}

static ssize_t sensor_enable_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	printk("enabled: %d\n", jsa1127_data.enabled);
	return snprintf(buf, sizeof(buf), "%d\n", jsa1127_data.enabled);
}

static ssize_t sensor_enable_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	int value = simple_strtoul(buf, NULL, 10);
	unsigned long delay = delay_to_jiffies(jsa1127_data.delay);

	mutex_lock(&jsa1127_data.mutex);

	if (value == 1 && jsa1127_data.enabled == 0)
	{
		if (jsa1127_cmd_send(CMD_ACTIVATE_MODE) < 0) //enable light sensor
		{
			printk("enable jsa1127 failed!");
			return 0;
		}
		else
		{
			dprintk(DEBUG_INFO, "enable light sensor success\n");
			jsa1127_data.enabled = 1;
			schedule_delayed_work(&jsa1127_data.work, (delay + 1));
		}
	}
	else if (jsa1127_data.enabled == 1 && value == 0)
	{
		if (jsa1127_cmd_send(CMD_SHUTDOWN_MODE) < 0) //disable light sensor
		{
			printk("disable jsa1127 failed!");
		}
		
		jsa1127_data.enabled = 0;
		cancel_delayed_work_sync(&jsa1127_data.work);	
	}
	else
	{
		//do nothing
		dprintk(DEBUG_INFO, "Do nothing at this operation time!");
	}
	
	mutex_unlock(&jsa1127_data.mutex);

	dprintk(DEBUG_INFO, "set enabled as %d", jsa1127_data.enabled);

	return count;
}

static ssize_t sensor_resolution_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
	printk("resolution: %ld", jsa1127_data.resolution);
	return snprintf(buf, sizeof(buf), "%ld\n", jsa1127_data.resolution);
}

static ssize_t sensor_resolution_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
	int value = simple_strtoul(buf, NULL, 10);

	if (value < 0)
		return 0;

	mutex_lock(&jsa1127_data.mutex);
	jsa1127_data.resolution = (unsigned long) value;
	mutex_unlock(&jsa1127_data.mutex);

	dprintk(DEBUG_INFO, "set resolution as %ld", jsa1127_data.resolution);

	return count;
}

static struct device_attribute sensor_dev_attr_data = __ATTR(data, S_IRUGO|S_IWUSR|S_IWGRP,
    sensor_data_show, NULL);
static struct device_attribute sensor_dev_attr_delay = __ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
    sensor_delay_show, sensor_delay_store);
static struct device_attribute sensor_dev_attr_enable = __ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
    sensor_enable_show, sensor_enable_store);
static struct device_attribute sensor_dev_attr_resolution = __ATTR(resolution, S_IRUGO|S_IWUSR|S_IWGRP,
    sensor_resolution_show, sensor_resolution_store);

static struct attribute *sensor_attributes[] = {
    &sensor_dev_attr_data.attr,
    &sensor_dev_attr_delay.attr,
    &sensor_dev_attr_enable.attr,
    &sensor_dev_attr_resolution.attr,
    NULL
};

static struct attribute_group sensor_attribute_group = {
    .attrs = sensor_attributes
};

static int __devinit jsa1127_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret = 0;

    /* Chcek I2C */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        printk("Check I2C functionality failed.");
        ret = -EIO;
        goto err_check_functionality_failed;
    }

	/* Get driver data and i2c client */
	jsa1127_data.client = client;
//	jsa1127_data.pdev = (struct jsa1127_platform_data *) client->dev.platform_data;
//	if (!jsa1127_data.pdev)
//	{
//		printk("Please check platform data!");
//		ret = -EIO;
//		goto err_platform_data;
//	}
//
//	/* initial platform configuration*/
//	if (!jsa1127_data.pdev && !jsa1127_data.pdev->configure_platform)
//	{
//		dprintk(DEBUG_INFO,"initalize platform setting!");
//		jsa1127_data.pdev->configure_platform();
//	}

	/* Initial jsa1127 chip and check connect with i2c bus */

	dprintk(DEBUG_INFO, "jsa1127_probe 111111 !\n");
	ret = initial_jsa1127_chip();
	if (ret < 0)
	{
		printk("initial chip error!");
		goto err_init_chip;
	}

	printk("jsa1127_probe 2222 !\n");

	/* register input device and delay work queue for evnet polling */
	ret = jsa1127_input_register();
	if (ret < 0 )
	{
		printk("jsa1127 input register error!");
		goto err_input_failed;
	}
	
	/* initial delay workqueue */
	mutex_init(&jsa1127_data.mutex);
	INIT_DELAYED_WORK(&jsa1127_data.work, jsa1127_work_func);

	/* setup the attr for control by others */
	ret = sysfs_create_group(&jsa1127_data.input_dev->dev.kobj, &sensor_attribute_group);
	if (ret < 0)
	{
		printk("register attributes failed!");
		goto err_sysfs_register;
	}

	dprintk(DEBUG_INFO,"Probe Done.");
	return 0;

err_sysfs_register:
err_input_failed:
err_init_chip:
//err_platform_data:
err_check_functionality_failed:
	
	return ret;
}

static int jsa1127_remove(struct i2c_client *client)
{
    cancel_delayed_work_sync(&jsa1127_data.work);
    sysfs_remove_group(&jsa1127_data.input_dev->dev.kobj, &sensor_attribute_group);
    input_unregister_device(jsa1127_data.input_dev);
    input_free_device(jsa1127_data.input_dev);

    dprintk(DEBUG_INFO,"Remove JSA1127 Module Done.");
    return 0;
}

static int i2c_write_bytes(struct i2c_client *client, uint8_t *data, uint16_t len)
{
	struct i2c_msg msg;
	int ret=-1;
	
	msg.flags = !I2C_M_RD;
	msg.addr = client->addr;
	msg.len = len;
	msg.buf = data;		
	
	ret=i2c_transfer(client->adapter, &msg,1);
	return ret;
}


static bool gsensor_i2c_test(struct i2c_client * client)
{
	int ret, retry;
	uint8_t test_data[1] = { 0 };	//only write a data address.
	
	for(retry=0; retry < 2; retry++)
	{
		ret =i2c_write_bytes(client, test_data, 1);	//Test i2c.
		if (ret == 1)
			break;
		msleep(5);
	}
	
	return ret==1 ? true : false;
}


/**
 * ls_detect - Device detection callback for automatic device creation
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
static int ls_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;
	int ret = -1;
	int i2c_num = 0;
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
            return -ENODEV;
            
	if (ls_sensor_info.twi_id == adapter->nr) {
		for (i2c_num = 0; i2c_num < (sizeof(i2c_address)/sizeof(i2c_address[0])); i2c_num++) {
			client->addr = i2c_address[i2c_num];
			 ret = gsensor_i2c_test(client);
        	if(!ret){
        		printk("jsa1127:I2C connection might be something wrong or maybe the other gsensor equipment! \n");
        	}else{           	    
            	pr_info("I2C connection sucess!\n");
            	strlcpy(info->type, PRODUCT_NAME, I2C_NAME_SIZE);
    		    return 0;	
	             }
	   }

	}
	
	return -ENODEV;
	
}

static const struct i2c_device_id jsa1127_id[] = {
    {PRODUCT_NAME, 0},
    { }
};
MODULE_DEVICE_TABLE(i2c, jsa1127_id);

static struct i2c_driver jsa1127_driver = {
	.class 	= I2C_CLASS_HWMON,
    .driver = {
    	.name  		= PRODUCT_NAME,
        .owner 		= THIS_MODULE,
    },
       
    .probe     		= jsa1127_probe,
    .remove    		= jsa1127_remove,
    .id_table  		= jsa1127_id,
    .detect	   		= ls_detect,
    .address_list   = normal_i2c,
};


/* driver initial part */
static int __init jsa1127_init(void)
{
	int ret = 0;
    dprintk(DEBUG_INFO, "register jsa1127 i2c driver!");
    
    if (input_fetch_sysconfig_para(&(ls_sensor_info.input_type))) {
		printk("%s: ls_fetch_sysconfig_para err.\n", __func__);
		return 0;
	} else {
		ret = input_init_platform_resource(&(ls_sensor_info.input_type));
		if (0 != ret) {
			printk("%s:ls_init_platform_resource err. \n", __func__);    
		}
	}

	if (ls_sensor_info.sensor_used == 0) {
		printk("*** ls_used set to 0 !\n");
		printk("*** if use light_sensor,please put the sys_config.fex ls_used set to 1. \n");
		return 0;
	}
	
    return i2c_add_driver(&jsa1127_driver);
}
//module_init(jsa1127_init);
late_initcall(jsa1127_init);

static void __exit jsa1127_exit(void)
{
    dprintk(DEBUG_INFO,"deregister jsa1127 i2c driver!");
    i2c_del_driver(&jsa1127_driver);
    input_free_platform_resource(&(ls_sensor_info.input_type));
}
module_exit(jsa1127_exit);

module_param_named(debug_mask, debug_mask, int, 0644);
MODULE_AUTHOR("<Solteam Corp.>");
MODULE_DESCRIPTION("Solteam JSA1127 Ambient Light Sensor Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.8");

