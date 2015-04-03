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
#include <mach/sys_config.h>
#include <mach/dma.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include "../src/include/nfc.h"

static struct clk *ahb_nand_clk = NULL;
static struct clk *mod_nand_clk = NULL;

static int nanddma_completed_flag = 1;
static DECLARE_WAIT_QUEUE_HEAD(DMA_wait);
int 	dma_hdle;
int seq=0;
int nand_handle=0;

static int nandrb_ready_flag = 1;
static DECLARE_WAIT_QUEUE_HEAD(NAND_RB_WAIT);

//#define RB_INT_MSG_ON
#ifdef  RB_INT_MSG_ON
#define dbg_rbint(fmt, args...) printk(fmt, ## args)
#else
#define dbg_rbint(fmt, ...)  ({})
#endif

//#define RB_INT_WRN_ON
#ifdef  RB_INT_WRN_ON
#define dbg_rbint_wrn(fmt, args...) printk(fmt, ## args)
#else
#define dbg_rbint_wrn(fmt, ...)  ({})
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
struct sw_dma_client nand_dma_client = {
	.name="NAND_DMA",
};

int NAND_ClkRequest(void)
{
    printk("[NAND] nand clk request start\n");
	ahb_nand_clk = clk_get(NULL,"ahb_nfc");
	if(!ahb_nand_clk) {
		return -1;
	}
	mod_nand_clk = clk_get(NULL,"nfc");
		if(!mod_nand_clk) {
		return -1;
	}
	printk("[NAND] nand clk request ok!\n");
	return 0;
}

void NAND_ClkRelease(void)
{
	clk_put(ahb_nand_clk);
	clk_put(mod_nand_clk);
}


int NAND_AHBEnable(void)
{
	return clk_enable(ahb_nand_clk);
}

int NAND_ClkEnable(void)
{
	return clk_enable(mod_nand_clk);
}

void NAND_AHBDisable(void)
{
	clk_disable(ahb_nand_clk);
}

void NAND_ClkDisable(void)
{
	clk_disable(mod_nand_clk);
}

int NAND_SetClk(__u32 nand_clk)
{
	return clk_set_rate(mod_nand_clk, nand_clk*2000000);
}

int NAND_GetClk(void)
{
	return (clk_get_rate(mod_nand_clk)/2000000);
}



void nanddma_buffdone(struct sw_dma_chan * ch, void *buf, int size,enum sw_dma_buffresult result){
	nanddma_completed_flag = 1;
	wake_up( &DMA_wait );
	//printk("buffer done. nanddma_completed_flag: %d\n", nanddma_completed_flag);
}

int  nanddma_opfn(struct sw_dma_chan * ch,   enum sw_chan_op op_code){
	if(op_code == SW_DMAOP_START)
		nanddma_completed_flag = 0;

	//printk("buffer opfn: %d, nanddma_completed_flag: %d\n", (int)op_code, nanddma_completed_flag);

	return 0;
}

int NAND_RequestDMA(void)
{
	dma_hdle = sw_dma_request(DMACH_DNAND, &nand_dma_client, NULL);
	if(dma_hdle < 0)
		return dma_hdle;

	sw_dma_set_opfn(dma_hdle, nanddma_opfn);
	sw_dma_set_buffdone_fn(dma_hdle, nanddma_buffdone);

	return dma_hdle;

}


__s32  NAND_ReleaseDMA(void)
{
	return 0;
}


__s32 NAND_SettingDMA(void * pArg)
{
	sw_dma_setflags(dma_hdle, SW_DMAF_AUTOSTART);
	return sw_dma_config(dma_hdle, (struct dma_hw_conf*)pArg);
}

int NAND_StartDMA(int rw, __u32 saddr, __u32 daddr, __u32 bytes)
{
	return 0;
}

void eLIBs_CleanFlushDCacheRegion_nand(void *adr, size_t bytes)
{
	__cpuc_flush_dcache_area(adr, bytes + (1 << 5) * 2 - 2);
}


__s32 NAND_DMAEqueueBuf(__u32 buff_addr, __u32 len)
{
	eLIBs_CleanFlushDCacheRegion_nand((void *)buff_addr, (size_t)len);

	nanddma_completed_flag = 0;
	return sw_dma_enqueue((int)dma_hdle, (void*)(seq++), buff_addr, len);
}

int NAND_QueryDmaStat(void)
{
	return 0;
}


__s32 NAND_WaitDmaFinish(void)
{
	 wait_event(DMA_wait, nanddma_completed_flag);

    return 0;
}

void NAND_DMAConfigStart(int rw, unsigned int buff_addr, int len)
{
	struct dma_hw_conf nand_hwconf = {
		.xfer_type = DMAXFER_D_BWORD_S_BWORD,
		.hf_irq = SW_DMA_IRQ_FULL,
		.cmbk = 0x7f077f07,
	};

	nand_hwconf.dir = rw+1;

	if(rw == 0){
		nand_hwconf.from = 0x01C03030,
		nand_hwconf.address_type = DMAADDRT_D_LN_S_IO,
		nand_hwconf.drqsrc_type = DRQ_TYPE_NAND;
	} else {
		nand_hwconf.to = 0x01C03030,
		nand_hwconf.address_type = DMAADDRT_D_IO_S_LN,
		nand_hwconf.drqdst_type = DRQ_TYPE_NAND;
	}

	NAND_SettingDMA((void*)&nand_hwconf);
	NAND_DMAEqueueBuf(buff_addr, len);
}




void NAND_EnRbInt(void)
{
	//clear interrupt
	NFC_WRITE_REG(NFC_REG_ST,NFC_RB_B2R);
	if(NFC_READ_REG(NFC_REG_ST)&NFC_RB_B2R)
	{
		dbg_rbint_wrn("nand clear rb int status error in int enable \n");
		dbg_rbint_wrn("rb status: 0x%x\n", NFC_READ_REG(NFC_REG_ST));
	}

	nandrb_ready_flag = 0;

	//enable interrupt
	NFC_WRITE_REG(NFC_REG_INT, NFC_B2R_INT_ENABLE);

	dbg_rbint("rb int en\n");
}


void NAND_ClearRbInt(void)
{

	//disable interrupt
	NFC_WRITE_REG(NFC_REG_INT, 0);

	dbg_rbint("rb int clear\n");

	//clear interrupt
	NFC_WRITE_REG(NFC_REG_ST,NFC_READ_REG(NFC_REG_ST));
	if(NFC_READ_REG(NFC_REG_ST)&NFC_RB_B2R)
	{
		dbg_rbint_wrn("nand clear rb int status error in int clear \n");
		dbg_rbint_wrn("rb status: 0x%x\n", NFC_READ_REG(NFC_REG_ST));
	}

	nandrb_ready_flag = 0;
}


void NAND_RbInterrupt(void)
{

	dbg_rbint("rb int occor! \n");
	if(!(NFC_READ_REG(NFC_REG_ST)&NFC_RB_B2R))
	{
		dbg_rbint_wrn("nand rb int late, rb status: 0x%x, rb int en: 0x%x \n",NFC_READ_REG(NFC_REG_ST),NFC_READ_REG(NFC_REG_INT));
	}

    NAND_ClearRbInt();

    nandrb_ready_flag = 1;
	wake_up( &NAND_RB_WAIT );

}


__s32 NAND_WaitRbReady(void)
{
	__u32 rb;

	NAND_EnRbInt();

	//wait_event(NAND_RB_WAIT, nandrb_ready_flag);
	dbg_rbint("rb wait, nfc_ctl: 0x%x, rb status: 0x%x, rb int en: 0x%x\n", NFC_READ_REG(NFC_REG_CTL), NFC_READ_REG(NFC_REG_ST), NFC_READ_REG(NFC_REG_INT));

	if(nandrb_ready_flag)
	{
		dbg_rbint("fast rb int\n");
		NAND_ClearRbInt();
		return 0;
	}

	rb=  ( NFC_READ_REG(NFC_REG_CTL) & NFC_RB_SEL ) >>3;
	if(!rb)
	{
		if(NFC_READ_REG(NFC_REG_ST) & NFC_RB_STATE0)
		{
			dbg_rbint_wrn("rb0 fast ready \n");
			dbg_rbint_wrn("nfc_ctl: 0x%x, rb status: 0x%x, rb int en: 0x%x\n", NFC_READ_REG(NFC_REG_CTL), NFC_READ_REG(NFC_REG_ST), NFC_READ_REG(NFC_REG_INT));
			NAND_ClearRbInt();
			return 0;
		}

	}
	else
	{
		if(NFC_READ_REG(NFC_REG_ST) & NFC_RB_STATE1)
		{
			dbg_rbint_wrn("rb1 fast ready \n");
			dbg_rbint_wrn("nfc_ctl: 0x%x, rb status: 0x%x, rb int en: 0x%x\n", NFC_READ_REG(NFC_REG_CTL), NFC_READ_REG(NFC_REG_ST), NFC_READ_REG(NFC_REG_INT));
			NAND_ClearRbInt();
			return 0;
		}
	}

	if(wait_event_timeout(NAND_RB_WAIT, nandrb_ready_flag, 1*HZ)==0)
	{
		dbg_rbint_wrn("nand wait rb ready time out\n");
		dbg_rbint_wrn("rb wait time out, nfc_ctl: 0x%x, rb status: 0x%x, rb int en: 0x%x\n", NFC_READ_REG(NFC_REG_CTL), NFC_READ_REG(NFC_REG_ST), NFC_READ_REG(NFC_REG_INT));
		NAND_ClearRbInt();
	}
	else
	{
		dbg_rbint("nand wait rb ready ok\n");
	}

    return 0;
}


void NAND_PIORequest(void)
{
	printk("[NAND] nand gpio_request\n");
	nand_handle = gpio_request_ex("nand_para",NULL);
}

void NAND_PIORelease(void)
{

	//printk("[NAND] nand gpio_release\n");
	//gpio_release("nand_para",NULL);

}

void* NAND_Malloc(unsigned int Size)
{
    return kmalloc(Size, GFP_KERNEL);
}

void NAND_Free(void *pAddr, unsigned int Size)
{
    kfree(pAddr);
}

int NAND_Print(const char * str, ...)
{
    printk(str);

    return 0;
}

void *NAND_IORemap(unsigned int base_addr, unsigned int size)
{
    return (void *)base_addr;
}

