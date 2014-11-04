/* linux/drivers/video/sunxi/disp2/disp/dev_fb.c
 *
 * Copyright (c) 2013 Allwinnertech Co., Ltd.
 * Author: Tyle <tyle@allwinnertech.com>
 *
 * Framebuffer driver for sunxi platform
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "dev_disp.h"

typedef struct
{
	struct device           *dev;
	disp_init_para          disp_init;

	bool                    fb_enable[FB_MAX];
	disp_fb_mode            fb_mode[FB_MAX];
	u32                     layer_hdl[FB_MAX][2];//channel, layer_id
	struct fb_info *        fbinfo[FB_MAX];
	disp_fb_create_info     fb_para[FB_MAX];
	wait_queue_head_t       wait[3];
	unsigned long           wait_count[3];
	struct work_struct      vsync_work[3];
	ktime_t                 vsync_timestamp[3];

	int                     blank[3];
}fb_info_t;

static fb_info_t g_fbi;

#define FBHANDTOID(handle)  ((handle) - 100)
#define FBIDTOHAND(ID)  ((ID) + 100)

struct __fb_addr_para
{
	int fb_paddr;
	int fb_size;
};
static struct __fb_addr_para g_fb_addr;

s32 sunxi_get_fb_addr_para(struct __fb_addr_para *fb_addr_para)
{
	if(fb_addr_para){
		fb_addr_para->fb_paddr = g_fb_addr.fb_paddr;
		fb_addr_para->fb_size  = g_fb_addr.fb_size;
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL(sunxi_get_fb_addr_para);

//              0:ARGB    1:BRGA    2:ABGR    3:RGBA
//seq           ARGB        BRGA       ARGB       BRGA
//br_swqp    0              0            1              1
s32 parser_disp_init_para(disp_init_para * init_para)
{
	int  value;
	int  i;

	memset(init_para, 0, sizeof(disp_init_para));

	if(disp_sys_script_get_item("disp_init", "disp_init_enable", &value, 1) < 0) {
		__wrn("fetch script data disp_init.disp_init_enable fail\n");
		return -1;
	}
	init_para->b_init = value;

	if(disp_sys_script_get_item("disp_init", "disp_mode", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.disp_mode fail\n");
		return -1;
	}
	init_para->disp_mode= value;

	//screen0
	if(disp_sys_script_get_item("disp_init", "screen0_output_type", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.screen0_output_type fail\n");
		return -1;
	}
	if(value == 0) {
		init_para->output_type[0] = DISP_OUTPUT_TYPE_NONE;
	}	else if(value == 1) {
		init_para->output_type[0] = DISP_OUTPUT_TYPE_LCD;
	}	else if(value == 2)	{
		init_para->output_type[0] = DISP_OUTPUT_TYPE_TV;
	}	else if(value == 3)	{
		init_para->output_type[0] = DISP_OUTPUT_TYPE_HDMI;
	}	else if(value == 4)	{
		init_para->output_type[0] = DISP_OUTPUT_TYPE_VGA;
	}	else {
		__wrn("invalid screen0_output_type %d\n", init_para->output_type[0]);
		return -1;
	}

	if(disp_sys_script_get_item("disp_init", "screen0_output_mode", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.screen0_output_mode fail\n");
		return -1;
	}
	if(init_para->output_type[0] == DISP_OUTPUT_TYPE_TV || init_para->output_type[0] == DISP_OUTPUT_TYPE_HDMI
	    || init_para->output_type[0] == DISP_OUTPUT_TYPE_VGA) {
		init_para->output_mode[0]= value;
	}

	//screen1
	if(disp_sys_script_get_item("disp_init", "screen1_output_type", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.screen1_output_type fail\n");
		return -1;
	}
	if(value == 0) {
		init_para->output_type[1] = DISP_OUTPUT_TYPE_NONE;
	}	else if(value == 1)	{
		init_para->output_type[1] = DISP_OUTPUT_TYPE_LCD;
	}	else if(value == 2)	{
		init_para->output_type[1] = DISP_OUTPUT_TYPE_TV;
	}	else if(value == 3)	{
		init_para->output_type[1] = DISP_OUTPUT_TYPE_HDMI;
	}	else if(value == 4)	{
		init_para->output_type[1] = DISP_OUTPUT_TYPE_VGA;
	}	else {
		__wrn("invalid screen1_output_type %d\n", init_para->output_type[1]);
		return -1;
	}

	if(disp_sys_script_get_item("disp_init", "screen1_output_mode", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.screen1_output_mode fail\n");
		return -1;
	}
	if(init_para->output_type[1] == DISP_OUTPUT_TYPE_TV || init_para->output_type[1] == DISP_OUTPUT_TYPE_HDMI
	    || init_para->output_type[1] == DISP_OUTPUT_TYPE_VGA) {
		init_para->output_mode[1]= value;
	}

	//screen2
	if(disp_sys_script_get_item("disp_init", "screen2_output_type", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.screen2_output_type fail\n");
	}
	if(value == 0) {
		init_para->output_type[2] = DISP_OUTPUT_TYPE_NONE;
	}	else if(value == 1) {
		init_para->output_type[2] = DISP_OUTPUT_TYPE_LCD;
	}	else if(value == 2)	{
		init_para->output_type[2] = DISP_OUTPUT_TYPE_TV;
	}	else if(value == 3)	{
		init_para->output_type[2] = DISP_OUTPUT_TYPE_HDMI;
	}	else if(value == 4)	{
		init_para->output_type[2] = DISP_OUTPUT_TYPE_VGA;
	}	else {
		__wrn("invalid screen0_output_type %d\n", init_para->output_type[2]);
	}

	if(disp_sys_script_get_item("disp_init", "screen2_output_mode", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.screen2_output_mode fail\n");
	}
	if(init_para->output_type[2] == DISP_OUTPUT_TYPE_TV || init_para->output_type[2] == DISP_OUTPUT_TYPE_HDMI
	    || init_para->output_type[2] == DISP_OUTPUT_TYPE_VGA) {
		init_para->output_mode[2]= value;
	}

	//fb0
	init_para->buffer_num[0]= 2;

	if(disp_sys_script_get_item("disp_init", "fb0_format", &value, 1) < 0) {
		__wrn("fetch script data disp_init.fb0_format fail\n");
		return -1;
	}
	init_para->format[0]= value;

	if(disp_sys_script_get_item("disp_init", "fb0_width", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.fb0_width fail\n");
		return -1;
	}
	init_para->fb_width[0]= value;

	if(disp_sys_script_get_item("disp_init", "fb0_height", &value, 1) < 0)	{
		__wrn("fetch script data disp_init.fb0_height fail\n");
		return -1;
	}
	init_para->fb_height[0]= value;

	//fb1
	init_para->buffer_num[1]= 2;

	if(disp_sys_script_get_item("disp_init", "fb1_format", &value, 1) < 0) {
		__wrn("fetch script data disp_init.fb1_format fail\n");
	}
	init_para->format[1]= value;

	if(disp_sys_script_get_item("disp_init", "fb1_width", &value, 1) < 0) {
		__wrn("fetch script data disp_init.fb1_width fail\n");
	}
	init_para->fb_width[1]= value;

	if(disp_sys_script_get_item("disp_init", "fb1_height", &value, 1) < 0) {
		__wrn("fetch script data disp_init.fb1_height fail\n");
	}
	init_para->fb_height[1]= value;

	//fb2
	init_para->buffer_num[2]= 2;

	if(disp_sys_script_get_item("disp_init", "fb2_format", &value, 1) < 0) {
		__wrn("fetch script data disp_init.fb2_format fail\n");
	}
	init_para->format[2]= value;

	if(disp_sys_script_get_item("disp_init", "fb2_width", &value, 1) < 0) {
		__wrn("fetch script data disp_init.fb2_width fail\n");
	}
	init_para->fb_width[2]= value;

	if(disp_sys_script_get_item("disp_init", "fb2_height", &value, 1) < 0) {
		__wrn("fetch script data disp_init.fb2_height fail\n");
	}
	init_para->fb_height[2]= value;

	__inf("====display init para begin====\n");
	__inf("b_init:%d\n", init_para->b_init);
	__inf("disp_mode:%d\n\n", init_para->disp_mode);
	for(i=0; i<3; i++) {
		__inf("output_type[%d]:%d\n", i, init_para->output_type[i]);
		__inf("output_mode[%d]:%d\n", i, init_para->output_mode[i]);
	}
	for(i=0; i<3; i++) {
		__inf("buffer_num[%d]:%d\n", i, init_para->buffer_num[i]);
		__inf("format[%d]:%d\n", i, init_para->format[i]);
		__inf("fb_width[%d]:%d\n", i, init_para->fb_width[i]);
		__inf("fb_height[%d]:%d\n", i, init_para->fb_height[i]);
	}
	__inf("====display init para end====\n");

	return 0;
}

#define sys_put_wvalue(addr, data) writel(data, (void __iomem*)addr)
s32 fb_draw_colorbar(u32 base, u32 width, u32 height, struct fb_var_screeninfo *var)
{
	u32 i=0, j=0;

	if(!base)
		return -1;

	for(i = 0; i<height; i++) {
		for(j = 0; j<width/4; j++) {
			u32 offset = 0;

			if(var->bits_per_pixel == 32)	{
					offset = width * i + j;
					sys_put_wvalue(base + offset*4, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->red.length)-1)<<var->red.offset));

					offset = width * i + j + width/4;
					sys_put_wvalue(base + offset*4, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->green.length)-1)<<var->green.offset));

					offset = width * i + j + width/4*2;
					sys_put_wvalue(base + offset*4, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->blue.length)-1)<<var->blue.offset));

					offset = width * i + j + width/4*3;
					sys_put_wvalue(base + offset*4, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->red.length)-1)<<var->red.offset) | (((1<<var->green.length)-1)<<var->green.offset));
				}
#if 0
				else if(var->bits_per_pixel == 16) {
					offset = width * i + j;
					sys_put_hvalue(base + offset*2, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->red.length)-1)<<var->red.offset));

					offset = width * i + j + width/4;
					sys_put_hvalue(base + offset*2, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->green.length)-1)<<var->green.offset));

					offset = width * i + j + width/4*2;
					sys_put_hvalue(base + offset*2, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->blue.length)-1)<<var->blue.offset));

					offset = width * i + j + width/4*3;
					sys_put_hvalue(base + offset*2, (((1<<var->transp.length)-1)<<var->transp.offset) | (((1<<var->red.length)-1)<<var->red.offset) | (((1<<var->green.length)-1)<<var->green.offset));
			}
#endif
		}
	}

	return 0;
}

s32 fb_draw_gray_pictures(u32 base, u32 width, u32 height, struct fb_var_screeninfo *var)
{
	u32 time = 0;

	for(time = 0; time<18; time++) {
		u32 i=0, j=0;

		for(i = 0; i<height; i++)	{
			for(j = 0; j<width; j++) {
				u32 addr = base + (i*width+ j)*4;
				u32 value = (0xff<<24) | ((time*15)<<16) | ((time*15)<<8) | (time*15);

				sys_put_wvalue(addr, value);
			}
		}
	}
	return 0;
}

static int Fb_map_video_memory(struct fb_info *info)
{
#ifndef FB_RESERVED_MEM
	unsigned map_size = PAGE_ALIGN(info->fix.smem_len);
	struct page *page;

	page = alloc_pages(GFP_KERNEL, get_order(map_size));
	if(page != NULL) {
		info->screen_base = page_address(page);
		info->fix.smem_start = virt_to_phys(info->screen_base);
		memset(info->screen_base,0x0,info->fix.smem_len);
		__inf("Fb_map_video_memory(alloc pages), pa=0x%08lx size:0x%x\n",info->fix.smem_start, info->fix.smem_len);
		return 0;
	}	else {
		__wrn("alloc_pages fail! size:0x%x\n", info->fix.smem_len);
		return -ENOMEM;
	}
#else
	info->screen_base = (char __iomem *)disp_malloc(info->fix.smem_len, (u32 *)(&info->fix.smem_start));
	if(info->screen_base)	{
		__inf("Fb_map_video_memory(reserve), pa=0x%x size:0x%x\n",(unsigned int)info->fix.smem_start, (unsigned int)info->fix.smem_len);
		memset((void* __force)info->screen_base,0x0,info->fix.smem_len);

		g_fb_addr.fb_paddr = (unsigned int)info->fix.smem_start;
		g_fb_addr.fb_size=info->fix.smem_len;

		return 0;
	} else {
		__wrn("disp_malloc fail!\n");
		return -ENOMEM;
	}

	return 0;
#endif
}


static inline void Fb_unmap_video_memory(struct fb_info *info)
{
#ifndef FB_RESERVED_MEM
	unsigned map_size = PAGE_ALIGN(info->fix.smem_len);

	free_pages((unsigned long)info->screen_base,get_order(map_size));
#else
	disp_free((void * __force)info->screen_base, (void*)info->fix.smem_start, info->fix.smem_len);
#endif
}

static s32 disp_fb_to_var(disp_pixel_format format, struct fb_var_screeninfo *var)
{
	switch(format) {
	case DISP_FORMAT_ARGB_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;
		break;
	case DISP_FORMAT_ABGR_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;
		break;
	case DISP_FORMAT_RGBA_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->blue.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		break;
	case DISP_FORMAT_BGRA_8888:
		var->bits_per_pixel = 32;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->red.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		break;
	case DISP_FORMAT_RGB_888:
		var->bits_per_pixel = 24;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGR_888:
		var->bits_per_pixel = 24;
		var->transp.length = 0;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_RGB_565:
		var->bits_per_pixel = 16;
		var->transp.length = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGR_565:
		var->bits_per_pixel = 16;
		var->transp.length = 0;
		var->red.length = 5;
		var->green.length = 6;
		var->blue.length = 5;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_ARGB_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 4;
		var->blue.length = 4;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;

		break;
	case DISP_FORMAT_ABGR_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 4;
		var->blue.length = 4;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;

		break;
	case DISP_FORMAT_RGBA_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 5;
		var->blue.length = 4;
		var->transp.offset = 0;
		var->blue.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGRA_4444:
		var->bits_per_pixel = 16;
		var->transp.length = 4;
		var->red.length = 4;
		var->green.length = 4;
		var->blue.length = 4;
		var->transp.offset = 0;
		var->red.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_ARGB_1555:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;

		break;
	case DISP_FORMAT_ABGR_1555:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;

		break;
	case DISP_FORMAT_RGBA_5551:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->blue.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;

		break;
	case DISP_FORMAT_BGRA_5551:
		var->bits_per_pixel = 16;
		var->transp.length = 1;
		var->red.length = 5;
		var->green.length = 5;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->red.offset = var->transp.offset + var->transp.length;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;

		break;
	default:
		__wrn("[FB]not support format %d\n", format);
	}

	__inf("disp_fb_to_var, format%d para: %dbpp, alpha(%d,%d),reg(%d,%d),green(%d,%d),blue(%d,%d)\n", (int)format, (int)var->bits_per_pixel,
	    (int)var->transp.offset, (int)var->transp.length, (int)var->red.offset, (int)var->red.length, (int)var->green.offset,
	    (int)var->green.length, (int)var->blue.offset, (int)var->blue.length);

	return 0;
}

static s32 var_to_disp_fb(disp_fb_info *fb, struct fb_var_screeninfo *var, struct fb_fix_screeninfo * fix)
{
	if(var->nonstd == 0)//argb
	{
		switch (var->bits_per_pixel)
		{
			case 32:
				if(var->red.offset == 16 && var->green.offset == 8 && var->blue.offset == 0)
					fb->format = DISP_FORMAT_ARGB_8888;
				else if(var->blue.offset == 24 && var->green.offset == 16 && var->red.offset == 8)
					fb->format = DISP_FORMAT_BGRA_8888;
				else if(var->blue.offset == 16 && var->green.offset == 8 && var->red.offset == 0)
					fb->format = DISP_FORMAT_ABGR_8888;
				else if(var->red.offset == 24 && var->green.offset == 16 && var->blue.offset == 8)
					fb->format = DISP_FORMAT_RGBA_8888;
				else
					__wrn("[FB]invalid argb format<transp.offset:%d,red.offset:%d,green.offset:%d,blue.offset:%d>\n",
							var->transp.offset,var->red.offset,var->green.offset,var->blue.offset);

				break;
			case 24:
				if (var->red.offset == 16 && var->green.offset == 8 && var->blue.offset == 0) { //rgb
					fb->format = DISP_FORMAT_RGB_888;
				} else if (var->blue.offset == 16 && var->green.offset == 8 && var->red.offset == 0) {//bgr
					fb->format = DISP_FORMAT_BGR_888;
				} else {
					__wrn("[FB]invalid format<transp.offset:%d,red.offset:%d,green.offset:%d,blue.offset:%d>\n",
							var->transp.offset,var->red.offset,var->green.offset,var->blue.offset);
				}

				break;
			case 16:
				if (var->red.offset == 11 && var->green.offset == 5 && var->blue.offset == 0) {
					fb->format = DISP_FORMAT_RGB_565;
				} else if (var->blue.offset == 11 && var->green.offset == 5 && var->red.offset == 0) {
					fb->format = DISP_FORMAT_BGR_565;
				} else if (var->transp.offset == 12 && var->red.offset == 8 &&
						var->green.offset == 4 && var->blue.offset == 0) {
					fb->format = DISP_FORMAT_ARGB_4444;
				} else if (var->transp.offset == 12 && var->blue.offset == 8 &&
						var->green.offset == 4 && var->red.offset == 0) {
					fb->format = DISP_FORMAT_ABGR_4444;
				} else if (var->red.offset == 12 && var->green.offset == 8 &&
						var->blue.offset == 4 && var->transp.offset == 0) {
					fb->format = DISP_FORMAT_RGBA_4444;
				} else if (var->blue.offset == 12 && var->green.offset == 8 &&
						var->red.offset == 4 && var->transp.offset == 0) {
					fb->format = DISP_FORMAT_BGRA_4444;
				} else if (var->transp.offset == 15 && var->red.offset == 10 &&
						var->green.offset == 5 && var->blue.offset == 0) {
					fb->format = DISP_FORMAT_ARGB_1555;
				} else if (var->transp.offset == 15 && var->blue.offset == 10 &&
						var->green.offset == 5 && var->red.offset == 0) {
					fb->format = DISP_FORMAT_ABGR_1555;
				} else if (var->red.offset == 11 && var->green.offset == 6 &&
						var->blue.offset == 1 && var->transp.offset == 0) {
					fb->format = DISP_FORMAT_RGBA_5551;
				} else if (var->blue.offset == 11 && var->green.offset == 6 &&
						var->red.offset == 1 && var->transp.offset == 0) {
					fb->format = DISP_FORMAT_BGRA_5551;
				} else {
					__wrn("[FB]invalid format<transp.offset:%d,red.offset:%d,green.offset:%d,blue.offset:%d>\n",
							var->transp.offset,var->red.offset,var->green.offset,var->blue.offset);
				}

				break;

			default:
				__wrn("invalid bits_per_pixel :%d\n", var->bits_per_pixel);
				return -EINVAL;
		}
	}
	__inf("var_to_disp_fb, format%d para: %dbpp, alpha(%d,%d),reg(%d,%d),green(%d,%d),blue(%d,%d)\n", (int)fb->format, (int)var->bits_per_pixel,
	    (int)var->transp.offset, (int)var->transp.length, (int)var->red.offset, (int)var->red.length, (int)var->green.offset,
	  (int)var->green.length, (int)var->blue.offset, (int)var->blue.length);

	fb->size[0].width = var->xres_virtual;
	fb->size[1].width = var->xres_virtual;
	fb->size[2].width = var->xres_virtual;

	fix->line_length = (var->xres_virtual * var->bits_per_pixel) / 8;

	return 0;
}


static int sunxi_fb_open(struct fb_info *info, int user)
{
	return 0;
}
static int sunxi_fb_release(struct fb_info *info, int user)
{
	return 0;
}


static int fb_wait_for_vsync(struct fb_info *info)
{
	unsigned long count;
	u32 sel = 0;
	int ret;
	int num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	for(sel = 0; sel < num_screens; sel++) {
		if(sel==g_fbi.fb_mode[info->node]) {
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			if(!mgr || !mgr->device || (NULL == mgr->device->is_enabled))
					return 0;

			if(0 == mgr->device->is_enabled(mgr->device))
				return 0;

			count = g_fbi.wait_count[sel];
			ret = wait_event_interruptible_timeout(g_fbi.wait[sel], count != g_fbi.wait_count[sel], msecs_to_jiffies(50));
			if (ret == 0)	{
				__inf("timeout\n");
				return -ETIMEDOUT;
			}
		}
	}

	return 0;
}

static int sunxi_fb_pan_display(struct fb_var_screeninfo *var,struct fb_info *info)
{
	u32 sel = 0;
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("fb %d, pos=%d,%d\n", info->node, var->xoffset, var->yoffset);

	for(sel = 0; sel < num_screens; sel++) {
		if(sel==g_fbi.fb_mode[info->node]) {
			u32 buffer_num = 1;
			u32 y_offset = 0;
			s32 chan = g_fbi.layer_hdl[info->node][0];
			s32 layer_id = g_fbi.layer_hdl[info->node][1];
			disp_layer_config config;
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			memset(&config, 0, sizeof(disp_layer_config));
			if(mgr && mgr->get_layer_config && mgr->set_layer_config) {
				config.channel = chan;
				config.layer_id = layer_id;
				if(0 != mgr->get_layer_config(mgr, &config, 1)) {
					__wrn("fb %d, get_layer_config(%d,%d,%d) fail\n", info->node, sel, chan, layer_id);
					return -1;
				}
				config.info.fb.crop.x = ((long long)var->xoffset) << 32;
				config.info.fb.crop.y = ((unsigned long long)(var->yoffset + y_offset)) << 32;;
				config.info.fb.crop.width = ((long long)var->xres) << 32;
				config.info.fb.crop.height = ((long long)(var->yres / buffer_num)) << 32;
				if(0 != mgr->set_layer_config(mgr, &config, 1)) {
					__wrn("fb %d, set_layer_config(%d,%d,%d) fail\n", info->node, sel, chan, layer_id);
					return -1;
				}
			}
		}
	}

	fb_wait_for_vsync(info);

	return 0;
}

static int sunxi_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	return 0;
}

static int sunxi_fb_set_par(struct fb_info *info)
{
	u32 sel = 0;
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("sunxi_fb_set_par\n");

	for(sel = 0; sel < num_screens; sel++) {
		if(sel==g_fbi.fb_mode[info->node]) {
			struct fb_var_screeninfo *var = &info->var;
			struct fb_fix_screeninfo * fix = &info->fix;
			u32 buffer_num = 1;
			u32 y_offset = 0;
			s32 chan = g_fbi.layer_hdl[info->node][0];
			s32 layer_id = g_fbi.layer_hdl[info->node][1];
			disp_layer_config config;
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			if(mgr && mgr->get_layer_config && mgr->set_layer_config) {
				config.channel = chan;
				config.layer_id = layer_id;
				mgr->get_layer_config(mgr, &config, 1);
			}

			var_to_disp_fb(&(config.info.fb), var, fix);
			config.info.fb.crop.x = var->xoffset;
			config.info.fb.crop.y = var->yoffset + y_offset;
			config.info.fb.crop.width = var->xres;
			config.info.fb.crop.height = var->yres / buffer_num;
			if(mgr && mgr->get_layer_config && mgr->set_layer_config)
				mgr->set_layer_config(mgr, &config, 1);
		}
	}
	return 0;
}

static int sunxi_fb_blank(int blank_mode, struct fb_info *info)
{
	u32 sel = 0;
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("sunxi_fb_blank,mode:%d\n",blank_mode);

	for(sel = 0; sel < num_screens; sel++) {
		if(sel == g_fbi.fb_mode[info->node]) {
			s32 chan = g_fbi.layer_hdl[info->node][0];
			s32 layer_id = g_fbi.layer_hdl[info->node][1];
			disp_layer_config config;
			struct disp_manager *mgr = g_disp_drv.mgr[sel];

			if (blank_mode == FB_BLANK_POWERDOWN)	{
				if(mgr && mgr->get_layer_config && mgr->set_layer_config) {
					config.channel = chan;
					config.layer_id = layer_id;
					mgr->get_layer_config(mgr, &config, 1);
					config.enable = 0;
					mgr->set_layer_config(mgr, &config, 1);
				}
			}	else {
				if(mgr && mgr->get_layer_config && mgr->set_layer_config) {
					config.channel = chan;
					config.layer_id = layer_id;
					mgr->get_layer_config(mgr, &config, 1);
					config.enable = 1;
					mgr->set_layer_config(mgr, &config, 1);
				}
			}
		}
	}
	return 0;
}

static int sunxi_fb_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	__inf("sunxi_fb_cursor\n");

	return 0;
}

s32 drv_disp_vsync_event(u32 sel)
{
	g_fbi.vsync_timestamp[sel] = ktime_get();

	schedule_work(&g_fbi.vsync_work[sel]);

	return 0;
}

static void send_vsync_work_0(struct work_struct *work)
{
	char buf[64];
	char *envp[2];

	snprintf(buf, sizeof(buf), "VSYNC0=%llu",ktime_to_ns(g_fbi.vsync_timestamp[0]));
	envp[0] = buf;
	envp[1] = NULL;
	kobject_uevent_env(&g_fbi.dev->kobj, KOBJ_CHANGE, envp);
}

static void send_vsync_work_1(struct work_struct *work)
{
	char buf[64];
	char *envp[2];

	snprintf(buf, sizeof(buf), "VSYNC1=%llu",ktime_to_ns(g_fbi.vsync_timestamp[1]));
	envp[0] = buf;
	envp[1] = NULL;
	kobject_uevent_env(&g_fbi.dev->kobj, KOBJ_CHANGE, envp);
}

static void send_vsync_work_2(struct work_struct *work)
{
	char buf[64];
	char *envp[2];

	snprintf(buf, sizeof(buf), "VSYNC2=%llu",ktime_to_ns(g_fbi.vsync_timestamp[2]));
	envp[0] = buf;
	envp[1] = NULL;
	kobject_uevent_env(&g_fbi.dev->kobj, KOBJ_CHANGE, envp);
}

void DRV_disp_int_process(u32 sel)
{
	g_fbi.wait_count[sel]++;
	wake_up_interruptible(&g_fbi.wait[sel]);

	return ;
}

static int sunxi_fb_ioctl(struct fb_info *info, unsigned int cmd,unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
#if 0
	case FBIOGET_VBLANK:
	{
		struct fb_vblank vblank;
		disp_video_timings tt;
		u32 line = 0;
		u32 sel;

		sel = (g_fbi.fb_mode[info->node] == FB_MODE_SCREEN1)?1:0;
		line = bsp_disp_get_cur_line(sel);
		bsp_disp_get_timming(sel, &tt);

		memset(&vblank, 0, sizeof(struct fb_vblank));
		vblank.flags |= FB_VBLANK_HAVE_VBLANK;
		vblank.flags |= FB_VBLANK_HAVE_VSYNC;
		if(line <= (tt.ver_total_time-tt.y_res))	{
			vblank.flags |= FB_VBLANK_VBLANKING;
		}
		if((line > tt.ver_front_porch) && (line < (tt.ver_front_porch+tt.ver_sync_time)))	{
			vblank.flags |= FB_VBLANK_VSYNCING;
		}

		if (copy_to_user((void __user *)arg, &vblank, sizeof(struct fb_vblank)))
		ret = -EFAULT;

		break;
	}
#endif

	case FBIO_WAITFORVSYNC:
	{
		//ret = fb_wait_for_vsync(info);
		break;
	}

	default:
		break;
	}
	return ret;
}

static struct fb_ops dispfb_ops =
{
	.owner		    = THIS_MODULE,
	.fb_open        = sunxi_fb_open,
	.fb_release     = sunxi_fb_release,
	.fb_pan_display	= sunxi_fb_pan_display,
	.fb_ioctl       = sunxi_fb_ioctl,
	.fb_check_var   = sunxi_fb_check_var,
	.fb_set_par     = sunxi_fb_set_par,
	.fb_blank       = sunxi_fb_blank,
	.fb_cursor      = sunxi_fb_cursor,
};

static s32 display_fb_request(u32 fb_id, disp_fb_create_info *fb_para)
{
	struct fb_info *info = NULL;
	disp_layer_config config;
	u32 sel;
	u32 xres, yres;
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("%s,fb_id:%d\n", __func__, fb_id);

	if(g_fbi.fb_enable[fb_id]) {
		__wrn("%s, fb%d is already requested!\n", __func__, fb_id);
		return -1;
	}
	info = g_fbi.fbinfo[fb_id];

	xres = fb_para->width;
	yres = fb_para->height;
	if((0 == xres) || (0 == yres) || (0 == info->var.bits_per_pixel)) {
		__wrn("invalid paras xres(%d), yres(%d) bpp(%d) \n", xres, yres, info->var.bits_per_pixel);
		return -1;
	}

	info->var.xoffset       = 0;
	info->var.yoffset       = 0;
	info->var.xres          = xres;
	info->var.yres          = yres;
	info->var.xres_virtual  = xres;
	info->fix.line_length   = (fb_para->width * info->var.bits_per_pixel) >> 3;
	info->fix.smem_len      = info->fix.line_length * fb_para->height * fb_para->buffer_num;
	if(0 != info->fix.line_length)
		info->var.yres_virtual  = info->fix.smem_len / info->fix.line_length;
	Fb_map_video_memory(info);

	for(sel = 0; sel < num_screens; sel++) {
		if(sel == fb_para->fb_mode)	{
			u32 y_offset = 0, src_width = xres, src_height = yres;
			disp_video_timings tt;
			struct disp_manager *mgr = NULL;
			mgr = g_disp_drv.mgr[sel];
			if(mgr && mgr->device && mgr->device->get_timings) {
				mgr->device->get_timings(mgr->device, &tt);
				if(0 != tt.pixel_clk)
					g_fbi.fbinfo[fb_id]->var.pixclock = 1000000000 / tt.pixel_clk;
				g_fbi.fbinfo[fb_id]->var.left_margin = tt.hor_back_porch;
				g_fbi.fbinfo[fb_id]->var.right_margin = tt.hor_front_porch;
				g_fbi.fbinfo[fb_id]->var.upper_margin = tt.ver_back_porch;
				g_fbi.fbinfo[fb_id]->var.lower_margin = tt.ver_front_porch;
				g_fbi.fbinfo[fb_id]->var.hsync_len = tt.hor_sync_time;
				g_fbi.fbinfo[fb_id]->var.vsync_len = tt.ver_sync_time;
			}
			info->var.width = bsp_disp_get_screen_physical_width(sel);
			info->var.height = bsp_disp_get_screen_physical_height(sel);

			memset(&config, 0, sizeof(disp_layer_config));
			/* fb bound to layer(0,0)  */
			config.channel = 0;
			config.layer_id = 0;
			config.enable = 1;
			config.info.screen_win.width = src_width;
			config.info.screen_win.height = src_height;
			config.info.screen_win.width = (0 == fb_para->output_width)? src_width:fb_para->output_width;
			config.info.screen_win.height = (0 == fb_para->output_height)? src_width:fb_para->output_height;

			config.info.mode = LAYER_MODE_BUFFER;
			config.info.alpha_mode = 1;
			config.info.alpha_value = 0xff;
			config.info.fb.crop.x = ((long long)0) << 32;
			config.info.fb.crop.y = ((long long)y_offset) << 32;
			config.info.fb.crop.width = ((long long)src_width) << 32;
			config.info.fb.crop.height = ((long long)src_height) << 32;
			config.info.screen_win.x = 0;
			config.info.screen_win.y = 0;
			var_to_disp_fb(&(config.info.fb), &(info->var), &(info->fix));
			config.info.fb.addr[0] = (u32)info->fix.smem_start;
			config.info.fb.addr[1] = 0;
			config.info.fb.addr[2] = 0;
			config.info.fb.align[0] = 32;
			config.info.fb.align[1] = 32;
			config.info.fb.align[2] = 32;
			config.info.fb.flags = DISP_BF_NORMAL;
			config.info.fb.scan = DISP_SCAN_PROGRESSIVE;
			config.info.fb.size[0].width = fb_para->width;
			config.info.fb.size[0].height = fb_para->height;
			config.info.fb.size[1].width = fb_para->width;
			config.info.fb.size[1].height = fb_para->height;
			config.info.fb.size[2].width = fb_para->width;
			config.info.fb.size[2].height = fb_para->height;
			config.info.fb.color_space = DISP_BT601;

			if(mgr && mgr->set_layer_config)
				mgr->set_layer_config(mgr, &config, 1);

			//TEST
			{
				disp_color cl = {0xff,0x00, 0xfe,0x00};
				if(mgr && mgr->set_back_color)
					mgr->set_back_color(mgr, &cl);
			}

			/* fb bound to layer(0,0)  */
			g_fbi.layer_hdl[fb_id][0] = 0;
			g_fbi.layer_hdl[fb_id][1] = 0;
		}
	}

	g_fbi.fb_enable[fb_id] = 1;
	g_fbi.fb_mode[fb_id] = fb_para->fb_mode;
	memcpy(&g_fbi.fb_para[fb_id], fb_para, sizeof(disp_fb_create_info));

	return 0;
}

static s32 display_fb_release(u32 fb_id)
{
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();

	__inf("%s, fb_id:%d\n", __func__, fb_id);

	if((fb_id >= 0) && g_fbi.fb_enable[fb_id]) {
		u32 sel = 0;
		struct fb_info * info = g_fbi.fbinfo[fb_id];

		for(sel = 0; sel < num_screens; sel++) {
			if(sel == g_fbi.fb_mode[fb_id])	{
				struct disp_manager *mgr = NULL;
				disp_layer_config config;

				mgr = g_disp_drv.mgr[sel];
				memset(&config, 0, sizeof(disp_layer_config));
				config.channel = g_fbi.layer_hdl[fb_id][0];
				config.layer_id = g_fbi.layer_hdl[fb_id][1];
				if(mgr && mgr->set_layer_config)
					mgr->set_layer_config(mgr, &config, 1);
			}
		}
		g_fbi.layer_hdl[fb_id][0] = 0;
		g_fbi.layer_hdl[fb_id][1] = 0;
		g_fbi.fb_mode[fb_id] = FB_MODE_SCREEN0;
		memset(&g_fbi.fb_para[fb_id], 0, sizeof(disp_fb_create_info));
		g_fbi.fb_enable[fb_id] = 0;

		Fb_unmap_video_memory(info);

		return 0;
	}	else {
		__wrn("invalid paras fb_id:%d in %s\n", fb_id, __func__);
		return -1;
	}
}

s32 Display_set_fb_timming(u32 sel)
{
	u8 fb_id=0;

	for(fb_id=0; fb_id<FB_MAX; fb_id++) {
		if(g_fbi.fb_enable[fb_id]) {
			if(sel==g_fbi.fb_mode[fb_id])	{
				disp_video_timings tt;
				struct disp_manager *mgr = g_disp_drv.mgr[sel];

				if(mgr && mgr->device && mgr->device->get_timings) {
					mgr->device->get_timings(mgr->device, &tt);
					if(0 != tt.pixel_clk)
						g_fbi.fbinfo[fb_id]->var.pixclock = 1000000000 / tt.pixel_clk;
					g_fbi.fbinfo[fb_id]->var.left_margin = tt.hor_back_porch;
					g_fbi.fbinfo[fb_id]->var.right_margin = tt.hor_front_porch;
					g_fbi.fbinfo[fb_id]->var.upper_margin = tt.ver_back_porch;
					g_fbi.fbinfo[fb_id]->var.lower_margin = tt.ver_front_porch;
					g_fbi.fbinfo[fb_id]->var.hsync_len = tt.hor_sync_time;
					g_fbi.fbinfo[fb_id]->var.vsync_len = tt.ver_sync_time;
				}
			}
		}
	}

	return 0;
}

s32 fb_init(struct platform_device *pdev)
{
	disp_fb_create_info fb_para;
	s32 i;
	u32 num_screens;

	g_fbi.dev = &pdev->dev;
	num_screens = bsp_disp_feat_get_num_screens();

	pr_info("[DISP] %s\n", __func__);

	INIT_WORK(&g_fbi.vsync_work[0], send_vsync_work_0);
	INIT_WORK(&g_fbi.vsync_work[1], send_vsync_work_1);
	INIT_WORK(&g_fbi.vsync_work[2], send_vsync_work_2);
	init_waitqueue_head(&g_fbi.wait[0]);
	init_waitqueue_head(&g_fbi.wait[1]);
	init_waitqueue_head(&g_fbi.wait[2]);
	disp_register_sync_finish_proc(DRV_disp_int_process);

	for(i=0; i<8; i++) {
		g_fbi.fbinfo[i] = framebuffer_alloc(0, g_fbi.dev);
		g_fbi.fbinfo[i]->fbops   = &dispfb_ops;
		g_fbi.fbinfo[i]->flags   = 0;
		g_fbi.fbinfo[i]->device  = g_fbi.dev;
		g_fbi.fbinfo[i]->par     = &g_fbi;
		g_fbi.fbinfo[i]->var.xoffset         = 0;
		g_fbi.fbinfo[i]->var.yoffset         = 0;
		g_fbi.fbinfo[i]->var.xres            = 800;
		g_fbi.fbinfo[i]->var.yres            = 480;
		g_fbi.fbinfo[i]->var.xres_virtual    = 800;
		g_fbi.fbinfo[i]->var.yres_virtual    = 480*2;
		g_fbi.fbinfo[i]->var.nonstd = 0;
		g_fbi.fbinfo[i]->var.bits_per_pixel = 32;
		g_fbi.fbinfo[i]->var.transp.length = 8;
		g_fbi.fbinfo[i]->var.red.length = 8;
		g_fbi.fbinfo[i]->var.green.length = 8;
		g_fbi.fbinfo[i]->var.blue.length = 8;
		g_fbi.fbinfo[i]->var.transp.offset = 24;
		g_fbi.fbinfo[i]->var.red.offset = 16;
		g_fbi.fbinfo[i]->var.green.offset = 8;
		g_fbi.fbinfo[i]->var.blue.offset = 0;
		g_fbi.fbinfo[i]->var.activate = FB_ACTIVATE_FORCE;
		g_fbi.fbinfo[i]->fix.type	    = FB_TYPE_PACKED_PIXELS;
		g_fbi.fbinfo[i]->fix.type_aux	= 0;
		g_fbi.fbinfo[i]->fix.visual 	= FB_VISUAL_TRUECOLOR;
		g_fbi.fbinfo[i]->fix.xpanstep	= 1;
		g_fbi.fbinfo[i]->fix.ypanstep	= 1;
		g_fbi.fbinfo[i]->fix.ywrapstep	= 0;
		g_fbi.fbinfo[i]->fix.accel	    = FB_ACCEL_NONE;
		g_fbi.fbinfo[i]->fix.line_length = g_fbi.fbinfo[i]->var.xres_virtual * 4;
		g_fbi.fbinfo[i]->fix.smem_len = g_fbi.fbinfo[i]->fix.line_length * g_fbi.fbinfo[i]->var.yres_virtual * 2;
		g_fbi.fbinfo[i]->screen_base = NULL;
		g_fbi.fbinfo[i]->fix.smem_start = 0x0;

		register_framebuffer(g_fbi.fbinfo[i]);
	}
	parser_disp_init_para(&(g_fbi.disp_init));

	if(g_fbi.disp_init.b_init) {
		u32 fb_num = 0;

		fb_num = 1;
		for(i = 0; i<fb_num; i++)	{
			u32 screen_id = g_fbi.disp_init.disp_mode;

			disp_fb_to_var(g_fbi.disp_init.format[i], &(g_fbi.fbinfo[i]->var));
			fb_para.buffer_num= g_fbi.disp_init.buffer_num[i];
			if((g_fbi.disp_init.fb_width[i] == 0) || (g_fbi.disp_init.fb_height[i] == 0))	{
				fb_para.width = bsp_disp_get_screen_width_from_output_type(screen_id,
				    g_fbi.disp_init.output_type[screen_id], g_fbi.disp_init.output_mode[screen_id]);
				fb_para.height = bsp_disp_get_screen_height_from_output_type(screen_id,
				    g_fbi.disp_init.output_type[screen_id], g_fbi.disp_init.output_mode[screen_id]);
			}	else {
				fb_para.width = g_fbi.disp_init.fb_width[i];
				fb_para.height = g_fbi.disp_init.fb_height[i];
			}
			fb_para.output_width = bsp_disp_get_screen_width_from_output_type(screen_id,
				    g_fbi.disp_init.output_type[screen_id], g_fbi.disp_init.output_mode[screen_id]);
			fb_para.output_height = bsp_disp_get_screen_height_from_output_type(screen_id,
				    g_fbi.disp_init.output_type[screen_id], g_fbi.disp_init.output_mode[screen_id]);
			if(g_fbi.disp_init.disp_mode == DISP_INIT_MODE_SCREEN0)	{
				fb_para.fb_mode = FB_MODE_SCREEN0;
			}	else if(g_fbi.disp_init.disp_mode == DISP_INIT_MODE_SCREEN1) {
				fb_para.fb_mode = FB_MODE_SCREEN1;
			}	 else if(g_fbi.disp_init.disp_mode == DISP_INIT_MODE_SCREEN2) {
				fb_para.fb_mode = FB_MODE_SCREEN2;
			}

			display_fb_request(i, &fb_para);
#if defined (__FPGA_DEBUG__)
			fb_draw_colorbar((u32 __force)g_fbi.fbinfo[i]->screen_base, fb_para.width, fb_para.height*fb_para.buffer_num, &(g_fbi.fbinfo[i]->var));
#endif
		}
	}

	return 0;
}

s32 fb_exit(void)
{
	u8 fb_id=0;

	for(fb_id=0; fb_id<FB_MAX; fb_id++) {
		if(g_fbi.fbinfo[fb_id] != NULL) {
			display_fb_release(FBIDTOHAND(fb_id));
		}
	}

	for(fb_id=0; fb_id<8; fb_id++) {
		unregister_framebuffer(g_fbi.fbinfo[fb_id]);
		framebuffer_release(g_fbi.fbinfo[fb_id]);
		g_fbi.fbinfo[fb_id] = NULL;
	}

	return 0;
}

