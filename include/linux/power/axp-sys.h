#ifndef _AXP_SYS_H
#define _AXP_SYS_H

#include <linux/mfd/axp-mfd.h>

extern int axp_usb_det(void);
extern int axp_usbcur(aw_charge_type type);
extern int axp_usbvol(aw_charge_type type);
extern int axp_powerkey_get(void);
extern void axp_powerkey_set(int value);
extern unsigned long axp_read_power_sply(void);
#endif
