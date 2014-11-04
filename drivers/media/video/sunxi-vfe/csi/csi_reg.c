/*
 * sunxi csi register read/write interface
 * Author:raymonxiu
 */

#include <linux/kernel.h>
#include "csi_reg_i.h"
#include "csi_reg.h"

#if defined CONFIG_ARCH_SUN8IW1P1
#define ADDR_BIT_R_SHIFT 0
#define CLK_POL 0
#elif defined CONFIG_ARCH_SUN8IW3P1
#define ADDR_BIT_R_SHIFT 0
#define CLK_POL 1
#elif defined CONFIG_ARCH_SUN9IW1P1 
#define ADDR_BIT_R_SHIFT 2
#define CLK_POL 1
#elif defined CONFIG_ARCH_SUN8IW5P1
#define ADDR_BIT_R_SHIFT 0
#define CLK_POL 1
#elif defined CONFIG_ARCH_SUN8IW6P1
#define ADDR_BIT_R_SHIFT 2
#define CLK_POL 1
#elif defined CONFIG_ARCH_SUN8IW7P1
#define ADDR_BIT_R_SHIFT 2
#define CLK_POL 1
#elif defined CONFIG_ARCH_SUN8IW8P1
#define ADDR_BIT_R_SHIFT 2
#define CLK_POL 1
#endif


volatile CSI_EN_REG_t                    *csi_en_reg[MAX_CSI]                    ;
volatile CSI_IF_CFG_REG_t                *csi_if_cfg_reg[MAX_CSI]                ;
volatile CSI_CAP_REG_t                   *csi_cap_reg[MAX_CSI]                   ;
volatile CSI_SYNC_CNT_REG_t              *csi_sync_cnt_reg[MAX_CSI]              ;
volatile CSI_FIFO_THRS_REG_t             *csi_fifo_thrs_reg[MAX_CSI]             ;
volatile CSI_PTN_LEN_REG_t               *csi_ptn_len_reg[MAX_CSI]               ;
volatile CSI_PTN_ADDR_REG_t              *csi_ptn_addr_reg[MAX_CSI]              ;
volatile CSI_VER_REG_t                   *csi_ver_reg[MAX_CSI]                   ;
volatile CSI_CH_CFG_REG_t                *csi_ch_cfg_reg[MAX_CSI]                ;
volatile CSI_CH_SCALE_REG_t              *csi_ch_scale_reg[MAX_CSI]              ;
volatile CSI_CH_F0_BUFA_REG_t            *csi_ch_f0_bufa_reg[MAX_CSI]            ;
volatile CSI_CH_F1_BUFA_REG_t            *csi_ch_f1_bufa_reg[MAX_CSI]            ;
volatile CSI_CH_F2_BUFA_REG_t            *csi_ch_f2_bufa_reg[MAX_CSI]            ;
volatile CSI_CH_STA_REG_t                *csi_ch_sta_reg[MAX_CSI]                ;
volatile CSI_CH_INT_EN_REG_t             *csi_ch_int_en_reg[MAX_CSI]             ;
volatile CSI_CH_INT_STA_REG_t            *csi_ch_int_sta_reg[MAX_CSI]            ;
volatile CSI_CH_HSIZE_REG_t              *csi_ch_hsize_reg[MAX_CSI]              ;
volatile CSI_CH_VSIZE_REG_t              *csi_ch_vsize_reg[MAX_CSI]              ;
volatile CSI_CH_BUF_LEN_REG_t            *csi_ch_buf_len_reg[MAX_CSI]            ;
volatile CSI_CH_FLIP_SIZE_REG_t          *csi_ch_flip_size_reg[MAX_CSI]          ;
volatile CSI_CH_FRM_CLK_CNT_REG_t        *csi_ch_frm_clk_cnt_reg[MAX_CSI]        ;
volatile CSI_CH_ACC_ITNL_CLK_CNT_REG_t   *csi_ch_acc_intl_clk_cnt_reg[MAX_CSI]   ;


int csi_set_base_addr(unsigned int sel, unsigned int addr)
{
  if(sel > MAX_CSI-1)
    return -1;
  csi_en_reg[sel]                   =  (volatile CSI_EN_REG_t                  *)(addr + CSI_EN_REG_OFF                 );
  csi_if_cfg_reg[sel]               =  (volatile CSI_IF_CFG_REG_t              *)(addr + CSI_IF_CFG_REG_OFF             );
  csi_cap_reg[sel]                  =  (volatile CSI_CAP_REG_t                 *)(addr + CSI_CAP_REG_OFF                );
  csi_sync_cnt_reg[sel]             =  (volatile CSI_SYNC_CNT_REG_t            *)(addr + CSI_SYNC_CNT_REG_OFF           );
  csi_fifo_thrs_reg[sel]            =  (volatile CSI_FIFO_THRS_REG_t           *)(addr + CSI_FIFO_THRS_REG_OFF          );
  csi_ptn_len_reg[sel]              =  (volatile CSI_PTN_LEN_REG_t             *)(addr + CSI_PTN_LEN_REG_OFF            );
  csi_ptn_addr_reg[sel]             =  (volatile CSI_PTN_ADDR_REG_t            *)(addr + CSI_PTN_ADDR_REG_OFF           );
  csi_ver_reg[sel]                  =  (volatile CSI_VER_REG_t                 *)(addr + CSI_VER_REG_OFF                );
  csi_ch_cfg_reg[sel]               =  (volatile CSI_CH_CFG_REG_t              *)(addr + CSI_CH_CFG_REG_OFF             );
  csi_ch_scale_reg[sel]             =  (volatile CSI_CH_SCALE_REG_t            *)(addr + CSI_CH_SCALE_REG_OFF           );
  csi_ch_f0_bufa_reg[sel]           =  (volatile CSI_CH_F0_BUFA_REG_t          *)(addr + CSI_CH_F0_BUFA_REG_OFF         );
  csi_ch_f1_bufa_reg[sel]           =  (volatile CSI_CH_F1_BUFA_REG_t          *)(addr + CSI_CH_F1_BUFA_REG_OFF         );
  csi_ch_f2_bufa_reg[sel]           =  (volatile CSI_CH_F2_BUFA_REG_t          *)(addr + CSI_CH_F2_BUFA_REG_OFF         );
  csi_ch_sta_reg[sel]               =  (volatile CSI_CH_STA_REG_t              *)(addr + CSI_CH_STA_REG_OFF             );
  csi_ch_int_en_reg[sel]            =  (volatile CSI_CH_INT_EN_REG_t           *)(addr + CSI_CH_INT_EN_REG_OFF          );
  csi_ch_int_sta_reg[sel]           =  (volatile CSI_CH_INT_STA_REG_t          *)(addr + CSI_CH_INT_STA_REG_OFF         );
  csi_ch_hsize_reg[sel]             =  (volatile CSI_CH_HSIZE_REG_t            *)(addr + CSI_CH_HSIZE_REG_OFF           );
  csi_ch_vsize_reg[sel]             =  (volatile CSI_CH_VSIZE_REG_t            *)(addr + CSI_CH_VSIZE_REG_OFF           );
  csi_ch_buf_len_reg[sel]           =  (volatile CSI_CH_BUF_LEN_REG_t          *)(addr + CSI_CH_BUF_LEN_REG_OFF         );
  csi_ch_flip_size_reg[sel]         =  (volatile CSI_CH_FLIP_SIZE_REG_t        *)(addr + CSI_CH_FLIP_SIZE_REG_OFF       );
  csi_ch_frm_clk_cnt_reg[sel]       =  (volatile CSI_CH_FRM_CLK_CNT_REG_t      *)(addr + CSI_CH_FRM_CLK_CNT_REG_OFF     );
  csi_ch_acc_intl_clk_cnt_reg[sel]  =  (volatile CSI_CH_ACC_ITNL_CLK_CNT_REG_t *)(addr + CSI_CH_ACC_ITNL_CLK_CNT_REG_OFF);
  
  return 0;
}

/* open module */
void csi_enable(unsigned int sel)
{
  csi_en_reg[sel]->bits.csi_en = 1;
}

void csi_disable(unsigned int sel)
{
  csi_en_reg[sel]->bits.csi_en = 0;
}

/* configure */
void csi_if_cfg(unsigned int sel, struct csi_if_cfg *csi_if_cfg)
{
  csi_if_cfg_reg[sel]->bits.src_type = csi_if_cfg->src_type;
  
  if(csi_if_cfg->interface < 0x80)
  	csi_if_cfg_reg[sel]->bits.csi_if = csi_if_cfg->interface;
  else
  	csi_if_cfg_reg[sel]->bits.mipi_if = 1;
  	
  csi_if_cfg_reg[sel]->bits.if_data_width = csi_if_cfg->data_width;
}

void csi_timing_cfg(unsigned int sel, struct csi_timing_cfg *csi_tmg_cfg)
{
  csi_if_cfg_reg[sel]->bits.vref_pol = csi_tmg_cfg->vref; 
  csi_if_cfg_reg[sel]->bits.href_pol = csi_tmg_cfg->href;
	csi_if_cfg_reg[sel]->bits.clk_pol = (csi_tmg_cfg->sample==CLK_POL)?1:0;
  csi_if_cfg_reg[sel]->bits.field = csi_tmg_cfg->field;
}

void csi_fmt_cfg(unsigned int sel, unsigned int ch, struct csi_fmt_cfg *csi_fmt_cfg)
{
  (csi_ch_cfg_reg[sel] + ch*CSI_CH_OFF)->bits.input_fmt = csi_fmt_cfg->input_fmt;
  (csi_ch_cfg_reg[sel] + ch*CSI_CH_OFF)->bits.output_fmt = csi_fmt_cfg->output_fmt;
  (csi_ch_cfg_reg[sel] + ch*CSI_CH_OFF)->bits.field_sel = csi_fmt_cfg->field_sel;
  (csi_ch_cfg_reg[sel] + ch*CSI_CH_OFF)->bits.input_seq = csi_fmt_cfg->input_seq;
}

/* buffer */
void csi_set_buffer_address(unsigned int sel, unsigned int ch, enum csi_buf_sel buf, u64 addr)
{
	(csi_ch_f0_bufa_reg[sel] + ch*CSI_CH_OFF + buf)->dwval = addr >> ADDR_BIT_R_SHIFT;
}

u64 csi_get_buffer_address(unsigned int sel, unsigned int ch, enum csi_buf_sel buf)
{
  return ((csi_ch_f0_bufa_reg[sel] + ch*CSI_CH_OFF + (buf<<2))->dwval) << ADDR_BIT_R_SHIFT;
}

/* capture */
void csi_capture_start(unsigned int sel, unsigned int ch_total_num, enum csi_cap_mode csi_cap_mode)
{
  csi_cap_reg[sel]->dwval = (((ch_total_num == 4) ? csi_cap_mode:0)<<24) +
                       (((ch_total_num == 3) ? csi_cap_mode:0)<<16) + 
                       (((ch_total_num == 2) ? csi_cap_mode:0)<<8 )+
                       (((ch_total_num == 1) ? csi_cap_mode:0));
}

void csi_capture_stop(unsigned int sel, unsigned int ch_total_num, enum csi_cap_mode csi_cap_mode)
{
  csi_cap_reg[sel]->dwval = 0;
}


void csi_capture_get_status(unsigned int sel, unsigned int ch, struct csi_capture_status *status)
{
  status->picture_in_progress = (csi_ch_sta_reg[sel] + ch*CSI_CH_OFF)->bits.scap_sta;
  status->video_in_progress   = (csi_ch_sta_reg[sel] + ch*CSI_CH_OFF)->bits.vcap_sta;
//  status->field_status        = (csi_ch_sta_reg[sel] + ch*CSI_CH_OFF)->bits.field_sta;
}

/* size */
void csi_set_size(unsigned int sel, unsigned int ch, unsigned int length_h, unsigned int length_v, unsigned int buf_length_y, unsigned int buf_length_c)
{
  (csi_ch_hsize_reg[sel] + ch*CSI_CH_OFF)->bits.hor_len = length_h;
  (csi_ch_vsize_reg[sel] + ch*CSI_CH_OFF)->bits.ver_len = length_v; 
  (csi_ch_buf_len_reg[sel] + ch*CSI_CH_OFF)->bits.buf_len = buf_length_y;
  (csi_ch_buf_len_reg[sel] + ch*CSI_CH_OFF)->bits.buf_len_c = buf_length_c; 
}


/* offset */
void csi_set_offset(unsigned int sel, unsigned int ch, unsigned int start_h, unsigned int start_v)
{
  (csi_ch_hsize_reg[sel] + ch*CSI_CH_OFF)->bits.hor_start = start_h;
  (csi_ch_vsize_reg[sel] + ch*CSI_CH_OFF)->bits.ver_start = start_v;
}


/* interrupt */
void csi_int_enable(unsigned int sel, unsigned int ch, enum csi_int_sel interrupt)
{
  (csi_ch_int_en_reg[sel] + ch*CSI_CH_OFF)->dwval |= interrupt;
}

void csi_int_disable(unsigned int sel, unsigned int ch, enum csi_int_sel interrupt)
{
  (csi_ch_int_en_reg[sel] + ch*CSI_CH_OFF)->dwval &= ~interrupt;
}

void inline csi_int_get_status(unsigned int sel, unsigned int ch,struct csi_int_status *status)
{
  status->capture_done     = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.cd_pd;
  status->frame_done       = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.fd_pd;
  status->buf_0_overflow   = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.fifo0_of_pd;
  status->buf_1_overflow   = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.fifo1_of_pd;
  status->buf_2_overflow   = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.fifo2_of_pd;
  status->protection_error = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.prtc_err_pd;
  status->hblank_overflow  = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.hb_of_pd;
  status->vsync_trig       = (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->bits.vs_pd;
}

void inline csi_int_clear_status(unsigned int sel, unsigned int ch, enum csi_int_sel interrupt)
{
  (csi_ch_int_sta_reg[sel] + ch*CSI_CH_OFF)->dwval = interrupt;
}

