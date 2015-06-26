#ifndef  __edp_hal__h
#define  __edp_hal__h

#include "edp_core.h"

extern void edp_set_baddr(unsigned int edp_baseaddr);
extern int  edp_init(unsigned int lane_cnt, unsigned int bit_rate);
extern void edp_remove(void);
extern void edp_s_video_gen_src(unsigned int src);
extern int  edp_s_video_mode(struct video_timming *tmg);
extern int  edp_training(unsigned int init_swing_lv);
extern void edp_s_video_on(void);
extern void edp_s_video_off(void);
extern void edp_get_sink_info(struct sink_info *sink_inf);
extern void edp_get_training_info(struct training_info *train_inf);
extern unsigned int edp_g_hpd_status(void);
extern unsigned int dp_get_start_dly(void);
extern void edp_set_start_dly(unsigned int dly);
extern void dp_set_line_cnt(unsigned int line0_cnt, unsigned int line1_cnt);
extern void edp_int_enable(enum edp_int);
extern void edp_int_disable(enum edp_int);
extern unsigned int edp_get_int_status(enum edp_int);
extern void edp_clr_int_status(enum edp_int);

#endif
