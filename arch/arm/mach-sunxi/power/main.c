/*
 * SUNXI suspend
 *
 * Copyright (C) 2014 AllWinnertech Ltd.
 * Author: xiafeng <xiafeng@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/time.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/smp.h>
#include <linux/delay.h>

#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <linux/device.h>
#include <linux/console.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/tlbflush.h>
#include <asm/mach/map.h>
#include <mach/sys_config.h>

#include <asm/smp_plat.h>
#include <asm/delay.h>
#include <asm/cp15.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <mach/cci.h>
#include <asm/mcpm.h>

#include <linux/arisc/arisc.h>
#include <linux/power/aw_pm.h>
#include <mach/hardware.h>
#include <mach/platform.h>


/* sun9i platform support two clusters,
 * cluster0 : cortex-a7,
 * cluster1 : cortex-a15.
 */
#define A7_CLUSTER	0
#define A15_CLUSTER	1
#define MAX_CLUSTERS	2

#define AW_PM_DBG   1
#ifdef PM_DBG
#undef PM_DBG
#endif
#if AW_PM_DBG
    #define PM_DBG(format,args...)   printk("[pm]"format,##args)
#else
    #define PM_DBG(format,args...)   do{}while(0)
#endif

#define BEFORE_EARLY_SUSPEND    (0x00)
#define SUSPEND_BEGIN           (0x20)
#define SUSPEND_ENTER           (0x40)
#define BEFORE_LATE_RESUME      (0x60)
#define LATE_RESUME_START       (0x80)
#define CLK_RESUME_START        (0xA0)
#define AFTER_LATE_RESUME	(0xC0)
#define RESUME_COMPLETE_FLAG    (0xE0)
#define SUSPEND_FAIL_FLAG       (0xFF)
#define FIRST_BOOT_FLAG         (0x00)

standby_type_e standby_type = NON_STANDBY;
EXPORT_SYMBOL(standby_type);
standby_level_e standby_level = STANDBY_INITIAL;
EXPORT_SYMBOL(standby_level);

static unsigned long debug_mask = 0;
static unsigned long time_to_wakeup = 0;

extern char sunxi_bromjump_start;
extern char sunxi_bromjump_end;

extern unsigned long cpu_brom_addr[2];

static inline int aw_mem_get_status(void)
{
	return readl(IO_ADDRESS(SUNXI_RTC_PBASE + 0x104));
}

static inline void aw_mem_set_status(int val)
{
	writel(val, IO_ADDRESS(SUNXI_RTC_PBASE + 0x104));
	asm volatile ("dsb");
	asm volatile ("isb");
}

/*
 * aw_pm_valid() - determine if given system sleep state is supported
 *
 * @state: suspend state
 * @return: if the state is valid, return 1, else return 0
 */
static int aw_pm_valid(suspend_state_t state)
{
	if (!((state > PM_SUSPEND_ON) && (state < PM_SUSPEND_MAX))) {
		PM_DBG("state:%d invalid!\n", state);
		return 0;
	}
	PM_DBG("state:%d valid\n", state);

	if (PM_SUSPEND_STANDBY == state)
		standby_type = SUPER_STANDBY;
	else if (PM_SUSPEND_MEM == state || PM_SUSPEND_BOOTFAST == state)
		standby_type = NORMAL_STANDBY;

	return 1;
}

/*
 * aw_pm_begin() - Initialise a transition to given system sleep state
 *
 * @state: suspend state
 * @return: return 0 for process successed
 * @note: this function will be called before devices suspened
 */
static int aw_pm_begin(suspend_state_t state)
{
	int suspend_status;
	static bool backup_console_suspend_enabled = 0;
	static bool backup_initcall_debug = 0;
	static int backup_console_loglevel = 0;
	static int backup_debug_mask = 0;

	suspend_status = aw_mem_get_status();
	if (RESUME_COMPLETE_FLAG != suspend_status) {
		printk("last suspend err, rtc:%x\n", suspend_status);
		/*
		 * adjust: loglevel, console_suspend, initcall_debug,
		 * debug_mask config, disable console suspend
		 */
		backup_console_suspend_enabled = console_suspend_enabled;
		console_suspend_enabled = 0;
		/* enable initcall_debug */
		backup_initcall_debug = initcall_debug;
		initcall_debug = 1;
		/* change loglevel to 8 */
		backup_console_loglevel = console_loglevel;
		console_loglevel = 8;
		/* change debug_mask to 0xff */
		backup_debug_mask = debug_mask;
		debug_mask |= 0x0f;
	} else {
		/* restore console suspend */
		console_suspend_enabled = backup_console_suspend_enabled;
		/* restore initcall_debug */
		initcall_debug = backup_initcall_debug;
		/* restore console_loglevel */
		console_loglevel = backup_console_loglevel;
		/* restore debug_mask */
		debug_mask = backup_debug_mask;
	}

	aw_mem_set_status(SUSPEND_BEGIN | 0x01);

	return 0;
}

/*
 * aw_pm_prepare() - Prepare for entering the system suspend state
 *
 * @return: return 0 for process successed, and negative code for error
 * @note: called after devices suspended, and before device late suspend
 *             call-back functions
 */
static int aw_pm_prepare(void)
{
	aw_mem_set_status(SUSPEND_BEGIN | 0x03);

	return 0;
}

/*
 * aw_pm_prepare_late() - Finish preparing for entering the system suspend state
 *
 * @return: return 0 for process successed, and negative code for error
 * @note: called before disabling nonboot CPUs and after device
 *        drivers' late suspend callbacks have been executed
 */
static int aw_pm_prepare_late(void)
{
	aw_mem_set_status(SUSPEND_BEGIN | 0x05);

	return 0;
}

static inline void aw_suspend_cpu_die(void)
{
	unsigned long actlr;

	/* step1: disable cache */
	asm("mrc    p15, 0, %0, c1, c0, 0" : "=r" (actlr) );
	actlr &= ~(1<<2);
	asm("mcr    p15, 0, %0, c1, c0, 0\n" : : "r" (actlr));

	/* step2: clean and ivalidate L1 cache */
	flush_cache_all();
	outer_flush_all();

	/* step3: execute a CLREX instruction */
	asm("clrex" : : : "memory", "cc");

	/* step4: switch cpu from SMP mode to AMP mode,
	 * aim is to disable cache coherency
	 */
	asm("mrc    p15, 0, %0, c1, c0, 1" : "=r" (actlr) );
	actlr &= ~(1<<6);
	asm("mcr    p15, 0, %0, c1, c0, 1\n" : : "r" (actlr));

#if (defined CONFIG_ARCH_SUN8IW6P1) || (defined CONFIG_ARCH_SUN9IW1P1)
	/* set cluster down state */
	__mcpm_outbound_leave_critical(A7_CLUSTER, CLUSTER_DOWN);
	/* disable cluster cci snoop */
	disable_cci_snoops(A7_CLUSTER);
#endif
	/* step5: execute an ISB instruction */
	isb();
	/* step6: execute a DSB instruction  */
	dsb();

	/* step7: execute a WFI instruction */
	while(1)
		asm("wfi" : : : "memory", "cc");
}

/*
 * aw_suspend_enter() - enter suspend state
 *
 * @val: useless
 * @return: no return if success
 */
static int aw_suspend_enter(unsigned long val)
{
	super_standby_para_t st_para;

	aw_mem_set_status(SUSPEND_ENTER | 0x03);

	standby_level = STANDBY_WITH_POWER_OFF;

	memset((void *)&st_para, 0, sizeof(super_standby_para_t));

	st_para.timeout = time_to_wakeup;
	if (st_para.timeout > 0)
		st_para.event |= CPUS_WAKEUP_TIMEOUT;
	st_para.gpio_enable_bitmap = 0;
	st_para.cpux_gpiog_bitmap = 0;
	st_para.pextended_standby = NULL;
	st_para.resume_code_length = 0;

	/* set cpu0 entry address */
#if (defined CONFIG_ARCH_SUN8IW6P1) || (defined CONFIG_ARCH_SUN9IW1P1)
	mcpm_set_entry_vector(0, 0, cpu_resume);
	st_para.resume_entry = virt_to_phys(&sunxi_bromjump_start);
	st_para.resume_code_src = virt_to_phys(mcpm_entry_point);
	PM_DBG("cpu resume:%x, mcpm enter:%x\n",
	       cpu_resume, virt_to_phys(mcpm_entry_point));
#else
	//cpu_brom_addr[0] = cpu_resume;
	st_para.resume_entry = virt_to_phys(&sunxi_bromjump_start);
	st_para.resume_code_src = virt_to_phys(cpu_resume);
	PM_DBG("cpu resume:%x\n", virt_to_phys(cpu_resume));
#endif
	if (unlikely(debug_mask)) {
		printk(KERN_INFO "standby paras:\n" \
		       "  event:%x\n" \
		       "  resume_code_src:%x\n" \
		       "  resume_entry:%x\n" \
		       "  timeout:%u\n" \
		       "  gpio_enable_bitmap:%x\n" \
		       "  cpux_gpiog_bitmap:%x\n" \
		       "  pextended_standby:%p\n", \
		       (unsigned int)st_para.event,
		       (unsigned int)st_para.resume_code_src,
		       (unsigned int)st_para.resume_entry,
		       (unsigned int)st_para.timeout,
		       (unsigned int)st_para.gpio_enable_bitmap,
		       (unsigned int)st_para.cpux_gpiog_bitmap,
		       st_para.pextended_standby);
	}

	if (unlikely(debug_mask)) {
		printk(KERN_INFO "system environment\n");
	}

	/* call cpus interface to enter suspend */
#ifdef CONFIG_SUNXI_ARISC
	arisc_standby_super(&st_para, NULL, NULL);
#else
#warning "ARISC driver should be configed!"
#endif

	aw_suspend_cpu_die();

	return 0;
}

/*
 * aw_pm_enter() - Enter the system sleep state
 *
 * @state: suspend state
 * @return: return 0 is process successed
 * @note: the core function for platform sleep
 */
static int aw_pm_enter(suspend_state_t state)
{
	aw_mem_set_status(SUSPEND_ENTER | 0x01);

	return cpu_suspend(0, aw_suspend_enter);
}

/*
 * aw_pm_wake() - platform wakeup
 *
 * @return: called when the system has just left a sleep state,
 *          after the nonboot CPUs have been enabled and before
 *          device drivers' early resume callbacks are executed.
 */
static void aw_pm_wake(void)
{
	aw_mem_set_status(AFTER_LATE_RESUME);
}

/*
 * aw_pm_finish() - Finish wake-up of the platform
 *
 * @return: called prior to calling device drivers' regular suspend callbacks
 */
static void aw_pm_finish(void)
{
	aw_mem_set_status(RESUME_COMPLETE_FLAG);
}

/*
 * aw_pm_end() - Notify the platform that system is in work mode now
 *
 * @note: called after resuming devices, to indicate the
 *        platform that the system has returned to the working state or the
 *        transition to the sleep state has been aborted.
 */
static void aw_pm_end(void)
{
	aw_mem_set_status(RESUME_COMPLETE_FLAG);
}

/*
 * aw_pm_recover() - Recover platform from a suspend failure
 *
 * @note: called by the PM core if the suspending of devices fails.
 */
static void aw_pm_recover(void)
{
	printk("suspend failure!\n");
	aw_mem_set_status(SUSPEND_FAIL_FLAG);
}


/* define platform_suspend_ops call-back functions, registered into PM core */
static struct platform_suspend_ops aw_pm_ops = {
    .valid = aw_pm_valid,
    .begin = aw_pm_begin,
    .prepare = aw_pm_prepare,
    .prepare_late = aw_pm_prepare_late,
    .enter = aw_pm_enter,
    .wake = aw_pm_wake,
    .finish = aw_pm_finish,
    .end = aw_pm_end,
    .recover = aw_pm_recover,
};

/*
 * aw_pm_init() - initial pm sub-system
 */
static int __init aw_pm_init(void)
{
	PM_DBG("aw_pm_init\n");
	suspend_set_ops(&aw_pm_ops);

	return 0;
}

/*
 * aw_pm_exit() - exit pm sub-system
 */
static void __exit aw_pm_exit(void)
{
	PM_DBG("aw_pm_exit!\n");
	suspend_set_ops(NULL);
}

module_param_named(debug_mask, debug_mask, ulong, S_IRUGO | S_IWUSR);
module_param_named(time_to_wakeup, time_to_wakeup, ulong, S_IRUGO | S_IWUSR);
core_initcall(aw_pm_init);
module_exit(aw_pm_exit);
