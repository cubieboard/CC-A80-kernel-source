/*
*********************************************************************************************************
*                                                    LINUX-KERNEL
*                                        AllWinner Linux Platform Develop Kits
*                                                   Kernel Module
*
*                                    (c) Copyright 2006-2011, kevin.z China
*                                             All Rights Reserved
*
* File    : pm.h
* By      : kevin.z
* Version : v1.0
* Date    : 2011-5-27 14:08
* Descript: power manager
* Update  : date                auther      ver     notes
*********************************************************************************************************
*/
#ifndef __AW_PM_H__
#define __AW_PM_H__
/**max device number of pmu*/
#define PMU_MAX_DEVS        2

/**
*@name PMU command
*@{
*/
#define AW_PMU_SET          0x10
#define AW_PMU_VALID        0x20
/**
*@}
*/

/*
* the wakeup source of main cpu: cpu0
*/
#define CPU0_WAKEUP_MSGBOX      (1<<0)  /* external interrupt, pmu event for ex.    */
#define CPU0_WAKEUP_KEY         (1<<1)  /* key event    */

//the wakeup source of assistant cpu: cpus
#define CPUS_WAKEUP_LOWBATT     (1<<2 )
#define CPUS_WAKEUP_USB         (1<<3 )
#define CPUS_WAKEUP_AC          (1<<4 )
#define CPUS_WAKEUP_ASCEND      (1<<5 )
#define CPUS_WAKEUP_DESCEND     (1<<6 )
#define CPUS_WAKEUP_SHORT_KEY   (1<<7 )
#define CPUS_WAKEUP_LONG_KEY    (1<<8 )
#define CPUS_WAKEUP_IR          (1<<9 )
#define CPUS_WAKEUP_ALM0        (1<<10)
#define CPUS_WAKEUP_ALM1        (1<<11)
#define CPUS_WAKEUP_TIMEOUT     (1<<12)
#define CPUS_WAKEUP_GPIO        (1<<13)
#define CPUS_WAKEUP_USBMOUSE    (1<<14)
#define CPUS_WAKEUP_LRADC       (1<<15)
#define CPUS_WAKEUP_CODEC       (1<<16)
#define CPUS_WAKEUP_BAT_TEMP    (1<<17)
#define CPUS_WAKEUP_FULLBATT    (1<<18)
#define CPUS_WAKEUP_HMIC        (1<<19)
#define CPUS_WAKEUP_POWER_EXP   (1<<31)
#define CPUS_WAKEUP_KEY         (CPUS_WAKEUP_SHORT_KEY | CPUS_WAKEUP_LONG_KEY)

//for format all the wakeup gpio into one word.
#define WAKEUP_GPIO_PL(num)         (1 << (num))
#define WAKEUP_GPIO_PM(num)         (1 << (num + 12))
#define WAKEUP_GPIO_AXP(num)        (1 << (num + 24))
#define WAKEUP_GPIO_GROUP(group)    (1 << (group - 'A'))
#define PLL_NUM (11)
#define BUS_NUM (6)

typedef struct pll_para{
	int n;
	int k;
	int m;
	int p;
}pll_para_t;

typedef struct bus_para{
	int src;
	int pre_div;
	int div_ratio;
	int n;
	int m;
}bus_para_t;

typedef struct extended_standby{
	/*
	 * id of extended standby
	 */
	unsigned long id;
	/*
	 * clk tree para description as follow:
	 * vdd_dll : exdev : avcc : vcc_wifi : vcc_dram: vdd_sys : vdd_cpux : vdd_gpu : vcc_io : vdd_cpus
	 */
	int pwr_dm_en;	//bitx = 1, mean power on when sys is in standby state. otherwise, vice verse.

	/*
	 * Hosc: losc: ldo: ldo1
	 */
	int osc_en;

	/*
	 * pll_10: pll_9: pll_mipi: pll_8: pll_7: pll_6: pll_5: pll_4: pll_3: pll_2: pll_1
	 */
	int init_pll_dis;

	/*
	 * pll_10: pll_9: pll_mipi: pll_8: pll_7: pll_6: pll_5: pll_4: pll_3: pll_2: pll_1
	 */
	int exit_pll_en;

	/*
	 * set corresponding bit if it's pll factors need to be set some value.
	 * pll_10: pll_9: pll_mipi: pll_8: pll_7: pll_6: pll_5: pll_4: pll_3: pll_2: pll_1
	 */
	int pll_change;

	/*
	 * fill in the enabled pll freq factor sequently. unit is khz pll6: 0x90041811
	 * factor n/m/k/p already do the pretreatment of the minus one
	 */
	pll_para_t pll_factor[PLL_NUM];

	/*
	 * bus_en: cpu:axi:atb/apb:ahb1:apb1:apb2,
	 * normally, only clk src need be cared.
	 * so, at a31, only cpu:ahb1:apb2 need be cared.
	 * pll1->cpu -> axi
	 *	     -> atb/apb
	 * ahb1 -> apb1
	 * apb2
	 */
	int bus_change;

	/*
	 * bus_src: ahb1, apb2 src;
	 * option:  pllx:axi:hosc:losc
	 */
	bus_para_t bus_factor[BUS_NUM];
}extended_standby_t;

#define CPUS_ENABLE_POWER_EXP   (1<<31)
#define CPUS_WAKEUP_POWER_STA   (1<<1 )
#define CPUS_WAKEUP_POWER_CSM   (1<<0 )
typedef struct sst_power_info_para
{
	/*
	 * define for sun9iw1p1
	 * power_reg bit0 ~ 7 AXP_main REG10, bit 8~15 AXP_main REG12
	 * power_reg bit16~23 AXP_slav REG10, bit24~31 AXP_slav REG11
	 *
	 * AXP_main REG10: 0-off, 1-on
	 * bit7   bit6   bit5   bit4   bit3   bit2   bit1   bit0
	 * aldo2  aldo1  dcdc5  dcdc4  dcdc3  dcdc2  dcdc1  dc5ldo
	 *
	 * REG12: bit0~5:0-off, 1-on, bit6~7: 0-on, 1-off, dc1sw's power come from dcdc1
	 * bit7   bit6   bit5   bit4   bit3   bit2   bit1   bit0
	 * dc1sw  swout  aldo3  dldo2  dldo1  eldo3  eldo2  eldo1
	 *
	 * AXP_slave REG10: 0-off, 1-on. dcdc b&c is not used, ignore them.
	 * bit7   bit6   bit5   bit4   bit3   bit2   bit1   bit0
	 * aldo3  aldo2  aldo1  dcdce  dcdcd  dcdcc  dcdcb  dcdca
	 *
	 * AXP_slave REG11: 0-off, 1-on
	 * bit7   bit6   bit5   bit4   bit3   bit2   bit1   bit0
	 * swout  cldo3  cldo2  cldo1  bldo4  bldo3  bldo2  bldo1
	 */
	/*
	 * define for sun8iw5p1
	 * power_reg0 ~ 7 AXP_main REG10,  8~15 AXP_main REG12
	 * power_reg16~32 null
	 *
	 * AXP_main REG10: 0-off, 1-on
	 * bit7   bit6	 bit5   bit4   bit3   bit2   bit1   bit0
	 * aldo2  aldo1  dcdc5  dcdc4  dcdc3  dcdc2  dcdc1  dc5ldo
	 *
	 * REG12: 0-off, 1-on
	 * bit7   bit6   bit5   bit4   bit3   bit2   bit1   bit0
	 * dc1sw  dldo4  dldo3  dldo2  dldo1  eldo3  eldo2  eldo1
	 *
	 * REG13: bit16 aldo3, 0-off, 1-on
	 * REG90: bit17 gpio0/ldo, 0-off, 1-on
	 * REG92: bit18 gpio1/ldo, 0-off, 1-on
	 */
	unsigned int enable;		/* enable bit */
	unsigned int power_reg;		/* registers of power state should be */
	signed int system_power;	/* the max power of system, signed, power mabe negative when charge */
} sst_power_info_para_t;

typedef	struct super_standby_para
{
	unsigned long event;			//cpus wakeup event types
	unsigned long resume_code_src; 		//cpux resume code src
	unsigned long resume_code_length; 	//cpux resume code length
	unsigned long resume_entry; 		//cpux resume entry
	unsigned long timeout;			//wakeup after timeout seconds
	unsigned long gpio_enable_bitmap;
	unsigned long cpux_gpiog_bitmap;
	extended_standby_t *pextended_standby;
} super_standby_para_t;

typedef	struct normal_standby_para
{
	unsigned long event;		//cpus wakeup event types
	unsigned long timeout;		//wakeup after timeout seconds
	unsigned long gpio_enable_bitmap;
	unsigned long cpux_gpiog_bitmap;
	extended_standby_t *pextended_standby;
} normal_standby_para_t;


//define cpus wakeup src
#define CPUS_MEM_WAKEUP              (CPUS_WAKEUP_LOWBATT | CPUS_WAKEUP_USB | CPUS_WAKEUP_AC | \
						CPUS_WAKEUP_DESCEND | CPUS_WAKEUP_ASCEND | CPUS_WAKEUP_ALM0 | CPUS_WAKEUP_GPIO)
#define CPUS_BOOTFAST_WAKEUP         (CPUS_WAKEUP_LOWBATT | CPUS_WAKEUP_LONG_KEY |CPUS_WAKEUP_ALM0|CPUS_WAKEUP_USB|CPUS_WAKEUP_AC )

/*used in normal standby*/
#define CPU0_MEM_WAKEUP              (CPU0_WAKEUP_MSGBOX)
#define CPU0_BOOTFAST_WAKEUP         (CPU0_WAKEUP_MSGBOX)


/**
*@brief struct of pmu device arg
*/
struct aw_pmu_arg{
    unsigned int  twi_port;		/**<twi port for pmu chip   */
    unsigned char dev_addr;		/**<address of pmu device   */
};


/**
*@brief struct of standby
*/
struct aw_standby_para{
	unsigned int event;		/**<event type for system wakeup    */
	unsigned int axp_event;		/**<axp event type for system wakeup    */
	unsigned int debug_mask;	/* debug mask */
	signed int   timeout;		/**<time to power off system from now, based on second */
	unsigned long gpio_enable_bitmap;
};


/**
*@brief struct of power management info
*/
struct aw_pm_info{
    struct aw_standby_para		standby_para;   /* standby parameter            */
    struct aw_pmu_arg			pmu_arg;        /**<args used by main function  */
};

typedef struct sst_dram_info
{
	unsigned int dram_crc_enable;
	unsigned int *dram_crc_src;
	unsigned int dram_crc_len;
	unsigned int dram_crc_error;
	unsigned int dram_crc_total_count;
	unsigned int dram_crc_error_count;
} sst_dram_info_t;

typedef struct standby_info_para
{
	sst_power_info_para_t power_state; /* size 3W=12B */
	sst_dram_info_t dram_state; /*size 6W=24B */
} standby_info_para_t;

#endif /* __AW_PM_H__ */
