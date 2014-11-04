/* include/linux/scenelock.h
 *
 * Copyright (C) 2013-2014 allwinner.
 *
 *  By      : liming
 *  Version : v1.0
 *  Date    : 2013-4-17 09:08
 */

#ifndef _LINUX_SCENELOCK_H
#define _LINUX_SCENELOCK_H

#include <linux/list.h>
#include <linux/ktime.h>
#include <linux/power/aw_pm.h>

typedef enum AW_POWER_SCENE
{
	SCENE_TALKING_STANDBY,
	SCENE_USB_STANDBY,
	SCENE_MP3_STANDBY,
	SCENE_BOOT_FAST,
	SCENE_SUPER_STANDBY,
	SCENE_GPIO_STANDBY,
	SCENE_NORMAL_STANDBY,
	SCENE_MISC_STANDBY,
	SCENE_MAX
} aw_power_scene_e;

typedef enum POWER_SCENE_FLAGS
{
	TALKING_STANDBY_FLAG           = (1<<0x0),
	USB_STANDBY_FLAG               = (1<<0x1),
	MP3_STANDBY_FLAG               = (1<<0x2),
	SUPER_STANDBY_FLAG             = (1<<0x3),
	NORMAL_STANDBY_FLAG            = (1<<0x4),
	GPIO_STANDBY_FLAG              = (1<<0x5),
	MISC_STANDBY_FLAG              = (1<<0x6),
	BOOT_FAST_STANDBY_FLAG         = (1<<0x7),
} power_scene_flags;

struct scene_lock {
#ifdef CONFIG_SCENELOCK
	struct list_head    link;
	int                 flags;
	int                 count;
	const char         *name;
#endif
};

typedef enum CPU_WAKEUP_SRC {
/* the wakeup source of main cpu: cpu0, only used in normal standby */
	CPU0_MSGBOX_SRC		= CPU0_WAKEUP_MSGBOX,  /* external interrupt, pmu event for ex. */
	CPU0_KEY_SRC		= CPU0_WAKEUP_KEY,     /* key event, volume home menu enter */

/* the wakeup source of assistant cpu: cpus */
	CPUS_LOWBATT_SRC	= CPUS_WAKEUP_LOWBATT,  /* low battery event */
	CPUS_USB_SRC		= CPUS_WAKEUP_USB ,     /* usb insert event */
	CPUS_AC_SRC		= CPUS_WAKEUP_AC,       /* charger insert event */
	CPUS_ASCEND_SRC		= CPUS_WAKEUP_ASCEND,   /* power key ascend event */
	CPUS_DESCEND_SRC	= CPUS_WAKEUP_DESCEND,  /* power key descend event */
	CPUS_SHORT_KEY_SRC	= CPUS_WAKEUP_SHORT_KEY,/* power key short press event */
	CPUS_LONG_KEY_SRC	= CPUS_WAKEUP_LONG_KEY, /* power key long press event */
	CPUS_IR_SRC		= CPUS_WAKEUP_IR,       /* IR key event */
	CPUS_ALM0_SRC		= CPUS_WAKEUP_ALM0,     /* alarm0 event */
	CPUS_ALM1_SRC		= CPUS_WAKEUP_ALM1,     /* alarm1 event */
	CPUS_TIMEOUT_SRC	= CPUS_WAKEUP_TIMEOUT,  /* debug test event */
	CPUS_GPIO_SRC		= CPUS_WAKEUP_GPIO,     /* GPIO interrupt event, only used in extended standby */
	CPUS_USBMOUSE_SRC	= CPUS_WAKEUP_USBMOUSE, /* USBMOUSE key event, only used in extended standby */
	CPUS_LRADC_SRC		= CPUS_WAKEUP_LRADC ,   /* key event, volume home menu enter, only used in extended standby */
	CPUS_CODEC_SRC		= CPUS_WAKEUP_CODEC,    /* codec irq, only used in extended standby*/
	CPUS_BAT_TEMP_SRC	= CPUS_WAKEUP_BAT_TEMP, /* baterry temperature low and high */
	CPUS_FULLBATT_SRC	= CPUS_WAKEUP_FULLBATT, /* baterry full */
	CPUS_HMIC_SRC		= CPUS_WAKEUP_HMIC,     /* earphone insert/pull event, only used in extended standby */
	CPUS_KEY_SL_SRC		= CPUS_WAKEUP_KEY       /* power key short and long press event */
}cpu_wakeup_src_e;

typedef struct extended_standby_manager{
	extended_standby_t *pextended_standby;
	unsigned long event;
	unsigned long wakeup_gpio_map;
	unsigned long wakeup_gpio_group;
}extended_standby_manager_t;

typedef struct scene_extended_standby {
	/*
	 * id of extended standby
	 */
	unsigned long id;
	/*
	 * scene type of extended standby
	 */
	aw_power_scene_e scene_type;
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

	struct list_head list; /* list of all extended standby */

	char * name;
} scene_extended_standby_t;

extern scene_extended_standby_t extended_standby[8];

const extended_standby_manager_t *get_extended_standby_manager(void);

bool set_extended_standby_manager(scene_extended_standby_t *local_standby);

int extended_standby_enable_wakeup_src(cpu_wakeup_src_e src, int para);

int extended_standby_disable_wakeup_src(cpu_wakeup_src_e src, int para);

int extended_standby_check_wakeup_state(cpu_wakeup_src_e src, int para);

int extended_standby_show_state(void);

#ifdef CONFIG_SCENELOCK

void scene_lock_init(struct scene_lock *lock, aw_power_scene_e type, const char *name);
void scene_lock_destroy(struct scene_lock *lock);
void scene_lock(struct scene_lock *lock);
void scene_unlock(struct scene_lock *lock);

/* check_scene_locked returns a zero value if the scene_lock is currently
 * locked.
 */
int check_scene_locked(aw_power_scene_e type);

/* scene_lock_active returns 0 if no scene locks of the specified type are active,
 * and non-zero if one or more scene locks are held.
 */
int scene_lock_active(struct scene_lock *lock);

int enable_wakeup_src(cpu_wakeup_src_e src, int para);

int disable_wakeup_src(cpu_wakeup_src_e src, int para);

int check_wakeup_state(cpu_wakeup_src_e src, int para);

int standby_show_state(void);

#else

static inline void scene_lock_init(struct scene_lock *lock, int type,
					const char *name) {}
static inline void scene_lock_destroy(struct scene_lock *lock) {}
static inline void scene_lock(struct scene_lock *lock) {}
static inline void scene_unlock(struct scene_lock *lock) {}

static inline int check_scene_locked(aw_power_scene_e type) { return -1; }
static inline int scene_lock_active(struct scene_lock *lock) { return 0; }

static inline int enable_wakeup_src(cpu_wakeup_src_e src, int para) { return 0; }
static inline int disable_wakeup_src(cpu_wakeup_src_e src, int para) { return 0; }
static inline int check_wakeup_state(cpu_wakeup_src_e src, int para) { return 0; }
static inline int standby_show_state(void) { return 0; }

#endif

#endif

