#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <mach/sys_config.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include "rf_pm.h"

static char *wifi_para = "wifi_para";
struct wl_func_info  wl_info;

extern int sunxi_gpio_req(struct gpio_config *gpio);
extern int get_rf_mod_type(void);
extern char * get_rf_mod_name(void);
extern int rf_module_power(int onoff);
extern int rf_pm_gpio_ctrl(char *name, int level);
#define wifi_pm_msg(...)    do {printk("[wifi_pm]: "__VA_ARGS__);} while(0)

void wifi_pm_power(int on)
{
	int mod_num = get_rf_mod_type();
	int on_off = 0;
	
	if (on > 0){
	  on_off = 1;
	} else {
    on_off = 0;
  }
  
	switch(mod_num){
    case 1:   /* ap6181 */
    case 2:   /* ap6210 */
    case 5:   /* rtl8723bs */
    case 7:   /* ap6476 */
    case 8:   /* ap6330 */
    	if (wl_info.wl_reg_on != -1)
    	  gpio_set_value(wl_info.wl_reg_on, on_off);
      break;

    case 3:   /* rtl8188eu */
    	rf_module_power(on_off);
    	break;
    
    case 4:   /* rtl8723au */
    	break;
    	
    case 6:   /* esp8089 */
      rf_pm_gpio_ctrl("chip_en", on_off);
      mdelay(100);
      if (wl_info.wl_reg_on != -1)
    	  gpio_set_value(wl_info.wl_reg_on, on_off);
      break;    

    default:
    	wifi_pm_msg("wrong module select %d !\n", mod_num);
  }
  
  wl_info.wl_power_state = on_off;
}
EXPORT_SYMBOL(wifi_pm_power);

int get_wifi_mod_type(void)
{
	return get_rf_mod_type();
}
EXPORT_SYMBOL(get_wifi_mod_type);

int wifi_pm_gpio_ctrl(char *name, int level)
{
	int i = 0;
	int gpio = 0;
	char * gpio_name[1] = {"wl_reg_on"};

	for (i = 0; i < 1; i++) {
		if (strcmp(name, gpio_name[i]) == 0) {
			switch (i)
			{
				case 0: /*wl_reg_on*/
					gpio = wl_info.wl_reg_on;
					break;
				default:
					wifi_pm_msg("no matched gpio.\n");
			}
			break;
		}
	}

  gpio_set_value(gpio, level);
	printk("gpio %s set val %d, act val %d\n", name, level, gpio_get_value(gpio));

	return 0;
}
EXPORT_SYMBOL(wifi_pm_gpio_ctrl);

#ifdef CONFIG_PROC_FS
static int wifi_pm_power_stat(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	char *p = page;

	p += sprintf(p, "%s : wl power state %s\n", wl_info.module_name, wl_info.wl_power_state ? "on" : "off");
	return p - page;
}

static int wifi_pm_power_ctrl(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int power = simple_strtoul(buffer, NULL, 10);
	
	power = power ? 1 : 0;
  wifi_pm_power(power);
	return sizeof(power);	
}

static inline void awwifi_procfs_attach(void)
{
	char proc_rootname[] = "driver/wifi-pm";
  struct wl_func_info *wl_info_p = &wl_info;

	wl_info_p->proc_root = proc_mkdir(proc_rootname, NULL);
	if (IS_ERR(wl_info_p->proc_root))
	{
		wifi_pm_msg("failed to create procfs \"driver/wifi-pm\".\n");
	}

	wl_info_p->proc_power = create_proc_entry("power", 0644, wl_info_p->proc_root);
	if (IS_ERR(wl_info_p->proc_power))
	{
		wifi_pm_msg("failed to create procfs \"power\".\n");
	}
	
	wl_info_p->proc_power->data = wl_info_p;
	wl_info_p->proc_power->read_proc = wifi_pm_power_stat;
	wl_info_p->proc_power->write_proc = wifi_pm_power_ctrl;
}

static inline void awwifi_procfs_remove(void)
{
	struct wl_func_info *wl_info_p = &wl_info;
	char proc_rootname[] = "driver/wifi-pm";

	remove_proc_entry("power", wl_info_p->proc_root);
	remove_proc_entry(proc_rootname, NULL);
}
#else
static inline void awwifi_procfs_attach(void) {}
static inline void awwifi_procfs_remove(void) {}
#endif

static int wifi_pm_get_res(void)
{
	script_item_value_type_e type;
	script_item_u val; 
  struct gpio_config  *gpio_p = NULL;

	type = script_get_item(wifi_para, "wifi_used", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		wifi_pm_msg("failed to fetch wifi configuration!\n");
		return -1;
	}
	if (!val.val) {
		wifi_pm_msg("no wifi used in configuration\n");
		return -1;
	}
	wl_info.wifi_used = val.val;
	
	wl_info.wl_reg_on = -1;
	type = script_get_item(wifi_para, "wl_reg_on", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_PIO!=type)
		wifi_pm_msg("get wl_reg_on gpio failed\n");
	else {
		gpio_p = &val.gpio;
	  wl_info.wl_reg_on = gpio_p->gpio;
	  sunxi_gpio_req(gpio_p);
	}

	return 0;
}

static int __devinit wifi_pm_probe(struct platform_device *pdev)
{
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
	return 0;
}

static int wifi_pm_resume(struct device *dev)
{
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
	memset(&wl_info, 0, sizeof(struct wl_func_info));
	wifi_pm_get_res();
	if (!wl_info.wifi_used)
		return 0;

  wl_info.module_name = get_rf_mod_name();
  wl_info.wl_power_state = 0;
  
	platform_device_register(&wifi_pm_dev);
	return platform_driver_register(&wifi_pm_driver);
}

static void __exit wifi_pm_exit(void)
{
	if (!wl_info.wifi_used)
		return;

	platform_driver_unregister(&wifi_pm_driver);
	platform_device_unregister(&wifi_pm_dev);
}

module_init(wifi_pm_init);
module_exit(wifi_pm_exit);

