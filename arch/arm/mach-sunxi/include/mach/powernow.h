/*
 * arch/arm/mach-sun7i/include/mach/powernow.h
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

#ifndef _POWERNOW_H_
#define _POWERNOW_H_

#include <linux/notifier.h>

#define SW_POWERNOW_EXTREMITY           0
#define SW_POWERNOW_USB                 1
#define SW_POWERNOW_PERFORMANCE         2

#define SW_POWERNOW_USBSTAT_INACTIVE    0               
#define SW_POWERNOW_USBSTAT_ACTIVE      1               

int register_sw_powernow_notifier(struct notifier_block *nb);
int unregister_sw_powernow_notifier(struct notifier_block *nb);
void sw_powernow_switch_to(int mode);
void sw_powernow_set_usb(int status);

#endif /* _POWERNOW_H_ */
