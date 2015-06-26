#ifndef  __edp_core__h
#define  __edp_core__h

#include "edp_type.h"

#define TU_SIZE       32
#define STA_DLY       5
#define MAX_LANE_NUM  4
#define TRAIN_CNT     10

extern void edp_delay_ms(int t);
extern void set_base_addr(unsigned int edp_baseaddr);
extern int  phy_init(unsigned int lane_cnt, unsigned int bit_rate);
extern void phy_exit(void);
extern int phy_res_cal(void);
extern int phy_cfg(unsigned char swing_lv, unsigned char preemp_lv, unsigned char postcur2_lv);
extern int  dp_ctrl_init(void);
extern void dp_ctrl_exit(void);
extern int  dp_sink_init(void);
extern void dp_sink_exit(void);
extern void dp_video_gen_src(unsigned int src);
extern int  dp_video_set(struct video_timming *tmg);
extern int dp_training_sequence(unsigned int init_swing_lv);
extern void dp_video_enable(void);
extern void dp_video_disable(void);
extern void dp_get_sink_info(struct sink_info *sink_inf);
extern void dp_get_training_info(struct training_info *train_inf);
extern unsigned int dp_get_hpd_status(void);
extern void dp_set_start_dly(unsigned int dly);
extern void dp_set_line_cnt(unsigned int line0_cnt, unsigned int line1_cnt);
extern void dp_int_enable(enum edp_int);
extern void dp_int_disable(enum edp_int);
extern int dp_get_int_status(enum edp_int);
extern void dp_clr_int_status(enum edp_int);

#endif
