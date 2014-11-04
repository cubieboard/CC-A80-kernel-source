/*
 * LEDs driver for sunxi
 *
 * allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/module.h>

#include <linux/device.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/sys_config.h>
#include <linux/gpio.h>

static struct gpio_led gpio_leds[] = {
        {
                .name   = "led1",
                .active_low = 0,
                .default_state = LEDS_GPIO_DEFSTATE_OFF,
        },
        {
                .name   = "led2",
                .active_low = 0,
                .default_state = LEDS_GPIO_DEFSTATE_OFF,
        },
		{
                .name   = "led3",
                .active_low = 0,
                .default_state = LEDS_GPIO_DEFSTATE_OFF,
        },
};

static struct gpio_led_platform_data gpio_led_info = {
        .leds           = gpio_leds,
        .num_leds       = ARRAY_SIZE(gpio_leds),
};

static struct platform_device leds_gpio = {
        .name   = "sunxi-led",
        .id     = -1,
        .dev    = {
                .platform_data  = &gpio_led_info,
        },
};

struct gpio_led_data {
	struct led_classdev cdev;
	unsigned gpio;
	struct work_struct work;
	u8 new_level;
	u8 can_sleep;
	u8 active_low;
	u8 blinking;
	int (*platform_gpio_blink_set)(unsigned gpio, int state,
			unsigned long *delay_on, unsigned long *delay_off);
};

static void gpio_led_work(struct work_struct *work)
{
	struct gpio_led_data	*led_dat =
		container_of(work, struct gpio_led_data, work);

	if (led_dat->blinking) {
		led_dat->platform_gpio_blink_set(led_dat->gpio,
						 led_dat->new_level,
						 NULL, NULL);
		led_dat->blinking = 0;
	} else
		gpio_set_value_cansleep(led_dat->gpio, led_dat->new_level);
}

static void gpio_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct gpio_led_data *led_dat =
		container_of(led_cdev, struct gpio_led_data, cdev);
	int level;

	if (value == LED_OFF)
		level = 0;
	else
		level = 1;

	if (led_dat->active_low)
		level = !level;

	/* Setting GPIOs with I2C/etc requires a task context, and we don't
	 * seem to have a reliable way to know if we're already in one; so
	 * let's just assume the worst.
	 */
	if (led_dat->can_sleep) {
		led_dat->new_level = level;
		schedule_work(&led_dat->work);
	} else {
		if (led_dat->blinking) {
			led_dat->platform_gpio_blink_set(led_dat->gpio, level,
							 NULL, NULL);
			led_dat->blinking = 0;
		} else
			gpio_set_value(led_dat->gpio, level);
	}
}

static int gpio_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off)
{
	struct gpio_led_data *led_dat =
		container_of(led_cdev, struct gpio_led_data, cdev);

	led_dat->blinking = 1;
	return led_dat->platform_gpio_blink_set(led_dat->gpio, GPIO_LED_BLINK,
						delay_on, delay_off);
}

static int __devinit create_gpio_led(const struct gpio_led *template,
	struct gpio_led_data *led_dat, struct device *parent,
	int (*blink_set)(unsigned, int, unsigned long *, unsigned long *))
{
	int ret, state;

	led_dat->gpio = -1;

	/* skip leds that aren't available */
	if (!gpio_is_valid(template->gpio)) {
		printk(KERN_INFO "Skipping unavailable LED gpio %d (%s)\n",
				template->gpio, template->name);
		return 0;
	}

	ret = gpio_request(template->gpio, template->name);
	if (ret < 0)
		return ret;

	led_dat->cdev.name = template->name;
	led_dat->cdev.default_trigger = template->default_trigger;
	led_dat->gpio = template->gpio;
	led_dat->can_sleep = gpio_cansleep(template->gpio);
	led_dat->active_low = template->active_low;
	led_dat->blinking = 0;
	if (blink_set) {
		led_dat->platform_gpio_blink_set = blink_set;
		led_dat->cdev.blink_set = gpio_blink_set;
	}
	led_dat->cdev.brightness_set = gpio_led_set;
	if (template->default_state == LEDS_GPIO_DEFSTATE_KEEP)
		state = !!gpio_get_value_cansleep(led_dat->gpio) ^ led_dat->active_low;
	else
		state = (template->default_state == LEDS_GPIO_DEFSTATE_ON);
	led_dat->cdev.brightness = state ? LED_FULL : LED_OFF;
	if (!template->retain_state_suspended)
		led_dat->cdev.flags |= LED_CORE_SUSPENDRESUME;

	ret = gpio_direction_output(led_dat->gpio, led_dat->active_low ^ state);
	if (ret < 0)
		goto err;
		
	INIT_WORK(&led_dat->work, gpio_led_work);

	ret = led_classdev_register(parent, &led_dat->cdev);
	if (ret < 0)
		goto err;

	return 0;
err:
	gpio_free(led_dat->gpio);
	return ret;
}

static void delete_gpio_led(struct gpio_led_data *led)
{
	if (!gpio_is_valid(led->gpio))
		return;
	led_classdev_unregister(&led->cdev);
	cancel_work_sync(&led->work);
	gpio_free(led->gpio);
}

struct gpio_leds_priv {
	int num_leds;
	struct gpio_led_data leds[];
};

static inline int sizeof_gpio_leds_priv(int num_leds)
{
	return sizeof(struct gpio_leds_priv) +
		(sizeof(struct gpio_led_data) * num_leds);
}

static int __devinit gpio_led_probe(struct platform_device *pdev)
{
	struct gpio_led_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_leds_priv *priv;
	int i, ret = 0;

	if (pdata && pdata->num_leds) {
		priv = kzalloc(sizeof_gpio_leds_priv(pdata->num_leds),GFP_KERNEL);
		if (!priv)
			return -ENOMEM;

		priv->num_leds = pdata->num_leds;
		for (i = 0; i < priv->num_leds; i++) {
			ret = create_gpio_led(&pdata->leds[i],
					      &priv->leds[i],
					      &pdev->dev, pdata->gpio_blink_set);
			if (ret < 0) {
				/* On failure: unwind the led creations */
				for (i = i - 1; i >= 0; i--)
					delete_gpio_led(&priv->leds[i]);
				kfree(priv);
				return ret;
			}
		}
	} 
//	else {
//		priv = gpio_leds_create_of(pdev);
//		if (!priv)
//			return -ENODEV;
//	}

	platform_set_drvdata(pdev, priv);

	return 0;
}

static int __devexit gpio_led_remove(struct platform_device *pdev)
{
	struct gpio_leds_priv *priv = dev_get_drvdata(&pdev->dev);
	int i;

	for (i = 0; i < priv->num_leds; i++)
		delete_gpio_led(&priv->leds[i]);

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(priv);

	return 0;
}

static struct platform_driver gpio_led_driver = {
	.probe		= gpio_led_probe,
	.remove		= __devexit_p(gpio_led_remove),
	.driver		= {
		.name	= "sunxi-led",
		.owner	= THIS_MODULE,
	},
};
/**
 * led_sysconfig_para - get config info from sysconfig.fex file.
 * return value:  
 *                    = 0; success;
 *                    < 0; err
 */
static int led_sysconfig_para()
{
	int ret = -1;
	int i;
	int led_used = 0;
	int led_num = 0;
	script_item_u   val;
	script_item_value_type_e  type;
	char led_name[5];
	char trigger_name[32];
	pr_info("=====%s=====. \n", __func__);

	type = script_get_item("led_para", "led_used", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		printk("%s: led_used script_get_item  err. \n", __func__);
		goto script_get_item_err;
	}
	led_used = val.val;
	
	if (1 != led_used) {
		printk("%s: led_unused. \n",  __func__);
		goto script_get_item_err;
	}

	type = script_get_item("led_para", "led_num", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		printk("%s: led_num script_get_item err. \n",__func__ );
		goto script_get_item_err;
	}
	led_num = val.val;
	for(i=0;i<led_num;i++){
		
		sprintf(led_name, "%s%d", "led", i+1);
		sprintf(trigger_name, "%s%s", led_name, "_trigger");
		type = script_get_item("led_para", led_name, &val);
		if (SCIRPT_ITEM_VALUE_TYPE_PIO != type) {
			printk("script_get_item %s err\n",led_name);
			goto script_get_item_err;
		}
		gpio_leds[i].gpio = val.gpio.gpio;
		printk("%s gpio number is %d\n", led_name,gpio_leds[i].gpio);

		type = script_get_item("led_para",trigger_name, &val);
		if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
			printk("script_get_item %s err\n",trigger_name);
			goto script_get_item_err;
		}
		gpio_leds[i].default_trigger = val.str;
		printk("%s gpio number is %s\n", trigger_name,gpio_leds[i].default_trigger);
	}
	
	return 0;

script_get_item_err:
	printk("=========script_get_item_err============\n");
	return ret;
}
static int __init sunxi_led_init(void)
{
	int err = 0;
	printk("[led]:%s begin!\n", __func__);
	if(led_sysconfig_para()<0){
		printk("%s: led_sysconfig_para error\n", __FUNCTION__);
		return 0;
	}
	platform_device_register(&leds_gpio);
	platform_driver_register(&gpio_led_driver);
}

static void __exit sunxi_led_exit(void)
{
	platform_device_unregister(&leds_gpio);
	platform_driver_unregister(&gpio_led_driver);
}

module_init(sunxi_led_init);
module_exit(sunxi_led_exit);
MODULE_DESCRIPTION("SUNXI LED driver");
MODULE_LICENSE("GPL");
