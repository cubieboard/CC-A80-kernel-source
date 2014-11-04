/*
 * drivers/usb/host/sunxi_hci.c
 * (C) Copyright 2010-2015
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * yangnaitian, 2011-5-24, create this file
 * javen, 2011-7-18, add clock and power switch
 *
 * sunxi HCI Driver
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/clk/sunxi_name.h>
#include <linux/gpio.h>

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/dma-mapping.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/unaligned.h>
#include <mach/irqs.h>
#include <mach/platform.h>
#include <mach/sys_config.h>
#include <linux/regulator/consumer.h>

#include  "sunxi_hci.h"

#ifndef  SUNXI_USB_FPGA
static char* usbc_name[4] 			= {"usbc0", "usbc1", "usbc2", "usbc3"};
#endif

#ifdef  CONFIG_ARCH_SUN8IW5
#define  USBPHYC_REG_o_PHYCTL		    0x0410
#else
#define  USBPHYC_REG_o_PHYCTL		    0x0404
#endif

#ifdef CONFIG_ARCH_SUN9IW1
#define SUNXI_CCM_VBASE  SUNXI_CCM_MOD_VBASE
#define SUNXI_SRAMCTRL_VBASE SUNXI_SRAM_C_VBASE
#define HCI_PHY_CONTROL_REGISTER 0x04
#define HCI_SIE_CONTROL_REGISTER 0x00

#define VBAT_USBH_VALUE  3000000
#define VBAT_HSIC_VALUE  1200000
static u32 usb_hci_clock_cnt = 0;
static struct clk *hci_ahb_gate = NULL;
#ifdef CONFIG_USB_SUNXI_HSIC
static struct regulator* vbat_hsic_hdle = NULL;
static char *vbat_hsic_name = "axp15_bldo4";
#else
static struct regulator* vbat_usbh_hdle = NULL;
static char *vbat_usbh_name = "axp22_aldo1";
#endif
static u32 usbc_base[4] 			= {0, (u32 __force)SUNXI_USB_HCI0_VBASE, (u32 __force)SUNXI_USB_HCI1_VBASE, (u32 __force)SUNXI_USB_HCI2_VBASE};
static u32 ehci_irq_no[4] 			= {0, SUNXI_IRQ_USB_EHCI0, SUNXI_IRQ_USB_EHCI1, SUNXI_IRQ_USB_EHCI2};
static u32 ohci_irq_no[4] 			= {0, SUNXI_IRQ_USB_OHCI0, SUNXI_IRQ_USB_OHCI1, SUNXI_IRQ_USB_OHCI2};
#ifndef  SUNXI_USB_FPGA
static int open_hci_clock(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{

	int reg_value = 0;
	int i = 0;

	DMSG_INFO("[%s]: open hci clock,usbc_no:%d, is_ohci:%d\n", sunxi_hci->hci_name, sunxi_hci->usbc_no, ohci);

	switch (sunxi_hci->usbc_no){
	case 1:
		/*PHY*/
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value |= (0x01<<1);// enable SCLK_GATING_HCI0_PHY
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value |= (0x01<<17);//enable HCI0_PHY_RST
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		/*SIE*/
		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		reg_value |= (0x1<<1); //enable USB HCI0 AHB Gating
		reg_value |= (0x1<<17); //enable USB HCI0 Module Reset

		if(ohci){
			reg_value |= (0x1<<2);//enable USB OHCI0 Special Clock(12M and 48M) Gating
		}

		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);

		break;

	case 2:
		//force high to ehci control
		reg_value = USBC_Readl(sunxi_hci->usb_vbase+ SUNXI_USB_PMU_IRQ_ENABLE);
		reg_value |= (1 << 1);
		reg_value |= (1 << 20);
		reg_value |= (1 << 17);
		USBC_Writel(reg_value, (sunxi_hci->usb_vbase+ SUNXI_USB_PMU_IRQ_ENABLE));

		/*PHY*/
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value |= (0x01<<2); //enable 480M_GATING_HCI1_HSIC
		reg_value &= ~(0x01<<3); //disablde SCLK_GATING_HCI1_ULPIMP/ULPISP
		reg_value |= (0x01<<10); //enble 12M_GATING_HCI1_HSIC
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value |= (0x01<<18);//enbale HCI1_HSIC_RST
		reg_value &= ~(0x01<<19);//disenable HCI1_ULPIMP_RST/HCI1_ULPISP_RST
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		/*SIE*/
		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		reg_value |= (0x1<<3); //enable USB HCI1 AHB Gating
		reg_value |= (0x1<<18); //enable USB HCI1 Module Reset

		if(ohci){
			reg_value &= ~(0x1<<4); //disenable USB OHCI1 Special Clock(12M and 48M) Gating
		}

		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		break;

	case 3:
		/*PHY*/
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value |= (0x01<<5); //enable SCLK_GATING_HCI2_UTMIPHY
		reg_value &= ~(0x01<<4); //disablde 480M_GATING_HCI2_HSIC
		reg_value &= ~(0x01<<10); //disablde 12M_GATING_HCI2_HSIC
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value |= (0x01<<21);//enbale HCI2_UTMIPHY_RST
		reg_value &= ~(0x01<<20);//disenable HCI2_HSIC_RST
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		/*SIE*/
		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		reg_value |= (0x1<<5); //enable USB HCI2 AHB Gating
		reg_value |= (0x1<<19); //enable USB HCI2 Module Reset

		if(ohci){
			reg_value |= (0x1<<6); //enable USB OHCI1 Special Clock(12M and 48M) Gating
		}

		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);

		break;

	default:
		return 0;

	}
	return 0;
}

static int close_hci_clock(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
	int reg_value = 0;
	int i = 0;

	DMSG_INFO("[%s]: close hci clock,usbc_no:%d, is_ohci:%d\n", sunxi_hci->hci_name, sunxi_hci->usbc_no, ohci);

	switch (sunxi_hci->usbc_no){
	case 1:
		/*PHY*/
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value &= ~(0x01<<1);// disablde SCLK_GATING_HCI0_PHY
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value &= ~(0x01<<17);//disablde HCI0_PHY_RST
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		/*SIE*/
		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		reg_value &= ~(0x1<<1); //disablde USB HCI0 AHB Gating
		reg_value &= ~(0x1<<17); //disablde USB HCI0 Module Reset

		if(ohci){
			reg_value &= ~(0x1<<2);//enable USB OHCI0 Special Clock(12M and 48M) Gating
		}

		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);

		break;

	case 2:
		/*PHY*/
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value &= ~(0x01<<2); //disablde 480M_GATING_HCI1_HSIC
		reg_value &= ~(0x01<<3); //disablde SCLK_GATING_HCI1_ULPIMP/ULPISP
		reg_value &= ~(0x01<<10); //disablde 12M_GATING_HCI1_HSIC
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value &= ~(0x01<<18);//disablde HCI1_HSIC_RST
		reg_value &= ~(0x01<<19);//disenable HCI1_ULPIMP_RST/HCI1_ULPISP_RST
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		/*SIE*/
		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		reg_value &= ~(0x1<<3); //disablde USB HCI1 AHB Gating
		reg_value &= ~(0x1<<18); //disablde USB HCI1 Module Reset

		if(ohci){
			reg_value &= ~(0x1<<4); //disenable USB OHCI1 Special Clock(12M and 48M) Gating
		}

		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		break;

	case 3:
		/*PHY*/
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value &= ~(0x01<<5); //disablde SCLK_GATING_HCI2_UTMIPHY
		reg_value &= ~(0x01<<4); //disablde 480M_GATING_HCI2_HSIC
		reg_value &= ~(0x01<<10); //disablde 12M_GATING_HCI2_HSIC
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);
		reg_value &= ~(0x01<<21);//disablde HCI2_UTMIPHY_RST
		reg_value &= ~(0x01<<20);//disenable HCI2_HSIC_RST
		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_PHY_CONTROL_REGISTER);

		/*SIE*/
		for(i = 0; i < 0x10; i++);
		reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);
		reg_value &= ~(0x1<<5); //disablde USB HCI2 AHB Gating
		reg_value &= ~(0x1<<19); //disablde USB HCI2 Module Reset

		if(ohci){
			reg_value &= ~(0x1<<6); //disablde USB OHCI1 Special Clock(12M and 48M) Gating
		}

		USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + HCI_SIE_CONTROL_REGISTER);

		break;

	default:
		return 0;
	}

	return 0;
}
#endif

#else
#ifndef SUNXI_USB_FPGA /* kill warning */
static char* usbc_ahb_ehci_name[3]  = {"", USBEHCI_CLK, USBEHCI_CLK};
static char* usbc_ahb_ohci_name[4]  = {"", USBOHCI_CLK, USBOHCI_CLK, USBOHCI_CLK};
	#ifdef CONFIG_ARCH_SUN8IW8
		static char* usbc_phy_name[3] 	= {USBPHY0_CLK, USBPHY0_CLK, USBPHY0_CLK};
	#else
		static char* usbc_phy_name[3]	= {USBPHY0_CLK, USBPHY1_CLK, USBPHY1_CLK};
	#endif

#endif
static u32 usbc_base[4] 			= {(u32 __force)SUNXI_USB_OTG_VBASE, (u32 __force)SUNXI_USB_HCI0_VBASE, (u32 __force)SUNXI_USB_HCI0_VBASE, (u32 __force)SUNXI_USB_HCI0_VBASE};
static u32 ehci_irq_no[3] 			= {0, SUNXI_IRQ_USBEHCI0, SUNXI_IRQ_USBEHCI0};
static u32 ohci_irq_no[4] 			= {0, SUNXI_IRQ_USBOHCI0, SUNXI_IRQ_USBOHCI0, SUNXI_IRQ_USBOHCI0};
#endif

static u32 usb1_set_vbus_cnt = 0;
static u32 usb2_set_vbus_cnt = 0;
static u32 usb3_set_vbus_cnt = 0;

static u32 usb1_enable_passly_cnt = 0;
static u32 usb2_enable_passly_cnt = 0;
static u32 usb3_enable_passly_cnt = 0;

#ifndef  SUNXI_USB_FPGA
static void sunxi_usb_3g_config(struct sunxi_hci_hcd *sunxi_hci)
{
	script_item_value_type_e type = 0;
	script_item_u item_temp;
	u32 usb_3g_used	  = 0;
	u32 usb_3g_usbc_num  = 0;
	u32 usb_3g_usbc_type = 0;

	/* 3g_used */
	type = script_get_item("3g_para", "3g_used", &item_temp);
	if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
		usb_3g_used = item_temp.val;
	}else{
		//DMSG_PANIC("WRN: script_parser_fetch usb_3g_used failed\n");
		return;
	}

	/* 3g_usbc_num */
	type = script_get_item("3g_para", "3g_usbc_num", &item_temp);
	if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
		usb_3g_usbc_num = item_temp.val;
	}else{
		//DMSG_PANIC("WRN: script_parser_fetch usb_3g_usbc_num failed\n");
		return;
	}

	/* 3g_usbc_type */
	type = script_get_item("3g_para", "3g_usbc_type", &item_temp);
	if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
		usb_3g_usbc_type = item_temp.val;
	}else{
		//DMSG_PANIC("WRN: script_parser_fetch usb_3g_usbc_type failed\n");
		return;
	}

	/* only open the controller witch used by 3G */
	if(sunxi_hci->usbc_no == usb_3g_usbc_num){
		sunxi_hci->used = 0;
		if(sunxi_hci->usbc_type == usb_3g_usbc_type){
			sunxi_hci->used = 1;
		}
	}
	return;
}
#endif

static s32 request_usb_regulator_io(struct sunxi_hci_hcd *sunxi_hci)
{
	if(sunxi_hci->regulator_io != NULL){
		sunxi_hci->regulator_io_hdle= regulator_get(NULL, sunxi_hci->regulator_io);
		if(IS_ERR(sunxi_hci->regulator_io_hdle)) {
			DMSG_PANIC("ERR: some error happen, %s,regulator_io_hdle fail to get regulator!", sunxi_hci->hci_name);
			return 0;
		}

		if(regulator_set_voltage(sunxi_hci->regulator_io_hdle , sunxi_hci->regulator_value, sunxi_hci->regulator_value) < 0 ){
			DMSG_PANIC("ERR: regulator_set_voltage: %s fail\n",sunxi_hci->hci_name);
			regulator_put(sunxi_hci->regulator_io_hdle);
			return 0;
		}
	}

	return 0;
}

static s32 release_usb_regulator_io(struct sunxi_hci_hcd *sunxi_hci)
{
	if(sunxi_hci->regulator_io != NULL){
		regulator_put(sunxi_hci->regulator_io_hdle);
	}
	return 0;
}

static s32 get_usb_cfg(struct sunxi_hci_hcd *sunxi_hci)
{
#ifndef  SUNXI_USB_FPGA
	script_item_value_type_e type = 0;
	script_item_u item_temp;

	/* usbc enable */
	type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_used", &item_temp);
	if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
		sunxi_hci->used = item_temp.val;
	}else{
		DMSG_INFO("get %s usbc enable failed\n" ,sunxi_hci->hci_name);
		sunxi_hci->used = 0;
	}

	/* usbc restrict_gpio */
	type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_restrict_gpio", &sunxi_hci->restrict_gpio_set);
	if(type == SCIRPT_ITEM_VALUE_TYPE_PIO){
		sunxi_hci->usb_restrict_valid = 1;
	}else{
		DMSG_INFO("%s(restrict_gpio) is invalid\n", sunxi_hci->hci_name);
		sunxi_hci->usb_restrict_valid = 0;
	}

	/* usbc drv_vbus */
	type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_drv_vbus_gpio", &sunxi_hci->drv_vbus_gpio_set);
	if(type == SCIRPT_ITEM_VALUE_TYPE_PIO){
		sunxi_hci->drv_vbus_gpio_valid = 1;
	}else{
		DMSG_INFO("%s(drv vbus) is invalid\n", sunxi_hci->hci_name);
		sunxi_hci->drv_vbus_gpio_valid = 0;
	}

	/* host_init_state */
	type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_host_init_state", &item_temp);
	if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
		sunxi_hci->host_init_state = item_temp.val;
	}else{
		DMSG_INFO("script_parser_fetch host_init_state failed\n");
		sunxi_hci->host_init_state = 1;
	}


	/* get usb_restrict_flag */
	type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_restric_flag", &item_temp);
	if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
		sunxi_hci->usb_restrict_flag = item_temp.val;
	}else{
		DMSG_INFO("get usb_restrict_flag failed\n");
		sunxi_hci->usb_restrict_flag = 0;
	}

	type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_not_suspend", &item_temp);
	if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
		sunxi_hci->not_suspend = item_temp.val;
	}else{
		DMSG_INFO("get usb_restrict_flag failed\n");
		sunxi_hci->not_suspend = 0;
	}

	/* get regulator io information */
	type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_regulator_io", &item_temp);
	if (type == SCIRPT_ITEM_VALUE_TYPE_STR) {
		if (!strcmp(item_temp.str, "nocare")) {
			DMSG_INFO("get usb_regulator is nocare\n");
			sunxi_hci->regulator_io = NULL;
		}else{
			sunxi_hci->regulator_io = item_temp.str;

			type = script_get_item(usbc_name[sunxi_hci->usbc_no], "usb_regulator_vol", &item_temp);
			if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
				sunxi_hci->regulator_value = item_temp.val;
			}else{
				DMSG_INFO("get usb_voltage is failed\n");
				sunxi_hci->regulator_value = 0;
			}
		}
	}else {
		DMSG_INFO("get usb_regulator is failed\n");
		sunxi_hci->regulator_io = NULL;
	}

	sunxi_usb_3g_config(sunxi_hci);

	/* wifi_used */
	if(sunxi_hci->host_init_state == 0){
		u32 usb_wifi_used = 0;
		u32 usb_wifi_usbc_num  = 0;
		u32 usb_wifi_usbc_type = 0;

		type = script_get_item("wifi_para", "wifi_used", &item_temp);
		if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
			usb_wifi_used = item_temp.val;
		}else{
			DMSG_INFO("script_parser_fetch wifi_used failed\n");
			usb_wifi_used = 0;
		}

		if(usb_wifi_used){
			/* wifi_usbc_num */
			type = script_get_item("wifi_para", "wifi_usbc_id", &item_temp);
			if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
				usb_wifi_usbc_num = item_temp.val;
			}else{
				DMSG_INFO("script_parser_fetch wifi_usbc_id failed\n");
				usb_wifi_usbc_num = 0;
			}

			/* wifi_usbc_type */
			type = script_get_item("wifi_para", "wifi_usbc_type", &item_temp);
			if(type == SCIRPT_ITEM_VALUE_TYPE_INT){
				usb_wifi_usbc_type = item_temp.val;
			}else{
				DMSG_INFO("script_parser_fetch wifi_usbc_type failed\n");
				usb_wifi_usbc_type = 0;
			}

			/* only open the module used by wifi */
			if(sunxi_hci->usbc_no == usb_wifi_usbc_num){
				sunxi_hci->used = 0;
				if(sunxi_hci->usbc_type == usb_wifi_usbc_type){
					sunxi_hci->used = 1;
				}
			}
		}
	}
#else
	sunxi_hci->used = 1;
	#ifdef CONFIG_ARCH_SUN8IW8
		sunxi_hci->host_init_state = 0;
	#else
		sunxi_hci->host_init_state = 1;
	#endif
#endif

	return 0;
}

#ifndef CONFIG_ARCH_SUN9IW1
static __u32 USBC_Phy_GetCsr(__u32 usbc_no)
{
	__u32 val = 0x0;

	switch(usbc_no){
	case 0:
		val = (u32 __force)SUNXI_USB_OTG_VBASE + USBPHYC_REG_o_PHYCTL;
		break;
	case 1:
		val = (u32 __force)SUNXI_USB_OTG_VBASE + USBPHYC_REG_o_PHYCTL;
		break;
	case 2:
		val = (u32 __force)SUNXI_USB_OTG_VBASE + USBPHYC_REG_o_PHYCTL;
		break;
	default:
		break;
	}

	return val;
}
static __u32 USBC_Phy_TpWrite(__u32 usbc_no, __u32 addr, __u32 data, __u32 len)
{
	__u32 temp = 0, dtmp = 0;
	__u32 j=0;

	dtmp = data;
	for(j = 0; j < len; j++)
	{
		/* set  the bit address to be write */
		temp = USBC_Readl(USBC_Phy_GetCsr(usbc_no));
		temp &= ~(0xff << 8);
		temp |= ((addr + j) << 8);
		USBC_Writel(temp, USBC_Phy_GetCsr(usbc_no));

		temp = USBC_Readb(USBC_Phy_GetCsr(usbc_no));
		temp &= ~(0x1 << 7);
		temp |= (dtmp & 0x1) << 7;
		temp &= ~(0x1 << (usbc_no << 1));
		USBC_Writeb(temp, USBC_Phy_GetCsr(usbc_no));

		temp = USBC_Readb(USBC_Phy_GetCsr(usbc_no));
		temp |= (0x1 << (usbc_no << 1));
		USBC_Writeb( temp, USBC_Phy_GetCsr(usbc_no));

		temp = USBC_Readb(USBC_Phy_GetCsr(usbc_no));
		temp &= ~(0x1 << (usbc_no <<1 ));
		USBC_Writeb(temp, USBC_Phy_GetCsr(usbc_no));
		dtmp >>= 1;
	}

	return data;
}

static __u32 USBC_Phy_Write(__u32 usbc_no, __u32 addr, __u32 data, __u32 len)
{
	return USBC_Phy_TpWrite(usbc_no, addr, data, len);
}

static void UsbPhyInit(__u32 usbc_no)
{
//	DMSG_INFO("csr1: usbc%d: 0x%x\n", usbc_no, (u32)USBC_Readl(USBC_Phy_GetCsr(usbc_no)));

	/* adjust the 45 ohm resistance */
	if(usbc_no == 0){
		USBC_Phy_Write(usbc_no, 0x0c, 0x01, 1);
	}

//	DMSG_INFO("csr2-0: usbc%d: 0x%x\n", usbc_no, (u32)USBC_Phy_Read(usbc_no, 0x0c, 1));

	/* adjust USB0 PHY's rate and range */
	USBC_Phy_Write(usbc_no, 0x20, 0x14, 5);

//	DMSG_INFO("csr2-1: usbc%d: 0x%x\n", usbc_no, (u32)USBC_Phy_Read(usbc_no, 0x20, 5));

	/* adjust disconnect threshold value */
	USBC_Phy_Write(usbc_no, 0x2a, 3, 2); /*by wangjx*/

//	DMSG_INFO("csr2: usbc%d: 0x%x\n", usbc_no, (u32)USBC_Phy_Read(usbc_no, 0x2a, 2));
//	DMSG_INFO("csr3: usbc%d: 0x%x\n", usbc_no, (u32)USBC_Readl(USBC_Phy_GetCsr(usbc_no)));

	return;
}
#endif

#ifndef  SUNXI_USB_FPGA
static s32 clock_init(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
#ifdef CONFIG_ARCH_SUN9IW1
	return 0;
#else
	if(ohci){  /* ohci */
		sunxi_hci->ahb = clk_get(NULL, usbc_ahb_ohci_name[sunxi_hci->usbc_no]);
		if (IS_ERR(sunxi_hci->ahb)){
			DMSG_PANIC("ERR: get ohci%d abh clk failed.\n", (sunxi_hci->usbc_no - 1));
			goto failed;
		}
	}else{  /* ehci */
		sunxi_hci->ahb = clk_get(NULL, usbc_ahb_ehci_name[sunxi_hci->usbc_no]);
		if (IS_ERR(sunxi_hci->ahb)){
			DMSG_PANIC("ERR: get ehci%d abh clk failed.\n", (sunxi_hci->usbc_no - 1));
			goto failed;
		}
	}
#ifdef CONFIG_ARCH_SUN8IW8
	sunxi_hci->mod_usbphy = clk_get(NULL, usbc_phy_name[0]);
	if (IS_ERR(sunxi_hci->mod_usbphy)){
		DMSG_PANIC("ERR: get usb%d mod_usbphy failed.\n", sunxi_hci->usbc_no);
		goto failed;
	}
#else
	sunxi_hci->mod_usbphy = clk_get(NULL, usbc_phy_name[sunxi_hci->usbc_no]);
	if (IS_ERR(sunxi_hci->mod_usbphy)){
		DMSG_PANIC("ERR: get usb%d mod_usbphy failed.\n", sunxi_hci->usbc_no);
		goto failed;
	}
#endif

	return 0;

failed:
	if(IS_ERR(sunxi_hci->ahb)){
		//clk_put(sunxi_hci->ahb);
		sunxi_hci->ahb = NULL;
	}

	if(IS_ERR(sunxi_hci->mod_usbphy)){
		//clk_put(sunxi_hci->mod_usbphy);
		sunxi_hci->mod_usbphy = NULL;
	}

	return -1;
#endif
}

static s32 clock_exit(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
#ifdef CONFIG_ARCH_SUN9IW1
	return 0;
#else
	if(!IS_ERR(sunxi_hci->ahb)){
		clk_put(sunxi_hci->ahb);
		sunxi_hci->ahb = NULL;
	}

	if(!IS_ERR(sunxi_hci->mod_usbphy)){
		clk_put(sunxi_hci->mod_usbphy);
		sunxi_hci->mod_usbphy = NULL;
	}
	return 0;
#endif

}

static int open_clock(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
	DMSG_INFO("[%s]: open clock, is_open: %d\n", sunxi_hci->hci_name, sunxi_hci->clk_is_open);
#ifdef CONFIG_ARCH_SUN9IW1

	#ifdef CONFIG_USB_SUNXI_HSIC
		if(regulator_enable(vbat_hsic_hdle) < 0){
			DMSG_INFO("ERR: vbat_hsic: regulator_enable fail\n");
			return 0;
		}
	#else
		if(regulator_enable(vbat_usbh_hdle) < 0){
		       DMSG_INFO("ERR: vbat_usbh: regulator_enable fail\n");
		       return 0;
		}
	#endif

	if(usb_hci_clock_cnt == 0 && (hci_ahb_gate != NULL)){
		DMSG_INFO("clk_prepare_enable: hci_ahb_gate\n");
		if(clk_prepare_enable(hci_ahb_gate)){
			DMSG_PANIC("ERR:try to prepare_enable %s_ahb failed!\n", sunxi_hci->hci_name);
		}
	}
	usb_hci_clock_cnt ++;

	if(!sunxi_hci->clk_is_open){
		sunxi_hci->clk_is_open = 1;
		open_hci_clock(sunxi_hci,ohci);
	}

#else

	#ifdef CONFIG_ARCH_SUN8IW8
	{
		int reg_value = 0;
		reg_value = USBC_Readl(usbc_base[0] + 0x420);
		reg_value &= ~(0x01);
		USBC_Writel(reg_value, (usbc_base[0] + 0x420));

		reg_value = USBC_Readl(sunxi_hci->usb_vbase + 0x810);
		reg_value &= ~(0x01<<1);
		USBC_Writel(reg_value, (sunxi_hci->usb_vbase + 0x810));

		UsbPhyInit(0);
	}
	#else
	{
		UsbPhyInit(sunxi_hci->usbc_no);
	}
	#endif


	if(sunxi_hci->ahb && sunxi_hci->mod_usbphy && !sunxi_hci->clk_is_open){
		sunxi_hci->clk_is_open = 1;
		if(clk_prepare_enable(sunxi_hci->ahb)){
			DMSG_PANIC("ERR:try to prepare_enable %s_ahb failed!\n", sunxi_hci->hci_name);
		}
		mdelay(10);

		#ifdef  CONFIG_USB_SUNXI_HSIC
		{
			u32 reg_value = 0;
			u32 i = 0;
			u32 ccmu_base = (u32)SUNXI_CCM_VBASE;
			reg_value = USBC_Readl(ccmu_base + 0xcc);
			reg_value |= (0x01<<10);
			reg_value |= (0x01<<11);
			USBC_Writel(reg_value, (ccmu_base + 0xcc));

			for(i=0; i < 0x100; i++);

			reg_value |= (0x01 << 2);
			USBC_Writel(reg_value, (ccmu_base + 0xcc));
		}
		#else
			if(clk_prepare_enable(sunxi_hci->mod_usbphy)){
				DMSG_PANIC("ERR:try to prepare_enable %s_usbphy failed!\n", sunxi_hci->hci_name);
			}
			mdelay(10);
		#endif

	}else{
		DMSG_PANIC("[%s]: wrn: open clock failed, (0x%p, 0x%p, %d, 0x%p)\n",
			sunxi_hci->hci_name,
			sunxi_hci->ahb, sunxi_hci->mod_usbphy, sunxi_hci->clk_is_open,
			sunxi_hci->mod_usb);
	}
#endif
	return 0;
}

static int close_clock(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
	DMSG_INFO("[%s]: close clock, is_open: %d\n", sunxi_hci->hci_name, sunxi_hci->clk_is_open);
#ifdef CONFIG_ARCH_SUN9IW1
	usb_hci_clock_cnt --;
	if(usb_hci_clock_cnt == 0 && (hci_ahb_gate != NULL)){
		DMSG_INFO("clk_disable_unprepare: hci_ahb_gate\n");
		clk_disable_unprepare(hci_ahb_gate);
	}

	if(sunxi_hci->clk_is_open){
		sunxi_hci->clk_is_open = 0;
		close_hci_clock(sunxi_hci,ohci);
	}

	#ifdef CONFIG_USB_SUNXI_HSIC
		if(regulator_disable(vbat_hsic_hdle) < 0){
			DMSG_INFO("ERR: vbat_hsic: regulator_disable fail\n");
			return 0;
		}
	#else
		if(regulator_disable(vbat_usbh_hdle) < 0){
		       DMSG_INFO("ERR: vbat_usbh: regulator_disable fail\n");
		       return 0;
		}
	#endif

	return 0;
#else
	if(sunxi_hci->ahb && sunxi_hci->mod_usbphy && sunxi_hci->clk_is_open){
		sunxi_hci->clk_is_open = 0;

	#ifdef  CONFIG_USB_SUNXI_HSIC
		{
			u32 reg_value = 0;
			u32 i = 0;
			u32 ccmu_base = (u32)SUNXI_CCM_VBASE;
			reg_value = USBC_Readl(ccmu_base + 0xcc);
			reg_value &= ~ (0x01<<10);
			reg_value &= ~ (0x01<<11);
			USBC_Writel(reg_value, (ccmu_base + 0xcc));

			for(i=0; i < 0x100; i++);

			reg_value &= ~ (0x01 << 2);
			USBC_Writel(reg_value, (ccmu_base + 0xcc));
		}
	#else
			clk_disable_unprepare(sunxi_hci->mod_usbphy);
	#endif
		clk_disable_unprepare(sunxi_hci->ahb);
		mdelay(10);
	}else{
		DMSG_PANIC("[%s]: wrn: open clock failed, (0x%p, 0x%p, %d, 0x%p)\n",
				sunxi_hci->hci_name,sunxi_hci->ahb,
				sunxi_hci->mod_usbphy, sunxi_hci->clk_is_open,
				sunxi_hci->mod_usb);
	}
	return 0;
#endif
}
#else
static s32 clock_init(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
	return 0;
}

static s32 clock_exit(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
	return 0;
}

static int open_clock(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
#ifndef CONFIG_ARCH_SUN9IW1
	u32 reg_value = 0;
	u32 ccmu_base = (u32 __force)SUNXI_CCM_VBASE;

	DMSG_INFO("[%s]: open clock\n", sunxi_hci->hci_name);
#ifdef CONFIG_ARCH_SUN8IW8
	reg_value = USBC_Readl(usbc_base[0] + 0x420);
	reg_value &= ~(0x01);
	USBC_Writel(reg_value, (usbc_base[0] + 0x420));

	reg_value = USBC_Readl(sunxi_hci->usb_vbase + 0x810);
	reg_value &= ~(0x01<<1);
	USBC_Writel(reg_value, (sunxi_hci->usb_vbase + 0x810));

	UsbPhyInit(0);
#endif
	//Gating AHB clock for USB_phy1
	reg_value = USBC_Readl(ccmu_base + 0x60);
	reg_value |= (1 << 26);			        /* AHB clock gate ehci */
	reg_value |= (1 << 29); 			/* AHB clock gate ohci */
	reg_value |= (1 << 24);	           		/* AHB clock gate usb0 */
	USBC_Writel(reg_value, (ccmu_base + 0x60));

	reg_value = USBC_Readl(ccmu_base + 0x2c0);
	reg_value |= (1 << 26);			        /* ehci reset control, de-assert */
	reg_value |= (1 << 29); 			/* ohci reset control, de-assert */
	reg_value |= (1 << 24);	                        /* AHB clock gate usb0 */
	USBC_Writel(reg_value, (ccmu_base + 0x2c0));

	reg_value = USBC_Readl(ccmu_base + 0x100);
	reg_value |= (1 << 18);
	reg_value |= (1 << 17);
	USBC_Writel(reg_value, (ccmu_base + 0x100));

	//delay to wati SIE stable
	reg_value = 10000;
	while(reg_value--);

	//Enable module clock for USB phy1
	reg_value = USBC_Readl(ccmu_base + 0xcc);
	reg_value |= (1 << 16); 			/* gating specal clock for ohci */
	reg_value |= (1 << 9);				/* gating specal clock for usb phy1(ehci0,ohci0) */
	reg_value |= (1 << 8);				/* gating specal clock for usb phy0(otg) */
	reg_value |= (1 << 1);				/* usb phy1 reset */
	reg_value |= (1 << 0);				/* usb phy0 reset */

	USBC_Writel(reg_value, (ccmu_base + 0xcc));

	//delay some time
	reg_value = 10000;
	while(reg_value--);

	UsbPhyInit(sunxi_hci->usbc_no);

	printk("open_clock 0x60(0x%x), 0xcc(0x%x),0x2c0(0x%x)\n",
		(u32)USBC_Readl(ccmu_base + 0x60),
		(u32)USBC_Readl(ccmu_base + 0xcc),
		(u32)USBC_Readl(ccmu_base + 0x2c0));
	return 0;
#else
	u32 reg_value = 0;
	u32 i = 0;
	u32 ccmu_base = (u32)SUNXI_CCM_VBASE;

	DMSG_INFO("[%s]: open clock\n", sunxi_hci->hci_name);
	usb_hci_clock_cnt ++ ;

	//AHB ALL clock gate
	reg_value = USBC_Readl(ccmu_base + 0x184);
	reg_value |= (1 << 1); 			/* AHB clock gate usb0 */
	USBC_Writel(reg_value, (ccmu_base + 0x184));

	reg_value = USBC_Readl(ccmu_base + 0x1A4);
	reg_value |= (1 << 0); 			/* AHB clock gate usb0 */
	USBC_Writel(reg_value, (ccmu_base + 0x1A4));

	reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + 0x04);
	reg_value &= ~(0x1f<<1);
	reg_value &= ~(0x01<<10);
	reg_value &= ~(0x01<<12);
	reg_value &= ~(0x1f<<17);
	reg_value |= (0x01<<3);
	USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + 0x04);
	for(i=0; i<0x10; i++);
	reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + 0x04);
	reg_value |= (0x01<<19);
	USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + 0x04);

	reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE);
	reg_value &= ~(0xf<<1);
	reg_value &= ~(0xf<<17);
	reg_value |= (0x1<<3);
	reg_value |= (0x1<<4);
	USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE);
	for(i=0; i<0x10; i++);
	reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE);
	reg_value |= (0x01<<18);
	USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE);

	printk("open_clock 0x184(0x%x), 0x1A4(0x%x),0x%x(0x%x),0x%x(0x%x)\n",
		(u32)USBC_Readl(ccmu_base + 0x184),
		(u32)USBC_Readl(ccmu_base + 0x1A4),
		(u32)(SUNXI_USB_CTRL_VBASE + 0x04),(u32)USBC_Readl(SUNXI_USB_CTRL_VBASE + 0x04),
		(u32)SUNXI_USB_CTRL_VBASE, (u32)USBC_Readl(SUNXI_USB_CTRL_VBASE));

	return 0;

#endif
}

static int close_clock(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
#ifndef CONFIG_ARCH_SUN9IW1
	u32 reg_value = 0;
	u32 ccmu_base = (u32 __force)SUNXI_CCM_VBASE;

	DMSG_INFO("[%s]: close clock\n", sunxi_hci->hci_name);

	//Gating AHB clock for USB_phy1
	reg_value = USBC_Readl(ccmu_base + 0x60);
	reg_value &= ~(1 << 29);			/* close AHB for ohci */
	reg_value &= ~(1 << 26);			/* close AHB for ehci */
	USBC_Writel(reg_value, (ccmu_base + 0x60));

	reg_value = USBC_Readl(ccmu_base + 0x2c0);
	reg_value &= ~(1 << 29);			/* ohci reset control, assert */
	reg_value &= ~(1 << 26);			/* ehci reset control, assert */
	USBC_Writel(reg_value, (ccmu_base + 0x2c0));

	//delay to wati SIE stable
	reg_value = 10000;
	while(reg_value--);

	//Enable module clock for USB phy1
	reg_value = USBC_Readl(ccmu_base + 0xcc);
	reg_value &= ~(1 << 16);			/* close specal clock for ohci */
	reg_value &= ~(1 << 9);				/* close specal clock for usb phy1(ehci0,ohci0) */
	reg_value &= ~(1 << 8);				/* close specal clock for usb phy0(otg) */
	reg_value &= ~(1 << 1);				/* usb phy1 reset */
	reg_value &= ~(1 << 0);				/* usb phy0 reset */
	USBC_Writel(reg_value, (ccmu_base + 0xcc));

	return 0;
#else
	u32 reg_value = 0;
	u32 ccmu_base = (u32)SUNXI_CCM_VBASE;

	DMSG_INFO("[%s]: close clock\n", sunxi_hci->hci_name);
	usb_hci_clock_cnt --;
	//AHB ALL clock gate
	reg_value = USBC_Readl(ccmu_base + 0x184);
	reg_value &= ~(1 << 1);			/* AHB clock gate usb0 */
	USBC_Writel(reg_value, (ccmu_base + 0x184));

	reg_value = USBC_Readl(ccmu_base + 0x1A4);
	reg_value &= ~(1 << 0);			/* AHB clock gate usb0 */
	USBC_Writel(reg_value, (ccmu_base + 0x1A4));

	reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE + 0x04);

	reg_value &= ~(0x01<<3);
	reg_value &= ~(0x01<<19);
	USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE + 0x04);

	reg_value = USBC_Readl(SUNXI_USB_CTRL_VBASE);
	reg_value &= ~(0x1<<3);
	reg_value &= ~(0x1<<4);
	reg_value &= ~(0x01<<18);
	USBC_Writel(reg_value, SUNXI_USB_CTRL_VBASE);
	return 0;
#endif
}
#endif

static void usb_passby(struct sunxi_hci_hcd *sunxi_hci, u32 enable)
{
	unsigned long reg_value = 0;
	spinlock_t lock;
	unsigned long flags = 0;

	spin_lock_init(&lock);
	spin_lock_irqsave(&lock, flags);

	/*enable passby*/
	if(sunxi_hci->usbc_no == 1){
		reg_value = USBC_Readl(sunxi_hci->usb_vbase + SUNXI_USB_PMU_IRQ_ENABLE);
		if(enable && usb1_enable_passly_cnt == 0){
			reg_value |= (1 << 10);		/* AHB Master interface INCR8 enable */
			reg_value |= (1 << 9);		/* AHB Master interface burst type INCR4 enable */
			reg_value |= (1 << 8);		/* AHB Master interface INCRX align enable */
#ifdef SUNXI_USB_FPGA
			reg_value |= (0 << 0);		/* enable ULPI, disable UTMI */
#else
			reg_value |= (1 << 0);		/* enable UTMI, disable ULPI */
#endif
		}else if(!enable && usb1_enable_passly_cnt == 1){
			reg_value &= ~(1 << 10);	/* AHB Master interface INCR8 disable */
			reg_value &= ~(1 << 9);		/* AHB Master interface burst type INCR4 disable */
			reg_value &= ~(1 << 8);		/* AHB Master interface INCRX align disable */
			reg_value &= ~(1 << 0);		/* ULPI bypass disable */
		}
		USBC_Writel(reg_value, (sunxi_hci->usb_vbase + SUNXI_USB_PMU_IRQ_ENABLE));

		if(enable){
			usb1_enable_passly_cnt++;
		}else{
			usb1_enable_passly_cnt--;
		}
	}else if(sunxi_hci->usbc_no == 2){
		reg_value = USBC_Readl(sunxi_hci->usb_vbase + SUNXI_USB_PMU_IRQ_ENABLE);
		if(enable && usb2_enable_passly_cnt == 0){
			reg_value |= (1 << 10);		/* AHB Master interface INCR8 enable */
			reg_value |= (1 << 9);		/* AHB Master interface burst type INCR4 enable */
			reg_value |= (1 << 8);		/* AHB Master interface INCRX align enable */
			reg_value |= (1 << 0);		/* ULPI bypass enable */
		}else if(!enable && usb2_enable_passly_cnt == 1){
			reg_value &= ~(1 << 10);	/* AHB Master interface INCR8 disable */
			reg_value &= ~(1 << 9);		/* AHB Master interface burst type INCR4 disable */
			reg_value &= ~(1 << 8);		/* AHB Master interface INCRX align disable */
			reg_value &= ~(1 << 0);		/* ULPI bypass disable */
		}
		USBC_Writel(reg_value, (sunxi_hci->usb_vbase + SUNXI_USB_PMU_IRQ_ENABLE));

		if(enable){
			usb2_enable_passly_cnt++;
		}else{
			usb2_enable_passly_cnt--;
		}
	}else if(sunxi_hci->usbc_no == 3){
		reg_value = USBC_Readl(sunxi_hci->usb_vbase + SUNXI_USB_PMU_IRQ_ENABLE);
		if(enable && usb3_enable_passly_cnt == 0){
			reg_value |= (1 << 10);		/* AHB Master interface INCR8 enable */
			reg_value |= (1 << 9);		/* AHB Master interface burst type INCR4 enable */
			reg_value |= (1 << 8);		/* AHB Master interface INCRX align enable */
			reg_value |= (1 << 0);		/* ULPI bypass enable */
		}else if(!enable && usb3_enable_passly_cnt == 1){
			reg_value &= ~(1 << 10);	/* AHB Master interface INCR8 disable */
			reg_value &= ~(1 << 9);		/* AHB Master interface burst type INCR4 disable */
			reg_value &= ~(1 << 8);		/* AHB Master interface INCRX align disable */
			reg_value &= ~(1 << 0);		/* ULPI bypass disable */
		}
		USBC_Writel(reg_value, (sunxi_hci->usb_vbase + SUNXI_USB_PMU_IRQ_ENABLE));

		if(enable){
			usb3_enable_passly_cnt++;
		}else{
			usb3_enable_passly_cnt--;
		}
	}else{
		DMSG_PANIC("EER: unkown usbc_no(%d)\n", sunxi_hci->usbc_no);
		spin_unlock_irqrestore(&lock, flags);
		return;
	}

	spin_unlock_irqrestore(&lock, flags);

	return;
}

static void hci_port_configure(struct sunxi_hci_hcd *sunxi_hci, u32 enable)
{
}

#ifndef  SUNXI_USB_FPGA

static int alloc_pin(struct sunxi_hci_hcd *sunxi_ehci)
{
	u32 ret = 1;

	if(sunxi_ehci->drv_vbus_gpio_valid){
		ret = gpio_request(sunxi_ehci->drv_vbus_gpio_set.gpio.gpio, NULL);
		if(ret != 0){
			DMSG_PANIC("ERR: gpio_request failed\n");
			sunxi_ehci->drv_vbus_gpio_valid = 0;
		}else{
			gpio_direction_output(sunxi_ehci->drv_vbus_gpio_set.gpio.gpio, 0);
		}
	}

	if(sunxi_ehci->usb_restrict_valid){
		ret = gpio_request(sunxi_ehci->restrict_gpio_set.gpio.gpio, NULL);
		if(ret != 0){
			DMSG_PANIC("ERR: gpio_request failed\n");
			sunxi_ehci->usb_restrict_valid = 0;
		}else{
			gpio_direction_output(sunxi_ehci->restrict_gpio_set.gpio.gpio, 0);
		}
	}
	if(sunxi_ehci->usb_restrict_valid){
		if(sunxi_ehci->usb_restrict_flag){
			__gpio_set_value(sunxi_ehci->restrict_gpio_set.gpio.gpio, 0);
		}else{
			__gpio_set_value(sunxi_ehci->restrict_gpio_set.gpio.gpio, 1);
		}
	}

	return 0;
}

static void free_pin(struct sunxi_hci_hcd *sunxi_ehci)
{
	if(sunxi_ehci->drv_vbus_gpio_valid){
		gpio_free(sunxi_ehci->drv_vbus_gpio_set.gpio.gpio);
		sunxi_ehci->drv_vbus_gpio_valid = 0;
	}

	if(sunxi_ehci->usb_restrict_valid){
		gpio_free(sunxi_ehci->restrict_gpio_set.gpio.gpio);
		sunxi_ehci->drv_vbus_gpio_valid = 0;
	}

	return;
}

static void __sunxi_set_vbus(struct sunxi_hci_hcd *sunxi_hci, int is_on)
{
	u32 on_off = 0;

	DMSG_INFO("[%s]: Set USB Power %s\n", sunxi_hci->hci_name, (is_on ? "ON" : "OFF"));

	/* set power flag */
	sunxi_hci->power_flag = is_on;

	/* set power */
	if(sunxi_hci->drv_vbus_gpio_set.gpio.data == 0){
		on_off = is_on ? 1 : 0;
	}else{
		on_off = is_on ? 0 : 1;
	}
	if(sunxi_hci->drv_vbus_gpio_valid){

		if((sunxi_hci->regulator_io != NULL) && (sunxi_hci->regulator_io_hdle != NULL)){
			if(on_off){
				if(regulator_enable(sunxi_hci->regulator_io_hdle) < 0){
					DMSG_INFO("ERR: %s: regulator_enable fail\n", sunxi_hci->hci_name);
				}

			}else{
				if(regulator_disable(sunxi_hci->regulator_io_hdle) < 0){
					DMSG_INFO("ERR: %s: regulator_disable fail\n",  sunxi_hci->hci_name);
				}
			}
		}

		__gpio_set_value(sunxi_hci->drv_vbus_gpio_set.gpio.gpio, on_off);
	}

	return;
}

#else

static int alloc_pin(struct sunxi_hci_hcd *sunxi_ehci)
{
	return 0;
}

static void free_pin(struct sunxi_hci_hcd *sunxi_ehci)
{
	return;
}

static void __sunxi_set_vbus(struct sunxi_hci_hcd *sunxi_hci, int is_on)
{
	return;
}

#endif

static void sunxi_set_vbus(struct sunxi_hci_hcd *sunxi_hci, int is_on)
{
	DMSG_DEBUG("[%s]: sunxi_set_vbus cnt %d\n",
		sunxi_hci->hci_name,
		(sunxi_hci->usbc_no == 1) ? usb1_set_vbus_cnt : usb2_set_vbus_cnt);

	if(sunxi_hci->usbc_no == 1){
		if(is_on && usb1_set_vbus_cnt == 0){
			__sunxi_set_vbus(sunxi_hci, is_on);  /* power on */
		}else if(!is_on && usb1_set_vbus_cnt == 1){
			__sunxi_set_vbus(sunxi_hci, is_on);  /* power off */
		}

		if(is_on){
			usb1_set_vbus_cnt++;
		}else{
			usb1_set_vbus_cnt--;
		}
	}else if(sunxi_hci->usbc_no == 2){
		if(is_on && usb2_set_vbus_cnt == 0){
			__sunxi_set_vbus(sunxi_hci, is_on);  /* power on */
		}else if(!is_on && usb2_set_vbus_cnt == 1){
			__sunxi_set_vbus(sunxi_hci, is_on);  /* power off */
		}

		if(is_on){
			usb2_set_vbus_cnt++;
		}else{
			usb2_set_vbus_cnt--;
		}
	}else{
		if(is_on && usb3_set_vbus_cnt == 0){
			__sunxi_set_vbus(sunxi_hci, is_on);  /* power on */
		}else if(!is_on && usb3_set_vbus_cnt == 1){
			__sunxi_set_vbus(sunxi_hci, is_on);  /* power off */
		}

		if(is_on){
			usb3_set_vbus_cnt++;
		}else{
			usb3_set_vbus_cnt--;
		}
	}

	return;
}

//---------------------------------------------------------------
//  EHCI
//---------------------------------------------------------------

#define  SUNXI_EHCI_NAME		"sunxi-ehci"
static const char ehci_name[] = SUNXI_EHCI_NAME;

static struct sunxi_hci_hcd sunxi_ehci0;
static struct sunxi_hci_hcd sunxi_ehci1;
static struct sunxi_hci_hcd sunxi_ehci2;

static u64 sunxi_ehci_dmamask = DMA_BIT_MASK(32);

static struct platform_device sunxi_usb_ehci_device[] = {
	[0] = {
		.name		= ehci_name,
		.id		= 1,
		.dev 		= {
			.dma_mask		= &sunxi_ehci_dmamask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
			.platform_data		= &sunxi_ehci0,
		},
	},

	[1] = {
		.name		= ehci_name,
		.id		= 2,
		.dev 		= {
			.dma_mask		= &sunxi_ehci_dmamask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
			.platform_data		= &sunxi_ehci1,
		},
	},

	[2] = {
		.name		= ehci_name,
		.id		= 3,
		.dev 		= {
			.dma_mask		= &sunxi_ehci_dmamask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
			.platform_data		= &sunxi_ehci2,
		},
	},
};

//---------------------------------------------------------------
//  OHCI
//---------------------------------------------------------------
#define  SUNXI_OHCI_NAME		"sunxi-ohci"
static const char ohci_name[] = SUNXI_OHCI_NAME;

static struct sunxi_hci_hcd sunxi_ohci0;

#if defined(CONFIG_USB_SUNXI_OHCI0) || defined(CONFIG_USB_SUNXI_OHCI1) || defined(CONFIG_USB_SUNXI_OHCI2) /* kill warning */
static struct sunxi_hci_hcd sunxi_ohci1;
static struct sunxi_hci_hcd sunxi_ohci2;

static u64 sunxi_ohci_dmamask = DMA_BIT_MASK(32);

static struct platform_device sunxi_usb_ohci_device[] = {
	[0] = {
		.name		= ohci_name,
		.id		= 1,
		.dev 		= {
			.dma_mask		= &sunxi_ohci_dmamask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
			.platform_data		= &sunxi_ohci0,
		},
	},

	[1] = {
		.name		= ohci_name,
		.id		= 2,
		.dev 		= {
			.dma_mask		= &sunxi_ohci_dmamask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
			.platform_data		= &sunxi_ohci1,
		},
	},

	[2] = {
		.name		= ohci_name,
		.id		= 3,
		.dev 		= {
			.dma_mask		= &sunxi_ohci_dmamask,
			.coherent_dma_mask	= DMA_BIT_MASK(32),
			.platform_data		= &sunxi_ohci2,
		},
	},
};
#endif

static void print_sunxi_hci(struct sunxi_hci_hcd *sunxi_hci)
{
	DMSG_DEBUG("\n------%s config------\n", sunxi_hci->hci_name);
	DMSG_DEBUG("hci_name                 = %s\n", sunxi_hci->hci_name);
	DMSG_DEBUG("irq_no                   = %d\n", sunxi_hci->irq_no);
	DMSG_DEBUG("usbc_no                  = %d\n", sunxi_hci->usbc_no);

	DMSG_DEBUG("usb_vbase                = 0x%p\n", sunxi_hci->usb_vbase);
	DMSG_DEBUG("sram_vbase               = 0x%p\n", sunxi_hci->sram_vbase);
	DMSG_DEBUG("clock_vbase              = 0x%p\n", sunxi_hci->clock_vbase);
	DMSG_DEBUG("sdram_vbase              = 0x%p\n", sunxi_hci->sdram_vbase);

	DMSG_DEBUG("used                     = %d\n", sunxi_hci->used);
	DMSG_DEBUG("host_init_state          = %d\n", sunxi_hci->host_init_state);

	DMSG_DEBUG("gpio_name                = %s\n", sunxi_hci->drv_vbus_gpio_set.gpio_name);
	DMSG_DEBUG("port                     = %d\n", sunxi_hci->drv_vbus_gpio_set.port);
	DMSG_DEBUG("port_num                 = %d\n", sunxi_hci->drv_vbus_gpio_set.port_num);
	DMSG_DEBUG("mul_sel                  = %d\n", sunxi_hci->drv_vbus_gpio_set.mul_sel);
	DMSG_DEBUG("pull                     = %d\n", sunxi_hci->drv_vbus_gpio_set.pull);
	DMSG_DEBUG("drv_level                = %d\n", sunxi_hci->drv_vbus_gpio_set.drv_level);
	DMSG_DEBUG("data                     = %d\n", sunxi_hci->drv_vbus_gpio_set.data);

	DMSG_DEBUG("\n--------------------------\n");

	return;
}

static int init_sunxi_hci(struct sunxi_hci_hcd *sunxi_hci, u32 usbc_no, u32 ohci, const char *hci_name)
{
	s32 ret = 0;

	memset(sunxi_hci, 0, sizeof(struct sunxi_hci_hcd));

	sunxi_hci->usbc_no = usbc_no;
	sunxi_hci->usbc_type = ohci ? SUNXI_USB_OHCI : SUNXI_USB_EHCI;

	if(ohci){
		sunxi_hci->irq_no = ohci_irq_no[sunxi_hci->usbc_no];
	}else{
		sunxi_hci->irq_no = ehci_irq_no[sunxi_hci->usbc_no];
	}

	sprintf(sunxi_hci->hci_name, "%s%d", hci_name, sunxi_hci->usbc_no);

	sunxi_hci->usb_vbase	= (void __iomem	*)usbc_base[sunxi_hci->usbc_no];
	sunxi_hci->sram_vbase	= (void __iomem	*)SUNXI_SRAMCTRL_VBASE;
	sunxi_hci->clock_vbase	= (void __iomem	*)SUNXI_CCM_VBASE;
	sunxi_hci->gpio_vbase	= (void __iomem	*)SUNXI_PIO_VBASE;
	sunxi_hci->sdram_vbase	= (void __iomem	*)SUNXI_SDMMC1_VBASE;

	get_usb_cfg(sunxi_hci);
	request_usb_regulator_io(sunxi_hci);
	sunxi_hci->open_clock	= open_clock;
	sunxi_hci->close_clock	= close_clock;
	sunxi_hci->set_power	= sunxi_set_vbus;
	sunxi_hci->usb_passby	= usb_passby;
	sunxi_hci->port_configure = hci_port_configure;

#ifdef  CONFIG_USB_SUNXI_HSIC
	u32 reg_value = 0;
	reg_value = USBC_Readl(sunxi_hci->sram_vbase+ SUNXI_USB_PMU_IRQ_ENABLE);
	reg_value |= (1 << 1);
	reg_value |= (1 << 20);
	reg_value |= (1 << 17);
	USBC_Writel(reg_value, (sunxi_hci->sram_vbase+ SUNXI_USB_PMU_IRQ_ENABLE));
#endif

#ifndef CONFIG_ARCH_SUN9IW1
#ifdef  SUNXI_USB_FPGA
	fpga_config_use_hci((__u32 __force)sunxi_hci->sram_vbase);
#endif
#endif

	ret = clock_init(sunxi_hci, ohci);
	if(ret != 0){
		DMSG_PANIC("ERR: clock_init failed\n");
		goto failed1;
	}

	print_sunxi_hci(sunxi_hci);

	return 0;

failed1:
	return -1;
}

static int exit_sunxi_hci(struct sunxi_hci_hcd *sunxi_hci, u32 ohci)
{
	clock_exit(sunxi_hci, ohci);
	release_usb_regulator_io(sunxi_hci);
	return 0;
}

static int __init sunxi_hci_init(void)
{
	/* USB1 */
	init_sunxi_hci(&sunxi_ehci0, 1, 0, ehci_name);
	init_sunxi_hci(&sunxi_ohci0, 1, 1, ohci_name);
	alloc_pin(&sunxi_ehci0);

#ifdef CONFIG_ARCH_SUN9IW1
	hci_ahb_gate = clk_get(NULL, USBHCI_CLK);
	if (IS_ERR(hci_ahb_gate)){
		DMSG_PANIC("ERR: OPEN hci_ahb_gate failed.\n");
	}

#ifdef CONFIG_USB_SUNXI_HSIC
	vbat_hsic_hdle = regulator_get(NULL, vbat_hsic_name);
	if(IS_ERR(vbat_hsic_hdle)) {
		DMSG_PANIC("ERR: some error happen, vbat_hsic_hdle fail to get regulator!\n");
		return 0;
	}

	if(regulator_set_voltage(vbat_hsic_hdle , VBAT_HSIC_VALUE, VBAT_HSIC_VALUE) < 0 ){
		DMSG_PANIC("ERR: hsic_vbat regulator_set_voltage fail\n",);
		regulator_put(vbat_hsic_hdle);
		return 0;
	}
#else
	vbat_usbh_hdle = regulator_get(NULL, vbat_usbh_name);
	if(IS_ERR(vbat_usbh_hdle)) {
		DMSG_PANIC("ERR: some error happen,vbat_usbh_hdle fail to get regulator!\n");
		return 0;
	}

	if( regulator_set_voltage(vbat_usbh_hdle , VBAT_USBH_VALUE, VBAT_USBH_VALUE) < 0 ){
		DMSG_PANIC("ERR: usbh_vbat regulator_set_voltage fail, return\n");
		regulator_put(vbat_usbh_hdle);
		return 0;
	}

#endif

	/* USB2 */
	init_sunxi_hci(&sunxi_ehci1, 2, 0, ehci_name);
	init_sunxi_hci(&sunxi_ohci1, 2, 1, ohci_name);
	alloc_pin(&sunxi_ehci1);

	/* USB3 */
	init_sunxi_hci(&sunxi_ehci2, 3, 0, ehci_name);
	init_sunxi_hci(&sunxi_ohci2, 3, 1, ohci_name);
	alloc_pin(&sunxi_ehci2);
#endif

#ifdef  CONFIG_USB_SUNXI_EHCI0
	if(sunxi_ehci0.used){
		platform_device_register(&sunxi_usb_ehci_device[0]);
	}else{
		DMSG_INFO("ERR: usb%d %s is not enable\n", sunxi_ehci0.usbc_no, sunxi_ehci0.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_OHCI0
	if(sunxi_ohci0.used){
		platform_device_register(&sunxi_usb_ohci_device[0]);
	}else{
		DMSG_INFO("usb%d %s is not enable\n", sunxi_ohci0.usbc_no, sunxi_ohci0.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_EHCI1
	if(sunxi_ehci1.used){
		platform_device_register(&sunxi_usb_ehci_device[1]);
	}else{
		DMSG_INFO("usb%d %s is not enable\n", sunxi_ehci1.usbc_no, sunxi_ehci1.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_OHCI1
	if(sunxi_ohci1.used){
		platform_device_register(&sunxi_usb_ohci_device[1]);
	}else{
		DMSG_INFO("usb%d %s is not enable\n", sunxi_ohci1.usbc_no, sunxi_ohci1.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_EHCI2
	if(sunxi_ehci2.used){
			platform_device_register(&sunxi_usb_ehci_device[2]);
	}else{
		DMSG_INFO("usb%d %s is not enable\n", sunxi_ehci2.usbc_no, sunxi_ehci2.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_OHCI2
	if(sunxi_ohci2.used){
		platform_device_register(&sunxi_usb_ohci_device[2]);
	}else{
		DMSG_INFO("usb%d %s is not enable\n", sunxi_ohci2.usbc_no, sunxi_ohci2.hci_name);
	}
#endif

	return 0;
}

static void __exit sunxi_hci_exit(void)
{

#ifdef  CONFIG_USB_SUNXI_EHCI0
	if(sunxi_ehci0.used){
		platform_device_unregister(&sunxi_usb_ehci_device[0]);
	}else{
		DMSG_INFO("usb%d %s is disable\n", sunxi_ehci0.usbc_no, sunxi_ehci0.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_OHCI0
	if(sunxi_ohci0.used){
		platform_device_unregister(&sunxi_usb_ohci_device[0]);
	}else{
		DMSG_INFO("usb%d %s is disable\n", sunxi_ohci0.usbc_no, sunxi_ohci0.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_EHCI1
	if(sunxi_ehci1.used){
		platform_device_unregister(&sunxi_usb_ehci_device[1]);
	}else{
		DMSG_INFO("usb%d %s is disable\n", sunxi_ehci1.usbc_no, sunxi_ehci1.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_OHCI1
	if(sunxi_ohci1.used){
		platform_device_unregister(&sunxi_usb_ohci_device[1]);
	}else{
		DMSG_INFO("usb%d %s is disable\n", sunxi_ohci1.usbc_no, sunxi_ohci1.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_EHCI2
	if(sunxi_ehci2.used){
		platform_device_unregister(&sunxi_usb_ehci_device[2]);
	}else{
		DMSG_INFO("usb%d %s is disable\n", sunxi_ehci2.usbc_no, sunxi_ehci2.hci_name);
	}
#endif

#ifdef  CONFIG_USB_SUNXI_OHCI2
	if(sunxi_ohci2.used){
		platform_device_unregister(&sunxi_usb_ohci_device[2]);
	}else{
		DMSG_INFO("usb%d %s is disable\n", sunxi_ohci2.usbc_no, sunxi_ohci2.hci_name);
	}
#endif

	/* USB1 */
	exit_sunxi_hci(&sunxi_ehci0, 0);
	exit_sunxi_hci(&sunxi_ohci0, 1);
	free_pin(&sunxi_ehci0);

#ifdef CONFIG_ARCH_SUN9IW1
	if(!IS_ERR(hci_ahb_gate)){
		clk_put(hci_ahb_gate);
		hci_ahb_gate = NULL;
	}

#ifdef CONFIG_USB_SUNXI_HSIC
	regulator_put(vbat_hsic_hdle);
#else
	regulator_put(vbat_usbh_hdle);
#endif

	/* USB2 */
	exit_sunxi_hci(&sunxi_ehci1, 0);
	exit_sunxi_hci(&sunxi_ohci1, 1);
	free_pin(&sunxi_ehci1);

	/*USB3 */
	exit_sunxi_hci(&sunxi_ehci2, 0);
	exit_sunxi_hci(&sunxi_ohci2, 1);
	free_pin(&sunxi_ehci2);
#endif

	return ;
}

fs_initcall(sunxi_hci_init);
//module_init(sunxi_hci_init);
module_exit(sunxi_hci_exit);

