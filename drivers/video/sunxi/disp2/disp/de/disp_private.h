#ifndef _DISP_PRIVATE_H_
#define _DISP_PRIVATE_H_
#include "bsp_display.h"
#include "disp_features.h"

#if defined(__LINUX_PLAT__)
#define DE_INF __inf
#define DE_MSG __msg
#define DE_WRN __wrn
#define DE_DBG __debug
#define OSAL_IRQ_RETURN IRQ_HANDLED
#else
#define DE_INF(msg...)
#define DE_MSG __msg
#define DE_WRN __wrn
#define DE_DBG __debug
#ifndef OSAL_IRQ_RETURN
#define OSAL_IRQ_RETURN DIS_SUCCESS
#endif
#endif

typedef enum
{
	DIS_SUCCESS=0,
	DIS_FAIL=-1,
	DIS_PARA_FAILED=-2,
	DIS_PRIO_ERROR=-3,
	DIS_OBJ_NOT_INITED=-4,
	DIS_NOT_SUPPORT=-5,
	DIS_NO_RES=-6,
	DIS_OBJ_COLLISION=-7,
	DIS_DEV_NOT_INITED=-8,
	DIS_DEV_SRAM_COLLISION=-9,
	DIS_TASK_ERROR = -10,
	DIS_PRIO_COLLSION = -11
}disp_return_value;

/*basic data information definition*/
enum disp_layer_feat {
	DISP_LAYER_FEAT_GLOBAL_ALPHA        = 1 << 0,
	DISP_LAYER_FEAT_PIXEL_ALPHA         = 1 << 1,
	DISP_LAYER_FEAT_GLOBAL_PIXEL_ALPHA  = 1 << 2,
	DISP_LAYER_FEAT_PRE_MULT_ALPHA      = 1 << 3,
	DISP_LAYER_FEAT_COLOR_KEY           = 1 << 4,
	DISP_LAYER_FEAT_ZORDER              = 1 << 5,
	DISP_LAYER_FEAT_POS                 = 1 << 6,
	DISP_LAYER_FEAT_3D                  = 1 << 7,
	DISP_LAYER_FEAT_SCALE               = 1 << 8,
	DISP_LAYER_FEAT_DE_INTERLACE        = 1 << 9,
	DISP_LAYER_FEAT_COLOR_ENHANCE       = 1 << 10,
	DISP_LAYER_FEAT_DETAIL_ENHANCE      = 1 << 11,
};

typedef enum
{
	DISP_PIXEL_TYPE_RGB=0x0,
	DISP_PIXEL_TYPE_YUV=0x1,
}disp_pixel_type;

typedef enum
{
	LAYER_ATTR_DIRTY       = 0x00000001,
	LAYER_VI_FC_DIRTY      = 0x00000002,
	LAYER_HADDR_DIRTY      = 0x00000004,
	LAYER_SIZE_DIRTY       = 0x00000008,
	BLEND_ENABLE_DIRTY     = 0x00000010,
	BLEND_ATTR_DIRTY       = 0x00000020,
	BLEND_CTL_DIRTY        = 0x00000040,
	BLEND_OUT_DIRTY        = 0x00000080,
	LAYER_ALL_DIRTY        = 0x000000ff,
}disp_layer_dirty_flags;

typedef enum
{
	MANAGER_ENABLE_DIRTY     = 0x00000001,
	MANAGER_CK_DIRTY         = 0x00000002,
	MANAGER_BACK_COLOR_DIRTY = 0x00000004,
	MANAGER_SIZE_DIRTY       = 0x00000008,
	MANAGER_ALL_DIRTY        = 0x0000000f,
}disp_manager_dirty_flags;

struct disp_layer_config_data
{
	disp_layer_config config;
	disp_layer_dirty_flags flag;
};

struct disp_manager_info {
	disp_color back_color;
	disp_colorkey ck;
	disp_rectsz size;
	bool enable;
};

struct disp_manager_data
{
	struct disp_manager_info config;
	disp_manager_dirty_flags flag;
};

struct disp_clk_info
{
		u32                     clk;
		u32                     clk_div;
		u32                     h_clk;
		u32                     clk_src;
		u32                     clk_div2;

		u32                     clk_p;
		u32                     clk_div_p;
		u32                     h_clk_p;
		u32                     clk_src_p;

		u32                     ahb_clk;
		u32                     h_ahb_clk;
		u32                     dram_clk;
		u32                     h_dram_clk;

		bool                    enabled;
};

struct disp_enhance_info
{
	//basic adjust
	u32         bright;
	u32         contrast;
	u32         saturation;
	u32         hue;
	u32         mode;
	//ehnance
	u32         sharp;	//0-off; 1~3-on.
	u32         auto_contrast;	//0-off; 1~3-on.
	u32					auto_color;	//0-off; 1-on.
	u32         fancycolor_red; //0-Off; 1-2-on.
	u32         fancycolor_green;//0-Off; 1-2-on.
	u32         fancycolor_blue;//0-Off; 1-2-on.
	disp_rect   window;
	u32         enable;
	disp_rectsz size;
};

typedef enum
{
	ENHANCE_NONE_DIRTY   = 0x0,
	ENHANCE_ENABLE_DIRTY = 0x1 << 0,
	ENHANCE_BRIGHT_DIRTY = 0x1 << 1,
	ENHANCE_SHARP_DIRTY  = 0x1 << 2,
	ENHANCE_SIZE_DIRTY   = 0x1 << 3,
	ENHANCE_MODE_DIRTY   = 0X1 << 4,
	ENHANCE_ALL_DIRTY    = 0x7,
}disp_enhance_dirty_flags;

struct disp_enhance_config
{
	struct disp_enhance_info info;
	disp_enhance_dirty_flags flags;
};

typedef enum
{
	SMBL_DIRTY_NONE      = 0x00000000,
	SMBL_DIRTY_ENABLE    = 0x00000001,
	SMBL_DIRTY_WINDOW    = 0x00000002,
	SMBL_DIRTY_SIZE      = 0x00000004,
	SMBL_DIRTY_BL        = 0x00000008,
	SMBL_DIRTY_ALL       = 0x0000000F,
}disp_smbl_dirty_flags;

struct disp_smbl_info
{
	disp_rect                window;
	u32                      enable;
	disp_rectsz              size;
	u32                      backlight;
	u32                      backlight_dimming;
	disp_smbl_dirty_flags    flags;
};

extern struct disp_device* disp_get_lcd(u32 disp);

struct disp_hdmi {
	struct disp_device dispdev;

	s32 (*check_support_mode)(struct disp_hdmi* hdmi, u8 mode);
	s32 (*set_func)(struct disp_hdmi* hdmi, disp_hdmi_func* func);
	s32 (*set_video_info)(struct disp_hdmi* hdmi, disp_video_timings* video_info);
	s32 (*read_edid)(struct disp_hdmi* hdmi, u8 *buf, u32 len);
};

extern struct disp_hdmi* disp_get_hdmi(u32 disp);

extern struct disp_manager* disp_get_layer_manager(u32 disp);

struct disp_layer {
	/* data fields */
	char name[16];
	u32 disp;
	u32 chn;
	u32 id;

	enum disp_layer_feat caps;
	struct disp_manager *manager;
	struct list_head list;
	void* data;

	/* function fileds */

	s32 (*is_support_caps)(struct disp_layer* layer, enum disp_layer_feat caps);
	s32 (*is_support_format)(struct disp_layer* layer, disp_pixel_format fmt);
	s32 (*set_manager)(struct disp_layer* layer, struct disp_manager *mgr);
	s32 (*unset_manager)(struct disp_layer* layer);

	s32 (*check)(struct disp_layer* layer, disp_layer_config *config);
	s32 (*save_and_dirty_check)(struct disp_layer* layer, disp_layer_config *config);
	s32 (*get_config)(struct disp_layer* layer, disp_layer_config *config);
	s32 (*apply)(struct disp_layer* layer);
	s32 (*force_apply)(struct disp_layer* layer);
	s32 (*is_dirty)(struct disp_layer* layer);

	/* init: NULL
	 * exit: NULL
	 */
	s32 (*init)(struct disp_layer *layer);
	s32 (*exit)(struct disp_layer *layer);

	s32 (*get_frame_id)(struct disp_layer *layer);

	s32 (*dump)(struct disp_layer* layer, char *buf);
};

struct disp_csc_config
{
	u32 in_fmt;
	u32 in_mode;
	u32 out_fmt;
	u32 out_mode;
	u32 out_color_range;
	u32 brightness;
	u32 contrast;
	u32 saturation;
	u32 hue;
	//u32 alpha;
};

enum
{
	DE_RGB = 0,
	DE_YUV = 1,
};

typedef enum
{
  CAPTURE_DIRTY_ADDRESS   = 0x00000001,
  CAPTURE_DIRTY_WINDOW	  = 0x00000002,
  CAPTURE_DIRTY_SIZE	  = 0x00000004,
  CAPTURE_DIRTY_ALL 	  = 0x00000007,
}disp_capture_dirty_flags;

struct disp_capture_config
{
	disp_s_frame in_frame;   //only format/size/crop valid
	disp_s_frame out_frame;
	u32 disp;              //which disp channel to be capture
	disp_capture_dirty_flags flags;
};

extern struct disp_layer* disp_get_layer(u32 disp, u32 chn, u32 layer_id);
extern struct disp_layer* disp_get_layer_1(u32 disp, u32 layer_id);
extern struct disp_smbl* disp_get_smbl(u32 disp);
extern struct disp_enhance* disp_get_enhance(u32 disp);
extern struct disp_capture* disp_get_capture(u32 disp);

extern s32 disp_delay_ms(u32 ms);
extern s32 disp_delay_us(u32 us);
extern s32 disp_init_lcd(disp_bsp_init_para * para);
extern s32 disp_init_feat(void);
extern s32 disp_init_mgr(disp_bsp_init_para * para);
extern s32 disp_init_enhance(disp_bsp_init_para * para);
extern s32 disp_init_smbl(disp_bsp_init_para * para);
extern s32 disp_init_capture(disp_bsp_init_para *para);

//FIXME, add micro
#include "./lowlevel_sun8iw6/disp_al.h"
#include "disp_device.h"

u32 dump_layer_config(struct disp_layer_config_data *data);

#endif

