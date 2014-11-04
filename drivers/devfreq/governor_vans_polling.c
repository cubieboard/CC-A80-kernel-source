/*
 *  drivers/devfreq/governor_vans_polling.c
 *
 *  Copyright (C) 2012 Reuuimlla Technology
 *  pannan<pannan@reuuimllatech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/devfreq.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/module.h>
#include "governor.h"
#include "dramfreq/sunxi-dramfreq.h"

#if (1)
    #define VANS_DBG(format,args...)   printk("[vans] "format,##args)
#else
    #define VANS_DBG(format,args...)   do{}while(0)
#endif

#define VANS_ERR(format,args...)   printk(KERN_ERR "[vans] ERR:"format,##args)

#ifdef CONFIG_CPU_FREQ_GOV_FANTASYS
extern int cpufreq_fantasys_cpu_lock(int num_core);
extern int cpufreq_fantasys_cpu_unlock(int num_core);
static int online_backup_fantasys = 0;
extern atomic_t g_hotplug_lock;
#endif

static struct cpumask online_backup_cpumask;
static int online_backup = 0;
static struct mutex timer_mutex;
static unsigned int g_bw_debug = 0;
extern struct master_bw_info master_info_list[MASTER_MAX];
extern unsigned int master_bw_usage[MASTER_MAX];

static void master_bw_dbg(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(master_info_list); i++) {
        VANS_DBG("%s: %u(KB/s)\n", master_info_list[i].name, master_bw_usage[master_info_list[i].type]);
    }
}

static int devfreq_vans_func(struct devfreq *df, unsigned long *freq)
{
    struct devfreq_dev_status stat;
    int err = df->profile->get_dev_status(df->dev.parent, &stat);
    unsigned int total_bw, bit_wide;
    unsigned long freq_target;

    if (err)
        return err;

    if (unlikely(g_bw_debug))
        master_bw_dbg();

    bit_wide = *(unsigned int *)stat.private_data;
    total_bw = master_bw_usage[MASTER_TOTAL];
    /*
    * freq_target * 2 * (N / 8) = total_bw / efficiency_factor
    * N = 8,16
    */
    freq_target = total_bw * 100 * 8 / (DRAMFREQ_EFFICIENCY_FACTOR * 2 * bit_wide);
    VANS_DBG("freq_target=%luKHz\n", freq_target);
    freq_target = __get_valid_freq(freq_target);
    *freq = freq_target;

    VANS_DBG("try to switch dram freq to %luKHz\n", *freq);

    return 0;
}

static int dramfreq_lock_cpu0(void)
{
    struct cpufreq_policy policy;
    int cpu, nr_down;

    if (cpufreq_get_policy(&policy, 0)) {
        VANS_ERR("can not get cpu policy\n");
        return -1;
    }

#ifdef CONFIG_CPU_FREQ_GOV_FANTASYS
    if (!strcmp(policy.governor->name, "fantasys")) {
        online_backup_fantasys = atomic_read(&g_hotplug_lock);
        if ((online_backup_fantasys >= 0) && (online_backup_fantasys <= nr_cpu_ids)) {
            cpufreq_fantasys_cpu_lock(1);
            while (num_online_cpus() != 1) {
                msleep(50);
            }
        }
        goto out;
    }
#endif

    online_backup = num_online_cpus();
    cpumask_clear(&online_backup_cpumask);
    cpumask_copy(&online_backup_cpumask, cpu_online_mask);
    if ((online_backup > 1) && (online_backup <= nr_cpu_ids)) {
        nr_down = online_backup - 1;
        for_each_cpu(cpu, &online_backup_cpumask) {
            if (cpu == 0)
                continue;

            if (nr_down-- == 0)
                break;

            printk("cpu down:%d\n", cpu);
            cpu_down(cpu);
        }
    } else if (online_backup == 1) {
        printk("only cpu0 online, need not down\n");
    } else {
        VANS_ERR("ERROR online cpu sum is %d\n", online_backup);
    }

#ifdef CONFIG_CPU_FREQ_GOV_FANTASYS
out:
#endif
    return 0;
}

static int dramfreq_unlock_cpu0(void)
{
    struct cpufreq_policy policy;
    int cpu, nr_up;

    if (cpufreq_get_policy(&policy, 0)) {
        VANS_ERR("can not get cpu policy\n");
        return -1;
    }

#ifdef CONFIG_CPU_FREQ_GOV_FANTASYS
    if (!strcmp(policy.governor->name, "fantasys")) {
        if (online_backup_fantasys >= 0 && online_backup_fantasys <= nr_cpu_ids) {
            printk("online_backup_fantasys is %d\n", online_backup_fantasys);
            cpufreq_fantasys_cpu_unlock(1);
            if (online_backup_fantasys) {
                cpufreq_fantasys_cpu_lock(online_backup_fantasys);
                while (num_online_cpus() != online_backup_fantasys) {
                    msleep(50);
                }
            }
        }
        goto out;
    }
#endif

    if (online_backup > 1 && online_backup <= nr_cpu_ids) {
        nr_up = online_backup - 1;
        for_each_cpu(cpu, &online_backup_cpumask) {
            if (cpu == 0)
                continue;

            if (nr_up-- == 0)
                break;

            printk("cpu up:%d\n", cpu);
            cpu_up(cpu);
        }
    } else if (online_backup == 1) {
        printk("only cpu0 online, need not up\n");
    } else {
        VANS_ERR("ERROR online backup cpu sum is %d\n", online_backup);
    }

#ifdef CONFIG_CPU_FREQ_GOV_FANTASYS
out:
#endif
    return 0;
}

static ssize_t show_bw_debug(struct device *dev, struct device_attribute *attr,
            char *buf)
{
    return sprintf(buf, "%u\n", g_bw_debug);
}

static ssize_t store_bw_debug(struct device *dev, struct device_attribute *attr,
            const char *buf, size_t count)
{
    unsigned int input;
    int ret;
    ret = sscanf(buf, "%u", &input);
    if (ret != 1)
        return -EINVAL;
    g_bw_debug = input > 0;
    return count;
}

static DEVICE_ATTR(bw_debug, 0600, show_bw_debug, store_bw_debug);

static struct attribute *dev_entries[] = {
    &dev_attr_bw_debug.attr,
    NULL,
};

static struct attribute_group dev_attr_group = {
    .name   = "vans",
    .attrs  = dev_entries,
};

static int vans_init(struct devfreq *devfreq)
{
    cpumask_clear(&online_backup_cpumask);
    mutex_init(&timer_mutex);

    return sysfs_create_group(&devfreq->dev.kobj, &dev_attr_group);
}

static void vans_exit(struct devfreq *devfreq)
{
    sysfs_remove_group(&devfreq->dev.kobj, &dev_attr_group);
    mutex_destroy(&timer_mutex);
}

const struct devfreq_governor devfreq_vans = {
    .name = "vans",
    .init = vans_init,
    .exit = vans_exit,
    .get_target_freq = devfreq_vans_func,
};

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("'governor_vans' - a simple dram frequency policy");
MODULE_AUTHOR("pannan");
