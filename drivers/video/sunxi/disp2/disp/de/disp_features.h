#ifndef _DISP_FEATURES_H_
#define _DISP_FEATURES_H_

#include "bsp_display.h"
#include "disp_private.h"

struct disp_features {
	const s32 num_screens;
	const s32 *num_channels;
	const s32 *num_layers;
	const s32 *is_support_capture;
	const disp_output_type *supported_output_types;
};

s32 bsp_disp_feat_get_num_screens(void);
s32 bsp_disp_feat_get_num_channels(u32 disp);
s32 bsp_disp_feat_get_num_layers(u32 screen_id);
s32 bsp_disp_feat_get_num_layers_by_chn(u32 disp, u32 chn);
bool bsp_disp_feat_is_supported_output_types(u32 screen_id, disp_output_type output_type);
bool bsp_disp_feat_is_support_capture(u32 disp);
s32 disp_init_feat(void);

#endif

