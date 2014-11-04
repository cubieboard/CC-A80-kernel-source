/*
 * sunxi csi register read/write interface register header file
 * Author:raymonxiu
 */
#ifndef __CSI__REG__I__H__
#define __CSI__REG__I__H__

#define u32 unsigned int

//
// Register Offset
//
#define CSI_EN_REG_OFF                      0X000
#define CSI_IF_CFG_REG_OFF                  0X004
#define CSI_CAP_REG_OFF                     0X008
#define CSI_SYNC_CNT_REG_OFF                0X00C
#define CSI_FIFO_THRS_REG_OFF               0X010
#define CSI_PTN_LEN_REG_OFF                 0X030
#define CSI_PTN_ADDR_REG_OFF                0X034
#define CSI_VER_REG_OFF                     0X03C
#define CSI_CH_CFG_REG_OFF                  0X044
#define CSI_CH_SCALE_REG_OFF                0X04C
#define CSI_CH_F0_BUFA_REG_OFF              0X050
#define CSI_CH_F1_BUFA_REG_OFF              0X058
#define CSI_CH_F2_BUFA_REG_OFF              0X060
#define CSI_CH_STA_REG_OFF                  0X06C
#define CSI_CH_INT_EN_REG_OFF               0X070
#define CSI_CH_INT_STA_REG_OFF              0X074
#define CSI_CH_HSIZE_REG_OFF                0X080
#define CSI_CH_VSIZE_REG_OFF                0X084
#define CSI_CH_BUF_LEN_REG_OFF              0X088
#define CSI_CH_FLIP_SIZE_REG_OFF            0X08C
#define CSI_CH_FRM_CLK_CNT_REG_OFF          0X090
#define CSI_CH_ACC_ITNL_CLK_CNT_REG_OFF     0X094

#define CSI_CH_OFF                          (0x0100>>2)

//
// Register Address
//
#define CSI_EN_REG_ADDR                     ( CSI_VBASE + CSI_EN_REG_OFF                  )   // CSI enable register
#define CSI_IF_CFG_REG_ADDR                 ( CSI_VBASE + CSI_IF_CFG_REG_OFF              )   // CSI Interface Configuration Register
#define CSI_CAP_REG_ADDR                    ( CSI_VBASE + CSI_CAP_REG_OFF                 )   // CSI Capture Register
#define CSI_SYNC_CNT_REG_ADDR               ( CSI_VBASE + CSI_SYNC_CNT_REG_OFF            )   // CSI Synchronization Counter Register
#define CSI_FIFO_THRS_REG_ADDR              ( CSI_VBASE + CSI_FIFO_THRS_REG_OFF           )   // CSI FIFO Threshold Register
#define CSI_PTN_LEN_REG_ADDR                ( CSI_VBASE + CSI_PTN_LEN_REG_OFF             )   // CSI Pattern Generation Length register
#define CSI_PTN_ADDR_REG_ADDR               ( CSI_VBASE + CSI_PTN_ADDR_REG_OFF            )   // CSI Pattern Generation Address register
#define CSI_VER_REG_ADDR                    ( CSI_VBASE + CSI_VER_REG_OFF                 )   // CSI Version Register
#define CSI_CH_CFG_REG_ADDR                 ( CSI_VBASE + CSI_CH_CFG_REG_OFF              )   // CSI Channel_0 configuration register
#define CSI_CH_SCALE_REG_ADDR               ( CSI_VBASE + CSI_CH_SCALE_REG_OFF            )   // CSI Channel_0 scale register
#define CSI_CH_F0_BUFA_REG_ADDR             ( CSI_VBASE + CSI_CH_F0_BUFA_REG_OFF          )   // CSI Channel_0 FIFO 0 output buffer-A address register
#define CSI_CH_F1_BUFA_REG_ADDR             ( CSI_VBASE + CSI_CH_F1_BUFA_REG_OFF          )   // CSI Channel_0 FIFO 1 output buffer-A address register
#define CSI_CH_F2_BUFA_REG_ADDR             ( CSI_VBASE + CSI_CH_F2_BUFA_REG_OFF          )   // CSI Channel_0 FIFO 2 output buffer-A address register
#define CSI_CH_STA_REG_ADDR                 ( CSI_VBASE + CSI_CH_STA_REG_OFF              )   // CSI Channel_0 status register
#define CSI_CH_INT_EN_REG_ADDR              ( CSI_VBASE + CSI_CH_INT_EN_REG_OFF           )   // CSI Channel_0 interrupt enable register
#define CSI_CH_INT_STA_REG_ADDR             ( CSI_VBASE + CSI_CH_INT_STA_REG_OFF          )   // CSI Channel_0 interrupt status register
#define CSI_CH_HSIZE_REG_ADDR               ( CSI_VBASE + CSI_CH_HSIZE_REG_OFF            )   // CSI Channel_0 horizontal size register
#define CSI_CH_VSIZE_REG_ADDR               ( CSI_VBASE + CSI_CH_VSIZE_REG_OFF            )   // CSI Channel_0 vertical size register
#define CSI_CH_BUF_LEN_REG_ADDR             ( CSI_VBASE + CSI_CH_BUF_LEN_REG_OFF          )   // CSI Channel_0 line buffer length register
#define CSI_CH_FLIP_SIZE_REG_ADDR           ( CSI_VBASE + CSI_CH_FLIP_SIZE_REG_OFF        )   // CSI Channel_0 flip size register
#define CSI_CH_FRM_CLK_CNT_REG_ADDR         ( CSI_VBASE + CSI_CH_FRM_CLK_CNT_REG_OFF      )   // CSI Channel_0 frame clock counter register
#define CSI_CH_ACC_ITNL_CLK_CNT_REG_ADDR    ( CSI_VBASE + CSI_CH_ACC_ITNL_CLK_CNT_REG_OFF )   // CSI Channel_0 accumulated and internal clock counter register


// 
// Detail information of registers
//

typedef union
{
  u32 dwval;
  struct
  {
    u32 csi_en          :  1 ;    // Default: 0; 
    u32 ptn_gen_en      :  1 ;    // Default: 0; 
    u32 clk_cnt         :  1 ;    // Default: 0; 
    u32 clk_cnt_spl     :  1 ;    // Default: 0; 
    u32 ptn_start       :  1 ;    // Default: 0; 
    u32 res0            : 25 ;    // Default: ; 
    u32 ver_en          :  1 ;    // Default: 0x0; 
    u32 res1            :  1 ;    // Default: ; 
  } bits;
} CSI_EN_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 csi_if          :  5 ;    // Default: 0; 
    u32 res0            :  2 ;    // Default: ; 
    u32 mipi_if         :  1 ;    // Default: 0; 
    u32 if_data_width   :  2 ;    // Default: 0; 
    u32 if_bus_seq      :  2 ;    // Default: 0; 
    u32 res1            :  4 ;    // Default: ; 
    u32 clk_pol         :  1 ;    // Default: 1; 
    u32 href_pol        :  1 ;    // Default: 0; 
    u32 vref_pol        :  1 ;    // Default: 1; 
    u32 field           :  1 ;    // Default: 0; 
    u32 fps_ds          :  1 ;    // Default: 0; 
    u32 src_type        :  1 ;    // Default: 0; 
    u32 dst             :  1 ;    // Default: 0; 
    u32 res2            :  9 ;    // Default: ; 
  } bits;
} CSI_IF_CFG_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 ch0_scap_on     :  1 ;    // Default: 0; 
    u32 ch0_vcap_on     :  1 ;    // Default: 0; 
    u32 res0            :  6 ;    // Default: ; 
    u32 ch1_scap_on     :  1 ;    // Default: 0; 
    u32 ch1_vcap_on     :  1 ;    // Default: 0; 
    u32 res1            :  6 ;    // Default: ; 
    u32 ch2_scap_on     :  1 ;    // Default: 0; 
    u32 ch2_vcap_on     :  1 ;    // Default: 0; 
    u32 res2            :  6 ;    // Default: ; 
    u32 ch3_scap_on     :  1 ;    // Default: 0; 
    u32 ch3_vcap_on     :  1 ;    // Default: 0; 
    u32 res3            :  6 ;    // Default: ; 
  } bits;
} CSI_CAP_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 sync_cnt        : 24 ;    // Default: 0; 
    u32 res0            :  8 ;    // Default: ; 
  } bits;
} CSI_SYNC_CNT_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 fifo_thrs       : 12 ;    // Default: 0x400; 
    u32 res0            : 20 ;    // Default: ; 
  } bits;
} CSI_FIFO_THRS_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 ptn_len              ;    // Default: 0x0; 
  } bits;
} CSI_PTN_LEN_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 ptn_addr             ;    // Default: 0x0; 
  } bits;
} CSI_PTN_ADDR_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 ver                  ;    // Default: ; 
  } bits;
} CSI_VER_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 res0            :  8 ;    // Default: ; 
    u32 input_seq       :  2 ;    // Default: 2; 
    u32 field_sel       :  2 ;    // Default: 0; 
    u32 hflip_en        :  1 ;    // Default: 0; 
    u32 vflip_en        :  1 ;    // Default: 0; 
    u32 res1            :  2 ;    // Default: ; 
    u32 output_fmt      :  4 ;    // Default: 0; 
    u32 input_fmt       :  4 ;    // Default: 3; 
    u32 pad_val         :  8 ;    // Default: 0; 
  } bits;
} CSI_CH_CFG_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 quart_en        :  1 ;    // Default: 0; 
    u32 res0            : 31 ;    // Default: ; 
  } bits;
} CSI_CH_SCALE_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 c0f0_bufa            ;    // Default: 0; 
  } bits;
} CSI_CH_F0_BUFA_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 c0f1_bufa            ;    // Default: 0; 
  } bits;
} CSI_CH_F1_BUFA_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 c0f2_bufa            ;    // Default: 0; 
  } bits;
} CSI_CH_F2_BUFA_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 scap_sta        :  1 ;    // Default: 0; 
    u32 vcap_sta        :  1 ;    // Default: 0; 
    u32 field_sta       :  1 ;    // Default: 0; 
    u32 res0            : 29 ;    // Default: ; 
  } bits;
} CSI_CH_STA_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 cd_int_en       :  1 ;    // Default: 0; 
    u32 fd_int_en       :  1 ;    // Default: 0; 
    u32 fifo0_of_int_en :  1 ;    // Default: 0; 
    u32 fifo1_of_int_en :  1 ;    // Default: 0; 
    u32 fifo2_of_int_en :  1 ;    // Default: 0; 
    u32 prtc_err_int_en :  1 ;    // Default: 0; 
    u32 hb_of_int_en    :  1 ;    // Default: 0; 
    u32 vs_int_en       :  1 ;    // Default: 0; 
    u32 res0            : 24 ;    // Default: ; 
  } bits;
} CSI_CH_INT_EN_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 cd_pd           :  1 ;    // Default: 0; 
    u32 fd_pd           :  1 ;    // Default: 0; 
    u32 fifo0_of_pd     :  1 ;    // Default: 0; 
    u32 fifo1_of_pd     :  1 ;    // Default: 0; 
    u32 fifo2_of_pd     :  1 ;    // Default: 0; 
    u32 prtc_err_pd     :  1 ;    // Default: 0; 
    u32 hb_of_pd        :  1 ;    // Default: 0; 
    u32 vs_pd           :  1 ;    // Default: 0; 
    u32 res0            : 24 ;    // Default: ; 
  } bits;
} CSI_CH_INT_STA_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 hor_start       : 13 ;    // Default: 0; 
    u32 res0            :  3 ;    // Default: ; 
    u32 hor_len         : 13 ;    // Default: 500; 
    u32 res1            :  3 ;    // Default: ; 
  } bits;
} CSI_CH_HSIZE_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 ver_start       : 13 ;    // Default: 0; 
    u32 res0            :  3 ;    // Default: ; 
    u32 ver_len         : 13 ;    // Default: 1E0; 
    u32 res1            :  3 ;    // Default: ; 
  } bits;
} CSI_CH_VSIZE_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 buf_len         : 13 ;    // Default: 280; 
    u32 res0            :  3 ;    // Default: ; 
    u32 buf_len_c       : 13 ;    // Default: 140; 
    u32 res1            :  3 ;    // Default: ; 
  } bits;
} CSI_CH_BUF_LEN_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 valid_len       : 13 ;    // Default: 280; 
    u32 res0            :  3 ;    // Default: ; 
    u32 ver_len         : 13 ;    // Default: 1E0; 
    u32 res1            :  3 ;    // Default: ; 
  } bits;
} CSI_CH_FLIP_SIZE_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 frm_clk_cnt     : 24 ;    // Default: 0; 
    u32 res0            :  8 ;    // Default: ; 
  } bits;
} CSI_CH_FRM_CLK_CNT_REG_t;

typedef union
{
  u32 dwval;
  struct
  {
    u32 itnl_clk_cnt    : 24 ;    // Default: 0; 
    u32 acc_clk_cnt     :  8 ;    // Default: 0; 
  } bits;
} CSI_CH_ACC_ITNL_CLK_CNT_REG_t;


//------------------------------------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------------------------------------

#endif // __CSI__REG__I__H__
