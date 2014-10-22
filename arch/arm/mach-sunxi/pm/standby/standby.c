/*
*********************************************************************************************************
*                                                    LINUX-KERNEL
*                                        AllWinner Linux Platform Develop Kits
*                                                   Kernel Module
*
*                                    (c) Copyright 2006-2011, kevin.z China
*                                             All Rights Reserved
*
* File    : standby.c
* By      : kevin.z
* Version : v1.0
* Date    : 2011-5-30 18:34
* Descript: platform standby fucntion.
* Update  : date                auther      ver     notes
*********************************************************************************************************
*/
#include "standby_i.h"

static void restore_ccu(void);
static void backup_ccu(void);
static void destory_mmu(void);
static void restore_mmu(void);
static void cache_count_init(void);
static void cache_count_get(void);
static void cache_count_output(void);

extern char *__bss_start;
extern char *__bss_end;
extern char *__standby_start;
extern char *__standby_end;

static __u32 sp_backup;
static __u32 ttb_0r_backup = 0;
#define MMU_START	(0xc0004000)
#define MMU_END 	(0xc0007ffc) //reserve 0xffff0000 range.
static __u32 mmu_backup[((MMU_END - MMU_START)>>2) + 1];

static void standby(void);

#ifdef CHECK_CACHE_TLB_MISS
int d_cache_miss_start	= 0;
int d_tlb_miss_start	= 0;
int i_tlb_miss_start	= 0;
int i_cache_miss_start	= 0;
int d_cache_miss_end	= 0;
int d_tlb_miss_end	= 0;
int i_tlb_miss_end	= 0;
int i_cache_miss_end	= 0;
#endif


/* parameter for standby, it will be transfered from sys_pwm module */
struct aw_pm_info  pm_info;
struct normal_standby_para normal_standby_para_info;

/*
*********************************************************************************************************
*                                   STANDBY MAIN PROCESS ENTRY
*
* Description: standby main process entry.
*
* Arguments  : arg  pointer to the parameter that transfered from sys_pwm module.
*
* Returns    : none
*
* Note       : the code&data may resident in cache.
*********************************************************************************************************
*/
int standby_main(struct aw_pm_info *arg)
{
	char    *tmpPtr = (char *)&__bss_start;

	if(!arg){
		/* standby parameter is invalid */
		return -1;
	}

	/* flush data and instruction tlb, there is 32 items of data tlb and 32 items of instruction tlb,
	The TLB is normally allocated on a rotating basis. The oldest entry is always the next allocated */
	mem_flush_tlb();
	
	/* clear bss segment */
	do{*tmpPtr ++ = 0;}while(tmpPtr <= (char *)&__bss_end);

	/* save stack pointer registger, switch stack to sram */
	sp_backup = save_sp();

	save_mem_status(RESUME0_START | 0X01);
	/* copy standby parameter from dram */
	standby_memcpy(&pm_info, arg, sizeof(pm_info));
	
	/* preload tlb for standby */
	mem_preload_tlb();
	
	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
	/* init module before dram enter selfrefresh */
	/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
	/*init perf counter for timing.*/
	mem_clk_init(1);
	init_perfcounters(1, 0); //need double check..
	
	save_mem_status(RESUME0_START | 0X02);
	/* initialise standby modules */
	standby_arisc_init();
	standby_clk_init();
	mem_tmr_init();
	save_mem_status(RESUME0_START | 0X03);

	if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_STANDBY)){
		//don't need init serial ,depend kernel?
		serial_init();
		save_mem_status(RESUME0_START | 0X04);
		printk("normal standby wakeup src config = 0x%x. \n", pm_info.standby_para.event);
		save_mem_status(RESUME0_START | 0X05);
	}
	save_mem_status(RESUME0_START | 0X06);

	/* init some system wake source */
	if(pm_info.standby_para.event & CPU0_WAKEUP_MSGBOX){
		if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_STANDBY)){
			printk("enable CPU0_WAKEUP_MSGBOX. \n");
		}
		mem_enable_int(INT_SOURCE_MSG_BOX);
	}
	if(pm_info.standby_para.event & CPU0_WAKEUP_KEY){
		standby_key_init();
		mem_enable_int(INT_SOURCE_LRADC);
	}

	/* process standby */
	if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_CACHE_TLB_MISS)){
		cache_count_init();
	}

	save_mem_status(RESUME0_START | 0X05);
	//busy_waiting();
	standby();
	
	save_mem_status(RESUME1_START | 0x03);
	/* check system wakeup event */
	pm_info.standby_para.event = 0;
	//actually, msg_box int will be clear by arisc-driver.
	pm_info.standby_para.event |= mem_query_int(INT_SOURCE_MSG_BOX)? 0:CPU0_WAKEUP_MSGBOX;
	pm_info.standby_para.event |= mem_query_int(INT_SOURCE_LRADC)? 0:CPU0_WAKEUP_KEY;

	//restore intc config.
	if(pm_info.standby_para.event & CPU0_WAKEUP_KEY){
		standby_key_exit();
	}

	/*check completion status: only after restore completion, access dram is allowed. */
	save_mem_status(RESUME1_START | 0x04);
	while(standby_arisc_check_restore_status()){
		if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_STANDBY)){
			printk("0xf1c20050 value: 0x%x. \n", *((volatile unsigned int *)0xf1c20050));
			printk("0xf1c20000 value: 0x%x. \n", *((volatile unsigned int *)0xf1c20000));
		}
		;
	}
	
	if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_CACHE_TLB_MISS)){
		cache_count_get();
		if(d_cache_miss_end || d_tlb_miss_end || i_tlb_miss_end || i_cache_miss_end){
		printk("=============================NOTICE====================================. \n");
		cache_count_output();
		}else{
			printk("no miss. \n");
			//cache_count_output();
		}
	}
	
	save_mem_status(RESUME1_START | 0x05);	
	/* disable watch-dog */
	mem_tmr_disable_watchdog();
	if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_STANDBY)){
		printk("after mem_tmr_disable_watchdog. \n");
	}

	/* exit standby module */
	mem_tmr_exit();
	standby_clk_exit();
	standby_arisc_exit();
	
	save_mem_status(RESUME1_START | 0x06);
	if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_STANDBY)){
		printk("after standby_arisc_exit. \n");
	}
	/* restore stack pointer register, switch stack back to dram */
	restore_sp(sp_backup);

	if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_STANDBY)){
		printk("after restore_sp. \n");
	}

	if(unlikely(pm_info.standby_para.debug_mask&PM_STANDBY_PRINT_STANDBY)){
		//restore serial clk & gpio config.
		serial_exit();
	}

	/* report which wake source wakeup system */
	arg->standby_para.event = pm_info.standby_para.event;
	arg->standby_para.axp_event = pm_info.standby_para.axp_event;

	//enable_cache();
	save_mem_status(RESUME1_START | 0x07);

	/* FIXME: seems the dram para have some err.
	 * 1. the dram crc, in normal standby case, may have crc err.
	 *	such as the region: 0x40000000 -> 0x40010000
	 * 2. need delay, such as: 5ms, before return to kernel.
	 * 3. the bug is inexplicable, the summary as follow:
	 *	3.1 rtc err code: may be 5004, or 5005, or 8000.
	 *		mean, the reason for cpu die is not sure.
	 *	3.2 add delay here, bring good effect for cpu running.
	 *		but if we have an condition expresstion before delay, 
	 *		it may have no effect. 
	 *		so, memory attribute or bus behavior may have 
	 *		contribute to this bug. to locate the real reason,
	 *		not use compile optiomize is better option.
	 *	3.3 dram crc, in normal standby case, may have crc err.
	 * 4. the right flow to correct this bug is:
	 * 	4.1 make sure dram crc is right. (right now, crc err occur.)
	 *	4.2 make sure the sramA1 memory attribute is correct.
	 *		(right now, strongly-order is in use? conflict with trm.)
	 */
	 
	if(likely(pm_info.standby_para.debug_mask&PM_STANDBY_TEST)){	
		init_perfcounters(1, 0); //need double check..
		change_runtime_env();
		delay_ms(5);	
	}	
	return 0;
}

/*
*********************************************************************************************************
*                                     SYSTEM PWM ENTER STANDBY MODE
*
* Description: enter standby mode.
*
* Arguments  : none
*
* Returns    : none;
*********************************************************************************************************
*/
static void standby(void)
{
	/*backup clk freq and voltage*/
	backup_ccu();
	
	/*notify arisc enter normal standby*/
	normal_standby_para_info.event = pm_info.standby_para.axp_event;
	normal_standby_para_info.timeout = pm_info.standby_para.timeout;
	normal_standby_para_info.gpio_enable_bitmap = pm_info.standby_para.gpio_enable_bitmap;
	
	
	standby_arisc_standby_normal((&normal_standby_para_info));

	/* cpu enter sleep, wait wakeup by interrupt */
	asm("WFI");

	/*restore cpu0 ccu: enable hosc and change to 24M. */
	restore_ccu();

	/*query wakeup src*/
	standby_arisc_query_wakeup_src((unsigned long *)&(pm_info.standby_para.axp_event));
	save_mem_status(RESUME1_START | 0x01);
	/* enable watch-dog to prevent in case dram training failed */
	mem_tmr_enable_watchdog();
	save_mem_status(RESUME1_START | 0x02);
	/* notify for cpus to: restore cpus freq and volt, restore dram */
	standby_arisc_notify_restore(STANDBY_ARISC_ASYNC);	

	return;
}

static void backup_ccu(void)
{
	return;
}

/*change clk src to hosc*/
static void restore_ccu(void)
{
	
#if(ALLOW_DISABLE_HOSC)
		/* enable LDO, ldo1, enable HOSC */
		standby_clk_ldoenable();
		standby_clk_pll1enable();
		/* delay 10ms for power be stable */
		standby_delay_cycle(1); //?ms
		//switch to 24M src
		standby_clk_core2hosc();
#endif
	
		return;
}

/*
*********************************************************************************************************
*                                    destory_mmu
*
* Description: to destory the mmu mapping, so, the tlb miss will result in an data/cache abort 
*              while not accessing dram.
* Arguments  : none
*
* Returns    : none;
*********************************************************************************************************
*/
static void destory_mmu(void)
{
	__u32 ttb_1r = 0;
	int i = 0;
	volatile  __u32 * p_mmu = (volatile  __u32 *)MMU_START;
	
	for(p_mmu = (volatile  __u32 *)MMU_START; p_mmu < (volatile  __u32 *)MMU_END; p_mmu++, i++)
	{		
		mmu_backup[i] = *p_mmu;	
		*p_mmu = 0;			
	}
	flush_dcache();

	//u need to set ttbr0 to 0xc0004000?
	//backup
	asm volatile ("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttb_0r_backup));
	//get ttbr1
	asm volatile ("mrc p15, 0, %0, c2, c0, 1" : "=r"(ttb_1r));
	//use ttbr1 to set ttbr0
	asm volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r"(ttb_1r));
	asm volatile ("dsb");
	asm volatile ("isb");

	return;
}

static void restore_mmu(void)
{
	volatile  __u32 * p_mmu = (volatile  __u32 *)MMU_START;
	int i = 0;
	
	//restore ttbr0
	asm volatile ("mcr p15, 0, %0, c2, c0, 0" : : "r"(ttb_0r_backup));
	asm volatile ("dsb");
	asm volatile ("isb");

	for(p_mmu = (volatile  __u32 *)MMU_START; p_mmu < (volatile  __u32 *)MMU_END; p_mmu++, i++)
	{
			*p_mmu = mmu_backup[i];			
	}

	flush_dcache();
	return;
}

#ifdef CHECK_CACHE_TLB_MISS

static void cache_count_init(void)
{
	set_event_counter(D_CACHE_MISS);
	set_event_counter(D_TLB_MISS);
	set_event_counter(I_CACHE_MISS);
	set_event_counter(I_TLB_MISS);
	init_event_counter(1, 0);
	d_cache_miss_start = get_event_counter(D_CACHE_MISS);
	d_tlb_miss_start = get_event_counter(D_TLB_MISS);
	i_tlb_miss_start = get_event_counter(I_TLB_MISS);
	i_cache_miss_start = get_event_counter(I_CACHE_MISS);

	return;
}

static void cache_count_get(void)
{
	d_cache_miss_end = get_event_counter(D_CACHE_MISS);
	d_tlb_miss_end = get_event_counter(D_TLB_MISS);
	i_tlb_miss_end = get_event_counter(I_TLB_MISS);
	i_cache_miss_end = get_event_counter(I_CACHE_MISS);

	return;
}

static void cache_count_output(void)
{
	printk("d_cache_miss_start = %d, d_cache_miss_end= %d. \n", d_cache_miss_start, d_cache_miss_end);
	printk("d_tlb_miss_start = %d, d_tlb_miss_end= %d. \n", d_tlb_miss_start, d_tlb_miss_end);
	printk("i_cache_miss_start = %d, i_cache_miss_end= %d. \n", i_cache_miss_start, i_cache_miss_end);
	printk("i_tlb_miss_start = %d, i_tlb_miss_end= %d. \n", i_tlb_miss_start, i_tlb_miss_end);

	return;
}

#else
static void cache_count_init(void)
{
	return;
}

static void cache_count_get(void)
{
	return;
}

static void cache_count_output(void)
{
	return;
}


#endif

