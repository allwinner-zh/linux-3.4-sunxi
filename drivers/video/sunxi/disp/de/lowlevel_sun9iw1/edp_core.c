#include "edp_core.h"

//static unsigned int dp_base                  ;
static unsigned int dp_int_mask              ;
static unsigned int dp_int_status            ;
static unsigned int dp_line_triger           ;
static unsigned int dp_ctrl                  ;
static unsigned int dp_start_dly             ;
static unsigned int dp_general_timer         ;
static unsigned int dp_scramble_seed         ;
static unsigned int dp_vid_ctrl              ;
static unsigned int dp_vid_idel_ctrl         ;
static unsigned int dp_vid_tu_ctrl           ;
static unsigned int dp_total_symbol_per_line ;
static unsigned int dp_hwidth                ;
static unsigned int dp_vheight               ;
static unsigned int dp_htotal                ;
static unsigned int dp_vtotal                ;
static unsigned int dp_hstart                ;
static unsigned int dp_vstart                ;
static unsigned int dp_hsp_hsw               ;
static unsigned int dp_vsp_vsw               ;
static unsigned int dp_misc                  ;
static unsigned int dp_mvid                  ;
static unsigned int dp_nvid                  ;
static unsigned int dp_training_ctrl         ;
static unsigned int dp_aux_ctrl              ;
static unsigned int dp_aux_addr              ;
static unsigned int dp_aux_rw_len            ;
static unsigned int dp_aux_fifo_status       ;
static unsigned int dp_aux_rfifo             ;
static unsigned int dp_aux_wfifo             ;
static unsigned int dp_aux_status            ;
static unsigned int ana_pllbus0              ;
static unsigned int ana_pllbus1              ;
static unsigned int ana_pllbus2              ;
static unsigned int ana_pllbus3              ;
static unsigned int ana_drvbus0              ;
static unsigned int ana_drvbus1              ;
static unsigned int ana_drvbus2              ;
static unsigned int ana_drvbus3              ;
static unsigned int ana_drvbus4              ;
static unsigned int ana_drvbus5              ;
static unsigned int ana_drvin                ;

static char dp_rx_info[256];
static char fp_tx_buf[16];
static char fp_rx_buf[16];
static int dp_rev,dp_enhanced_frame_cap,dp_max_link_rate,dp_max_lane_count;
static int training_aux_rd_interval;
static int eDP_capable;
static int glb_lane_cnt;
static unsigned int glb_bit_rate;
unsigned int glb_swing_lv,glb_pre_lv,glb_postcur2_lv = 0;

struct video_para
{
  int mvid;
  int nvid;
  int fill;
};

struct tx_current
{
  unsigned int mc;
  unsigned int tap1;
  unsigned int tap2;
  unsigned int tap1_op;
  unsigned int tap2_op;
};

static struct tx_current rbr_tbl[] =
{
  //mc,tap1,tap2,tap1_op,tap2_op

  //swing lv0, pre l0~l3
  {0xffff,0x03030303,0x0000,0x00000000,0x0000},
  {0xffff,0x0d0d0d0d,0x0000,0x05050505,0x0000},
  {0x7777,0x1f1f1f1f,0x0000,0x0a0a0a0a,0x0000},
  {0xffff,0x1f1f1f1f,0xbbbb,0x14141414,0x0000},
  //swing lv1, pre l0~l2
  {0xffff,0x0d0d0d0d,0x0000,0x00000000,0x0000},
  {0xcccc,0x1f1f1f1f,0x0000,0x08080808,0x0000},
  {0xffff,0x1f1f1f1f,0xbbbb,0x0f0f0f0f,0x0000},
  {0xffff,0x1f1f1f1f,0xbbbb,0x0f0f0f0f,0x0000},//not allowed
  //swing lv2, pre l0~l1
  {0x7777,0x1f1f1f1f,0x0000,0x00000000,0x0000},
  {0xffff,0x1f1f1f1f,0xbbbb,0x0a0a0a0a,0x0000},
  {0xffff,0x1f1f1f1f,0xbbbb,0x0a0a0a0a,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xbbbb,0x0a0a0a0a,0x0000}, //not allowed
  //swing l3, pre l0
  {0xffff,0x1f1f1f1f,0xbbbb,0x00000000,0x0000}, //optional
  {0xffff,0x1f1f1f1f,0xbbbb,0x00000000,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xbbbb,0x00000000,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xbbbb,0x00000000,0x0000}, //not allowed
};

static struct tx_current hbr_tbl[] =
{
  //mc,tap1,tap2,tap1_op,tap2_op

  //swing lv0, pre l0~l3
  {0xffff,0x05050505,0x0000,0x02020202,0x0000},
  {0xffff,0x0f0f0f0f,0x0000,0x06060606,0x0000},
  {0x9999,0x1f1f1f1f,0x0000,0x0b0b0b0b,0x0000},
  {0xffff,0x1f1f1f1f,0xdddd,0x15151515,0x0000},
  //swing lv1, pre l0~l2
  {0xffff,0x0f0f0f0f,0x0000,0x02020202,0x0000},
  {0xeeee,0x1f1f1f1f,0x0000,0x09090909,0x0000},
  {0xffff,0x1f1f1f1f,0xdddd,0x10101010,0x0000},
  {0xffff,0x1f1f1f1f,0xdddd,0x10101010,0x0000}, //not allowed
  //swing lv2, pre l0~l1
  {0x9999,0x1f1f1f1f,0x0000,0x02020202,0x0000},
  {0xffff,0x1f1f1f1f,0xffff,0x0d0d0d0d,0x0000},
  {0xffff,0x1f1f1f1f,0xffff,0x0d0d0d0d,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xffff,0x0d0d0d0d,0x0000}, //not allowed
  //swing l3, pre l0
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //optional
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //not allowed
};

static struct tx_current hbr2_tbl[] =
{
  //mc,tap1,tap2,tap1_op,tap2_op

  //swing lv0, pre l0~l3
  {0xffff,0x05050505,0x0000,0x02020202,0x0000},
  {0xffff,0x0f0f0f0f,0x0000,0x06060606,0x0000},
  {0x9999,0x1f1f1f1f,0x0000,0x0b0b0b0b,0x0000},
  {0xffff,0x1f1f1f1f,0xdddd,0x15151515,0x0000},
  //swing lv1, pre l0~l2
  {0xffff,0x0f0f0f0f,0x0000,0x02020202,0x0000},
  {0xeeee,0x1f1f1f1f,0x0000,0x09090909,0x0000},
  {0xffff,0x1f1f1f1f,0xdddd,0x10101010,0x0000},
  {0xffff,0x1f1f1f1f,0xdddd,0x10101010,0x0000}, //not allowed
  //swing lv2, pre l0~l1
  {0x9999,0x1f1f1f1f,0x0000,0x02020202,0x0000},
  {0xffff,0x1f1f1f1f,0xffff,0x0d0d0d0d,0x0000},
  {0xffff,0x1f1f1f1f,0xffff,0x0d0d0d0d,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xffff,0x0d0d0d0d,0x0000}, //not allowed
  //swing l3, pre l0
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //optional
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //not allowed
  {0xffff,0x1f1f1f1f,0xffff,0x04040404,0x0000}, //not allowed
};

void set_base_addr(unsigned int edp_baseaddr)
{
  dp_int_mask               =     (edp_baseaddr + 0x004);
  dp_int_status             =     (edp_baseaddr + 0x008);
  dp_line_triger            =     (edp_baseaddr + 0x00c);
  dp_ctrl                   =     (edp_baseaddr + 0x010);
  dp_start_dly              =     (edp_baseaddr + 0x014);
  dp_general_timer          =     (edp_baseaddr + 0x018);
  dp_scramble_seed          =     (edp_baseaddr + 0x01c);
  dp_vid_ctrl               =     (edp_baseaddr + 0x020);
  dp_vid_idel_ctrl          =     (edp_baseaddr + 0x024);
  dp_vid_tu_ctrl            =     (edp_baseaddr + 0x028);
  dp_total_symbol_per_line  =     (edp_baseaddr + 0x030);
  dp_hwidth                 =     (edp_baseaddr + 0x040);
  dp_vheight                =     (edp_baseaddr + 0x044);
  dp_htotal                 =     (edp_baseaddr + 0x048);
  dp_vtotal                 =     (edp_baseaddr + 0x04c);
  dp_hstart                 =     (edp_baseaddr + 0x050);
  dp_vstart                 =     (edp_baseaddr + 0x054);
  dp_hsp_hsw                =     (edp_baseaddr + 0x058);
  dp_vsp_vsw                =     (edp_baseaddr + 0x05c);
  dp_misc                   =     (edp_baseaddr + 0x060);
  dp_mvid                   =     (edp_baseaddr + 0x064);
  dp_nvid                   =     (edp_baseaddr + 0x068);
  dp_training_ctrl          =     (edp_baseaddr + 0x200);
  dp_aux_ctrl               =     (edp_baseaddr + 0x300);
  dp_aux_addr               =     (edp_baseaddr + 0x304);
  dp_aux_rw_len             =     (edp_baseaddr + 0x308);
  dp_aux_fifo_status        =     (edp_baseaddr + 0x30c);
  dp_aux_rfifo              =     (edp_baseaddr + 0x310);
  dp_aux_wfifo              =     (edp_baseaddr + 0x314);
  dp_aux_status             =     (edp_baseaddr + 0x318);
  ana_pllbus0               =     (edp_baseaddr + 0x500);
  ana_pllbus1               =     (edp_baseaddr + 0x504);
  ana_pllbus2               =     (edp_baseaddr + 0x508);
  ana_pllbus3               =     (edp_baseaddr + 0x50c);
  ana_drvbus0               =     (edp_baseaddr + 0x520);
  ana_drvbus1               =     (edp_baseaddr + 0x524);
  ana_drvbus2               =     (edp_baseaddr + 0x528);
  ana_drvbus3               =     (edp_baseaddr + 0x52c);
  ana_drvbus4               =     (edp_baseaddr + 0x530);
  ana_drvbus5               =     (edp_baseaddr + 0x534);
  ana_drvin                 =     (edp_baseaddr + 0x538);
}

void edp_delay_ms(int t)
{
  int reg;
  EDP_SBIT32(dp_ctrl, BIT31);
  EDP_WUINT32(dp_general_timer,0x00000000);
  EDP_WUINT32(dp_int_status   ,0xffffffff);
  EDP_WUINT32(dp_general_timer,(1<<29) + (15<<16) + (0x10000 - t) );
  do {
    reg = EDP_RUINT32(dp_int_status);
  }while((reg & 0x20) == 0);

  EDP_WUINT32(dp_general_timer,0x00000000);  //close timer
  //EDP_CBIT32(dp_ctrl, BIT31);
}

void aux_cfg(void)
{
  EDP_WUINT32(dp_aux_ctrl,0x00000000);
}

int aux_wr(int addr,int len,char* buf)
{
  int i;

  if(len <= 0)
  {
    return 0;
  }

  EDP_WUINT32(dp_aux_addr,addr);
  EDP_WUINT32(dp_aux_rw_len,(len -1) << 8);
  EDP_SBIT32(dp_aux_fifo_status,BIT15);
  EDP_CBIT32(dp_aux_fifo_status,BIT15);
  for(i=0;i < len;i++)
  {
    EDP_WUINT8(dp_aux_wfifo,buf[i]);
  }
  EDP_WUINT32(dp_aux_ctrl,0x80000000);

  edp_delay_ms(1);

  if(EDP_RUINT32(dp_aux_ctrl)&0x80000000)
    return -1;

  return len - (EDP_RUINT32(dp_aux_fifo_status)&0x1f);
}

int aux_rd(int addr,int len,char* buf)
{
  int i,cnt;

  if(len <= 0)
  {
    return 0;
  }

  EDP_WUINT32(dp_aux_addr,addr);
  EDP_WUINT32(dp_aux_rw_len,(len -1) );
  EDP_SBIT32(dp_aux_fifo_status,BIT31);
  EDP_CBIT32(dp_aux_fifo_status,BIT31);
  EDP_WUINT32(dp_aux_ctrl,0x02000000);
  EDP_WUINT32(dp_aux_ctrl,0x82000000);

  edp_delay_ms(1);

  if(EDP_RUINT32(dp_aux_ctrl)&0x80000000)
    return -1;

  cnt = ( EDP_RUINT32(dp_aux_fifo_status) >> 16) & 0x1f;

  for(i = 0; i < cnt; i++)
  {
    buf[i] = EDP_RUINT8(dp_aux_rfifo);
  }

  return cnt;
}

int aux_get_last_error(void)
{
  return EDP_RUINT32(dp_aux_status);
}

static int phy_pll_set(unsigned int bit_rate)
{
  if(bit_rate == 5400)
  {
    EDP_WUINT32(ana_pllbus0,(0xe<<28) + (1<<25) + (1<<24) +(2<<16) + (2<<11)+ (4<<8) + (0x5a<<0) ); //pll 5.4G
  }
  else if(bit_rate == 2700)
  {
    EDP_WUINT32(ana_pllbus0,(0xe<<28) + (1<<25) + (1<<24) +(2<<16) + (2<<11)+ (4<<8) + (0x2d<<0) ); //pll 2.7G
  }
  else
  {
    EDP_WUINT32(ana_pllbus0,(0xe<<28) + (1<<25) + (0<<24) +(2<<16) + (2<<11)+ (4<<8) + (0x2d<<0) ); //pll 1.62G
  }

  return 0;
}



int phy_cfg(unsigned char swing_lv, unsigned char preemp_lv, unsigned char postcur2_lv)
{
  unsigned int mc,tap1,tap2,tap1_op,tap2_op;
  unsigned char index;

  index = (swing_lv + 1) * (preemp_lv + 1) - 1;

  switch(glb_bit_rate)
  {
      case 1620:
        mc      = rbr_tbl[index].mc;
        tap1    = rbr_tbl[index].tap1;
        tap2    = rbr_tbl[index].tap2;
        tap1_op = rbr_tbl[index].tap1_op;
        tap2_op = rbr_tbl[index].tap2_op;
        break;
      case 2700:
        mc      = hbr_tbl[index].mc;
        tap1    = hbr_tbl[index].tap1;
        tap2    = hbr_tbl[index].tap2;
        tap1_op = hbr_tbl[index].tap1_op;
        tap2_op = hbr_tbl[index].tap2_op;
        break;
      case 5400:
        mc      = hbr2_tbl[index].mc;
        tap1    = hbr2_tbl[index].tap1;
        tap2    = hbr2_tbl[index].tap2;
        tap1_op = hbr2_tbl[index].tap1_op;
        tap2_op = hbr2_tbl[index].tap2_op;
        break;
      default:
        mc      = hbr_tbl[index].mc;
        tap1    = hbr_tbl[index].tap1;
        tap2    = hbr_tbl[index].tap2;
        tap1_op = hbr_tbl[index].tap1_op;
        tap2_op = hbr_tbl[index].tap2_op;
        break;
  }

  //mc
  EDP_CBIT32(ana_drvbus3, 0xffff0000);
  EDP_SBIT32(ana_drvbus3, ( mc >> ( (MAX_LANE_NUM - glb_lane_cnt) << 2) ) << 16 );

  //tap1
  EDP_CBIT32(ana_drvbus5, 0xffffffff);
  EDP_SBIT32(ana_drvbus5, tap1 >> ( (MAX_LANE_NUM - glb_lane_cnt) << 3) );

  //tap2
  EDP_CBIT32(ana_drvbus2, 0x0000ffff);
  EDP_SBIT32(ana_drvbus2, tap2 >> ( (MAX_LANE_NUM - glb_lane_cnt) << 2) );

  //tap1 option
  EDP_CBIT32(ana_drvbus4, 0xffffffff);
  EDP_SBIT32(ana_drvbus4, tap1_op >> ( (MAX_LANE_NUM - glb_lane_cnt) << 3) );

  //tap2 option
  EDP_CBIT32(ana_drvbus2, 0xffff0000);
  EDP_SBIT32(ana_drvbus2, tap2_op >> ( (MAX_LANE_NUM - glb_lane_cnt) << 2) << 16 );

  //tap1 enable
  EDP_CBIT32(ana_drvbus3, 0x0000000f);
  EDP_SBIT32(ana_drvbus3, 0xf >> (MAX_LANE_NUM - glb_lane_cnt) );

  //tap2 enable
  EDP_CBIT32(ana_drvbus3, 0x000000f0);
  EDP_SBIT32(ana_drvbus3, ( 0xf >> (MAX_LANE_NUM - glb_lane_cnt) ) << 4 );

  //tx
  EDP_CBIT32(ana_drvbus0, 0x000f0000);
  EDP_SBIT32(ana_drvbus0, ( 0xf >> (MAX_LANE_NUM - glb_lane_cnt) ) << 16 );

  EDP_CBIT32(ana_drvbus0, 0x0000001f);
  EDP_SBIT32(ana_drvbus0, ( 0xf >> (MAX_LANE_NUM - glb_lane_cnt) ) | 0x10);

  EDP_CBIT32(ana_drvbus0, 0x0f000000);
  EDP_SBIT32(ana_drvbus0, ( 0xf >> (MAX_LANE_NUM - glb_lane_cnt) ) << 24 );

  EDP_CBIT32(ana_drvbus0, 0x00f0c000);
  EDP_SBIT32(ana_drvbus0, ( ( 0xf >> (MAX_LANE_NUM - glb_lane_cnt) ) << 20 ) | 0xc000);

  EDP_CBIT32(ana_drvbus1, 0x00cc0000);
  EDP_SBIT32(ana_drvbus1, (0x2 << 22) | (0x2 << 18) );

  return 0;
}

int phy_res_cal()
{
  //rc
  EDP_SBIT32(ana_drvbus0,0x10000000);
  EDP_SBIT32(ana_drvbus1,0x00100000);
  EDP_SBIT32(ana_drvbus0,0x00000100);
  EDP_SBIT32(ana_drvbus0,0x00002000);
  EDP_SBIT32(ana_drvbus0,0x00001000);
  edp_delay_ms(1);
  EDP_WUINT32(ana_drvin,0x100);
#if 0
	to_cnt = 0;
  while( ( EDP_RUINT32(ana_drvin)&0x100 ) != 0x100)
  {
    edp_delay_ms(1);
    to_cnt ++;
    EDP_WUINT32(ana_drvin,0x100);
    if(to_cnt == 3)
    {
      EDP_CBIT32(ana_drvbus0,0x00002000);
      return -1;
    }
  }
#endif
  EDP_CBIT32(ana_drvbus0,0x00002000);
  return 0;
}

int phy_init(unsigned int lane_cnt, unsigned int bit_rate)
{
  //pu
  EDP_WUINT32(ana_drvbus0, 0x0);
  EDP_SBIT32(ana_drvbus0,0x40000000);
  EDP_SBIT32(ana_drvbus0,0x80000000);
  EDP_SBIT32(ana_drvbus0,0x20000000);

  if( (lane_cnt > MAX_LANE_NUM) || (lane_cnt == 0) )
    return -1;

  phy_pll_set(bit_rate);
  glb_bit_rate = bit_rate;
  glb_lane_cnt = lane_cnt;

  return 0;
}

void phy_exit()
{
  //pd
  EDP_CBIT32(ana_drvbus0,0x1fffffff);
  EDP_SBIT32(ana_drvbus0,0x20000000);
  EDP_SBIT32(ana_drvbus0,0x80000000);
  EDP_SBIT32(ana_drvbus0,0x40000000);
  EDP_CBIT32(ana_pllbus0, BIT31 | BIT30 | BIT29);
  glb_bit_rate = 0;
  glb_lane_cnt = 0;
}

int dp_ctrl_init()
{
  int lane_sel;
  EDP_WUINT32(dp_ctrl,        0x00000000);
  EDP_WUINT32(dp_int_mask,    0x00000000);
  EDP_WUINT32(dp_int_status,  0xffffffff);
  EDP_WUINT32(dp_line_triger, 1<<16     );

  //EDP_WUINT32(EDP_BASEADDR + 0x3f0,0x00000010);   // data output invert
  if(glb_lane_cnt == 1)
    lane_sel = 0;
  else if(glb_lane_cnt == 2)
    lane_sel = 1;
  else if(glb_lane_cnt == 4)
    lane_sel = 2;
  else
    lane_sel = 2;

  EDP_WUINT32(dp_ctrl,(lane_sel<<26));
  EDP_SBIT32(dp_ctrl,BIT31);
  return 0;
}

void dp_ctrl_exit()
{
  EDP_CBIT32(dp_ctrl,BIT31);
}

//call after phy_init and phy_cfg()
int dp_sink_init()
{
  int i,blk,ret;

  // link configuration
  blk = 8;
  for(i=0;i<16/blk;i++)           //read 16 Byte dp sink capability
  {
    ret = aux_rd(0x0000 + i* blk,16,dp_rx_info+ i* blk);
    if(ret == -1)
      return ret;
  }

  fp_tx_buf[0] = 0x01;
  ret = aux_wr(0x00600,1,fp_tx_buf);      //set sink to D0 mode.
  if(ret == -1)
    return ret;

  dp_rev            = dp_rx_info[0]&0x0f;
  dp_max_link_rate  = dp_rx_info[1]* 270000000;
  dp_max_lane_count = dp_rx_info[2]&0x0f;

  if(dp_rx_info[0x0d]!= 0)
  {
    eDP_capable = 1;
  }
  else
  {
    eDP_capable = 0;
  }

  if(dp_rx_info[0x0e]== 0)
  {
    training_aux_rd_interval = 1;
  }
  else
  {
    training_aux_rd_interval = 4 *dp_rx_info[0x0e];
  }

  dp_enhanced_frame_cap =  0;
  if( (dp_rev == 1) && (dp_rx_info[2]&0x80) )
  {
    dp_enhanced_frame_cap =  1;
  }
  else if (dp_rev == 2)
  {
    dp_enhanced_frame_cap =  1;
  }

  // link configuration
  //fp_tx_buf[0] = 0x01;
  //aux_wr(0x00600,1,fp_tx_buf);      //set sink to D0 mode.

  fp_tx_buf[0] = glb_bit_rate / 270;
  ret = aux_wr(0x00100, 1, fp_tx_buf);      //set bandwidth  = 1.62G
  if(ret == -1)
    return ret;

  if(dp_enhanced_frame_cap)
  {
    fp_tx_buf[0] = 0x80 | glb_lane_cnt;
  }
  else
  {
    fp_tx_buf[0] = 0x00 | glb_lane_cnt;
  }
  ret = aux_wr(0x00101, 1, fp_tx_buf);      //set lane conut
  if(ret == -1)
    return ret;

  return 0;
}

void dp_sink_exit()
{

}

static int cdiv(int a, int b)
{
	while(1) {
		if(a == b)
			break;

		else if(a > b)
			a = a - b;
		else
			b = b - a;
	}

	return a;
}

//static int para_convert(int lanes,int color_bits,int ht_per_lane,int ht, struct video_para * result)
//{
//   int Mvid,Nvid;
//   int K;
//   float ratio;
//
//   K        = cdiv(ht,ht_per_lane);
//   Mvid     = ht          / K;
//   Nvid     = ht_per_lane / K;
//
//   ratio    = 3.0 * color_bits * Mvid /(lanes * 8 * Nvid);
//
//   if( ratio >= 1)
//     return -1;
//
//
//   result->fill  = TU_SIZE  - TU_SIZE * ratio;//(3.0 * color_bits * Mvid)/(lanes * 8 * Nvid);
//
//   result->mvid  = Mvid;
//   result->nvid  = Nvid;
//
//   return 0;
//}

static int para_convert(int lanes,int color_bits,int ht_per_lane,int ht, struct video_para * result)
{
  int Mvid,Nvid;
  int K;
  unsigned int ratio;

  K = cdiv(ht,ht_per_lane);
  Mvid = ht / K;
  Nvid = ht_per_lane / K;

  ratio = ((3 * color_bits * Mvid << 10) + (lanes * 8 * Nvid)/2 )/(lanes * 8 * Nvid);

  if( ratio >= 0x400) {
    return -1;
  }

  result->fill  = (TU_SIZE * (0x400 - ratio) ) >> 10;

  result->mvid  = Mvid;
  result->nvid  = Nvid;

  return 0;
}


//call before video_set
//src = 0 normal
//src = 1 color bar
//src = 2 mosaic
void dp_video_gen_src(unsigned int src)
{
  EDP_CBIT32(dp_vid_ctrl              , 3 );
  EDP_SBIT32(dp_vid_ctrl              , src );
}

//call after dp_ctrl_init and dp_sink_init
//return 0 successful
//return -1 fps or vt error
//return -2 symbol per line error
//return -3 tu ratio error
int dp_video_set(struct video_timming *tmg)
{
  struct video_para para;
  unsigned int total_symbol_per_line;
  int ret;

  total_symbol_per_line = ((tmg->ht- tmg->ht/8) * 4 / glb_lane_cnt);

  if(tmg->fps == 0 || tmg->vt == 0)
  {
    return -1;
  }
  else
  {
    total_symbol_per_line = (glb_bit_rate * 100000 / (tmg->vt * tmg->fps));
    if(total_symbol_per_line < (tmg->ht * 24 / (glb_lane_cnt * 10)))
      return -2;
  }

  ret = para_convert(glb_lane_cnt,8,total_symbol_per_line,tmg->ht,&para);
  if(ret != 0)
    return -3;

  EDP_CBIT32(dp_vid_ctrl              , (3<<2) );
  EDP_SBIT32(dp_vid_ctrl              , (1<<2) );
//  EDP_WUINT32(dp_start_dly             , STA_DLY);
  EDP_WUINT32(dp_vid_idel_ctrl         , 3);
  EDP_WUINT32(dp_vid_tu_ctrl           , (para.fill<<16) + TU_SIZE);
  EDP_WUINT32(dp_hwidth                , tmg->x);
  EDP_WUINT32(dp_vheight               , tmg->y);
  EDP_WUINT32(dp_htotal                , tmg->ht);
  EDP_WUINT32(dp_vtotal                , tmg->vt);
  EDP_WUINT32(dp_hstart                , tmg->hbp);
  EDP_WUINT32(dp_vstart                , tmg->vbp);
  EDP_WUINT32(dp_hsp_hsw               , (1<<15) + tmg->hpsw);
  EDP_WUINT32(dp_vsp_vsw               , (1<<15) + tmg->vpsw);
  EDP_WUINT32(dp_misc                  , 0x0021);
  EDP_WUINT32(dp_mvid                  , para.mvid);
  EDP_WUINT32(dp_nvid                  , para.nvid);
  EDP_WUINT32(dp_total_symbol_per_line , total_symbol_per_line);

  if(eDP_capable)
  {
    EDP_WUINT32(dp_scramble_seed,0xfffe);
  }
  else
  {
    EDP_WUINT32(dp_scramble_seed,0xffff);
  }

  if(dp_enhanced_frame_cap)
    EDP_SBIT32(dp_ctrl,BIT23);
  else
    EDP_CBIT32(dp_ctrl,BIT23);

  return 0;
}

void dp_video_enable()
{
  EDP_SBIT32(dp_ctrl,BIT0);
  edp_delay_ms(1);  //delay 1ms before disable training
  EDP_WUINT32(dp_training_ctrl,0x0000000);
}

void dp_video_disable()
{
  EDP_CBIT32(dp_ctrl,BIT0);
}

static int dp_tps1_test(unsigned int swing_lv, unsigned int pre_lv, unsigned int postcur2_lv,
                unsigned char is_swing_max, unsigned char is_pre_max)            //for clock recovery
{
  unsigned int to_cnt;
  int ret;

  EDP_WUINT32(dp_training_ctrl,1);
  EDP_SBIT32(dp_training_ctrl,BIT31);

  phy_cfg(swing_lv, pre_lv, postcur2_lv);

  edp_delay_ms(10);

  to_cnt = TRAIN_CNT;
  while(1)
  {
    fp_tx_buf[0] = 0x21;        //set pattern 1 with scramble disalbe
    //set pattern 1 with max swing and emphasis 0x13
    fp_tx_buf[1] = ( (is_pre_max & 0x1) << 5 ) | ( (pre_lv & 0x3) << 3) | ( (is_swing_max & 0x1) << 2 ) | (swing_lv & 0x3);
    fp_tx_buf[2] = fp_tx_buf[1];
    fp_tx_buf[3] = fp_tx_buf[1];
    fp_tx_buf[4] = fp_tx_buf[1];
    ret = aux_wr(0x00102,5,fp_tx_buf);
    if(ret == -1)
      return ret;

    edp_delay_ms(30);
    edp_delay_ms(training_aux_rd_interval);   //wait for the training finish

    ret = aux_rd(0x0202,6,fp_rx_buf);
    if(ret == -1)
    return ret;

    if(glb_lane_cnt == 1)
    {
      if( (fp_rx_buf[0]&0x01) == 0x01 )
        return 0;
    }

    if(glb_lane_cnt == 2)
    {
      if( (fp_rx_buf[0]&0x11) == 0x11 )
        return 0;
    }
    if(glb_lane_cnt == 4)
    {
      if( ((fp_rx_buf[0]&0x11) == 0x11) &&  ((fp_rx_buf[1]&0x11) == 0x11) )
        return 0;
    }

    to_cnt--;
    if(to_cnt == 0)
    return -1;
  }
}

static int dp_tps2_test(unsigned int swing_lv, unsigned int pre_lv, unsigned int postcur2_lv,
            unsigned char is_swing_max, unsigned char is_pre_max)            //for EQ
{
  int result,ret;
  unsigned int to_cnt;

  EDP_WUINT32(dp_training_ctrl,2);
  EDP_SBIT32(dp_training_ctrl,BIT31);
  edp_delay_ms(10);

  phy_cfg(swing_lv, pre_lv, postcur2_lv);

  result = -1;
  to_cnt = TRAIN_CNT;
  while(1)
  {
    fp_tx_buf[0] = 0x22;        //set pattern 2 with scramble disalbe
    fp_tx_buf[1] = ( (is_pre_max & 0x1) << 5 ) | ( (pre_lv & 0x3) << 3) | ( (is_swing_max & 0x1) << 2 ) | (swing_lv & 0x3);
    fp_tx_buf[2] = fp_tx_buf[1];
    fp_tx_buf[3] = fp_tx_buf[1];
    fp_tx_buf[4] = fp_tx_buf[1];
    ret = aux_wr(0x00102,5,fp_tx_buf);
    if(ret == -1)
      return ret;

    edp_delay_ms(training_aux_rd_interval);   //wait for the training finish
    edp_delay_ms(30);
    ret = aux_rd(0x0202,6,fp_rx_buf);
    if(ret == -1)
      return ret;

    if(glb_lane_cnt == 1)
    {
      if( (fp_rx_buf[0]&0x07) == 0x07 )
      {
        result = 0;
        goto tps2_end;
      }
    }

    if(glb_lane_cnt == 2)
    {
      if( (fp_rx_buf[0]&0x77) == 0x77 )
      {
        result = 0;
        goto tps2_end;
      }
    }

    if(glb_lane_cnt == 4)
    {
      if( ((fp_rx_buf[0]&0x77) == 0x77) &&  ((fp_rx_buf[1]&0x77) == 0x77) )
      {
        result = 0;
        goto tps2_end;
      }
    }

    to_cnt--;
    if(to_cnt == 0)
      return -1;
  }

tps2_end:
  fp_tx_buf[0] = 0x00;    //102
  fp_tx_buf[1] = ( (is_pre_max & 0x1) << 5 ) | ( (pre_lv & 0x3) << 3) | ( (is_swing_max & 0x1) << 2 ) | (swing_lv & 0x3);
  fp_tx_buf[2] = fp_tx_buf[1];
  fp_tx_buf[3] = fp_tx_buf[1];
  fp_tx_buf[4] = fp_tx_buf[1];

  fp_tx_buf[5] = 0x00;    //107
  fp_tx_buf[6] = 0x01;    //108
  fp_tx_buf[7] = 0x00;    //109
  fp_tx_buf[8] = 0x00;    //10a
  fp_tx_buf[9] = 0x00;    //10b
  fp_tx_buf[10] = 0x00;   //10c
  fp_tx_buf[11] = 0x00;   //10d
  fp_tx_buf[12] = 0x00;   //10e
  ret = aux_wr(0x0102,13,fp_tx_buf);
  if(ret == -1)
    return ret;

  return result;
}

//call after dp_ctrl_init and dp_sink_init
int dp_training_sequence(unsigned int init_swing_lv)
{
  int result;
  unsigned char is_swing_max,is_pre_max;

  for(glb_pre_lv = 0; glb_pre_lv<= 3; glb_pre_lv++)
    for(glb_swing_lv = 0; glb_swing_lv <= 3; glb_swing_lv++)
    {
      if(glb_swing_lv < init_swing_lv)
        continue;

      is_swing_max = 0;
      is_pre_max = 0;
      if(glb_swing_lv == 0)
      {
        if(glb_pre_lv == 3)
          is_pre_max = 1;
      }
      else if(glb_swing_lv == 1)
      {
        if(glb_pre_lv >= 2)
          is_pre_max = 1;
      }
      else if (glb_swing_lv == 2)
      {
        if(glb_pre_lv >= 1)
          is_pre_max = 1;
      }
      else if (glb_swing_lv == 3)
      {
        if(glb_pre_lv >= 0)
          is_pre_max = 1;
      }
      if(glb_swing_lv == 3)
        is_swing_max = 1;

      result = dp_tps1_test(glb_swing_lv, glb_pre_lv, glb_postcur2_lv,is_swing_max,is_pre_max);
      if(result == 0)
        goto train_next;
      else
        continue;
    }
    goto train_end;
train_next:
  for(glb_pre_lv = 0; glb_pre_lv<= 3; glb_pre_lv++)
    for(glb_swing_lv = 0; glb_swing_lv <= 3; glb_swing_lv++)
    {
      if(glb_swing_lv < init_swing_lv)
        continue;

      is_swing_max = 0;
      is_pre_max = 0;
      if(glb_swing_lv == 0)
      {
        if(glb_pre_lv == 3)
          is_pre_max = 1;
      }
      else if(glb_swing_lv == 1)
      {
        if(glb_pre_lv >= 2)
          is_pre_max = 1;
      }
      else if (glb_swing_lv == 2)
      {
        if(glb_pre_lv >= 1)
          is_pre_max = 1;
      }
      else if (glb_swing_lv == 3)
      {
        if(glb_pre_lv >= 0)
          is_pre_max = 1;
      }
      if(glb_swing_lv == 3)
        is_swing_max = 1;

      result = dp_tps2_test(glb_swing_lv, glb_pre_lv, glb_postcur2_lv,is_swing_max,is_pre_max);
      if(result == 0)
      goto train_end;
      }

train_end:
  return result;
}

int dp_quality_test(void)
{
// quality pattern 2
  while(1)
  {
    EDP_WUINT32(dp_training_ctrl,0x80000020);

    fp_tx_buf[0] = 0x00;    //102
    fp_tx_buf[1] = 0x12;    //103
    fp_tx_buf[2] = 0x12;    //104
    fp_tx_buf[3] = 0x12;    //105
    fp_tx_buf[4] = 0x12;    //106
    fp_tx_buf[5] = 0x00;    //107
    fp_tx_buf[6] = 0x01;    //108
    fp_tx_buf[7] = 0x00;    //109
    fp_tx_buf[8] = 0x00;    //10a
    fp_tx_buf[9] = 0x02;    //10b
    fp_tx_buf[10] = 0x02;   //10c
    fp_tx_buf[11] = 0x02;   //10d
    fp_tx_buf[12] = 0x02;   //10e
    aux_wr(0x0102,13,fp_tx_buf);

    edp_delay_ms(10);
    aux_rd(0x0210,8,fp_rx_buf);

    edp_delay_ms(200);
    aux_rd(0x0210,8,fp_rx_buf);

  // quality pattern 3

    EDP_WUINT32(dp_training_ctrl,0x80000030);

    fp_tx_buf[0] = 0x00;    //102
    fp_tx_buf[1] = 0x12;    //103
    fp_tx_buf[2] = 0x12;    //104
    fp_tx_buf[3] = 0x12;    //105
    fp_tx_buf[4] = 0x12;    //106
    fp_tx_buf[5] = 0x00;    //107
    fp_tx_buf[6] = 0x01;    //108
    fp_tx_buf[7] = 0x00;    //109
    fp_tx_buf[8] = 0x00;    //10a
    fp_tx_buf[9] = 0x03;    //10b
    fp_tx_buf[10] = 0x03;   //10c
    fp_tx_buf[11] = 0x03;   //10d
    fp_tx_buf[12] = 0x03;   //10e
    aux_wr(0x0102,13,fp_tx_buf);

    edp_delay_ms(10);
    aux_rd(0x0210,8,fp_rx_buf);
    edp_delay_ms(100);
    aux_rd(0x0210,8,fp_rx_buf);
  }

  fp_tx_buf[0] = 0x00;    //102
  fp_tx_buf[1] = 0x13;    //103
  fp_tx_buf[2] = 0x13;    //104
  fp_tx_buf[3] = 0x13;    //105
  fp_tx_buf[4] = 0x13;    //106
  fp_tx_buf[5] = 0x00;    //107
  fp_tx_buf[6] = 0x01;    //108
  fp_tx_buf[7] = 0x00;    //109
  fp_tx_buf[8] = 0x00;    //10a
  fp_tx_buf[9] = 0x00;    //10b
  fp_tx_buf[10] = 0x00;   //10c
  fp_tx_buf[11] = 0x00;   //10d
  fp_tx_buf[12] = 0x00;   //10e
  aux_wr(0x0102,13,fp_tx_buf);

  return 0;
}

void dp_get_sink_info(struct sink_info *sink_inf)
{
  sink_inf->dp_rev = dp_rev;
  sink_inf->dp_enhanced_frame_cap = dp_enhanced_frame_cap;
  sink_inf->dp_max_link_rate = dp_max_link_rate;
  sink_inf->dp_max_lane_count = dp_max_lane_count;
  sink_inf->eDP_capable = eDP_capable;
}

void dp_get_training_info(struct training_info *train_inf)
{
  train_inf->swing_lv = glb_swing_lv;
  train_inf->preemp_lv = glb_pre_lv;
  train_inf->postcur2_lv = glb_postcur2_lv;
}

unsigned int dp_get_hpd_status()
{
  if(EDP_RUINT32(dp_int_status)&0x1)
    return 1;
  else
    return 0;
}

void dp_set_line_cnt(unsigned int line0_cnt, unsigned int line1_cnt)
{
  EDP_WUINT32(dp_line_triger,line0_cnt<<16 | line1_cnt);
}

void dp_set_start_dly(unsigned int dly)
{
  EDP_WUINT32(dp_start_dly, dly);
}

unsigned int dp_get_start_dly(void)
{
	return EDP_RUINT32(dp_start_dly);
}

/*
 * edp_int:LINE0,LINE1
 */
void dp_int_enable(enum edp_int intterupt)
{
  EDP_SBIT32(dp_int_mask,intterupt);
}

/*
 * edp_int:LINE0,LINE1
 */
void dp_int_disable(enum edp_int intterupt)
{
  EDP_CBIT32(dp_int_mask,intterupt);
}

/*
 * edp_int:LINE0,LINE1
 * return:
 * 0: no pending
 * 1: pending
 * -1: int type not exist
 */
int dp_get_int_status(enum edp_int intterupt)
{
  switch(intterupt)
  {
    case LINE0:
      return (EDP_RUINT32(dp_int_status) & intterupt) >> 31;
    case LINE1:
      return (EDP_RUINT32(dp_int_status) & intterupt) >> 30;
    case FIFO_EMPTY:
      return (EDP_RUINT32(dp_int_status) & intterupt) >> 29;
    default:
      return -1;
  }
}

/*
 * edp_int:LINE0,LINE1
 */
void dp_clr_int_status(enum edp_int intterupt)
{
  EDP_SBIT32(dp_int_status,intterupt);
}
