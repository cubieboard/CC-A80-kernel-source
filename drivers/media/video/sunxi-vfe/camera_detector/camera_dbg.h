#ifndef __CAMERA_DBG_H__
#define __CAMERA_DBG_H__

/*
0: no print
1: only inf && msg
2: inf && msg && err
*/

#define CAMERA_DBG_LEVEL 0

#if (CAMERA_DBG_LEVEL == 2)
#define detect_print(x...) printk("[camera_print][detect][L%d]", __LINE__);printk(x)
#define list_print(x...) printk("[camera_print][list][L%d]", __LINE__);printk(x)
#define camera_err(x...) printk("[camera_print][ERR][L%d]", __LINE__);printk(x)
#elif (CAMERA_DBG_LEVEL == 1)
#define detect_print(x...) printk("[camera_print][detect][L%d]", __LINE__);printk(x)
#define list_print(x...) printk("[camera_print][list][L%d]", __LINE__);printk(x)
#define camera_err(x...) 
#else
#define detect_print(x...) 
#define list_print(x...) 
#define camera_err(x...) 
#endif

#endif
