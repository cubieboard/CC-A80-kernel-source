#ifndef __DISP_DEVICE_H__
#define __DISP_DEVICE_H__

#include "disp_private.h"

struct disp_device* disp_device_alloc(void);
void disp_device_release(struct disp_device *dispdev);

#endif
