#ifndef _DISP_AL_H_
#define _DISP_AL_H_

#include "../bsp_display.h"
#include "../disp_private.h"
#include "de_hal.h"
#include "de_enhance.h"
#include "de_wb.h"
#include "de_smbl.h"
#include "de_csc.h"

extern int disp_al_manager_apply(unsigned int disp, struct disp_manager_data *data);
extern int disp_al_layer_apply(unsigned int disp, struct disp_layer_config_data *data, unsigned int layer_num);
extern s32 disp_init_al(disp_bsp_init_para * para);
extern int disp_al_manager_sync(unsigned int disp);
extern int disp_al_manager_update_regs(unsigned int disp);

int disp_al_enhance_apply(unsigned int disp, struct disp_enhance_config *config);
int disp_al_enhance_update_regs(unsigned int disp);
int disp_al_enhance_sync(unsigned int disp);

s32 disp_al_smbl_apply(unsigned int disp, struct disp_smbl_info *info);
s32 disp_al_smbl_update_regs(unsigned int disp);
s32 disp_al_smbl_sync(unsigned int disp);
s32 disp_al_smbl_get_status(unsigned int disp);

s32 disp_al_capture_sync(u32 disp);
s32 disp_al_capture_apply(unsigned int disp, struct disp_capture_config *cfg);
s32 disp_al_capture_get_status(unsigned int disp);

#endif
