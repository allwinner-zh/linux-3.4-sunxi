/*
 * Copyright (C) 2013 Allwinnertech, kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <mach/clock.h>
#include <mach/platform.h>
#include <mach/hardware.h>
#include <mach/sys_config.h>
#include <linux/dma-mapping.h>
#include <mach/dma.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include "nand_lib.h"
#include "nand_blk.h"

struct clk *pll6;
struct clk *nand0_clk;
struct clk *ahb_nand0;
struct clk *nand1_clk;
struct clk *ahb_nand1;


int seq=0;
int nand_handle=0;


#ifdef __LINUX_NAND_SUPPORT_INT__
static int nandrb_ready_flag[2] = {1, 1};
static int nanddma_ready_flag[2] = {1, 1};
static DECLARE_WAIT_QUEUE_HEAD(NAND_RB_WAIT_CH0);
static DECLARE_WAIT_QUEUE_HEAD(NAND_RB_WAIT_CH1);

static DECLARE_WAIT_QUEUE_HEAD(NAND_DMA_WAIT_CH0);
static DECLARE_WAIT_QUEUE_HEAD(NAND_DMA_WAIT_CH1);

#endif

//#define RB_INT_MSG_ON
#ifdef  RB_INT_MSG_ON
#define dbg_rbint(fmt, args...) printk(fmt, ## args)
#else
#define dbg_rbint(fmt, ...)  ({})
#endif

#define RB_INT_WRN_ON
#ifdef  RB_INT_WRN_ON
#define dbg_rbint_wrn(fmt, args...) printk(fmt, ## args)
#else
#define dbg_rbint_wrn(fmt, ...)  ({})
#endif

//#define DMA_INT_MSG_ON
#ifdef  DMA_INT_MSG_ON
#define dbg_dmaint(fmt, args...) printk(fmt, ## args)
#else
#define dbg_dmaint(fmt, ...)  ({})
#endif

#define DMA_INT_WRN_ON
#ifdef  DMA_INT_WRN_ON
#define dbg_dmaint_wrn(fmt, args...) printk(fmt, ## args)
#else
#define dbg_dmaint_wrn(fmt, ...)  ({})
#endif

/*
*********************************************************************************************************
*                                               DMA TRANSFER END ISR
*
* Description: dma transfer end isr.
*
* Arguments  : none;
*
* Returns    : EPDK_TRUE/ EPDK_FALSE
*********************************************************************************************************
*/
int NAND_ClkRequest(__u32 nand_index)
{
	long    rate;

	pll6 = clk_get(NULL, "sys_pll6");
	if(NULL == pll6 || IS_ERR(pll6)) {
		printk("%s: clock handle invalid!\n", __func__);
		return -1;
	}

	rate = clk_get_rate(pll6);
		//printk("%s: get pll6 rate %dHZ\n", __func__, (__u32)rate);

	if(nand_index == 0) {
		nand0_clk = clk_get(NULL, "mod_nand0");
		ahb_nand0 = clk_get(NULL, "ahb_nand0");

		/* 检查clock句柄有效性 */
		if(NULL == nand0_clk || IS_ERR(nand0_clk)
			|| NULL == ahb_nand0 || IS_ERR(ahb_nand0)){
			printk("%s: clock handle invalid!\n", __func__);
			return -1;
		}

		/* 设置 NAND0 控制器的父时钟为PLL6 */
		if(clk_set_parent(nand0_clk, pll6))
			printk("%s: set nand0_clk parent to pll6 failed!\n", __func__);

		/* 设置NAND0 控制器的频率为50Mhz */
		rate = clk_round_rate(nand0_clk, 20000000);
		if(clk_set_rate(nand0_clk, rate))
			printk("%s: set nand0_clk rate to %dHZ failed!\n", __func__, (__u32)rate);

		/* 打开NAND0 的AHB GATING */
		if(clk_enable(ahb_nand0))
			printk("%s: enable ahb_nand0 failed!\n", __func__);
		/* 打开NAND0 的模块 GATING */
		if(clk_enable(nand0_clk))
			printk("%s: enable nand0_clk failed!\n", __func__);
		/* 放开NAND0 的复位 */
		if(clk_reset(nand0_clk, AW_CCU_CLK_RESET))
			printk("%s: RESET nand0_clk failed!\n", __func__);
		if(clk_reset(nand0_clk, AW_CCU_CLK_NRESET))
			printk("%s: NRESET nand0_clk failed!\n", __func__);
	} else if(nand_index == 1) {
		nand1_clk = clk_get(NULL, "mod_nand1");
		ahb_nand1 = clk_get(NULL, "ahb_nand1");

		/* 检查clock句柄有效性 */
		if(NULL == nand1_clk || IS_ERR(nand1_clk)
		|| NULL == ahb_nand1 || IS_ERR(ahb_nand1)){
			printk("%s: clock handle invalid!\n", __func__);
			return -1;
		}

		/* 设置 NAND1 控制器的父时钟为PLL6 */
		if(clk_set_parent(nand1_clk, pll6))
			printk("%s: set nand1_clk parent to pll6 failed!\n", __func__);

		/* 设置NAND1 控制器的频率为50Mhz */
		rate = clk_round_rate(nand1_clk, 20000000);
		if(clk_set_rate(nand1_clk, rate))
			printk("%s: set nand1_clk rate to %dHZ failed!\n", __func__, (__u32)rate);

		/* 打开NAND1 的AHB GATING */
		if(clk_enable(ahb_nand1))
			printk("%s: enable ahb_nand1 failed!\n", __func__);
		/* 打开NAND1 的模块 GATING */
		if(clk_enable(nand1_clk))
			printk("%s: enable nand1_clk failed!\n", __func__);
		/* 放开NAND1 的复位 */
		if(clk_reset(nand1_clk, AW_CCU_CLK_RESET))
			printk("%s: RESET nand1_clk failed!\n", __func__);
		if(clk_reset(nand1_clk, AW_CCU_CLK_NRESET))
			printk("%s: NRESET nand1_clk failed!\n", __func__);
	} else {
		printk("NAND_ClkRequest, nand_index error: 0x%x\n", nand_index);
		return -1;
	}

	return 0;

}

void NAND_ClkRelease(__u32 nand_index)
{


	if(nand_index == 0)
	{
		if(NULL != nand0_clk && !IS_ERR(nand0_clk)) {
			/* 使NAND0控制器进入复位状态 */
			if(clk_reset( nand0_clk, AW_CCU_CLK_RESET))
				printk("%s: RESET nand0_clk failed!\n", __func__);
			/* 关闭NAND0的模块时钟 */
			clk_disable(nand0_clk);
			/* 释放nand0_clk句柄 */
			clk_put(nand0_clk);
			nand0_clk = NULL;
		}
		if(NULL != ahb_nand0 && !IS_ERR(ahb_nand0)) {
			/* 关闭NAND00的AHB GATING */
			clk_disable(ahb_nand0);
			/* 释放ahb_sdc0句柄 */
			clk_put(ahb_nand0);
			ahb_nand0 = NULL;
		}
	}
	else if(nand_index == 1)
	{
		if(NULL != nand1_clk && !IS_ERR(nand0_clk)) {
			/* 使NAND0控制器进入复位状态 */
			if(clk_reset( nand1_clk, AW_CCU_CLK_RESET))
				printk("%s: RESET nand0_clk failed!\n", __func__);
			/* 关闭NAND0的模块时钟 */
			clk_disable(nand1_clk);
			/* 释放nand0_clk句柄 */
			clk_put(nand1_clk);
			nand0_clk = NULL;
		}
		if(NULL != ahb_nand1 && !IS_ERR(ahb_nand1)) {
			/* 关闭NAND00的AHB GATING */
			clk_disable(ahb_nand1);
			/* 释放ahb_sdc0句柄 */
			clk_put(ahb_nand1);
			ahb_nand1 = NULL;
		}
	}
	else
	{
		printk("NAND_ClkRequest, nand_index error: 0x%x\n", nand_index);
	}

	if(NULL != pll6 && !IS_ERR(pll6)) {
		/* 释放pll6句柄 */
		clk_put(pll6);
		pll6 = NULL;
	}
}

int NAND_SetClk(__u32 nand_index, __u32 nand_clk)
{
	long    rate;

	if(nand_index == 0) {
		/* 检查clock句柄有效性 */
		if(NULL == nand0_clk || IS_ERR(nand0_clk)){
			printk("%s: clock handle invalid!\n", __func__);
			return -1;
		}

		/* 设置NAND0 控制器的频率为50Mhz */
		rate = clk_round_rate(nand0_clk, nand_clk*2000000);
		if(clk_set_rate(nand0_clk, rate))
			printk("%s: set nand0_clk rate to %dHZ failed! nand_clk: 0x%x\n", __func__, (__u32)rate, nand_clk);

	} else if(nand_index == 1) {
		/* 检查clock句柄有效性 */
		if(NULL == nand1_clk || IS_ERR(nand1_clk)) {
			printk("%s: clock handle invalid!\n", __func__);
			return -1;
		}

		/* 设置NAND1 控制器的频率为50Mhz */
		rate = clk_round_rate(nand1_clk, nand_clk*2000000);
		if(clk_set_rate(nand1_clk, rate))
			printk("%s: set nand1_clk rate to %dHZ failed! nand_clk: 0x%x\n", __func__, (__u32)rate, nand_clk);
	} else {
		printk("NAND_SetClk, nand_index error: 0x%x\n", nand_index);
		return -1;
	}

	return 0;
}

int NAND_GetClk(__u32 nand_index)
{
	long    rate;

	if(nand_index == 0) {
		/* 检查clock句柄有效性 */
		if(NULL == nand0_clk || IS_ERR(nand0_clk)){
			printk("%s: clock handle invalid!\n", __func__);
			return -1;
		}

		/* 设置NAND0 控制器的频率为50Mhz */
		rate = clk_get_rate(nand0_clk);
	} else if(nand_index == 1) {
		/* 检查clock句柄有效性 */
		if(NULL == nand1_clk || IS_ERR(nand1_clk)){
			printk("%s: clock handle invalid!\n", __func__);
			return -1;
		}

		/* 设置NAND1 控制器的频率为50Mhz */
		rate = clk_get_rate(nand1_clk);
	} else {
		printk("NAND_GetClk, nand_index error: 0x%x\n", nand_index);
		return -1;
	}

	return (rate/2000000);
}

void eLIBs_CleanFlushDCacheRegion_nand(void *adr, size_t bytes)
{
	__cpuc_flush_dcache_area(adr, bytes + (1 << 5) * 2 - 2);
}


__s32 NAND_CleanFlushDCacheRegion(__u32 buff_addr, __u32 len)
{
	//eLIBs_CleanFlushDCacheRegion_nand((void *)buff_addr, (size_t)len);
    return 0;
}


__u32 tmp_dma_phy_addr[2] = {0, 0};
__u32 tmp_dma_len[2] = {0, 0};

__u32 NAND_DMASingleMap(__u32 rw, __u32 buff_addr, __u32 len)
{
    __u32 mem_addr;
    __u32 nand_index = NAND_GetCurrentCH();

    if (rw == 1)
    {
	    mem_addr = (__u32)dma_map_single(NULL, (void *)buff_addr, len, DMA_TO_DEVICE);
	}
	else
    {
	    mem_addr = (__u32)dma_map_single(NULL, (void *)buff_addr, len, DMA_FROM_DEVICE);
	}

	tmp_dma_phy_addr[nand_index] = mem_addr;
	tmp_dma_len[nand_index] = len;

	return mem_addr;
}

__u32 NAND_DMASingleUnmap(__u32 rw, __u32 buff_addr, __u32 len)
{
	__u32 mem_addr = buff_addr;
	__u32 nand_index = NAND_GetCurrentCH();

    if(tmp_dma_phy_addr[nand_index]!=mem_addr)
    {
    	printk("NAND_DMASingleUnmap, dma addr not match! nand_index: 0x%x, tmp_dma_phy_addr:0x%x, mem_addr: 0x%x\n", nand_index,tmp_dma_phy_addr[nand_index], mem_addr);
	mem_addr = tmp_dma_phy_addr[nand_index];
    }

    if(tmp_dma_len[nand_index] != len)
    {
	    printk("NAND_DMASingleUnmap, dma len not match! nand_index: 0x%x, tmp_dma_len:0x%x, len: 0x%x\n", nand_index,tmp_dma_len[nand_index], len);
	    len = tmp_dma_len[nand_index];
	}


	if (rw == 1)
	{
	    dma_unmap_single(NULL, (dma_addr_t)mem_addr, len, DMA_TO_DEVICE);
	}
	else
	{
	    dma_unmap_single(NULL, (dma_addr_t)mem_addr, len, DMA_FROM_DEVICE);
	}

	return mem_addr;
}



#ifdef __LINUX_SUPPORT_DMA_INT__
void NAND_EnDMAInt(void)
{
	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);

	//clear interrupt
	#if 0
	NFC_DmaIntClearStatus();
	if(NFC_DmaIntGetStatus())
	{
		dbg_rbint_wrn("nand clear dma int status error in int enable \n");
		dbg_rbint_wrn("dma status: 0x%x\n", NFC_DmaIntGetStatus());
	}
	#endif
	nanddma_ready_flag[nand_index] = 0;

	//enable interrupt
	NFC_DmaIntEnable();

	dbg_rbint("dma int en\n");
}

void NAND_ClearDMAInt(void)
{
	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);

	//disable interrupt
	NFC_DmaIntDisable();
	dbg_dmaint("ch %d dma int clear\n", nand_index);

	//clear interrupt
	//NFC_DmaIntClearStatus();
	//if(NFC_DmaIntGetStatus())
	//{
	//	dbg_dmaint_wrn("nand clear dma int status error in int clear \n");
	//	dbg_dmaint_wrn("dma status: 0x%x\n", NFC_DmaIntGetStatus());
	//}

	nanddma_ready_flag[nand_index] = 0;
}

void NAND_DMAInterrupt(void)
{
	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);

	dbg_dmaint("ch %d dma int occor! \n", nand_index);
	if(!NFC_DmaIntGetStatus())
	{
		dbg_dmaint_wrn("nand dma int late \n");
	}

    NAND_ClearDMAInt();

    nanddma_ready_flag[nand_index] = 1;
    if(nand_index == 0)
	wake_up( &NAND_DMA_WAIT_CH0 );
    else if(nand_index == 1)
	wake_up( &NAND_DMA_WAIT_CH1 );


}

__s32 NAND_WaitDmaFinish(void)
{
	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);

	NAND_EnDMAInt();

	//wait_event(NAND_RB_WAIT, nandrb_ready_flag);
	dbg_dmaint("ch %d dma wait\n", nand_index);

	if(nanddma_ready_flag[nand_index])
	{
		dbg_dmaint("ch %d fast dma int\n", nand_index);
		NAND_ClearDMAInt();
		return 0;
	}

	if(NFC_DmaIntGetStatus())
	{
		dbg_dmaint("ch %d dma fast ready \n", nand_index);
		NAND_ClearDMAInt();
		return 0;
	}

    	if(nand_index == 0)
    	{
    		if(wait_event_timeout(NAND_DMA_WAIT_CH0, nanddma_ready_flag[nand_index], 1*HZ)==0)
		{
			dbg_dmaint_wrn("nand wait dma int time out, ch: 0x%d, dma_status: %x, dma_ready_flag: 0x%x\n", nand_index, NFC_DmaIntGetStatus(),nanddma_ready_flag[nand_index]);
			while(!NFC_DmaIntGetStatus());
			NAND_ClearDMAInt();
		}
		else
		{
			dbg_dmaint("nand wait dma ready ok\n");
			NAND_ClearDMAInt();
		}
    	}
	else if(nand_index ==1)
	{
		if(wait_event_timeout(NAND_DMA_WAIT_CH1, nanddma_ready_flag[nand_index], 1*HZ)==0)
		{
			dbg_dmaint_wrn("nand wait dma int time out, ch: 0x%d, dma_status: %x, dma_ready_flag: 0x%x\n", nand_index, NFC_DmaIntGetStatus(),nanddma_ready_flag[nand_index]);
			while(!NFC_DmaIntGetStatus());
			NAND_ClearDMAInt();
		}
		else
		{
			dbg_dmaint("nand wait dma ready ok\n");
			NAND_ClearDMAInt();
		}
	}
	else
	{
		NAND_ClearDMAInt();
		printk("NAND_WaitDmaFinish, error nand_index: 0x%x\n", nand_index);
	}

    return 0;
}

#else
__s32 NAND_WaitDmaFinish(void)
{
    return 0;
}

#endif


#ifdef __LINUX_SUPPORT_RB_INT__
void NAND_EnRbInt(void)
{
	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);

	//clear interrupt
	NFC_RbIntClearStatus();

	nandrb_ready_flag[nand_index] = 0;

	//enable interrupt
	NFC_RbIntEnable();

	dbg_rbint("rb int en\n");
}


void NAND_ClearRbInt(void)
{
    	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);

	//disable interrupt
	NFC_RbIntDisable();;

	dbg_rbint("rb int clear\n");

	//clear interrupt
	NFC_RbIntClearStatus();

	//check rb int status
	if(NFC_RbIntGetStatus())
	{
		dbg_rbint_wrn("nand %d clear rb int status error in int clear \n", nand_index);
	}

	nandrb_ready_flag[nand_index] = 0;
}


void NAND_RbInterrupt(void)
{
	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);

	dbg_rbint("rb int occor! \n");
	if(!NFC_RbIntGetStatus())
	{
		dbg_rbint_wrn("nand rb int late \n");
	}

    NAND_ClearRbInt();

    nandrb_ready_flag[nand_index] = 1;
    if(nand_index == 0)
	wake_up( &NAND_RB_WAIT_CH0 );
    else if(nand_index ==1)
    	wake_up( &NAND_RB_WAIT_CH1 );

}

__s32 NAND_WaitRbReady(void)
{
	__u32 rb;
	__u32 nand_index;

	nand_index = NAND_GetCurrentCH();
	if(nand_index >1)
		printk("NAND_ClearDMAInt, nand_index error: 0x%x\n", nand_index);


	NAND_EnRbInt();

	//wait_event(NAND_RB_WAIT, nandrb_ready_flag);
	dbg_rbint("rb wait \n");

	if(nandrb_ready_flag[nand_index])
	{
		dbg_rbint("fast rb int\n");
		NAND_ClearRbInt();
		return 0;
	}

	rb=  NFC_GetRbSelect();
	if(NFC_GetRbStatus(rb))
	{
		dbg_rbint("rb %u fast ready \n", rb);
		NAND_ClearRbInt();
		return 0;
	}

	//printk("NAND_WaitRbReady, ch %d\n", nand_index);

	if(nand_index == 0)
	{
		if(wait_event_timeout(NAND_RB_WAIT_CH0, nandrb_ready_flag[nand_index], 1*HZ)==0)
		{
			dbg_rbint_wrn("nand wait rb int time out, ch: %d\n", nand_index);
			NAND_ClearRbInt();
		}
		else
		{	NAND_ClearRbInt();
			dbg_rbint("nand wait rb ready ok\n");
		}
	}
	else if(nand_index ==1)
	{
		if(wait_event_timeout(NAND_RB_WAIT_CH1, nandrb_ready_flag[nand_index], 1*HZ)==0)
		{
			dbg_rbint_wrn("nand wait rb int time out, ch: %d\n", nand_index);
			NAND_ClearRbInt();
		}
		else
		{	NAND_ClearRbInt();
			dbg_rbint("nand wait rb ready ok\n");
		}
	}
	else
	{
		NAND_ClearRbInt();
	}

    return 0;
}
#else
__s32 NAND_WaitRbReady(void)
{
    return 0;
}
#endif

#define NAND_CH0_INT_EN_REG    (0xf1c03000+0x8)
#define NAND_CH1_INT_EN_REG    (0xf1c05000+0x8)
#define NAND_CH0_INT_ST_REG    (0xf1c03000+0x4)
#define NAND_CH1_INT_ST_REG    (0xf1c05000+0x4)
#define NAND_RB_INT_BITMAP     (0x1)
#define NAND_DMA_INT_BITMAP    (0x4)
#define __NAND_REG(x)    (*(volatile unsigned int   *)(x))


void NAND_Interrupt(__u32 nand_index)
{
	if(nand_index >1)
		printk("NAND_Interrupt, nand_index error: 0x%x\n", nand_index);
#ifdef __LINUX_NAND_SUPPORT_INT__

    //printk("nand interrupt!\n");
#ifdef __LINUX_SUPPORT_RB_INT__
#if 0

    if(NFC_RbIntOccur())
    {
        dbg_rbint("nand rb int\n");
        NAND_RbInterrupt();
    }
#else
    if(nand_index == 0)
    {
    	if((__NAND_REG(NAND_CH0_INT_EN_REG)&NAND_RB_INT_BITMAP)&&(__NAND_REG(NAND_CH0_INT_ST_REG)&NAND_RB_INT_BITMAP))
		NAND_RbInterrupt();
    }
    else if(nand_index == 1)
    {
    	if((__NAND_REG(NAND_CH1_INT_EN_REG)&NAND_RB_INT_BITMAP)&&(__NAND_REG(NAND_CH1_INT_ST_REG)&NAND_RB_INT_BITMAP))
		NAND_RbInterrupt();
    }
#endif
#endif

#ifdef __LINUX_SUPPORT_DMA_INT__
#if 0
    if(NFC_DmaIntOccur())
    {
        dbg_dmaint("nand dma int\n");
        NAND_DMAInterrupt();
    }
#else
    if(nand_index == 0)
    {
    	if((__NAND_REG(NAND_CH0_INT_EN_REG)&NAND_DMA_INT_BITMAP)&&(__NAND_REG(NAND_CH0_INT_ST_REG)&NAND_DMA_INT_BITMAP))
		NAND_DMAInterrupt();
    }
    else if(nand_index == 1)
    {
    	if((__NAND_REG(NAND_CH1_INT_EN_REG)&NAND_DMA_INT_BITMAP)&&(__NAND_REG(NAND_CH1_INT_ST_REG)&NAND_DMA_INT_BITMAP))
		NAND_DMAInterrupt();
    }

#endif

#endif

#endif
}


__u32 NAND_VA_TO_PA(__u32 buff_addr)
{
    return (__u32)(__pa((void *)buff_addr));
}

void NAND_PIORequest(__u32 nand_index)
{
	int	cnt, i;
	script_item_u *list = NULL;

	if(nand_index == 0)
	{
		//printk("[NAND] nand0 gpio_request\n");

		/* 获取gpio list */
		cnt = script_get_pio_list("nand0_para", &list);
		if(0 == cnt) {
			printk("get nand0_para gpio list failed\n");
			return;
		}
		/* 申请gpio */
		for(i = 0; i < cnt; i++)
			if(0 != gpio_request(list[i].gpio.gpio, NULL))
				printk("request nand0_para gpio list failed\n");
		/* 配置gpio list */
		if(0 != sw_gpio_setall_range(&list[0].gpio, cnt))
			printk("sw_gpio_setall_range failed\n");
	}
	else if(nand_index ==1)
	{
		//printk("[NAND] nand1 gpio_request\n");

		cnt = script_get_pio_list("nand1_para", &list);
		if(0 == cnt) {
			printk("get nand1_para gpio list failed\n");
			return;
		}
		/* 申请gpio */
		for(i = 0; i < cnt; i++)
			if(0 != gpio_request(list[i].gpio.gpio, NULL))
				printk("request nand1_para gpio list failed\n");
		/* 配置gpio list */
		if(0 != sw_gpio_setall_range(&list[0].gpio, cnt))
			printk("sw_gpio_setall_range failed\n");
	}
	else
	{
		printk("NAND_PIORequest, nand_index error: 0x%x\n", nand_index);
	}


}

void NAND_PIORelease(__u32 nand_index)
{

	int	cnt;
	script_item_u *list = NULL;

	if(nand_index == 0)
	{
		//printk("[NAND] nand gpio_release\n");

		/* 获取gpio list */
		cnt = script_get_pio_list("nand0_para", &list);
		if(0 == cnt) {
			printk("get nand0_para gpio list failed\n");
			return;
		}

		/* 释放gpio */
		while(cnt--)
			gpio_free(list[cnt].gpio.gpio);
	}
	else if(nand_index == 1)
	{
		cnt = script_get_pio_list("nand1_para", &list);
		if(0 == cnt) {
			printk("get nand1_para gpio list failed\n");
			return;
		}

		/* 释放gpio */
		while(cnt--)
			gpio_free(list[cnt].gpio.gpio);
	}
	else
	{
		printk("NAND_PIORelease, nand_index error: 0x%x\n", nand_index);
	}

}


void NAND_Memset(void* pAddr, unsigned char value, unsigned int len)
{
    memset(pAddr, value, len);
}

void NAND_Memcpy(void* pAddr_dst, void* pAddr_src, unsigned int len)
{
    memcpy(pAddr_dst, pAddr_src, len);
}

void* NAND_Malloc(unsigned int Size)
{
     	return kmalloc(Size, GFP_KERNEL);
}

void NAND_Free(void *pAddr, unsigned int Size)
{
    kfree(pAddr);
}

int NAND_Print(const char *fmt, ...)
{
	va_list args;
	int r;

	va_start(args, fmt);
	r = vprintk(fmt, args);
	va_end(args);

	return r;
}

void *NAND_IORemap(unsigned int base_addr, unsigned int size)
{
    return (void *)base_addr;
}

__u32 NAND_GetIOBaseAddrCH0(void)
{
	return 0xf1c03000;
}

__u32 NAND_GetIOBaseAddrCH1(void)
{
	return 0xf1c05000;
}



