#include "disp_device.h"

static s32 disp_device_set_manager(struct disp_device* dispdev, struct disp_manager *mgr)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("device %d, mgr %d\n", dispdev->disp, mgr->disp);

	dispdev->manager = mgr;
	mgr->device = dispdev;

	return DIS_SUCCESS;
}

static s32 disp_device_unset_manager(struct disp_device* dispdev)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("\n");

	dispdev->manager->device = NULL;
	dispdev->manager = NULL;

	return DIS_SUCCESS;
}

static s32 disp_device_enable(struct disp_device* dispdev)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("\n");

	if(dispdev->manager && dispdev->manager->enable)
		dispdev->manager->enable(dispdev->manager);

	if(dispdev->ops && dispdev->ops->enable)
		dispdev->ops->enable(dispdev);

	return -1;
}

static s32 disp_device_disable(struct disp_device* dispdev)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}
	DE_INF("\n");

	if(dispdev->ops && dispdev->ops->disable)
		dispdev->ops->disable(dispdev);

	if(dispdev->manager && dispdev->manager->disable)
		return dispdev->manager->disable(dispdev->manager);

	return -1;
}

static s32 disp_device_is_enabled(struct disp_device* dispdev)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->is_enabled)
		return dispdev->ops->is_enabled(dispdev);

	return 0;
}

static s32 disp_device_is_used(struct disp_device* dispdev)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->is_used)
		return dispdev->ops->is_used(dispdev);

	return 0;
}

static s32 disp_device_get_resolution(struct disp_device* dispdev, u32 *xres, u32 *yres)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	*xres = dispdev->timings.x_res;
	*yres = dispdev->timings.y_res;

	return 0;
}

static s32 disp_device_get_timings(struct disp_device* dispdev, disp_video_timings *timings)
{
	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(timings)
		memcpy(timings, &dispdev->timings, sizeof(disp_video_timings));

	return 0;
}

static s32 disp_device_set_timings(struct disp_device* dispdev, disp_video_timings *timings)
{
	s32 ret = 0;

	if((NULL == dispdev) || (NULL == timings)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->set_timings)
		ret = dispdev->ops->set_timings(dispdev, timings);

	if((0 == ret))
		memcpy(&dispdev->timings, timings, sizeof(disp_video_timings));

	return ret;
}

static s32 disp_device_set_mode(struct disp_device* dispdev, u32 mode)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->set_mode)
		ret = dispdev->ops->set_mode(dispdev, mode);

	return ret;
}

static s32 disp_device_get_mode(struct disp_device* dispdev)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->get_mode)
		ret = dispdev->ops->get_mode(dispdev);

	return ret;
}

static s32 disp_device_set_bright(struct disp_device* dispdev, u32 bright)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->set_bright)
		ret = dispdev->ops->set_bright(dispdev, bright);

	return ret;
}

static s32 disp_device_get_bright(struct disp_device* dispdev)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->get_bright)
		ret = (s32)dispdev->ops->get_bright(dispdev);

	return ret;
}

static s32 disp_device_dump(struct disp_device* dispdev, char *buf)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->dump)
		ret = dispdev->ops->dump(dispdev, buf);

	return ret;
}

static disp_csc_type disp_device_get_input_csc(struct disp_device* dispdev)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->get_input_csc)
		ret = dispdev->ops->get_input_csc(dispdev);

	return ret;
}

static s32 disp_device_init(struct disp_device* dispdev)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->init)
		ret = dispdev->ops->init(dispdev);

	return ret;
}

static s32 disp_device_exit(struct disp_device* dispdev)
{
	s32 ret = 0;

	if((NULL == dispdev)) {
		DE_WRN("NULL hdl!\n");
		return DIS_FAIL;
	}

	if(dispdev->ops && dispdev->ops->exit)
		ret = dispdev->ops->exit(dispdev);

	return ret;
}


static struct disp_device default_device = {
	.init = disp_device_init,
	.exit = disp_device_exit,
	.set_manager = disp_device_set_manager,
	.unset_manager = disp_device_unset_manager,
	.enable = disp_device_enable,
	.disable = disp_device_disable,
	.is_enabled = disp_device_is_enabled,
	.is_used = disp_device_is_used,
	.get_resolution = disp_device_get_resolution,
	.get_timings = disp_device_get_timings,
	.set_timings = disp_device_set_timings,
	.set_mode = disp_device_set_mode,
	.get_mode = disp_device_get_mode,
	.set_bright = disp_device_set_bright,
	.get_bright = disp_device_get_bright,
	.get_input_csc = disp_device_get_input_csc,
	.dump = disp_device_dump,
};

struct disp_device* disp_device_alloc(void)
{
	struct disp_device *dispdev = NULL;

	dispdev = (struct disp_device*)disp_sys_malloc(sizeof(struct disp_device));
	if(NULL == dispdev) {
		DE_WRN("malloc memory(%d bytes) fail!\n", sizeof(struct disp_device));
	} else {
		memcpy(dispdev, &default_device, sizeof(struct disp_device));
	}

	return dispdev;
}

void disp_device_release(struct disp_device *dispdev)
{
	disp_sys_free((void*)dispdev);
}


