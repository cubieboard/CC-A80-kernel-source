#include "panels.h"
#include "default_panel.h"

struct sunxi_lcd_drv g_lcd_drv;

sunxi_lcd_panel* panel_array[] = {
	&default_panel,
	/* add new panel below */

	NULL,
};

static void lcd_set_panel_funs(void)
{
	int i;

	for(i=0; panel_array[i] != NULL; i++) {
		sunxi_lcd_register_panel(panel_array[i]);
	}

	return ;
}

int lcd_init(void)
{
	sunxi_disp_get_source_ops(&g_lcd_drv.src_ops);
	lcd_set_panel_funs();

	return 0;
}
