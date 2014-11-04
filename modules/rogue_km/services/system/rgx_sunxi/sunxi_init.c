#include "pvrsrv_device.h"
#include "syscommon.h"

#include "pvrsrv_error.h"
#include "img_types.h"
#include "pvr_debug.h"
#include "ion_sys.h"

#include <linux/version.h>
#include PVR_ANDROID_ION_HEADER
#include PVR_ANDROID_ION_PRIV_HEADER
#include <linux/err.h>
#include <linux/slab.h>

#include <linux/version.h>
#include <linux/err.h>
#include <linux/hardirq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <linux/io.h>
#include <linux/clk/sunxi_name.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <mach/sys_config.h>
#include "power.h"
#include "sunxi_init.h"

#ifdef CONFIG_CPU_BUDGET_THERMAL
#include <linux/cpu_budget_cooling.h>
static int Is_powernow = 0;
#endif

#define AXI_CLK_FREQ 320

static struct clk *gpu_core_clk        = NULL;
static struct clk *gpu_mem_clk         = NULL;
static struct clk *gpu_axi_clk         = NULL;
static struct clk *gpu_pll_clk         = NULL;
static struct regulator *rgx_regulator = NULL;
static int b_throttle                  = 0;
static int extreme_flag                = 0;
static unsigned int min_vf_level_val   = 4;
static unsigned int max_vf_level_val   = 6;

static unsigned int vf_table[9][2]=
{
	{ 700,  48},
	{ 800, 120},
	{ 800, 240},
	{ 900, 320},
	{ 900, 384},
	{1000, 480},
	{1100, 528},
	{1200, 572},
	{1200, 624},
};

long int GetConfigFreq(IMG_VOID)
{
	return vf_table[min_vf_level_val][1]*1000*1000;
}

static IMG_VOID AssertGpuResetSignal(IMG_VOID)
{
	unsigned int reg;
	reg = readl(SUNXI_CCM_MOD_VBASE + NMI_IRQ_STATUS);
	reg &= ~(1<<9);
	writel(reg, SUNXI_CCM_MOD_VBASE + NMI_IRQ_STATUS);
	reg = readl(SUNXI_CCM_MOD_VBASE + NMI_INIT_CTRL_REG);
	reg &= ~(1<<3);
	writel(reg, SUNXI_CCM_MOD_VBASE + NMI_INIT_CTRL_REG);
}

static IMG_VOID DeAssertGpuResetSignal(IMG_VOID)
{
	unsigned int reg;
	reg = readl(SUNXI_CCM_MOD_VBASE + NMI_INIT_CTRL_REG);
	reg |= 1<<3;
	writel(reg, SUNXI_CCM_MOD_VBASE + NMI_INIT_CTRL_REG);
	reg = readl(SUNXI_CCM_MOD_VBASE + NMI_IRQ_STATUS);
	reg |= 1<<9;
	writel(reg, SUNXI_CCM_MOD_VBASE + NMI_IRQ_STATUS);	
}

static IMG_VOID RgxEnableClock(IMG_VOID)
{
	if(gpu_core_clk->enable_count == 0)
	{
		unsigned int reg;
		
		if(clk_prepare_enable(gpu_pll_clk))
			printk(KERN_ERR "Failed to enable pll9 clock!\n");	
		if(clk_prepare_enable(gpu_core_clk))
			printk(KERN_ERR "Failed to enable core clock!\n");
		if(clk_prepare_enable(gpu_mem_clk))
			printk(KERN_ERR "Failed to enable mem clock!\n");
		if(clk_prepare_enable(gpu_axi_clk))
			printk(KERN_ERR "Failed to enable axi clock!\n");
			
		writel(1 << 8, SUNXI_GPU_CTRL_VBASE + 0x18);
		
		/* enalbe gpu ctrl clk */
		reg = readl(SUNXI_CCM_MOD_VBASE + 0x180);
		reg |= 1<<3;
		writel(reg, SUNXI_CCM_MOD_VBASE + 0x180);
	}
}

static IMG_VOID RgxDisableClock(IMG_VOID)
{				
	if(gpu_core_clk->enable_count == 1)
	{
		unsigned int reg;
				
		/* disalbe gpu ctrl clk */
		reg = readl(SUNXI_CCM_MOD_VBASE + 0x180);
		reg &= ~(1<<3);
		writel(reg, SUNXI_CCM_MOD_VBASE + 0x180);
		
		clk_disable_unprepare(gpu_axi_clk);
		clk_disable_unprepare(gpu_mem_clk);	
		clk_disable_unprepare(gpu_core_clk);
		clk_disable_unprepare(gpu_pll_clk);
	}
}

static IMG_VOID RgxEnablePower(IMG_VOID)
{
	if(!regulator_is_enabled(rgx_regulator))
	{
		regulator_enable(rgx_regulator); 		
	}
}

static IMG_VOID RgxDisablePower(IMG_VOID)
{
	if(regulator_is_enabled(rgx_regulator))
	{
		regulator_disable(rgx_regulator); 		
	}
}

static int SetGpuVol(int vf_level)
{
	if(regulator_set_voltage(rgx_regulator, vf_table[vf_level][0]*1000, vf_table[max_vf_level_val][0]*1000) != 0)
    {
		printk(KERN_ERR "Failed to set gpu power voltage!\n");
		return -1;
    }
	udelay(20);
	return 0;
}

static int SetClkVal(const char clk_name[], int freq)
{
	struct clk *clk = NULL;
	
	if(!strcmp(clk_name, "pll"))
	{
		clk = gpu_pll_clk;
	}
	else if(!strcmp(clk_name, "core"))
	{
		clk = gpu_core_clk;
	}
	else if(!strcmp(clk_name, "mem"))
	{
		clk = gpu_mem_clk;
	}
	else
	{
		clk = gpu_axi_clk;
		goto out1;
	}
	
	if(clk_set_rate(clk, freq*1000*1000))
    {
		goto err_out;
    }
	goto out2;
	
out1:
	if(clk_set_rate(clk, freq))
    {
		goto err_out;
    }
	
out2:
	clk = NULL;
	udelay(100);
	printk(KERN_INFO "Set gpu %s clock successfully\n", clk_name);
	return 0;	

err_out:
	printk(KERN_ERR "Failed to set gpu %s clock!\n", clk_name);
	return -1;
}

static IMG_VOID RgxDvfsChange(int vf_level, int up_flag)
{
	PVRSRVDevicePreClockSpeedChange(0, IMG_TRUE, NULL);
	printk(KERN_INFO "RGX DVFS begin\n");
	if(up_flag == 1)
	{
		SetGpuVol(vf_level);
		SetClkVal("pll", vf_table[vf_level][1]);
	}
	else
	{
		SetClkVal("pll", vf_table[vf_level][1]);
		SetGpuVol(vf_level);
	}
	printk(KERN_INFO "RGX DVFS end\n");
	PVRSRVDevicePostClockSpeedChange(0, IMG_TRUE, NULL);
}

static IMG_VOID ParseFexPara(IMG_VOID)
{
    script_item_u min_vf_level, max_vf_level;
    if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("rgx_para", "min_vf_level", &min_vf_level))
    {              
        if(min_vf_level.val > 0 && min_vf_level.val < 9)
		{
			min_vf_level_val = min_vf_level.val - 1;
		}
    }
    if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("rgx_para", "max_vf_level", &max_vf_level))
    {              
        if(max_vf_level.val >= min_vf_level_val && max_vf_level.val < 9)
        {
            max_vf_level_val = max_vf_level.val - 1;
        }
        else if(max_vf_level_val < min_vf_level_val)
        {
            max_vf_level_val = min_vf_level_val;
        }
    }              
}

PVRSRV_ERROR AwPrePowerState(PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{
	if(eNewPowerState == PVRSRV_DEV_POWER_STATE_ON)
	{
		RgxEnableClock();
	}
	return PVRSRV_OK;
}

PVRSRV_ERROR AwPostPowerState(PVRSRV_DEV_POWER_STATE eNewPowerState, PVRSRV_DEV_POWER_STATE eCurrentPowerState, IMG_BOOL bForced)
{
	if(eNewPowerState == PVRSRV_DEV_POWER_STATE_OFF)
	{
		RgxDisableClock();
	}	
	return PVRSRV_OK;
}

PVRSRV_ERROR RgxResume(IMG_VOID)
{
	RgxEnablePower();
	
	mdelay(2);
	
	/* set external isolation invalid */
	writel(0, SUNXI_R_PRCM_VBASE + GPU_PWROFF_GATING);

	RgxEnableClock();
	
	/* wait until gpu pll is stable */
	while(!(readl(SUNXI_CCM_PLL_VBASE + 0x9c) & 0x100));
	
	DeAssertGpuResetSignal();
	
	return PVRSRV_OK;
}

PVRSRV_ERROR RgxSuspend(IMG_VOID)
{	
	AssertGpuResetSignal();
	
	RgxDisableClock();
	
	/* set external isolation valid */
	writel(1, SUNXI_R_PRCM_VBASE + GPU_PWROFF_GATING);
	
	RgxDisablePower();
	
	return PVRSRV_OK;
}

#ifdef CONFIG_CPU_BUDGET_THERMAL
static int rgx_throttle_notifier_call(struct notifier_block *nfb, unsigned long mode, void *cmd)
{
    int retval = NOTIFY_DONE;
	if(mode == BUDGET_GPU_THROTTLE)
    {
        b_throttle=1;
        Is_powernow = 0;
    }
    else
	{
        b_throttle=0;
        if(cmd && (*(int *)cmd) == 1 && !Is_powernow)
		{
            Is_powernow = 1;
        }
		else if(cmd && (*(int *)cmd) == 0 && Is_powernow)
		{
            Is_powernow = 0;
        }
    }
	
	if(Is_powernow != extreme_flag)
	{
		if(Is_powernow == 1)
		{
			RgxDvfsChange(max_vf_level_val, 1);
			extreme_flag = 1;
		}
		else
		{
			RgxDvfsChange(min_vf_level_val, 0);
			extreme_flag = 0;
		}
	}	
	else
	{
		printk(KERN_INFO "RGX Don't need DVFS\n");
	}
	return retval;
}
static struct notifier_block rgx_throttle_notifier = {
.notifier_call = rgx_throttle_notifier_call,
};
#endif /* CONFIG_CPU_BUDGET_THERMAL */

IMG_VOID RgxSunxiInit(IMG_VOID)
{	
	ParseFexPara();
	
	rgx_regulator = regulator_get(NULL, "axp22_dcdc2");
	if (IS_ERR(rgx_regulator)) {
	    printk(KERN_ERR "Failed to get rgx regulator \n");
        rgx_regulator = NULL;
	}
	
	gpu_core_clk  = clk_get(NULL, GPUCORE_CLK);
	gpu_mem_clk   = clk_get(NULL, GPUMEM_CLK);
	gpu_axi_clk   = clk_get(NULL, GPUAXI_CLK);
	gpu_pll_clk   = clk_get(NULL, PLL9_CLK);
	
	SetGpuVol(min_vf_level_val);
		
	SetClkVal("pll", vf_table[min_vf_level_val][1]);
	SetClkVal("core", vf_table[min_vf_level_val][1]);
	SetClkVal("mem", vf_table[min_vf_level_val][1]);
	SetClkVal("axi", AXI_CLK_FREQ);
		
	RgxResume();

#ifdef CONFIG_CPU_BUDGET_THERMAL
	register_budget_cooling_notifier(&rgx_throttle_notifier);
#endif /* CONFIG_CPU_BUDGET_THERMAL */

	printk(KERN_INFO "Sunxi init successfully\n");
}

struct ion_device *g_psIonDev;
extern struct ion_device *idev;

PVRSRV_ERROR IonInit(void *phPrivateData)
{
	g_psIonDev    = idev;
	return PVRSRV_OK;
}

struct ion_device *IonDevAcquire(IMG_VOID)
{
	return g_psIonDev;
}

IMG_VOID IonDevRelease(struct ion_device *psIonDev)
{
	/* Nothing to do, sanity check the pointer we're passed back */
	PVR_ASSERT(psIonDev == g_psIonDev);
}

IMG_UINT32 IonPhysHeapID(IMG_VOID)
{
	return 0;
}

IMG_VOID IonDeinit(IMG_VOID)
{
	g_psIonDev = NULL;
}