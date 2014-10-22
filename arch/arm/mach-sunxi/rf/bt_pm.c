#include <linux/module.h>
#include <linux/init.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <mach/sys_config.h>

#define RF_MSG(...)     do {printk("[rfkill]: "__VA_ARGS__);} while(0)

#if (defined CONFIG_MMC)
extern int wifi_pm_get_mod_type(void);
extern int wifi_pm_gpio_ctrl(char* name, int level);
#else
static __inline int wifi_pm_get_mod_type(void) {return 0;}
static __inline int wifi_pm_gpio_ctrl(char* name, int level) {return -1;}
#endif

static const char bt_name[] = "rtl8723bs";
static struct rfkill *sw_rfkill;
static script_item_u val;

static int rfkill_set_power(void *data, bool blocked)
{
	unsigned int mod_sel = wifi_pm_get_mod_type();
	
	RF_MSG("rfkill set power %d\n", blocked);
	
	switch (mod_sel)
	{
		case 2: /* realtek rtl8723bs */
			if (!blocked) {
				wifi_pm_gpio_ctrl("rtl8723bs_bt_regon", 1);
			} else {
				wifi_pm_gpio_ctrl("rtl8723bs_bt_regon", 0);
			}
			break; 
		case 3: /* ap6181 */
		case 4: /* ap6210 */
		case 5: /* ap6330 */
			if (!blocked) {
				wifi_pm_gpio_ctrl("ap6xxx_bt_regon", 1);
			} else {
				wifi_pm_gpio_ctrl("ap6xxx_bt_regon", 0);
			}
			break;    
		default:
			RF_MSG("no bt module matched !!\n");
	}
	
	msleep(10);
	return 0;
}

static struct rfkill_ops sw_rfkill_ops = {
	.set_block = rfkill_set_power,
};

static int sw_rfkill_probe(struct platform_device *pdev)
{
	int ret = 0;

	sw_rfkill = rfkill_alloc(bt_name, &pdev->dev, 
					RFKILL_TYPE_BLUETOOTH, &sw_rfkill_ops, NULL);
	if (unlikely(!sw_rfkill))
		return -ENOMEM;

    rfkill_set_states(sw_rfkill, true, false);

	ret = rfkill_register(sw_rfkill);
	if (unlikely(ret)) {
		rfkill_destroy(sw_rfkill);
	}
	return ret;
}

static int sw_rfkill_remove(struct platform_device *pdev)
{
	if (likely(sw_rfkill)) {
		rfkill_unregister(sw_rfkill);
		rfkill_destroy(sw_rfkill);
	}
	return 0;
}

static struct platform_driver sw_rfkill_driver = {
	.probe = sw_rfkill_probe,
	.remove = sw_rfkill_remove,
	.driver = { 
		.name = "sunxi-rfkill",
		.owner = THIS_MODULE,
	},
};

static struct platform_device sw_rfkill_dev = {
	.name = "sunxi-rfkill",
};

static int __init sw_rfkill_init(void)
{
	script_item_value_type_e type;

	type = script_get_item("bt_para", "bt_used", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		RF_MSG("failed to fetch bt configuration!\n");
		return -1;
	}
	if (!val.val) {
		RF_MSG("init no bt used in configuration\n");
		return 0;
	}

	platform_device_register(&sw_rfkill_dev);
	return platform_driver_register(&sw_rfkill_driver);
}

static void __exit sw_rfkill_exit(void)
{
	if (!val.val) {
		RF_MSG("exit no bt used in configuration");
		return ;
	}

	platform_device_unregister(&sw_rfkill_dev);
	platform_driver_unregister(&sw_rfkill_driver);
}

late_initcall(sw_rfkill_init);
module_exit(sw_rfkill_exit);

MODULE_DESCRIPTION("sunxi-rfkill driver");
MODULE_AUTHOR("Aaron.magic<mgaic@reuuimllatech.com>");
MODULE_LICENSE(GPL);

