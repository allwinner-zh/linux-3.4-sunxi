/*
 * acx00-core.c  --  Device access for allwinnertech acx00
 *
 * Author: 
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mfd/core.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/mfd/acx00-mfd.h>
#include <linux/arisc/arisc.h>
#include <linux/pwm.h>

//#define AUDIO_RSB_BUS
#define SUNXI_CHIP_NAME	"ACX00-CHIP"
static unsigned int twi_id = 3;

struct regmap_config acx00_base_regmap_config = {
	.reg_bits = 8,
	.val_bits = 16,
};

#ifdef AUDIO_RSB_BUS
/**
 * acx00_rsb_reg_read: Read a single ACX00X00 register.
 *
 * @reg: Register to read.
 */
int acx00_rsb_reg_read(unsigned short reg)
{
	int	ret;
	arisc_rsb_block_cfg_t rsb_data;
	unsigned char addr;
	unsigned int data;

	addr = (unsigned char)reg;
	rsb_data.len = 1;
	rsb_data.datatype = RSB_DATA_TYPE_HWORD;
	rsb_data.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	rsb_data.devaddr = RSB_RTSADDR_ACX00X00;
	rsb_data.regaddr = &addr;
	rsb_data.data = &data;

	/* read axp registers */
	ret = arisc_rsb_read_block_data(&rsb_data);
	if (ret != 0) {
		pr_err("failed reads to 0x%02x\n", reg);
		return ret;
	}
	return data;
}
EXPORT_SYMBOL_GPL(acx00_rsb_reg_read);

/**
 * acx00_rsb_reg_write: Write a single ACX00 register.
 *
 * @reg: Register to write to.
 * @val: Value to write.
 */
int acx00_rsb_reg_write(unsigned short reg, unsigned short value)
{
	int	ret;
	arisc_rsb_block_cfg_t rsb_data;
	unsigned char addr;
	unsigned int data;

	addr = (unsigned char)reg;
	data = value;
	rsb_data.len = 1;
	rsb_data.datatype = RSB_DATA_TYPE_HWORD;
	rsb_data.msgattr = ARISC_MESSAGE_ATTR_SOFTSYN;
	rsb_data.devaddr = RSB_RTSADDR_ACX00X00;
	rsb_data.regaddr = &addr;
	rsb_data.data = &data;

	/* read axp registers */
	ret = arisc_rsb_write_block_data(&rsb_data);
	if (ret != 0) {
		pr_err("failed reads to 0x%02x\n", reg);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(acx00_rsb_reg_write);
#endif

/**
 * acx00_reg_read: Read a single ACX00 register.
 *
 * @acx00: Device to read from.
 * @reg: Register to read.
 */
int acx00_reg_read(struct acx00 *acx00, unsigned short reg)
{
#ifdef AUDIO_RSB_BUS
 	return acx00_rsb_reg_read(reg);
#else
	unsigned int val;
	int ret;
	mutex_lock(&acx00->lock);
	regmap_write(acx00->regmap, 0xfe, reg>>8);
	ret = regmap_read(acx00->regmap, reg&0xff, &val);
	mutex_unlock(&acx00->lock);
	if (ret < 0)
		return ret;
	else
		return val;
#endif
}
EXPORT_SYMBOL_GPL(acx00_reg_read);

/**
 * acx00_reg_write: Write a single ACX00 register.
 *
 * @acx00: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int acx00_reg_write(struct acx00 *acx00, unsigned short reg,
		     unsigned short val)
{
	int ret;
	#ifdef AUDIO_RSB_BUS
	return acx00_rsb_reg_write(reg, val);
	#else
//pr_err("%s,l:%d,reg:%x, reg>>8:%x, reg&0xff:%x\n", __func__, __LINE__, reg, reg>>8, reg&0xff);
	mutex_lock(&acx00->lock);
	regmap_write(acx00->regmap, 0xfe, reg>>8);
	ret = regmap_write(acx00->regmap, reg&0xff, val);
	mutex_unlock(&acx00->lock);	

	return ret;
	#endif
}
EXPORT_SYMBOL_GPL(acx00_reg_write);

static struct mfd_cell acx00_devs[] = {
	{
		.name = "acx00-codec",
	},
	{
		.name = "rtc0",
	},
	{
		.name = "tv",
	},
	{
		.name = "acx-ephy",
	},
};

/*
 * Instantiate the generic non-control parts of the device.
 */
static __devinit int acx00_device_init(struct acx00 *acx00, int irq)
{
	int ret;

	dev_set_drvdata(acx00->dev, acx00);

	ret = mfd_add_devices(acx00->dev, -1,
			      acx00_devs, ARRAY_SIZE(acx00_devs),
			      NULL, 0);
	if (ret != 0) {
		dev_err(acx00->dev, "Failed to add children: %d\n", ret);
		goto err;
	}
	pr_err("%s,line:%d\n", __func__, __LINE__);
	return 0;

err:
	mfd_remove_devices(acx00->dev);
	return ret;
}

static __devexit void acx00_device_exit(struct acx00 *acx00)
{
	mfd_remove_devices(acx00->dev);
}

static __devinit int acx00_i2c_probe(struct i2c_client *i2c,
				      const struct i2c_device_id *id)
{
	struct acx00 *acx00;
#ifndef AUDIO_RSB_BUS
	int ret = 0;
#endif

	pr_err("%s, line:%d, i2c->irq:%d\n", __func__, __LINE__, i2c->irq);
	acx00 = devm_kzalloc(&i2c->dev, sizeof(struct acx00), GFP_KERNEL);
	if (acx00 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(i2c, acx00);
	acx00->dev = &i2c->dev;
	acx00->irq = i2c->irq;
	mutex_init(&acx00->lock);
#ifdef AUDIO_RSB_BUS
	if (arisc_rsb_set_rtsaddr(RSB_DEVICE_SADDR7, RSB_RTSADDR_ACX00)) {
		pr_err("AUDIO config codec failed\n");
	}
#else
	acx00->regmap = devm_regmap_init_i2c(i2c, &acx00_base_regmap_config);
	if (IS_ERR(acx00->regmap)) {
		ret = PTR_ERR(acx00->regmap);
		dev_err(acx00->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	acx00->pwm_ac200 = pwm_request(0, NULL);
	pwm_config(acx00->pwm_ac200, 20, 41);
	pwm_enable(acx00->pwm_ac200);

	acx00_reg_write(acx00, 0x0002,0x1);
#endif

	return acx00_device_init(acx00, i2c->irq);
}

static __devexit int acx00_i2c_remove(struct i2c_client *i2c)
{
	struct acx00 *acx00 = i2c_get_clientdata(i2c);

	acx00_device_exit(acx00);

	pwm_disable(acx00->pwm_ac200);
	pwm_free(acx00->pwm_ac200);
	return 0;
}

static int acx00_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	struct i2c_adapter *adapter = client->adapter;

	pr_err("%s, l:%d, twi_id:%d, adapter->nr:%d\n", __func__, __LINE__, twi_id, adapter->nr);
	if (twi_id == adapter->nr) {
		strlcpy(info->type, SUNXI_CHIP_NAME, I2C_NAME_SIZE);
		return 0;
	} else {
		return -ENODEV;
	}
}

static const unsigned short normal_i2c[] = {0x10, I2C_CLIENT_END};

static const struct i2c_device_id acx00_id[] = {
	{"ACX00-CHIP", 2},
	{}
};

static struct i2c_driver acx00_i2c_driver = {
	.class 		= I2C_CLASS_HWMON,
	.id_table 	= acx00_id,
	.probe 		= acx00_i2c_probe,
	.remove 	= __devexit_p(acx00_i2c_remove),
	.driver 	= {
		.owner 	= THIS_MODULE,
		.name 	= "ACX00-CHIP",
	},
	.address_list = normal_i2c,
};

static int __init acx00_i2c_init(void)
{
	int ret;
pr_err("%s,l:%d\n", __func__, __LINE__);
	acx00_i2c_driver.detect = acx00_detect;
	ret = i2c_add_driver(&acx00_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register acx00 I2C driver: %d\n", ret);

	return ret;
}
subsys_initcall(acx00_i2c_init);

static void __exit acx00_i2c_exit(void)
{
	i2c_del_driver(&acx00_i2c_driver);
}
module_exit(acx00_i2c_exit);

MODULE_DESCRIPTION("Core support for the ACX00X00 audio CODEC");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("huangxin<huangxin@allwinnertech.com>");
