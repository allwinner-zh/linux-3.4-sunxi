#ifndef RF__PM__H
#define RF__PM__H

//rf module common info
struct rf_mod_info {
  int       num;
  char      *power[4];
  int       power_vol[4];
  int       power_switch;
  int       chip_en;
  char      *lpo_use_apclk; 
};

//wl function info
struct wl_func_info {
	int  wifi_used;
  char *module_name;
  int  wl_reg_on;
  int  wl_power_state;

#ifdef CONFIG_PROC_FS
	struct proc_dir_entry		*proc_root;
	struct proc_dir_entry		*proc_power;
#endif  
};

#endif

