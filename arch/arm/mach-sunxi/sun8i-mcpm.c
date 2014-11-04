/*
 * linux/arch/arm/mach-sunxi/sun8i-mcpm.c
 *
 * Copyright(c) 2013-2015 Allwinnertech Co., Ltd.
 *         http://www.allwinnertech.com
 *
 * Author: sunny <sunny@allwinnertech.com>
 *
 * allwinner sun8i platform multi-cluster power management operations.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/delay.h>

#include <asm/mcpm.h>
#include <asm/proc-fns.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/cp15.h>
#include <asm/smp_plat.h>

#include <mach/platform.h>
#include <mach/cci.h>
#include <mach/sun8i/platsmp.h>

#define SUN8I_POWER_SWITCH_OFF	(0xFF)
#define SUN8I_POWER_SWITCH_ON	(0x00)
#define SUN8I_IS_WFI_MODE(cluster, cpu)  (readl(SUNXI_CPUXCFG_VBASE + SUNXI_CLUSTER_CPU_STATUS(cluster)) & (1 << (16 + cpu)))

/*
 * We can't use regular spinlocks. In the switcher case, it is possible
 * for an outbound CPU to call power_down() after its inbound counterpart
 * is already live using the same logical CPU number which trips lockdep
 * debugging.
 */
static arch_spinlock_t sun8i_mcpm_lock = __ARCH_SPIN_LOCK_UNLOCKED;

/* sunxi cluster and cpu power-management models */
static void __iomem *sun8i_prcm_base;
static void __iomem *sun8i_cpuxcfg_base;
static void __iomem *sun8i_cpuscfg_base;

/* sunxi cluster and cpu use status,
 * this is use to detect the first-man and last-man.
 */
static int sun8i_cpu_use_count[MAX_NR_CLUSTERS][MAX_CPUS_PER_CLUSTER];
static int sun8i_cluster_use_count[MAX_NR_CLUSTERS];

/* use to sync cpu kill and die sync,
 * the cpu which will to die just process cpu internal operations,
 * such as invalid L1 cache and shutdown SMP bit,
 * the cpu power-down relative operations should be executed by killer cpu.
 * so we must ensure the kill operation should after cpu die operations.
 */
static cpumask_t dead_cpus[MAX_NR_CLUSTERS];

/* hrtimer, use to power-down cluster process,
 * power-down cluster use the time-delay solution mainly for keep long time
 * L2-cache coherecy, this can decrease IKS switch and cpu power-on latency.
 */
static struct hrtimer cluster_power_down_timer;
static int sun8i_cpu_power_control(unsigned int cpu, unsigned int cluster, bool enable)
{
	unsigned int value;
	if (enable) {
		/* step1: power switch on */
		writel(SUN8I_POWER_SWITCH_ON, sun8i_prcm_base + SUNXI_CPU_PWR_CLAMP(cluster, cpu));
		while(SUN8I_POWER_SWITCH_ON != readl(sun8i_prcm_base + SUNXI_CPU_PWR_CLAMP(cluster, cpu))) {
			;
		}
		mdelay(5);

		/* step2: power gating off */
		value = readl(sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		value &= (~(0x1<<cpu));
		writel(value, sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		mdelay(2);

		pr_info("sun8i power-up cluster-%d cpu-%d\n", cluster, cpu);

		/* step3: clear reset */
		value  = readl(sun8i_cpuscfg_base + SUNXI_CLUSTER_PWRON_RESET(cluster));
		value |= (1<<cpu);
		writel(value, sun8i_cpuscfg_base + SUNXI_CLUSTER_PWRON_RESET(cluster));

		/* step4: core reset */
		value  = readl(sun8i_cpuxcfg_base + SUNXI_CPU_RST_CTRL(cluster));
		value |= (1<<cpu);
		writel(value, sun8i_cpuxcfg_base + SUNXI_CPU_RST_CTRL(cluster));

		pr_info("sun8i power-up cluster-%d cpu-%d already\n", cluster, cpu);
	} else {
		/*
		 * power-down cpu core process
		 */
		/* step1: assert core reset */
		value  = readl(sun8i_cpuxcfg_base + SUNXI_CPU_RST_CTRL(cluster));
		value &= ~(1 << cpu);
		writel(value, sun8i_cpuxcfg_base + SUNXI_CPU_RST_CTRL(cluster));

		/* step2: assert reset */
		value  = readl(sun8i_cpuscfg_base + SUNXI_CLUSTER_PWRON_RESET(cluster));
		value &= ~(1 << cpu);
		writel(value, sun8i_cpuscfg_base + SUNXI_CLUSTER_PWRON_RESET(cluster));


		/* step3: power gating on */
 		mdelay(2);
		value = readl(sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		value |= (1 << cpu);
		writel(value, sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));

		mdelay(5);

		/* step4: power switch off */
		writel(SUN8I_POWER_SWITCH_OFF, sun8i_prcm_base + SUNXI_CPU_PWR_CLAMP(cluster, cpu));
		mdelay(1);
		pr_info("sun8i power-down cluster-%d cpu-%d already\n", cluster, cpu);
	}
	return 0;
}

static int sun8i_cluster_power_control(unsigned int cluster, bool enable)
{
	unsigned int value;
	if (enable) {

		pr_info("sun8i power-up cluster-%d\n", cluster);

		/* clear cluster power-off gating */
		value = readl(sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		value &= (~(0x1<<4));
		writel(value, sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));

		pr_info("sun8i power-up cluster-%d ok\n", cluster);
	} else {

		pr_info("sun8i power-down cluster-%d\n", cluster);

		/* enable cluster power-off gating */
		value = readl(sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		value |= (1<<4);
		writel(value, sun8i_prcm_base + SUNXI_CLUSTER_PWROFF_GATING(cluster));

		pr_info("sun8i power-down cluster-%d ok\n", cluster);
	}

	/* disable cluster clock */

	/* de-assert cluster reset */

	/* power-down cluster dcdc regulator */

	return 0;
}

static int sun8i_cluster_power_status(unsigned int cluster)
{
	int          status = 0;
	unsigned int value;

	value = readl(sun8i_cpuxcfg_base + SUNXI_CLUSTER_CPU_STATUS(cluster));

	/* cluster WFI status :
	 * all cpu cores enter WFI mode + L2Cache enter WFI status
	 */
	if ((((value >> 16) & 0xf) == 0xf) && (value & 0x1)) {
		status = 1;
	}

	return status;
}

enum hrtimer_restart sun8i_cluster_power_down(struct hrtimer *timer)
{
	ktime_t              period;
	int                  cluster;
	enum hrtimer_restart ret;
	arch_spin_lock(&sun8i_mcpm_lock);
	if (sun8i_cluster_use_count[0] == 0) {
		cluster = 0;
	} else if (sun8i_cluster_use_count[1] == 0) {
		cluster = 1;
	} else {
		ret = HRTIMER_NORESTART;
		goto end;
	}

	if (sun8i_cluster_power_status(cluster)) {
		period = ktime_set(0, 10000000);
		hrtimer_forward_now(timer, period);
		ret = HRTIMER_RESTART;
		goto end;
	} else {
		disable_cci_snoops(cluster);
		sun8i_cluster_power_control(cluster, 0);
		ret = HRTIMER_NORESTART;
	}
end:
	arch_spin_unlock(&sun8i_mcpm_lock);
	return ret;
}

static int sun8i_mcpm_power_up(unsigned int cpu, unsigned int cluster)
{
	pr_info("%s: cluster %u cpu %u\n", __func__, cluster, cpu);
	if (cpu >= MAX_CPUS_PER_CLUSTER || cluster >= MAX_NR_CLUSTERS) {
		return -EINVAL;
	}
	/*
	 * Since this is called with IRQs enabled, and no arch_spin_lock_irq
	 * variant exists, we need to disable IRQs manually here.
	 */
	local_irq_disable();
	arch_spin_lock(&sun8i_mcpm_lock);

	sun8i_cpu_use_count[cluster][cpu]++;
	if (sun8i_cpu_use_count[cluster][cpu] == 1) {

		sun8i_cluster_use_count[cluster]++;
		if (sun8i_cluster_use_count[cluster] == 1) {
			/* first-man should power-up cluster resource */
			pr_info("sun8i power-on first-man cpu-%d on cluster-%d", cpu, cluster);
			sun8i_cluster_power_control(cluster, 1);

			/* enable cci snoops */
			enable_cci_snoops(cluster);
		}
		/* power-up cpu core */
		sun8i_cpu_power_control(cpu, cluster, 1);
	} else if (sun8i_cpu_use_count[cluster][cpu] != 2) {
		/*
		 * The only possible values are:
		 * 0 = CPU down
		 * 1 = CPU (still) up
		 * 2 = CPU requested to be up before it had a chance
		 *     to actually make itself down.
		 * Any other value is a bug.
		 */
		BUG();
	} else {
		pr_info("sun8i power-on cluster-%d cpu-%d been skiped, usage count %d\n",
		         cluster, cpu, sun8i_cpu_use_count[cluster][cpu]);
	}
	arch_spin_unlock(&sun8i_mcpm_lock);
	local_irq_enable();

	return 0;
}

static void sun8i_mcpm_power_down(void)
{
	unsigned int mpidr, cpu, cluster;
	bool last_man = false, skip_power_down = false;
	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_info("%s: cluster %u cpu %u\n", __func__, cluster, cpu);

	BUG_ON(cpu >= MAX_CPUS_PER_CLUSTER || cluster >= MAX_NR_CLUSTERS);

	__mcpm_cpu_going_down(cpu, cluster);

	arch_spin_lock(&sun8i_mcpm_lock);
	BUG_ON(__mcpm_cluster_state(cluster) != CLUSTER_UP);

	sun8i_cpu_use_count[cluster][cpu]--;
	if (sun8i_cpu_use_count[cluster][cpu] == 0) {
		/* check is the last-man */
		sun8i_cluster_use_count[cluster]--;
		if (sun8i_cluster_use_count[cluster]) {
			last_man = true;
		}
	} else if (sun8i_cpu_use_count[cluster][cpu] == 1) {
		/*
		 * A power_up request went ahead of us.
		 * Even if we do not want to shut this CPU down,
		 * the caller expects a certain state as if the WFI
		 * was aborted.  So let's continue with cache cleaning.
		 */
		skip_power_down = true;
	} else {
		/* all other state is error */
		BUG();
	}

	/* notify sun8i_mcpm_cpu_kill() that hardware shutdown is finished */
	if (!skip_power_down) {
		cpumask_set_cpu(cpu, &dead_cpus[cluster]);
	}

	if (last_man && __mcpm_outbound_enter_critical(cpu, cluster)) {
		arch_spin_unlock(&sun8i_mcpm_lock);

		/*
		 * Flush all cache levels for this cluster.
		 *
		 * A15/A7 can hit in the cache with SCTLR.C=0, so we don't need
		 * a preliminary flush here for those CPUs.  At least, that's
		 * the theory -- without the extra flush, Linux explodes on
		 * RTSM (maybe not needed anymore, to be investigated).
		 */
		flush_cache_all();
		set_cr(get_cr() & ~CR_C);
		flush_cache_all();

		/*
		 * This is a harmless no-op.  On platforms with a real
		 * outer cache this might either be needed or not,
		 * depending on where the outer cache sits.
		 */
		outer_flush_all();

		/* Disable local coherency by clearing the ACTLR "SMP" bit: */
		set_auxcr(get_auxcr() & ~(1 << 6));

		__mcpm_outbound_leave_critical(cluster, CLUSTER_DOWN);
	} else {
		arch_spin_unlock(&sun8i_mcpm_lock);

		/*
		 * Flush the local CPU cache.
		 *
		 * A15/A7 can hit in the cache with SCTLR.C=0, so we don't need
		 * a preliminary flush here for those CPUs.  At least, that's
		 * the theory -- without the extra flush, Linux explodes on
		 * RTSM (maybe not needed anymore, to be investigated).
		 */
		flush_cache_louis();
		set_cr(get_cr() & ~CR_C);
		flush_cache_louis();

		/* Disable local coherency by clearing the ACTLR "SMP" bit: */
		set_auxcr(get_auxcr() & ~(1 << 6));
	}

	__mcpm_cpu_down(cpu, cluster);

	/* Now we are prepared for power-down, do it: */
	if (!skip_power_down) {
		dsb();
		wfi();
	}
	/* Not dead at this point?  Let our caller cope. */
}

static void sun8i_mcpm_powered_up(void)
{
	unsigned int mpidr, cpu, cluster;

	/* this code is execute in inbound cpu context */
	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	/* check outbound cluster have valid cpu core */
	if (sun8i_cluster_use_count[!cluster] == 0) {
		/* startup cluster power-down timer */
		hrtimer_start(&cluster_power_down_timer, ktime_set(0, 1000000), HRTIMER_MODE_REL);
	}
}

static void sun8i_smp_init_cpus(void)
{
	unsigned int i, ncores;

	ncores = MAX_NR_CLUSTERS * MAX_CPUS_PER_CLUSTER;

	/* sanity check */
	if (ncores > nr_cpu_ids) {
		pr_warn("SMP: %u cores greater than maximum (%u), clipping\n",
			ncores, nr_cpu_ids);
		ncores = nr_cpu_ids;
	}
	/* setup the set of possible cpus */
	for (i = 0; i < ncores; i++) {
		set_cpu_possible(i, true);
	}
}

static int sun8i_mcpm_cpu_kill(unsigned int cpu)
{
	int i;
	int killer;
	unsigned int cluster_id;
	unsigned int cpu_id;
	unsigned int mpidr;

	mpidr      = cpu_logical_map(cpu);
	cpu_id     = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	killer = get_cpu();
	put_cpu();
	pr_info("sun8i hotplug: cpu(%d) try to kill cpu(%d)\n", killer, cpu);

	for (i = 0; i < 1000; i++) {
		if (cpumask_test_cpu(cpu_id, &dead_cpus[cluster_id]) && SUN8I_IS_WFI_MODE(cluster_id, cpu_id)) {

			/* power-down cpu core */
			sun8i_cpu_power_control(cpu_id, cluster_id, 0);
			return 1;
		}
		mdelay(1);
	}

	pr_err("sun8i hotplug: try to kill cpu:%d failed!\n", cpu);

	return 0;
}

int sun8i_mcpm_cpu_disable(unsigned int cpu)
{
	unsigned int cluster_id;
	unsigned int cpu_id;
	unsigned int mpidr;

	mpidr      = cpu_logical_map(cpu);
	cpu_id     = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster_id = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	cpumask_clear_cpu(cpu_id, &dead_cpus[cluster_id]);
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}

static const struct mcpm_platform_ops sun8i_mcpm_power_ops = {
	.power_up	= sun8i_mcpm_power_up,
	.power_down	= sun8i_mcpm_power_down,
	.powered_up     = sun8i_mcpm_powered_up,
	.smp_init_cpus  = sun8i_smp_init_cpus,
	.cpu_kill       = sun8i_mcpm_cpu_kill,
	.cpu_disable    = sun8i_mcpm_cpu_disable,
};

static void __init sun8i_mcpm_boot_cpu_init(void)
{
	unsigned int mpidr, cpu, cluster;

	mpidr = read_cpuid_mpidr();
	cpu = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	pr_info("%s: cpu %u cluster %u\n", __func__, cpu, cluster);
	BUG_ON(cpu >= MAX_CPUS_PER_CLUSTER || cluster >= MAX_NR_CLUSTERS);
	sun8i_cpu_use_count[cluster][cpu] = 1;
	sun8i_cluster_use_count[cluster]++;
}

extern void sun8i_power_up_setup(unsigned int affinity_level);
extern int __init cci_init(void);

static int __init sun8i_mcpm_init(void)
{
	int ret;

	/* initialize cci driver first */
	cci_init();

	/* initialize prcm and cpucfg model virtual base address */
	sun8i_prcm_base   = (void __iomem *)(SUNXI_R_PRCM_VBASE);
	sun8i_cpuxcfg_base = (void __iomem *)(SUNXI_CPUXCFG_VBASE);
	sun8i_cpuscfg_base = (void __iomem *)(SUNXI_R_CPUCFG_VBASE);
	sun8i_mcpm_boot_cpu_init();

	/* initialize cluster power-down timer */
	hrtimer_init(&cluster_power_down_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cluster_power_down_timer.function = sun8i_cluster_power_down;
	hrtimer_start(&cluster_power_down_timer, ktime_set(0, 10000000), HRTIMER_MODE_REL);

	ret = mcpm_platform_register(&sun8i_mcpm_power_ops);
	if (!ret) {
		ret = mcpm_sync_init(sun8i_power_up_setup);
	}

	/* set sun8i platform non-boot cpu startup entry. */
	sunxi_set_secondary_entry((void *)(virt_to_phys(mcpm_entry_point)));

	/* hard-encode by sunny to support sun8i 2big+2little,
	 * we should use device-tree to config the cluster and cpu topology information.
	 * but sun8i not support device-tree now, so I just hard-encode for temp debug.
	 */
	cpu_logical_map(0) = 0x000;
	cpu_logical_map(1) = 0x001;
	cpu_logical_map(2) = 0x100;
	cpu_logical_map(3) = 0x101;

	return 0;
}
early_initcall(sun8i_mcpm_init);
