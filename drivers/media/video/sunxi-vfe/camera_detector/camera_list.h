#ifndef __CAMERA_LIST_H__
#define __CAMERA_LIST_H__

#include "camera_includes.h"

#define MAX_CAMERA_LIST_ITEM                21

#define CAMERA_LIST_ITEM_INIT(camera_name, i2c_addr_step, i2c_data_step, i2c_address, stby_on_pol, stby_off_pol, rst_on_pol, rst_off_pol, pwr_on_pol, pwr_off_pol) \
{\
    .name           = #camera_name,\
    .pwr_on         = camera_pwr_on_##camera_name,\
    .pwr_off        = camera_pwr_off_##camera_name,\
    .detect         = camera_detect_##camera_name,\
    .i2c_addr       = i2c_address,\
    .REG_ADDR_STEP  = i2c_addr_step,\
    .REG_DATA_STEP  = i2c_data_step,\
    .CSI_STBY_ON    = stby_on_pol,\
    .CSI_STBY_OFF   = stby_off_pol,\
    .CSI_RST_ON     = rst_on_pol,\
    .CSI_RST_OFF    = rst_off_pol,\
    .CSI_PWR_ON     = pwr_on_pol,\
    .CSI_PWR_OFF    = pwr_off_pol,\
    .need_detect    = FALSE,\
}\

typedef void (*__camera_power_f)(__u32 /* list index */, __camera_info_t *);

typedef __s32 (*__camera_detect_f)(__u32 /* list index */, struct i2c_adapter *);

typedef struct {
    __u8                *name;
    
    __u32               i2c_addr;
    __u32               REG_ADDR_STEP;
    __u32               REG_DATA_STEP;
    
    __u32               CSI_STBY_ON;
    __u32               CSI_STBY_OFF;
    __u32               CSI_RST_ON;
    __u32               CSI_RST_OFF;
    __u32               CSI_PWR_ON;
    __u32               CSI_PWR_OFF;
    
    __bool              need_detect;
    __camera_power_f    pwr_on;
    __camera_power_f    pwr_off;
    __camera_detect_f   detect;
}__camera_list_t;

struct regval_list {
	unsigned char reg_num[2];
	unsigned char value[2];
};

extern __camera_list_t camera_list[MAX_CAMERA_LIST_ITEM];

#endif
