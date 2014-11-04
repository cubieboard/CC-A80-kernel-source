#include "disp_manager.h"

struct disp_manager_private_data {
	bool applied;
	bool enabled;
	struct disp_manager_data *cfg;

	s32 (*shadow_protect)(u32 disp, bool protect);

	u32 reg_base;
	u32 irq_no;
	struct disp_clk_info clk;
};

#if defined(__LINUX_PLAT__)
static spinlock_t mgr_data_lock;
static struct mutex mgr_mlock;
#else
static u32 mgr_data_lock;
static u32 mgr_mlock;
#endif

static struct disp_manager *mgrs = NULL;
static struct disp_manager_private_data *mgr_private;
static struct disp_manager_data *mgr_cfgs;

static struct disp_layer_config_data* lyr_cfgs;

/*
 * layer unit
 */
struct disp_layer_private_data {
	struct disp_layer_config_data *cfg;
	s32 (*shadow_protect)(u32 sel, bool protect);
};

static struct disp_layer *lyrs = NULL;
static struct disp_layer_private_data *lyr_private;
struct disp_layer* disp_get_layer(u32 disp, u32 chn, u32 layer_id)
{
	u32 num_screens, num_channels, num_layers;
	u32 i,j,k,layer_index = 0;

	num_screens = bsp_disp_feat_get_num_screens();
	if((disp >= num_screens)) {
		DE_WRN("disp %d is out of range %d\n",
			disp, num_screens);
		return NULL;
	}
	for(i=0; i<num_screens; i++) {
		num_channels = bsp_disp_feat_get_num_channels(i);
		for(j=0; j<num_channels; j++) {
			num_layers = bsp_disp_feat_get_num_layers_by_chn(disp, j);
			for(k=0; k<num_layers; k++) {
				if((i == disp) && (j == chn) && (k == layer_id)) {
					DE_INF("%d,%d,%d\n", i, j, k);
					return &lyrs[layer_index];
				}
				layer_index ++;
			}
		}
	}

	DE_WRN("%s fail\n", __func__);
	return NULL;

}

struct disp_layer* disp_get_layer_1(u32 disp, u32 layer_id)
{
	u32 num_screens, num_layers;
	u32 i,k;
	u32 layer_index = 0, start_index = 0;

	num_screens = bsp_disp_feat_get_num_screens();
	if((disp >= num_screens)) {
		DE_WRN("disp %d is out of range %d\n",
			disp, num_screens);
		return NULL;
	}

	for(i=0; i<disp; i++)
		start_index += bsp_disp_feat_get_num_layers(i);

	layer_id += start_index;

	for(i=0; i<num_screens; i++) {
			num_layers = bsp_disp_feat_get_num_layers(i);
			for(k=0; k<num_layers; k++) {
				if(layer_index == layer_id) {
					DE_INF("disp%d layer%d: %d,%d,%d\n", disp, layer_id, lyrs[layer_index].disp, lyrs[layer_index].chn, lyrs[layer_index].id);
					return &lyrs[layer_index];
				}
				layer_index ++;
		}
	}

	DE_WRN("%s fail\n", __func__);
	return NULL;
}

static struct disp_layer_private_data *disp_lyr_get_priv(struct disp_layer *lyr)
{
	if(NULL == lyr) {
		DE_WRN("NULL hdl!\n");
		return NULL;
	}

	return (struct disp_layer_private_data *)lyr->data;
}

static s32 disp_lyr_set_manager(struct disp_layer *lyr, struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);

	if((NULL == lyr) || (NULL == lyrp) || (NULL == mgr)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	lyr->manager = mgr;
	list_add_tail(&lyr->list, &mgr->lyr_list);
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return DIS_SUCCESS;
}

static s32 disp_lyr_unset_manager(struct disp_layer *lyr)
{
	unsigned long flags;
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	lyr->manager = NULL;
	list_del(&lyr->list);
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return DIS_SUCCESS;
}

static s32 disp_lyr_check(struct disp_layer *lyr, disp_layer_config *config)
{
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
	//

	return DIS_SUCCESS;
}

static s32 disp_lyr_save_and_dirty_check(struct disp_layer *lyr, disp_layer_config *config)
{
	unsigned long flags;
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);
	char buf[512];

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	if(lyrp->cfg) {
		memcpy(&lyrp->cfg->config, config, sizeof(disp_layer_config));
		//FIXME
		lyrp->cfg->flag |= LAYER_ALL_DIRTY;
	} else {
		DE_INF("cfg is NULL\n");
	}
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);
	DE_INF("layer:ch%d, layer%d, format=%d, size=<%d,%d>, crop=<%lld,%lld,%lld,%lld>,frame=<%d,%d>, en=%d addr[0x%llx,0x%llx,0x%llx>\n",
		config->channel, config->layer_id,  config->info.fb.format,
		config->info.fb.size[0].width, config->info.fb.size[0].height,
		config->info.fb.crop.x>>32, config->info.fb.crop.y>>32, config->info.fb.crop.width>>32, config->info.fb.crop.height>>32,
		config->info.screen_win.width, config->info.screen_win.height,
		config->enable, config->info.fb.addr[0], config->info.fb.addr[1], config->info.fb.addr[2]);

	lyr->dump(lyr, buf);
	DE_INF("%s\n", buf);

	return DIS_SUCCESS;
}

static s32 disp_lyr_is_dirty(struct disp_layer *lyr)
{
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	if(lyrp->cfg) {
		return (lyrp->cfg->flag & LAYER_ALL_DIRTY);
	}

	return 0;
}

static s32 disp_lyr_get_config(struct disp_layer *lyr, disp_layer_config *config)
{
	unsigned long flags;
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	if(lyrp->cfg) {
		memcpy(config, &lyrp->cfg->config, sizeof(disp_layer_config));
	}
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return DIS_SUCCESS;
}

static s32 disp_lyr_apply(struct disp_layer *lyr)
{
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	return DIS_SUCCESS;
}

static s32 disp_lyr_force_apply(struct disp_layer *lyr)
{
	unsigned long flags;
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	lyrp->cfg->flag |= LAYER_ALL_DIRTY;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);
	disp_lyr_apply(lyr);

	return DIS_SUCCESS;
}

static s32 disp_lyr_dump(struct disp_layer *lyr, char *buf)
{
	unsigned long flags;
	struct disp_layer_config_data data;
	struct disp_layer_private_data *lyrp = disp_lyr_get_priv(lyr);
	u32 count = 0;

	if((NULL == lyr) || (NULL == lyrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	memcpy(&data, lyrp->cfg, sizeof(struct disp_layer_config_data));
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	count += sprintf(buf + count, " %5s ", (data.config.info.mode == LAYER_MODE_BUFFER)? "BUF":"COLOR");
	count += sprintf(buf + count, " %8s ", (data.config.enable==1)?"enable":"disable");
	count += sprintf(buf + count, "ch[%1d] ", data.config.channel);
	count += sprintf(buf + count, "lyr[%1d] ", data.config.layer_id);
	count += sprintf(buf + count, "z[%1d] ", data.config.info.zorder);
	count += sprintf(buf + count, "pre_m[%1s] ", (data.config.info.fb.pre_multiply)? "Y":"N");
	count += sprintf(buf + count, "alpha[%5s %3d] ", (data.config.info.alpha_mode)? "globl":"pixel", data.config.info.alpha_value);
	count += sprintf(buf + count, "fmt[%3d] ", data.config.info.fb.format);
	count += sprintf(buf + count, "size[%4d,%4d;%4d,%4d;%4d,%4d] ", data.config.info.fb.size[0].width, data.config.info.fb.size[0].height,
		data.config.info.fb.size[0].width, data.config.info.fb.size[0].height,data.config.info.fb.size[0].width, data.config.info.fb.size[0].height);
	count += sprintf(buf + count, "crop[%4lld,%4lld,%4lld,%4lld] ", data.config.info.fb.crop.x, data.config.info.fb.crop.y,
		data.config.info.fb.crop.width, data.config.info.fb.crop.height);
	count += sprintf(buf + count, "frame[%4d,%4d,%4d,%4d] ", data.config.info.screen_win.x, data.config.info.screen_win.y, data.config.info.screen_win.width, data.config.info.screen_win.height);
	count += sprintf(buf + count, "addr[%8llx,%8llx,%8llx] ", data.config.info.fb.addr[0], data.config.info.fb.addr[1], data.config.info.fb.addr[2]);
	count += sprintf(buf + count, "\n");

	return count;
}

static s32 disp_init_lyr(disp_bsp_init_para * para)
{
	u32 num_screens = 0, num_channels = 0, num_layers = 0;
	u32 max_num_layers = 0;
	u32 disp, chn, layer_id, layer_index = 0;

	DE_INF("disp_init_lyr\n");

	num_screens = bsp_disp_feat_get_num_screens();
	for(disp=0; disp<num_screens; disp++) {
		max_num_layers += bsp_disp_feat_get_num_layers(disp);
	}

	lyrs = (struct disp_layer *)disp_sys_malloc(sizeof(struct disp_layer) * max_num_layers);
	if(NULL == lyrs) {
		DE_WRN("malloc memory fail! size=0x%x\n", sizeof(struct disp_layer) * max_num_layers);
		return DIS_FAIL;
	}
	DE_INF("malloc memory ok! size=0x%x\n", sizeof(struct disp_layer) * max_num_layers);

	lyr_private = (struct disp_layer_private_data *)disp_sys_malloc(sizeof(struct disp_layer_private_data) * max_num_layers);
	if(NULL == lyr_private) {
		DE_WRN("malloc memory fail! size=0x%x\n", sizeof(struct disp_layer_private_data) * max_num_layers);
		return DIS_FAIL;
	}
	DE_INF("malloc memory ok! size=0x%x\n", sizeof(struct disp_layer_private_data) * max_num_layers);

	lyr_cfgs = (struct disp_layer_config_data *)disp_sys_malloc(sizeof(struct disp_layer_config_data) * max_num_layers);
	if(NULL == lyr_cfgs) {
		DE_WRN("malloc memory fail! size=0x%x\n", sizeof(struct disp_layer_private_data) * max_num_layers);
		return DIS_FAIL;
	}
	DE_INF("malloc memory ok! size=0x%x\n", sizeof(struct disp_layer_private_data) * max_num_layers);

	for(disp=0; disp<num_screens; disp++) {

		num_channels = bsp_disp_feat_get_num_channels(disp);
		DE_INF("%s, disp %d, num_channels=%d\n", __func__, disp, num_channels);
		for(chn=0; chn<num_channels; chn++) {
			num_layers = bsp_disp_feat_get_num_layers_by_chn(disp, chn);
			DE_INF("%s, disp %d, chan %d, num_layers=%d\n", __func__, disp, chn, num_layers);
			for(layer_id=0; layer_id<num_layers; layer_id++,layer_index ++) {
				struct disp_layer *lyr = &lyrs[layer_index];
				struct disp_layer_config_data *lyr_cfg = &lyr_cfgs[layer_index];
				struct disp_layer_private_data *lyrp = &lyr_private[layer_index];

				lyrp->shadow_protect = para->shadow_protect;
				lyrp->cfg = lyr_cfg;

				sprintf(lyr->name, "mgr%d chn%d lyr%d", disp, chn, layer_id);
				lyr->disp = disp;
				lyr->chn = chn;
				lyr->id = layer_id;
				lyr->data = (void*)lyrp;

				lyr->set_manager = disp_lyr_set_manager;
				lyr->unset_manager = disp_lyr_unset_manager;
				lyr->apply = disp_lyr_apply;
				lyr->force_apply = disp_lyr_force_apply;
				lyr->check = disp_lyr_check;
				lyr->save_and_dirty_check = disp_lyr_save_and_dirty_check;
				lyr->get_config = disp_lyr_get_config;
				lyr->dump = disp_lyr_dump;
				lyr->is_dirty = disp_lyr_is_dirty;
				//lyr->init = disp_lyr_init;
				//lyr->exit = disp_lyr_exit;

				//lyr->init(lyr);
			}
		}
	}

	return 0;
}

struct disp_manager* disp_get_layer_manager(u32 disp)
{
	u32 num_screens;

	num_screens = bsp_disp_feat_get_num_screens();
	if(disp >= num_screens) {
		DE_WRN("disp %d out of range\n", disp);
		return NULL;
	}

	return &mgrs[disp];
}
static struct disp_manager_private_data *disp_mgr_get_priv(struct disp_manager *mgr)
{
	if(NULL == mgr) {
		DE_WRN("NULL hdl!\n");
		return NULL;
	}

	return &mgr_private[mgr->disp];
}

static s32 disp_mgr_shadow_protect(struct disp_manager *mgr, bool protect)
{
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	if(mgrp->shadow_protect)
		return mgrp->shadow_protect(mgr->disp, protect);

	return -1;
}

static s32 disp_mgr_clk_init(struct disp_manager *mgr)
{
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	//FIXME

	return 0;
}

static s32 disp_mgr_clk_exit(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->clk.enabled = false;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return 0;
}

static s32 disp_mgr_clk_enable(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	DE_INF("mgr %d clk enable\n", mgr->disp);
#if defined(__FPGA_DEBUG__)
	writel(readl(SUNXI_CCM_VBASE) | 0x1000, SUNXI_CCM_VBASE + 0x2c4);//de reset
	writel(0xffffffff, SUNXI_DE_VBASE + 0x00);//top clk reset
	writel(0xffffffff, SUNXI_DE_VBASE + 0x04);//top clk reset
	writel(0xffffffff, SUNXI_DE_VBASE + 0x08);//top clk reset
	writel(0xffffffff, SUNXI_DE_VBASE + 0x0c);//top clk reset
#endif

	//FIXME
	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->clk.enabled = true;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);
	return 0;
}

static s32 disp_mgr_clk_disable(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	//FIXME
	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->clk.enabled = false;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return 0;
}

static s32 disp_mgr_init(struct disp_manager *mgr)
{
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_mgr_clk_init(mgr);
	return 0;
}

static s32 disp_mgr_exit(struct disp_manager *mgr)
{
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	//FIXME, disable manager
	disp_mgr_clk_exit(mgr);

	return 0;
}

static s32 disp_mgr_set_back_color(struct disp_manager *mgr, disp_color *back_color)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	memcpy(&mgrp->cfg->config.back_color, back_color, sizeof(disp_color));
	mgrp->cfg->flag |= MANAGER_BACK_COLOR_DIRTY;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	mgr->apply(mgr);

	return DIS_SUCCESS;
}

static s32 disp_mgr_get_back_color(struct disp_manager *mgr, disp_color *back_color)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

  disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	memcpy(back_color, &mgrp->cfg->config.back_color, sizeof(disp_color));
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return DIS_SUCCESS;
}

static s32 disp_mgr_set_color_key(struct disp_manager *mgr, disp_colorkey *ck)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	memcpy(&mgrp->cfg->config.ck, ck, sizeof(disp_colorkey));
	mgrp->cfg->flag |= MANAGER_CK_DIRTY;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	mgr->apply(mgr);

	return DIS_SUCCESS;
}

static s32 disp_mgr_get_color_key(struct disp_manager *mgr, disp_colorkey *ck)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	memcpy(ck, &mgrp->cfg->config.ck, sizeof(disp_colorkey));
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return DIS_SUCCESS;
}

static s32 disp_mgr_set_layer_config(struct disp_manager *mgr, disp_layer_config *config, unsigned int layer_num)
{
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);
	unsigned int num_layers = 0, layer_index = 0;

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
	DE_INF("mgr%d, config %d layers\n", mgr->disp, layer_num);

	DE_INF("layer:ch%d, layer%d, format=%d, size=<%d,%d>, crop=<%lld,%lld,%lld,%lld>,frame=<%d,%d>, en=%d addr[0x%llx,0x%llx,0x%llx>\n",
		config->channel, config->layer_id,  config->info.fb.format,
		config->info.fb.size[0].width, config->info.fb.size[0].height,
		config->info.fb.crop.x>>32, config->info.fb.crop.y>>32, config->info.fb.crop.width>>32, config->info.fb.crop.height>>32,
		config->info.screen_win.width, config->info.screen_win.height,
		config->enable, config->info.fb.addr[0], config->info.fb.addr[1], config->info.fb.addr[2]);

	num_layers = bsp_disp_feat_get_num_layers(mgr->disp);
	if((NULL == config) || (layer_num == 0) || (layer_num > num_layers)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	for(layer_index=0; layer_index < layer_num; layer_index++) {
		struct disp_layer *lyr = NULL;

		lyr = disp_get_layer(mgr->disp, config->channel, config->layer_id);
		if(NULL == lyr)
			continue;
		if(!lyr->check(lyr, config)) {
			lyr->save_and_dirty_check(lyr, config);
		}
		config++;
	}

	if(mgr->apply)
		mgr->apply(mgr);

	return DIS_SUCCESS;
}

static s32 disp_mgr_get_layer_config(struct disp_manager *mgr, disp_layer_config *config, unsigned int layer_num)
{
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);
	struct disp_layer *lyr;
	unsigned int num_layers = 0, layer_index = 0;

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	num_layers = bsp_disp_feat_get_num_layers(mgr->disp);
	if((NULL == config) || (layer_num == 0) || (layer_num > num_layers)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
	for(layer_index=0; layer_index < layer_num; layer_index++) {
		lyr = disp_get_layer(mgr->disp, config->channel, config->layer_id);
		if(NULL == lyr) {
			DE_WRN("get layer(%d,%d,%d) fail\n", mgr->disp, config->channel, config->layer_id);
			continue;
		}
		if(lyr->get_config(lyr, config))
			lyr->get_config(lyr, config);
		config++;
	}

	return DIS_SUCCESS;
}

static s32 disp_mgr_sync(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);
	struct disp_enhance *enhance = mgr->enhance;
	struct disp_smbl *smbl = mgr->smbl;
	struct disp_capture *cptr = mgr->cptr;

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
#if 0
	//print current line
	{
		u32 reg = *(volatile u32*)0xf1c0c0fc;
		reg = (reg >> 16) & 0xfff;
		if(reg > 25)
			printk("%s L%d %d\n", __func__, __LINE__, reg);
	}
#endif

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	if(!mgrp->enabled) {
		disp_sys_irqunlock((void*)&mgr_data_lock, &flags);
		return -1;
	}
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	//mgr->update_regs(mgr);
	disp_al_manager_sync(mgr->disp);
	mgr->update_regs(mgr);
	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->applied = false;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	/* enhance */
	if(enhance && enhance->sync)
		enhance->sync(enhance);

	/* smbl */
	if(smbl && smbl->sync)
		smbl->sync(smbl);

	/* capture */
	if(cptr && cptr->sync)
		cptr->sync(cptr);

	return DIS_SUCCESS;
}

static s32 disp_mgr_update_regs(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
	//__inf("disp_mgr_update_regs, mgr%d\n", mgr->disp);

#if 0
	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	if(!mgrp->enabled) {
		disp_sys_irqunlock((void*)&mgr_data_lock, &flags);
	   return -1;
	}
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);
#endif

	//disp_mgr_shadow_protect(mgr, true);
	//FIXME update_regs, other module may need to sync while manager don't
	//if(true == mgrp->applied)
		disp_al_manager_update_regs(mgr->disp);
	//disp_mgr_shadow_protect(mgr, false);

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->applied = false;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return DIS_SUCCESS;
}

static s32 disp_mgr_apply(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);
	struct disp_manager_data data;
	bool dirty = false;
	struct disp_layer *lyr = NULL;

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
	DE_INF("mgr %d apply\n", mgr->disp);

  disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	if((mgrp->enabled) && (mgrp->cfg->flag & MANAGER_ALL_DIRTY))
	{
		dirty = true;
		memcpy(&data, mgrp->cfg, sizeof(struct disp_manager_data));
		mgrp->cfg->flag &= ~MANAGER_ALL_DIRTY;
	}

	list_for_each_entry(lyr, &mgr->lyr_list, list)
	{
		if(lyr->is_dirty && lyr->is_dirty(lyr))
			dirty = true;
	}

	mgrp->applied = true;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	disp_mgr_shadow_protect(mgr, true);
	if(dirty) {
		u32 num_layers = bsp_disp_feat_get_num_layers(mgr->disp);
		disp_al_manager_apply(mgr->disp, mgrp->cfg);
		//FIXME, disp 1' lyr_cfgs should start from 16
		disp_al_layer_apply(mgr->disp, lyr_cfgs, num_layers);
	}
	disp_mgr_shadow_protect(mgr, false);

	disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->applied = true;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return DIS_SUCCESS;
}

static s32 disp_mgr_force_apply(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);
	struct disp_layer* lyr = NULL;

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
	__inf("%s, mgr%d\n", __func__, mgr->disp);

  disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->cfg->flag |= MANAGER_ALL_DIRTY;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);
	list_for_each_entry(lyr, &mgr->lyr_list, list) {
		lyr->force_apply(lyr);
	}

	disp_mgr_apply(mgr);
	disp_mgr_update_regs(mgr);

	return 0;
}

static s32 disp_mgr_enable(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);
	unsigned int width = 0, height = 0;

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}
	DE_INF("mgr %d enable\n", mgr->disp);

	disp_mgr_clk_enable(mgr);

	if(mgr->device && mgr->device->get_resolution)
		mgr->device->get_resolution(mgr->device, &width, &height);

  disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->enabled = 1;
	mgrp->cfg->config.enable = 1;
	mgrp->cfg->flag |= MANAGER_ENABLE_DIRTY;

	mgrp->cfg->config.size.width = width;
	mgrp->cfg->config.size.height = height;
	mgrp->cfg->flag |= MANAGER_SIZE_DIRTY;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	disp_mgr_force_apply(mgr);

	return 0;
}

static s32 disp_mgr_disable(struct disp_manager *mgr)
{
	unsigned long flags;
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	DE_INF("mgr %d disable\n", mgr->disp);

	disp_mgr_clk_disable(mgr);

  disp_sys_irqlock((void*)&mgr_data_lock, &flags);
	mgrp->enabled = 0;
	disp_sys_irqunlock((void*)&mgr_data_lock, &flags);

	return 0;
}

static s32 disp_mgr_is_enabled(struct disp_manager *mgr)
{
	struct disp_manager_private_data *mgrp = disp_mgr_get_priv(mgr);

	if((NULL == mgr) || (NULL == mgrp)) {
		DE_WRN("NULL hdl!\n");
		return -1;
	}

	return mgrp->enabled;

}

s32 disp_init_mgr(disp_bsp_init_para * para)
{
	u32 num_screens;
	u32 disp;
	struct disp_manager *mgr;
	struct disp_manager_private_data *mgrp;

	DE_INF("%s\n", __func__);

#if defined(__LINUX_PLAT__)
	spin_lock_init(&mgr_data_lock);
	mutex_init(&mgr_mlock);
#endif
	num_screens = bsp_disp_feat_get_num_screens();
	mgrs = (struct disp_manager *)disp_sys_malloc(sizeof(struct disp_manager) * num_screens);
	if(NULL == mgrs) {
		DE_WRN("malloc memory fail!\n");
		return DIS_FAIL;
	}
	mgr_private = (struct disp_manager_private_data *)disp_sys_malloc(sizeof(struct disp_manager_private_data) * num_screens);
	if(NULL == mgr_private) {
		DE_WRN("malloc memory fail! size=0x%x x %d\n", sizeof(struct disp_manager_private_data), num_screens);
		return DIS_FAIL;
	}
	mgr_cfgs = (struct disp_manager_data *)disp_sys_malloc(sizeof(struct disp_manager_data) * num_screens);
	if(NULL == mgr_private) {
		DE_WRN("malloc memory fail! size=0x%x x %d\n", sizeof(struct disp_manager_private_data), num_screens);
		return DIS_FAIL;
	}

	for(disp=0; disp<num_screens; disp++) {
		mgr = &mgrs[disp];
		mgrp = &mgr_private[disp];

		DE_INF("mgr %d, 0x%x\n", disp, (u32)mgr);

		sprintf(mgr->name, "mgr%d", disp);
		mgr->disp = disp;
		mgrp->cfg = &mgr_cfgs[disp];
		mgrp->shadow_protect = para->shadow_protect;


		mgr->enable = disp_mgr_enable;
		mgr->disable = disp_mgr_disable;
		mgr->is_enabled = disp_mgr_is_enabled;
		mgr->set_color_key = disp_mgr_set_color_key;
		mgr->get_color_key = disp_mgr_get_color_key;
		mgr->set_back_color = disp_mgr_set_back_color;
		mgr->get_back_color = disp_mgr_get_back_color;
		mgr->set_layer_config = disp_mgr_set_layer_config;
		mgr->get_layer_config = disp_mgr_get_layer_config;

		mgr->init = disp_mgr_init;
		mgr->exit = disp_mgr_exit;

		mgr->apply = disp_mgr_apply;
		mgr->update_regs = disp_mgr_update_regs;
		mgr->force_apply = disp_mgr_force_apply;
		mgr->sync = disp_mgr_sync;

		INIT_LIST_HEAD(&mgr->lyr_list);

		mgr->init(mgr);
	}

	disp_init_lyr(para);
	return 0;
}

