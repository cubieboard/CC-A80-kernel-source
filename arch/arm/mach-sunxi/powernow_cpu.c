/*
 * arch/arm/mach-sun7i/powernow.c
 * (c) Copyright 2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * James Deng <csjamesdeng@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <mach/sys_config.h>
#include <mach/powernow.h>

#ifdef CONFIG_CPU_FREQ_GOV_AUTO_HOTPLUG
extern int hotplug_cpu_lock(int num_core);
extern int hotplug_cpu_unlock(int num_core);
#endif
extern void __init arch_get_fast_and_slow_cpus(struct cpumask *fast,
		struct cpumask *slow);

#define MAX_CLUSTERS	        2
#define EXTREMITY_CPUFREQ_RANGE     400000

#define CPUFREQ_MIN_C0_PERFORMANCE	120000
#define CPUFREQ_MAX_C0_PERFORMANCE	1600000
#define CPUFREQ_MIN_C0_EXTREMITY	912000
#define CPUFREQ_MAX_C0_EXTREMITY	1600000

#define CPUFREQ_MIN_C1_PERFORMANCE	120000
#define CPUFREQ_MAX_C1_PERFORMANCE	1800000
#define CPUFREQ_MIN_C1_EXTREMITY	1200000
#define CPUFREQ_MAX_C1_EXTREMITY	2100000

struct cpufreq_info{
	int cpufreq_min;
	int cpufreq_max;
};

struct cpufreq_powernow_info{
    struct cpufreq_info performance_freq[MAX_CLUSTERS];
    struct cpufreq_info extremity_freq[MAX_CLUSTERS];
};

struct cpufreq_powernow_info cpu_powernow_info = {
    .performance_freq[0] = {CPUFREQ_MIN_C0_PERFORMANCE, CPUFREQ_MAX_C0_PERFORMANCE },
    .performance_freq[1] = {CPUFREQ_MIN_C1_PERFORMANCE, CPUFREQ_MAX_C1_PERFORMANCE },
    .extremity_freq[0] = {CPUFREQ_MIN_C0_EXTREMITY,   CPUFREQ_MAX_C0_EXTREMITY   },
    .extremity_freq[1] = {CPUFREQ_MIN_C1_EXTREMITY,   CPUFREQ_MAX_C1_EXTREMITY   },
};

static struct cpumask hmp_fast_cpu_mask;
static struct cpumask hmp_slow_cpu_mask;
static int cur_mode = SW_POWERNOW_PERFORMANCE;
static void powernow_cpu(struct work_struct *work);
static DECLARE_WORK(powernow_cpu_work, powernow_cpu);
static struct workqueue_struct *powernow_cpu_wq;

static void powernow_cpu(struct work_struct *work)
{
    struct cpufreq_policy *cpu_policy;
    int next_cpu;
    struct cpumask new_mask;
    struct cpufreq_info *target_freq_info = NULL;
    int index = 5000;
    unsigned long mode = cur_mode;

    cpumask_clear(&new_mask);
    switch(mode){
    case SW_POWERNOW_EXTREMITY:
#ifdef CONFIG_CPU_FREQ_GOV_AUTO_HOTPLUG
        hotplug_cpu_lock(num_possible_cpus());
        while(index-- && mode == cur_mode){
            if (num_online_cpus() == num_possible_cpus()){
                break;
            }
            mdelay(1);
        }
#endif
        target_freq_info = cpu_powernow_info.extremity_freq;
        break;
        
    case SW_POWERNOW_PERFORMANCE:
    default:
        target_freq_info = cpu_powernow_info.performance_freq;
        break;
    }
    cpumask_copy(&new_mask, cpu_online_mask);
    next_cpu = 0;
    while(next_cpu < CONFIG_NR_CPUS && mode == cur_mode){
        cpu_policy = cpufreq_cpu_get(next_cpu);
        if (cpu_policy){
            cpumask_andnot(&new_mask, &new_mask, cpu_policy->cpus);
            if (cpumask_test_cpu(next_cpu, &hmp_fast_cpu_mask)){
                cpu_policy->user_policy.max = target_freq_info[1].cpufreq_max;
                cpu_policy->user_policy.min = target_freq_info[1].cpufreq_min;
                cpu_policy->user_policy.policy = cpu_policy->policy;
                cpu_policy->user_policy.governor = cpu_policy->governor;
            }else{
                cpu_policy->user_policy.max = target_freq_info[0].cpufreq_max;
                cpu_policy->user_policy.min = target_freq_info[0].cpufreq_min;
                cpu_policy->user_policy.policy = cpu_policy->policy;
                cpu_policy->user_policy.governor = cpu_policy->governor;
            }
            cpufreq_cpu_put(cpu_policy);
            cpufreq_update_policy(next_cpu);
        }
        next_cpu = cpumask_next(next_cpu, &new_mask);
    }

#ifdef CONFIG_CPU_FREQ_GOV_AUTO_HOTPLUG
    if (mode != SW_POWERNOW_EXTREMITY && mode == cur_mode){
        hotplug_cpu_unlock(num_possible_cpus());
    }
#endif
}

static int cpu_powernow_notifier_call(struct notifier_block *nfb,
        unsigned long mode, void *cmd)
{
    if (mode != cur_mode){
        cur_mode = mode;
        cancel_work_sync(&powernow_cpu_work);
        queue_work(powernow_cpu_wq, &powernow_cpu_work);
    }
    return NOTIFY_DONE;
}
 
static struct notifier_block cpu_powernow_notifier = {
    .notifier_call = cpu_powernow_notifier_call,
};

static int powercpu_pm_notify(struct notifier_block *nb, unsigned long event, void *dummy)
{
    if (event == PM_SUSPEND_PREPARE) {
        flush_work_sync(&powernow_cpu_work);
    }else if (event == PM_POST_SUSPEND) {
        if (cur_mode == SW_POWERNOW_EXTREMITY){
            cpu_powernow_notifier_call(NULL, SW_POWERNOW_EXTREMITY, NULL);
        }
    }
    return NOTIFY_OK;
}

static struct notifier_block powercpu_pm_notifier = {
    .notifier_call = powercpu_pm_notify,
};

/*
 * init cpu max/min frequency from sysconfig;
 * return: 0 - init cpu max/min successed, !0 - init cpu max/min failed;
 */
static int __init_freq_syscfg(void)
{
    script_item_u c0_max, c0_min, c1_max, c1_min;
    script_item_value_type_e type;
    unsigned int c0_freq_max = CPUFREQ_MAX_C0_PERFORMANCE;
    unsigned int c0_freq_min = CPUFREQ_MIN_C0_PERFORMANCE;
    unsigned int c1_freq_max = CPUFREQ_MAX_C1_PERFORMANCE;
    unsigned int c1_freq_min = CPUFREQ_MIN_C1_PERFORMANCE;

    type = script_get_item("dvfs_table", "c0_max_freq", &c0_max);
    if (SCIRPT_ITEM_VALUE_TYPE_INT == type) {
        c0_freq_max = c0_max.val/1000;
    }

    type = script_get_item("dvfs_table", "c0_min_freq", &c0_min);
    if (SCIRPT_ITEM_VALUE_TYPE_INT == type) {
        c0_freq_min = c0_min.val/1000;
    }

    type = script_get_item("dvfs_table", "c1_max_freq", &c1_max);
    if (SCIRPT_ITEM_VALUE_TYPE_INT == type) {
        c1_freq_max = c1_max.val/1000;
    }

    type = script_get_item("dvfs_table", "c1_min_freq", &c1_min);
    if (SCIRPT_ITEM_VALUE_TYPE_INT == type) {
        c1_freq_min = c1_min.val/1000;
    }

    if (c0_freq_min > c0_freq_max) {
        c0_freq_min = CPUFREQ_MIN_C0_PERFORMANCE;
        c0_freq_max = CPUFREQ_MAX_C0_PERFORMANCE;
    }

    if (c1_freq_min > c1_freq_max) {
        c1_freq_min = CPUFREQ_MIN_C1_PERFORMANCE;
        c1_freq_max = CPUFREQ_MAX_C1_PERFORMANCE;
    }
    cpu_powernow_info.performance_freq[0].cpufreq_min = c0_freq_min;
    cpu_powernow_info.performance_freq[0].cpufreq_max = c0_freq_max;
    cpu_powernow_info.performance_freq[1].cpufreq_min = c1_freq_min;
    cpu_powernow_info.performance_freq[1].cpufreq_max = c1_freq_max;
    return 0;

}

static int __init power_cpu_init(void) 
{
	powernow_cpu_wq = create_singlethread_workqueue("powernow_cpu_wq");
    arch_get_fast_and_slow_cpus(&hmp_fast_cpu_mask,&hmp_slow_cpu_mask); 
    __init_freq_syscfg();
    register_pm_notifier(&powercpu_pm_notifier);

    return register_sw_powernow_notifier(&cpu_powernow_notifier); 
}

late_initcall(power_cpu_init);

