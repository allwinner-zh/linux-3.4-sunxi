/*
 * rtl8723bs sdio wifi power management API
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <mach/sys_config.h>
#include <mach/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/pinconf-sunxi.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/clk/sunxi.h>
#include <linux/err.h>
#include "wifi_pm.h"

#define rtl8723bs_msg(...)	  do {printk("[rtl8723bs]: "__VA_ARGS__);} while(0)

static int rtl8723bs_chip_en = 0;
static int rtl8723bs_wl_regon = 0;
static int rtl8723bs_bt_regon = 0;
static char *axp_name[4] = {NULL};
static void rtl8723bs_config_32k_clk(void);

static int sunxi_rtl8723bs_gpio_req(struct gpio_config *gpio)
{
	int			   ret = 0;
	char		   pin_name[8] = {0};
	unsigned long  config;

	sunxi_gpio_to_name(gpio->gpio, pin_name);
	config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, gpio->mul_sel);
	ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
	if (ret) {
		rtl8723bs_msg("set gpio %s mulsel failed.\n",pin_name);
		return -1;
	}

	if (gpio->pull != GPIO_PULL_DEFAULT){
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, gpio->pull);
		ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
		if (ret) {
			rtl8723bs_msg("set gpio %s pull mode failed.\n",pin_name);
			return -1;
		}
	}

	if (gpio->drv_level != GPIO_DRVLVL_DEFAULT){
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, gpio->drv_level);
		ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
		if (ret) {
			rtl8723bs_msg("set gpio %s driver level failed.\n",pin_name);
			return -1;
		}
	}

	if (gpio->data != GPIO_DATA_DEFAULT) {
		config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, gpio->data);
		ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
		if (ret) {
			rtl8723bs_msg("set gpio %s initial val failed.\n",pin_name);
			return -1;
		}
	}

	return 0;
}

// power control by axp
static int rtl8723bs_module_power(int onoff)
{
	struct regulator* wifi_ldo[4] = {NULL};
	int i = 0, ret = 0;

	rtl8723bs_msg("rtl8723bs module power set by axp.\n");
	for (i = 0; axp_name[i] != NULL; i++){
		wifi_ldo[i] = regulator_get(NULL, axp_name[i]);
		if (IS_ERR(wifi_ldo[i])) {
			rtl8723bs_msg("get power regulator %s failed.\n", axp_name[i]);
			break;
		}
	}

	wifi_ldo[i] = NULL;
	if (onoff) {
		rtl8723bs_msg("regulator on.\n");
		
		for(i = 0; wifi_ldo[i] != NULL; i++){
			ret = regulator_set_voltage(wifi_ldo[i], 3000000, 3000000);
			if (ret < 0) {
				rtl8723bs_msg("regulator_set_voltage %s fail, return %d.\n", axp_name[i], ret);
				goto exit;
			}

			ret = regulator_enable(wifi_ldo[i]);
			if (ret < 0) {
				rtl8723bs_msg("regulator_enable %s fail, return %d.\n", axp_name[i], ret);
				goto exit;
			}
		}
	} else {
		rtl8723bs_msg("regulator off.\n");
		for(i = 0; wifi_ldo[i] != NULL; i++){
			ret = regulator_disable(wifi_ldo[i]);
			if (ret < 0) {
				rtl8723bs_msg("regulator_disable %s fail, return %d.\n", axp_name[i], ret);
				goto exit;
			}
		}
	}
exit:
	for(i = 0; wifi_ldo[i] != NULL; i++){
		regulator_put(wifi_ldo[i]);
	}
	
	return ret;
}

static int rtl8723bs_gpio_ctrl(char* name, int level)
{
	int i = 0;	
	int gpio = 0;
	char * gpio_name[2] = {"rtl8723bs_wl_regon", "rtl8723bs_bt_regon"};

	for (i = 0; i < 2; i++) {
		if (strcmp(name, gpio_name[i]) == 0) {
			switch (i)
			{
				case 0: /*rtl8723bs_wl_regon*/
					gpio = rtl8723bs_wl_regon;
					break;
				case 1: /*rtl8723bs_bt_regon*/
					gpio = rtl8723bs_bt_regon;
					break;
				default:
					rtl8723bs_msg("no matched gpio.\n");
			}
			break;
		}
	}

  __gpio_set_value(gpio, level);
  printk("gpio %s set val %d, act val %d\n", name, level, __gpio_get_value(gpio));
	
	return 0;
}

void rtl8723bs_power(int mode, int *updown)
{
	if (mode) {
		if (*updown) {
			rtl8723bs_gpio_ctrl("rtl8723bs_wl_regon", 1);
			mdelay(100);
		} else {
			rtl8723bs_gpio_ctrl("rtl8723bs_wl_regon", 0);
			mdelay(100);
		}
		rtl8723bs_msg("sdio wifi power state: %s\n", *updown ? "on" : "off");
	}
	return;	
}

void rtl8723bs_gpio_init(void)
{
	script_item_u val;
	script_item_value_type_e type;
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;
	int lpo_use_apclk = 0;

	type = script_get_item(wifi_para, "wifi_power", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rtl8723bs_msg("failed to fetch wifi_power\n");
	}

	axp_name[0] = val.str;
	rtl8723bs_msg("module power name %s\n", axp_name[0]);

	type = script_get_item(wifi_para, "wifi_power_ext1", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rtl8723bs_msg("failed to fetch wifi_power_ext1\n");
	}
	axp_name[1] = val.str;
	rtl8723bs_msg("module power ext1 name %s\n", axp_name[1]);

	type = script_get_item(wifi_para, "wifi_power_ext2", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		rtl8723bs_msg("failed to fetch wifi_power_ext2\n");
	}
	axp_name[2] = val.str;
	rtl8723bs_msg("module power ext2 name %s\n", axp_name[2]);

	axp_name[3] = NULL;

	type = script_get_item(wifi_para, "rtl8723bs_chip_en", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type) 
		rtl8723bs_msg("It has no rtl8723bs_chip_en gpio\n");
	else
		rtl8723bs_chip_en = val.gpio.gpio;
	sunxi_rtl8723bs_gpio_req(&val.gpio);

	type = script_get_item(wifi_para, "rtl8723bs_wl_regon", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type) 
		rtl8723bs_msg("get rtl8723bs rtl8723bs_wl_regon gpio failed\n");
	else
		rtl8723bs_wl_regon = val.gpio.gpio;
	sunxi_rtl8723bs_gpio_req(&val.gpio);


	type = script_get_item(wifi_para, "rtl8723bs_bt_regon", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type) 
		rtl8723bs_msg("get rtl8723bs rtl8723bs_bt_regon gpio failed\n");
	else
		rtl8723bs_bt_regon = val.gpio.gpio;
	sunxi_rtl8723bs_gpio_req(&val.gpio);

	type = script_get_item(wifi_para, "rtl8723bs_lpo_use_apclk", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT!=type)
		rtl8723bs_msg("get rtl8723bs rtl8723bs_lpo_use_apclk failed\n");
	else
		lpo_use_apclk = val.val;

	ops->gpio_ctrl	= rtl8723bs_gpio_ctrl;
	ops->power = rtl8723bs_power;

	rtl8723bs_module_power(1);
	
	//Enable chip
	if (rtl8723bs_chip_en != 0){
		__gpio_set_value(rtl8723bs_chip_en, 1);
	}
	
	if (lpo_use_apclk){
		rtl8723bs_config_32k_clk();
	}
}

static void rtl8723bs_config_32k_clk(void)
{
	struct clk *ap_32k = NULL;
	
	ap_32k = clk_get(NULL, "losc");
	if (!ap_32k){
		rtl8723bs_msg("Get ap 32k clk failed!\n");
		return ;
	}

	clk_enable(ap_32k);
}
