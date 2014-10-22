
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <mach/sys_config.h>
#include <mach/gpio.h>
#include <linux/proc_fs.h>
#include "wifi_pm.h"

#define wifi_pm_msg(...)    do {printk("[wifi_pm]: "__VA_ARGS__);} while(0)


struct wifi_pm_ops wifi_select_pm_ops;
static char* wifi_mod[] = {" ",
	"rtl8188eu",  /* 1 - RTL8188EU*/
	"rtl8723bs",  /* 2 - RTL8723BS*/
	"ap6181",     /* 3 - AP6181*/
	"ap6210",     /* 4 - AP6210*/
	"ap6330",     /* 5 - AP6330*/
};

int wifi_pm_get_mod_type(void)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;
	if (ops->wifi_used.val)
		return ops->module_sel.val;
	else {
		wifi_pm_msg("wifi_used = 0, please check your config !!\n");
		return 0;
	}
}
EXPORT_SYMBOL(wifi_pm_get_mod_type);

int wifi_pm_gpio_ctrl(char* name, int level)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;	
	if (ops->wifi_used.val && ops->gpio_ctrl)		
		return ops->gpio_ctrl(name, level);	
	else {		
		wifi_pm_msg("wifi_used = 0 or no gpio_ctrl, please check your config !!\n");
		return -1;	
	}
}
EXPORT_SYMBOL(wifi_pm_gpio_ctrl);

void wifi_pm_power(int on)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;
	int power = on;
	if (ops->wifi_used.val && ops->power)
		return ops->power(1, &power);
	else {
		wifi_pm_msg("wifi_used = 0 or no power ctrl, please check your config !!\n");
		return;
	}
}
EXPORT_SYMBOL(wifi_pm_power);

#ifdef CONFIG_PROC_FS
static int wifi_pm_power_stat(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	struct wifi_pm_ops *ops = (struct wifi_pm_ops *)data;
	char *p = page;
	int power = 0;

	if (ops->power)
		ops->power(0, &power);

	p += sprintf(p, "%s : power state %s\n", ops->mod_name, power ? "on" : "off");
	return p - page;
}

static int wifi_pm_power_ctrl(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	struct wifi_pm_ops *ops = (struct wifi_pm_ops *)data;
	int power = simple_strtoul(buffer, NULL, 10);
	
	power = power ? 1 : 0;
	if (ops->power)
		ops->power(1, &power);
	else
		wifi_pm_msg("No power control for %s\n", ops->mod_name);
	return sizeof(power);	
}

static inline void awwifi_procfs_attach(void)
{
	char proc_rootname[] = "driver/wifi-pm";
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;

	ops->proc_root = proc_mkdir(proc_rootname, NULL);
	if (IS_ERR(ops->proc_root))
	{
		wifi_pm_msg("failed to create procfs \"driver/wifi-pm\".\n");
	}

	ops->proc_power = create_proc_entry("power", 0644, ops->proc_root);
	if (IS_ERR(ops->proc_power))
	{
		wifi_pm_msg("failed to create procfs \"power\".\n");
	}
	ops->proc_power->data = ops;
	ops->proc_power->read_proc = wifi_pm_power_stat;
	ops->proc_power->write_proc = wifi_pm_power_ctrl;
}

static inline void awwifi_procfs_remove(void)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;
	char proc_rootname[] = "driver/wifi-pm";

	remove_proc_entry("power", ops->proc_root);
	remove_proc_entry(proc_rootname, NULL);
}
#else
static inline void awwifi_procfs_attach(void) {}
static inline void awwifi_procfs_remove(void) {}
#endif

static int wifi_pm_get_res(void)
{
	script_item_value_type_e type;
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;

	type = script_get_item(wifi_para, "wifi_used", &ops->wifi_used);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		wifi_pm_msg("failed to fetch wifi configuration!\n");
		return -1;
	}
	if (!ops->wifi_used.val) {
		wifi_pm_msg("no wifi used in configuration\n");
		return -1;
	}

	type = script_get_item(wifi_para, "wifi_sdc_id", &ops->sdio_id);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		wifi_pm_msg("failed to fetch sdio card's sdcid\n");
		return -1;
	}
	
	type = script_get_item(wifi_para, "wifi_usbc_id", &ops->usb_id);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		wifi_pm_msg("failed to fetch usb's id\n");
		return -1;
	}	

	type = script_get_item(wifi_para, "wifi_mod_sel", &ops->module_sel);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		wifi_pm_msg("failed to fetch sdio module select\n");
		return -1;
	}	
	ops->mod_name = wifi_mod[ops->module_sel.val];
	
	printk("[wifi]: select wifi: %s !!\n", wifi_mod[ops->module_sel.val]);

	return 0;
}

static int __devinit wifi_pm_probe(struct platform_device *pdev)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;

	switch (ops->module_sel.val) {
		case 1: /* RTL8188EU */
			rtl8188eu_gpio_init();
			break;
		case 2: /* RTL8723BS */
			rtl8723bs_gpio_init();
			break;
		case 3: /* AP6181 */
		case 4: /* AP6210 */
		case 5: /* AP6330 */
			ap6xxx_gpio_init();
			break;
		default:
			wifi_pm_msg("wrong sdio module select %d !\n", ops->module_sel.val);
	}

	awwifi_procfs_attach();
	wifi_pm_msg("wifi gpio init is OK !!\n");
	return 0;
}

static int __devexit wifi_pm_remove(struct platform_device *pdev)
{
	awwifi_procfs_remove();
	wifi_pm_msg("wifi gpio is released !!\n");
	return 0;
}

#ifdef CONFIG_PM
static int wifi_pm_suspend(struct device *dev)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;

	if (ops->standby)
		ops->standby(1);
	return 0;
}

static int wifi_pm_resume(struct device *dev)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;

	if (ops->standby)
		ops->standby(0);
	return 0;
}

static struct dev_pm_ops wifi_dev_pm_ops = {
	.suspend	= wifi_pm_suspend,
	.resume		= wifi_pm_resume,
};
#endif

static struct platform_device wifi_pm_dev = {
	.name           = "wifi_pm",
};

static struct platform_driver wifi_pm_driver = {
	.driver.name    = "wifi_pm",
	.driver.owner   = THIS_MODULE,
#ifdef CONFIG_PM
	.driver.pm      = &wifi_dev_pm_ops,
#endif
	.probe          = wifi_pm_probe,
	.remove         = __devexit_p(wifi_pm_remove),
};

static int __init wifi_pm_init(void)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;

	memset(ops, 0, sizeof(struct wifi_pm_ops));
	wifi_pm_get_res();
	if (!ops->wifi_used.val)
		return 0;

	platform_device_register(&wifi_pm_dev);
	return platform_driver_register(&wifi_pm_driver);
}

static void __exit wifi_pm_exit(void)
{
	struct wifi_pm_ops *ops = &wifi_select_pm_ops;
	if (!ops->wifi_used.val)
		return;

	memset(ops, 0, sizeof(struct wifi_pm_ops));
	platform_driver_unregister(&wifi_pm_driver);
}

module_init(wifi_pm_init);
module_exit(wifi_pm_exit);

