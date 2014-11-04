#ifndef __HDMI_HW_INTERFACE_H_
#define __HDMI_HW_INTERFACE_H_

struct sunxi_audio_para
{
    __s32   audio_en;
    __s32   sample_rate;
    __s32   channel_num;

    __s32   CTS;
    __s32   ACR_N;
    __s32   CH_STATUS0;
    __s32   CH_STATUS1;
    __u8    data_raw;		/*0:pcm;1:raw*/
    __u8    sample_bit;/* 8/16/20/24/32 bit */
};

typedef void (*hdmi_udelay) (unsigned long us);

int api_set_func(hdmi_udelay udelay);
void sunxi_video_config(u32 vic);
void sunxi_audio_config(struct sunxi_audio_para *info);
int sunxi_get_hpd(void);
int sunxi_ddc_read(char cmd,char pointer,char offset,int nbyte,char * pbuf);
void sunxi_set_reg_base(unsigned int address);
int sunxi_hdmi_enter_lp(void);

#endif /* API_H_ */

