
#include"bsp_isp.h"
#include"bsp_isp_algo.h"
#include"isp_module_cfg.h"
void bsp_isp_enable(void){return;}
void bsp_isp_disable(void){return;}
void bsp_isp_rot_enable(void){return;}
void bsp_isp_rot_disable(void){return;}
void bsp_isp_video_capture_start(void){return;}
void bsp_isp_video_capture_stop(void){return;}
void bsp_isp_image_capture_start(void){return;}
void bsp_isp_image_capture_stop(void){return;}
unsigned int bsp_isp_get_para_ready(void){return 0;}
void bsp_isp_set_para_ready(void){return;}
void bsp_isp_clr_para_ready(void){return;}

/*
 * irq_flag:
 * 
 * FINISH_INT_EN
 * START_INT_EN
 * PARA_SAVE_INT_EN 
 * PARA_LOAD_INT_EN 
 * SRC0_FIFO_INT_EN
 * SRC1_FIFO_INT_EN 
 * ROT_FINISH_INT_EN 
 * ISP_IRQ_EN_ALL
 */ 
void bsp_isp_irq_enable(unsigned int irq_flag){return;} 
void bsp_isp_irq_disable(unsigned int irq_flag){return;}
unsigned int bsp_isp_get_irq_status(unsigned int irq){return 0;}
int bsp_isp_int_get_enable(void){return 0;}    
void bsp_isp_clr_irq_status(unsigned int irq){return;}
void bsp_isp_set_statistics_addr(unsigned int addr){return;}
void bsp_isp_mirror_enable(enum isp_channel ch){return;}
void bsp_isp_flip_enable(enum isp_channel ch){return;}
void bsp_isp_mirror_disable(enum isp_channel ch){return;}
void bsp_isp_flip_disable(enum isp_channel ch){return;}
void bsp_isp_set_fmt(enum bus_pixeltype type, enum pixel_fmt *fmt){return;}
void bsp_isp_set_rot(enum isp_channel ch,enum isp_rot_angle angle){return;}

unsigned int bsp_isp_set_size(enum pixel_fmt *fmt, struct isp_size_settings *size_settings){return 0;}
void bsp_isp_set_output_addr(unsigned int buf_base_addr){return;}
void bsp_isp_set_base_addr(unsigned int vaddr){return;}
void bsp_isp_set_dma_load_addr(unsigned int dma_addr){return;}
void bsp_isp_set_dma_saved_addr(unsigned int dma_addr){return;}
void bsp_isp_set_map_load_addr(unsigned int vaddr){return;}
void bsp_isp_set_map_saved_addr(unsigned int vaddr){return;}
void bsp_isp_update_lut_lens_gamma_table(struct isp_table_addr *tbl_addr){return;}
void bsp_isp_update_drc_table(struct isp_table_addr *tbl_addr){return;}
void bsp_isp_init_platform(unsigned int platform_id){return;}
void bsp_isp_init(struct isp_init_para *para){return;}
void bsp_isp_exit(void){return;}
void bsp_isp_print_reg_saved(void){return;}


void isp_param_init(struct isp_gen_settings *isp_gen){return;}
int  isp_module_init(struct isp_gen_settings *isp_gen, struct isp_3a_result *isp_result){return 0;}
void isp_module_cleanup(struct isp_gen_settings *isp_gen){return;}
void isp_isr(struct isp_gen_settings *isp_gen, struct isp_3a_result *isp_result){return;}
void isp_module_restore_context(struct isp_gen_settings *isp_gen){return;}
void bsp_isp_s_brightness(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_contrast(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_saturation(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_hue(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_white_balance(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_exposure(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_gain(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_gain(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_hflip(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_vflip(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_power_line_frequency(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_hue_auto(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_white_balance_temperature(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_sharpness(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_chroma_agc(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_colorfx(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_brightness(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_band_stop_filter(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_power_line_frequency_auto(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_illuminators_1(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_illuminators_2(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_laststp1(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_private_base(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_hflip_thumb(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_vflip_thumb(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_focus_win_num(struct isp_gen_settings *isp_gen, int value, struct isp_h3a_coor_win *coor){return;}
void bsp_isp_s_auto_focus_ctrl(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_flash_mode(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_exposure_win_num(struct isp_gen_settings *isp_gen, int value, struct isp_h3a_coor_win *coor){return;}
void bsp_isp_s_gsensor_rotation(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_exposure_auto(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_exposure_absolute(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_exposure_auto_priority(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_focus_absolute(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_focus_relative(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_focus_auto(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_exposure_bias(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_n_preset_white_balance(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_wide_dynamic_rage(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_image_stabilization(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_iso_sensitivity(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_iso_sensitivity_auto(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_scene_mode(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_3a_lock(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_focus_start(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_focus_stop(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_focus_status(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_auto_focus_range(struct isp_gen_settings *isp_gen, int value){return;}

void bsp_isp_s_take_pic(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_r_gain(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_g_gain(struct isp_gen_settings *isp_gen, int value){return;}
void bsp_isp_s_b_gain(struct isp_gen_settings *isp_gen, int value){return;}

int get_pre_ev_cumul(struct isp_gen_settings *isp_gen,struct isp_3a_result *isp_result){return 0;}
void bsp_isp_s_hdr(struct isp_gen_settings *isp_gen, struct hdr_setting_t *hdr){return;}
void config_sensor_next_exposure(struct isp_gen_settings *isp_gen,struct isp_3a_result *isp_result){return;}
void isp_config_init(struct isp_gen_settings *isp_gen){return;}

void isp_setup_module_hw(struct isp_module_config *module_cfg) { return; }
