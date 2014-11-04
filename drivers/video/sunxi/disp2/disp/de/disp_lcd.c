#include "disp_lcd.h"

struct disp_lcd_private_data
{
	disp_lcd_flow             open_flow;
	disp_lcd_flow             close_flow;
	disp_panel_para           panel_info;
	__disp_lcd_cfg_t          lcd_cfg;
	disp_lcd_panel_fun        lcd_panel_fun;
	bool                      enabling;
	bool                      disabling;
	u32                       irq_no;
	u32                       reg_base;
	u32                       irq_no_dsi;
	u32                       reg_base_dsi;
	u32                       irq_no_edp;
	u32                       enabled;
	u32                       power_enabled;
	u32                       bl_enabled;
	struct {
		s32                     dev;
		u32                     channel;
		u32                     polarity;
		u32                     period_ns;
		u32                     duty_ns;
		u32                     enabled;
	}pwm_info;
	struct disp_clk_info           lcd_clk;
	struct disp_clk_info           dsi_clk;
	struct disp_clk_info           lvds_clk;
	struct disp_clk_info           edp_clk;
	struct disp_clk_info           extra_clk;
};
#if defined(__LINUX_PLAT__)
static spinlock_t lcd_data_lock;
#else
static int lcd_data_lock;
#endif

static struct disp_lcd *lcds = NULL;
static struct disp_lcd_private_data *lcd_private;

s32 disp_lcd_set_bright(struct disp_lcd *lcd, u32 bright);
s32 disp_lcd_get_bright(struct disp_lcd *lcd, u32 *bright);

struct disp_device* disp_get_lcd(u32 disp)
{
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();
	if(disp >= num_screens || !bsp_disp_feat_is_supported_output_types(disp, DISP_OUTPUT_TYPE_LCD)) {
		DE_WRN("disp %d not support lcd output\n", disp);
		return NULL;
	}

	return &lcds[disp].dispdev;
}
static struct disp_lcd_private_data *disp_lcd_get_priv(struct disp_lcd *lcd)
{
	if((NULL == lcd) || !bsp_disp_feat_is_supported_output_types(lcd->dispdev.disp, DISP_OUTPUT_TYPE_LCD)) {
		DE_WRN("disp %d not support lcd output\n", lcd->dispdev.disp);
		return NULL;
	}

	return &lcd_private[lcd->dispdev.disp];
}

static s32 disp_lcd_is_used(struct disp_device* dispdev)
{
	struct disp_lcd *lcd = container_of(dispdev, struct disp_lcd, dispdev);
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	s32 ret = 0;

	if((NULL == dispdev) || (NULL == lcd) || (NULL == lcdp)) {
		ret = 0;
	} else {
		if(bsp_disp_feat_is_supported_output_types(dispdev->disp, DISP_OUTPUT_TYPE_LCD))
			ret = (s32)lcdp->lcd_cfg.lcd_used;
	}
	return ret;
}

static s32 lcd_parse_panel_para(u32 disp, disp_panel_para * info)
{
    s32 ret = 0;
    char primary_key[25];
    s32 value = 0;

    sprintf(primary_key, "lcd%d_para", disp);
    memset(info, 0, sizeof(disp_panel_para));

    ret = disp_sys_script_get_item(primary_key, "lcd_x", &value, 1);
    if(ret == 1)
    {
        info->lcd_x = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_y", &value, 1);
    if(ret == 1)
    {
        info->lcd_y = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_width", &value, 1);
    if(ret == 1)
    {
        info->lcd_width = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_height", &value, 1);
    if(ret == 1)
    {
        info->lcd_height = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_dclk_freq", &value, 1);
    if(ret == 1)
    {
        info->lcd_dclk_freq = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_pwm_used", &value, 1);
    if(ret == 1)
    {
        info->lcd_pwm_used = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_pwm_ch", &value, 1);
    if(ret == 1)
    {
        info->lcd_pwm_ch = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_pwm_freq", &value, 1);
    if(ret == 1)
    {
        info->lcd_pwm_freq = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_pwm_pol", &value, 1);
    if(ret == 1)
    {
        info->lcd_pwm_pol = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_if", &value, 1);
    if(ret == 1)
    {
        info->lcd_if = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_hbp", &value, 1);
    if(ret == 1)
    {
        info->lcd_hbp = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_ht", &value, 1);
    if(ret == 1)
    {
        info->lcd_ht = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_vbp", &value, 1);
    if(ret == 1)
    {
        info->lcd_vbp = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_vt", &value, 1);
    if(ret == 1)
    {
        info->lcd_vt = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_hv_if", &value, 1);
    if(ret == 1)
    {
        info->lcd_hv_if = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_vspw", &value, 1);
    if(ret == 1)
    {
        info->lcd_vspw = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_hspw", &value, 1);
    if(ret == 1)
    {
        info->lcd_hspw = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_lvds_if", &value, 1);
    if(ret == 1)
    {
        info->lcd_lvds_if = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_lvds_mode", &value, 1);
    if(ret == 1)
    {
        info->lcd_lvds_mode = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_lvds_colordepth", &value, 1);
    if(ret == 1)
    {
        info->lcd_lvds_colordepth= value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_lvds_io_polarity", &value, 1);
    if(ret == 1)
    {
        info->lcd_lvds_io_polarity = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_cpu_if", &value, 1);
    if(ret == 1)
    {
        info->lcd_cpu_if = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_cpu_te", &value, 1);
    if(ret == 1)
    {
        info->lcd_cpu_te = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_frm", &value, 1);
    if(ret == 1)
    {
        info->lcd_frm = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_dsi_if", &value, 1);
    if(ret == 1)
    {
        info->lcd_dsi_if = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_dsi_lane", &value, 1);
    if(ret == 1)
    {
        info->lcd_dsi_lane = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_dsi_format", &value, 1);
    if(ret == 1)
    {
        info->lcd_dsi_format = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_dsi_eotp", &value, 1);
    if(ret == 1)
    {
        info->lcd_dsi_eotp = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_dsi_te", &value, 1);
    if(ret == 1)
    {
        info->lcd_dsi_te = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_edp_rate", &value, 1);
    if(ret == 1)
    {
        info->lcd_edp_rate = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_edp_lane", &value, 1);
    if(ret == 1)
    {
        info->lcd_edp_lane= value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_edp_colordepth", &value, 1);
    if(ret == 1)
    {
        info->lcd_edp_colordepth = value;
    }

	ret = disp_sys_script_get_item(primary_key, "lcd_edp_fps", &value, 1);
    if(ret == 1)
    {
        info->lcd_edp_fps = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_hv_clk_phase", &value, 1);
    if(ret == 1)
    {
        info->lcd_hv_clk_phase = value;
    }

	ret = disp_sys_script_get_item(primary_key, "lcd_hv_sync_polarity", &value, 1);
    if(ret == 1)
    {
        info->lcd_hv_sync_polarity = value;
    }
    ret = disp_sys_script_get_item(primary_key, "lcd_gamma_en", &value, 1);
    if(ret == 1)
    {
        info->lcd_gamma_en = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_cmap_en", &value, 1);
    if(ret == 1)
    {
        info->lcd_cmap_en = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_xtal_freq", &value, 1);
    if(ret == 1)
    {
        info->lcd_xtal_freq = value;
    }

    ret = disp_sys_script_get_item(primary_key, "lcd_size", (int*)info->lcd_size, 2);
    ret = disp_sys_script_get_item(primary_key, "lcd_model_name", (int*)info->lcd_model_name, 2);

    return 0;
}

#if 0
static void lcd_panel_parameter_check(u32 disp, struct disp_lcd* lcd)
{
	disp_panel_para* info;
	u32 cycle_num = 1;
	u32 Lcd_Panel_Err_Flag = 0;
	u32 Lcd_Panel_Wrn_Flag = 0;
	u32 Disp_Driver_Bug_Flag = 0;

	u32 lcd_fclk_frq;
	u32 lcd_clk_div;
	s32 ret = 0;

	char primary_key[20];
	s32 value = 0;

	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return ;
	}

	if(!disp_al_query_lcd_mod(lcd->dispdev.disp))
		return;

	sprintf(primary_key, "lcd%d_para", lcd->dispdev.disp);
	ret = disp_sys_script_get_item(primary_key, "lcd_used", &value, 1);

	if(ret != 0 ) {
		DE_WRN("get lcd%dpara lcd_used fail\n", lcd->dispdev.disp);
		return;
	} else {
		if(value != 1) {
			DE_WRN("lcd%dpara is not used\n", lcd->dispdev.disp);
			return;
		}
	}

	info = &(lcdp->panel_info);
	if(NULL == info) {
		DE_WRN("NULL hdl!\n");
		return;
	}

	if(info->lcd_if == 0 && info->lcd_hv_if == 8)
		cycle_num = 3;
	else if(info->lcd_if == 0 && info->lcd_hv_if == 10)
		cycle_num = 3;
	else if(info->lcd_if == 0 && info->lcd_hv_if == 11)
		cycle_num = 4;
	else if(info->lcd_if == 0 && info->lcd_hv_if == 12)
		cycle_num = 4;
	else if(info->lcd_if == 1 && info->lcd_cpu_if == 2)
		cycle_num = 3;
	else if(info->lcd_if == 1 && info->lcd_cpu_if == 4)
		cycle_num = 2;
	else if(info->lcd_if == 1 && info->lcd_cpu_if == 6)
		cycle_num = 2;
	else if(info->lcd_if == 1 && info->lcd_cpu_if == 10)
		cycle_num = 2;
	else if(info->lcd_if == 1 && info->lcd_cpu_if == 12)
		cycle_num = 3;
	else if(info->lcd_if == 1 && info->lcd_cpu_if == 14)
		cycle_num = 2;
	else
		cycle_num = 1;

	if(info->lcd_hbp > info->lcd_hspw)
	{
		;
	}
	else
	{
		Lcd_Panel_Err_Flag |= BIT0;
	}

	if(info->lcd_vbp > info->lcd_vspw)
	{
		;
	}
	else
	{
		Lcd_Panel_Err_Flag |= BIT1;
	}

	if(info->lcd_ht >= (info->lcd_hbp+info->lcd_x*cycle_num+4))
	{
		;
	}
	else
	{
		Lcd_Panel_Err_Flag |= BIT2;
	}

	if((info->lcd_vt) >= (info->lcd_vbp+info->lcd_y + 2))
	{
		;
	}
	else
	{
		Lcd_Panel_Err_Flag |= BIT3;
	}

	lcd_clk_div = disp_al_lcd_get_clk_div(disp);

	if(lcd_clk_div >= 6)
	{
		;
	}
	else if(lcd_clk_div >=2)
	{
		if((info->lcd_hv_clk_phase == 1) && (info->lcd_hv_clk_phase == 3))
		{
			Lcd_Panel_Err_Flag |= BIT10;
		}
	}
	else
	{
		Disp_Driver_Bug_Flag |= 1;
	}

	if((info->lcd_if == 1 && info->lcd_cpu_if == 0) ||	(info->lcd_if == 1 && info->lcd_cpu_if == 10)
		|| (info->lcd_if == 1 && info->lcd_cpu_if == 12) ||(info->lcd_if == 3 && info->lcd_lvds_colordepth == 1))
	{
		if(info->lcd_frm != 1)
			Lcd_Panel_Wrn_Flag |= BIT0;
	}
	else if(info->lcd_if == 1 && ((info->lcd_cpu_if == 2) || (info->lcd_cpu_if == 4) || (info->lcd_cpu_if == 6)
		|| (info->lcd_cpu_if == 8) || (info->lcd_cpu_if == 14)))
	{
		if(info->lcd_frm != 2)
			Lcd_Panel_Wrn_Flag |= BIT1;
	}

	lcd_fclk_frq = (info->lcd_dclk_freq * 1000 * 1000) / ((info->lcd_vt) * info->lcd_ht);
	if(lcd_fclk_frq < 50 || lcd_fclk_frq > 70)
	{
		Lcd_Panel_Wrn_Flag |= BIT2;
	}

	if(Lcd_Panel_Err_Flag != 0 || Lcd_Panel_Wrn_Flag != 0)
	{
		if(Lcd_Panel_Err_Flag != 0)
		{
			u32 i;
			for(i = 0; i < 200; i++)
			{
				//OSAL_PRINTF("*** Lcd in danger...\n");
			}
		}

		OSAL_PRINTF("*****************************************************************\n");
		OSAL_PRINTF("***\n");
		OSAL_PRINTF("*** LCD Panel Parameter Check\n");
		OSAL_PRINTF("***\n");
		OSAL_PRINTF("***             by guozhenjie\n");
		OSAL_PRINTF("***\n");
		OSAL_PRINTF("*****************************************************************\n");

		OSAL_PRINTF("*** \n");
		OSAL_PRINTF("*** Interface:");
		if(info->lcd_if == 0 && info->lcd_hv_if == 0)
			{OSAL_PRINTF("*** Parallel HV Panel\n");}
		else if(info->lcd_if == 0 && info->lcd_hv_if == 8)
			{OSAL_PRINTF("*** Serial HV Panel\n");}
		else if(info->lcd_if == 0 && info->lcd_hv_if == 10)
			{OSAL_PRINTF("*** Dummy RGB HV Panel\n");}
		else if(info->lcd_if == 0 && info->lcd_hv_if == 11)
			{OSAL_PRINTF("*** RGB Dummy HV Panel\n");}
		else if(info->lcd_if == 0 && info->lcd_hv_if == 12)
			{OSAL_PRINTF("*** Serial YUV Panel\n");}
		else if(info->lcd_if == 3 && info->lcd_lvds_colordepth== 0)
			{OSAL_PRINTF("*** 24Bit LVDS Panel\n");}
		else if(info->lcd_if == 3 && info->lcd_lvds_colordepth== 1)
			{OSAL_PRINTF("*** 18Bit LVDS Panel\n");}
		else if((info->lcd_if == 1) && (info->lcd_cpu_if == 0 || info->lcd_cpu_if == 10 || info->lcd_cpu_if == 12))
			{OSAL_PRINTF("*** 18Bit CPU Panel\n");}
		else if((info->lcd_if == 1) && (info->lcd_cpu_if == 2 || info->lcd_cpu_if == 4 ||
				info->lcd_cpu_if == 6 || info->lcd_cpu_if == 8 || info->lcd_cpu_if == 14))
			{OSAL_PRINTF("*** 16Bit CPU Panel\n");}
		else
		{
			OSAL_PRINTF("\n");
			OSAL_PRINTF("*** lcd_if:     %d\n",info->lcd_if);
			OSAL_PRINTF("*** lcd_hv_if:  %d\n",info->lcd_hv_if);
			OSAL_PRINTF("*** lcd_cpu_if: %d\n",info->lcd_cpu_if);
		}
		if(info->lcd_frm == 0)
			{OSAL_PRINTF("*** Lcd Frm Disable\n");}
		else if(info->lcd_frm == 1)
			{OSAL_PRINTF("*** Lcd Frm to RGB666\n");}
		else if(info->lcd_frm == 2)
			{OSAL_PRINTF("*** Lcd Frm to RGB565\n");}

		OSAL_PRINTF("*** \n");
		OSAL_PRINTF("*** Timing:\n");
		OSAL_PRINTF("*** lcd_x:      %d\n", info->lcd_x);
		OSAL_PRINTF("*** lcd_y:      %d\n", info->lcd_y);
		OSAL_PRINTF("*** lcd_ht:     %d\n", info->lcd_ht);
		OSAL_PRINTF("*** lcd_hbp:    %d\n", info->lcd_hbp);
		OSAL_PRINTF("*** lcd_vt:     %d\n", info->lcd_vt);
		OSAL_PRINTF("*** lcd_vbp:    %d\n", info->lcd_vbp);
		OSAL_PRINTF("*** lcd_hspw:   %d\n", info->lcd_hspw);
		OSAL_PRINTF("*** lcd_vspw:   %d\n", info->lcd_vspw);
		OSAL_PRINTF("*** lcd_frame_frq:  %dHz\n", lcd_fclk_frq);

		OSAL_PRINTF("*** \n");
		if(Lcd_Panel_Err_Flag & BIT0)
			{OSAL_PRINTF("*** Err01: Violate \"lcd_hbp > lcd_hspw\"\n");}
		if(Lcd_Panel_Err_Flag & BIT1)
			{OSAL_PRINTF("*** Err02: Violate \"lcd_vbp > lcd_vspw\"\n");}
		if(Lcd_Panel_Err_Flag & BIT2)
			{OSAL_PRINTF("*** Err03: Violate \"lcd_ht >= (lcd_hbp+lcd_x*%d+4)\"\n", cycle_num);}
		if(Lcd_Panel_Err_Flag & BIT3)
			{OSAL_PRINTF("*** Err04: Violate \"(lcd_vt) >= (lcd_vbp+lcd_y+2)\"\n");}
		if(Lcd_Panel_Err_Flag & BIT10)
			{OSAL_PRINTF("*** Err10: Violate \"lcd_hv_clk_phase\",use \"0\" or \"2\"");}
		if(Lcd_Panel_Wrn_Flag & BIT0)
			{OSAL_PRINTF("*** WRN01: Recommend \"lcd_frm = 1\"\n");}
		if(Lcd_Panel_Wrn_Flag & BIT1)
			{OSAL_PRINTF("*** WRN02: Recommend \"lcd_frm = 2\"\n");}
		if(Lcd_Panel_Wrn_Flag & BIT2)
			{OSAL_PRINTF("*** WRN03: Recommend \"lcd_dclk_frq = %d\"\n",
				((info->lcd_vt) * info->lcd_ht) * 60 / (1000 * 1000));}
		OSAL_PRINTF("*** \n");

		if(Lcd_Panel_Err_Flag != 0)
		{
			u32 image_base_addr;
			u32 reg_value = 0;

			image_base_addr = DE_Get_Reg_Base(disp);

			sys_put_wvalue(image_base_addr+0x804, 0xffff00ff);//set background color

			reg_value = sys_get_wvalue(image_base_addr + 0x800);
			sys_put_wvalue(image_base_addr+0x800, reg_value & 0xfffff0ff);//close all layer

			mdelay(2000);
			sys_put_wvalue(image_base_addr + 0x804, 0x00000000);//set background color
			sys_put_wvalue(image_base_addr + 0x800, reg_value);//open layer

			OSAL_PRINTF("*** Try new parameters,you can make it pass!\n");
		}
		OSAL_PRINTF("*** LCD Panel Parameter Check End\n");
		OSAL_PRINTF("*****************************************************************\n");
	}
}
#endif

static void lcd_get_sys_config(u32 disp, __disp_lcd_cfg_t *lcd_cfg)
{
    static char io_name[28][20] = {"lcdd0", "lcdd1", "lcdd2", "lcdd3", "lcdd4", "lcdd5", "lcdd6", "lcdd7", "lcdd8", "lcdd9", "lcdd10", "lcdd11",
                         "lcdd12", "lcdd13", "lcdd14", "lcdd15", "lcdd16", "lcdd17", "lcdd18", "lcdd19", "lcdd20", "lcdd21", "lcdd22",
                         "lcdd23", "lcdclk", "lcdde", "lcdhsync", "lcdvsync"};
    disp_gpio_set_t  *gpio_info;
    int  value = 1;
    char primary_key[20], sub_name[25];
    int i = 0;
    int  ret;

    sprintf(primary_key, "lcd%d_para", disp);

//lcd_used
    ret = disp_sys_script_get_item(primary_key, "lcd_used", &value, 1);
    if(ret == 1)
    {
        lcd_cfg->lcd_used = value;
    }

    if(lcd_cfg->lcd_used == 0) //no need to get lcd config if lcd_used eq 0
        return ;

//lcd_bl_en
    lcd_cfg->lcd_bl_en_used = 0;
    gpio_info = &(lcd_cfg->lcd_bl_en);
    ret = disp_sys_script_get_item(primary_key,"lcd_bl_en", (int *)gpio_info, sizeof(disp_gpio_set_t)/sizeof(int));
    if(ret == 3)
    {
        lcd_cfg->lcd_bl_en_used = 1;
    }

	sprintf(sub_name, "lcd_bl_regulator");
	ret = disp_sys_script_get_item(primary_key, sub_name, (int *)lcd_cfg->lcd_bl_regulator, 2);

//lcd_power0
	for(i=0; i<LCD_POWER_NUM; i++)
	{
		if(i==0)
			sprintf(sub_name, "lcd_power");
		else
			sprintf(sub_name, "lcd_power%d", i);
		lcd_cfg->lcd_power_type[i] = 0; /* invalid */
		ret = disp_sys_script_get_item(primary_key,sub_name, (int *)(lcd_cfg->lcd_regu[i]), 25);
		if(ret == 3) {
			/* gpio */
		  lcd_cfg->lcd_power_type[i] = 1; /* gpio */
		  memcpy(&(lcd_cfg->lcd_power[i]), lcd_cfg->lcd_regu[i], sizeof(disp_gpio_set_t));
		} else if(ret == 2) {
			/* str */
			lcd_cfg->lcd_power_type[i] = 2; /* regulator */
		}
	}

//lcd_gpio
    for(i=0; i<4; i++)
    {
        sprintf(sub_name, "lcd_gpio_%d", i);

        gpio_info = &(lcd_cfg->lcd_gpio[i]);
        ret = disp_sys_script_get_item(primary_key,sub_name, (int *)gpio_info, sizeof(disp_gpio_set_t)/sizeof(int));
        if(ret == 3)
        {
            lcd_cfg->lcd_gpio_used[i]= 1;
        }
    }

//lcd_gpio_scl,lcd_gpio_sda
    gpio_info = &(lcd_cfg->lcd_gpio[LCD_GPIO_SCL]);
    ret = disp_sys_script_get_item(primary_key,"lcd_gpio_scl", (int *)gpio_info, sizeof(disp_gpio_set_t)/sizeof(int));
    if(ret == 3)
    {
        lcd_cfg->lcd_gpio_used[LCD_GPIO_SCL]= 1;
    }
    gpio_info = &(lcd_cfg->lcd_gpio[LCD_GPIO_SDA]);
    ret = disp_sys_script_get_item(primary_key,"lcd_gpio_sda", (int *)gpio_info, sizeof(disp_gpio_set_t)/sizeof(int));
    if(ret == 3)
    {
        lcd_cfg->lcd_gpio_used[LCD_GPIO_SDA]= 1;
    }

	for(i = 0; i < LCD_GPIO_REGU_NUM; i++)
	{
		sprintf(sub_name, "lcd_gpio_regulator%d", i);

		ret = disp_sys_script_get_item(primary_key, sub_name, (int *)lcd_cfg->lcd_gpio_regulator[i], 2);
	}

//lcd io
    for(i=0; i<28; i++)
    {
        gpio_info = &(lcd_cfg->lcd_io[i]);
        ret = disp_sys_script_get_item(primary_key,io_name[i], (int *)gpio_info, sizeof(disp_gpio_set_t)/sizeof(int));
        if(ret == 3)
        {
            lcd_cfg->lcd_io_used[i]= 1;
        }
    }

	sprintf(sub_name, "lcd_io_regulator");
	ret = disp_sys_script_get_item(primary_key, sub_name, (int *)lcd_cfg->lcd_io_regulator, 2);

//backlight adjust
	for(i = 0; i < 101; i++) {
		sprintf(sub_name, "lcd_bl_%d_percent", i);
		lcd_cfg->backlight_curve_adjust[i] = 0;

		if(i == 100)
		lcd_cfg->backlight_curve_adjust[i] = 255;

		ret = disp_sys_script_get_item(primary_key, sub_name, &value, 1);
		if(ret == 1) {
			value = (value > 100)? 100:value;
			value = value * 255 / 100;
			lcd_cfg->backlight_curve_adjust[i] = value;
		}
	}


//init_bright
    sprintf(primary_key, "disp_init");
    sprintf(sub_name, "lcd%d_backlight", disp);

    ret = disp_sys_script_get_item(primary_key, sub_name, &value, 1);
    if(ret < 0)
    {
        lcd_cfg->backlight_bright = 197;
    }
    else
    {
        if(value > 256)
        {
            value = 256;
        }
        lcd_cfg->backlight_bright = value;
    }

//bright,constraction,saturation,hue
    sprintf(primary_key, "disp_init");
    sprintf(sub_name, "lcd%d_bright", disp);
    ret = disp_sys_script_get_item(primary_key, sub_name, &value, 1);
    if(ret < 0)
    {
        lcd_cfg->lcd_bright = 50;
    }
    else
    {
        if(value > 100)
        {
            value = 100;
        }
        lcd_cfg->lcd_bright = value;
    }

    sprintf(sub_name, "lcd%d_contrast", disp);
    ret = disp_sys_script_get_item(primary_key, sub_name, &value, 1);
    if(ret < 0)
    {
        lcd_cfg->lcd_contrast = 50;
    }
    else
    {
        if(value > 100)
        {
            value = 100;
        }
        lcd_cfg->lcd_contrast = value;
    }

    sprintf(sub_name, "lcd%d_saturation", disp);
    ret = disp_sys_script_get_item(primary_key, sub_name, &value, 1);
    if(ret < 0)
    {
        lcd_cfg->lcd_saturation = 50;
    }
    else
    {
        if(value > 100)
        {
            value = 100;
        }
        lcd_cfg->lcd_saturation = value;
    }

    sprintf(sub_name, "lcd%d_hue", disp);
    ret = disp_sys_script_get_item(primary_key, sub_name, &value, 1);
    if(ret < 0)
    {
        lcd_cfg->lcd_hue = 50;
    }
    else
    {
        if(value > 100)
        {
            value = 100;
        }
        lcd_cfg->lcd_hue = value;
    }
}

static s32 disp_lcd_pin_cfg(struct disp_lcd *lcd, u32 bon)
{
	int lcd_pin_hdl;
	int  i;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("lcd %d pin config, state %s, %d\n", lcd->dispdev.disp, (bon)? "on":"off", bon);

	//io-pad
	if(bon == 1) {
		if(!((!strcmp(lcdp->lcd_cfg.lcd_io_regulator, "")) || (!strcmp(lcdp->lcd_cfg.lcd_io_regulator, "none"))))
			disp_sys_power_enable(lcdp->lcd_cfg.lcd_io_regulator);
	}

	for(i=0; i<28; i++)	{
		if(lcdp->lcd_cfg.lcd_io_used[i]) {
			disp_gpio_set_t  gpio_info[1];

			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_io[i]), sizeof(disp_gpio_set_t));
			if(!bon) {
				gpio_info->mul_sel = 7;
			}	else {
				if((lcdp->panel_info.lcd_if == 3) && (gpio_info->mul_sel==2))	{
					gpio_info->mul_sel = 3;
				}
			}
			lcd_pin_hdl = disp_sys_gpio_request(gpio_info, 1);
			disp_sys_gpio_release(lcd_pin_hdl, 2);
		}
	}

	if(bon == 0) {
		if(!((!strcmp(lcdp->lcd_cfg.lcd_io_regulator, "")) || (!strcmp(lcdp->lcd_cfg.lcd_io_regulator, "none"))))
			disp_sys_power_disable(lcdp->lcd_cfg.lcd_io_regulator);
	}

	return DIS_SUCCESS;
}

s32 disp_lcd_pwm_enable(struct disp_lcd *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(disp_lcd_is_used(&lcd->dispdev) && lcdp->pwm_info.dev) {
		return disp_sys_pwm_enable(lcdp->pwm_info.dev);
	}
	DE_WRN("pwm device hdl is NULL\n");

	return DIS_FAIL;
}

s32 disp_lcd_pwm_disable(struct disp_lcd *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(disp_lcd_is_used(&lcd->dispdev) && lcdp->pwm_info.dev) {
		return disp_sys_pwm_disable(lcdp->pwm_info.dev);
	}
	DE_WRN("pwm device hdl is NULL\n");

	return DIS_FAIL;
}

s32 disp_lcd_backlight_enable(struct disp_lcd *lcd)
{
	disp_gpio_set_t  gpio_info[1];
	int hdl;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(disp_lcd_is_used(&lcd->dispdev) && (0 < lcdp->lcd_cfg.backlight_bright)) {
		disp_lcd_pwm_enable(lcd);
		if(lcdp->lcd_cfg.lcd_bl_en_used) {
			//io-pad
			if(!((!strcmp(lcdp->lcd_cfg.lcd_bl_regulator, "")) || (!strcmp(lcdp->lcd_cfg.lcd_bl_regulator, "none"))))
				disp_sys_power_enable(lcdp->lcd_cfg.lcd_bl_regulator);

			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_bl_en), sizeof(disp_gpio_set_t));

			hdl = disp_sys_gpio_request(gpio_info, 1);
			disp_sys_gpio_release(hdl, 2);
		}
	} else {
	}
	disp_sys_irqlock((void*)&lcd_data_lock, &flags);
	lcdp->bl_enabled = 1;
	disp_sys_irqunlock((void*)&lcd_data_lock, &flags);

	return 0;
}

s32 disp_lcd_backlight_disable(struct disp_lcd *lcd)
{
	disp_gpio_set_t  gpio_info[1];
	int hdl;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	unsigned long flags;

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(disp_lcd_is_used(&lcd->dispdev)) {
		if(lcdp->lcd_cfg.lcd_bl_en_used) {
			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_bl_en), sizeof(disp_gpio_set_t));
			gpio_info->data = (gpio_info->data==0)?1:0;
			gpio_info->mul_sel = 7;
			hdl = disp_sys_gpio_request(gpio_info, 1);
			disp_sys_gpio_release(hdl, 2);

			//io-pad
			if(!((!strcmp(lcdp->lcd_cfg.lcd_bl_regulator, "")) || (!strcmp(lcdp->lcd_cfg.lcd_bl_regulator, "none"))))
				disp_sys_power_disable(lcdp->lcd_cfg.lcd_bl_regulator);
		}
	}

	disp_lcd_pwm_disable(lcd);
	disp_sys_irqlock((void*)&lcd_data_lock, &flags);
	lcdp->bl_enabled = 0;
	disp_sys_irqunlock((void*)&lcd_data_lock, &flags);

	return 0;
}

s32 disp_lcd_power_enable(struct disp_lcd *lcd, u32 power_id)
{
	disp_gpio_set_t  gpio_info[1];
	int hdl;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(disp_lcd_is_used(&lcd->dispdev)) {
		if(lcdp->lcd_cfg.lcd_power_type[power_id] == 1) {
			/* gpio type */
			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_power[power_id]), sizeof(disp_gpio_set_t));

			hdl = disp_sys_gpio_request(gpio_info, 1);
			disp_sys_gpio_release(hdl, 2);
		} else if(lcdp->lcd_cfg.lcd_power_type[power_id] == 2) {
			/* regulator type */
			disp_sys_power_enable(lcdp->lcd_cfg.lcd_regu[power_id]);
		}
	}

	return 0;
}

s32 disp_lcd_power_disable(struct disp_lcd *lcd, u32 power_id)
{
	disp_gpio_set_t  gpio_info[1];
	int hdl;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(disp_lcd_is_used(&lcd->dispdev)) {
		if(lcdp->lcd_cfg.lcd_power_type[power_id] == 1) {
			memcpy(gpio_info, &(lcdp->lcd_cfg.lcd_power[power_id]), sizeof(disp_gpio_set_t));
			gpio_info->data = (gpio_info->data==0)?1:0;
			gpio_info->mul_sel = 7;
			hdl = disp_sys_gpio_request(gpio_info, 1);
			disp_sys_gpio_release(hdl, 2);
		} else if(lcdp->lcd_cfg.lcd_power_type[power_id] == 2) {
			/* regulator type */
			disp_sys_power_disable(lcdp->lcd_cfg.lcd_regu[power_id]);
		}
	}

	return 0;
}

s32 disp_lcd_bright_get_adjust_value(struct disp_lcd *lcd, u32 bright)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	bright = (bright > 255)? 255:bright;
	return lcdp->panel_info.lcd_extend_para.lcd_bright_curve_tbl[bright];
}

s32 disp_lcd_bright_curve_init(struct disp_lcd *lcd)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	u32 i = 0, j=0;
	u32 items = 0;
	u32 lcd_bright_curve_tbl[101][2];

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	for(i = 0; i < 101; i++) {
		if(lcdp->lcd_cfg.backlight_curve_adjust[i] == 0) {
			if(i == 0) {
				lcd_bright_curve_tbl[items][0] = 0;
				lcd_bright_curve_tbl[items][1] = 0;
				items++;
			}
		}	else {
			lcd_bright_curve_tbl[items][0] = 255 * i / 100;
			lcd_bright_curve_tbl[items][1] = lcdp->lcd_cfg.backlight_curve_adjust[i];
			items++;
		}
	}

	for(i=0; i<items-1; i++) {
		u32 num = lcd_bright_curve_tbl[i+1][0] - lcd_bright_curve_tbl[i][0];

		for(j=0; j<num; j++) {
			u32 value = 0;

			value = lcd_bright_curve_tbl[i][1] + ((lcd_bright_curve_tbl[i+1][1] - lcd_bright_curve_tbl[i][1]) * j)/num;
			lcdp->panel_info.lcd_extend_para.lcd_bright_curve_tbl[lcd_bright_curve_tbl[i][0] + j] = value;
		}
	}
	lcdp->panel_info.lcd_extend_para.lcd_bright_curve_tbl[255] = lcd_bright_curve_tbl[items-1][1];

	return 0;
}

s32 disp_lcd_set_bright(struct disp_lcd *lcd, u32 bright)
{
	u32 duty_ns;
	__u64 backlight_bright = bright;
	__u64 backlight_dimming;
	__u64 period_ns;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	bool need_enable_bl = false, need_disable_bl = false;

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if((0 == lcdp->lcd_cfg.backlight_bright) && (0 != bright))
		need_enable_bl = true;
	if((0 != lcdp->lcd_cfg.backlight_bright) && (0 == bright))
		need_disable_bl = true;

	backlight_bright = (backlight_bright > 255)? 255:backlight_bright;
	lcdp->lcd_cfg.backlight_bright = backlight_bright;

	if(lcdp->pwm_info.dev) {
		if(backlight_bright != 0)	{
			backlight_bright += 1;
		}
		backlight_bright = disp_lcd_bright_get_adjust_value(lcd, backlight_bright);

		lcdp->lcd_cfg.backlight_dimming = (0 == lcdp->lcd_cfg.backlight_dimming)? 256:lcdp->lcd_cfg.backlight_dimming;
		backlight_dimming = lcdp->lcd_cfg.backlight_dimming;
		period_ns = lcdp->pwm_info.period_ns;
		duty_ns = (backlight_bright * backlight_dimming *  period_ns/256 + 128) / 256;
		lcdp->pwm_info.duty_ns = duty_ns;

		disp_sys_pwm_config(lcdp->pwm_info.dev, duty_ns, period_ns);
	}
	if(need_enable_bl && (lcdp->enabled || lcdp->enabling) && lcdp->bl_enabled)
		disp_lcd_backlight_enable(lcd);
	if(need_disable_bl)
		disp_lcd_backlight_disable(lcd);

	return DIS_SUCCESS;
}

s32 disp_lcd_get_bright(struct disp_lcd *lcd, u32 *bright)
{
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	*bright = lcdp->lcd_cfg.backlight_bright;
	return DIS_SUCCESS;
}

//FIXME
extern void sync_event_proc(u32 disp);
#if defined(__LINUX_PLAT__)
s32 disp_lcd_event_proc(int irq, void *parg)
#else
static s32 disp_lcd_event_proc(void *parg)
#endif
{
	u32 disp = (u32)parg;
	//static u32 cntr=0;
	//FIXME
	writel(0x80000000, SUNXI_LCD0_VBASE + 0X04);

	sync_event_proc(disp);

	return DISP_IRQ_RETURN;
}

static s32 disp_lcd_enable(struct disp_device* dispdev)
{
	struct disp_lcd *lcd = container_of(dispdev, struct disp_lcd, dispdev);
	unsigned long flags;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);
	int i;

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("lcd %d\n", dispdev->disp);

#if defined(__FPGA_DEBUG__)
	//FIXME
	//********  for debug
	//clk
#if defined(CONFIG_ARCH_SUN9IW1)
#define SUNXI_CCM_VBASE SUNXI_CCM_MOD_VBASE
#endif

#if !defined(CONFIG_ARCH_SUN9IW1)
	writel(readl(SUNXI_CCM_VBASE) | 0x10, SUNXI_CCM_VBASE + 0x2c4);//lcd0 reset
	writel(readl(SUNXI_CCM_VBASE) | 0x1000, SUNXI_CCM_VBASE + 0x2c4);//de reset
#endif
	//gpio,pd28--pwm, ph0--pwr,pd29--bl_en
	writel(0x22222222, SUNXI_PIO_VBASE + 0x6c);
	writel(0x22222222, SUNXI_PIO_VBASE + 0x70);
	writel(0x22222222, SUNXI_PIO_VBASE + 0x74);
	writel(0x00122222, SUNXI_PIO_VBASE + 0x78);
	writel((readl(SUNXI_PIO_VBASE + 0xfc) & (~0x0000000f)) | 0x00000001, SUNXI_PIO_VBASE + 0xfc);
	writel((readl(SUNXI_PIO_VBASE + 0x10C) & (~0x0000000f)) | 0x00000001, SUNXI_PIO_VBASE + 0xfc);

	//lcd
	writel(0x000001f0, SUNXI_LCD0_VBASE + 0X40);//tcon0 ctl
	writel(0xf0000009, SUNXI_LCD0_VBASE + 0X44);//dclk
	writel(0x031f01df, SUNXI_LCD0_VBASE + 0X48);//x/y
	writel(0x041e002d, SUNXI_LCD0_VBASE + 0X4c);//ht/hbp
	writel(0x041a0016, SUNXI_LCD0_VBASE + 0X50);//vt/vbp
	writel(0x00000000, SUNXI_LCD0_VBASE + 0X54);//hspw/vspw
	writel(0x00000000, SUNXI_LCD0_VBASE + 0X58);//hv if

	writel(0x00000000, SUNXI_LCD0_VBASE + 0X8c);//tri en

	writel(0x800001f0, SUNXI_LCD0_VBASE + 0X40);//tcon0 ctl
	writel(0x80000000, SUNXI_LCD0_VBASE + 0X00);//tcon ctl

	writel(0x80000000, SUNXI_LCD0_VBASE + 0X04);//int
	//*****************
#endif

	disp_sys_irqlock((void*)&lcd_data_lock, &flags);
	lcdp->enabling = 1;
	disp_sys_irqunlock((void*)&lcd_data_lock, &flags);

	//FIXME
	for(i=0; i<lcdp->open_flow.func_num; i++) {
		if(lcdp->open_flow.func[i].func) {
			DE_INF("step %d, delay %d\n", i, lcdp->open_flow.func[i].delay);
			lcdp->open_flow.func[i].func(dispdev->disp);
			if(0 != lcdp->open_flow.func[i].delay)
				disp_delay_ms(lcdp->open_flow.func[i].delay);
		}
	}
	disp_sys_irqlock((void*)&lcd_data_lock, &flags);
	lcdp->enabled = 1;
	lcdp->enabling = 0;
	disp_sys_irqunlock((void*)&lcd_data_lock, &flags);

	return 0;
}

static s32 disp_lcd_disable(struct disp_device* dispdev)
{
	struct disp_lcd *lcd = container_of(dispdev, struct disp_lcd, dispdev);
	unsigned long flags;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("lcd %d\n", dispdev->disp);

#if defined(__FPGA_DEBUG__)
	//FIXME
	//********  for debug
	//clk
	writel(readl(SUNXI_CCM_VBASE) & (~0x10), SUNXI_CCM_VBASE + 0x2c4);//lcd0 reset
	writel(readl(SUNXI_CCM_VBASE) & (~0x1000), SUNXI_CCM_VBASE + 0x2c4);//de reset

	//gpio,pd28--pwm, ph0--pwr,pd29--bl_en
	writel(0x77777777, SUNXI_PIO_VBASE + 0x6c);
	writel(0x77777777, SUNXI_PIO_VBASE + 0x70);
	writel(0x77777777, SUNXI_PIO_VBASE + 0x74);
	writel(0x77727777, SUNXI_PIO_VBASE + 0x78);
	writel((readl(SUNXI_PIO_VBASE + 0xfc) & (~0x0000000f)) | 0x00000007, SUNXI_PIO_VBASE + 0xfc);

	//lcd
	writel(0x000001f0, SUNXI_LCD0_VBASE + 0X40);//tcon0 ctl
	writel(0xf0000009, SUNXI_LCD0_VBASE + 0X44);//dclk
	writel(0x031f01df, SUNXI_LCD0_VBASE + 0X48);//x/y
	writel(0x041e002d, SUNXI_LCD0_VBASE + 0X4c);//ht/hbp
	writel(0x041a0016, SUNXI_LCD0_VBASE + 0X50);//vt/vbp
	writel(0x00000000, SUNXI_LCD0_VBASE + 0X54);//hspw/vspw
	writel(0x00000000, SUNXI_LCD0_VBASE + 0X58);//hv if

	writel(0x000001f0, SUNXI_LCD0_VBASE + 0X40);//tcon0 ctl
	writel(0x000000000, SUNXI_LCD0_VBASE + 0X00);//tcon ctl

	writel(0x00000000, SUNXI_LCD0_VBASE + 0X04);//int
	//*****************
#endif
	disp_sys_irqlock((void*)&lcd_data_lock, &flags);
	lcdp->enabled = 0;
	disp_sys_irqunlock((void*)&lcd_data_lock, &flags);

	return 0;
}

static s32 disp_lcd_is_enabled(struct disp_device* dispdev)
{
	struct disp_lcd *lcd = container_of(dispdev, struct disp_lcd, dispdev);
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	return (s32)lcdp->enabled;
}

static s32 disp_lcd_register_panel(struct disp_lcd *lcd, sunxi_lcd_panel *panel)
{
	char primary_key[20], drv_name[32];
	s32 ret;
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	sprintf(primary_key, "lcd%d_para", lcd->dispdev.disp);

	ret = disp_sys_script_get_item(primary_key, "lcd_driver_name",  (int*)drv_name, 2);
	DE_INF("lcd %d, driver_name %s,  panel_name %s\n", lcd->dispdev.disp, drv_name, panel->name);
	if(!strcmp(drv_name, panel->name)) {
		u32 i;
		memset(&lcdp->lcd_panel_fun, 0, sizeof(disp_lcd_panel_fun));
		memset(&lcdp->open_flow, 0, sizeof(disp_lcd_flow));
		memset(&lcdp->close_flow, 0, sizeof(disp_lcd_flow));
		for(i=0; i<panel->open_steps && i<LCD_MAX_SEQUENCES; i++) {
			if(panel->open_seq) {
				lcdp->open_flow.func[i].func = panel->open_seq[i].func;
				lcdp->open_flow.func[i].delay = panel->open_seq[i].delay;
				lcdp->open_flow.func_num = i+1;
			}
		}
		for(i=0; i<panel->close_steps && i<LCD_MAX_SEQUENCES; i++) {
			if(panel->close_seq) {
				lcdp->close_flow.func[i].func = panel->close_seq[i].func;
				lcdp->close_flow.func[i].delay = panel->close_seq[i].delay;
				lcdp->close_flow.func_num = i+1;
			}
		}
		lcdp->lcd_panel_fun.cfg_panel_info = panel->func.cfg_panel_info;
		lcdp->lcd_panel_fun.lcd_user_defined_func = panel->func.lcd_user_defined_func;
	}
	return ret;
}

static s32 disp_lcd_init(struct disp_device* dispdev)
{
	struct disp_lcd *lcd = container_of(dispdev, struct disp_lcd, dispdev);
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == dispdev) || (NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("lcd %d\n", dispdev->disp);

	//FIXME, parse config, gpio init
	lcd_get_sys_config(dispdev->disp, &lcdp->lcd_cfg);
	if(disp_lcd_is_used(dispdev)) {
		disp_video_timings *timmings;
		disp_panel_para *panel_info;
		lcd_parse_panel_para(dispdev->disp, &lcdp->panel_info);
		timmings = &dispdev->timings;
		panel_info = &lcdp->panel_info;
		timmings->pixel_clk = panel_info->lcd_dclk_freq * 1000;
		timmings->x_res = panel_info->lcd_x;
		timmings->y_res = panel_info->lcd_y;
		timmings->hor_total_time= panel_info->lcd_ht;
		timmings->hor_sync_time= panel_info->lcd_hspw;
		timmings->hor_back_porch= panel_info->lcd_hbp-panel_info->lcd_hspw;
		timmings->hor_front_porch= panel_info->lcd_ht-panel_info->lcd_hbp - panel_info->lcd_x;
		timmings->ver_total_time= panel_info->lcd_vt;
		timmings->ver_sync_time= panel_info->lcd_vspw;
		timmings->ver_back_porch= panel_info->lcd_vbp-panel_info->lcd_vspw;
		timmings->ver_front_porch= panel_info->lcd_vt-panel_info->lcd_vbp -panel_info->lcd_y;
	}
	disp_lcd_bright_curve_init(lcd);

	if(disp_lcd_is_used(dispdev)) {
		__u64 backlight_bright;
		__u64 period_ns, duty_ns;
		if(lcdp->panel_info.lcd_pwm_used) {
			lcdp->pwm_info.channel = lcdp->panel_info.lcd_pwm_ch;
			lcdp->pwm_info.polarity = lcdp->panel_info.lcd_pwm_pol;
			lcdp->pwm_info.dev = disp_sys_pwm_request(lcdp->panel_info.lcd_pwm_ch);

			if(lcdp->panel_info.lcd_pwm_freq != 0) {
				period_ns = 1000*1000*1000 / lcdp->panel_info.lcd_pwm_freq;
			} else {
				DE_WRN("lcd%d.lcd_pwm_freq is ZERO\n", dispdev->disp);
				period_ns = 1000*1000*1000 / 1000;  //default 1khz
			}

			backlight_bright = lcdp->lcd_cfg.backlight_bright;

			duty_ns = (backlight_bright * period_ns) / 256;
			DE_DBG("[PWM]backlight_bright=%d,period_ns=%d,duty_ns=%d\n",(u32)backlight_bright,(u32)period_ns, (u32)duty_ns);
			disp_sys_pwm_set_polarity(lcdp->pwm_info.dev, lcdp->pwm_info.polarity);
			disp_sys_pwm_config(lcdp->pwm_info.dev, duty_ns, period_ns);
			lcdp->pwm_info.duty_ns = duty_ns;
			lcdp->pwm_info.period_ns = period_ns;
		}
		//FIXME, backlight disable
	}

	//lcd_panel_parameter_check(dispdev->disp, lcd);

//FIXME
  writel(0x0, SUNXI_LCD0_VBASE + 0X04);
	{
		disp_sys_register_irq(lcdp->irq_no,0,disp_lcd_event_proc,(void*)dispdev->disp,0,0);
#if !defined(__LINUX_PLAT__)
		disp_sys_enable_irq(lcdp->irq_no);
#endif
	}
	writel(0x80000000, SUNXI_LCD0_VBASE + 0X04);

	return 0;
}

static s32 disp_lcd_exit(struct disp_device* dispdev)
{
	struct disp_lcd *lcd = container_of(dispdev, struct disp_lcd, dispdev);
	struct disp_lcd_private_data *lcdp = disp_lcd_get_priv(lcd);

	if((NULL == dispdev) || (NULL == lcd) || (NULL == lcdp)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	/* FIXME
	if(!disp_al_query_lcd_mod(dispdev->disp)) {
		DE_WRN("lcd %d is not register\n", dispdev->disp);
		return DIS_FAIL;
	}
	*/

	disp_sys_disable_irq(lcdp->irq_no);
	disp_sys_unregister_irq(lcdp->irq_no, disp_lcd_event_proc,(void*)dispdev->disp);

	//FIXME, tcon & clk exit

	return 0;
}

static struct disp_device_ops lcd_ops = {
	.init = disp_lcd_init,
	.exit = disp_lcd_exit,
	.enable = disp_lcd_enable,
	.disable = disp_lcd_disable,
	.is_enabled = disp_lcd_is_enabled,
	.is_used = disp_lcd_is_used,
	//.get_input_csc = disp_lcd_get_input_csc,
	//.set_bright = disp_lcd_set_bright,
	//.get_bright = disp_lcd_get_bright,
};

s32 disp_init_lcd(disp_bsp_init_para * para)
{
	u32 num_screens;
	u32 disp;
	struct disp_lcd *lcd;
	struct disp_lcd_private_data *lcdp;

	DE_INF("disp_init_lcd\n");

#if defined(__LINUX_PLAT__)
	spin_lock_init(&lcd_data_lock);
#endif
	num_screens = bsp_disp_feat_get_num_screens();
	lcds = (struct disp_lcd *)disp_sys_malloc(sizeof(struct disp_lcd) * num_screens);
	if(NULL == lcds) {
		DE_WRN("malloc memory(%d bytes) fail!\n", sizeof(struct disp_lcd) * num_screens);
		return DIS_FAIL;
	}
	lcd_private = (struct disp_lcd_private_data *)disp_sys_malloc(sizeof(struct disp_lcd_private_data) * num_screens);
	if(NULL == lcd_private) {
		DE_WRN("malloc memory(%d bytes) fail!\n", sizeof(struct disp_lcd_private_data) * num_screens);
		return DIS_FAIL;
	}

	for(disp=0; disp<num_screens; disp++) {
		struct disp_device *dispdev = NULL;
		lcd = &lcds[disp];
		DE_INF("lcd %d, 0x%x\n", disp, (u32)lcd);
		lcdp = &lcd_private[disp];

		dispdev = disp_device_alloc();
		if(NULL == dispdev)
			continue;
		memcpy(&lcd->dispdev, dispdev, sizeof(struct disp_device));
		disp_device_release(dispdev);
		lcd->dispdev.ops = &lcd_ops;

		switch(disp) {
		case 0:
			lcd->dispdev.name = "lcd0";
			lcd->dispdev.disp = disp;
			lcd->dispdev.type = DISP_OUTPUT_TYPE_LCD;
			lcdp->irq_no = para->irq_no[DISP_MOD_LCD0 + disp];

			break;
		case 1:
			lcd->dispdev.name = "lcd1";
			lcd->dispdev.disp = disp;
			lcd->dispdev.type = DISP_OUTPUT_TYPE_LCD;
			lcdp->irq_no = para->irq_no[DISP_MOD_LCD0 + disp];

			break;
		case 2:
			lcd->dispdev.name = "lcd2";
			lcd->dispdev.disp = disp;
			lcd->dispdev.type = DISP_OUTPUT_TYPE_LCD;
			lcdp->irq_no = para->irq_no[DISP_MOD_LCD0 + disp];

			break;
		}
		DE_INF("lcd %d, reg_base=0x%x, irq_no=%d, reg_base_dsi=0x%x, irq_no_dsi=%d\n",
		    disp, lcdp->reg_base, lcdp->irq_no, lcdp->reg_base_dsi, lcdp->irq_no_dsi);

		lcd->register_panel = disp_lcd_register_panel;
		lcd->backlight_enable = disp_lcd_backlight_enable;
		lcd->backlight_disable = disp_lcd_backlight_disable;
		lcd->power_enable = disp_lcd_power_enable;
		lcd->power_disable = disp_lcd_power_disable;
		lcd->pin_cfg = disp_lcd_pin_cfg;
		lcd->dispdev.init(&lcd->dispdev);
	}

	return 0;
}

