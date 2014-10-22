/*-------------------------------------------------------------------------
    
-------------------------------------------------------------------------*/
/*************************************************************************/ /*!
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/version.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#include "sgxdefs.h"
#include "services_headers.h"
#include "sysinfo.h"
#include "sgxapi_km.h"
#include "sysconfig.h"
#include "sgxinfokm.h"
#include "syslocal.h"
#include "../../srvkm/env/linux/mutex.h"

#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <mach/platform.h>
#include <linux/clk/sunxi.h>
#include <linux/clk/sunxi_name.h>
#include <linux/stat.h>
#include <mach/sys_config.h>

#define USESAMEPLL
static struct clk *h_gpu_coreclk, *h_gpu_hydclk, *h_gpu_memclk, *h_gpu_pll;
#if !defined(USESAMEPLL)
static struct clk *h_gpu_periphpll,*h_axi_gpu;
#endif
static struct regulator *gpu_power;
static int dvfs_level = 4;
static int devfs_max_level = 9;
static unsigned char  manualctl;
extern struct platform_device *gpsPVRLDMDev;
extern PVRSRV_LINUX_MUTEX gsPMMutex;
int setv_f(int level);
typedef struct{
    int volt;
    int frequency;
}volttable_t;

static volttable_t v_ftable[9] =
{
    {700000, 312000000},
    {740000, 384000000},
    {800000, 456000000},
    {840000, 504000000},
    {900000, 624000000},
    {940000, 672000000},
    {1000000,720000000},
    {1040000,744000000},
    {1100000,792000000},
};
int setv_f(int level)
{
    int currentvolt,i,ret = 0;
    currentvolt = regulator_get_voltage(gpu_power);
    for(i = 0;i< (sizeof(v_ftable)/sizeof(volttable_t));i++)
    {
        if((v_ftable[i].volt - currentvolt > -2000) && (v_ftable[i].volt - currentvolt < 2000) )
        {
            break;
        }
    }
    if(dvfs_level != i)
    {
        printk(" It adjust the volt in other place  or have an err. we use factual dvfs_level dvfs_level:[%d] factual:[%d] \n",dvfs_level,i);
        dvfs_level = i;
    }
    if(level == dvfs_level)
    {
        return ret;
    }
    printk("adjust  the dvfs_level from [%d] to [%d]\n",dvfs_level,level);
    LinuxLockMutexNested(&gsPMMutex, PVRSRV_LOCK_CLASS_POWER);
    PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D1);
    if(level > dvfs_level)
    {
        ret = regulator_set_voltage(gpu_power, v_ftable[level].volt, v_ftable[level].volt);
        if(ret)
        {
            printk("set volt err.\n");
        }
        OSWaitus(200);
    }
    if (!ret && clk_set_rate(h_gpu_pll, v_ftable[level].frequency))
	{
	    printk(KERN_ALERT "clk_set of h_gpu_pll rate %d failed\n", v_ftable[level].frequency);
    }
    if(level < dvfs_level)
    {
        ret = regulator_set_voltage(gpu_power, v_ftable[level].volt, v_ftable[level].volt);
        if(ret)
        {
            printk("set volt err.\n");
        }
    }
    OSWaitus(200);
    PVRSRVSetPowerStateKM(PVRSRV_SYS_POWER_STATE_D0);
    dvfs_level = level;
    LinuxUnLockMutex(&gsPMMutex);
    return ret;
}

static ssize_t manual_dvfs_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    int cnt = 0,bufercnt = 0,ret = 0;
    char s[3];
    ret = sprintf(buf + bufercnt, "%d voltage  frequency\n",devfs_max_level);
    while(cnt < devfs_max_level)
    {
        if(cnt == dvfs_level)
        {
           sprintf(s, "->");
        }else{
            sprintf(s, "  ");
        }
        bufercnt += ret;
        ret = sprintf(buf + bufercnt, "%s%5dmV  %5dMHz\n",s,v_ftable[cnt].volt/1000,v_ftable[cnt].frequency/1000000);
        cnt ++;
    }
    if(manualctl)
    {
        manualctl--;
    }
    return bufercnt+ret;
}
static ssize_t manual_dvfs_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    int err ;
    unsigned long val;
    err = strict_strtoul(buf, 10, &val);
    if (err)
    {
		printk("Invalid size of buffer\n");
		return err;
	}
    if( val>=0 && val <devfs_max_level)
    {
        manualctl = 2;
        err = setv_f(val);
    }

	return count;
}


static ssize_t android_dvfs_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{

    *buf = dvfs_level;
    return 1;
}
static ssize_t android_dvfs_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    int err ;
    unsigned int val;
    val = *buf;
    if(!manualctl && val>=0 && val <devfs_max_level)
    {
        err = setv_f(val);
    }

	return count;
}
static DEVICE_ATTR(android, S_IRUGO|S_IWUGO,
    android_dvfs_show, android_dvfs_store);


static DEVICE_ATTR(manual, S_IRUGO|S_IWUSR|S_IWGRP,
    manual_dvfs_show, manual_dvfs_store);

static struct attribute *gpu_attributes[] =
{
    &dev_attr_manual.attr,
    &dev_attr_android.attr,   
     NULL
};

 struct attribute_group gpu_attribute_group = {
  .name = "dvfs",
  .attrs = gpu_attributes
};

static  int get_config(void)
{
    char vftbl_name[16] = {0};
    int cnt = 0,i = 0,numberoftable;
    script_item_u val;
    script_item_value_type_e type;
    numberoftable = sizeof(v_ftable)/sizeof(volttable_t);
    sprintf(vftbl_name, "gpu_dvfs_table");
    type = script_get_item("gpu_dvfs_table", "G_LV_count", &val);
    if (SCIRPT_ITEM_VALUE_TYPE_INT != type)
    {
        printk("get gpu dvfs count err from sysconfig failed\n");
        return 0;
    }
    cnt = val.val;
    if (cnt <0)
    {
        return 0;
    }else if(cnt >numberoftable)
    {
        cnt = numberoftable;
    }
    while( i< cnt)
    {
        sprintf(vftbl_name, "G_LV%d_freq",i);
        type = script_get_item("gpu_dvfs_table", vftbl_name, &val);
        if (SCIRPT_ITEM_VALUE_TYPE_INT != type)
        {
            printk("get gpu %s  from sysconfig failed\n",vftbl_name);
            goto ret;
        }
        v_ftable[i].frequency = val.val;

        sprintf(vftbl_name, "G_LV%d_volt",i);
        type = script_get_item("gpu_dvfs_table", vftbl_name, &val);
        if (SCIRPT_ITEM_VALUE_TYPE_INT != type)
        {
            printk("get gpu %s sysconfig failed\n",vftbl_name);
            goto ret;
        }
        v_ftable[i].volt = val.val;
        i++;
    }
ret:
    devfs_max_level = i;
    memset(&v_ftable[i],0,sizeof(v_ftable)- (sizeof(volttable_t)*i));
    printk("GPU has %d dvfs level\n",devfs_max_level);
    return 0;
}


static PVRSRV_ERROR PowerLockWrap(SYS_SPECIFIC_DATA *psSysSpecData, IMG_BOOL bTryLock)
{
	if (!in_interrupt())
	{
		if (bTryLock)
		{
			int locked = mutex_trylock(&psSysSpecData->sPowerLock);
			if (locked == 0)
			{
				return PVRSRV_ERROR_RETRY;
			}
		}
		else
		{
			mutex_lock(&psSysSpecData->sPowerLock);
		}
	}

	return PVRSRV_OK;
}

static IMG_VOID PowerLockUnwrap(SYS_SPECIFIC_DATA *psSysSpecData)
{
	if (!in_interrupt())
	{
		mutex_unlock(&psSysSpecData->sPowerLock);
	}
}

PVRSRV_ERROR SysPowerLockWrap(IMG_BOOL bTryLock)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	return PowerLockWrap(psSysData->pvSysSpecificData, bTryLock);
}

IMG_VOID SysPowerLockUnwrap(IMG_VOID)
{
	SYS_DATA	*psSysData;

	SysAcquireData(&psSysData);

	PowerLockUnwrap(psSysData->pvSysSpecificData);
}

/*
 * This function should be called to unwrap the Services power lock, prior
 * to calling any function that might sleep.
 * This function shouldn't be called prior to calling EnableSystemClocks
 * or DisableSystemClocks, as those functions perform their own power lock
 * unwrapping.
 * If the function returns IMG_TRUE, UnwrapSystemPowerChange must be
 * called to rewrap the power lock, prior to returning to Services.
 */
IMG_BOOL WrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	
	return IMG_TRUE;
}

IMG_VOID UnwrapSystemPowerChange(SYS_SPECIFIC_DATA *psSysSpecData)
{
	
}

/*
 * Return SGX timining information to caller.
 */
IMG_VOID SysGetSGXTimingInformation(SGX_TIMING_INFORMATION *psTimingInfo)
{
#if !defined(NO_HARDWARE)
	PVR_ASSERT(atomic_read(&gpsSysSpecificData->sSGXClocksEnabled) != 0);
#endif
	psTimingInfo->ui32CoreClockSpeed = SYS_SGX_CLOCK_SPEED;
	psTimingInfo->ui32HWRecoveryFreq = SYS_SGX_HWRECOVERY_TIMEOUT_FREQ;
	psTimingInfo->ui32uKernelFreq = SYS_SGX_PDS_TIMER_FREQ;
#if defined(SUPPORT_ACTIVE_POWER_MANAGEMENT)
	psTimingInfo->bEnableActivePM = IMG_TRUE;
#else
	psTimingInfo->bEnableActivePM = IMG_FALSE;
#endif /* SUPPORT_ACTIVE_POWER_MANAGEMENT */
	psTimingInfo->ui32ActivePowManLatencyms = SYS_SGX_ACTIVE_POWER_LATENCY_MS;
}

/*!
******************************************************************************

 @Function  EnableSGXClocks

 @Description Enable SGX clocks

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR EnableSGXClocks(SYS_DATA *psSysData, IMG_BOOL bNoDev)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	/* SGX clocks already enabled? */
	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) != 0)
	{
		return PVRSRV_OK;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "EnableSGXClocks: Enabling SGX Clocks"));

	/*open clock*/
	if (clk_enable(h_gpu_hydclk))
	{
		printk(KERN_ALERT "GPU hyd clk enable failed\n");
	}
	//only need open one clock, all clocks of this module will be opened  //note: zchmin 2014-1-24
	if (clk_enable(h_gpu_coreclk))
	{
		printk(KERN_ALERT "GPU core clk enable failed\n");
	}
	/*if (clk_enable(h_ahb_gpu))
	{
		printk(KERN_ALERT "GPU ahb clk enable failed\n");
	}*/
	if (clk_enable(h_gpu_memclk))
	{
		printk(KERN_ALERT "GPU mem clk enable failed\n");
	}
	
	/*
	 * pm_runtime_get_sync will fail if called as part of device
	 * unregistration.
	 */
	if (!bNoDev)
	{
		/*
		 * pm_runtime_get_sync returns 1 after the module has
		 * been reloaded.
		 */
		int res = pm_runtime_get_sync(&gpsPVRLDMDev->dev);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "EnableSGXClocks: pm_runtime_get_sync failed (%d)", -res));
			//return PVRSRV_ERROR_UNABLE_TO_ENABLE_CLOCK;
		}
		psSysSpecData->bPMRuntimeGetSync = IMG_TRUE;
	}

	SysEnableSGXInterrupts(psSysData);

	/* Indicate that the SGX clocks are enabled */
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 1);

#else	/* !defined(NO_HARDWARE) */
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif	/* !defined(NO_HARDWARE) */
	return PVRSRV_OK;
}


/*!
******************************************************************************

 @Function  DisableSGXClocks

 @Description Disable SGX clocks.

 @Return   none

******************************************************************************/
IMG_VOID DisableSGXClocks(SYS_DATA *psSysData)
{
#if !defined(NO_HARDWARE)
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;

	/* SGX clocks already disabled? */
	if (atomic_read(&psSysSpecData->sSGXClocksEnabled) == 0)
	{
		return;
	}

	PVR_DPF((PVR_DBG_MESSAGE, "DisableSGXClocks: Disabling SGX Clocks"));

	SysDisableSGXInterrupts(psSysData);
	
	/*close clock*/  //note: zchmin 2014-1-24
	if (NULL == h_gpu_memclk || IS_ERR(h_gpu_memclk))
	{
		printk(KERN_CRIT "GPU mem clk handle is invalid\n");
	}
	else
	{
		clk_disable(h_gpu_memclk);
	}
	if (NULL == h_gpu_coreclk || IS_ERR(h_gpu_coreclk))
	{
		printk(KERN_CRIT "GPU core clk handle is invalid\n");
	}
	else
	{
		clk_disable(h_gpu_coreclk);
	}
	if (NULL == h_gpu_hydclk || IS_ERR(h_gpu_hydclk))
	{
		printk(KERN_CRIT "GPU hyd clk handle is invalid\n");
	}
	else
	{
		clk_disable(h_gpu_hydclk);
	}
	
	if (psSysSpecData->bPMRuntimeGetSync)
	{
		int res = pm_runtime_put_sync(&gpsPVRLDMDev->dev);
		if (res < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "DisableSGXClocks: pm_runtime_put_sync failed (%d)", -res));
		}
		psSysSpecData->bPMRuntimeGetSync = IMG_FALSE;
	}

	/* Indicate that the SGX clocks are disabled */
	atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);

#else	/* !defined(NO_HARDWARE) */
	PVR_UNREFERENCED_PARAMETER(psSysData);
#endif	/* !defined(NO_HARDWARE) */
}

/*!
******************************************************************************

 @Function  EnableSystemClocks

 @Description Setup up the clocks for the graphics device to work.

 @Return   PVRSRV_ERROR

******************************************************************************/
PVRSRV_ERROR EnableSystemClocks(SYS_DATA *psSysData)
{
	SYS_SPECIFIC_DATA *psSysSpecData = (SYS_SPECIFIC_DATA *) psSysData->pvSysSpecificData;
	int pwr_reg,currentvolt,i;

	PVR_TRACE(("EnableSystemClocks: Enabling System Clocks"));
	if (!psSysSpecData->bSysClocksOneTimeInit)
	{
		/* GPU power setup */
		gpu_power = regulator_get(NULL,"vdd-gpu");
        currentvolt = regulator_get_voltage(gpu_power);
        get_config();
        for(i = 0;i< (sizeof(v_ftable)/sizeof(volttable_t));i++)
        {
            if(v_ftable[i].volt > currentvolt)
            {
                break;
            }
        }
        if(i > 0)
        {
            i--;
        }else{
            i=4;
            regulator_set_voltage(gpu_power, v_ftable[i].volt, v_ftable[i].volt);
            printk(" we get a err gpu dvfs level,use 4....");
        }
        dvfs_level = i;
        printk("initGPU V[%dmV]  F[%d][%dMHz]\n",currentvolt/1000,dvfs_level,v_ftable[dvfs_level].frequency/1000000);

		if (IS_ERR(gpu_power))
		{
			printk(KERN_ALERT "GPU power setup failed\n");
		}
		/* Set up PLL and clock parents */	
		h_gpu_pll = clk_get(NULL,PLL_GPU_CLK);
		if (!h_gpu_pll || IS_ERR(h_gpu_pll))
		{
			printk(KERN_ALERT "clk_get of PLL_GPU_CLK failed\n");
		}
		h_gpu_coreclk = clk_get(NULL, GPUCORE_CLK);
		if (!h_gpu_coreclk || IS_ERR(h_gpu_coreclk))
		{
			printk(KERN_ALERT "clk_get of mod_gpucore failed\n");
		}
		h_gpu_memclk = clk_get(NULL, GPUMEM_CLK);
		if (!h_gpu_memclk || IS_ERR(h_gpu_memclk))
		{
			printk(KERN_ALERT "clk_get of mod_gpumem failed\n");
		}
		h_gpu_hydclk = clk_get(NULL, GPUHYD_CLK);
		if (!h_gpu_hydclk || IS_ERR(h_gpu_hydclk))
		{
			printk(KERN_ALERT "clk_get of mod_gpuhyd failed\n");
		}
#if !defined(USESAMEPLL)
        h_gpu_periphpll = clk_get(NULL,PLL_PERIPH_CLK);
		if (!h_gpu_periphpll || IS_ERR(h_gpu_periphpll))
		{
			printk(KERN_ALERT "clk_get of h_gpu_periphpll failed\n");
		}
		h_axi_gpu = clk_get(NULL, AXI0_CLK);
		if (!h_axi_gpu || IS_ERR(h_axi_gpu))
		{
			printk(KERN_ALERT "clk_get of h_axi_gpu failed\n");
		}
#endif
		mutex_init(&psSysSpecData->sPowerLock);
		atomic_set(&psSysSpecData->sSGXClocksEnabled, 0);
		psSysSpecData->bSysClocksOneTimeInit = IMG_TRUE;
	}
    /* Set PLL frequency*/
	if (clk_set_rate(h_gpu_pll, v_ftable[dvfs_level].frequency))
	{
		printk(KERN_ALERT "clk_set of h_gpu_pll rate %d failed\n", v_ftable[dvfs_level].frequency);
	}

    if (clk_set_parent(h_gpu_coreclk, h_gpu_pll))
	{
		printk(KERN_ALERT "clk_set of gpu_coreclk parent to gpu_corepll failed\n");
	}
#if defined(USESAMEPLL)
	/* Set clock parents */
	if (clk_set_parent(h_gpu_hydclk, h_gpu_pll))
	{
		printk(KERN_ALERT "clk_set of gpu_hydclk parent to gpu_hydpll failed\n");
	}
	if (clk_set_parent(h_gpu_memclk, h_gpu_pll))   //note: maybe change to periph pll-- h_gpu_periphpll, zchmin 2014-1-24
	{
		printk(KERN_ALERT "clk_set of gpu_memclk parent to gpu_hydpll failed\n");
	}
#else
	if (clk_set_parent(h_gpu_hydclk, h_axi_gpu))
	{
		printk(KERN_ALERT "clk_set of gpu_hydclk parent to gpu_hydpll failed\n");
	}
	if (clk_set_parent(h_gpu_memclk, h_gpu_periphpll))   //note: maybe change to periph pll-- h_gpu_periphpll, zchmin 2014-1-24
	{
		printk(KERN_ALERT "clk_set of gpu_memclk parent to gpu_hydpll failed\n");
	}
#endif

	//clock prepare
	if (clk_prepare(h_gpu_pll))
	{
		printk(KERN_ALERT "clk_prepare of h_gpu_pll output failed\n");
	}
	if (clk_prepare(h_gpu_memclk))
	{
		printk(KERN_ALERT "clk_prepare of h_gpu_memclk output failed\n");
	}
	if (clk_prepare(h_gpu_hydclk))
	{
		printk(KERN_ALERT "clk_prepare of h_gpu_hydclk output failed\n");
	}
	if (clk_prepare(h_gpu_coreclk))
	{
		printk(KERN_ALERT "clk_prepare of h_gpu_coreclk output failed\n");
	}

	/* Enable GPU power */
	printk(KERN_DEBUG "GPU power on\n");
	if (regulator_enable(gpu_power))
	{
		printk(KERN_ALERT "GPU power on failed\n");
	}	
	/* GPU power off gating as invalid */
	pwr_reg = readl(IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x118);
	pwr_reg &= (~(0x1));
	writel(pwr_reg, IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x118);
	OSSleepms(2);

	if(sunxi_periph_reset_deassert(h_gpu_hydclk))
	{
		printk(KERN_ALERT "reset_deassert of gpu_clk failed\n");
	}
	/* Enable PLL, in EnableSystemClocks temporarily */
	if (clk_enable(h_gpu_pll))
	{
		printk(KERN_ALERT "enable of h_gpu_pll output failed\n");
	}
	
	return PVRSRV_OK;
}

/*!
******************************************************************************

 @Function  DisableSystemClocks

 @Description Disable the graphics clocks.

 @Return  none

******************************************************************************/
IMG_VOID DisableSystemClocks(SYS_DATA *psSysData)
{
	int pwr_reg;
	
	PVR_TRACE(("DisableSystemClocks: Disabling System Clocks"));
	/*
	 * Always disable the SGX clocks when the system clocks are disabled.
	 * This saves having to make an explicit call to DisableSGXClocks if
	 * active power management is enabled.
	 */
	DisableSGXClocks(psSysData);

	if(sunxi_periph_reset_assert(h_gpu_hydclk))
	{
		printk(KERN_CRIT "reset_assert of h_gpu_hydclk failed\n");
	}
	
	/* Disable PLL, in DisableSystemClocks temporarily */
    if (NULL == h_gpu_pll || IS_ERR(h_gpu_pll))
	{
		printk(KERN_ALERT "h_gpu_pll handle is invalid\n");
	}
	else
	{
        printk("clk_disable   h_gpu_pll\n");
		clk_disable(h_gpu_pll);
	}
	//unprepare, //note: zchmin 2014-1-24
	clk_unprepare(h_gpu_coreclk);
	clk_unprepare(h_gpu_memclk);
	clk_unprepare(h_gpu_hydclk);
	clk_unprepare(h_gpu_pll);

	/* GPU power off gating valid */
	pwr_reg = readl(IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x118);
	pwr_reg |= 0x1;
	writel(pwr_reg, IO_ADDRESS(SUNXI_R_PRCM_PBASE) + 0x118);
	
	/* GPU power off */
	printk(KERN_DEBUG "GPU power off\n");
	if (regulator_is_enabled(gpu_power))
	{
		if (regulator_disable(gpu_power))
		{
			printk(KERN_ALERT "GPU power off failed\n");
		}
	}
}

PVRSRV_ERROR SysPMRuntimeRegister(void)
{
	pm_runtime_enable(&gpsPVRLDMDev->dev);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysPMRuntimeUnregister(void)
{
	pm_runtime_disable(&gpsPVRLDMDev->dev);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsInitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecificData);

	return PVRSRV_OK;
}

PVRSRV_ERROR SysDvfsDeinitialize(SYS_SPECIFIC_DATA *psSysSpecificData)
{
	PVR_UNREFERENCED_PARAMETER(psSysSpecificData);

	return PVRSRV_OK;
}

