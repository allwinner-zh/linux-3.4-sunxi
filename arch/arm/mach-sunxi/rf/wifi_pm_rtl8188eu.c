/*
 * rtl8188eu usb wifi power management API
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <mach/sys_config.h>
#include <mach/gpio.h>
#include <linux/regulator/consumer.h>
#include "wifi_pm.h"

#define rtl8188eu_msg(...)    do {printk("[rtl8188eu]: "__VA_ARGS__);} while(0)

static int rtl8188eu_powerup = 0;
static int rtk8188eu_suspend = 0;
static char *axp_name[4] = {NULL};

// power control by axp
static int rtl8188eu_module_power(int onoff)
{
	struct regulator* wifi_ldo[4] = {NULL};
	static int first = 1;
	int i = 0, ret = 0;

	rtl8188eu_msg("rtl8188eu module power set by axp.\n");
	for (i = 0; axp_name[i] != NULL; i++){
	  wifi_ldo[i] = regulator_get(NULL, axp_name[i]);
	  if (IS_ERR(wifi_ldo[i])) {
      rtl8188eu_msg("get power regulator %s failed.\n", axp_name[i]);
      goto exit;
		}
	}

exit:
  wifi_ldo[i] = NULL;

	if (first) {
		rtl8188eu_msg("first time\n");
		for (i = 0; wifi_ldo[i] != NULL; i++){
		  ret = regulator_force_disable(wifi_ldo[i]);
      if (ret < 0) {
        rtl8188eu_msg("regulator_force_disable  %s fail, return %d.\n", axp_name[i], ret);
        regulator_put(wifi_ldo[i]);
        return ret;
      }
		}
		
		first = 0;
	}

	if (onoff) {
		rtl8188eu_msg("regulator on.\n");

		for(i = 0; wifi_ldo[i] != NULL; i++){
      ret = regulator_set_voltage(wifi_ldo[i], 3300000, 3300000);
      if (ret < 0) {
        rtl8188eu_msg("regulator_set_voltage %s fail, return %d.\n", axp_name[i], ret);
        regulator_put(wifi_ldo[i]);
        return ret;
      }

      ret = regulator_enable(wifi_ldo[i]);
      if (ret < 0) {
        rtl8188eu_msg("regulator_enable %s fail, return %d.\n", axp_name[i], ret);
        regulator_put(wifi_ldo[i]);
        return ret;
      }
		}
		
	} else {
		rtl8188eu_msg("regulator off.\n");
		for(i = 0; wifi_ldo[i] != NULL; i++){
      ret = regulator_disable(wifi_ldo[i]);
      if (ret < 0) {
        rtl8188eu_msg("regulator_disable %s fail, return %d.\n", axp_name[i], ret);
        regulator_put(wifi_ldo[i]);
        return ret;
      }
		}
	}
	
	for(i = 0; wifi_ldo[i] != NULL; i++){
	  regulator_put(wifi_ldo[i]);
	}
	return ret;
}

void rtl8188eu_power(int mode, int *updown)
{
    if (mode) {
        if (*updown) {
			rtl8188eu_module_power(1);
			udelay(50);
			rtl8188eu_powerup = 1;
        } else {
			rtl8188eu_module_power(0);
			rtl8188eu_powerup = 0;
        }
        rtl8188eu_msg("usb wifi power state: %s\n", *updown ? "on" : "off");
    } else {
        if (rtl8188eu_powerup)
            *updown = 1;
        else
            *updown = 0;
		rtl8188eu_msg("usb wifi power state: %s\n", rtl8188eu_powerup ? "on" : "off");
    }
    return;	
}

static void rtl8188eu_standby(int instadby)
{
	if (instadby) {
		if (rtl8188eu_powerup) {
			rtl8188eu_module_power(0);
			rtk8188eu_suspend = 1;
		}
	} else {
		if (rtk8188eu_suspend) {
			rtl8188eu_module_power(1);
			rtk8188eu_suspend = 0;
		}
	}
	rtl8188eu_msg("usb wifi : %s\n", instadby ? "suspend" : "resume");
}

void rtl8188eu_gpio_init(void)
{
	script_item_u val;
	script_item_value_type_e type;
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;

	rtl8188eu_msg("exec rtl8188eu_wifi_gpio_init\n");

	type = script_get_item(wifi_para, "wifi_power", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rtl8188eu_msg("failed to fetch wifi_power\n");
		return;
	}

	axp_name[0] = val.str;
	rtl8188eu_msg("module power name %s\n", axp_name[0]);

  type = script_get_item(wifi_para, "wifi_power_ext1", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rtl8188eu_msg("failed to fetch wifi_power\n");
		return;
	}
  axp_name[1] = val.str;
  rtl8188eu_msg("module power ext1 name %s\n", axp_name[1]);
  
  type = script_get_item(wifi_para, "wifi_power_ext2", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rtl8188eu_msg("failed to fetch wifi_power\n");
		return;
	}
  axp_name[2] = val.str;
  rtl8188eu_msg("module power ext2 name %s\n", axp_name[2]);

  axp_name[3] = NULL;
	
	rtl8188eu_powerup = 0;
	rtk8188eu_suspend = 0;
	ops->power     = rtl8188eu_power;
	ops->standby   = rtl8188eu_standby;

	// force to disable wifi power in system booting,
	// make sure wifi power is down when system start up
	rtl8188eu_module_power(0);
}
