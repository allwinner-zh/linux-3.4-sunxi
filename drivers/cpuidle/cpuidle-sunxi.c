/* linux/arch/arm/mach-sunxi/cpuidle.c
 *
 * Copyright (C) 2013-2014 allwinner.
 * kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/export.h>
#include <linux/time.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/suspend.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/clockchips.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/arisc/arisc.h>

#include <asm/smp_plat.h>
#include <asm/delay.h>
#include <asm/cp15.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <mach/cpuidle-sunxi.h>

/*mask for c1 state*/
struct cpumask sunxi_cpu_try_enter_idle_mask;
struct cpumask sunxi_cpu_idle_mask;
static struct cpumask sunxi_cpu0_try_kill_mask;
static struct cpumask sunxi_cpu0_kill_mask;
DEFINE_RAW_SPINLOCK(sunxi_cpu_idle_c1_lock);

/*mask for c2state*/
static struct cpumask sunxi_core_in_c2state_mask;
DEFINE_RAW_SPINLOCK(sunxi_cpu_idle_c2_lock);

static int cpu0_response_flag = 0;
atomic_t sunxi_user_idle_driver = ATOMIC_INIT(0);
static DEFINE_PER_CPU(struct cpuidle_device, sunxi_cpuidle_device);

static int sunxi_enter_c0state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;
	local_irq_disable();
	do_gettimeofday(&before);

	cpu_do_idle();

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
				(after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time < 0? 0 : idle_time;
	return index;
}

static void sunxi_cpu0_disable_others(void *info)
{
	int cpu;
	unsigned long flags;
	struct cpumask tmp_mask;

	raw_spin_lock_irqsave(&sunxi_cpu_idle_c1_lock, flags);
	cpu0_response_flag = 0x10;
	while(!cpumask_empty(&sunxi_cpu_try_enter_idle_mask)) {
		for_each_cpu(cpu, &sunxi_cpu_try_enter_idle_mask) {
			/*send an ipi to the target cpu*/
			if(!cpumask_test_cpu(cpu,&sunxi_cpu0_try_kill_mask)) {
				cpumask_set_cpu(cpu, &sunxi_cpu0_try_kill_mask);
				cpumask_clear(&tmp_mask);
				cpumask_set_cpu(cpu, &tmp_mask);
				gic_raise_softirq(&tmp_mask, 0);
				__delay(50);
			}
			if(!cpumask_test_cpu(cpu,&sunxi_cpu0_kill_mask)){
				/*target cpu maybe two state,in wfi or wait for lock*/
				raw_spin_unlock(&sunxi_cpu_idle_c1_lock);
			   /*
				* here need delay to let other cpu get lock
				*/
				__delay(50);
			   /*
				* give a chance for others to clear
				* sunxi_cpu_try_enter_idle_mask
				*/
				raw_spin_lock(&sunxi_cpu_idle_c1_lock);
				continue;
			}
			/* if current cpu is not in wfi, try next */
			while(!SUNXI_CPU_IS_WFI_MODE(cpu)) {
				continue;
			}
			sunxi_cpuidle_power_down_cpu(cpu);
			cpumask_set_cpu(cpu, &sunxi_cpu_idle_mask);
			cpumask_clear_cpu(cpu, &sunxi_cpu0_kill_mask);
			cpumask_clear_cpu(cpu, &sunxi_cpu0_try_kill_mask);
            cpumask_clear_cpu(cpu, &sunxi_cpu_try_enter_idle_mask);
		}
	}

	cpu0_response_flag = 0;
   /*
	* here clear sunxi_cpu_kill_mask because
	* may be cpu0 send a wake up ipi to cpux,
	* but a cpu is waiting a lock to leave idle state and no change to be powered down.
	*/
	cpumask_clear(&sunxi_cpu0_try_kill_mask);
	raw_spin_unlock_irqrestore(&sunxi_cpu_idle_c1_lock, flags);
}

static int sunxi_sleep_cpu_secondary_finish(unsigned long val)
{
	unsigned long flags;
	bool need_smp_call;
	unsigned int cpu;

    /* check if need response some ipi interrupt */
	if(sunxi_pending_sgi()){
		return 1;
	}else{
		raw_spin_lock_irqsave(&sunxi_cpu_idle_c1_lock, flags);
		cpu = get_logical_index(read_cpuid_mpidr()&0xFFFF);
		need_smp_call = cpumask_empty(&sunxi_cpu_try_enter_idle_mask);
		cpumask_set_cpu(cpu, &sunxi_cpu_try_enter_idle_mask);

		if(need_smp_call && !cpu0_response_flag) {
			cpu0_response_flag = 0x01;
			raw_spin_unlock_irqrestore(&sunxi_cpu_idle_c1_lock, flags);
			smp_call_function_single(0,sunxi_cpu0_disable_others,0,0);
		}else{
			raw_spin_unlock_irqrestore(&sunxi_cpu_idle_c1_lock, flags);
		}
		asm("wfi");

		cpu = get_logical_index(read_cpuid_mpidr()&0xFFFF);
		if(cpumask_test_cpu(cpu,&sunxi_cpu0_try_kill_mask)) {
			cpumask_set_cpu(cpu,&sunxi_cpu0_kill_mask);
			sunxi_idle_cpu_die();
		}
		raw_spin_lock_irqsave(&sunxi_cpu_idle_c1_lock, flags);
		cpumask_clear_cpu(cpu, &sunxi_cpu_try_enter_idle_mask);
		raw_spin_unlock_irqrestore(&sunxi_cpu_idle_c1_lock, flags);

		return 1;
	}
}

static int sunxi_cpu_core_power_down(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int ret;
	cpu_pm_enter();
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);
	smp_wmb();
	ret = cpu_suspend(0, sunxi_sleep_cpu_secondary_finish);
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
	cpu_pm_exit();

	return index;
}
static int sunxi_enter_c1state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;

	local_irq_disable();
	do_gettimeofday(&before);

	if(dev->cpu == 0){
		cpu_do_idle();
	}else{
		sunxi_cpu_core_power_down(dev,drv,index);
	}

	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
				(after.tv_usec - before.tv_usec);
	dev->last_residency = idle_time;

	return index;
}

static int sunxi_sleep_all_core_finish(unsigned long val)
{
	unsigned int cpu;
	struct cpumask tmp;
	struct sunxi_enter_idle_para sunxi_enter_idle_para_info;
	cpu = get_logical_index(read_cpuid_mpidr()&0xFFFF);

	raw_spin_lock(&sunxi_cpu_idle_c2_lock);
	cpumask_copy(&tmp,&sunxi_core_in_c2state_mask);

	if(cpumask_equal(&tmp,&cpu_power_up_state_mask)){
		cpumask_clear_cpu(cpu,&tmp);
		if(cpumask_equal(&tmp, &sunxi_cpu_idle_mask) && (!gic_pending_irq(0))){

			raw_spin_unlock(&sunxi_cpu_idle_c2_lock);
			/* call cpus interface to power down cpu0*/
			if(!gic_pending_irq(0)){

				/*set cpu0 entry address*/
				mcpm_set_entry_vector(0, 0, cpu_resume);
				/*call cpus interface to clear its irq pending*/
				sunxi_enter_idle_para_info.flags = 0x10;
				sunxi_enter_idle_para_info.resume_addr = (void *)(virt_to_phys(mcpm_entry_point));

				arisc_enter_cpuidle(NULL,NULL,(struct sunxi_enter_idle_para *)(&sunxi_enter_idle_para_info));
				sunxi_idle_cluster_die(A7_CLUSTER);
			}else{
				asm("wfi");
			}
		}else{
			raw_spin_unlock(&sunxi_cpu_idle_c2_lock);
		}
	}else{
		raw_spin_unlock(&sunxi_cpu_idle_c2_lock);
		asm("wfi");
	}

	return 1;
}

static int sunxi_all_cpu_power_down_in_c2state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	int ret;
	cpu_pm_enter();
	//cpu_cluster_pm_enter();
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &dev->cpu);
	smp_wmb();
	ret = cpu_suspend(0, sunxi_sleep_all_core_finish);
	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &dev->cpu);
	//cpu_cluster_pm_exit();
	cpu_pm_exit();
	return index;
}
static int sunxi_enter_c2state(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct timeval before, after;
	int idle_time;
	unsigned int cpu_id;

	local_irq_disable();
	do_gettimeofday(&before);
	cpu_id = get_logical_index(read_cpuid_mpidr()&0xFFFF);

	if(dev->cpu >= MAX_CPU_IN_CLUSTER){
		/*other cpu in c3state*/
		sunxi_cpu_core_power_down(dev,drv,index);
	}else{

		raw_spin_lock(&sunxi_cpu_idle_c2_lock);
		cpumask_set_cpu(cpu_id,&sunxi_core_in_c2state_mask);
		if(dev->cpu == 0){
			if(cpumask_equal(&sunxi_core_in_c2state_mask, &cpu_power_up_state_mask)){
				raw_spin_unlock(&sunxi_cpu_idle_c2_lock);
				sunxi_all_cpu_power_down_in_c2state(dev,drv,index);
			}else{
				raw_spin_unlock(&sunxi_cpu_idle_c2_lock);
				cpu_do_idle();
			}
		}else{
			raw_spin_unlock(&sunxi_cpu_idle_c2_lock);
			sunxi_cpu_core_power_down(dev,drv,index);
		}
		raw_spin_lock(&sunxi_cpu_idle_c2_lock);
		cpumask_clear_cpu(cpu_id,&sunxi_core_in_c2state_mask);
		raw_spin_unlock(&sunxi_cpu_idle_c2_lock);
	}
	do_gettimeofday(&after);
	local_irq_enable();
	idle_time = (after.tv_sec - before.tv_sec) * USEC_PER_SEC +
				(after.tv_usec - before.tv_usec);

	dev->last_residency = idle_time < 0? 0 : idle_time;
	return index;
}

/**
 * @enter: low power process function
 * @exit_latency: latency of exit this state, based on us
 * @power_usage: power used by cpu under this state, based on mw
 * @target_residency: the minimum of time should cpu spend in
 *   this state, based on us
 */
static struct cpuidle_state sunxi_cpuidle_set[] __initdata = {
	[0] = {
		.enter			    = sunxi_enter_c0state,
		.exit_latency		= 1,
		.power_usage        = 1000,
		.target_residency	= 100,
		.flags			    = CPUIDLE_FLAG_TIME_VALID,
		.name			    = "C0",
		.desc			    = "ARM clock gating(WFI)",
	},
	[1] = {
		.enter			    = sunxi_enter_c1state,
		.exit_latency		= 3000,
		.power_usage        = 500,
		.target_residency	= 10000,
		.flags			    = CPUIDLE_FLAG_TIME_VALID,
		.name			    = "C1",
		.desc			    = "SUNXI CORE POWER DOWN",
	},
#ifdef CONFIG_CPU_FREQ_GOV_AUTO_HOTPLUG
	[2] = {
		.enter              = sunxi_enter_c2state,
		.exit_latency       = 10000,
		.power_usage        = 100,
		.target_residency   = 20000,
		.flags              = CPUIDLE_FLAG_TIME_VALID,
		.name               = "C2",
		.desc               = "SUNXI CLUSTER POWER DOWN",
	},
#endif
};

static struct cpuidle_driver sunxi_idle_driver = {
	.name		= "sunxi_idle",
	.owner		= THIS_MODULE,
};

static int __init sunxi_init_cpuidle(void)
{
	int i, max_cpuidle_state, cpu;
	struct cpuidle_device *device;

	cpumask_clear(&sunxi_cpu_try_enter_idle_mask);
	cpumask_clear(&sunxi_cpu_idle_mask);
	cpumask_clear(&sunxi_cpu_try_enter_idle_mask);
	cpumask_clear(&sunxi_cpu0_kill_mask);
	atomic_set(&sunxi_user_idle_driver,0x01);

	max_cpuidle_state = ARRAY_SIZE(sunxi_cpuidle_set);
	sunxi_idle_driver.state_count = max_cpuidle_state;

	for (i = 0; i < max_cpuidle_state; i++) {
		sunxi_idle_driver.states[i] = sunxi_cpuidle_set[i];
	}

	sunxi_idle_driver.safe_state_index = 0;
	cpuidle_register_driver(&sunxi_idle_driver);

	for_each_possible_cpu(cpu) {
		device = &per_cpu(sunxi_cpuidle_device, cpu);
		device->cpu = cpu;
		device->state_count = max_cpuidle_state;
		if (cpuidle_register_device(device)) {
			printk(KERN_ERR "CPUidle register device failed\n,");
			return -EIO;
		}
	}
	return 0;
}
device_initcall(sunxi_init_cpuidle);
