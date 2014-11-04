#ifndef __PANEL_H__
#define __PANEL_H__
#include "../de/bsp_display.h"
#include "lcd_source.h"

extern sunxi_lcd_panel * panel_array[];

#define SUNXI_LCD_FUNC(lcd_func, delay_ms) \
{\
	.func = lcd_func, \
	.delay = delay_ms \
},

struct sunxi_lcd_drv
{
  struct sunxi_disp_source_ops      src_ops;
};

extern int sunxi_disp_get_source_ops(struct sunxi_disp_source_ops *src_ops);
#endif
