#ifndef __DEV_DISP_H__
#define __DEV_DISP_H__

#include "de/bsp_display.h"
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define FB_RESERVED_MEM

typedef enum
{
	DISPLAY_NORMAL = 0,
	DISPLAY_LIGHT_SLEEP = 1,
	DISPLAY_DEEP_SLEEP = 2,
}disp_standby_flags;

struct info_mm
{
	void *info_base;	/* Virtual address */
	unsigned long mem_start;	/* Start of frame buffer mem */
				/* (physical address) */
	u32 mem_len;			/* Length of frame buffer mem */
};

struct proc_list
{
	void (*proc)(u32 screen_id);
	struct list_head list;
};

struct ioctl_list
{
	unsigned int cmd;
	int (*func)(unsigned int cmd, unsigned long arg);
	struct list_head list;
};

struct standby_cb_list
{
	int (*suspend)(void);
	int (*resume)(void);
	struct list_head list;
};

typedef struct
{
	bool                  b_init;
	disp_init_mode        disp_mode;//0:single screen0(fb0); 1:single screen1(fb0); 2:single screen2(fb0)

	//for screen0/1/2
	disp_output_type      output_type[3];
	unsigned int          output_mode[3];

	//for fb0/1/2
	unsigned int          buffer_num[3];
	disp_pixel_format     format[3];
	unsigned int          fb_width[3];
	unsigned int          fb_height[3];
}disp_init_para;

//FIXME
#define DISP_NUMS_SCREEN 2
typedef struct
{
	struct device           *dev;
	u32                     reg_base[DISP_MOD_NUM];
	u32                     reg_size[DISP_MOD_NUM];
	u32                     irq_no[DISP_MOD_NUM];

	disp_init_para          disp_init;
	struct disp_manager     *mgr[DISP_NUMS_SCREEN];

	struct proc_list        sync_proc_list;
	struct proc_list        sync_finish_proc_list;
	struct ioctl_list       ioctl_extend_list;
	struct standby_cb_list  stb_cb_list;
	struct mutex            mlock;
	struct work_struct      resume_work[3];
	struct work_struct      start_work;

	u32    		              exit_mode;//0:clean all  1:disable interrupt
	bool			              b_lcd_enabled[3];
}disp_drv_info;

struct sunxi_disp_mod {
	disp_mod_id id;
	char name[32];
};

#define DISP_RESOURCE(res_name, res_start, res_end, res_flags) \
{\
	.start = (int __force)res_start, \
	.end = (int __force)res_end, \
	.flags = res_flags, \
	.name = #res_name \
},

int disp_open(struct inode *inode, struct file *file);
int disp_release(struct inode *inode, struct file *file);
ssize_t disp_read(struct file *file, char __user *buf, size_t count, loff_t *ppos);
ssize_t disp_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos);
int disp_mmap(struct file *file, struct vm_area_struct * vma);
long disp_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

s32 disp_create_heap(u32 pHeapHead, u32 pHeapHeadPhy, u32 nHeapSize);
void *disp_malloc(u32 num_bytes, u32 *phy_addr);
void  disp_free(void *virt_addr, void* phy_addr, u32 num_bytes);

extern s32 disp_register_sync_proc(void (*proc)(u32));
extern s32 disp_unregister_sync_proc(void (*proc)(u32));
extern s32 disp_register_sync_finish_proc(void (*proc)(u32));
extern s32 disp_unregister_sync_finish_proc(void (*proc)(u32));
extern s32 disp_register_ioctl_func(unsigned int cmd, int (*proc)(unsigned int cmd, unsigned long arg));
extern s32 disp_unregister_ioctl_func(unsigned int cmd);
extern s32 disp_register_standby_func(int (*suspend)(void), int (*resume)(void));
extern s32 disp_unregister_standby_func(int (*suspend)(void), int (*resume)(void));

extern disp_drv_info    g_disp_drv;

extern int sunxi_disp_get_source_ops(struct sunxi_disp_source_ops *src_ops);
extern s32 disp_lcd_open(u32 sel);
extern s32 disp_lcd_close(u32 sel);
extern s32 fb_init(struct platform_device *pdev);
extern s32 fb_exit(void);
extern int lcd_init(void);

#endif
