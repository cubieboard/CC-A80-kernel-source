#include "disp_al.h"
#include "de_hal.h"

int disp_al_layer_apply(unsigned int disp, struct disp_layer_config_data *data, unsigned int layer_num)
{
	return de_al_lyr_apply(disp, data, layer_num);
}

int disp_al_manager_apply(unsigned int disp, struct disp_manager_data *data)
{
	return de_al_mgr_apply(disp, data);
}

int disp_al_manager_sync(unsigned int disp)
{
	return de_al_mgr_sync(disp);
}

int disp_al_manager_update_regs(unsigned int disp)
{
	return de_al_mgr_update_regs(disp);
}

int disp_al_enhance_apply(unsigned int disp, struct disp_enhance_config *config)
{
	return de_enhance_apply(disp, config);
}

int disp_al_enhance_update_regs(unsigned int disp)
{
	return de_enhance_update_regs(disp);
}

int disp_al_enhance_sync(unsigned int disp)
{
	return de_enhance_sync(disp);
}

#if 1
s32 disp_al_capture_sync(u32 disp)
{
	static u32 count = 0;
		if(count < 10) {
			count ++;
			__inf("disp %d\n", disp);
		}
	WB_EBIOS_Update_Regs(disp);
	WB_EBIOS_Writeback_Enable(disp, 1);
	return 0;
}

s32 disp_al_capture_apply(unsigned int disp, struct disp_capture_config *cfg)
{
	static u32 count = 0;
		if(count < 10) {
			count ++;
			__inf("disp %d\n", disp);
		}
		__inf("disp%d, in_fmt=%d, in_stride<%d,%d,%d>, window<%d,%d,%d,%d>, out_fmt=%d, out_stride<%d,%d,%d>,crop<%d,%d,%d,%d>, addr<0x%llx,0x%llx,0x%llx>\n",
			disp, cfg->in_frame.format, cfg->in_frame.size[0].width, cfg->in_frame.size[1].width, cfg->in_frame.size[2].width,
			cfg->in_frame.crop.x, cfg->in_frame.crop.y, cfg->in_frame.crop.width, cfg->in_frame.crop.height,
			cfg->out_frame.format,  cfg->out_frame.size[0].width, cfg->out_frame.size[1].width, cfg->out_frame.size[2].width,
			cfg->out_frame.crop.x, cfg->out_frame.crop.y, cfg->out_frame.crop.width,
			cfg->out_frame.crop.height, cfg->out_frame.addr[0], cfg->out_frame.addr[1], cfg->out_frame.addr[2]);
	return WB_EBIOS_Apply(disp, cfg);
}

s32 disp_al_capture_get_status(unsigned int disp)
{
	__inf("disp %d\n", disp);
	return WB_EBIOS_Get_Status(disp);
}

#endif
s32 disp_al_smbl_apply(unsigned int disp, struct disp_smbl_info *info)
{
	static u32 count = 0;
		if(count < 10) {
			count ++;
			__inf("disp %d\n", disp);
		}
	return de_smbl_apply(disp, info);
}

s32 disp_al_smbl_update_regs(unsigned int disp)
{
	static u32 count = 0;
		if(count < 10) {
			count ++;
			__inf("disp %d\n", disp);
		}
	return de_smbl_update_regs(disp);
}

s32 disp_al_smbl_sync(unsigned int disp)
{
	static u32 count = 0;
		if(count < 10) {
			count ++;
			__inf("disp %d\n", disp);
		}
	return de_smbl_sync(disp);
}

s32 disp_al_smbl_get_status(unsigned int disp)
{
	static u32 count = 0;
		if(count < 10) {
			count ++;
			__inf("disp %d\n", disp);
		}

	return de_smbl_get_status(disp);
}

int disp_init_al(disp_bsp_init_para * para)
{
	de_al_init(para);
	de_enhance_init(para);
	//FIXME
	de_smbl_init(0, para->reg_base[DISP_MOD_DE]);
	de_smbl_init(1, para->reg_base[DISP_MOD_DE]);
	de_ccsc_init(para);
	WB_EBIOS_Init(para);

	return 0;
}

