/*
 * linux/arch/arm/mach-sunxi/sun9i-chip.c
 *
 * Copyright(c) 2014-2016 Allwinnertech Co., Ltd.
 *         http://www.allwinnertech.com
 *
 * Author: sunny <sunny@allwinnertech.com>
 * Author: superm <superm@allwinnertech.com>
 *
 * allwinner sunxi soc chip version and chip id manager.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/cache.h>
#include <linux/syscore_ops.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/arisc/arisc.h>
#include <asm/cacheflush.h>

#include <mach/platform.h>
#include <mach/sunxi-chip.h>

static unsigned int sunxi_soc_ver;
static unsigned int sunxi_soc_chipid[4];
static unsigned int sunxi_pmu_chipid[4];
static unsigned int sunxi_serial[4];

unsigned int sunxi_get_soc_ver(void)
{
	return sunxi_soc_ver;
}
EXPORT_SYMBOL(sunxi_get_soc_ver);

int sunxi_get_soc_chipid(u8 *chipid)
{
	memcpy(chipid, sunxi_soc_chipid, 16);

	return 0;
}
EXPORT_SYMBOL(sunxi_get_soc_chipid);

int sunxi_get_pmu_chipid(u8 *chipid)
{
	memcpy(chipid, sunxi_pmu_chipid, 16);
	
	return 0;
}
EXPORT_SYMBOL(sunxi_get_pmu_chipid);

int sunxi_get_serial(u8 *serial)
{
	memcpy(serial, sunxi_serial, 16);

	return 0;
}
EXPORT_SYMBOL(sunxi_get_serial);

/*
 * get sunxi soc bin
 *
 * return: the bin of sunxi soc, like that:
 * 0 : fail
 * 1 : slow
 * 2 : normal
 * 3 : fast
 */
unsigned int sunxi_get_soc_bin(void)
{
	u8 chipid_u8[16];
	u32 chipid_u32[4];
	u32 type = 0;

	sunxi_get_soc_chipid(chipid_u8);
	memcpy(chipid_u32, chipid_u8, sizeof(chipid_u8));
#if defined CONFIG_ARCH_SUN9IW1P1
	type = (chipid_u32[1] >> 14) & 0x3f;
#elif defined CONFIG_ARCH_SUN8IW5P1
	type = (chipid_u32[3] >> 11) & 0x3f;
#endif
	switch (type)
	{
		case 0b000000:
			return 0;
		case 0b000001:
			return 1;
		case 0b000011:
			return 0;
		case 0b000111:
			return 2;
		default:
			return 0;
	}
}
EXPORT_SYMBOL(sunxi_get_soc_bin);

unsigned int __hex2dec(unsigned int hex)
{
	unsigned int dec;

	switch (hex) {
	        case 48: dec = 0; break;
	        case 49: dec = 1; break;
	        case 50: dec = 2; break;
	        case 51: dec = 3; break;
	        case 52: dec = 4; break;
	        case 53: dec = 5; break;
	        case 54: dec = 6; break;
	        case 55: dec = 7; break;
	        case 56: dec = 8; break;
	        case 57: dec = 9; break;
	        default:
		pr_err("something wrong in chip id\n");
		dec = 0;
	}

	return dec;
}

void __init sunxi_soc_ver_init(void)
{
#if (defined CONFIG_ARCH_SUN8IW1P1)
	u32 reg_val = 0x1;

	/* sun8iw1p1 chip version init */
	writel(reg_val, SUNXI_RTC_VBASE + 0x20c);
	reg_val = readl(SUNXI_RTC_VBASE + 0x20c);
	if (reg_val == 0x1)
		sunxi_soc_ver = SUN8IW1P1_REV_A;
	else if (reg_val == 0x0)
		sunxi_soc_ver = SUN8IW1P1_REV_B;
	else if (reg_val == 0x2)
		sunxi_soc_ver = SUN8IW1P1_REV_C;
	else if (reg_val == 0x100002)
		sunxi_soc_ver = SUN8IW1P1_REV_D;
	else
		sunxi_soc_ver = SUN8IW1P1_REV_D;
#elif (defined CONFIG_ARCH_SUN8IW3P1)
	/* sun8iw3p1 chip version init */
	if ((readl(SUNXI_R_PRCM_VBASE + 0x190) >> 0x3) & 0x1) {
		sunxi_soc_ver = SUN8IW3P1_REV_B;
	} else {
		sunxi_soc_ver = SUN8IW3P1_REV_A;
	}
#elif (defined CONFIG_ARCH_SUN8IW5P1)
	/* sun8iw5p1 chip version init */
	if ((readl(SUNXI_R_PRCM_VBASE + 0x190) >> 0x3) & 0x1) {
		sunxi_soc_ver = SUN8IW5P1_REV_B;
	} else {
		sunxi_soc_ver = SUN8IW5P1_REV_A;
	}
#elif (defined CONFIG_ARCH_SUN8IW6P1)
	/* sun8iw6p1 chip version init */
	if ((readl(SUNXI_R_PRCM_VBASE + 0x190) >> 0x3) & 0x1) {
		sunxi_soc_ver = SUN8IW6P1_REV_B;
	} else {
		sunxi_soc_ver = SUN8IW6P1_REV_A;
	}
#elif (defined CONFIG_ARCH_SUN9IW1P1)
	/* sun9iw1p1 chip version init */
	if ((readl(SUNXI_R_PRCM_VBASE + 0x190) >> 0x3) & 0x1) {
		sunxi_soc_ver = SUN9IW1P1_REV_B;
	} else {
		sunxi_soc_ver = SUN9IW1P1_REV_A;
	}
#endif
}

void __init sunxi_chip_id_init(void)
{
#if (defined CONFIG_ARCH_SUN8IW1P1)

	/* sun8iw1p1 pmu chip id init */
#ifdef CONFIG_SUNXI_ARISC
	arisc_axp_get_chip_id((u8 *)sunxi_pmu_chipid);
#endif

	/* sun8iw1p1 serial init */
	if (sunxi_pmu_chipid[0] == 0 && sunxi_pmu_chipid[1] == 0 &&
		sunxi_pmu_chipid[2] == 0 && sunxi_pmu_chipid[3] == 0) {
		memset((void *)sunxi_serial, 0, sizeof(sunxi_serial));
		return 0;
	}

	sunxi_serial[0] = sunxi_pmu_chipid[3];
	sunxi_serial[1] = (sunxi_pmu_chipid[0] >> 24) & 0xff;
	sunxi_serial[1] |= (sunxi_pmu_chipid[1] & 0xff) << 8;
	sunxi_serial[1] |= ((sunxi_pmu_chipid[1] >> 8) & 0xff) << 16;
	sunxi_serial[1] |= __hex2dec((sunxi_pmu_chipid[1] >> 16) & 0xff) << 24;
	sunxi_serial[1] |= __hex2dec((sunxi_pmu_chipid[1] >> 24) & 0xff) << 28;
	sunxi_serial[2] |= (sunxi_pmu_chipid[2]&0xff000000) >> 20;
	sunxi_serial[2] |= __hex2dec(sunxi_pmu_chipid[2]&0xff);
#elif (defined CONFIG_ARCH_SUN8IW3P1)

	/* sun8iw3p1 pmu chip id init */
#ifdef CONFIG_SUNXI_ARISC
	arisc_axp_get_chip_id((u8 *)sunxi_pmu_chipid);
#endif

	/* sun8iw3p1 serial init */
	if (sunxi_pmu_chipid[0] == 0 && sunxi_pmu_chipid[1] == 0 &&
		sunxi_pmu_chipid[2] == 0 && sunxi_pmu_chipid[3] == 0) {
		memset((void *)sunxi_serial, 0, sizeof(sunxi_serial));
		return 0;
	}

	sunxi_serial[0] = sunxi_pmu_chipid[3];
	sunxi_serial[1] = (sunxi_pmu_chipid[0] >> 24) & 0xff;
	sunxi_serial[1] |= (sunxi_pmu_chipid[1] & 0xff) << 8;
	sunxi_serial[1] |= ((sunxi_pmu_chipid[1] >> 8) & 0xff) << 16;
	sunxi_serial[1] |= __hex2dec((sunxi_pmu_chipid[1] >> 16) & 0xff) << 24;
	sunxi_serial[1] |= __hex2dec((sunxi_pmu_chipid[1] >> 24) & 0xff) << 28;
	sunxi_serial[2] |= (sunxi_pmu_chipid[2]&0xff000000) >> 20;
	sunxi_serial[2] |= __hex2dec(sunxi_pmu_chipid[2]&0xff);
#elif (defined CONFIG_ARCH_SUN8IW5P1)

	/* sun8iw5p1 soc chip id init */
	sunxi_soc_chipid[0] = readl(SUNXI_SID_VBASE);
	sunxi_soc_chipid[1] = readl(SUNXI_SID_VBASE + 0x4);
	sunxi_soc_chipid[2] = readl(SUNXI_SID_VBASE + 0x8);
	sunxi_soc_chipid[3] = readl(SUNXI_SID_VBASE + 0xc);

	/* sun8iw5p1 pmu chip id init */
#ifdef CONFIG_SUNXI_ARISC
	arisc_axp_get_chip_id((u8 *)sunxi_pmu_chipid);
#endif

	/* sun8iw5p1 serial init */
	sunxi_serial[0] = sunxi_soc_chipid[3];
	sunxi_serial[1] = sunxi_soc_chipid[2];
	sunxi_serial[2] = (sunxi_soc_chipid[1] >> 16) & 0xFFFF;
#elif (defined CONFIG_ARCH_SUN8IW6P1)

	/* sun8iw6p1 soc chip id init */
	sunxi_soc_chipid[0] = readl(SUNXI_SID_VBASE + 0x200);
	sunxi_soc_chipid[1] = readl(SUNXI_SID_VBASE + 0x200 + 0x4);
	sunxi_soc_chipid[2] = readl(SUNXI_SID_VBASE + 0x200 + 0x8);
	sunxi_soc_chipid[3] = readl(SUNXI_SID_VBASE + 0x200 + 0xc);

	/* sun8iw6p1 pmu chip id init */
#ifdef CONFIG_SUNXI_ARISC
	arisc_axp_get_chip_id((u8 *)sunxi_pmu_chipid);
#endif

	/* sun8iw6p1 serial init */
	sunxi_serial[0] = sunxi_soc_chipid[3];
	sunxi_serial[1] = sunxi_soc_chipid[2];
	sunxi_serial[2] = (sunxi_soc_chipid[1] >> 16) & 0xFFFF;
#elif (defined CONFIG_ARCH_SUN9IW1P1)

	/* sun9iw1p1 soc chip id init */
	sunxi_soc_chipid[0] = readl(SUNXI_SID_VBASE + 0x200);
	sunxi_soc_chipid[1] = readl(SUNXI_SID_VBASE + 0x200 + 0x4);
	sunxi_soc_chipid[2] = readl(SUNXI_SID_VBASE + 0x200 + 0x8);
	sunxi_soc_chipid[3] = readl(SUNXI_SID_VBASE + 0x200 + 0xc);

	/* sun9iw1p1 pmu chip id init */
#ifdef CONFIG_SUNXI_ARISC
	arisc_axp_get_chip_id((u8 *)sunxi_pmu_chipid);
#endif

	/* sun9iw1p1 serial init */
	sunxi_serial[0] = sunxi_soc_chipid[3];
	sunxi_serial[1] = sunxi_soc_chipid[2];
	sunxi_serial[2] = (sunxi_soc_chipid[1] >> 16) & 0xFFFF;
#endif
}


