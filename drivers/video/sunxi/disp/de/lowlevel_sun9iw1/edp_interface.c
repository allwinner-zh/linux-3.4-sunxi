#include "edp_hal.h"

#define EDP_BASE_ADDR 0x03c30000

edp_print_string _edp_print_str;
edp_print_value _edp_print_val;

int edp_set_print_str_func(edp_print_string print)
{
	_edp_print_str = print;
	return 0;
}

int edp_set_print_val_func(edp_print_value print)
{
	_edp_print_val = print;
	return 0;
}

void edp_print_str(char *c)
{
	if(_edp_print_str)
		_edp_print_str(c);
}

void edp_print_val(unsigned int val)
{
	if(_edp_print_val)
		_edp_print_val(val);
}

int edp_set_base_address(unsigned int address)
{
	edp_set_baddr(EDP_BASE_ADDR + address);
	return 0;
}

int edp_enable(struct edp_para *para, struct video_timming *tmg)
{
	int ret;
	struct sink_info sink_inf;
	struct training_info train_inf;
	unsigned int start_delay;

	ret = edp_init(para->lane_count, para->bit_rate);
	if(ret == -1)
	{
		edp_print_str("[EDP]lane count error\n");
	}
	else if(ret == -3)
	{
		edp_print_str("[EDP]dp sink init error\n");
	}

	edp_get_sink_info(&sink_inf);
	//print sink info
	edp_print_str("[EDP]sink info:");
	edp_print_val(sink_inf.dp_enhanced_frame_cap);
	edp_print_val(sink_inf.dp_max_lane_count);
	edp_print_val(sink_inf.dp_max_link_rate);
	edp_print_val(sink_inf.dp_rev);
	edp_print_val(sink_inf.eDP_capable);
	edp_print_str("\n");

	//0:normal 1:color bar 2:mosaic
	edp_s_video_gen_src(0);
	edp_set_start_dly(para->start_delay); //depend on vblank and dram mdfs needs
	ret = edp_s_video_mode(tmg);
	if(ret == -1)
	{
		edp_print_str("[EDP]fps or vt error\n");
	}
	else if(ret == -2)
	{
		edp_print_str("[EDP]symbol per line error\n");
	}
	else if(ret == -3)
	{
		edp_print_str("[EDP]tu ratio error\n");
	}

	ret = edp_training(para->swing_level);//init swing level
	if(ret == -1)
	{
		edp_print_str("[EDP]training error\n");
	}

	edp_get_training_info(&train_inf);
	//print training info
	edp_print_str("[EDP]train inf:");
	edp_print_val(train_inf.postcur2_lv);
	edp_print_val(train_inf.preemp_lv);
	edp_print_val(train_inf.swing_lv);
	edp_print_str("\n");

	edp_s_video_on();

	start_delay = edp_get_start_delay();
	dp_set_line_cnt(1, start_delay - 2);

	return 0;
}

void edp_disable(void)
{
	edp_s_video_off();
	edp_remove();
}

void edp_set_start_delay(unsigned int delay)
{
	dp_set_start_dly(delay);
}

unsigned int edp_get_start_delay(void)
{
	return dp_get_start_dly();
}
