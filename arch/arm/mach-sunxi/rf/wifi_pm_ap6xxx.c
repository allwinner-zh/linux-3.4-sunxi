/*
 * ap6xxx sdio wifi power management API
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
#include <linux/arisc/arisc.h>
#include "wifi_pm.h"

#define ap6xxx_msg(...)    do {printk("[ap6xxx]: "__VA_ARGS__);} while(0)

static int ap6xxx_wl_regon;
static int ap6xxx_bt_regon;
static char *axp_name[4] = {NULL};
static char *sdio_power = NULL;

static void set_ac100_32k(int cko_no);

static int sunxi_ap6xxx_gpio_req(struct gpio_config *gpio)
{
	int            ret = 0;
	char           pin_name[8] = {0};
	unsigned long  config;

	sunxi_gpio_to_name(gpio->gpio, pin_name);
	config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_FUNC, gpio->mul_sel);
	ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
	if (ret) {
        ap6xxx_msg("set gpio %s mulsel failed.\n",pin_name);
        return -1;
    }

	if (gpio->pull != GPIO_PULL_DEFAULT){
        config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_PUD, gpio->pull);
        ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
        if (ret) {
                ap6xxx_msg("set gpio %s pull mode failed.\n",pin_name);
                return -1;
        }
	}

	if (gpio->drv_level != GPIO_DRVLVL_DEFAULT){
        config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DRV, gpio->drv_level);
        ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
        if (ret) {
            ap6xxx_msg("set gpio %s driver level failed.\n",pin_name);
            return -1;
        }
    }

	if (gpio->data != GPIO_DATA_DEFAULT) {
        config = SUNXI_PINCFG_PACK(SUNXI_PINCFG_TYPE_DAT, gpio->data);
        ret = pin_config_set(SUNXI_PINCTRL, pin_name, config);
        if (ret) {
            ap6xxx_msg("set gpio %s initial val failed.\n",pin_name);
            return -1;
        }
    }

  return 0;
}

// power control by axp
static int ap6xxx_module_power(int onoff)
{
	struct regulator* wifi_ldo[4] = {(struct regulator*)0xffffffff, (struct regulator*)0xffffffff, (struct regulator*)0xffffffff, (struct regulator*)0xffffffff};
#if 0
	static int first = 1;
#endif
	int i = 0, ret = 0;

	ap6xxx_msg("ap6xxx module power set by axp.\n");

    for (i = 0; axp_name[i] != NULL; i++){
        wifi_ldo[i] = regulator_get(NULL, axp_name[i]);
        if (IS_ERR(wifi_ldo[i])) {
            ap6xxx_msg("get power regulator %s failed.\n", axp_name[i]);
            goto exit;
        }
	}

exit:
    //wifi_ldo[i] = NULL;

#if 0
    if (first) {
        ap6xxx_msg("first time\n");

        for (i = 0; !IS_ERR(wifi_ldo[i]); i++){
            ret = regulator_force_disable(wifi_ldo[i]);
            if (ret < 0) {
                ap6xxx_msg("regulator_force_disable  %s fail, return %d.\n", axp_name[i], ret);
                regulator_put(wifi_ldo[i]);
                return ret;
            }
        }
        first = 0;
    }
#endif

    if (onoff) {
        for (i = 0; !IS_ERR(wifi_ldo[i]); i++){
            if(!strcmp(axp_name[i], "axp15_sw0"))
                ;
            else if(i == 0)
                ret = regulator_set_voltage(wifi_ldo[i], 3300000, 3300000);
            else
                ret = regulator_set_voltage(wifi_ldo[i], 3000000, 3000000);

            if (ret < 0) {
                    ap6xxx_msg("regulator_set_voltage %s fail, return %d.\n", axp_name[i], ret);
                    regulator_put(wifi_ldo[i]);
                    return ret;
            }
            ret = regulator_enable(wifi_ldo[i]);
            if (ret < 0) {
                    ap6xxx_msg("regulator_enable %s fail, return %d.\n", axp_name[i], ret);
                    regulator_put(wifi_ldo[i]);
                    return ret;
            }
            ap6xxx_msg("regulator %s on.\n", axp_name[i]);
        }
	} else {
        for(i = 0; !IS_ERR(wifi_ldo[i]); i++){
            ret = regulator_disable(wifi_ldo[i]);
            if (ret < 0) {
                ap6xxx_msg("regulator_disable %s fail, return %d.\n", axp_name[i], ret);
                regulator_put(wifi_ldo[i]);
                return ret;
            }
            ap6xxx_msg("regulator %s off.\n", axp_name[i]);
        }
	}

	for(i = 0; !IS_ERR(wifi_ldo[i]); i++){
        regulator_put(wifi_ldo[i]);
	}

	return ret;
}

static int ap6xxx_gpio_ctrl(char* name, int level)
{
	int i = 0;
	int gpio = 0;
	char * gpio_name[2] = {"ap6xxx_wl_regon", "ap6xxx_bt_regon"};

	for (i = 0; i < 2; i++) {
		if (strcmp(name, gpio_name[i]) == 0) {
			switch (i)
			{
				case 0: /*ap6xxx_wl_regon*/
					gpio = ap6xxx_wl_regon;
					break;
				case 1: /*ap6xxx_bt_regon*/
					gpio = ap6xxx_bt_regon;
					break;
				default:
					ap6xxx_msg("no matched gpio.\n");
			}
			break;
		}
	}

	__gpio_set_value(gpio, level);
	printk("gpio %s set val %d, act val %d\n", name, level, __gpio_get_value(gpio));

	return 0;
}

void ap6xxx_power(int mode, int *updown)
{
	if (mode) {
	    int ret = 0;
	    struct regulator* wifi_ldo = NULL;
	    wifi_ldo = regulator_get(NULL, sdio_power);
        if (IS_ERR(wifi_ldo)) {
            ap6xxx_msg("get power regulator %s failed.\n", sdio_power);
        }
		if (*updown) {
		    if(!IS_ERR(wifi_ldo)){
                ret = regulator_set_voltage(wifi_ldo, 3000000, 3000000);
                if (ret < 0) {
                        ap6xxx_msg("regulator_set_voltage %s fail, return %d.\n", sdio_power, ret);
                }

                ret = regulator_enable(wifi_ldo);
                if (ret < 0) {
                        ap6xxx_msg("regulator_enable %s fail, return %d.\n", sdio_power, ret);
                }
                else
                    ap6xxx_msg("enable %s .\n", sdio_power);
            }
			ap6xxx_gpio_ctrl("ap6xxx_wl_regon", 1);
			mdelay(100);
        } else {
            ap6xxx_gpio_ctrl("ap6xxx_wl_regon", 0);
            if(!IS_ERR(wifi_ldo)){
                ret = regulator_disable(wifi_ldo);
                if (ret < 0) {
                        ap6xxx_msg("regulator_disable %s fail, return %d.\n", sdio_power, ret);
                }
                else
                    ap6xxx_msg("disable %s .\n", sdio_power);
            }
            mdelay(100);
        }
        regulator_put(wifi_ldo);
        ap6xxx_msg("sdio wifi power state: %s\n", *updown ? "on" : "off");
	}
	return;
}

void ap6xxx_gpio_init(void)
{
	script_item_u val;
	script_item_value_type_e type;
	int lpo_use_apclk = 0;
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;
	struct gpio_config  *gpio_p = NULL;

	type = script_get_item(wifi_para, "wifi_power", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		ap6xxx_msg("failed to fetch wifi_power\n");
	}else{
        axp_name[0] = val.str;
        ap6xxx_msg("module power name %s\n", axp_name[0]);
    }

    type = script_get_item(wifi_para, "wifi_power_ext1", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		ap6xxx_msg("failed to fetch wifi_power_ext1\n");
	}else{
        axp_name[1] = val.str;
        ap6xxx_msg("module power ext1 name %s\n", axp_name[1]);
    }

    type = script_get_item(wifi_para, "wifi_power_ext2", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		ap6xxx_msg("failed to fetch wifi_power_ext2\n");
	}else{
        axp_name[2] = val.str;
        ap6xxx_msg("module power ext2 name %s\n", axp_name[2]);
    }

    axp_name[3] = NULL;

    type = script_get_item(wifi_para, "sdio_power", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_STR != type) {
		ap6xxx_msg("failed to fetch sdio_power\n");
	}else{
        sdio_power = val.str;
        ap6xxx_msg("sdio_power name %s\n", sdio_power);
    }

	type = script_get_item(wifi_para, "ap6xxx_wl_regon", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type)
		ap6xxx_msg("get ap6xxx ap6xxx_wl_regon gpio failed\n");
	else {
		gpio_p = &val.gpio;
		ap6xxx_wl_regon = gpio_p->gpio;
		sunxi_ap6xxx_gpio_req(gpio_p);
	}

	type = script_get_item(wifi_para, "ap6xxx_bt_regon", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type)
		ap6xxx_msg("get ap6xxx ap6xxx_bt_regon gpio failed\n");
	else {
		gpio_p = &val.gpio;
		ap6xxx_bt_regon = gpio_p->gpio;
		sunxi_ap6xxx_gpio_req(gpio_p);
	}

	type = script_get_item(wifi_para, "ap6xxx_lpo_use_apclk", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT!=type)
		ap6xxx_msg("get ap6xxx ap6xxx_lpo_use_apclk failed\n");
	else
		lpo_use_apclk = val.val;

	ops->gpio_ctrl	= ap6xxx_gpio_ctrl;
	ops->power = ap6xxx_power;

	ap6xxx_module_power(1);

	if (lpo_use_apclk){
	    ap6xxx_msg("set cko%d 32k clk out\n", lpo_use_apclk);
		set_ac100_32k(lpo_use_apclk);
	}

}

static void set_ac100_32k(int cko_no)
{
    arisc_rsb_block_cfg_t rsb_data;
    unsigned char addr;
    unsigned int data;

    if(cko_no < 1 || cko_no > 3){
        ap6xxx_msg("set cko%d 32k clk out fail\n", cko_no);
        return;
    }

    addr = (unsigned char)(0xc0 + cko_no);
    data = 1; // Pre-division and Post-division is 0, CK32KBB_MUX_SEL is 32KHz from RTC. enable it.
    rsb_data.len = 1;
    rsb_data.datatype = RSB_DATA_TYPE_HWORD;
    rsb_data.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
    rsb_data.devaddr = RSB_RTSADDR_AC100;
    rsb_data.regaddr = &addr;
    rsb_data.data = &data;

    if (arisc_rsb_set_rtsaddr(RSB_DEVICE_SADDR7, RSB_RTSADDR_AC100)) {
        pr_err("%s err: config codec failed\n", __func__);
    }
    /* write registers */
    if (arisc_rsb_write_block_data(&rsb_data))
        pr_err("%s(%d) err: write reg-0x%x failed", __func__, __LINE__, 0xc0 + cko_no);

}
