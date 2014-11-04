#ifndef __BSP_DISPLAY_H__
#define __BSP_DISPLAY_H__

#if defined(CONFIG_FPGA_V4_PLATFORM) || defined(CONFIG_FPGA_V7_PLATFORM) || defined(CONFIG_A67_FPGA)
#define __FPGA_DEBUG__
#endif

#define __LINUX_PLAT__

#if defined(__LINUX_PLAT__)
#include <linux/module.h>
#include "linux/kernel.h"
#include "linux/mm.h"
#include <asm/uaccess.h>
#include <asm/memory.h>
#include <asm/unistd.h>
#include "linux/semaphore.h"
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>
#include <linux/fb.h>
#include <linux/sched.h>   //wake_up_process()
#include <linux/kthread.h> //kthread_create()��kthread_run()
#include <linux/err.h> //IS_ERR()��PTR_ERR()
#include <linux/delay.h>
#include <linux/platform_device.h>
#include "asm-generic/int-ll64.h"
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm.h>
#include <mach/sys_config.h>
#include <mach/irqs.h>
#include <mach/platform.h>
#include <linux/ion_sunxi.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include <asm/div64.h>

#include <video/sunxi_display2.h>
#include "../disp_sys_intf.h"

#define DEFAULT_PRINT_LEVLE 0
#define __inf(msg...)       do{if(bsp_disp_get_print_level()){printk(KERN_WARNING "[DISP] %s,line:%d:",__func__,__LINE__);printk(msg);}}while(0)
#define __msg(msg...)       do{if(bsp_disp_get_print_level()){printk(KERN_WARNING "[DISP] %s,line:%d:",__func__,__LINE__);printk(msg);}}while(0)
#define __wrn(msg...)       do{{printk(KERN_WARNING "[DISP] %s,line:%d:",__func__,__LINE__);printk(msg);}}while(0)
#define __here__            do{if(bsp_disp_get_print_level()==2){printk(KERN_WARNING "[DISP] %s,line:%d\n",__func__,__LINE__);}}while(0)
#define __debug(msg...)     do{if(bsp_disp_get_print_level()==2){printk(KERN_WARNING "[DISP] %s,line:%d:",__func__,__LINE__);printk(msg);}}while(0)

#endif//end of define __LINUX_PLAT__

typedef enum
{
	DISP_MOD_DE           = 0,
	DISP_MOD_LCD0         = 1,
	DISP_MOD_LCD1         = 2,
	DISP_MOD_LCD2         = 3,
	DISP_MOD_DSI0         = 4,
	DISP_MOD_DSI1         = 5,
	DISP_MOD_DSI2         = 6,
	DISP_MOD_HDMI         = 7,
	DISP_MOD_NUM          = 8,
}disp_mod_id;

typedef struct
{
	u32 reg_base[DISP_MOD_NUM];
	u32 reg_size[DISP_MOD_NUM];
	u32 irq_no[DISP_MOD_NUM];

	s32 (*disp_int_process)(u32 sel);
	s32 (*vsync_event)(u32 sel);
	s32 (*start_process)(void);
	s32 (*capture_event)(u32 sel);
	s32 (*shadow_protect)(u32 sel, bool protect);
}disp_bsp_init_para;

typedef struct
{
	unsigned int   lcd_gamma_tbl[256];
	unsigned int   lcd_cmap_tbl[2][3][4];
	unsigned int   lcd_bright_curve_tbl[256];
}panel_extend_para;

typedef struct
{
	void (*cfg_panel_info)(panel_extend_para * info);
	int (*lcd_user_defined_func)(unsigned int sel, unsigned int para1, unsigned int para2, unsigned int para3);
}disp_lcd_panel_fun;

typedef struct lcd_function
{
	void (*func) (unsigned int sel);
	unsigned int delay;//ms
}disp_lcd_sequnces;

typedef struct{
	char name[32];
	disp_lcd_panel_fun func;
	disp_lcd_sequnces  *open_seq;
	u32                open_steps;
	disp_lcd_sequnces  *close_seq;
	u32                close_steps;
}sunxi_lcd_panel;

#define LCD_MAX_SEQUENCES 5
typedef struct lcd_flow
{
    disp_lcd_sequnces func[LCD_MAX_SEQUENCES];
    unsigned int func_num;
    unsigned int cur_step;
}disp_lcd_flow;

struct disp_manager;
struct disp_device;
struct disp_smbl;
struct disp_enhance;
struct disp_capture;

struct disp_device_ops {
	s32 (*init)(struct disp_device *dispdev);
	s32 (*exit)(struct disp_device *dispdev);

	s32 (*set_manager)(struct disp_device *dispdev, struct disp_manager *mgr);
	s32 (*unset_manager)(struct disp_device *dispdev);

	s32 (*enable)(struct disp_device *dispdev);
	s32 (*disable)(struct disp_device *dispdev);
	s32 (*is_enabled)(struct disp_device *dispdev);
	s32 (*is_used)(struct disp_device *dispdev);
	s32 (*get_resolution)(struct disp_device *dispdev, u32 *xres, u32 *yres);
	s32 (*get_dimensions)(struct disp_device *dispdev, u32 *width, u32 *height);
	s32 (*set_timings)(struct disp_device *dispdev, disp_video_timings *timings);
	s32 (*get_timings)(struct disp_device *dispdev, disp_video_timings *timings);
	s32 (*check_timings)(struct disp_device *dispdev, disp_video_timings *timings);

	disp_csc_type (*get_input_csc)(struct disp_device *dispdev);

	/* power manager */
	s32 (*early_suspend)(struct disp_device *dispdev);
	s32 (*late_resume)(struct disp_device *dispdev);
	s32 (*suspend)(struct disp_device *dispdev);
	s32 (*resume)(struct disp_device *dispdev);

	s32 (*dump)(struct disp_device *dispdev, char *buf);

	/* HDMI /TV */
	s32 (*set_mode)(struct disp_device *dispdev, u32 mode);
	s32 (*get_mode)(struct disp_device *dispdev);

	/* LCD */
	s32 (*set_bright)(struct disp_device *dispdev, u32 bright);
	s32 (*get_bright)(struct disp_device *dispdev);
};

struct disp_device {
	/* data fields */
	char *name;
	u32 disp;
	disp_output_type type;
	struct disp_manager *manager;
	struct disp_device_ops *ops; //the specified device ops
	disp_video_timings timings;

	/* function fileds  */
	/* init: script init && clock init && pwm init && register irq
	 * exit: clock exit && unregister irq
	 */
	s32 (*init)(struct disp_device *dispdev);
	s32 (*exit)(struct disp_device *dispdev);

	s32 (*set_manager)(struct disp_device *dispdev, struct disp_manager *mgr);
	s32 (*unset_manager)(struct disp_device *dispdev);

	s32 (*enable)(struct disp_device *dispdev);
	s32 (*disable)(struct disp_device *dispdev);
	s32 (*is_enabled)(struct disp_device *dispdev);
	s32 (*is_used)(struct disp_device *dispdev);
	s32 (*get_resolution)(struct disp_device *dispdev, u32 *xres, u32 *yres);
	s32 (*get_dimensions)(struct disp_device *dispdev, u32 *width, u32 *height);
	s32 (*set_timings)(struct disp_device *dispdev, disp_video_timings *timings);
	s32 (*get_timings)(struct disp_device *dispdev, disp_video_timings *timings);
	s32 (*check_timings)(struct disp_device *dispdev, disp_video_timings *timings);

	disp_csc_type (*get_input_csc)(struct disp_device *dispdev);

	/* power manager */
	s32 (*early_suspend)(struct disp_device *dispdev);
	s32 (*late_resume)(struct disp_device *dispdev);
	s32 (*suspend)(struct disp_device *dispdev);
	s32 (*resume)(struct disp_device *dispdev);

	s32 (*dump)(struct disp_device *dispdev, char *buf);

	/* HDMI /TV */
	s32 (*set_mode)(struct disp_device *dispdev, u32 mode);
	s32 (*get_mode)(struct disp_device *dispdev);

	/* LCD */
	s32 (*set_bright)(struct disp_device *dispdev, u32 bright);
	s32 (*get_bright)(struct disp_device *dispdev);
};

/* manager */
struct disp_manager {
	/* data fields */
	char name[25];
	u32 disp;
	u32 num_chns;
	u32 num_layers;
	struct disp_device *device;
	struct disp_smbl *smbl;
	struct disp_enhance *enhance;
	struct disp_capture *cptr;

	struct list_head lyr_list;

	/* function fields */
	s32 (*enable)(struct disp_manager *mgr);
	s32 (*disable)(struct disp_manager *mgr);
	s32 (*is_enabled)(struct disp_manager *mgr);

	/* init: clock init && reg init && register irq
	 * exit: clock exit && unregister irq
	 */
	s32 (*init)(struct disp_manager *mgr);
	s32 (*exit)(struct disp_manager *mgr);

	s32 (*set_back_color)(struct disp_manager *mgr,	disp_color *bk_color);
	s32 (*get_back_color)(struct disp_manager *mgr,	disp_color *bk_color);
	s32 (*set_color_key)(struct disp_manager *mgr, disp_colorkey *ck);
	s32 (*get_color_key)(struct disp_manager *mgr, disp_colorkey *ck);

	s32 (*get_screen_size)(struct disp_manager *mgr, u32 *width, u32 *height);
	s32 (*set_screen_size)(struct disp_manager *mgr, u32 width, u32 height);

	/* layer mamage */
	s32 (*check_layer_zorder)(struct disp_manager *mgr, disp_layer_config *config, u32 layer_num);
	s32 (*set_layer_config)(struct disp_manager *mgr, disp_layer_config *config, unsigned int layer_num);
	s32 (*get_layer_config)(struct disp_manager *mgr, disp_layer_config *config, unsigned int layer_num);
	s32 (*extend_layer_config)(struct disp_manager *mgr, disp_layer_config *info, unsigned int layer_num);

	s32 (*apply)(struct disp_manager *mgr);
	s32 (*force_apply)(struct disp_manager *mgr);
	s32 (*update_regs)(struct disp_manager *mgr);
	s32 (*sync)(struct disp_manager *mgr);
#if 0
	/* power manager */
	s32 (*early_suspend)(struct disp_manager *mgr);
	s32 (*late_resume)(struct disp_manager *mgr);
	s32 (*suspend)(struct disp_manager *mgr);
	s32 (*resume)(struct disp_manager *mgr);
#endif
	/* debug interface, dump manager info */
	s32 (*dump)(struct disp_manager *mgr, char *buf);
};

struct disp_lcd {
	struct disp_device dispdev;

	s32 (*backlight_enable)(struct disp_lcd *lcd);
	s32 (*backlight_disable)(struct disp_lcd *lcd);
#if 0
	s32 (*pwm_enable)(struct disp_lcd *lcd);
	s32 (*pwm_disable)(struct disp_lcd *lcd);
#endif
	s32 (*power_enable)(struct disp_lcd *lcd, u32 power_id);
	s32 (*power_disable)(struct disp_lcd *lcd, u32 power_id);
	s32 (*tcon_enable)(struct disp_lcd *lcd);
	s32 (*tcon_disable)(struct disp_lcd *lcd);
	s32 (*set_bright)(struct disp_lcd *lcd, u32 birhgt);
	s32 (*get_bright)(struct disp_lcd *lcd);
	s32 (*set_bright_dimming)(struct disp_lcd *lcd, u32 dimming);
	disp_lcd_flow *(*get_open_flow)(struct disp_lcd *lcd);
	disp_lcd_flow *(*get_close_flow)(struct disp_lcd *lcd);
	s32 (*enable)(struct disp_lcd *lcd);
	s32 (*disable)(struct disp_lcd *lcd);
	s32 (*pre_enable)(struct disp_lcd *lcd);
	s32 (*post_enable)(struct disp_lcd *lcd);
	s32 (*pre_disable)(struct disp_lcd *lcd);
	s32 (*post_disable)(struct disp_lcd *lcd);
	s32 (*set_panel_func)(struct disp_lcd *lcd, disp_lcd_panel_fun * lcd_cfg);
	s32 (*get_panel_driver_name)(struct disp_lcd *lcd, char *name);
	s32 (*pin_cfg)(struct disp_lcd *lcd, u32 bon);
	//s32 (*set_open_func)(struct disp_lcd* lcd, LCD_FUNC func, u32 delay);
	//s32 (*set_close_func)(struct disp_lcd* lcd, LCD_FUNC func, u32 delay);
	s32 (*set_gamma_tbl)(struct disp_lcd* lcd, u32 *tbl, u32 size);
	s32 (*enable_gamma)(struct disp_lcd* lcd);
	s32 (*disable_gamma)(struct disp_lcd* lcd);
	s32 (*register_panel)(struct disp_lcd* lcd, sunxi_lcd_panel *panel);
};

struct disp_smbl {
	/* static fields */
	char *name;
	u32 disp;
	u32 backlight;
	struct disp_manager *manager;

	/*
	 * The following functions do not block:
	 *
	 * is_enabled
	 * set_layer_info
	 * get_layer_info
	 *
	 * The rest of the functions may block and cannot be called from
	 * interrupt context
	 */

	s32 (*enable)(struct disp_smbl *smbl);
	s32 (*disable)(struct disp_smbl *smbl);
	bool (*is_enabled)(struct disp_smbl *smbl);
	s32 (*set_manager)(struct disp_smbl* smbl, struct disp_manager *mgr);
	s32 (*unset_manager)(struct disp_smbl* smbl);

	/* init: NULL
	 * exit: NULL
	 */
	s32 (*init)(struct disp_smbl *smbl);
	s32 (*exit)(struct disp_smbl *smbl);

	s32 (*apply)(struct disp_smbl *smbl);
	s32 (*update_regs)(struct disp_smbl *smbl);
	s32 (*force_apply)(struct disp_smbl *smbl);
	s32 (*sync)(struct disp_smbl *smbl);

	s32 (*set_window)(struct disp_smbl* smbl, disp_rect *window);
	s32 (*get_window)(struct disp_smbl* smbl, disp_rect *window);
};

struct disp_enhance {
	/* static fields */
	char *name;
	u32 disp;
	struct disp_manager *manager;

	/*
	 * The following functions do not block:
	 *
	 * is_enabled
	 * set_layer_info
	 * get_layer_info
	 *
	 * The rest of the functions may block and cannot be called from
	 * interrupt context
	 */

	s32 (*enable)(struct disp_enhance *enhance);
	s32 (*disable)(struct disp_enhance *enhance);
	bool (*is_enabled)(struct disp_enhance *enhance);
	s32 (*set_manager)(struct disp_enhance* enhance, struct disp_manager *mgr);
	s32 (*unset_manager)(struct disp_enhance* enhance);

	/* init: NULL
	 * exit: NULL
	 */
	s32 (*init)(struct disp_enhance *enhance);
	s32 (*exit)(struct disp_enhance *enhance);

	s32 (*apply)(struct disp_enhance *enhance);
	s32 (*update_regs)(struct disp_enhance *enhance);
	s32 (*force_apply)(struct disp_enhance *enhance);
	s32 (*sync)(struct disp_enhance *enhance);

	/* power manager */
	s32 (*early_suspend)(struct disp_enhance* enhance);
	s32 (*late_resume)(struct disp_enhance* enhance);
	s32 (*suspend)(struct disp_enhance* enhance);
	s32 (*resume)(struct disp_enhance* enhance);

	s32 (*set_bright)(struct disp_enhance* enhance, u32 val);
	s32 (*set_saturation)(struct disp_enhance* enhance, u32 val);
	s32 (*set_contrast)(struct disp_enhance* enhance, u32 val);
	s32 (*set_hue)(struct disp_enhance* enhance, u32 val);
	s32 (*set_mode)(struct disp_enhance* enhance, u32 val);
	s32 (*set_window)(struct disp_enhance* enhance, disp_rect *window);
	s32 (*get_bright)(struct disp_enhance* enhance);
	s32 (*get_saturation)(struct disp_enhance* enhance);
	s32 (*get_contrast)(struct disp_enhance* enhance);
	s32 (*get_hue)(struct disp_enhance* enhance);
	s32 (*get_mode)(struct disp_enhance* enhance);
	s32 (*get_window)(struct disp_enhance* enhance, disp_rect *window);
	//s32 (*set_para)(struct disp_enhance *enhance, disp_enhance_para *para);
};

struct disp_capture {
	char * name;
	u32 disp;
	struct disp_manager * manager;

	s32 (*set_manager)(struct disp_capture *cptr, struct disp_manager *mgr);
	s32 (*unset_manager)(struct disp_capture *cptr);
	s32 (*start)(struct disp_capture *cptr);
	s32 (*commmit)(struct disp_capture *cptr, disp_capture_info *info);
	s32 (*stop)(struct disp_capture *cptr);
	s32 (*sync)(struct disp_capture *cptr);
	s32 (*init)(struct disp_capture *cptr);
	s32 (*exit)(struct disp_capture *cptr);

	/* inner interface */
	s32 (*apply)(struct disp_capture *cptr);
};

typedef enum
{
	LCD_IF_HV			  = 0,
	LCD_IF_CPU			= 1,
	LCD_IF_LVDS			= 3,
	LCD_IF_DSI			= 4,
	LCD_IF_EDP      = 5,
	LCD_IF_EXT_DSI  = 6,
}disp_lcd_if;

typedef enum
{
	LCD_HV_IF_PRGB_1CYC		  = 0,  //parallel hv
	LCD_HV_IF_SRGB_3CYC		  = 8,  //serial hv
	LCD_HV_IF_DRGB_4CYC		  = 10, //Dummy RGB
	LCD_HV_IF_RGBD_4CYC		  = 11, //RGB Dummy
	LCD_HV_IF_CCIR656_2CYC	= 12,
}disp_lcd_hv_if;

typedef enum
{
	LCD_HV_SRGB_SEQ_RGB_RGB	= 0,
	LCD_HV_SRGB_SEQ_RGB_BRG	= 1,
	LCD_HV_SRGB_SEQ_RGB_GBR	= 2,
	LCD_HV_SRGB_SEQ_BRG_RGB	= 4,
	LCD_HV_SRGB_SEQ_BRG_BRG	= 5,
	LCD_HV_SRGB_SEQ_BRG_GBR	= 6,
	LCD_HV_SRGB_SEQ_GRB_RGB	= 8,
	LCD_HV_SRGB_SEQ_GRB_BRG	= 9,
	LCD_HV_SRGB_SEQ_GRB_GBR	= 10,
}disp_lcd_hv_srgb_seq;

typedef enum
{
	LCD_HV_SYUV_SEQ_YUYV	= 0,
	LCD_HV_SYUV_SEQ_YVYU	= 1,
	LCD_HV_SYUV_SEQ_UYUV	= 2,
	LCD_HV_SYUV_SEQ_VYUY	= 3,
}disp_lcd_hv_syuv_seq;

typedef enum
{
	LCD_HV_SYUV_FDLY_0LINE	= 0,
	LCD_HV_SRGB_FDLY_2LINE	= 1, //ccir ntsc
	LCD_HV_SRGB_FDLY_3LINE	= 2, //ccir pal
}disp_lcd_hv_syuv_fdly;

typedef enum
{
	LCD_CPU_IF_RGB666_18PIN = 0,
	LCD_CPU_IF_RGB666_9PIN  = 10,
	LCD_CPU_IF_RGB666_6PIN  = 12,
	LCD_CPU_IF_RGB565_16PIN = 8,
	LCD_CPU_IF_RGB565_8PIN  = 14,
}disp_lcd_cpu_if;

typedef enum
{
	LCD_TE_DISABLE	= 0,
	LCD_TE_RISING		= 1,
	LCD_TE_FALLING  = 2,
}disp_lcd_te;

typedef enum
{
	LCD_LVDS_IF_SINGLE_LINK		= 0,
	LCD_LVDS_IF_DUAL_LINK		  = 1,
}disp_lcd_lvds_if;

typedef enum
{
	LCD_LVDS_8bit		= 0,
	LCD_LVDS_6bit		= 1,
}disp_lcd_lvds_colordepth;

typedef enum
{
	LCD_LVDS_MODE_NS		  = 0,
	LCD_LVDS_MODE_JEIDA		= 1,
}disp_lcd_lvds_mode;

typedef enum
{
	LCD_DSI_IF_VIDEO_MODE	  = 0,
	LCD_DSI_IF_COMMAND_MODE	= 1,
	LCD_DSI_IF_BURST_MODE   = 2,
}disp_lcd_dsi_if;

typedef enum
{
	LCD_DSI_1LANE			= 1,
	LCD_DSI_2LANE			= 2,
	LCD_DSI_3LANE			= 3,
	LCD_DSI_4LANE			= 4,
}disp_lcd_dsi_lane;

typedef enum
{
	LCD_DSI_FORMAT_RGB888	  = 0,
	LCD_DSI_FORMAT_RGB666	  = 1,
	LCD_DSI_FORMAT_RGB666P	= 2,
	LCD_DSI_FORMAT_RGB565	  = 3,
}disp_lcd_dsi_format;


typedef enum
{
	LCD_FRM_BYPASS	= 0,
	LCD_FRM_RGB666	= 1,
	LCD_FRM_RGB565	= 2,
}disp_lcd_frm;

typedef enum
{
	LCD_CMAP_B0	= 0x0,
	LCD_CMAP_G0	= 0x1,
	LCD_CMAP_R0	= 0x2,
	LCD_CMAP_B1	= 0x4,
	LCD_CMAP_G1	= 0x5,
	LCD_CMAP_R1	= 0x6,
	LCD_CMAP_B2	= 0x8,
	LCD_CMAP_G2	= 0x9,
	LCD_CMAP_R2	= 0xa,
	LCD_CMAP_B3	= 0xc,
	LCD_CMAP_G3	= 0xd,
	LCD_CMAP_R3	= 0xe,
}disp_lcd_cmap_color;

typedef struct
{
	unsigned int lp_clk_div;
	unsigned int hs_prepare;
	unsigned int hs_trail;
	unsigned int clk_prepare;
	unsigned int clk_zero;
	unsigned int clk_pre;
	unsigned int clk_post;
	unsigned int clk_trail;
	unsigned int hs_dly_mode;
	unsigned int hs_dly;
	unsigned int lptx_ulps_exit;
	unsigned int hstx_ana0;
	unsigned int hstx_ana1;
}__disp_dsi_dphy_timing_t;

typedef struct
{
	disp_lcd_if              lcd_if;

	disp_lcd_hv_if           lcd_hv_if;
	disp_lcd_hv_srgb_seq     lcd_hv_srgb_seq;
	disp_lcd_hv_syuv_seq     lcd_hv_syuv_seq;
	disp_lcd_hv_syuv_fdly    lcd_hv_syuv_fdly;

	disp_lcd_lvds_if         lcd_lvds_if;
	disp_lcd_lvds_colordepth lcd_lvds_colordepth; //color depth, 0:8bit; 1:6bit
	disp_lcd_lvds_mode       lcd_lvds_mode;
	unsigned int             lcd_lvds_io_polarity;

	disp_lcd_cpu_if          lcd_cpu_if;
	disp_lcd_te              lcd_cpu_te;

	disp_lcd_dsi_if          lcd_dsi_if;
	disp_lcd_dsi_lane        lcd_dsi_lane;
	disp_lcd_dsi_format      lcd_dsi_format;
	unsigned int             lcd_dsi_eotp;
	unsigned int             lcd_dsi_vc;
	disp_lcd_te              lcd_dsi_te;

	unsigned int             lcd_dsi_dphy_timing_en;
	__disp_dsi_dphy_timing_t*	lcd_dsi_dphy_timing_p;

	unsigned int            lcd_edp_rate; //1(1.62G); 2(2.7G); 3(5.4G)
	unsigned int            lcd_edp_lane; //  1/2/4lane
	unsigned int            lcd_edp_colordepth; //color depth, 0:8bit; 1:6bit
	unsigned int            lcd_edp_fps;

	unsigned int            lcd_dclk_freq;
	unsigned int            lcd_x; //horizontal resolution
	unsigned int            lcd_y; //vertical resolution
	unsigned int            lcd_width; //width of lcd in mm
	unsigned int            lcd_height;//height of lcd in mm
	unsigned int            lcd_xtal_freq;

	unsigned int            lcd_pwm_used;
	unsigned int            lcd_pwm_ch;
	unsigned int            lcd_pwm_freq;
	unsigned int            lcd_pwm_pol;

	unsigned int            lcd_rb_swap;
	unsigned int            lcd_rgb_endian;

	unsigned int            lcd_vt;
	unsigned int            lcd_ht;
	unsigned int            lcd_vbp;
	unsigned int            lcd_hbp;
	unsigned int            lcd_vspw;
	unsigned int            lcd_hspw;

	unsigned int            lcd_hv_clk_phase;
	unsigned int            lcd_hv_sync_polarity;

	unsigned int            lcd_frm;
	unsigned int            lcd_gamma_en;
	unsigned int            lcd_cmap_en;
	unsigned int            lcd_bright_curve_en;
	panel_extend_para       lcd_extend_para;

	char                    lcd_size[8]; //e.g. 7.9, 9.7
	char                    lcd_model_name[32];

	unsigned int            tcon_index; //not need to config for user
	unsigned int            lcd_fresh_mode;//not need to config for user
	unsigned int            lcd_dclk_freq_original; //not need to config for user
}disp_panel_para;

struct sunxi_disp_source_ops
{
  int (*sunxi_lcd_delay_ms)(unsigned int ms);
  int (*sunxi_lcd_delay_us)(unsigned int us);
  int (*sunxi_lcd_tcon_enable)(unsigned int scree_id);
  int (*sunxi_lcd_tcon_disable)(unsigned int scree_id);
  int (*sunxi_lcd_cpu_write)(unsigned int scree_id, unsigned int command, unsigned int *para, unsigned int para_num);
  int (*sunxi_lcd_cpu_write_index)(unsigned int scree_id, unsigned int index);
  int (*sunxi_lcd_cpu_write_data)(unsigned int scree_id, unsigned int data);
  int (*sunxi_lcd_dsi_write)(unsigned int scree_id, unsigned char command, unsigned char *para, unsigned int para_num);
  int (*sunxi_lcd_dsi_clk_enable)(__u32 screen_id, __u32 en);
  int (*sunxi_lcd_backlight_enable)(unsigned int screen_id);
  int (*sunxi_lcd_backlight_disable)(unsigned int screen_id);
  int (*sunxi_lcd_power_enable)(unsigned int screen_id, unsigned int pwr_id);
  int (*sunxi_lcd_power_disable)(unsigned int screen_id, unsigned int pwr_id);
  int (*sunxi_lcd_register_panel)(sunxi_lcd_panel *panel);
  int (*sunxi_lcd_pin_cfg)(unsigned int screen_id, unsigned int bon);
  int (*sunxi_lcd_gpio_set_value)(unsigned int screen_id, unsigned int io_index, u32 value);
  int (*sunxi_lcd_gpio_set_direction)(unsigned int screen_id, unsigned int io_index, u32 direction);
};

s32 bsp_disp_init(disp_bsp_init_para * para);
s32 bsp_disp_exit(u32 mode);
s32 bsp_disp_open(void);
s32 bsp_disp_close(void);
s32 bsp_disp_get_print_level(void);
s32 bsp_disp_feat_get_num_screens(void);
s32 bsp_disp_feat_get_num_channels(u32 disp);
s32 bsp_disp_feat_get_num_layers(u32 screen_id);
s32 bsp_disp_feat_get_num_layers_by_chn(u32 disp, u32 chn);
bool bsp_disp_feat_is_supported_output_types(u32 screen_id, disp_output_type output_type);
s32 bsp_disp_get_screen_physical_width(u32 disp);
s32 bsp_disp_get_screen_physical_height(u32 disp);
s32 bsp_disp_get_screen_width(u32 disp);
s32 bsp_disp_get_screen_height(u32 disp);
s32 bsp_disp_get_screen_width_from_output_type(u32 disp, u32 output_type, u32 output_mode);
s32 bsp_disp_get_screen_height_from_output_type(u32 disp, u32 output_type, u32 output_mode);
//lcd
s32 bsp_disp_lcd_register_panel(sunxi_lcd_panel *panel);
s32 bsp_disp_lcd_backlight_enable(u32 disp);
s32 bsp_disp_lcd_backlight_disable(u32 disp);
s32 bsp_disp_lcd_power_enable(u32 disp, u32 power_id);
s32 bsp_disp_lcd_power_disable(u32 disp, u32 power_id);
s32 bsp_disp_lcd_set_bright(u32 disp, u32 bright);
s32 bsp_disp_lcd_get_bright(u32 disp);

s32 bsp_disp_vsync_event_enable(u32 disp, bool enable);
s32 bsp_disp_shadow_protect(u32 disp, bool protect);

s32 disp_delay_ms(u32 ms);
s32 disp_delay_us(u32 us);

extern struct disp_manager* disp_get_layer_manager(u32 disp);

#endif

