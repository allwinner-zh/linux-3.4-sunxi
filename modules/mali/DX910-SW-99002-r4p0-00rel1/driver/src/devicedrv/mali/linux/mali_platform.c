#include "mali_kernel_common.h"
#include "mali_osk.h"

#include <linux/mali/mali_utgard.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/module.h>  
#include <linux/clk.h>
#include <linux/clk/sunxi_name.h>
#include <linux/clk-private.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <mach/irqs.h>
#include <mach/sys_config.h>
#include <mach/platform.h>

#if defined(CONFIG_CPU_BUDGET_THERMAL)
#include <linux/cpu_budget_cooling.h>
static int cur_mode = 0;
#elif defined(CONFIG_SW_POWERNOW)
#include <mach/powernow.h>
#endif

static struct clk *mali_clk              = NULL;
static struct clk *gpu_pll               = NULL;
static struct regulator *mali_regulator  = NULL;
extern unsigned long totalram_pages;

struct __fb_addr_para 
{
	unsigned int fb_paddr;
	unsigned int fb_size;
};

extern void sunxi_get_fb_addr_para(struct __fb_addr_para *fb_addr_para);

static unsigned int vf_table[4][2] =
{
	{ 950, 128},
	{1100, 252},
	{1100, 384},
	{1100, 384},
};

static struct mali_gpu_device_data mali_gpu_data;

#if defined(CONFIG_ARCH_SUN8IW3P1) || defined(CONFIG_ARCH_SUN8IW5P1) || defined(CONFIG_ARCH_SUN8IW9P1)
static struct resource mali_gpu_resources[]=
{
    MALI_GPU_RESOURCES_MALI400_MP2_PMU(SUNXI_GPU_PBASE, SUNXI_IRQ_GPUGP, SUNXI_IRQ_GPUGPMMU, \
                                        SUNXI_IRQ_GPUPP0, SUNXI_IRQ_GPUPPMMU0, SUNXI_IRQ_GPUPP1, SUNXI_IRQ_GPUPPMMU1)
};
#elif defined(CONFIG_ARCH_SUN8IW7P1)
static struct resource mali_gpu_resources[]=
{
    MALI_GPU_RESOURCES_MALI400_MP2_PMU(SUNXI_GPU_PBASE, SUNXI_IRQ_GPU_GP, SUNXI_IRQ_GPU_GPMMU, \
                                        SUNXI_IRQ_GPU_PP0, SUNXI_IRQ_GPU_PPMMU0, SUNXI_IRQ_GPU_PP1, SUNXI_IRQ_GPU_PPMMU1)
};
#endif

static struct platform_device mali_gpu_device =
{
    .name = MALI_GPU_NAME_UTGARD,
    .id = 0,
    .dev.coherent_dma_mask = DMA_BIT_MASK(32),
};

/*
***************************************************************
 @Function	  :get_gpu_clk

 @Description :Get gpu related clocks

 @Input		  :None

 @Return	  :Zero or error code
***************************************************************
*/
static int get_gpu_clk(void)
{
#if defined(CONFIG_ARCH_SUN8IW3P1)	
	gpu_pll = clk_get(NULL, PLL8_CLK);
#elif defined(CONFIG_ARCH_SUN8IW5P1) || defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW9P1)
	gpu_pll = clk_get(NULL, PLL_GPU_CLK);
#endif
	if(!gpu_pll || IS_ERR(gpu_pll))
	{
		printk(KERN_ERR "Failed to get gpu pll clock!\n");
		return -1;
	} 
	
	mali_clk = clk_get(NULL, GPU_CLK);
	if(!mali_clk || IS_ERR(mali_clk))
	{
		printk(KERN_ERR "Failed to get mali clock!\n");     
		return -1;
	}
	
	return 0;
}

/*
***************************************************************
 @Function	  :set_freq

 @Description :Set gpu related clocks

 @Input		  :Frequency value

 @Return	  :Zero or error code
***************************************************************
*/
static int set_freq(int freq)
{
	if(clk_set_rate(gpu_pll, freq*1000*1000))
    {
        printk(KERN_ERR "Failed to set gpu pll clock!\n");
		return -1;
    }
	
	if(clk_set_rate(mali_clk, freq*1000*1000))
	{
		printk(KERN_ERR "Failed to set mali clock!\n");
		return -1;
	}
	
	return 0;
}
	
/*
***************************************************************
 @Function	  :enable_gpu_clk

 @Description :Enable gpu related clocks

 @Input		  :None

 @Return	  :None
***************************************************************
*/
void enable_gpu_clk(void)
{
	if(mali_clk->enable_count == 0)
	{
		if(clk_prepare_enable(gpu_pll))
		{
			printk(KERN_ERR "Failed to enable gpu pll!\n");
		}
		
		if(clk_prepare_enable(mali_clk))
		{
			printk(KERN_ERR "Failed to enable mali clock!\n");
		}
	}
}

/*
***************************************************************
 @Function	  :disable_gpu_clk
	
 @Description :Disable gpu related clocks
	
 @Input		  :None

 @Return	  :None
***************************************************************
*/
void disable_gpu_clk(void)
{
	if(mali_clk->enable_count == 1)
	{
		clk_disable_unprepare(mali_clk);
		clk_disable_unprepare(gpu_pll);
	}
}

#if defined(CONFIG_CPU_BUDGET_THERMAL) || defined(CONFIG_SW_POWERNOW)
/*
***************************************************************
 @Function	  :set_vol
	
 @Description :Set gpu voltage
	
 @Input		  :Voltage value

 @Return	  :Zero or error code
***************************************************************
*/
static int set_vol(int vol)
{
	if(regulator_set_voltage(mali_regulator, vol*1000, vf_table[3][0]*1000) != 0)
    {
		printk(KERN_ERR "Failed to set gpu power voltage!\n");
		return -1;
    }
	
	/* wait for power stability */
	udelay(20);
	
	return 0;
}
	
/*
***************************************************************
 @Function	  :mali_dvfs_change
		
 @Description :Change the mode of gpu

 @Input		  :level, up_flag

 @Return	  :NULL
***************************************************************
*/
void mali_dvfs_change(int level, int up_flag)
{
	mali_dev_pause();
	printk(KERN_INFO "Mali dvfs change begin\n");
	if(up_flag == 1)
	{
		if(!set_vol(vf_table[level][0]))
		{
			goto out;
		}
        if(!set_freq(vf_table[level][1]))
		{
			goto out;
		}
	}
	else
	{
		if(!set_freq(vf_table[level][1]))
		{
			goto out;
		}
        if(!set_vol(vf_table[level][0]))
		{
			goto out;
		}
	}

	printk(KERN_ERR "Gpu DVFS failed!\n");	

out:
	printk(KERN_INFO "Mali dvfs change end\n");
	mali_dev_resume();
	return;
}
#endif /* defined(CONFIG_CPU_BUDGET_THERMAL) || defined(CONFIG_SW_POWERNOW) */

#if defined(CONFIG_CPU_BUDGET_THERMAL)
/*
***************************************************************
 @Function	  :mali_throttle_notifier_call

 @Description :The callback of throttle notifier
		
 @Input		  :nfb, mode, cmd

 @Return	  :retval
***************************************************************
*/
static int mali_throttle_notifier_call(struct notifier_block *nfb, unsigned long mode, void *cmd)
{
    int retval = NOTIFY_DONE;
	
	if(mode == BUDGET_GPU_THROTTLE && cur_mode == 1)
    {
		mali_dvfs_change(2, 0);
        cur_mode = 0;
    }
    else
	{
        if(cmd && (*(int *)cmd) == 1 && cur_mode != 1)
		{
			mali_dvfs_change(3, 1);
            cur_mode = 1;
        }
		else if(cmd && (*(int *)cmd) == 0 && cur_mode != 0)
		{
			if(cur_mode == 1)
			{
				mali_dvfs_change(2, 0);
			}
			else if(cur_mode == 2)
			{
				mali_dvfs_change(2, 1);
			}
            cur_mode = 0;
        }
		else if(cmd && (*(int *)cmd) == 2 && cur_mode != 2)
		{
			mali_dvfs_change(1, 0);
			cur_mode = 2;
		}
    }	
	
	return retval;
}
static struct notifier_block mali_throttle_notifier = {
.notifier_call = mali_throttle_notifier_call,
};
#elif defined(CONFIG_SW_POWERNOW)
/*
***************************************************************
 @Function	  :mali_powernow_notifier_call

 @Description :The callback of powernow notifier

 @Input		  :this, mode, cmd

 @Return	  :0
***************************************************************
*/
static int mali_powernow_notifier_call(struct notifier_block *this, unsigned long mode, void *cmd)
{	
	if(mode == 0 && cur_mode != 0)
	{
		mali_dvfs_change(3, 1);
		cur_mode = 1;
	}
	else if(mode == 1 && cur_mode != 1)
	{
		mali_dvfs_change(2, 0);
		cur_mode = 0;
	}	
	
    return 0;
}

static struct notifier_block mali_powernow_notifier = {
	.notifier_call = mali_powernow_notifier_call,
};
#endif

/*
***************************************************************
 @Function	  :parse_fex

 @Description :Parse fex file data of gpu

 @Input		  :normal_freq, extreme_freq, extreme_vol

 @Return	  :None
***************************************************************
*/
static void parse_fex(void)
{
	script_item_u mali_used, mali_max_freq, mali_max_vol, mali_clk_freq;
	
	if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("clock", "pll8", &mali_clk_freq)) 
	{
		if(mali_clk_freq.val > 0 && mali_clk_freq.val <= vf_table[2][1])
		{
			vf_table[2][1] = mali_clk_freq.val;
		}
	}
	else
	{
		goto err_out;
	}	
	
	if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("mali_para", "mali_used", &mali_used))
	{		
		if(mali_used.val == 1)
		{
			if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("mali_para", "mali_extreme_freq", &mali_max_freq)) 
			{
                if (mali_max_freq.val > mali_clk_freq.val && mali_max_freq.val > vf_table[3][1])
				{
                    vf_table[3][1] = mali_max_freq.val;
                }    
            }
			else
			{
				goto err_out;
			}
			
			if(SCIRPT_ITEM_VALUE_TYPE_INT == script_get_item("mali_para", "mali_extreme_vol", &mali_max_vol))
			{
                if (mali_max_vol.val > 0)
				{
                    vf_table[3][0] = mali_max_vol.val;
                } 
			}
			else
			{
				goto err_out;
			}
		}
	}
	else
	{
		goto err_out;
	}	

	printk(KERN_INFO "Get mali parameter successfully\n");
	return;
	
err_out:
	printk(KERN_ERR "Failed to get mali parameter!\n");
	return;
}
/*
***************************************************************
 @Function	  :mali_platform_init

 @Description :Init the power and clocks of gpu

 @Input		  :None

 @Return	  :_MALI_OSK_ERR_OK or error code
***************************************************************
*/
_mali_osk_errcode_t mali_platform_init(void)
{
	parse_fex();
	
	/* get gpu regulator */
	mali_regulator = regulator_get(NULL, "axp22_dcdc2");	
	if (IS_ERR(mali_regulator)) 
	{
	    printk(KERN_ERR "Failed to get mali regulator!\n");
		goto err_out;
	}
	
	if(get_gpu_clk())
	{
		goto err_out;
	}
	
	set_freq(vf_table[2][1]);
	
	enable_gpu_clk();
	
#if defined(CONFIG_CPU_BUDGET_THERMAL)
	register_budget_cooling_notifier(&mali_throttle_notifier);
#elif defined(CONFIG_SW_POWERNOW)
	register_sw_powernow_notifier(&mali_powernow_notifier);
#endif

	printk(KERN_INFO "Init Mali gpu successfully\n");
    MALI_SUCCESS;

err_out:
	printk(KERN_ERR "Failed to init Mali gpu!\n");
	return _MALI_OSK_ERR_FAULT;
}

/*
***************************************************************
 @Function	  :mali_platform_device_unregister

 @Description :Unregister mali platform device

 @Input		  :None

 @Return	  :0
***************************************************************
*/
static int mali_platform_device_unregister(void)
{
    platform_device_unregister(&mali_gpu_device);
	
#if defined(CONFIG_SW_POWERNOW)
	unregister_sw_powernow_notifier(&mali_powernow_notifier);
#endif /* CONFIG_SW_POWERNOW */

	disable_gpu_clk();

	return 0;
}

/*
***************************************************************
 @Function	  :sun8i_mali_platform_device_register

 @Description :Register mali platform device

 @Input		  :None

 @Return	  :0 or error code
***************************************************************
*/
int sun8i_mali_platform_device_register(void)
{
    int err;
	
    struct __fb_addr_para fb_addr_para={0};

    sunxi_get_fb_addr_para(&fb_addr_para);

    err = platform_device_add_resources(&mali_gpu_device, mali_gpu_resources, sizeof(mali_gpu_resources) / sizeof(mali_gpu_resources[0]));
    if (0 == err)
	{
        mali_gpu_data.fb_start = fb_addr_para.fb_paddr;
        mali_gpu_data.fb_size = fb_addr_para.fb_size;
		mali_gpu_data.shared_mem_size = totalram_pages * PAGE_SIZE; /* B */

        err = platform_device_add_data(&mali_gpu_device, &mali_gpu_data, sizeof(mali_gpu_data));
        if(0 == err)
		{
            err = platform_device_register(&mali_gpu_device);
            if (0 == err){
                if(_MALI_OSK_ERR_OK != mali_platform_init())
				{
					return _MALI_OSK_ERR_FAULT;
				}
#if defined(CONFIG_PM_RUNTIME)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
				pm_runtime_set_autosuspend_delay(&(mali_gpu_device.dev), 1000);
				pm_runtime_use_autosuspend(&(mali_gpu_device.dev));
#endif
				pm_runtime_enable(&(mali_gpu_device.dev));
#endif /* CONFIG_PM_RUNTIME */

                return 0;
            }
        }

        mali_platform_device_unregister();
    }
	
    return err;
}
