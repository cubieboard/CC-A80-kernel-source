/*
 * arch/arm/mach-sunxi/include/mach/sun9i/memory-sun9iw1p1.h
 *
 * Copyright(c) 2013-2015 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: liugang <liugang@allwinnertech.com>
 *
 * sun9i memory header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MEMORY_SUN9I_W1P1_H
#define __MEMORY_SUN9I_W1P1_H

#define PLAT_PHYS_OFFSET         UL(0x20000000)
#ifdef CONFIG_EVB_PLATFORM
#define PLAT_MEM_SIZE            SZ_1G
#else
#define PLAT_MEM_SIZE            SZ_512M
#endif

#define SYS_CONFIG_MEMBASE       (PLAT_PHYS_OFFSET + SZ_32M + SZ_16M)        /* 0x23000000 */
#define SYS_CONFIG_MEMSIZE       (SZ_128K)                                   /* 0x00020000 */

#define SUPER_STANDBY_MEM_BASE   (SYS_CONFIG_MEMBASE + SYS_CONFIG_MEMSIZE)   /* 0x23020000 */
#define SUPER_STANDBY_MEM_SIZE   (SZ_2K)                                     /* 0x00000800 */

#if defined(CONFIG_ION) || defined(CONFIG_ION_MODULE)
#define ION_CARVEOUT_MEM_BASE    (0x60000000) /* 0x23100000 */
#define ION_CARVEOUT_MEM_SIZE    (CONFIG_ION_SUNXI_CARVEOUT_SIZE * SZ_1M)
#endif

#define SRAM_DDRFREQ_OFFSET	0xf0014000
#define __sram	__section(.sram.text)
#define __sramdata __section(.sram.data)

#define SUNXI_DDRFREQ_SRAM_SECTION(OFFSET, align) 			\
	. = ALIGN(align);					\
	__sram_start = .;					\
	.sram_text OFFSET : AT(__sram_start) {			\
		. = ALIGN(align);				\
		__sram_text_start = .;				\
		*(.sram.text)					\
		__sram_text_end = .;				\
	}							\
	.sram_data OFFSET + SIZEOF(.sram_text) :		\
		AT(__sram_start + SIZEOF(.sram_text)) {		\
		. = ALIGN(align);				\
		__sram_data_start = .;				\
		*(.sram.data)					\
		__sram_data_end = .;				\
	}							\
	. = __sram_start + SIZEOF(.sram_text) +			\
			SIZEOF(.sram_data);			\
	__sram_end = .;

#endif /* __MEMORY_SUN9I_W1P1_H */
