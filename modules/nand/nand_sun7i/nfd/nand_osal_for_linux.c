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
#include <linux/dma-mapping.h>
#include <mach/dma.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <linux/gpio.h>
#include "nand_lib.h"
#include "nand_blk.h"

static struct clk *ahb_nand_clk = NULL;
static struct clk *mod_nand_clk = NULL;

static __u32 NAND_DMASingleMap(__u32 rw, __u32 buff_addr, __u32 len);
static __u32 NAND_DMASingleUnmap(__u32 rw, __u32 buff_addr, __u32 len);

static __u32 dma_phy_address = 0;
static __u32 dma_len_temp = 0;
static __u32 rw_flag = 0x1234;
#define NAND_READ 0x5555
#define NAND_WRITE 0xAAAA

static DECLARE_WAIT_QUEUE_HEAD(DMA_wait);
dma_hdl_t dma_hdle = (dma_hdl_t)NULL;

int seq=0;
int nand_handle=0;
dma_cb_t done_cb;
dma_config_t dma_config;

static int nanddma_completed_flag = 1;

static int dma_start_flag = 0;

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
//struct sw_dma_client nand_dma_client = {
//	.name="NAND_DMA",
//};

int NAND_ClkRequest(void)
{
    printk("[NAND] nand clk request start\n");
	ahb_nand_clk = clk_get(NULL, CLK_AHB_NAND);
	if(!ahb_nand_clk||IS_ERR(ahb_nand_clk)) {
		return -1;
	}
	mod_nand_clk = clk_get(NULL, CLK_MOD_NFC);
		if(!mod_nand_clk||IS_ERR(mod_nand_clk)) {
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


#ifdef  __LINUX_SUPPORT_DMA_INT__
void nanddma_buffdone(dma_hdl_t dma_hdle, void *parg)
{
	nanddma_completed_flag = 1;
	wake_up( &DMA_wait );
	//printk("buffer done. nanddma_completed_flag: %d\n", nanddma_completed_flag);
}

__s32 NAND_WaitDmaFinish(void)
{
	__u32 rw;
	__u32 buff_addr;
	__u32 len;

	wait_event(DMA_wait, nanddma_completed_flag);

	if(rw_flag==(__u32)NAND_READ)
		rw = 0;
	else
		rw = 1;
	buff_addr = dma_phy_address;
	len = dma_len_temp;
	NAND_DMASingleUnmap(rw, buff_addr, len);

	rw_flag = 0x1234;

    return 0;
}


#else
void nanddma_buffdone(dma_hdl_t dma_hdle, void *parg)
{
	return 0;
}

__s32 NAND_WaitDmaFinish(void)
{

    return 0;
}

#endif
int  nanddma_opfn(dma_hdl_t dma_hdle, void *parg){
	//if(op_code == SW_DMAOP_START)
	//	nanddma_completed_flag = 0;

	//printk("buffer opfn: %d, nanddma_completed_flag: %d\n", (int)op_code, nanddma_completed_flag);

	return 0;
}

dma_hdl_t NAND_RequestDMA(void)
{
	dma_hdle = sw_dma_request("NAND_DMA",CHAN_DEDICATE);
	if(dma_hdle == NULL)
	{
		printk("[NAND DMA] request dma fail\n");
		return dma_hdle;
	}
	printk("[NAND DMA] request dma success\n");

//	sw_dma_set_opfn(dma_hdle, nanddma_opfn);
//	sw_dma_set_buffdone_fn(dma_hdle, nanddma_buffdone);

	/* set full done callback */
	done_cb.func = nanddma_buffdone;
	done_cb.parg = NULL;
	if(0 != sw_dma_ctl(dma_hdle, DMA_OP_SET_FD_CB, (void *)&done_cb)) {
		printk("[NAND DMA] set fulldone_cb fail\n");
	}
	printk("[NAND DMA] set fulldone_cb success\n");


	return dma_hdle;

}


__s32  NAND_ReleaseDMA(void)
{
    if(dma_start_flag==1)
    {
        dma_start_flag=0;
        if(0 != sw_dma_ctl(dma_hdle, DMA_OP_STOP, NULL))
    	{
    		printk("[NAND DMA] stop dma fail\n");
    	}
    }

    if(0!= sw_dma_release(dma_hdle))
    {
        printk("[NAND DMA] release dma fail\n");
    }
	return 0;
}



void eLIBs_CleanFlushDCacheRegion_nand(void *adr, size_t bytes)
{
    /* Removes cache line align operation, which have been done
     * in __cpuc_flush_dcache_area function.
     */
	__cpuc_flush_dcache_area(adr, bytes/*  + (1 << 5) * 2 - 2 */);
}


int NAND_QueryDmaStat(void)
{
	return 0;
}

__u32 NAND_DMASingleMap(__u32 rw, __u32 buff_addr, __u32 len)
{
    __u32 mem_addr;

    if (rw == 1)
    {
	    mem_addr = (__u32)dma_map_single(NULL, (void *)buff_addr, len, DMA_TO_DEVICE);
	}
	else
    {
	    mem_addr = (__u32)dma_map_single(NULL, (void *)buff_addr, len, DMA_FROM_DEVICE);
	}

	return mem_addr;
}


__u32 NAND_DMASingleUnmap(__u32 rw, __u32 buff_addr, __u32 len)
{
	__u32 mem_addr = buff_addr;

	if (rw == 1)
	{
	    dma_unmap_single(NULL, (dma_addr_t)mem_addr, len, DMA_TO_DEVICE);
	}
	else
	{
	    dma_unmap_single(NULL, (dma_addr_t)mem_addr, len, DMA_FROM_DEVICE);
	}

}



void NAND_DMAConfigStart(int rw, unsigned int buff_addr, int len)
{
	__u32 buff_phy_addr = 0;


//config dma
	if(rw == 0)//read from nand
	{
		/* config para */
		memset(&dma_config, 0, sizeof(dma_config));
		dma_config.xfer_type.src_data_width 	= DATA_WIDTH_32BIT;
		dma_config.xfer_type.src_bst_len 	= DATA_BRST_4;
		dma_config.xfer_type.dst_data_width 	= DATA_WIDTH_32BIT;
		dma_config.xfer_type.dst_bst_len 	= DATA_BRST_4;
		dma_config.address_type.src_addr_mode 	= DDMA_ADDR_IO;
		dma_config.address_type.dst_addr_mode 	= DDMA_ADDR_LINEAR;
		dma_config.src_drq_type 	= D_DST_NAND;
		dma_config.dst_drq_type 	= D_DST_SDRAM;
		dma_config.bconti_mode 		= false;
		dma_config.irq_spt 		= CHAN_IRQ_FD;
//        printk("nand read config done!");
	}
	else //write to nand
	{
		/* config para */
		memset(&dma_config, 0, sizeof(dma_config));
		dma_config.xfer_type.src_data_width 	= DATA_WIDTH_32BIT;
		dma_config.xfer_type.src_bst_len 	= DATA_BRST_4;
		dma_config.xfer_type.dst_data_width 	= DATA_WIDTH_32BIT;
		dma_config.xfer_type.dst_bst_len 	= DATA_BRST_4;
		dma_config.address_type.src_addr_mode 	= DDMA_ADDR_LINEAR;
		dma_config.address_type.dst_addr_mode 	= DDMA_ADDR_IO;
		dma_config.src_drq_type 	= D_DST_SDRAM;
		dma_config.dst_drq_type 	= D_DST_NAND;
		dma_config.bconti_mode 		= false;
		dma_config.irq_spt 		= CHAN_IRQ_FD;
//        printk("nand write config done!");
	}


	if(0 != sw_dma_config(dma_hdle, &dma_config)) {
		printk("[NAND DMA] config dma fail\n");
	}


	{
		dma_para_t para;
		para.src_blk_sz 	= 0x7f;
		para.src_wait_cyc 	= 0x07;
		para.dst_blk_sz 	= 0x7f;
		para.dst_wait_cyc 	= 0x07;
		if(0 != sw_dma_ctl(dma_hdle, DMA_OP_SET_PARA_REG, &para))
		{
			printk("[NAND DMA] set dma para fail\n");
		}

	}
	nanddma_completed_flag = 0;

//enqueue buf
	if(rw == 0)//read
	{
		//eLIBs_CleanFlushDCacheRegion_nand((void *)buff_addr, (size_t)len);

		//buff_phy_addr = virt_to_phys(buff_addr);
		buff_phy_addr = NAND_DMASingleMap( rw,  buff_addr, len);
		if(rw_flag != 0x1234)
			printk("[NAND DMA ERR] rw_flag != 0x1234\n");
		rw_flag =(__u32)NAND_READ;
		dma_phy_address = buff_phy_addr;
		dma_len_temp = len;
		/* enqueue  buf */
  //      printk("%s:%d: buff_addr=%x,len= %d\n",__FUNCTION__,__LINE__,buff_addr,len);
//		printk("%s:%d: buff_phy_addr=%x,len= %d\n",__FUNCTION__,__LINE__,buff_phy_addr,len);
		if(0 != sw_dma_enqueue(dma_hdle, 0x01c03030, buff_phy_addr, len))
		{
			printk("[NAND DMA] enqueue buffer fail\n");
		}

	}
	else//write
	{
		//eLIBs_CleanFlushDCacheRegion_nand((void *)buff_addr, (size_t)len);

		//buff_phy_addr = virt_to_phys(buff_addr);
		buff_phy_addr = NAND_DMASingleMap( rw,  buff_addr, len);
		if(rw_flag != 0x1234)
			printk("[NAND DMA ERR] rw_flag != 0x1234\n");
		rw_flag = (__u32)NAND_WRITE;
		dma_phy_address = buff_phy_addr;
		dma_len_temp = len;
		/* enqueue  buf */
		if(0 != sw_dma_enqueue(dma_hdle, buff_phy_addr, 0x01c03030, len))
		{
			printk("[NAND DMA] enqueue buffer fail\n");
		}

	}
//start dma
    if(dma_start_flag==0)
    {
        dma_start_flag=1;
        printk("[NAND DMA] start dma***************************************** \n");
        if(0 != sw_dma_ctl(dma_hdle, DMA_OP_START, NULL))
    	{
    		printk("[NAND DMA] start dma fail\n");
    	}
    }
}




void NAND_EnRbInt(void)
{
	//clear interrupt
	NFC_RbIntClearStatus();

	nandrb_ready_flag = 0;

	//enable interrupt
	NFC_RbIntEnable();

	dbg_rbint("rb int en\n");
}


void NAND_ClearRbInt(void)
{

	//disable interrupt
	NFC_RbIntDisable();;

	dbg_rbint("rb int clear\n");

	//clear interrupt
	NFC_RbIntClearStatus();

	//check rb int status
	if(NFC_RbIntGetStatus())
	{
		dbg_rbint_wrn("nand  clear rb int status error in int clear \n");
	}

	nandrb_ready_flag = 0;
}


void NAND_RbInterrupt(void)
{

	dbg_rbint("rb int occor! \n");
	if(!NFC_RbIntGetStatus())
	{
		dbg_rbint_wrn("nand rb int late \n");
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
	dbg_rbint("rb wait \n");

	if(nandrb_ready_flag)
	{
		dbg_rbint("fast rb int\n");
		NAND_ClearRbInt();
		return 0;
	}

	rb=  NFC_GetRbSelect();
	if(!rb)
	{
		if(NFC_GetRbStatus(rb))
		{
			dbg_rbint("rb %u fast ready \n", rb);
			NAND_ClearRbInt();
			return 0;
		}
	}
	else
	{
		if(NFC_GetRbStatus(rb))
		{
			dbg_rbint("rb %u fast ready \n", rb);
			NAND_ClearRbInt();
			return 0;
		}
	}



	if(wait_event_timeout(NAND_RB_WAIT, nandrb_ready_flag, 1*HZ)==0)
	{
		dbg_rbint_wrn("nand wait rb int time out\n");
		NAND_ClearRbInt();

	}
	else
	{
		dbg_rbint("nand wait rb ready ok\n");
	}

	return 0;

}


#if 1
void NAND_PIORequest(void)
{
	int	cnt, i;
	script_item_u *list = NULL;

	/* 获取gpio list */
	cnt = script_get_pio_list("nand_para", &list);
	if(0 == cnt) {
		printk("get nand_para gpio list failed\n");
		return;
	}
	/* 申请gpio */
	for(i = 0; i < cnt; i++)
		if(0 != gpio_request(list[i].gpio.gpio, NULL))
			goto end;
	/* 配置gpio list */
	if(0 != sw_gpio_setall_range(&list[0].gpio, cnt))
	{
		printk("sw_gpio_setall_range failed\n");
		goto end;
	}
	return;
end:
    printk("nand:gpio_request failed\n");
	/* 释放gpio */
	while(i--)
		gpio_free(list[i].gpio.gpio);

}
#else
void NAND_PIORequest(void){};
#endif

void NAND_PIORelease(void)
{

	int	cnt, i;
	script_item_u *list = NULL;

	/* 获取gpio list */
	cnt = script_get_pio_list("nand_para", &list);
	if(0 == cnt) {
		printk("get nand_para gpio list failed\n");
		return;
	}

	for(i = 0; i < cnt; i++)
		gpio_free(list[i].gpio.gpio);

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

int NAND_Print(const char * fmt, ...)
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
    return (void *)0xf1c03000;
}

__u32 NAND_GetIOBaseAddr(void)
{
	return 0xf1c03000;
}


int NAND_get_storagetype()
{
    script_item_value_type_e script_ret;
    script_item_u storage_type;

    script_ret = script_get_item("target","storage_type", &storage_type);
    if(script_ret!=SCIRPT_ITEM_VALUE_TYPE_INT)
    {
           printk("nand init fetch storage_type failed\n");
           storage_type.val=0;
           return storage_type.val;
    }

    return storage_type.val;


}



