#include "disp_features.h"

static const struct disp_features *disp_current_features;

static const u32 sun8iw6_disp_num_channels[] = {
	/* DISP0 */
	4,
	/* DISP1 */
	2,
};

static const u32 sun8iw6_disp_num_layers[] = {
	/* DISP0 CH0 */
	4,
	/* DISP0 CH1 */
	4,
	/* DISP0 CH2 */
	4,
	/* DISP0 CH3 */
	4,
	/* DISP1 CH0 */
	4,
	/* DISP1 CH1 */
	4,
};

static const disp_output_type sun8iw6_disp_supported_output_types[] = {
	/* DISP0 */
	DISP_OUTPUT_TYPE_LCD,
	/* DISP1 */
	DISP_OUTPUT_TYPE_HDMI,
};

static const u32 sun8iw6_disp_is_support_capture[] = {
	/* DISP0 */
	1,
	/* DISP1 */
	0,
};

static const struct disp_features sun8iw6_disp_features = {
	.num_screens = 2,
	.num_channels = sun8iw6_disp_num_channels,
	.num_layers = sun8iw6_disp_num_layers,
	.supported_output_types = sun8iw6_disp_supported_output_types,
	.is_support_capture = sun8iw6_disp_is_support_capture,
};

s32 bsp_disp_feat_get_num_screens(void)
{
	return disp_current_features->num_screens;
}

s32 bsp_disp_feat_get_num_channels(u32 disp)
{
	return disp_current_features->num_channels[disp];
}

s32 bsp_disp_feat_get_num_layers(u32 disp)
{
	u32 i, index = 0, num_channels = 0;
	s32 num_layers = 0;

	if(disp >= bsp_disp_feat_get_num_screens())
		return 0;

	for(i=0; i<disp; i++)
		index +=  bsp_disp_feat_get_num_channels(i);

	num_channels = bsp_disp_feat_get_num_channels(disp);
	for(i=0; i<num_channels; i++, index++)
		num_layers += disp_current_features->num_layers[index];

	return num_layers;
}

s32 bsp_disp_feat_get_num_layers_by_chn(u32 disp, u32 chn)
{
	u32 i, index = 0;

	if(disp >= bsp_disp_feat_get_num_screens())
		return 0;
	if(chn >= bsp_disp_feat_get_num_channels(disp))
		return 0;

	for(i=0; i<disp; i++)
		index +=  bsp_disp_feat_get_num_channels(i);
	index += chn;

	return disp_current_features->num_layers[index];
}

bool bsp_disp_feat_is_supported_output_types(u32 disp, disp_output_type output_type)
{
	return ((disp_current_features->supported_output_types[disp] & output_type) == output_type);
}

bool bsp_disp_feat_is_support_capture(u32 disp)
{
	return (disp_current_features->is_support_capture[disp] == 1);
}

s32 disp_init_feat(void)
{
#if defined(CONFIG_ARCH_SUN8IW6P1) || defined(CONFIG_ARCH_SUN9IW1P1)
	disp_current_features = &sun8iw6_disp_features;
#else
	DE_WRN("error, not support platform !\n");
#endif

#if 0
	{
		u32 num_screens;
		DE_INF("------------FEAT---------\n");
		num_screens = bsp_disp_feat_get_num_screens();
		DE_INF("screens:%d\n", num_screens);
		for(disp=0; disp<num_screens; disp++) {
			u32 num_chns = bsp_disp_feat_get_num_channels(disp);
			u32 num_layers	=  bsp_disp_feat_get_num_layers(disp);
			u32 i;
			DE_INF("screen %d: %d chns, %d layers\n", disp, num_chns, num_layers);
			for(i=0; i<num_chns; i++) {
				num_layers = bsp_disp_feat_get_num_layers_by_chn(disp, i);
				DE_INF("screen %d, chn %d: %d layers\n", disp, i, num_layers);
			}

		}
		DE_INF("-------------------------\n");
	}
#endif
	return 0;
}

