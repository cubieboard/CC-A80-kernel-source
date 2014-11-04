/*
 * arch/arm/mach-sunxi/include/mach/memory.h
 *
 * Copyright(c) 2013-2015 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: liugang <liugang@allwinnertech.com>
 *
 * sunxi memory header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __SUNXI_MEMORY_H
#define __SUNXI_MEMORY_H

/*
 * NOTE: CMA reserved area: (CONFIG_CMA_RESERVE_BASE ~ CONFIG_CMA_RESERVE_BASE + CONFIG_CMA_SIZE_MBYTES),
 *   which is reserved in the function of dma_contiguous_reserve, drivers/base/dma-contiguous.c.
 *   Please DO NOT conflict with it when you reserved your own areas.
 *
 *   We need to restrict CMA area in the front 256M, because VE only support these address.
 */

#if defined CONFIG_ARCH_SUN8IW1P1
#include "sun8i/memory-sun8iw1p1.h"
#elif defined CONFIG_ARCH_SUN8IW3P1
#include "sun8i/memory-sun8iw3p1.h"
#elif defined CONFIG_ARCH_SUN8IW5P1
#include "sun8i/memory-sun8iw5p1.h"
#elif defined CONFIG_ARCH_SUN8IW6P1
#include "sun8i/memory-sun8iw6p1.h"
#elif defined CONFIG_ARCH_SUN8IW7P1
#include "sun8i/memory-sun8iw7p1.h"
#elif defined CONFIG_ARCH_SUN9IW1P1
#include "sun9i/memory-sun9iw1p1.h"
#else
#error "please select a platform\n"
#endif

#endif
