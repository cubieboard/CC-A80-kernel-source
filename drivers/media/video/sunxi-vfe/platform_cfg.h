/*
 * header file
 * get platform config info
 * Author: raymonxiu
 *
 */

#ifndef __PLATFORM_CFG__H__
#define __PLATFORM_CFG__H__

#if defined CONFIG_FPGA_V4_PLATFORM
#define FPGA_VER
#elif defined CONFIG_FPGA_V7_PLATFORM
#define FPGA_VER
#endif

#define VFE_SYS_CONFIG
#define SUNXI_MEM

#ifdef FPGA_VER
#define FPGA_PIN
#else
#define VFE_CLK
#define VFE_GPIO
#define VFE_PMU
#endif

#include <mach/irqs.h>
#include <mach/platform.h>
#include <linux/gpio.h>

#ifdef VFE_CLK
//#include <mach/clock.h>
#include <linux/clk.h>
#include <linux/clk/sunxi_name.h>
#endif

#ifdef VFE_GPIO
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <mach/gpio.h>
#endif

#ifdef VFE_PMU
#include <linux/regulator/consumer.h>
#endif

#ifdef VFE_SYS_CONFIG
#include <mach/sys_config.h>
#endif
#include <mach/sunxi-chip.h>

#ifdef FPGA_VER
#define DPHY_CLK (48*1000*1000)
#else
#define DPHY_CLK (150*1000*1000)
#endif

#define VFE_CORE_CLK_RATE (500*1000*1000)
#define VFE_CLK_NOT_EXIST					NULL

//CSI & ISP regs configs
#if defined CONFIG_ARCH_SUN8IW1P1

#define VFE_ISP_REGULATOR				""
#define VFE_CSI_REGULATOR				""

#define CSI0_REGS_BASE          				0x01cb0000
#define CSI1_REGS_BASE          				0x01cb3000
#define ISP_REGS_BASE           				0x01cb8000
#define GPIO_REGS_VBASE					0xf1c20800
#define CPU_DRAM_PADDR_ORG 			0x40000000
#define HW_DMA_OFFSET					0x00000000
#define MAX_VFE_INPUT   					2     //the maximum number of input source of video front end
#define VFE_CORE_CLK						CSI0_S_CLK
#define VFE_CORE_CLK_SRC				PLL10_CLK
#define VFE_MASTER_CLK0					CSI0_M_CLK
#define VFE_MASTER_CLK1					CSI1_M_CLK
#define VFE_MASTER_CLK_24M_SRC		HOSC_CLK
#define VFE_MASTER_CLK_PLL_SRC		PLL7_CLK
#define VFE_MIPI_DPHY_CLK				MIPICSI_CLK
#define VFE_MIPI_DPHY_CLK_SRC			PLL7_CLK
#define VFE_MIPI_CSI_CLK					VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_VPU_CLK						VFE_CLK_NOT_EXIST

//set vfe core clk base on sensor size
#define CORE_CLK_RATE_FOR_2M (108*1000*1000)
#define CORE_CLK_RATE_FOR_3M (216*1000*1000)
#define CORE_CLK_RATE_FOR_5M (216*1000*1000)
#define CORE_CLK_RATE_FOR_8M (432*1000*1000)
#define CORE_CLK_RATE_FOR_16M (500*1000*1000)

#elif defined CONFIG_ARCH_SUN8IW3P1

#define MIPI_CSI_NOT_EXIST
#define VFE_ISP_REGULATOR				""
#define VFE_CSI_REGULATOR				""
#define CSI0_REGS_BASE          				0x01cb0000
#define ISP_REGS_BASE           				0x01cb8000
#define GPIO_REGS_VBASE					0xf1c20800
#define CPU_DRAM_PADDR_ORG 			0x40000000
#define HW_DMA_OFFSET					0x00000000
#define MAX_VFE_INPUT   					1     //the maximum number of input source of video front end
#define VFE_CORE_CLK						CSI_S_CLK
#define VFE_CORE_CLK_SRC				PLL3_CLK
#define VFE_MASTER_CLK0					CSI_M_CLK
#define VFE_MASTER_CLK1					VFE_CLK_NOT_EXIST
#define VFE_MASTER_CLK_24M_SRC		HOSC_CLK
#define VFE_MASTER_CLK_PLL_SRC		PLL3_CLK
#define VFE_MIPI_DPHY_CLK				VFE_CLK_NOT_EXIST
#define VFE_MIPI_DPHY_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK					VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_VPU_CLK						VFE_CLK_NOT_EXIST

//set vfe core clk base on sensor size
#define CORE_CLK_RATE_FOR_2M (108*1000*1000)
#define CORE_CLK_RATE_FOR_3M (216*1000*1000)
#define CORE_CLK_RATE_FOR_5M (216*1000*1000)
#define CORE_CLK_RATE_FOR_8M (432*1000*1000)
#define CORE_CLK_RATE_FOR_16M (500*1000*1000)


#elif defined CONFIG_ARCH_SUN9IW1P1

//#define VFE_ISP_REGULATOR				"axp15_dcdc4"
//#define VFE_CSI_REGULATOR				"axp15_bldo1"

#define VFE_ISP_REGULATOR				""
#define VFE_CSI_REGULATOR				""
#define USE_SPECIFIC_CCI
#define CSI0_REGS_BASE          				0x03800000
#define CSI0_CCI_REG_BASE				0x03803000
#define CSI1_REGS_BASE          				0x03900000
#define CSI1_CCI_REG_BASE				0x03903000
#define ISP_REGS_BASE           				0x03808000
#define GPIO_REGS_VBASE					0xf6000800
#define CPU_DRAM_PADDR_ORG 			0x20000000
#define HW_DMA_OFFSET					0x20000000
#define MAX_VFE_INPUT   					2     //the maximum number of input source of video front end
#define VFE_CORE_CLK						CSI_ISP_CLK
#define VFE_CORE_CLK_SRC				VFE_CLK_NOT_EXIST
#define VFE_MASTER_CLK0					CSI0_MCLK_CLK
#define VFE_MASTER_CLK1					CSI1_MCLK_CLK
#define VFE_MASTER_CLK_24M_SRC		HOSC_CLK
#define VFE_MASTER_CLK_PLL_SRC		PLL7_CLK
#define VFE_MIPI_DPHY_CLK				VFE_CLK_NOT_EXIST
#define VFE_MIPI_DPHY_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK 					MIPI_CSI_CLK
#define VFE_MIPI_CSI_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_VPU_CLK						CPURVDDVE_CLK

//set vfe core clk base on sensor size
#define CORE_CLK_RATE_FOR_2M (108*1000*1000)
#define CORE_CLK_RATE_FOR_3M (216*1000*1000)
#define CORE_CLK_RATE_FOR_5M (216*1000*1000)
#define CORE_CLK_RATE_FOR_8M (432*1000*1000)
#define CORE_CLK_RATE_FOR_16M (500*1000*1000)

#elif defined CONFIG_ARCH_SUN8IW5P1
#define MIPI_CSI_NOT_EXIST

#define VFE_ISP_REGULATOR				""
#define VFE_CSI_REGULATOR				""

#define CSI0_REGS_BASE          				0x01cb0000
#define ISP_REGS_BASE           				0x01cb8000
#define GPIO_REGS_VBASE					0xf1c20800
#define CPU_DRAM_PADDR_ORG 			0x40000000
#define HW_DMA_OFFSET					0x00000000
#define MAX_VFE_INPUT   					1     //the maximum number of input source of video front end
#define VFE_CORE_CLK						CSI_S_CLK
#define VFE_CORE_CLK_SRC				PLL_VIDEO_CLK
#define VFE_MASTER_CLK0					CSI_M_CLK
#define VFE_MASTER_CLK1					VFE_CLK_NOT_EXIST
#define VFE_MASTER_CLK_24M_SRC		HOSC_CLK
#define VFE_MASTER_CLK_PLL_SRC		PLL_VIDEO_CLK
#define VFE_MIPI_DPHY_CLK				VFE_CLK_NOT_EXIST
#define VFE_MIPI_DPHY_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK					VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_VPU_CLK						VFE_CLK_NOT_EXIST

//set vfe core clk base on sensor size
#define CORE_CLK_RATE_FOR_2M (108*1000*1000)
#define CORE_CLK_RATE_FOR_3M (216*1000*1000)
#define CORE_CLK_RATE_FOR_5M (216*1000*1000)
#define CORE_CLK_RATE_FOR_8M (432*1000*1000)
#define CORE_CLK_RATE_FOR_16M (432*1000*1000)

#elif defined CONFIG_ARCH_SUN8IW6P1

#define VFE_ISP_REGULATOR				""
#define VFE_CSI_REGULATOR				""

#define CSI0_REGS_BASE          				0x01cb0000
#define ISP_REGS_BASE           				0x01cb8000
#define GPIO_REGS_VBASE					0xf1c20800
#define CPU_DRAM_PADDR_ORG 			0x40000000
#define HW_DMA_OFFSET					0x00000000
#define MAX_VFE_INPUT   					1     //the maximum number of input source of video front end
#define VFE_CORE_CLK						CSI_S_CLK
#define VFE_CORE_CLK_SRC				PLL_VE_CLK
#define VFE_MASTER_CLK0					CSI_M_CLK
#define VFE_MASTER_CLK1					VFE_CLK_NOT_EXIST
#define VFE_MASTER_CLK_24M_SRC		HOSC_CLK
#define VFE_MASTER_CLK_PLL_SRC		PLL_DE_CLK
#define VFE_MIPI_DPHY_CLK				VFE_CLK_NOT_EXIST
#define VFE_MIPI_DPHY_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK					MIPI_CSI_CLK
#define VFE_MIPI_CSI_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_VPU_CLK						VFE_CLK_NOT_EXIST

//set vfe core clk base on sensor size
#define CORE_CLK_RATE_FOR_2M (108*1000*1000)
#define CORE_CLK_RATE_FOR_3M (216*1000*1000)
#define CORE_CLK_RATE_FOR_5M (216*1000*1000)
#define CORE_CLK_RATE_FOR_8M (432*1000*1000)
#define CORE_CLK_RATE_FOR_16M (432*1000*1000)

#elif defined CONFIG_ARCH_SUN8IW7P1
#define MIPI_CSI_NOT_EXIST

#define VFE_ISP_REGULATOR				""
#define VFE_CSI_REGULATOR				""
#define USE_SPECIFIC_CCI
#define CSI0_CCI_REG_BASE				0x01cb3000

#define CSI0_REGS_BASE          				0x01cb0000
#define ISP_REGS_BASE           				0x01cb8000
#define GPIO_REGS_VBASE					0xf1c20800
#define CPU_DRAM_PADDR_ORG 			0x40000000
#define HW_DMA_OFFSET					0x00000000
#define MAX_VFE_INPUT   					1     //the maximum number of input source of video front end
#define VFE_CORE_CLK						CSI_S_CLK
#define VFE_CORE_CLK_SRC				PLL_VIDEO_CLK
#define VFE_MASTER_CLK0					CSI_M_CLK
#define VFE_MASTER_CLK1					VFE_CLK_NOT_EXIST
#define VFE_MASTER_CLK_24M_SRC		HOSC_CLK
#define VFE_MASTER_CLK_PLL_SRC		PLL_VIDEO_CLK
#define VFE_MIPI_DPHY_CLK				VFE_CLK_NOT_EXIST
#define VFE_MIPI_DPHY_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK					VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_VPU_CLK						VFE_CLK_NOT_EXIST

//set vfe core clk base on sensor size
#define CORE_CLK_RATE_FOR_2M (108*1000*1000)
#define CORE_CLK_RATE_FOR_3M (216*1000*1000)
#define CORE_CLK_RATE_FOR_5M (216*1000*1000)
#define CORE_CLK_RATE_FOR_8M (432*1000*1000)
#define CORE_CLK_RATE_FOR_16M (432*1000*1000)

#elif defined CONFIG_ARCH_SUN8IW8P1

#define VFE_ISP_REGULATOR				""
#define VFE_CSI_REGULATOR				""
#define USE_SPECIFIC_CCI
#define CSI0_CCI_REG_BASE				0x01cb3000

#define CSI0_REGS_BASE          				0x01cb0000
#define CSI1_REGS_BASE          				0x01cb4000

#define ISP_REGS_BASE           				0x01cb8000
#define GPIO_REGS_VBASE					0xf1c20800
#define CPU_DRAM_PADDR_ORG 			0x40000000
#define HW_DMA_OFFSET					0x00000000
#define MAX_VFE_INPUT   					2     //the maximum number of input source of video front end
#define VFE_CORE_CLK						CSI0_S_CLK
#define VFE_CORE_CLK_SRC				PLL_ISP_CLK
#define VFE_MASTER_CLK0					CSI0_M_CLK
#define VFE_MASTER_CLK1					CSI1_M_CLK
#define VFE_MASTER_CLK_24M_SRC		HOSC_CLK
#define VFE_MASTER_CLK_PLL_SRC		PLL_ISP_CLK
#define VFE_MIPI_DPHY_CLK				VFE_CLK_NOT_EXIST
#define VFE_MIPI_DPHY_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_MIPI_CSI_CLK					MIPICSI_CLK 
#define VFE_MIPI_CSI_CLK_SRC			VFE_CLK_NOT_EXIST
#define VFE_VPU_CLK						VFE_CLK_NOT_EXIST

//set vfe core clk base on sensor size
#define CORE_CLK_RATE_FOR_2M (108*1000*1000)
#define CORE_CLK_RATE_FOR_3M (216*1000*1000)
#define CORE_CLK_RATE_FOR_5M (216*1000*1000)
#define CORE_CLK_RATE_FOR_8M (432*1000*1000)
#define CORE_CLK_RATE_FOR_16M (432*1000*1000)


#endif

//CSI & ISP size configs

#define CSI0_REG_SIZE               0x1000
#define MIPI_CSI_REG_SIZE           0x1000
#define MIPI_DPHY_REG_SIZE          0x1000
#define CSI0_CCI_REG_SIZE          0x1000
#define CSI1_REG_SIZE               0x1000
#define CSI1_CCI_REG_SIZE          0x1000
#define ISP_REG_SIZE                0x1000
#define ISP_LOAD_REG_SIZE           0x1000
#define ISP_SAVED_REG_SIZE          0x1000

//ISP size configs

#if defined CONFIG_ARCH_SUN9IW1P1

//stat size configs
#define ISP_STAT_TOTAL_SIZE         0x1500

#define ISP_STAT_HIST_MEM_SIZE      0x0200
#define ISP_STAT_AE_MEM_SIZE        0x0600
#define ISP_STAT_AWB_MEM_SIZE       0x0200
#define ISP_STAT_AF_MEM_SIZE        0x0500
#define ISP_STAT_AFS_MEM_SIZE       0x0200
#define ISP_STAT_AWB_WIN_MEM_SIZE   0x0400

#define ISP_STAT_HIST_MEM_OFS       0x0
#define ISP_STAT_AE_MEM_OFS         (ISP_STAT_HIST_MEM_OFS + ISP_STAT_HIST_MEM_SIZE)
#define ISP_STAT_AWB_MEM_OFS        (ISP_STAT_AE_MEM_OFS   + ISP_STAT_AE_MEM_SIZE)
#define ISP_STAT_AF_MEM_OFS         (ISP_STAT_AWB_MEM_OFS  + ISP_STAT_AWB_MEM_SIZE)
#define ISP_STAT_AFS_MEM_OFS        (ISP_STAT_AF_MEM_OFS   + ISP_STAT_AF_MEM_SIZE)
#define ISP_STAT_AWB_WIN_MEM_OFS    (ISP_STAT_AFS_MEM_OFS   + ISP_STAT_AFS_MEM_SIZE)

//table size configs

#define ISP_LUT_LENS_GAMMA_MEM_SIZE 0x1000
#define ISP_LUT_MEM_SIZE            0x0400
#define ISP_LENS_MEM_SIZE           0x0600
#define ISP_GAMMA_MEM_SIZE          0x0200
#define ISP_DRC_MEM_SIZE            0x0200

#define ISP_LUT_MEM_OFS             0x0
#define ISP_LENS_MEM_OFS            (ISP_LUT_MEM_OFS + ISP_LUT_MEM_SIZE)
#define ISP_GAMMA_MEM_OFS           (ISP_LENS_MEM_OFS + ISP_LENS_MEM_SIZE)

#elif defined CONFIG_ARCH_SUN8IW6P1

//stat size configs

#define ISP_STAT_TOTAL_SIZE         0x2100

#define ISP_STAT_HIST_MEM_SIZE      0x0200
#define ISP_STAT_AE_MEM_SIZE        0x0600
#define ISP_STAT_AWB_MEM_SIZE       0x0200
#define ISP_STAT_AF_MEM_SIZE        0x0500
#define ISP_STAT_AFS_MEM_SIZE       0x0200
#define ISP_STAT_AWB_WIN_MEM_SIZE   0x1000

#define ISP_STAT_HIST_MEM_OFS       0x0
#define ISP_STAT_AE_MEM_OFS         (ISP_STAT_HIST_MEM_OFS + ISP_STAT_HIST_MEM_SIZE)
#define ISP_STAT_AWB_MEM_OFS        (ISP_STAT_AE_MEM_OFS   + ISP_STAT_AE_MEM_SIZE)
#define ISP_STAT_AF_MEM_OFS         (ISP_STAT_AWB_MEM_OFS  + ISP_STAT_AWB_MEM_SIZE)
#define ISP_STAT_AFS_MEM_OFS        (ISP_STAT_AF_MEM_OFS   + ISP_STAT_AF_MEM_SIZE)
#define ISP_STAT_AWB_WIN_MEM_OFS    (ISP_STAT_AFS_MEM_OFS   + ISP_STAT_AFS_MEM_SIZE)

//table size configs

#define ISP_LUT_LENS_GAMMA_MEM_SIZE 0x800
#define ISP_LUT_MEM_SIZE            0x0
#define ISP_LENS_MEM_SIZE           0x0600
#define ISP_GAMMA_MEM_SIZE          0x0200

#define ISP_DRC_MEM_SIZE            0x0200

#define ISP_LUT_MEM_OFS             0x0
#define ISP_LENS_MEM_OFS            (ISP_LUT_MEM_OFS + ISP_LUT_MEM_SIZE)
#define ISP_GAMMA_MEM_OFS           (ISP_LENS_MEM_OFS + ISP_LENS_MEM_SIZE)

#elif defined CONFIG_ARCH_SUN8IW8P1

//stat size configs

#define ISP_STAT_TOTAL_SIZE         0x2100

#define ISP_STAT_HIST_MEM_SIZE      0x0200
#define ISP_STAT_AE_MEM_SIZE        0x0600
#define ISP_STAT_AWB_MEM_SIZE       0x0200
#define ISP_STAT_AF_MEM_SIZE        0x0500
#define ISP_STAT_AFS_MEM_SIZE       0x0200
#define ISP_STAT_AWB_WIN_MEM_SIZE   0x1000

#define ISP_STAT_HIST_MEM_OFS       0x0
#define ISP_STAT_AE_MEM_OFS         (ISP_STAT_HIST_MEM_OFS + ISP_STAT_HIST_MEM_SIZE)
#define ISP_STAT_AWB_MEM_OFS        (ISP_STAT_AE_MEM_OFS   + ISP_STAT_AE_MEM_SIZE)
#define ISP_STAT_AF_MEM_OFS         (ISP_STAT_AWB_MEM_OFS  + ISP_STAT_AWB_MEM_SIZE)
#define ISP_STAT_AFS_MEM_OFS        (ISP_STAT_AF_MEM_OFS   + ISP_STAT_AF_MEM_SIZE)
#define ISP_STAT_AWB_WIN_MEM_OFS    (ISP_STAT_AFS_MEM_OFS   + ISP_STAT_AFS_MEM_SIZE)

//table size configs

#define ISP_LUT_LENS_GAMMA_MEM_SIZE 0x800
#define ISP_LUT_MEM_SIZE            0x0
#define ISP_LENS_MEM_SIZE           0x0600
#define ISP_GAMMA_MEM_SIZE          0x0200

#define ISP_DRC_MEM_SIZE            0x0200

#define ISP_LUT_MEM_OFS             0x0
#define ISP_LENS_MEM_OFS            (ISP_LUT_MEM_OFS + ISP_LUT_MEM_SIZE)
#define ISP_GAMMA_MEM_OFS           (ISP_LENS_MEM_OFS + ISP_LENS_MEM_SIZE)

#else
//stat size configs

#define ISP_STAT_TOTAL_SIZE         0x1700

#define ISP_STAT_HIST_MEM_SIZE      0x0200
#define ISP_STAT_AE_MEM_SIZE        0x0c00
#define ISP_STAT_AWB_MEM_SIZE       0x0500
#define ISP_STAT_AF_MEM_SIZE        0x0200
#define ISP_STAT_AFS_MEM_SIZE       0x0200
#define ISP_STAT_AWB_WIN_MEM_SIZE   0x0000

#define ISP_STAT_HIST_MEM_OFS       0x0
#define ISP_STAT_AE_MEM_OFS         (ISP_STAT_HIST_MEM_OFS + ISP_STAT_HIST_MEM_SIZE)
#define ISP_STAT_AWB_MEM_OFS        (ISP_STAT_AE_MEM_OFS   + ISP_STAT_AE_MEM_SIZE)
#define ISP_STAT_AF_MEM_OFS         (ISP_STAT_AWB_MEM_OFS  + ISP_STAT_AWB_MEM_SIZE)
#define ISP_STAT_AFS_MEM_OFS        (ISP_STAT_AF_MEM_OFS   + ISP_STAT_AF_MEM_SIZE)
#define ISP_STAT_AWB_WIN_MEM_OFS    (ISP_STAT_AFS_MEM_OFS   + ISP_STAT_AFS_MEM_SIZE)

//table size configs

#define ISP_LUT_LENS_GAMMA_MEM_SIZE 0x1000
#define ISP_LUT_MEM_SIZE            0x0400
#define ISP_LENS_MEM_SIZE           0x0600
#define ISP_GAMMA_MEM_SIZE          0x0200
#define ISP_DRC_MEM_SIZE            0x0200

#define ISP_LUT_MEM_OFS             0x0
#define ISP_LENS_MEM_OFS            (ISP_LUT_MEM_OFS + ISP_LUT_MEM_SIZE)
#define ISP_GAMMA_MEM_OFS           (ISP_LENS_MEM_OFS + ISP_LENS_MEM_SIZE)
#endif


#define MAX_CH_NUM      4
#define MAX_INPUT_NUM   2     //the maximum number of device connected to the same bus
#define MAX_ISP_STAT_BUF  5   //the maximum number of isp statistic buffer
#define MAX_SENSOR_DETECT_NUM  3   //the maximum number of detect sensor on the same bus.

#define MAX_AF_WIN_NUM 1
#define MAX_AE_WIN_NUM 1

#endif //__PLATFORM_CFG__H__
