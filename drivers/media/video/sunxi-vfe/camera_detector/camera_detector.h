#ifndef __CAMERA_DETECTOR_H__
#define __CAMERA_DETECTOR_H__

#include "camera_includes.h"

//error number
#define ECFGNOEX            1   //config not exit
#define ECFGERR             2   //config error
#define ECAMNUMERR          3   //camera number error
#define ECLKSRC             4   //clock source error
#define EDETECTFAIL         5   //detect camera fail
#define EPMUPIN             6   //get pmu pin fail
#define ENORMALPIN          7   //get normal pin fail
#define ECFGPIN             8   //config pin fail
#define EI2CADAPTER         9   //get I2C adapter fail

typedef struct {
    __u32                   num;
    __camera_csi_connect_e  csi_connect_type;
    __camera_info_t         camera[MAX_CAMERA_NUM];
}__camera_detector_t;

#endif
