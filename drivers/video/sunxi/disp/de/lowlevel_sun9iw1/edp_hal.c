#include "edp_hal.h"

void edp_set_baddr(unsigned int edp_baseaddr)
{
  set_base_addr(edp_baseaddr);
}

/*
 * return 0: successful
 * return -1: lane count error
 * return -2: res cal error
 * return -3: dp sink init error
 */
int edp_init(unsigned int lane_cnt, unsigned int bit_rate)
{
	int try_cnt = 0;
  if(lane_cnt > MAX_LANE_NUM)
    return -1;

  phy_init(lane_cnt,bit_rate);

  phy_res_cal();

  phy_cfg(0,0,0);
  dp_ctrl_init();

  while(dp_sink_init()) {
		try_cnt ++;
		edp_delay_ms(50);
		if(try_cnt >= 3)
			return -3;
  }

  return 0;
}

void edp_remove(void)
{
  dp_sink_exit();
  dp_ctrl_exit();
  phy_exit();
}

/*
 * src 0: normal
 * src 1: color bar
 * src 2: mosaic
 */
void edp_s_video_gen_src(unsigned int src)
{
	dp_video_gen_src(src);
}

/*
 * return 0 successful
 * return -1 fps or vt error
 * return -2 symbol per line error
 * return -3 tu ratio error
 */
int edp_s_video_mode(struct video_timming *tmg)
{
  return dp_video_set(tmg);
}

/*
 * return 0: successful
 * return -1: not successful
 */
int edp_training(unsigned int init_swing_lv)
{
//	dp_quality_test();

  if(dp_training_sequence(init_swing_lv))
    return -1;

  return 0;
}

void edp_s_video_on(void)
{
  dp_video_enable();
}

void edp_s_video_off(void)
{
  dp_video_disable();
}

void edp_get_sink_info(struct sink_info *sink_inf)
{
	dp_get_sink_info(sink_inf);
}

void edp_get_training_info(struct training_info *train_inf)
{
	dp_get_training_info(train_inf);
}

/*
 * return 1: detected
 * return 0: not detected
 */
unsigned int edp_g_hpd_status(void)
{
  return dp_get_hpd_status();
}

void edp_set_start_dly(unsigned int dly)
{
	dp_set_start_dly(dly);
}

/*
 * line0_cnt: set to 1 indicates that first line in v blanking
 * line1_cnt:
 */
void edp_set_line_cnt(unsigned int line0_cnt, unsigned int line1_cnt)
{
	dp_set_line_cnt(line0_cnt,line1_cnt);
}
/*
 * edp_int:LINE0,LINE1
 */
void edp_int_enable(enum edp_int intterupt)
{
  dp_int_enable(intterupt);
}

/*
 * edp_int:LINE0,LINE1
 */
void edp_int_disable(enum edp_int intterupt)
{
  dp_int_disable(intterupt);
}

/*
 * edp_int:LINE0,LINE1
 * return 1: pending
 * return 0: no pending
 */
unsigned int edp_get_int_status(enum edp_int intterupt)
{
	return dp_get_int_status(intterupt);
}

/*
 * edp_int:LINE0,LINE1
 */
void edp_clr_int_status(enum edp_int intterupt)
{
	dp_clr_int_status(intterupt);
}



