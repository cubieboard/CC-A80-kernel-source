
#ifndef _LINUX_SCENELOCK_DATA_H
#define _LINUX_SCENELOCK_DATA_H

#if defined CONFIG_ARCH_SUN8IW1P1
scene_extended_standby_t extended_standby[8] = {
	{
		.id           	= TALKING_STANDBY_FLAG,
		.scene_type	= SCENE_TALKING_STANDBY,
		.pwr_dm_en      = 0xf3,      //mean gpu, cpu is powered off.
		.osc_en         = 0xf,
		.init_pll_dis   = (~(0x12)), //mean pll2 is on. pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x21,      //mean enable pll1 and pll6
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "talking_standby",
	},
	{
		.id  		= USB_STANDBY_FLAG,
		.scene_type	= SCENE_USB_STANDBY,
		.pwr_dm_en      = 0xf3,      //mean gpu, cpu is powered off.
		.osc_en         = 0xf ,      //mean all osc is powered on.
		.init_pll_dis   = (~(0x30)), //mean pll6 is on.pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x1,       //mean enable pll1 and pll6
		.pll_change     = 0x20,
		.pll_factor[5]  = {0x18,0x1,0x1,0},
		.bus_change     = 0x4,
		.bus_factor[2]  = {0x8,0,0x3,0,0},
		.name		= "usb_standby",
	},
	{
		.id  		= MP3_STANDBY_FLAG,
		.scene_type	= SCENE_MP3_STANDBY,
		.name		= "mp3_standby",
	},
	{
		.id		= BOOT_FAST_STANDBY_FLAG,
		.scene_type	= SCENE_BOOT_FAST,
		.name		= "boot_fast",
	},
	{
		.id             = SUPER_STANDBY_FLAG,
		.scene_type	= SCENE_SUPER_STANDBY,
		.pwr_dm_en      = 0xa1,      //mean avcc, dram, cpus is on.
		.osc_en         = 0x4,	// mean losc is on.
		.init_pll_dis   = (~(0x10)), //mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "super_standby",
	},
	{
		.id             = NORMAL_STANDBY_FLAG,
		.scene_type	= SCENE_NORMAL_STANDBY,
		.pwr_dm_en      = 0xfff,     	 //mean all power domain is on.
		.osc_en         = 0xf,		// mean Hosc&Losc&ldo&ldo1 is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = (~(0x10)),
		.pll_change     = 0x1,
		.pll_factor[0]  = {0x10,0,0,0},
		.bus_change     = 0x5,	             //ahb1&apb2 is changed.
		.bus_factor[0]  = {0x2,0,0,0,0},     //apb2 src is Hosc.
		.bus_factor[2]  = {0x2,0,0,0,0},     //ahb1 src is Hosc.
		.name		= "normal_standby",
	},
	{
		.id             = GPIO_STANDBY_FLAG,
		.scene_type	= SCENE_GPIO_STANDBY,
		.pwr_dm_en      = 0xb3,     	 //mean avcc, dram, sys, io, cpus is on.
		.osc_en         = 0x4,		// mean losc is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x4,	//ahb1 is changed.
		.bus_factor[2]  = {0x0,0,0,0,0},     //src is losc.
		.name		= "gpio_standby",
	},
	{
		.id		= MISC_STANDBY_FLAG,
		.scene_type	= SCENE_MISC_STANDBY,
		.name		= "misc_standby",
	},
};
#elif defined CONFIG_ARCH_SUN8IW3P1
scene_extended_standby_t extended_standby[8] = {
	 {
		.id           	= TALKING_STANDBY_FLAG,
		.scene_type	= SCENE_TALKING_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf,
		.init_pll_dis   = (~(0x12)), //mean pll2 is on. pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x21,      //mean enable pll1 and pll6
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "talking_standby",
	},
	{
		.id  		= USB_STANDBY_FLAG,
		.scene_type	= SCENE_USB_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf ,      //mean all osc is powered on.
		.init_pll_dis   = (~(0x30)), //mean pll6 is on.pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x1,       //mean enable pll1 and pll6
		.pll_change     = 0x20,
		.pll_factor[5]  = {0x18,0x1,0x1,0},
		.bus_change     = 0x4,
		.bus_factor[2]  = {0x8,0,0x3,0,0},
		.name		= "usb_standby",
	},
	{
		.id  		= MP3_STANDBY_FLAG,
		.scene_type	= SCENE_MP3_STANDBY,
		.name		= "mp3_standby",
	},
	{
		.id  		= BOOT_FAST_STANDBY_FLAG,
		.scene_type	= SCENE_BOOT_FAST,
		.name		= "boot_fast",
	},
	{
		.id             = SUPER_STANDBY_FLAG,
		.scene_type	= SCENE_SUPER_STANDBY,
		.pwr_dm_en      = 0xa1,      //mean avcc, dram, cpus is on.
		.osc_en         = 0x4,	     // mean losc is on.
		.init_pll_dis   = (~(0x10)), //mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "super_standby",
	},
	{
		.id             = NORMAL_STANDBY_FLAG,
		.scene_type	= SCENE_NORMAL_STANDBY,
		.pwr_dm_en      = 0xfff,     	 //mean all power domain is on.
		.osc_en         = 0xf,		// mean Hosc&Losc&ldo&ldo1 is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = (~(0x10)),
		.pll_change     = 0x1,
		.pll_factor[0]  = {0x10,0,0,0},
		.bus_change     = 0x5,	             //ahb1&apb2 is changed.
		.bus_factor[0]  = {0x2,0,0,0,0},     //apb2 src is Hosc.
		.bus_factor[2]  = {0x2,0,0,0,0},     //ahb1 src is Hosc.
		.name		= "normal_standby",
	},
	{
		.id             = GPIO_STANDBY_FLAG,
		.scene_type	= SCENE_GPIO_STANDBY,
		.pwr_dm_en      = 0xb3,     	 //mean avcc, dram, sys, io, cpus is on.
		.osc_en         = 0x4,		// mean losc is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x4,	//ahb1 is changed.
		.bus_factor[2]  = {0x0,0,0,0,0},     //src is losc.
		.name		= "gpio_standby",
	},
	{
		.id		= MISC_STANDBY_FLAG,
		.scene_type	= SCENE_MISC_STANDBY,
		.name		= "misc_standby",
	},
};
#elif defined CONFIG_ARCH_SUN8IW5P1	// FIXME: need double check.
scene_extended_standby_t extended_standby[8] = {
	 {
		.id           	= TALKING_STANDBY_FLAG,
		.scene_type	= SCENE_TALKING_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf,
		.init_pll_dis   = (~(0x12)), //mean pll2 is on. pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x21,      //mean enable pll1 and pll6
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "talking_standby",
	},
	{
		.id  		= USB_STANDBY_FLAG,
		.scene_type	= SCENE_USB_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf ,      //mean all osc is powered on.
		.init_pll_dis   = (~(0x30)), //mean pll6 is on.pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x1,       //mean enable pll1 and pll6
		.pll_change     = 0x20,
		.pll_factor[5]  = {0x18,0x1,0x1,0},
		.bus_change     = 0x4,
		.bus_factor[2]  = {0x8,0,0x3,0,0},
		.name		= "usb_standby",
	},
	{
		.id  		= MP3_STANDBY_FLAG,
		.scene_type	= SCENE_MP3_STANDBY,
		.name		= "mp3_standby",
	},
	{
		.id  		= BOOT_FAST_STANDBY_FLAG,
		.scene_type	= SCENE_BOOT_FAST,
		.name		= "boot_fast",
	},
	{
		.id             = SUPER_STANDBY_FLAG,
		.scene_type	= SCENE_SUPER_STANDBY,
		.pwr_dm_en      = 0xa1,      //mean avcc, dram, cpus is on.
		.osc_en         = 0x4,	// mean losc is on.
		.init_pll_dis   = (~(0x10)), //mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "super_standby",
	},
	{
		.id             = NORMAL_STANDBY_FLAG,
		.scene_type	= SCENE_NORMAL_STANDBY,
		.pwr_dm_en      = 0xfff,     	 //mean all power domain is on.
		.osc_en         = 0xf,		// mean Hosc&Losc&ldo&ldo1 is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = (~(0x10)),
		.pll_change     = 0x1,
		.pll_factor[0]  = {0x10,0,0,0},
		.bus_change     = 0x5,	             //ahb1&apb2 is changed.
		.bus_factor[0]  = {0x2,0,0,0,0},     //apb2 src is Hosc.
		.bus_factor[2]  = {0x2,0,0,0,0},     //ahb1 src is Hosc.
		.name		= "normal_standby",
	},
	{
		.id             = GPIO_STANDBY_FLAG,
		.scene_type	= SCENE_GPIO_STANDBY,
		.pwr_dm_en      = 0xb3,     	 //mean avcc, dram, sys, io, cpus is on.
		.osc_en         = 0x4,		// mean losc is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x4,
		.bus_factor[2]  = {0x0,0,0,0,0},     //src is losc.
		.name		= "gpio_standby",
	},
	{
		.id		= MISC_STANDBY_FLAG,
		.scene_type	= SCENE_MISC_STANDBY,
		.name		= "misc_standby",
	},
};
#elif defined CONFIG_ARCH_SUN8IW6P1	// FIXME: need double check.
scene_extended_standby_t extended_standby[8] = {
	 {
		.id           	= TALKING_STANDBY_FLAG,
		.scene_type	= SCENE_TALKING_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf,
		.init_pll_dis   = (~(0x12)), //mean pll2 is on. pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x21,      //mean enable pll1 and pll6
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "talking_standby",
	},
	{
		.id  		= USB_STANDBY_FLAG,
		.scene_type	= SCENE_USB_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf ,      //mean all osc is powered on.
		.init_pll_dis   = (~(0x30)), //mean pll6 is on.pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x1,       //mean enable pll1 and pll6
		.pll_change     = 0x20,
		.pll_factor[5]  = {0x18,0x1,0x1,0},
		.bus_change     = 0x4,
		.bus_factor[2]  = {0x8,0,0x3,0,0},
		.name		= "usb_standby",
	},
	{
		.id  		= MP3_STANDBY_FLAG,
		.scene_type	= SCENE_MP3_STANDBY,
		.name		= "mp3_standby",
	},
	{
		.id  		= BOOT_FAST_STANDBY_FLAG,
		.scene_type	= SCENE_BOOT_FAST,
		.name		= "boot_fast",
	},
	{
		.id             = SUPER_STANDBY_FLAG,
		.scene_type	= SCENE_SUPER_STANDBY,
		.pwr_dm_en      = 0xa1,      //mean avcc, dram, cpus is on.
		.osc_en         = 0x4,	// mean losc is on.
		.init_pll_dis   = (~(0x10)), //mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "super_standby",
	},
	{
		.id             = NORMAL_STANDBY_FLAG,
		.scene_type	= SCENE_NORMAL_STANDBY,
		.pwr_dm_en      = 0xfff,     	 //mean all power domain is on.
		.osc_en         = 0xf,		// mean Hosc&Losc&ldo&ldo1 is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = (~(0x10)),
		.pll_change     = 0x1,
		.pll_factor[0]  = {0x10,0,0,0},
		.bus_change     = 0x5,	             //ahb1&apb2 is changed.
		.bus_factor[0]  = {0x2,0,0,0,0},     //apb2 src is Hosc.
		.bus_factor[2]  = {0x2,0,0,0,0},     //ahb1 src is Hosc.
		.name		= "normal_standby",
	},
	{
		.id             = GPIO_STANDBY_FLAG,
		.scene_type	= SCENE_GPIO_STANDBY,
		.pwr_dm_en      = 0xb3,     	 //mean avcc, dram, sys, io, cpus is on.
		.osc_en         = 0x4,		// mean losc is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x4,
		.bus_factor[2]  = {0x0,0,0,0,0},     //src is losc.
		.name		= "gpio_standby",
	},
	{
		.id		= MISC_STANDBY_FLAG,
		.scene_type	= SCENE_MISC_STANDBY,
		.name		= "misc_standby",
	},
};
#elif defined (CONFIG_ARCH_SUN9IW1)	//FIXME. the config need double check.
scene_extended_standby_t extended_standby[8] = {
	{
		.id           	= TALKING_STANDBY_FLAG,
		.scene_type	= SCENE_TALKING_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf,
		.init_pll_dis   = (~(0x12)), //mean pll2 is on. pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x21,      //mean enable pll1 and pll6
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "talking_standby",
	},
	{
		.id  		= USB_STANDBY_FLAG,
		.scene_type	= SCENE_USB_STANDBY,
		.pwr_dm_en      = 0xf7,      //mean cpu is powered off.
		.osc_en         = 0xf ,      //mean all osc is powered on.
		.init_pll_dis   = (~(0x30)), //mean pll6 is on.pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x1,       //mean enable pll1 and pll6
		.pll_change     = 0x20,
		.pll_factor[5]  = {0x18,0x1,0x1,0},
		.bus_change     = 0x4,
		.bus_factor[2]  = {0x8,0,0x3,0,0},
		.name		= "usb_standby",
	},
	{
		.id  		= MP3_STANDBY_FLAG,
		.scene_type	= SCENE_MP3_STANDBY,
		.name		= "mp3_standby",
	},
	{
		.id		= BOOT_FAST_STANDBY_FLAG,
		.scene_type	= SCENE_BOOT_FAST,
		.name		= "boot_fast",
	},
	{
		.id             = SUPER_STANDBY_FLAG,
		.scene_type	= SCENE_SUPER_STANDBY,
		.pwr_dm_en      = 0xa1,      //mean avcc, dram, cpus is on.
		.osc_en         = 0x4,	// mean losc is on.
		.init_pll_dis   = (~(0x10)), //mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x0,
		.name		= "super_standby",
	},
	{
		.id             = NORMAL_STANDBY_FLAG,
		.scene_type	= SCENE_NORMAL_STANDBY,
		.pwr_dm_en      = 0xfff,     	 //mean all power domain is on.
		.osc_en         = 0xf,		// mean Hosc&Losc&ldo&ldo1 is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = (~(0x10)),
		.pll_change     = 0x1,
		.pll_factor[0]  = {0x10,0,0,0},
		.bus_change     = 0x5,	             //ahb1&apb2 is changed.
		.bus_factor[0]  = {0x2,0,0,0,0},     //apb2 src is Hosc.
		.bus_factor[2]  = {0x2,0,0,0,0},     //ahb1 src is Hosc.
		.name		= "normal_standby",
	},
	{
		.id             = GPIO_STANDBY_FLAG,
		.scene_type	= SCENE_GPIO_STANDBY,
		.pwr_dm_en      = 0xb3,     	 //mean avcc, dram, sys, io, cpus is on.
		.osc_en         = 0x4,		// mean losc is on.
		.init_pll_dis   = (~(0x10)), 	//mean pll5 is shundowned by dram driver.
		.exit_pll_en    = 0x0,
		.pll_change     = 0x0,
		.bus_change     = 0x4,	//ahb1 is changed.
		.bus_factor[2]  = {0x0,0,0,0,0},     //src is losc.
		.name		= "gpio_standby",
	},
	{
		.id		= MISC_STANDBY_FLAG,
		.scene_type	= SCENE_MISC_STANDBY,
		.name		= "misc_standby",
	},
};
#endif

#endif

