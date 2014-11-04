
#include "inv_sensors_sm.h"
#include "inv_sensors_aux.h"

#ifdef CONFIG_INV_SENSORS_MPU5400
#include "inv_sensors_mpu5400.h"
#endif //CONFIG_INV_SENSORS_MPU9X50

#ifdef CONFIG_INV_SENSORS_DMP
#include "inv_dmp_base.h"
#endif //CONFIG_INV_SENSORS_DMP



#define SM_DRIVER_LIST_MAX (4)

struct invsens_sm_cfg {
	struct invsens_board_cfg_t *board_cfg;

	struct invsens_driver_t *driver_list[SM_DRIVER_LIST_MAX];
	struct invsens_sm_ctrl_t sm_ctrl;
	struct invsens_driver_t *dmp_drv;

	u16 driver_count;
	u32 enabled_mask;
	long delays[INV_FUNC_NUM];
};


static int sm_init_cfg(struct invsens_sm_cfg *smcfg)
{
	int res = SM_SUCCESS;
	int i = 0;

	smcfg->enabled_mask = 0;
	smcfg->sm_ctrl.enable_mask = 0;

	for (i = 0; i < INV_FUNC_NUM; i++) {
		/*internal purpose of delay */
		smcfg->delays[i] = SM_DELAY_DEFAULT;

		/*sharing purpose of delay */
		smcfg->sm_ctrl.delays[i] = SM_DELAY_DEFAULT;
	}
	return res;
}

static int sm_init_drivers(struct invsens_sm_cfg *smcfg)
{
	int res = SM_SUCCESS, i;
	struct invsens_driver_t *drv = NULL;

	if (!smcfg)
		return -SM_EINVAL;

	for( i = 0; i < smcfg->driver_count; i ++) {
		drv = smcfg->driver_list[i];
		if (drv && drv->init)
			res = drv->init(drv);
	}
	return res;
}

static int sm_load_drivers(struct invsens_sm_cfg *smcfg)
{
	int res = 0;
	u16 drv_cnt = 0;
	INV_DBG_FUNC_NAME;

	smcfg->driver_count = 0;
	smcfg->dmp_drv = NULL;

#ifdef CONFIG_INV_SENSORS_MPU5400
	res = invsens_load_mpu5400(&smcfg->driver_list[drv_cnt++],
		smcfg->board_cfg);
	if (res)
		INVSENS_LOGD("mpu5400 driver failed : %d\n", res);
#endif

	res = invsens_load_aux(&smcfg->driver_list[drv_cnt++],
		smcfg->board_cfg);
	if (res)
		INVSENS_LOGD("aux driver failed : %d\n", res);

#ifdef CONFIG_INV_SENSORS_DMP
	res = invsens_load_dmp(&smcfg->driver_list[drv_cnt++],
		smcfg->board_cfg);
	if (res)
		INVSENS_LOGD("dmp driver failed : %d\n", res);
	smcfg->dmp_drv =smcfg->driver_list[drv_cnt - 1];
#endif

	smcfg->driver_count = drv_cnt;

	return res;
}

static u32 sm_get_physical_func(u32 func)
{
	u32 mask = 0;

	switch (func) {
	case INV_FUNC_GAMING_ROTATION_VECTOR:
		mask = ((1 << INV_FUNC_ACCEL) | (1 << INV_FUNC_GYRO));
		break;
	case INV_FUNC_SIGNIFICANT_MOTION_DETECT:
		mask = (1 << INV_FUNC_ACCEL);
		break;
	case INV_FUNC_STEP_DETECT:
		mask = (1 << INV_FUNC_ACCEL);
		break;
	case INV_FUNC_STEP_COUNT:
		mask = (1 << INV_FUNC_ACCEL);
		break;
	case INV_FUNC_SCREEN_ORIENTATION:
		mask = (1 << INV_FUNC_ACCEL);
		break;
	case INV_FUNC_ROTATION_VECTOR:
		mask = ((1 << INV_FUNC_ACCEL) | (1 << INV_FUNC_GYRO)
			| (1 << INV_FUNC_COMPASS));
		break;
	case INV_FUNC_GEOMAG_ROTATION_VECTOR:
		mask = ((1 << INV_FUNC_ACCEL) | (1 << INV_FUNC_COMPASS));
		break;
	case INV_FUNC_BATCH:
		break;
	case INV_FUNC_FLUSH:
		break;

	default:
		mask = (1 << func);
		break;
	}

	return mask;
}


static u32 sm_get_physical_func_mask(u32 func_mask)
{
	u32 mask = 0;
	int i = 0;

	for (i = 0; i < INV_FUNC_NUM; i++) {
		if (func_mask & (1 << i))
			mask |= sm_get_physical_func(i);
	}

	INVSENS_LOGD("%s : mask = 0x%x physical = 0x%x\n", __func__, func_mask,
		     mask);

	return mask;
}


static bool sm_needs_dmp(struct invsens_sm_cfg *smcfg, u32 enable_mask)
{
	/*dmp needs to be enabled as default*/
	bool is_dmp_needed = true;
#if 0
	if(!(enable_mask & ~(1 << INV_FUNC_ACCEL)))
		is_dmp_needed = false;
	else if(!(enable_mask & ~(1 << INV_FUNC_GYRO)))
		is_dmp_needed = false;
	else if(!(enable_mask & ~(1 << INV_FUNC_FT_SENSOR_ON)))
		is_dmp_needed = false;
#endif
	return is_dmp_needed;
}


static int sm_update_smctl(struct invsens_sm_cfg *smcfg, u32 enable_mask)
{
	int res = SM_SUCCESS;
	int i = 0;
	u32 mask = enable_mask;
	u32 pysical_mask;


	INV_DBG_FUNC_NAME;

	if (!smcfg)
		return -SM_EINVAL;

	smcfg->sm_ctrl.is_dmp_on = sm_needs_dmp(smcfg, enable_mask);

	for (i = 0; i < INV_FUNC_NUM; i++)
		if (enable_mask & (1 << i))
			smcfg->sm_ctrl.delays[i] = smcfg->delays[i];

	pysical_mask = sm_get_physical_func_mask(enable_mask);

	/*exclude compass if it isn't */
	if (!smcfg->sm_ctrl.has_compass) {
		pysical_mask &= ~(1 << INV_FUNC_COMPASS);
		mask &= ~(1 << INV_FUNC_COMPASS);
	}

	smcfg->sm_ctrl.pysical_mask = pysical_mask;
	smcfg->sm_ctrl.enable_mask = mask;

	INVSENS_LOGD("%s : enable_mask = %x\n", __func__,
		     smcfg->sm_ctrl.enable_mask);
	return res;
}

static struct invsens_driver_t *sm_find_driver(struct invsens_sm_cfg *smcfg,
	u32 func)
{
	int i;
	struct invsens_driver_t *drv = NULL;

	for( i = 0; i < smcfg->driver_count; i ++) {
		drv = smcfg->driver_list[i];
		if(drv && (drv->func_mask & ( 1 << func)))
			return drv;
	}

	return NULL;
}

static int sm_read_data(struct invsens_sm_cfg *smcfg,
	 struct invsens_data_list_t *data_list)
{
	int i;
	int res = SM_SUCCESS;
	struct invsens_driver_t *drv = NULL;

	for( i = 0; i < smcfg->driver_count; i ++) {
		drv = smcfg->driver_list[i];
		if (drv && drv->is_activated && drv->read)
			res = drv->read(drv, data_list);
	}

	return res;
}


int invsens_sm_init(struct invsens_sm_data_t *sm)
{
	int res = 0;
	struct invsens_sm_cfg *smcfg = NULL;

	INV_DBG_FUNC_NAME;

	smcfg = kzalloc(sizeof(struct invsens_sm_cfg), GFP_KERNEL);
	if (!smcfg) {
		INVSENS_LOGD("memory allocation error : smcfg\n");
		res = -SM_ENOMEM;
		goto error_case;
	}
	if (sm->board_cfg.platform_data) {
		smcfg->board_cfg = &sm->board_cfg;

		if (smcfg->board_cfg->platform_data->compass.aux_id !=
			INVSENS_AID_NONE)
			smcfg->sm_ctrl.has_compass = true;
		else
			smcfg->sm_ctrl.has_compass = false;
	}
	res = sm_init_cfg(smcfg);
	if (res) {
		INVSENS_LOGD("smcfg intialiation error = %d\n", res);
		goto error_case;
	}

	res = sm_load_drivers(smcfg);
	if (res) {
		INVSENS_LOGD("driver loading error = %d\n", res);
		goto error_case;
	}

	res = sm_init_drivers(smcfg);
	if (res) {
		INVSENS_LOGD("driver initialization error = %d\n", res);
		goto error_case;
	}

	sm->user_data = smcfg;

	return SM_SUCCESS;

      error_case:
	if (smcfg)
		kzfree(smcfg);
	return res;
}

int invsens_sm_term(struct invsens_sm_data_t *sm)
{
	struct invsens_sm_cfg *smcfg = NULL;

	smcfg = sm->user_data;
	sm->user_data = NULL;

	kfree(smcfg);

	return SM_SUCCESS;
}



int invsens_sm_enable_sensor(struct invsens_sm_data_t *sm, u32 func,
			     bool enable, long delay)
{
	int res = 0, i;
	struct invsens_sm_cfg *smcfg = NULL;
	struct invsens_driver_t *drv = NULL;
	u32 enable_mask;

	INV_DBG_FUNC_NAME;

	if (!sm)
		return -SM_EINVAL;

	smcfg = sm->user_data;

	if (enable)
		enable_mask = smcfg->enabled_mask | (1 << func);
	else
		enable_mask = smcfg->enabled_mask & ~(1 << func);

	if (delay > 0)
		smcfg->delays[func] = delay;

	(void) sm_update_smctl(smcfg, enable_mask);

	/*----------------------------------------
		1. enable sensors
	-----------------------------------------*/
	/*enable physical driver */
	for(i = 0; i < smcfg->driver_count; i++) {
		drv = smcfg->driver_list[i];
		if (drv && (drv->driver_layer == INV_DRIVER_LAYER_NATIVE))
			res = drv->enable(drv, smcfg->sm_ctrl.pysical_mask,
					  enable,
					  (void *) &smcfg->sm_ctrl);
	}

	/*enable dmp driver */
	drv = smcfg->dmp_drv;
	if (drv && smcfg->sm_ctrl.is_dmp_on
		&& (enable_mask & drv->func_mask))
		res = drv->enable(drv, smcfg->sm_ctrl.enable_mask,
				  enable, (void *) &smcfg->sm_ctrl);


	/*----------------------------------------
		2. post-enable
	-----------------------------------------*/
	/*enable physical driver */
	for(i = 0; i < smcfg->driver_count; i++) {
		drv = smcfg->driver_list[i];
		if (drv && drv->postenable &&
			(drv->driver_layer == INV_DRIVER_LAYER_NATIVE))
			res = drv->postenable(drv, smcfg->sm_ctrl.pysical_mask,
					  enable,
					  (void *) &smcfg->sm_ctrl);
	}

	/*enable dmp driver */
	drv = smcfg->dmp_drv;
	if (drv && drv->postenable
		&& smcfg->sm_ctrl.is_dmp_on
		&& (enable_mask & drv->func_mask))
		res = drv->postenable(drv, smcfg->sm_ctrl.enable_mask,
				  enable, (void *) &smcfg->sm_ctrl);



	if (res == SM_SUCCESS)
		smcfg->enabled_mask = enable_mask;

	return res;
}



int invsens_sm_set_delay(struct invsens_sm_data_t *sm, u32 func,
			 long delay)
{
	int res = 0;
	struct invsens_sm_cfg *smcfg = NULL;
	struct invsens_driver_t *drv = NULL;

	INV_DBG_FUNC_NAME;

	if (!sm)
		return -SM_EINVAL;

	smcfg = sm->user_data;

	smcfg->delays[func] = delay;

	INVSENS_LOGD("%s : delay[%d] = %ld\n ", __func__, func, delay);

	drv = sm_find_driver(smcfg, func);

	if (drv && (drv->set_delay))
		res = drv->set_delay(drv, (1 << func), delay);

	return res;
}


int invsens_sm_batch(struct invsens_sm_data_t *sm, u32 func, long period,
		     long timeout)
{
	int res = 0;
	struct invsens_sm_cfg *smcfg = NULL;
	struct invsens_driver_t *drv = NULL;

	INV_DBG_FUNC_NAME;

	if (!sm)
		return -SM_EINVAL;

	smcfg = sm->user_data;

	if (!timeout) {
		smcfg->delays[func] = period;
		INVSENS_LOGD("%s : delay[%d] = %ld\n ", __func__, func,
			     period);
		/*if func is already enabled, reset the update rate*/
		if(smcfg->enabled_mask & (1 << func))
			res = invsens_sm_enable_sensor(sm, func, true, period);
	} else {
		drv = sm_find_driver(smcfg, func);

		if (drv && (drv->batch))
			res =
			    drv->batch(drv, (1 << func), period, timeout);
	}
	return res;
}


int invsens_sm_flush(struct invsens_sm_data_t *sm, u32 func)
{
	int res = 0;
	struct invsens_sm_cfg *smcfg = NULL;
	struct invsens_driver_t *drv = NULL;

	INV_DBG_FUNC_NAME;

	if (!sm)
		return -SM_EINVAL;

	smcfg = sm->user_data;

	drv = sm_find_driver(smcfg, func);

	if (drv && (drv->flush))
		res = drv->flush(drv, (1 << func));

	return res;
}


int invsens_sm_sync(struct invsens_sm_data_t *sm, const char *key)
{
	int res = 0, i;
	struct invsens_sm_cfg *smcfg = NULL;
	struct invsens_driver_t *drv = NULL;
	struct invsens_ioctl_param_t ioctl;

	if (!sm)
		return -SM_EINVAL;

	INV_DBG_FUNC_NAME;

	smcfg = sm->user_data;

	for( i = 0; i < smcfg->driver_count; i ++) {
		drv = smcfg->driver_list[i];
		if (drv && (drv->sync != NULL))
			res = drv->sync(drv, key);
	}

	/*revise the start address of DMP*/
	if(invsens_sm_ioctl(sm,INV_IOCTL_GET_START_ADDR, 0,
		&ioctl)	== SM_IOCTL_HANDLED) {
		smcfg->sm_ctrl.start_addr = (int) ioctl.u32d[0];
	}
	
	return res;
}





int invsens_sm_read(struct invsens_sm_data_t *sm,
		    struct invsens_data_list_t *data_list)
{
	struct invsens_sm_cfg *smcfg = NULL;


	INV_DBG_FUNC_NAME;
	if (!sm) {
		printk("%d------------\n", __LINE__);
		return -SM_EINVAL;
	}
	
	if (sm->user_data == NULL) {
		printk("%d------------\n", __LINE__);
		return  -SM_EINVAL;
	}

	smcfg = (struct invsens_sm_cfg *)sm->user_data;

	
	if (smcfg == NULL) {
		printk("%d------------\n", __LINE__);
		return  -SM_EINVAL;
	}

	data_list->enable_mask =smcfg->enabled_mask;

	return sm_read_data(smcfg, data_list);
}


int invsens_sm_get_enabled_mask(struct invsens_sm_data_t *sm,
	u32 *enabled_mask)
{
	struct invsens_sm_cfg *smcfg = NULL;

	if (!sm)
		return -SM_EINVAL;

	INV_DBG_FUNC_NAME;

	smcfg = sm->user_data;

	if(enabled_mask == NULL)
		return -SM_EINVAL;

	*enabled_mask = smcfg->enabled_mask;

	return SM_SUCCESS;
}


bool invsens_sm_is_func_enabled(struct invsens_sm_data_t *sm, int func)
{
	struct invsens_sm_cfg *smcfg = NULL;
	bool is_eanbled = false;

	INV_DBG_FUNC_NAME;

	if (!sm)
		return -SM_EINVAL;

	smcfg = sm->user_data;

	if(smcfg->enabled_mask & (1 << func))
		is_eanbled = true;

	return is_eanbled;
}

int invsens_sm_ioctl(struct invsens_sm_data_t *sm, u32 cmd,
		     long lparam, void *vparam)
{
	struct invsens_sm_cfg *smcfg = NULL;
	struct invsens_driver_t *drv = NULL;

	int res = 0, i;

	INV_DBG_FUNC_NAME;

	smcfg = sm->user_data;

	for( i = 0; i < smcfg->driver_count; i ++) {
		drv = smcfg->driver_list[i];
		if (drv && (drv->ioctl!= NULL)) {
			res = drv->ioctl(drv, cmd, lparam, vparam);
			if(res == SM_IOCTL_HANDLED)
				return res;
		}
	}

	return SM_IOCTL_NOTHANDLED;
}
