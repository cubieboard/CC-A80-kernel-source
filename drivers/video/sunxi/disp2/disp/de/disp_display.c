#include "disp_display.h"

disp_dev_t gdisp;

s32 bsp_disp_init(disp_bsp_init_para * para)
{
	u32 num_screens, disp;

	memset(&gdisp,0x00,sizeof(disp_dev_t));
	memcpy(&gdisp.init_para,para,sizeof(disp_bsp_init_para));
	para->shadow_protect = bsp_disp_shadow_protect;
	disp_init_feat();

	num_screens = bsp_disp_feat_get_num_screens();
	for(disp = 0; disp < num_screens; disp++) {
#if defined(__LINUX_PLAT__)
		spin_lock_init(&gdisp.screen[disp].flag_lock);
#endif
	}

	bsp_disp_set_print_level(DEFAULT_PRINT_LEVLE);
	disp_init_al(para);
	disp_init_mgr(para);
	disp_init_lcd(para);
	disp_init_enhance(para);
	disp_init_smbl(para);
	disp_init_capture(para);

	disp_init_connections();

	return DIS_SUCCESS;
}

s32 bsp_disp_exit(u32 mode)
{
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	if(mode == DISP_EXIT_MODE_CLEAN_ALL) {
		/* close all mod and unregister all irq */

	}	else if(mode == DISP_EXIT_MODE_CLEAN_PARTLY) {
		/* unregister all irq */

	}

	return DIS_SUCCESS;
}

s32 bsp_disp_open(void)
{
	return DIS_SUCCESS;
}

s32 bsp_disp_close(void)
{
	return DIS_SUCCESS;
}

s32 disp_init_connections(void)
{
	u32 disp = 0;
	u32 num_screens = 0;
	u32 num_layers = 0,layer_id = 0;

	DE_INF("disp_init_connections\n");

	num_screens = bsp_disp_feat_get_num_screens();
	for(disp=0; disp<num_screens; disp++) {
		struct disp_manager *mgr;
		struct disp_layer *lyr;
		struct disp_device *lcd = NULL;
		struct disp_enhance *enhance = NULL;
		struct disp_smbl *smbl = NULL;
		struct disp_capture *cptr = NULL;

		mgr = disp_get_layer_manager(disp);
		if(!mgr)
			continue;

		/* connect layer & it's manager */
		num_layers = bsp_disp_feat_get_num_layers(disp);
		for(layer_id=0; layer_id<num_layers; layer_id++) {
			lyr = disp_get_layer_1(disp, layer_id);
			if(NULL != lyr) {
				lyr->set_manager(lyr, mgr);
			}
		}

		/* connect device & it's manager */
		if(bsp_disp_feat_is_supported_output_types(disp, DISP_OUTPUT_TYPE_LCD)) {
			lcd = disp_get_lcd(disp);
			if((lcd) && (lcd->set_manager))
				lcd->set_manager(lcd, mgr);
		} else {
			//other device
		}

		enhance = disp_get_enhance(disp);
		if(enhance && (enhance->set_manager)) {
			enhance->set_manager(enhance, mgr);
		}

		smbl = disp_get_smbl(disp);
		if(smbl && (smbl->set_manager)) {
			smbl->set_manager(smbl, mgr);
		}

		cptr = disp_get_capture(disp);
		if(cptr && (cptr->set_manager)) {
			cptr->set_manager(cptr, mgr);
		}
	}

	return 0;
}

/***********************************************************
 *
 * interrupt proc
 *
 ***********************************************************/
static s32 bsp_disp_cfg_get(u32 disp)
{
	return gdisp.screen[disp].cfg_cnt;
}

s32 bsp_disp_shadow_protect(u32 disp, bool protect)
{
	s32 ret = -1;
	u32 cnt = 0;
	u32 max_cnt = 50;
	u32 delay = 10;//us
	//disp_video_timings tt;
	struct disp_manager *mgr = NULL;
#ifdef __LINUX_PLAT__
	unsigned long flags;
#endif
	__inf("sel=%d, protect:%d,  cnt=%d\n", disp, protect, gdisp.screen[disp].cfg_cnt);

	mgr = disp_get_layer_manager(disp);
	if(mgr && mgr->device) {
		if(DISP_OUTPUT_TYPE_LCD == mgr->device->type) {
			//FIXME
		} else if(DISP_OUTPUT_TYPE_HDMI == mgr->device->type) {
			//FIXME
		}
	}

	if(protect) {
		while((0 != ret) && (cnt < max_cnt)) {
#ifdef __LINUX_PLAT__
			spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
			cnt ++;
			if(gdisp.screen[disp].have_cfg_reg == false) {
				gdisp.screen[disp].cfg_cnt++;
				ret = 0;
			}
#ifdef __LINUX_PLAT__
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
			if(0 != ret)
				disp_delay_us(delay);
		}

		if(0 != ret) {
			DE_INF("wait for reg load finish time out\n");
#if defined(__LINUX_PLAT__)
			spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
			gdisp.screen[disp].cfg_cnt++;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
		}
	} else {
#if defined(__LINUX_PLAT__)
			spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
			gdisp.screen[disp].cfg_cnt--;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
	}
	__inf("sel=%d, protect:%d,  cnt=%d\n", disp, protect, gdisp.screen[disp].cfg_cnt);
	return DIS_SUCCESS;
}

s32 bsp_disp_vsync_event_enable(u32 disp, bool enable)
{
	gdisp.screen[disp].vsync_event_en = enable;

	return DIS_SUCCESS;
}

static s32 disp_sync_all(u32 disp)
{
	struct disp_manager *mgr;

	mgr = disp_get_layer_manager(disp);
	if(!mgr) {
		DE_WRN("get mgr%d fail\n", disp);
	} else {
		if(mgr->sync)
			mgr->sync(mgr);
	}

	return 0;
}

void sync_event_proc(u32 disp)
{
	//u32 cur_line = 0, start_delay = 0;

#if defined(__LINUX_PLAT__)
	unsigned long flags;
#endif

	/*
	if(disp < 2) {
		//FIXME
		cur_line = disp_al_lcd_get_cur_line(disp);
		start_delay = disp_al_lcd_get_start_delay(disp);
		if(cur_line > start_delay-4) {
			return ;
		}
	}
	*/

#if defined(__LINUX_PLAT__)
	spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
	if(0 == bsp_disp_cfg_get(disp)) {
		gdisp.screen[disp].have_cfg_reg = true;
#if defined(__LINUX_PLAT__)
	spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
		disp_sync_all(disp);
		gdisp.screen[disp].have_cfg_reg = false;
		if(gdisp.init_para.disp_int_process)
			gdisp.init_para.disp_int_process(disp);

	} else {
#if defined(__LINUX_PLAT__)
	spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
	}

	return ;
}

s32 bsp_disp_set_print_level(u32 print_level)
{
	gdisp.print_level = print_level;

	return 0;
}

s32 bsp_disp_get_print_level(void)
{
	return gdisp.print_level;
}

s32 bsp_disp_get_screen_physical_width(u32 disp)
{
	s32 width = 0;
	//FIXME
	return width;
}

s32 bsp_disp_get_screen_physical_height(u32 disp)
{
	s32 height = 0;
	//FIXME

	return height;
}

s32 bsp_disp_get_screen_width(u32 disp)
{
	s32 width = 0;
	//FIXME
	return width;
}

s32 bsp_disp_get_screen_height(u32 disp)
{
	s32 height = 0;
	//FIXME

	return height;
}


s32 bsp_disp_get_screen_width_from_output_type(u32 disp, u32 output_type, u32 output_mode)
{
	u32 width = 800, height = 480;

	if(DISP_OUTPUT_TYPE_LCD == output_type) {
		struct disp_manager *mgr;
		mgr = disp_get_layer_manager(disp);
		if(mgr && mgr->device && mgr->device->get_resolution) {
			mgr->device->get_resolution(mgr->device, &width, &height);
		}
	} else if(DISP_OUTPUT_TYPE_HDMI == output_type) {
		switch(output_mode) {
		case DISP_TV_MOD_720P_50HZ:
		case DISP_TV_MOD_720P_60HZ:
			width = 1280;
			height = 720;
			break;
		case DISP_TV_MOD_1080P_50HZ:
		case DISP_TV_MOD_1080P_60HZ:
		case DISP_TV_MOD_1080P_30HZ:
		case DISP_TV_MOD_1080P_24HZ:
			width = 1920;
			height = 1080;
			break;
		case DISP_TV_MOD_3840_2160P_30HZ:
		case DISP_TV_MOD_3840_2160P_25HZ:
		case DISP_TV_MOD_3840_2160P_24HZ:
			width = 3840;
			height = 2160;
			break;
		}
	}
	/* FIXME: add other output device res */

	return width;
}

s32 bsp_disp_get_screen_height_from_output_type(u32 disp, u32 output_type, u32 output_mode)
{
	u32 width = 800, height = 480;

	if(DISP_OUTPUT_TYPE_LCD == output_type) {
		struct disp_manager *mgr;
		mgr = disp_get_layer_manager(disp);
		if(mgr && mgr->device && mgr->device->get_resolution) {
			mgr->device->get_resolution(mgr->device, &width, &height);
		}
	} else if(DISP_OUTPUT_TYPE_HDMI == output_type) {
		switch(output_mode) {
		case DISP_TV_MOD_720P_50HZ:
		case DISP_TV_MOD_720P_60HZ:
			width = 1280;
			height = 720;
			break;
		case DISP_TV_MOD_1080P_50HZ:
		case DISP_TV_MOD_1080P_60HZ:
		case DISP_TV_MOD_1080P_30HZ:
		case DISP_TV_MOD_1080P_24HZ:
			width = 1920;
			height = 1080;
			break;
		case DISP_TV_MOD_3840_2160P_30HZ:
		case DISP_TV_MOD_3840_2160P_25HZ:
		case DISP_TV_MOD_3840_2160P_24HZ:
			width = 3840;
			height = 2160;
			break;
		}
	}
	/* FIXME: add other output device res */

	return height;
}

s32 bsp_disp_lcd_register_panel(sunxi_lcd_panel *panel)
{
	u32 disp = 0;
	u32 num_screens = 0;
	s32 ret = 0, registered_cnt = 0;

	DE_INF("panel %s\n", panel->name);
	num_screens = bsp_disp_feat_get_num_screens();
	for(disp=0; disp<num_screens; disp++) {
		if(bsp_disp_feat_is_supported_output_types(disp, DISP_OUTPUT_TYPE_LCD)) {
			struct disp_device *dispdev;
			struct disp_lcd *lcd;

			dispdev = disp_get_lcd(disp);
			if(NULL == dispdev)
				continue;

			lcd = container_of(dispdev, struct disp_lcd, dispdev);
			if(NULL == lcd)
				continue;
			if((lcd) && (lcd->register_panel)) {
				ret = lcd->register_panel(lcd, panel);
				if(0 == ret) {
					gdisp.lcd_registered[disp] = 1;
					registered_cnt ++;
				}
			}
		}
	}

	if(0 != registered_cnt && gdisp.init_para.start_process)
		gdisp.init_para.start_process();

	return 0;
}

s32 bsp_disp_lcd_backlight_enable(u32 disp)
{
	s32 ret = -1;
	struct disp_device *dispdev;
	struct disp_lcd *lcd;

	dispdev = disp_get_lcd(disp);
	if(NULL == dispdev)
		return ret;

	lcd = container_of(dispdev, struct disp_lcd, dispdev);
	if(lcd && lcd->backlight_enable) {
		ret = lcd->backlight_enable(lcd);
	}

	return ret;
}

s32 bsp_disp_lcd_backlight_disable(u32 disp)
{
	s32 ret = -1;
	struct disp_device *dispdev;
	struct disp_lcd *lcd;

	dispdev = disp_get_lcd(disp);
	if(NULL == dispdev)
		return ret;

	lcd = container_of(dispdev, struct disp_lcd, dispdev);
	if(lcd && lcd->backlight_disable) {
		ret = lcd->backlight_disable(lcd);
	}

	return ret;
}

s32 bsp_disp_lcd_power_enable(u32 disp, u32 power_id)
{
	s32 ret = -1;
	struct disp_device *dispdev;
	struct disp_lcd *lcd;

	dispdev = disp_get_lcd(disp);
	if(NULL == dispdev)
		return ret;

	lcd = container_of(dispdev, struct disp_lcd, dispdev);
	if(lcd && lcd->power_enable) {
		ret = lcd->power_enable(lcd, power_id);
	}

	return ret;
}

s32 bsp_disp_lcd_power_disable(u32 disp, u32 power_id)
{
	s32 ret = -1;
	struct disp_device *dispdev;
	struct disp_lcd *lcd;

	dispdev = disp_get_lcd(disp);
	if(NULL == dispdev)
		return ret;

	lcd = container_of(dispdev, struct disp_lcd, dispdev);
	if(lcd && lcd->power_disable) {
		ret = lcd->power_disable(lcd, power_id);
	}

	return ret;
}

s32 bsp_disp_lcd_set_bright(u32 disp, u32 bright)
{
	s32 ret = -1;
	struct disp_device *dispdev;
	struct disp_lcd *lcd;

	dispdev = disp_get_lcd(disp);
	if(NULL == dispdev)
		return ret;

	lcd = container_of(dispdev, struct disp_lcd, dispdev);
	if(lcd && lcd->set_bright) {
		ret = lcd->set_bright(lcd, bright);
	}

	return ret;
}

s32 bsp_disp_lcd_get_bright(u32 disp)
{
	u32 bright = 0;
	struct disp_device *dispdev;
	struct disp_lcd *lcd;

	dispdev = disp_get_lcd(disp);
	if(NULL == dispdev)
		return bright;

	lcd = container_of(dispdev, struct disp_lcd, dispdev);
	if(lcd && lcd->get_bright)
		bright = lcd->get_bright(lcd);

	return bright;
}
