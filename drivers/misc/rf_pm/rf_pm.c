#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/clk/sunxi.h>
#include <linux/err.h>
#include <mach/sys_config.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include "rf_pm.h"

struct rf_mod_info mod_info;
static char *rf_para = "rf_para";
static struct clk *ap_32k = NULL;

char *module_list[] = {
	" ",           
	"ap6181",       
	"ap6210",      
	"rtl8188eu",   
	"rtl8723au",   
	"rtl8723bs",   
	"esp8089",      
	"ap6476",       
	"ap6330",      
};

#define rf_pm_msg(...)    do {printk("[rf_pm]: "__VA_ARGS__);} while(0)

int sunxi_gpio_req(struct gpio_config *gpio)
{
	int            ret = 0;
	char           pin_name[8] = {0};
	unsigned long  config;

	sunxi_gpio_to_name(gpio->gpio, pin_name);
	config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, gpio->mul_sel);
	ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
	if (ret) {
        rf_pm_msg("set gpio %s mulsel failed.\n",pin_name);
        return -1;
    }

	if (gpio->pull != GPIO_PULL_DEFAULT){
        config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, gpio->pull);
        ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
        if (ret) {
                rf_pm_msg("set gpio %s pull mode failed.\n",pin_name);
                return -1;
        }
	}

	if (gpio->drv_level != GPIO_DRVLVL_DEFAULT){
        config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, gpio->drv_level);
        ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
        if (ret) {
            rf_pm_msg("set gpio %s driver level failed.\n",pin_name);
            return -1;
        }
    }

	if (gpio->data != GPIO_DATA_DEFAULT) {
        config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, gpio->data);
        ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
        if (ret) {
            rf_pm_msg("set gpio %s initial val failed.\n",pin_name);
            return -1;
        }
  }

  return 0;
}
EXPORT_SYMBOL(sunxi_gpio_req);

//return mod index
int get_rf_mod_type(void)
{
	return mod_info.num;
}
EXPORT_SYMBOL(get_rf_mod_type);

//return module name
char * get_rf_mod_name(void)
{
	return module_list[mod_info.num];
}
EXPORT_SYMBOL(get_rf_mod_name);

int rf_module_power(int onoff)
{
	struct regulator* wifi_ldo[4] = {NULL};
	static int first = 1;
	int i = 0, ret = 0;

  for (i = 0; mod_info.power[i] != NULL; i++){
    wifi_ldo[i] = regulator_get(NULL, mod_info.power[i]);
    if (IS_ERR(wifi_ldo[i])) {
      rf_pm_msg("get power regulator %s failed.\n", mod_info.power[i]);
      break;
    }
  }

  wifi_ldo[i] = NULL;

  if (first) {
    rf_pm_msg("first time\n");
    
    for (i = 0; wifi_ldo[i] != NULL; i++){
      ret = regulator_force_disable(wifi_ldo[i]);
      if (ret < 0) {
        rf_pm_msg("regulator_force_disable  %s fail, return %d.\n", mod_info.power[i], ret);
        goto exit;
      }
    }
    first = 0;
    if (!onoff)
    	goto exit;
  }

  if (onoff) {
    rf_pm_msg("regulator on.\n");
    for(i = 0; wifi_ldo[i] != NULL; i++){
      ret = regulator_set_voltage(wifi_ldo[i], mod_info.power_vol[i], mod_info.power_vol[i]);
      if (ret < 0) {
        rf_pm_msg("set_voltage %s %d fail, return %d.\n", mod_info.power[i], mod_info.power_vol[i], ret);
        goto exit;
      }
        
      ret = regulator_enable(wifi_ldo[i]);
      if (ret < 0) {
        rf_pm_msg("regulator_enable %s fail, return %d.\n", mod_info.power[i], ret);
        goto exit;
      }
    }
	} else {
		rf_pm_msg("regulator off.\n");
    for(i = 0; wifi_ldo[i] != NULL; i++){
      ret = regulator_disable(wifi_ldo[i]);
      if (ret < 0) {
        rf_pm_msg("regulator_disable %s fail, return %d.\n", mod_info.power[i], ret);
        goto exit;
      }
    }
	}


  rf_pm_msg("mod info power switch %d\n", mod_info.power_switch);
  if (mod_info.power_switch != -1) {
    if (onoff) {
      gpio_set_value(mod_info.power_switch, 1);
    } else {
      gpio_set_value(mod_info.power_switch, 0);
    }
  }

exit:
	for(i = 0; wifi_ldo[i] != NULL; i++){
        regulator_put(wifi_ldo[i]);
	}
  
	return ret;
}
EXPORT_SYMBOL(rf_module_power);

int rf_pm_gpio_ctrl(char *name, int level)
{
	int i = 0;
	int gpio = 0;
	char * gpio_name[1] = {"chip_en"};

	for (i = 0; i < 1; i++) {
		if (strcmp(name, gpio_name[i]) == 0) {
			switch (i)
			{
				case 0: /*chip_en*/
					gpio = mod_info.chip_en;
					break;
				default:
					rf_pm_msg("no matched gpio.\n");
			}
			break;
		}
	}

  if (gpio != -1){
    gpio_set_value(gpio, level);
	  printk("gpio %s set val %d, act val %d\n", name, level, gpio_get_value(gpio));
  }
  
	return 0;
}
EXPORT_SYMBOL(rf_pm_gpio_ctrl);

static void enable_ap_32k(enable)
{
	int ret = 0;

	if (enable){
    ret = clk_prepare_enable(ap_32k);
    if (ret){
		  rf_pm_msg("enable ap 32k failed!\n");
	  }
  } else {
    clk_disable_unprepare(ap_32k);
  }
}

//get module resource
static int get_module_res(void)
{
  script_item_u val;
	script_item_value_type_e type;
	struct gpio_config  *gpio_p = NULL;
	struct rf_mod_info *mod_info_p = &mod_info;

  memset(mod_info_p, 0, sizeof(struct rf_mod_info));

	type = script_get_item(rf_para, "module_num", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		rf_pm_msg("failed to fetch wifi configuration!\n");
		return -1;
	}
	mod_info_p->num = val.val;
	if (mod_info_p->num <= 0) {
		rf_pm_msg("no wifi used in configuration\n");
		return -1;
	}
  rf_pm_msg("select module num is %d\n", mod_info_p->num);

  type = script_get_item(rf_para, "module_power1", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rf_pm_msg("failed to fetch module_power1\n");
	} else {
    mod_info_p->power[0] = val.str;
    rf_pm_msg("module power1 name %s\n", mod_info_p->power[0]);
  }

  type = script_get_item(rf_para, "module_power1_vol", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		rf_pm_msg("failed to fetch module_power1_vol\n");
	} else {
    mod_info_p->power_vol[0] = val.val;
    rf_pm_msg("module power1 vol %d\n", mod_info_p->power_vol[0]);
  }

  type = script_get_item(rf_para, "module_power2", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rf_pm_msg("failed to fetch module_power2\n");
	} else {
    mod_info_p->power[1] = val.str;
    rf_pm_msg("module power2 name %s\n", mod_info_p->power[1]);
  }

  type = script_get_item(rf_para, "module_power2_vol", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		rf_pm_msg("failed to fetch module_power2_vol\n");
	} else {
    mod_info_p->power_vol[1] = val.val;
    rf_pm_msg("module power2 vol %d\n", mod_info_p->power_vol[1]);
  }

  type = script_get_item(rf_para, "module_power3", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rf_pm_msg("failed to fetch module_power3\n");
	} else {
    mod_info_p->power[2] = val.str;
    rf_pm_msg("module power3 name %s\n", mod_info_p->power[2]);
  }
  
  type = script_get_item(rf_para, "module_power3_vol", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		rf_pm_msg("failed to fetch module_power3_vol\n");
	} else {
    mod_info_p->power_vol[2] = val.val;
    rf_pm_msg("module power3 vol %d\n", mod_info_p->power_vol[2]);
  }
  
  mod_info_p->power[3] = NULL;
  
  mod_info_p->power_switch = -1;
  type = script_get_item(rf_para, "power_switch", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type)
		rf_pm_msg("mod has no power_switch gpio\n");
	else {
		gpio_p = &val.gpio;
	  mod_info_p->power_switch = gpio_p->gpio;
	  sunxi_gpio_req(gpio_p);
	}
	
	mod_info_p->chip_en = -1;
  type = script_get_item(rf_para, "chip_en", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type)
		rf_pm_msg("mod has no chip_en gpio\n");
	else {
		gpio_p = &val.gpio;
	  mod_info_p->chip_en = gpio_p->gpio;
	  sunxi_gpio_req(gpio_p);
	}	
  
	type = script_get_item(rf_para, "lpo_use_apclk", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) 
		rf_pm_msg("failed to fetch lpo_use_apclk\n");
	else {
	  mod_info_p->lpo_use_apclk = val.str;
	}
	
	return 0;	
}

static int __init rf_pm_init(void)
{
	get_module_res();
	if (mod_info.num <= 0)
		return -1;

  //moduls power init
  switch(mod_info.num){
    case 1:   /* ap6181 */
    case 2:   /* ap6210 */
    case 5:   /* rtl8723bs */
    case 6:   /* esp8089 */	
    case 7:   /* ap6476 */
    case 8:   /* ap6330 */
    	rf_module_power(1);
      break;

    case 3:   /* rtl8188eu */
    case 4:   /* rtl8723au */
    	rf_module_power(0);
    	break;
    	
    default:
    	rf_pm_msg("wrong module select %d !\n", mod_info.num);
  }
  
  //opt ap 32k
  if(!mod_info.lpo_use_apclk && !strcmp(mod_info.lpo_use_apclk, "")){
  	ap_32k = clk_get(NULL, mod_info.lpo_use_apclk);
    if (!ap_32k || IS_ERR(ap_32k)){
      rf_pm_msg("Get ap 32k clk out failed!\n");
      return -1;
    }
    rf_pm_msg("set %s 32k out", mod_info.lpo_use_apclk);
    enable_ap_32k(1);
  }
  
  return 0;
}

static void __exit rf_pm_exit(void)
{
	if (ap_32k){
	  enable_ap_32k(0);
	  ap_32k = NULL;
	}
  rf_module_power(0);
}

module_init(rf_pm_init);
module_exit(rf_pm_exit);