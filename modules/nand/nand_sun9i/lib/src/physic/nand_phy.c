/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "../include/nand_type.h"
#include "../include/nand_physic.h"
#include "../include/nand_simple.h"
#include "../include/nfc.h"
#include "../include/nfc_reg.h"
//#include "nand_physic_interface.h"

extern  __u32 RetryCount[2][MAX_CHIP_SELECT_CNT];
extern void _add_cmd_list(NFC_CMD_LIST *cmd,__u32 value,__u32 addr_cycle,__u8 *addr,__u8 data_fetch_flag,
	__u8 main_data_fetch,__u32 bytecnt,__u8 wait_rb_flag);
extern void _cal_addr_in_chip(__u32 block, __u32 page, __u32 sector,__u8 *addr, __u8 cycle);
//extern __s32 _read_status(__u32 cmd_value, __u32 nBank);
//extern __s32 _write_single_page (struct boot_physical_param *writeop,__u32 program1,__u32 program2,__u8 dma_wait_mode, __u8 rb_wait_mode);
extern __s32 _write_single_page_first(struct boot_physical_param *writeop,__u32 program1,__u32 program2,__u8 dma_wait_mode, __u8 rb_wait_mode);
extern __s32 _write_single_page_wait(struct boot_physical_param *writeop,__u32 program1,__u32 program2,__u8 dma_wait_mode, __u8 rb_wait_mode);
//extern __s32 _read_single_page(struct boot_physical_param * readop,__u8 dma_wait_mode);
extern __s32 _read_single_page_first(struct boot_physical_param * readop,__u8 dma_wait_mode);
extern __s32 _read_single_page_wait(struct boot_physical_param * readop,__u8 dma_wait_mode);
extern void _pending_dma_irq_sem(void);
extern __s32 _phy_read_status(__u32 chip);
extern __s32 _wait_rb_ready(__u32 chip);
extern __s32 _wait_rb_ready_int(__u32 chip);
extern __u8 _cal_real_chip(__u32 global_bank);
extern __u8 _cal_real_rb(__u32 chip);
extern __u32 _cal_random_seed(__u32 page);
extern __s32 NFC_NormalCMD(NFC_CMD_LIST  *cmd_list);
extern __s32 NFC_ReadSecs(NFC_CMD_LIST  *rcmd, void *mainbuf,  void *cachebuf, void *sparebuf,__u32 secbitmap );
extern __s32 NFC_ReadSecs_v2(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u32 secbitmap);

/**********************************************************************
***********************translate block in bank into baock in chip**************
***********************************************************************/
#if 0
static __u32 _cal_first_valid_bit(__u32 secbitmap)
{
	__u32 firstbit = 0;

	while(!(secbitmap & 0x1))
	{
		secbitmap >>= 1;
		firstbit++;
	}

	return firstbit;
}
#endif

static __u32 _cal_valid_bits(__u32 secbitmap)
{
	__u32 validbit = 0;

	while(secbitmap)
	{
		if(secbitmap & 0x1)
			validbit++;
		secbitmap >>= 1;
	}

	return validbit;
}

static __u32 _check_continuous_bits(__u32 secbitmap)
{
	__u32 ret = 1; //1: bitmap is continuous, 0: bitmap is not continuous
	__u32 first_valid_bit = 0;
	__u32 flag = 0;

	while (secbitmap)
	{
		if (secbitmap & 0x1)
		{
			if (first_valid_bit == 0)
				first_valid_bit = 1;

			if (first_valid_bit && flag)
			{
				ret = 0;
				break;
			}
		}
		else
		{
			if (first_valid_bit == 1)
				flag = 1;
		}
		secbitmap >>= 1;
	}

	return ret;
}


__u32 _cal_block_in_chip(__u32 global_bank, __u32 super_blk_within_bank)
{
	__u32 blk_within_chip;
	__u32 single_blk_within_bank;
	__u32 bank_base;

	/*translate block 0 within  bank into blcok number within chip*/
	bank_base = global_bank%BNK_CNT_OF_CHIP*BLOCK_CNT_OF_DIE;

	if (SUPPORT_MULTI_PROGRAM ){
		 single_blk_within_bank = super_blk_within_bank * PLANE_CNT_OF_DIE -
		 	super_blk_within_bank%MULTI_PLANE_BLOCK_OFFSET;
	}

	else{
		single_blk_within_bank = super_blk_within_bank;
	}

	blk_within_chip = bank_base + single_blk_within_bank;

    if(!SUPPORT_DIE_SKIP)
    {
        if (blk_within_chip >= DIE_CNT_OF_CHIP * BLOCK_CNT_OF_DIE)
		    blk_within_chip = 0xffffffff;
    }
    else
    {

        blk_within_chip = blk_within_chip%BLOCK_CNT_OF_DIE + 2*BLOCK_CNT_OF_DIE*(blk_within_chip/BLOCK_CNT_OF_DIE);
        if (blk_within_chip >= 2*DIE_CNT_OF_CHIP * BLOCK_CNT_OF_DIE)
		    blk_within_chip = 0xffffffff;
    }


	return blk_within_chip;
}
/*
************************************************************************************************************************
*                           PHYSICAL BLOCK ERASE
*
*Description: Erase one nand flash physical block.
*
*Arguments  : pBlkAdr   the parameter of the physical block which need be erased.
*
*Return     : the result of the block erase;
*               = 0     erase physical block successful;
*               = -1    erase physical block failed.
************************************************************************************************************************
*/
__s32 _read_single_page_seq(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 rb;
	__u32 random_seed;

	//__u8 *sparebuf;
	__u8 sparebuf[4*64];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i;

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

	if(SUPPORT_RANDOM)
	{
		//random_seed = _cal_random_seed(readop->page);
		random_seed = 0x4a80;
		NFC_SetRandomSeed(random_seed);
		NFC_RandomEnable();
		ret = NFC_Read_Seq(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		NFC_RandomDisable();
		if(ret)
			ret = NFC_Read_Seq(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}
	else
	{
		ret = NFC_Read_Seq(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}




	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 2 * 4);
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}

__s32 _read_single_page_seq_16k(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 rb;
	__u32 random_seed;

	//__u8 *sparebuf;
	__u8 sparebuf[4*64];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i;

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

	if(SUPPORT_RANDOM)
	{
		//random_seed = _cal_random_seed(readop->page);
		random_seed = 0x4a80;
		NFC_SetRandomSeed(random_seed);
		NFC_RandomEnable();
		ret = NFC_Read_Seq_16K(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		NFC_RandomDisable();
		if(ret)
			ret = NFC_Read_Seq_16K(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}
	else
	{
		ret = NFC_Read_Seq_16K(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}




	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 4 * 4);
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}


__s32 _read_single_page_cfg(struct boot_physical_param *readop,__u8 dma_wait_mode, struct boot_ndfc_cfg *ndfc_cfg)
{
	__s32 ret;
	__u32 rb;
	__u32 random_seed;

	//__u8 *sparebuf;
	__u8 sparebuf[4*64];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i;

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

	if(SUPPORT_RANDOM)
	{
		//random_seed = _cal_random_seed(readop->page);
		random_seed = 0x4a80;
		NFC_SetRandomSeed(random_seed);
		NFC_RandomEnable();
		ret = NFC_Read_CFG(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE, ndfc_cfg);
		NFC_RandomDisable();
		if(ret)
			ret = NFC_Read_CFG(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE, ndfc_cfg);
	}
	else
	{
		ret = NFC_Read_CFG(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE, ndfc_cfg);
	}

	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 4 * 4);
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}

__s32 _read_single_page_1K(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i;

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);


	if(1)
	{
		//random_seed = _cal_random_seed(readop->page);
		random_seed = 0x4a80;
		NFC_SetRandomSeed(random_seed);
		NFC_RandomEnable();
		ret = NFC_Read_1K(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		NFC_RandomDisable();
		if(ret)
			ret = NFC_Read_1K(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);

	}
	else
	{
		ret = NFC_Read_1K(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}



	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 1 * 4); //// only one ecc block: 2*4->1*4
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}

__s32 _read_single_page_1st_1K_normal_ecc(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i;

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);


	if(1)
	{
		random_seed = _cal_random_seed(readop->page);
		//random_seed = 0x4a80;
		NFC_SetRandomSeed(random_seed);
		NFC_RandomEnable();
		ret = NFC_Read_1st_1K_Normal_Ecc(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		NFC_RandomDisable();
		if(ret)
			ret = NFC_Read_1st_1K_Normal_Ecc(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);

	}
	else
	{
		ret = NFC_Read_1st_1K_Normal_Ecc(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}



	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 1 * 4); ///only one ecc block
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}

__s32 _read_single_page_spare(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 k = 0;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 default_value[16];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i,n,m;
	__u32 free_page_flag = 0;
	//__s32 err, cnt;

	for (i=0; i<4*64; i++)
		sparebuf[i] = 0x55;

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

    if(SUPPORT_READ_RETRY)
    {
        if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x60))  //toshiba & Samsung mode & Sandisk mode & micron mode & intel mode
		{
			RetryCount[NandIndex][readop->chip] = 0;
			if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //Sandisk mode
			{
				if(READ_RETRY_MODE==0x30)//for sandisk 19nm flash
				{
					if((readop->page!=255)&&((readop->page==0)||((readop->page)%2))) //page low or page high
					{
						READ_RETRY_TYPE = 0x301009;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
					else
					{
						READ_RETRY_TYPE = 0x301409;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
				}
			}
        }
        for( k = 0; k<READ_RETRY_CYCLE+1;k++)
		{

			if(RetryCount[NandIndex][readop->chip]==(READ_RETRY_CYCLE+1))
				RetryCount[NandIndex][readop->chip] = 0;

			if(k>0)
			{
				if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
				{
					PHY_ERR("[Read_single_page_spare] NFC_ReadRetry fail \n");
					return -1;
				}
				if((0x32 == READ_RETRY_MODE)||(0x33 == READ_RETRY_MODE))
				{
					NFC_ReadRetry_Enable_Sandisk_A19();
				}
			}

			if(SUPPORT_RANDOM)
			{
				random_seed = _cal_random_seed(readop->page);
				NFC_SetRandomSeed(random_seed);
				NFC_RandomEnable();
				free_page_flag = 0;
				ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				if(readop->page == 0)
				{
					if((sparebuf[0] == 0x0a)&&(sparebuf[1] == 0x53)&&(sparebuf[2] == 0xb8)&&(sparebuf[3] == 0xc2))
						free_page_flag = 1;

				}
				else if((readop->page%128) == 127)
				{
					if((sparebuf[0] == 0x32)&&(sparebuf[1] == 0x43)&&(sparebuf[2] == 0xaa)&&(sparebuf[3] == 0x4e))
						free_page_flag = 1;

				}

				if(free_page_flag)
				{
					ret = 0;
					for (i=0; i<4; i++) //valid user data: 4*4 bytes
					{
						sparebuf[4*i] = 0xff;
						sparebuf[4*i+1] = 0xff;
						sparebuf[4*i+2] = 0xff;
						sparebuf[4*i+3] = 0xff;
					}
				}
				NFC_RandomDisable();
				if(ret == -ERR_ECC)
				{
					//PHY_DBG("%s: disable randomize and read spare again(k=%d)...\n", __func__, k);
					ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				}



				/**************************************************************************************
				* 1. add by Neil, from v2.09
				* 2. if spare area is all 0xff in random disable mode, it means the page is a clear page
				* 3. because in toshiba 24nm nand, too many clear pages are not all 0xff
				***************************************************************************************/
				if((ret == -ERR_ECC)&&(sparebuf[0]==0xff)&&(sparebuf[1]==0xff)&&(sparebuf[2]==0xff)&&(sparebuf[3]==0xff)&&(sparebuf[4]==0xff)&&(sparebuf[5]==0xff)&&(sparebuf[6]==0xff)&&(sparebuf[7]==0xff))
				{
					//PHY_DBG("[Read_single_page_spare] find not all 0xff clear page!  chip = %d, block = %d, page = %d\n", readop->chip, readop->block, readop->page);
					ret = 0;
				}

			}
			else
			{
				ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			}

			if((ret != -ERR_ECC)||(k==READ_RETRY_CYCLE))
			{
				if(k==0)
				{
					break;
				}
				else
				{
					if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x20))  //toshiba mode
					{
						if(0x10 == READ_RETRY_MODE)
						{
							//exit toshiba readretry
							PHY_ResetChip_CurCH(readop->chip);
						}
						else if(0x11 == READ_RETRY_MODE)
						{
							NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
						}
					}
					else if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //sandisk
					{
						NFC_ReadRetry_off(readop->chip);

					}
					else if((READ_RETRY_MODE>=0x20)&&(READ_RETRY_MODE<0x30))   //samsung mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x40)&&(READ_RETRY_MODE<0x50))   //micron mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x50)&&(READ_RETRY_MODE<0x60))   //intel mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}

					break;
				}
			}

			RetryCount[NandIndex][readop->chip]++;

		}

    	if(k>0)
    	{

			PHY_DBG("[Read_single_page_spare] NFC_ReadRetry %d cycles, ch =%d, chip = %d \n", (__u32)k, (__u32)NandIndex, (__u32)readop->chip);
			PHY_DBG("[Read_single_page_spare]	block = %d, page = %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)RetryCount[NandIndex][readop->chip]);

			//PHY_DBG("%s: %d ", __func__, ret);
			//for (i=0; i<16; i++)
			//	PHY_DBG("%x ", sparebuf[i]);
			//PHY_DBG("\n");
    		if(ret == -ERR_ECC)
    		{
    		    //PHY_DBG("ecc error--0!\n");
				if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)||(READ_RETRY_MODE==0x4)) //hynix mode
				{
					NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					RetryCount[NandIndex][readop->chip] = 0;
					PHY_DBG("ecc error--0!\n");
				}

				else if(0x32 == READ_RETRY_MODE)
				{
					for(m=0;m<3;m++)
					{
						RetryCount[NandIndex][readop->chip] = 0;
						for( n = 0; n<(READ_RETRY_CYCLE+1);n++)
						{
							if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
							{
								PHY_ERR("[Read_single_page_spare] NFC_ReadRetry fail \n");
								return -1;
							}
							if(m==0)
							{
								NFC_DSP_ON_Sandisk_A19();
							}
							else if(m==1)
							{
								NFC_ReadRetry_Prefix_Sandisk_A19();
							}
							else
							{
								if(n==0)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_Prefix_Sandisk_A19();
								NFC_DSP_ON_Sandisk_A19();
							}

							NFC_ReadRetry_Enable_Sandisk_A19();

							if(SUPPORT_RANDOM)
							{
								random_seed = _cal_random_seed(readop->page);
								NFC_SetRandomSeed(random_seed);
								NFC_RandomEnable();
								free_page_flag = 0;
								ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								if(readop->page == 0)
								{
									if((sparebuf[0] == 0x0a)&&(sparebuf[1] == 0x53)&&(sparebuf[2] == 0xb8)&&(sparebuf[3] == 0xc2))
										free_page_flag = 1;

								}
								else if((readop->page%128) == 127)
								{
									if((sparebuf[0] == 0x32)&&(sparebuf[1] == 0x43)&&(sparebuf[2] == 0xaa)&&(sparebuf[3] == 0x4e))
										free_page_flag = 1;

								}

								if(free_page_flag)
								{
									ret = 0;
									for (i=0; i<4; i++) //valid user data: 4*4 bytes
									{
										sparebuf[4*i] = 0xff;
										sparebuf[4*i+1] = 0xff;
										sparebuf[4*i+2] = 0xff;
										sparebuf[4*i+3] = 0xff;
									}
								}
								NFC_RandomDisable();
								if(ret == -ERR_ECC)
									ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}
							else
							{
								ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}

							if(ret != -ERR_ECC)
							{
								if(m==2)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_off(readop->chip);
								PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
								break;
							}
							if((ret == -ERR_ECC)&&(n==READ_RETRY_CYCLE)&&(m==2))
							{
								NFC_Test_Mode_Entry_Sandisk();
								NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
								NFC_Test_Mode_Exit_Sandisk();
								NFC_ReadRetry_off(readop->chip);
								break;
							}

							RetryCount[NandIndex][readop->chip]++;
						}
						if(ret != -ERR_ECC)
						{
							break;
						}
					}
					if(ret == -ERR_ECC)
					{
						PHY_DBG("ecc error--1!\n");
					}
				}
				else if(0x33 == READ_RETRY_MODE)
				{

					RetryCount[NandIndex][readop->chip] = 0;

					for(n=0; n<(READ_RETRY_CYCLE+1); n++)
					{
						if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
						{
							PHY_ERR("[Read_single_page_spare] NFC_ReadRetry fail\n");
							return -1;
						}
						if(n==0)
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
							NFC_Test_Mode_Exit_Sandisk();
						}
						NFC_ReadRetry_Prefix_Sandisk_A19();

						NFC_ReadRetry_Enable_Sandisk_A19();

						if(SUPPORT_RANDOM)
						{
							random_seed = _cal_random_seed(readop->page);
							NFC_SetRandomSeed(random_seed);
							NFC_RandomEnable();
							free_page_flag = 0;
							ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							if(readop->page == 0)
							{
								if((sparebuf[0] == 0x0a)&&(sparebuf[1] == 0x53)&&(sparebuf[2] == 0xb8)&&(sparebuf[3] == 0xc2))
									free_page_flag = 1;

							}
							else if((readop->page%128) == 127)
							{
								if((sparebuf[0] == 0x32)&&(sparebuf[1] == 0x43)&&(sparebuf[2] == 0xaa)&&(sparebuf[3] == 0x4e))
									free_page_flag = 1;

							}

							if(free_page_flag)
							{
								ret = 0;
								for (i=0; i<4; i++) //valid user data: 4*4 bytes
								{
									sparebuf[4*i] = 0xff;
									sparebuf[4*i+1] = 0xff;
									sparebuf[4*i+2] = 0xff;
									sparebuf[4*i+3] = 0xff;
								}
							}
							NFC_RandomDisable();
							if(ret == -ERR_ECC)
								ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
						}
						else
						{
							ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
						}
						if((ret != -ERR_ECC)||(n==READ_RETRY_CYCLE))
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
							NFC_Test_Mode_Exit_Sandisk();
							NFC_ReadRetry_off(readop->chip);
							PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
							break;
						}

						RetryCount[NandIndex][readop->chip]++;
					}
					if(ret == -ERR_ECC)
						PHY_DBG("ecc error--2!\n");
				}
				else
					PHY_DBG("ecc error--3!\n");

			}
		}
    	if(ret == ECC_LIMIT)
    		ret = ECC_LIMIT;


    }
    else
    {
		if(SUPPORT_RANDOM)
        {
			random_seed = _cal_random_seed(readop->page);
			NFC_SetRandomSeed(random_seed);
			NFC_RandomEnable();
			free_page_flag = 0;
			ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			if(readop->page == 0)
			{
				if((sparebuf[0] == 0x0a)&&(sparebuf[1] == 0x53)&&(sparebuf[2] == 0xb8)&&(sparebuf[3] == 0xc2))
					free_page_flag = 1;
			}
			else if((readop->page%128) == 127)
			{
				if((sparebuf[0] == 0x32)&&(sparebuf[1] == 0x43)&&(sparebuf[2] == 0xaa)&&(sparebuf[3] == 0x4e))
					free_page_flag = 1;
			}

			if(free_page_flag)
			{
				ret = 0;
				for (i=0; i<4; i++) //valid user data: 4*4 bytes
				{
					sparebuf[4*i] = 0xff;
					sparebuf[4*i+1] = 0xff;
					sparebuf[4*i+2] = 0xff;
					sparebuf[4*i+3] = 0xff;
				}
			}
			NFC_RandomDisable();
			if(ret == -ERR_ECC)
				ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		}
		else
		{
			ret = NFC_Read_Spare(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		}

    }

	if (dma_wait_mode)
		_pending_dma_irq_sem();

#if 0
	//////////////////////////////////////////////////////
	if (readop->page == (NandStorageInfo.PageCntPerPhyBlk-1))
	{
	    err = 0;
    	//check default value
    	cnt = 0;
		for (i=0; i<5; i++)
		{
			if (sparebuf[i] == 0x55)
				cnt++;
			else
				break;
		}
		if (cnt == 5)
			err = 1;

		if(sparebuf[1] == 0xaa)
		{
		    if(sparebuf[2] != 0xaa)
		    {
		        err = 2;
		    }
		    if(sparebuf[3] != 0xff)
		    {
		        err = 3;
		    }

		    if(sparebuf[4] != 0xff)
		    {
		        err = 4;
		    }
		}
		else
		{
		    err = 5;
		}

		if (err)
		{
			PHY_ERR("%s: %d wrong spare data: page %d  block %d  chip %d  --ch %d\n",__func__, err, readop->page, readop->block, readop->chip, NandIndex);
			for (i=0; i<16; i++)
			{
				PHY_ERR("0x%x ", sparebuf[i]);
			}
			PHY_ERR("\n");
			//return 0;
		}
	}
	////////////////////////////////////////////////////////
#endif

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 4 * 4); //2*4 --> 4*4
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}

__s32 _read_single_page_spare_first(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i;

	for (i=0; i<4*64; i++)
		sparebuf[i] = 0x77;

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*small block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++) {
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

	if(SUPPORT_RANDOM)
    {
		random_seed = _cal_random_seed(readop->page);
		NFC_SetRandomSeed(random_seed);
		NFC_RandomEnable();

		ret = NFC_Read_Spare_First(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}
	else
	{
		ret = NFC_Read_Spare_First(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}

	return ret;
}

__s32 _read_single_page_spare_wait(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 rb;
	__u8 sparebuf[4*64];
	__u32 i = 0;
	__u32 free_page_flag = 0;
	//__s32 err, cnt;

	//for (i=0; i<4*64; i++)
	//	sparebuf[i] = 0x66;

	ret = NFC_Read_Spare_Wait(NULL, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	if(readop->page == 0)
	{
		if((sparebuf[0] == 0x0a)&&(sparebuf[1] == 0x53)&&(sparebuf[2] == 0xb8)&&(sparebuf[3] == 0xc2))
			free_page_flag = 1;

	}
	else if((readop->page%128) == 127)
	{
		if((sparebuf[0] == 0x32)&&(sparebuf[1] == 0x43)&&(sparebuf[2] == 0xaa)&&(sparebuf[3] == 0x4e))
			free_page_flag = 1;

	}

	if(free_page_flag)
	{
		ret = 0;
		for (i=0; i<4; i++) // valid user data: 4*4 bytes
		{
			sparebuf[4*i] = 0xff;
			sparebuf[4*i+1] = 0xff;
			sparebuf[4*i+2] = 0xff;
			sparebuf[4*i+3] = 0xff;
		}
	}
	NFC_RandomDisable();
	if(ret == -ERR_ECC)
	{
		//PHY_DBG("%s: 1st read page spare: ch %d,  chip %d,  block %d, page %d, bitmap: 0x%x\n", __func__,
		//	NandIndex, readop->chip, readop->block, readop->page, readop->sectorbitmap);
		return(_read_single_page_spare(readop,dma_wait_mode));
	}

	if (dma_wait_mode)
		_pending_dma_irq_sem();

#if 0
////////////////////////////////////////////////
	if (readop->page == (NandStorageInfo.PageCntPerPhyBlk-1))
	{
	    err = 0;
    	//check default value
    	cnt = 0;
		for (i=0; i<5; i++)
		{
			if (sparebuf[i] == 0x77) //set in _read_single_page_spare_first()
				cnt++;
			else
				break;
		}
		if (cnt == 5)
			err = 1;

		if(sparebuf[1] == 0xaa)
		{
		    if(sparebuf[2] != 0xaa)
		    {
		        err = 2;
		    }
		    if(sparebuf[3] != 0xff)
		    {
		        err = 3;
		    }

		    if(sparebuf[4] != 0xff)
		    {
		        err = 4;
		    }
		}
		else
		{
		    err = 5;
		}

		if (err)
		{
			PHY_ERR("%s: %d wrong spare data: page %d  block %d  chip %d  --ch %d\n",__func__, err, readop->page, readop->block, readop->chip, NandIndex);
			for (i=0; i<16; i++)
			{
				PHY_ERR("0x%x ", sparebuf[i]);
			}
			PHY_ERR("\n");
			//return 0;
		}
	}
	////////////////////////////////////////////////
#endif


	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 2 * 4); // 2*4 --> 4*4
	}

	rb = _cal_real_rb(readop->chip);
	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}


/*
************************************************************************************************************************
*                       READ NAND FLASH PHYSICAL PAGE DATA
*
*Description: Read a page from a nand flash physical page to buffer.
*
*Arguments  : pPageAdr      the po__s32er to the accessed page parameter.
*
*Return     : the result of physical page read;
*               = 0     read physical page successful;
*               > 0     read physical page successful, but need do some process;
*               < 0     read physical page failed.
************************************************************************************************************************
*/


__s32 PHY_SimpleRead_Seq (struct boot_physical_param *readop)
{
	NandIndex = 0;

	return(_read_single_page_seq(readop,0));
}

__s32 PHY_SimpleRead_Seq_16K (struct boot_physical_param *readop)
{
	NandIndex = 0;

	return(_read_single_page_seq_16k(readop,0));
}


__s32 PHY_SimpleRead_CFG(struct boot_physical_param *readop, struct boot_ndfc_cfg *ndfc_cfg)
{
	NandIndex = 0;

	return(_read_single_page_cfg(readop,0, ndfc_cfg));
}

__s32 PHY_SimpleRead_1K (struct boot_physical_param *readop)
{
	NandIndex = 0;

	return(_read_single_page_1K(readop,0));
}

__s32 PHY_SimpleRead_1KCurCH (struct boot_physical_param *readop)
{
	//NandIndex = 0;

	return(_read_single_page_1K(readop,0));
}

__s32 PHY_SimpleRead_1st1K (struct boot_physical_param *readop)
{
	NandIndex = 0;

	return(_read_single_page_1st_1K_normal_ecc(readop,0));
}

__s32 PHY_SimpleRead_1st1KCurCH (struct boot_physical_param *readop)
{
	//NandIndex = 0;

	return(_read_single_page_1st_1K_normal_ecc(readop,0));
}


__s32  PHY_BlockErase(struct __PhysicOpPara_t *pBlkAdr)
{
	__u32 chip;
	__u32 rb;
	__u8 addr[4][5];
	__s32 ret=0;
	__u32 plane_cnt,i,list_len;
	__u32 block_in_chip;
	NFC_CMD_LIST cmd_list[8];

	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
		/*get chip no*/
		chip = _cal_real_chip(pBlkAdr->BankNum);
		rb = _cal_real_rb(chip);
		if (0xff == chip){
			PHY_ERR("PHY_BlockErase : beyond chip count\n");
			return -ERR_INVALIDPHYADDR;
		}
		/*get block no within chip*/
		block_in_chip = _cal_block_in_chip(pBlkAdr->BankNum,pBlkAdr->BlkNum);
		if (0xffffffff == block_in_chip){
			PHY_ERR("PHY_BlockErase : beyond block of per chip  count\n");
			return -ERR_INVALIDPHYADDR;
		}

		/*create cmd list*/
		plane_cnt = SUPPORT_MULTI_PROGRAM ? PLANE_CNT_OF_DIE : 1;

		for (i = 0; i < plane_cnt; i++){
			_cal_addr_in_chip(block_in_chip+ i*MULTI_PLANE_BLOCK_OFFSET,0, 0,addr[i], 3);
			_add_cmd_list(cmd_list+i,0x60,3,addr[i],NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);

		}
		_add_cmd_list(cmd_list + i,0xd0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
		list_len = plane_cnt + 1;
		for(i = 0; i < list_len - 1; i++){
			cmd_list[i].next = &(cmd_list[i+1]);
		}
		//_wait_rb_ready_int(chip);
		_wait_rb_ready(chip);
		NFC_SelectChip(chip);
		NFC_SelectRb(rb);

		_phy_read_status(chip);
		ret = NFC_Erase(cmd_list, SUPPORT_RB_IRQ);
		NFC_DeSelectChip(chip);
		NFC_DeSelectRb(rb);

		if (ret == -ERR_TIMEOUT)
			PHY_ERR("PHY_BlockErase : erase timeout\n");

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;
	return ret;
}

__s32 _read_sectors(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32  k;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 default_value[16];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i,n,m;
	__u8 *data_buf;
	__u32 data_size;

	data_buf = PageCachePool.TmpPageCache;
	//data_buf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE*512);

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

    if(SUPPORT_READ_RETRY)
    {
        if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x60))  //toshiba & Samsung mode & Sandisk mode & micron mode & intel mode
        {
			RetryCount[NandIndex][readop->chip] = 0;
			if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //Sandisk mode
			{
				if(READ_RETRY_MODE==0x30)//for sandisk 19nm flash
				{
					if((readop->page!=255)&&((readop->page==0)||((readop->page)%2))) //page low or page high
					{
						READ_RETRY_TYPE = 0x301009;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
					else
					{
						READ_RETRY_TYPE = 0x301409;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
				}
			}
		}
        for( k = 0; k<(READ_RETRY_CYCLE+1);k++)
		{

			if(RetryCount[NandIndex][readop->chip]==(READ_RETRY_CYCLE+1))
				RetryCount[NandIndex][readop->chip] = 0;

			if(k>0)
			{
				if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip], READ_RETRY_TYPE))
				{
					PHY_ERR("[Read_sectors] NFC_ReadRetry fail \n");
					return -1;
				}
				if((0x32 == READ_RETRY_MODE)||(0x33 == READ_RETRY_MODE))
				{
					NFC_ReadRetry_Enable_Sandisk_A19();
				}
			}

			if(SUPPORT_RANDOM)
			{
				random_seed = _cal_random_seed(readop->page);
				NFC_SetRandomSeed(random_seed);
				NFC_RandomEnable();
				ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				NFC_RandomDisable();
				if(ret == -ERR_ECC)
					ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);

					/**************************************************************************************
					* 1. add by Neil, from v2.09
					* 2. if spare area is all 0xff in random disable mode, it means the page is a clear page
					* 3. because in toshiba 24nm nand, too many clear pages are not all 0xff
					***************************************************************************************/
					if((ret == -ERR_ECC)&&(sparebuf[0]==0xff)&&(sparebuf[1]==0xff)&&(sparebuf[2]==0xff)&&(sparebuf[3]==0xff)&&(sparebuf[4]==0xff)&&(sparebuf[5]==0xff)&&(sparebuf[6]==0xff)&&(sparebuf[7]==0xff))
					{
						//PHY_DBG("[Read_sectors] find not all 0xff clear page!  chip = %d, block = %d, page = %d\n", readop->chip, readop->block, readop->page);
						ret = 0;
					}
			}
			else
			{
				ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			}

			if((ret != -ERR_ECC)||(k==(READ_RETRY_CYCLE)))
			{
				if(k==0)
				{
					break;
				}
				else
				{
					if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x20))  //toshiba mode
					{
						if(0x10 == READ_RETRY_MODE)
						{
							//exit toshiba readretry
							PHY_ResetChip_CurCH(readop->chip);
						}
						else if(0x11 == READ_RETRY_MODE)
						{
							NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
						}
					}
					else if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //sandisk
					{
						NFC_ReadRetry_off(readop->chip);
					}
					else if((READ_RETRY_MODE>=0x20)&&(READ_RETRY_MODE<0x30))   //samsung mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x40)&&(READ_RETRY_MODE<0x50))   //micron mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x50)&&(READ_RETRY_MODE<0x60))   //intel mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}

					break;
				}
			}
			RetryCount[NandIndex][readop->chip]++;

		}

        if(k>0)
    	{

			PHY_DBG("[Read_sectors] NFC_ReadRetry %d cycles, ch =%d, chip = %d \n", (__u32)k, (__u32)NandIndex, (__u32)readop->chip);
			PHY_DBG("[Read_sectors]	block = %d, page = %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)RetryCount[NandIndex][readop->chip]);
    		if(ret == -ERR_ECC)
			{

    			//PHY_DBG("spare buf: %x, %x, %x, %x, %x, %x, %x, %x\n", sparebuf[0],sparebuf[1],sparebuf[2],sparebuf[3],sparebuf[4],sparebuf[5],sparebuf[6],sparebuf[7]);
				if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)||(READ_RETRY_MODE==0x4)) //hynix mode
				{
					NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					RetryCount[NandIndex][readop->chip] = 0;
					PHY_DBG("ecc error--0!\n");
				}

				else if(0x32 == READ_RETRY_MODE)
				{
					for(m=0;m<3;m++)
					{
						RetryCount[NandIndex][readop->chip] = 0;
						for( n = 0; n<(READ_RETRY_CYCLE+1);n++)
						{
							if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
							{
								PHY_ERR("[Read_sectors] NFC_ReadRetry fail \n");
								return -1;
							}
							if(m==0)
							{
								NFC_DSP_ON_Sandisk_A19();
							}
							else if(m==1)
							{
								NFC_ReadRetry_Prefix_Sandisk_A19();
							}
							else
							{
								if(n==0)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_Prefix_Sandisk_A19();
								NFC_DSP_ON_Sandisk_A19();
							}

							NFC_ReadRetry_Enable_Sandisk_A19();

							if(SUPPORT_RANDOM)
							{
								random_seed = _cal_random_seed(readop->page);
								NFC_SetRandomSeed(random_seed);
								NFC_RandomEnable();
								ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								NFC_RandomDisable();
								if(ret == -ERR_ECC)
								{
									//PHY_DBG("%s(): disable randomize and read again(k=%d)...\n", __func__, k);
									ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								}
							}
							else
							{
								ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}

							if(ret != -ERR_ECC)
							{
								if(m==2)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_off(readop->chip);
								PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
								break;
							}
							if((ret == -ERR_ECC)&&(n==READ_RETRY_CYCLE)&&(m==2))
							{
								NFC_Test_Mode_Entry_Sandisk();
								NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
								NFC_Test_Mode_Exit_Sandisk();
								NFC_ReadRetry_off(readop->chip);
								break;
							}

							RetryCount[NandIndex][readop->chip]++;
						}
						if(ret != -ERR_ECC)
						{
							break;
						}
					}
					if(ret == -ERR_ECC)
					{
						PHY_DBG("ecc error--1!\n");
					}
				}
				else if(0x33 == READ_RETRY_MODE)
				{

					RetryCount[NandIndex][readop->chip] = 0;

					for(n=0; n<(READ_RETRY_CYCLE+1); n++)
					{
						if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
						{
							PHY_ERR("[Read_sectors] NFC_ReadRetry fail\n");
							return -1;
						}
						if(n==0)
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
							NFC_Test_Mode_Exit_Sandisk();
						}
						NFC_ReadRetry_Prefix_Sandisk_A19();

						NFC_ReadRetry_Enable_Sandisk_A19();

						if(SUPPORT_RANDOM)
						{
							random_seed = _cal_random_seed(readop->page);
							NFC_SetRandomSeed(random_seed);
							NFC_RandomEnable();
							ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							NFC_RandomDisable();
							if(ret == -ERR_ECC)
							{
								//PHY_DBG("%s(): disable randomize and read again(k=%d)...\n", __func__, k);
								ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}
						}
						else
						{
							ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
						}
						if((ret != -ERR_ECC)||(n==READ_RETRY_CYCLE))
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
							NFC_Test_Mode_Exit_Sandisk();
							NFC_ReadRetry_off(readop->chip);
							PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
							break;
						}

						RetryCount[NandIndex][readop->chip]++;
					}

					if(ret == -ERR_ECC)
						PHY_DBG("ecc error--2!\n");
				}
				else
					PHY_DBG("ecc error--3!\n");

			}
		}

	    if(ret == ECC_LIMIT)
		    ret =ECC_LIMIT;

    }
    else
    {

		if(SUPPORT_RANDOM)
        {
			random_seed = _cal_random_seed(readop->page);
			NFC_SetRandomSeed(random_seed);
			NFC_RandomEnable();
			ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			NFC_RandomDisable();
			if(ret == -ERR_ECC)
				ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);

		}
		else
		{
			ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		}


    }

	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf) {
		MEMCPY(readop->oobbuf,sparebuf, 4 * 4);
	}

	data_size = 0;
	for (i = 0; i < SECTOR_CNT_OF_SINGLE_PAGE; i++)
	{
		if (readop->sectorbitmap & (1 << i)) {
			//MEMCPY( (__u8 *)readop->mainbuf+i*512, data_buf+i*512, 512);
			MEMCPY( (__u8 *)readop->mainbuf+data_size, data_buf+i*512, 512);
			data_size += 512;
		}
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(data_buf,SECTOR_CNT_OF_SINGLE_PAGE*512);
	return ret;
}
//use normal cmd to read 2 sectors-----> don't support toshiba && sandisk a19nm
__s32 _read_sectors_first_new(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
       __s32 ret;
	__u32  k;
	__u32 rb;
	__u32 random_seed;
	__u8 *temp_buf, *data_buf;
	__u8 sparebuf[4*64];
	__u8 default_value[16];
	__u8 addr[2], pre_addr[5];
	NFC_CMD_LIST cmd_list[2], pre_cmd_list[2];
	__u32 list_len,i;
	__u32 secbitmap; //firstbit, validbit,
	void *oobbuf;
	//struct boot_physical_param readop_temp;
	//__u8 *pdata_src, *pdata_dst;
	//__u32 m,n,t, valid_sec_index, sec_index, temp_secbitmap, dataerrflag, errflag;
	//__u32 reg_val_bak[10], reg_val[10];
	//__u8 *tempbuf;
	//__u8 tempoob[4*64];
	//__u8 *oob_src, *oob_dst;


	//PRINT("_read_sectors, %x, %x\n", readop->sectorbitmap, readop->mainbuf);

	temp_buf = PageCachePool.TmpPageCache;
	data_buf = readop->mainbuf;
	secbitmap = readop->sectorbitmap;
	if(readop->oobbuf == NULL)
		oobbuf = NULL;
	else
		oobbuf = sparebuf;


	//cmd prepair
	//0x00
	_cal_addr_in_chip(readop->block,readop->page,0,pre_addr,5);
	_add_cmd_list(&pre_cmd_list[0],0x00,5,pre_addr,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	//0x30
	_add_cmd_list(&pre_cmd_list[1],0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);

	//0x05 0xe0, addr should recalt in lower physic layer
	addr[0] = 0;
	addr[1] = 0;
	_add_cmd_list(cmd_list,0x05,2,addr,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 1,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 2;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	//send 0x00+5addr+0x30cmd
	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

    if(SUPPORT_READ_RETRY)
    {
        if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x60))  //toshiba & Samsung mode & Sandisk mode & micron mode & intel mode
        {
			RetryCount[NandIndex][readop->chip] = 0;
			if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //Sandisk mode
			{
				if((readop->page!=255)&&((readop->page==0)||((readop->page)%2))) //page low or page high
				{
					READ_RETRY_TYPE = 0x301009;
					NFC_ReadRetryInit(READ_RETRY_TYPE);
				}
				else
				{
					READ_RETRY_TYPE = 0x301409;
					NFC_ReadRetryInit(READ_RETRY_TYPE);
				}
			}
        }

        for( k = 0; k<(READ_RETRY_CYCLE+1);k++)
		{

			if(RetryCount[NandIndex][readop->chip]==(READ_RETRY_CYCLE+1))
				RetryCount[NandIndex][readop->chip] = 0;

			if(k>0)
			{
				if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip], READ_RETRY_TYPE))
				{
					PHY_ERR("[Read_sectors] NFC_ReadRetry fail \n");
					return -1;
				}
			}

			if(SUPPORT_RANDOM)
			{
				random_seed = _cal_random_seed(readop->page);
				NFC_SetRandomSeed(random_seed);
				NFC_RandomEnable();
				NFC_NormalCMD(&pre_cmd_list[0]);
				NFC_NormalCMD(&pre_cmd_list[1]);
				_wait_rb_ready(readop->chip);
				ret = NFC_ReadSecs(cmd_list, data_buf, temp_buf, oobbuf, secbitmap);
				NFC_RandomDisable();
				if(ret == -ERR_ECC)
				{
					NFC_NormalCMD(&pre_cmd_list[0]);
					NFC_NormalCMD(&pre_cmd_list[1]);
					_wait_rb_ready(readop->chip);
					ret = NFC_ReadSecs(cmd_list, data_buf, temp_buf, oobbuf, secbitmap);
				}


					/**************************************************************************************
					* 1. add by Neil, from v2.09
					* 2. if spare area is all 0xff in random disable mode, it means the page is a clear page
					* 3. because in toshiba 24nm nand, too many clear pages are not all 0xff
					***************************************************************************************/
					if((ret == -ERR_ECC)&&(sparebuf[0]==0xff)&&(sparebuf[1]==0xff)&&(sparebuf[2]==0xff)&&(sparebuf[3]==0xff)&&(sparebuf[4]==0xff)&&(sparebuf[5]==0xff)&&(sparebuf[6]==0xff)&&(sparebuf[7]==0xff))
					{
						//PHY_DBG("[Read_sectors] find not all 0xff clear page!  chip = %d, block = %d, page = %d\n", readop->chip, readop->block, readop->page);
						ret = 0;
					}
			}
			else
			{
				NFC_NormalCMD(&pre_cmd_list[0]);
				NFC_NormalCMD(&pre_cmd_list[1]);
				_wait_rb_ready(readop->chip);
				ret = NFC_ReadSecs(cmd_list, data_buf, temp_buf, oobbuf, secbitmap);
			}

			if((ret != -ERR_ECC)||(k==(READ_RETRY_CYCLE)))
			{
				if(k==0)
				{
					break;
				}
				else
				{
					if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x20))  //toshiba mode
					{
						//exit toshiba readretry
						PHY_ResetChip_CurCH(readop->chip);
					}
					else if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //sandisk
					{
						NFC_ReadRetry_off(readop->chip);

					}
					else if((READ_RETRY_MODE>=0x20)&&(READ_RETRY_MODE<0x30))   //samsung mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x40)&&(READ_RETRY_MODE<0x50))   //micron mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x50)&&(READ_RETRY_MODE<0x60))   //intel mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}

					break;
				}
			}

			 RetryCount[NandIndex][readop->chip]++;

		}

        if(k>0)
    	{
    		PHY_DBG("[Read_sectors] NFC_ReadRetry %d cycles, ch =%d, chip = %d \n", (__u32)k, (__u32)NandIndex, (__u32)readop->chip);
			PHY_DBG("[Read_sectors]	block = %d, page = %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)RetryCount[NandIndex][readop->chip]);
    		if(ret == -ERR_ECC)
    		{
				PHY_DBG("ecc error!\n");
    			//PHY_DBG("spare buf: %x, %x, %x, %x, %x, %x, %x, %x\n", sparebuf[0],sparebuf[1],sparebuf[2],sparebuf[3],sparebuf[4],sparebuf[5],sparebuf[6],sparebuf[7]);
				if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)||(READ_RETRY_MODE==0x4)) //hynix mode
				{
					NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					RetryCount[NandIndex][readop->chip] = 0;
				}
			}
		}

	    if(ret == ECC_LIMIT)
		    ret =ECC_LIMIT;

    }
    else
    {

		if(SUPPORT_RANDOM)
        {
			random_seed = _cal_random_seed(readop->page);
			NFC_SetRandomSeed(random_seed);
			NFC_RandomEnable();
			NFC_NormalCMD(&pre_cmd_list[0]);
			NFC_NormalCMD(&pre_cmd_list[1]);
			_wait_rb_ready(readop->chip);
			ret = NFC_ReadSecs(cmd_list, data_buf, temp_buf, oobbuf, secbitmap);
			NFC_RandomDisable();
			if(ret == -ERR_ECC)
			{
				NFC_NormalCMD(&pre_cmd_list[0]);
				NFC_NormalCMD(&pre_cmd_list[1]);
				_wait_rb_ready(readop->chip);
				ret = NFC_ReadSecs(cmd_list, data_buf, temp_buf, oobbuf, secbitmap);
			}


		}
		else
		{
			NFC_NormalCMD(&pre_cmd_list[0]);
			NFC_NormalCMD(&pre_cmd_list[1]);
			_wait_rb_ready(readop->chip);
			ret = NFC_ReadSecs(cmd_list, data_buf, temp_buf, oobbuf, secbitmap);
		}


    }

	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 2 * 4);
	}

	for (i = 0; i < SECTOR_CNT_OF_SINGLE_PAGE; i++){
		if (readop->sectorbitmap & ((__u64)1 << i)){
			MEMCPY( (__u8 *)readop->mainbuf+i*512,data_buf+i*512, 512);
			}
		}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(data_buf,SECTOR_CNT_OF_SINGLE_PAGE*512);
	return ret;
}

__s32 _read_sectors_first(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32  k;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 default_value[16];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i,n,m;
	__u8 *data_buf;
	__u32 data_size;

	data_buf = PageCachePool.TmpPageCache;
	//data_buf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE*512);

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

    if(SUPPORT_READ_RETRY)
    {
        if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x60))  //toshiba & Samsung mode & Sandisk mode & micron mode & intel mode
        {
			RetryCount[NandIndex][readop->chip] = 0;
			if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //Sandisk mode
			{
				if(READ_RETRY_MODE==0x30)//for sandisk 19nm flash
				{
					if((readop->page!=255)&&((readop->page==0)||((readop->page)%2))) //page low or page high
					{
						READ_RETRY_TYPE = 0x301009;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
					else
					{
						READ_RETRY_TYPE = 0x301409;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
				}
			}
        }

        for( k = 0; k<(READ_RETRY_CYCLE+1);k++)
		{

			if(RetryCount[NandIndex][readop->chip]==(READ_RETRY_CYCLE+1))
				RetryCount[NandIndex][readop->chip] = 0;

			if(k>0)
			{
				if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip], READ_RETRY_TYPE))
				{
					PHY_ERR("[Read_sectors] NFC_ReadRetry fail \n");
					return -1;
				}
				if((0x32 == READ_RETRY_MODE)||(0x33 == READ_RETRY_MODE))
				{
					NFC_ReadRetry_Enable_Sandisk_A19();
				}
			}

			if(SUPPORT_RANDOM)
			{
				random_seed = _cal_random_seed(readop->page);
				NFC_SetRandomSeed(random_seed);
				NFC_RandomEnable();
				ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				NFC_RandomDisable();
				if(ret == -ERR_ECC)
					ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);

					/**************************************************************************************
					* 1. add by Neil, from v2.09
					* 2. if spare area is all 0xff in random disable mode, it means the page is a clear page
					* 3. because in toshiba 24nm nand, too many clear pages are not all 0xff
					***************************************************************************************/
					if((ret == -ERR_ECC)&&(sparebuf[0]==0xff)&&(sparebuf[1]==0xff)&&(sparebuf[2]==0xff)&&(sparebuf[3]==0xff)&&(sparebuf[4]==0xff)&&(sparebuf[5]==0xff)&&(sparebuf[6]==0xff)&&(sparebuf[7]==0xff))
					{
						//PHY_DBG("[Read_sectors] find not all 0xff clear page!  chip = %d, block = %d, page = %d\n", readop->chip, readop->block, readop->page);
						ret = 0;
					}
			}
			else
			{
				ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			}

			if((ret != -ERR_ECC)||(k==(READ_RETRY_CYCLE)))
			{
				if(k==0)
				{
					break;
				}
				else
				{
					if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x20))  //toshiba mode
					{
						if(0x10 == READ_RETRY_MODE)
						{
							//exit toshiba readretry
							PHY_ResetChip_CurCH(readop->chip);
						}
						else if(0x11 == READ_RETRY_MODE) //toshiba A19nm
						{
							NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
						}
					}
					else if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //sandisk
					{
						NFC_ReadRetry_off(readop->chip);
					}
					else if((READ_RETRY_MODE>=0x20)&&(READ_RETRY_MODE<0x30))   //samsung mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x40)&&(READ_RETRY_MODE<0x50))   //micron mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x50)&&(READ_RETRY_MODE<0x60))   //intel mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}

					break;
				}
			}

			RetryCount[NandIndex][readop->chip]++;

		}

        if(k>0)
    	{
			PHY_DBG("[Read_sectors] NFC_ReadRetry %d cycles, ch =%d, chip = %d \n", (__u32)k, (__u32)NandIndex, (__u32)readop->chip);
			PHY_DBG("[Read_sectors]	block = %d, page = %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)RetryCount[NandIndex][readop->chip]);

    		if(ret == -ERR_ECC)
			{

				//PHY_DBG("spare buf: %x, %x, %x, %x, %x, %x, %x, %x\n", sparebuf[0],sparebuf[1],sparebuf[2],sparebuf[3],sparebuf[4],sparebuf[5],sparebuf[6],sparebuf[7]);
				if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)||(READ_RETRY_MODE==0x4)) //hynix mode
				{
					NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					RetryCount[NandIndex][readop->chip] = 0;
					PHY_DBG("ecc error--0!\n");
				}

				else if(0x32 == READ_RETRY_MODE)
				{
					for(m=0;m<3;m++)
					{
						RetryCount[NandIndex][readop->chip] = 0;
						for(n=0; n<(READ_RETRY_CYCLE+1);n++)
						{
							if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
							{
								PHY_ERR("[Read_sectors_first] NFC_ReadRetry fail \n");
								return -1;
							}
							if(m==0)
							{
								NFC_DSP_ON_Sandisk_A19();
							}
							else if(m==1)
							{
								NFC_ReadRetry_Prefix_Sandisk_A19();
							}
							else
							{
								if(n==0)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_Prefix_Sandisk_A19();
								NFC_DSP_ON_Sandisk_A19();
							}

							NFC_ReadRetry_Enable_Sandisk_A19();

							if(SUPPORT_RANDOM)
							{
								random_seed = _cal_random_seed(readop->page);
								NFC_SetRandomSeed(random_seed);
								NFC_RandomEnable();
								ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								NFC_RandomDisable();
								if(ret == -ERR_ECC)
								{
									//PHY_DBG("%s(): disable randomize and read again(k=%d)...\n", __func__, k);
									ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								}
							}
							else
							{
								ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}


							if(ret != -ERR_ECC)
							{
								if(m==2)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_off(readop->chip);
								PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
								break;
							}
							if((ret == -ERR_ECC)&&(n==READ_RETRY_CYCLE)&&(m==2))
							{
								NFC_Test_Mode_Entry_Sandisk();
								NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
								NFC_Test_Mode_Exit_Sandisk();
								NFC_ReadRetry_off(readop->chip);
								break;
							}

							RetryCount[NandIndex][readop->chip]++;
						}

						if(ret != -ERR_ECC)
						{
							break;
						}
					}
					if(ret == -ERR_ECC)
					{
						PHY_DBG("ecc error--1!\n");
					}
				}
				else if(0x33 == READ_RETRY_MODE)
				{

					RetryCount[NandIndex][readop->chip] = 0;

					for(n=0; n<(READ_RETRY_CYCLE+1); n++)
					{
						if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
						{
							PHY_ERR("[Read_sectors_first] NFC_ReadRetry fail\n");
							return -1;
						}
						if(n==0)
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
							NFC_Test_Mode_Exit_Sandisk();
						}
						NFC_ReadRetry_Prefix_Sandisk_A19();

						NFC_ReadRetry_Enable_Sandisk_A19();

						if(SUPPORT_RANDOM)
						{
							random_seed = _cal_random_seed(readop->page);
							NFC_SetRandomSeed(random_seed);
							NFC_RandomEnable();
							ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							NFC_RandomDisable();
							if(ret == -ERR_ECC)
							{
								//PHY_DBG("%s(): disable randomize and read again(k=%d)...\n", __func__, k);
								ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}
						}
						else
						{
							ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
						}
						if((ret != -ERR_ECC)||(n==READ_RETRY_CYCLE))
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
							NFC_Test_Mode_Exit_Sandisk();
							NFC_ReadRetry_off(readop->chip);
							PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
							break;
						}

						RetryCount[NandIndex][readop->chip]++;
					}

					if(ret == -ERR_ECC)
						PHY_DBG("ecc error--2!\n");
				}
				else
					PHY_DBG("ecc error--3!\n");

			}
		}

	    if(ret == ECC_LIMIT)
		    ret = ECC_LIMIT;

    }
    else
    {

		if(SUPPORT_RANDOM)
        {
			random_seed = _cal_random_seed(readop->page);
			NFC_SetRandomSeed(random_seed);
			NFC_RandomEnable();
			ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			NFC_RandomDisable();
			if(ret == -ERR_ECC)
				ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);

		}
		else
		{
			ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		}


    }

	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf) {
		MEMCPY(readop->oobbuf,sparebuf, 2 * 4); //2*4 --> 4*4
	}

	data_size = 0;
	for (i = 0; i < SECTOR_CNT_OF_SINGLE_PAGE; i++)
	{
		if (readop->sectorbitmap & ((__u32)1 << i)) {
			MEMCPY( (__u8 *)readop->mainbuf+i*512, data_buf+i*512, 512);
			//MEMCPY( (__u8 *)readop->mainbuf+data_size, data_buf+i*512, 512);
			//data_size += 512;
		}
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(data_buf,SECTOR_CNT_OF_SINGLE_PAGE*512);
	return ret;
}

__s32 _read_sectors_wait(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	return 0;
}

__s32 _read_sectors_v2_first(struct boot_physical_param *readop, __u8 dma_wait_mode)
{
	__s32 ret;
	__u32 k;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 default_value[16];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len,i,n,m;
	//__u8 *data_buf;

	//data_buf = PageCachePool.TmpPageCache;
	//data_buf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE*512);

	//sparebuf = (__u8 *)MALLOC(SECTOR_CNT_OF_SINGLE_PAGE * 4);
	/*create cmd list*/
	/*samll block*/
	if (SECTOR_CNT_OF_SINGLE_PAGE == 1){
		_cal_addr_in_chip(readop->block,readop->page,0,addr,4);
		_add_cmd_list(cmd_list,0x00,4,addr,NDFC_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}
	/*large block*/
	else{
		/*the cammand have no corresponding feature if IGNORE was set, */
		_cal_addr_in_chip(readop->block,readop->page,0,addr,5);
		_add_cmd_list(cmd_list,0x00,5,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_NO_WAIT_RB);

	}
	_add_cmd_list(cmd_list + 1,0x05,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 2,0xe0,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list + 3,0x30,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
	list_len = 4;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

    if(SUPPORT_READ_RETRY)
    {
        if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x60))  //toshiba & Samsung mode & Sandisk mode & micron mode & intel mode
        {
			RetryCount[NandIndex][readop->chip] = 0;
			if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //Sandisk mode
			{
				if(READ_RETRY_MODE==0x30)//for sandisk 19nm flash
				{
					if((readop->page!=255)&&((readop->page==0)||((readop->page)%2))) //page low or page high
					{
						READ_RETRY_TYPE = 0x301009;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
					else
					{
						READ_RETRY_TYPE = 0x301409;
						NFC_ReadRetryInit(READ_RETRY_TYPE);
					}
				}
			}
        }

        for( k = 0; k<(READ_RETRY_CYCLE+1);k++)
		{
			if(RetryCount[NandIndex][readop->chip]==(READ_RETRY_CYCLE+1))
				RetryCount[NandIndex][readop->chip] = 0;

			if(k>0)
			{
				if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip], READ_RETRY_TYPE))
				{
					PHY_ERR("[Read_sectors] NFC_ReadRetry fail \n");
					return -1;
				}
				if((0x32 == READ_RETRY_MODE)||(0x33 == READ_RETRY_MODE))
				{
					NFC_ReadRetry_Enable_Sandisk_A19();
				}
			}

			if(SUPPORT_RANDOM)
			{
				random_seed = _cal_random_seed(readop->page);
				NFC_SetRandomSeed(random_seed);
				NFC_RandomEnable();
				//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
				NFC_RandomDisable();
				if(ret == -ERR_ECC)
					//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
					ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);

					/**************************************************************************************
					* 1. add by Neil, from v2.09
					* 2. if spare area is all 0xff in random disable mode, it means the page is a clear page
					* 3. because in toshiba 24nm nand, too many clear pages are not all 0xff
					***************************************************************************************/
					if((ret == -ERR_ECC)&&(sparebuf[0]==0xff)&&(sparebuf[1]==0xff)&&(sparebuf[2]==0xff)&&(sparebuf[3]==0xff)&&(sparebuf[4]==0xff)&&(sparebuf[5]==0xff)&&(sparebuf[6]==0xff)&&(sparebuf[7]==0xff))
					{
						//PHY_DBG("[Read_sectors] find not all 0xff clear page!  chip = %d, block = %d, page = %d\n", readop->chip, readop->block, readop->page);
						ret = 0;
					}
			}
			else
			{
				//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
			}

			if((ret != -ERR_ECC)||(k==(READ_RETRY_CYCLE)))
			{
				if(k==0)
				{
					break;
				}
				else
				{
					if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x20))  //toshiba mode
					{
						if(0x10 == READ_RETRY_MODE)
						{
							//exit toshiba readretry
							PHY_ResetChip_CurCH(readop->chip);
						}
						else if(0x11 == READ_RETRY_MODE) //toshiba A19nm
						{
							NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
						}
					}
					else if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //sandisk
					{
						NFC_ReadRetry_off(readop->chip);

					}
					else if((READ_RETRY_MODE>=0x20)&&(READ_RETRY_MODE<0x30))   //samsung mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x40)&&(READ_RETRY_MODE<0x50))   //micron mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}
					else if((READ_RETRY_MODE>=0x50)&&(READ_RETRY_MODE<0x60))   //intel mode
					{
						NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					}

					break;
				}
			}

			RetryCount[NandIndex][readop->chip]++;

		}

        if(k>0)
    	{
			PHY_DBG("[Read_sectors] NFC_ReadRetry %d cycles, ch =%d, chip = %d \n", (__u32)k, (__u32)NandIndex, (__u32)readop->chip);
			PHY_DBG("[Read_sectors]	block = %d, page = %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)RetryCount[NandIndex][readop->chip]);

    		if(ret == -ERR_ECC)
			{

    			//PHY_DBG("spare buf: %x, %x, %x, %x, %x, %x, %x, %x\n", sparebuf[0],sparebuf[1],sparebuf[2],sparebuf[3],sparebuf[4],sparebuf[5],sparebuf[6],sparebuf[7]);
				if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)||(READ_RETRY_MODE==0x4)) //hynix mode
				{
					NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					RetryCount[NandIndex][readop->chip] = 0;
					PHY_DBG("ecc error--0!\n");
				}
				else if(0x32 == READ_RETRY_MODE)
				{
					for(m=0;m<3;m++)
					{
						RetryCount[NandIndex][readop->chip] = 0;
						for(n=0; n<(READ_RETRY_CYCLE+1); n++)
						{
							if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
							{
								PHY_ERR("[Read_sectors_v2_first] NFC_ReadRetry fail \n");
								return -1;
							}
							if(m==0)
							{
								NFC_DSP_ON_Sandisk_A19();
							}
							else if(m==1)
							{
								NFC_ReadRetry_Prefix_Sandisk_A19();
							}
							else
							{
								if(n==0)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_Prefix_Sandisk_A19();
								NFC_DSP_ON_Sandisk_A19();
							}

							NFC_ReadRetry_Enable_Sandisk_A19();

							if(SUPPORT_RANDOM)
							{
								random_seed = _cal_random_seed(readop->page);
								NFC_SetRandomSeed(random_seed);
								NFC_RandomEnable();
								ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
								NFC_RandomDisable();
								if(ret == -ERR_ECC)
									ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
							}
							else
							{
								ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
							}

							if(ret != -ERR_ECC)
							{
								if(m==2)
								{
									NFC_Test_Mode_Entry_Sandisk();
									NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
									NFC_Test_Mode_Exit_Sandisk();
								}
								NFC_ReadRetry_off(readop->chip);
								PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
								break;
							}
							if((ret == -ERR_ECC)&&(n==READ_RETRY_CYCLE)&&(m==2))
							{
								NFC_Test_Mode_Entry_Sandisk();
								NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
								NFC_Test_Mode_Exit_Sandisk();
								NFC_ReadRetry_off(readop->chip);
								break;
							}

							RetryCount[NandIndex][readop->chip]++;
						}

						if(ret != -ERR_ECC)
						{
							break;
						}
					}
					if(ret == -ERR_ECC)
					{
						PHY_DBG("ecc error--1!\n");
					}
				}
				else if(0x33 == READ_RETRY_MODE)
				{

					RetryCount[NandIndex][readop->chip] = 0;

					for(n=0; n<(READ_RETRY_CYCLE+1); n++)
					{
						if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
						{
							PHY_ERR("[Read_sectors_v2_first] NFC_ReadRetry fail\n");
							return -1;
						}
						if(n==0)
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0xc0);
							NFC_Test_Mode_Exit_Sandisk();
						}
						NFC_ReadRetry_Prefix_Sandisk_A19();

						NFC_ReadRetry_Enable_Sandisk_A19();

						if(SUPPORT_RANDOM)
						{
							random_seed = _cal_random_seed(readop->page);
							NFC_SetRandomSeed(random_seed);
							NFC_RandomEnable();
							//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
							NFC_RandomDisable();
							if(ret == -ERR_ECC)
								//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
						}
						else
						{
							//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
						}
						if((ret != -ERR_ECC)||(n==READ_RETRY_CYCLE))
						{
							NFC_Test_Mode_Entry_Sandisk();
							NFC_Change_LMFLGFIX_NEXT_Sandisk(0x40);
							NFC_Test_Mode_Exit_Sandisk();
							NFC_ReadRetry_off(readop->chip);
							PHY_DBG("block = %d, page = %d, step %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)m, (__u32)RetryCount[NandIndex][readop->chip]);
							break;
						}

						RetryCount[NandIndex][readop->chip]++;
					}

					if(ret == -ERR_ECC)
						PHY_DBG("ecc error--2!\n");
				}
				else
					PHY_DBG("ecc error--3!\n");

			}
		}

	    if(ret == ECC_LIMIT)
		    ret =ECC_LIMIT;
    }
    else
    {
		if(SUPPORT_RANDOM)
        {
			random_seed = _cal_random_seed(readop->page);
			NFC_SetRandomSeed(random_seed);
			NFC_RandomEnable();
			//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
			NFC_RandomDisable();
			if(ret == -ERR_ECC)
				//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
		}
		else
		{
			//ret = NFC_Read(cmd_list, data_buf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			ret = NFC_ReadSecs_v2(cmd_list, readop->mainbuf, sparebuf, readop->sectorbitmap);
		}


    }

	if (dma_wait_mode)
		_pending_dma_irq_sem();

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 2 * 4); //2*4 --> 4*4
	}

#if 0
	for (i = 0; i < SECTOR_CNT_OF_SINGLE_PAGE; i++){
		if (readop->sectorbitmap & (1 << i)){
			MEMCPY( (__u8 *)readop->mainbuf+i*512,data_buf+i*512, 512);
			}
		}
#endif

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(data_buf,SECTOR_CNT_OF_SINGLE_PAGE*512);
	return ret;
}

__s32 _read_sectors_v2_wait(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	return 0;
}

/*********************************************************************
**************************two plane read operation***********************
***********************************************************************/
__s32 _two_plane_read(struct __PhysicOpPara_t *pPageAdr,__u8 dma_wait_mode)
{
	__u8 addr[2][5];
	__s32 ret;
	__u32 chip;
	__u32 rb;
	__u32 list_len,i,plane_cnt;
	__u64 bitmap_in_single_page;
	__u32 block_in_chip;

	NFC_CMD_LIST cmd_list[8];
	struct boot_physical_param readop;

	/*get chip no*/
	chip = _cal_real_chip(pPageAdr->BankNum);
	if (0xff == chip){
		PHY_ERR("PHY_PageRead : beyond chip count\n");
		return -ERR_INVALIDPHYADDR;
	}
	/*get block no within chip*/
	block_in_chip = _cal_block_in_chip(pPageAdr->BankNum,pPageAdr->BlkNum);
	if (0xffffffff == block_in_chip){
		PHY_ERR("PHY_PageRead : beyond block of per chip  count\n");
		return -ERR_INVALIDPHYADDR;
	}
	plane_cnt = SUPPORT_MULTI_PROGRAM ? PLANE_CNT_OF_DIE : 1;

	/*create cmd list*/
	/*send 0x60+3addr - 0x60 + 3addr - 0x30 for samsung 4k page*/
	for(i = 0; i< plane_cnt; i++){
		_cal_addr_in_chip(block_in_chip+i*MULTI_PLANE_BLOCK_OFFSET, pPageAdr->PageNum,0,addr[i],3);
		_add_cmd_list(cmd_list+i, 0x60,3,addr[i],NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
	}
	_add_cmd_list(cmd_list+i,0x30, 0,NDFC_IGNORE,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_WAIT_RB);
	list_len = plane_cnt + 1;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}
	rb = _cal_real_rb(chip);
	NFC_SelectChip(chip);
	NFC_SelectRb(rb);
	ret = _wait_rb_ready(chip);
	if (ret)
		return ret;

	ret = NFC_Read(cmd_list, NULL, NULL, 0,NDFC_NORMAL_MODE);

	/*send 0x00 +5addr --(05+2addr-e0...)*/
	for (i = 0; i < plane_cnt; i++){
		/*init single page operation param*/
		readop.chip = chip;
		readop.block = block_in_chip+ i*MULTI_PLANE_BLOCK_OFFSET;
		readop.page = pPageAdr->PageNum;
		readop.mainbuf = (__u8 *)(pPageAdr->MDataPtr) + 512*SECTOR_CNT_OF_SINGLE_PAGE*i;
		if (pPageAdr->SDataPtr)
			readop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 4*SECTOR_CNT_OF_SINGLE_PAGE*i;


		bitmap_in_single_page = FULL_BITMAP_OF_SINGLE_PAGE &(pPageAdr->SectBitmap >> (i*CHANNEL_CNT+ NandIndex));
		readop.sectorbitmap = bitmap_in_single_page;
		ret |= _read_sectors(&readop,dma_wait_mode);
	}
	NFC_DeSelectChip(chip);
	NFC_DeSelectRb(rb);
	return ret;
}
/*
************************************************************************************************************************
*                       READ NAND FLASH PHYSICAL PAGE DATA
*
*Description: Read a page from a nand flash physical page to buffer.
*
*Arguments  : pPageAdr      the po__s32er to the accessed page parameter.
*
*Return     : the result of physical page read;
*               = 0     read physical page successful;
*               > 0     read physical page successful, but need do some process;
*               < 0     read physical page failed.
************************************************************************************************************************
*/
__u32 send_flag(void)
{
	__u32 cfg=0x33;

	NandIndex =1;
	cfg |= ( NDFC_SEND_CMD1 );
	NDFC_WRITE_REG_CMD(cfg);

	return 0;
}

__s32  PHY_PageRead(struct __PhysicOpPara_t *pPageAdr)
{
	__s32 ret[2][2] = { {0, 0}, {0, 0}};
	__s32 result = 0;
	__u32 chip;
	__u32 block_in_chip;
	__u32 plane_cnt, i, j, k;
	__u32 bitmap_in_single_page;
	struct boot_physical_param readop;
	__u8 tmp_oob[2][2][16]; //__u8 tmp_oob[2][2][8];
	__u32 bad_flag =0;

	//unsigned long ts0,te0, ts1,te1;
	//unsigned long ts2[2],te2[2];
	//unsigned long ts3[2],te3[2];
	//unsigned long ts4[2],te4[2];
	//unsigned long ts5[2],te5[2];


	for (i=0; i<2; i++) /*plane*/
		for (j=0; j<2; j++) /*channel*/
			for (k=0; k<16; k++) /*user data*/
				tmp_oob[i][j][k] = 0x3c;

	plane_cnt = SUPPORT_MULTI_PROGRAM ? PLANE_CNT_OF_DIE : 1;

	/*get chip no*/
	chip = _cal_real_chip(pPageAdr->BankNum);
	if (0xff == chip) {
		PHY_ERR("PHY_PageRead : beyond chip count, %d.\n", chip);
		return -ERR_INVALIDPHYADDR;
	}
	/*get block no within chip*/
	block_in_chip = _cal_block_in_chip(pPageAdr->BankNum,pPageAdr->BlkNum);
	if (0xffffffff == block_in_chip) {
		PHY_ERR("PHY_PageRead : beyond block of per chip count, %d.\n", block_in_chip);
		return -ERR_INVALIDPHYADDR;
	}

	for (i=0; i<plane_cnt; i++)
	{
		//ts0 = jiffies();

		/*init single page operation param*/
		readop.chip = chip;
		readop.block = block_in_chip  + i*MULTI_PLANE_BLOCK_OFFSET;
		readop.page = pPageAdr->PageNum;

		for (NandIndex=0; NandIndex<CHANNEL_CNT; NandIndex++)
		{
			//ts2[NandIndex] = jiffies();
			if(i==0)
				_phy_read_status(chip);
			readop.mainbuf = (__u8 *)(pPageAdr->MDataPtr) + 512*SECTOR_CNT_OF_SINGLE_PAGE*(i*CHANNEL_CNT+ NandIndex);
			if (pPageAdr->SDataPtr) {
				readop.oobbuf = tmp_oob[i][NandIndex];
			} else {
				readop.oobbuf = NULL;
			}

			bitmap_in_single_page = FULL_BITMAP_OF_SINGLE_PAGE &(pPageAdr->SectBitmap >> ((i*CHANNEL_CNT+ NandIndex)*SECTOR_CNT_OF_SINGLE_PAGE));
			readop.sectorbitmap = bitmap_in_single_page;

			//ts3[NandIndex] = jiffies();
			if (bitmap_in_single_page){
				/*bitmap of this plane is valid */
				if (bitmap_in_single_page == FULL_BITMAP_OF_SINGLE_PAGE) {
					/*align page, use page mode */
					ret[NandIndex][i] |= _read_single_page_first(&readop, SUPPORT_DMA_IRQ);
				} else {
					if (NdfcVersion == NDFC_VERSION_V1)
						ret[NandIndex][i] |= _read_sectors_first(&readop, SUPPORT_DMA_IRQ);
					else if (NdfcVersion == NDFC_VERSION_V2)
						ret[NandIndex][i] |= _read_sectors_v2_first(&readop, SUPPORT_DMA_IRQ);
					else
						PHY_ERR("PHY_PageRead: wrong ndfc version, %dd\n", NdfcVersion);
				}

				//data_size_first += 512*validbit;
			}
			//te3[NandIndex] = jiffies();

			if(NandIndex == (CHANNEL_CNT-1))
				break;

			//te2[NandIndex] = jiffies();
		}
		//te0 = jiffies();

		//ts1 = jiffies();
		for(NandIndex=0; NandIndex<CHANNEL_CNT; NandIndex++)
		{
			//ts4[NandIndex] = jiffies();

			readop.mainbuf = (__u8 *)(pPageAdr->MDataPtr) + 512*SECTOR_CNT_OF_SINGLE_PAGE*(i*CHANNEL_CNT+ NandIndex);
			if (pPageAdr->SDataPtr) {
				//readop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 4*SECTOR_CNT_OF_SINGLE_PAGE*(plane_cnt*NandIndex + i);
				readop.oobbuf = tmp_oob[i][NandIndex];
			} else {
				readop.oobbuf = NULL;
			}

			bitmap_in_single_page = FULL_BITMAP_OF_SINGLE_PAGE &(pPageAdr->SectBitmap >> ((i*CHANNEL_CNT+ NandIndex)*SECTOR_CNT_OF_SINGLE_PAGE));
			readop.sectorbitmap = bitmap_in_single_page;

			//ts5[NandIndex] = jiffies();
			if (bitmap_in_single_page)
			{

				/*bitmap of this plane is valid */
				if (bitmap_in_single_page == FULL_BITMAP_OF_SINGLE_PAGE) {
					/*align page, use page mode */
					ret[NandIndex][i] |= _read_single_page_wait(&readop,SUPPORT_DMA_IRQ);
				} else {
					if (NdfcVersion == NDFC_VERSION_V1)
						ret[NandIndex][i] |= _read_sectors_wait(&readop, SUPPORT_DMA_IRQ);
					else if (NdfcVersion == NDFC_VERSION_V2)
						ret[NandIndex][i] |= _read_sectors_v2_wait(&readop,SUPPORT_DMA_IRQ);
					else
						PHY_ERR("PHY_PageRead: wrong ndfc version, %dd\n", NdfcVersion);
				}

				//data_size_wait += 512*validbit;
			}
			//te5[NandIndex] = jiffies();

			if(NandIndex == (CHANNEL_CNT-1))
				break;

			//te4[NandIndex] = jiffies();
		}

		//te1 = jiffies();
	}

	NandIndex = 0;

	//PHY_DBG("read: %d us wait: %d us  3-0: %d 3-1: %d  5-0: %d 5-1: %d\n", te0-ts0, te1-ts1, te3[0]-ts3[0], te3[1]-ts3[1], te5[0]-ts5[0], te5[1]-ts5[1]);

	if (pPageAdr->SDataPtr)
	{
		bad_flag = 0;
		for (i=0; i<plane_cnt; i++)
		{
			for (j=0; j<CHANNEL_CNT;j++)
			{
				if ((tmp_oob[i][j][0] != 0xff)&&(tmp_oob[i][j][0] != 0x3c))
				{
					bad_flag = 1;
					PHY_DBG("PHY_PageRead bad flag: bank 0x%x  block 0x%x  page 0x%x \n", (__u32)pPageAdr->BankNum,(__u32)pPageAdr->BlkNum,
	    					(__u32)pPageAdr->PageNum);
					PHY_DBG("plane:%d, ch:%d, chip:%x, blk:0x%x, pg:0x%x, 0x%08x 0x%08x 0x%08x 0x%08x\n",
							i, j, readop.chip, readop.block, readop.page,
							*((__u32 *)(&tmp_oob[i][j][0])), *((__u32 *)(&tmp_oob[i][j][4])),
							*((__u32 *)(&tmp_oob[i][j][8])), *((__u32 *)(&tmp_oob[i][j][12])));
				}
			}
		}

		if (bad_flag)
			tmp_oob[0][0][0] = 0;

#if 0
		if (SECTOR_CNT_OF_SINGLE_PAGE == 2) {
			/*physical page size is 1KB
			* if CHANNEL_CNT is 1, tmp_oob[0][1][0~3], tmp_oob[1][1][0~3] are invalid
			*/
			MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 4);
			MEMCPY((__u8 *)pPageAdr->SDataPtr +4, &tmp_oob[0][1][0], 4);
			MEMCPY((__u8 *)pPageAdr->SDataPtr +8, &tmp_oob[1][0][0], 4);
			MEMCPY((__u8 *)pPageAdr->SDataPtr +12, &tmp_oob[1][1][0], 4);
		} else if (SECTOR_CNT_OF_SINGLE_PAGE == 4) {
			/*physical page size is 2KB*/
			MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 8);
			if (CHANNEL_CNT == 1)
				MEMCPY((__u8 *)pPageAdr->SDataPtr +8, &tmp_oob[1][0][0], 8);
			else
				MEMCPY((__u8 *)pPageAdr->SDataPtr +8, &tmp_oob[0][1][0], 8);
		} else {
			/*physical page size is equal to or greater than 4KB*/
			MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 16);
		}
#else
		/*only support physical page size is equal to or greater than 4KB*/

//		if (readop.page == (NandStorageInfo.PageCntPerPhyBlk-1))
//		{
//		    err = 0;
//	    	//check default value
//	    	cnt = 0;
//			for (i=0; i<5; i++)
//			{
//				if (tmp_oob[0][0][i] == 0x3c)
//					cnt++;
//				else
//					break;
//			}
//			if (cnt == 5)
//				err = 1;
//
//			if(tmp_oob[0][0][1] == 0xaa)
//			{
//			    if(tmp_oob[0][0][2] != 0xaa)
//			    {
//			        err = 2;
//			    }
//			    if(tmp_oob[0][0][3] != 0xff)
//			    {
//			        err = 3;
//			    }
//
//			    if(tmp_oob[0][0][4] != 0xff)
//			    {
//			        err = 4;
//			    }
//			}
//			else
//			{
//			    err = 5;
//			}
//
//			if (err)
//			{
//				PHY_ERR("%s: %d wrong spare data: page %d  block %d  chip %d\n",__func__, err, readop.page, readop.block, readop.chip);
//				PHY_DBG("%s: plane0 -- ch0: \n", __func__);
//				for (i=0; i<16; i++)
//				{
//					PHY_ERR("0x%x ", tmp_oob[0][0][i]);
//				}
//				PHY_ERR("\n");
//
//				PHY_DBG("%s: plane0 -- ch1: \n", __func__);
//				for (i=0; i<16; i++)
//				{
//					PHY_ERR("0x%x ", tmp_oob[0][1][i]);
//				}
//				PHY_ERR("\n");
//
//				PHY_DBG("%s: plane1 -- ch0: \n", __func__);
//				for (i=0; i<16; i++)
//				{
//					PHY_ERR("0x%x ", tmp_oob[1][0][i]);
//				}
//				PHY_ERR("\n");
//
//				PHY_DBG("%s: plane1 -- ch1: \n", __func__);
//				for (i=0; i<16; i++)
//				{
//					PHY_ERR("0x%x ", tmp_oob[1][1][i]);
//				}
//				PHY_ERR("\n");
//
//				//return 0;
//			}
//		}
		MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 8);
#endif

		//only for test
#if 0
		i = 1;
		if (SECTOR_CNT_OF_SINGLE_PAGE >= 8) { //if (bad_flag) { //if (SECTOR_CNT_OF_SINGLE_PAGE >= 8) {
#if 0
			i = 0;
			for (j=0; j<16; j++)
				if (tmp_oob[0][0][j] != tmp_oob[0][1][j]) {
					PHY_ERR("\n\n[PHY_ERR]user data in plane0-ch0 and plane0-ch1 are different!");
					i = 1;
					break;
				}
#endif
			if (i == 1) {
				PHY_DBG("\nplane0-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[0][0][j]);
				PHY_DBG("\nplane0-ch1: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[0][1][j]);
				PHY_DBG("\n");
			}
#if 0
			i = 0;
			for (j=0; j<16; j++)
				if (tmp_oob[1][0][j] != tmp_oob[1][1][j]) {
					PHY_ERR("\n\n[PHY_ERR]user data in plane1-ch0 and plane1-ch1 are different!");
					i = 1;
					break;
				}
#endif
			if (i == 1) {
				PHY_DBG("\nplane1-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[1][0][j]);
				PHY_DBG("\nplane1-ch1: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[1][1][j]);
				PHY_DBG("\n");
			}
#if 0
			i = 0;
			for (j=0; j<16; j++)
				if (tmp_oob[0][0][j] != tmp_oob[1][0][j]) {
					PHY_ERR("\n\n[PHY_ERR]user data in plane0-ch0 and plane1-ch0 are different!");
					i = 1;
					break;
				}
			if (i == 1) {
				PHY_DBG("\nplane0-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[0][0][j]);
				PHY_DBG("\nplane1-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[1][0][j]);
				PHY_DBG("\n");
			}
#endif
		}

#endif
	}

	if((ret[0][0] == -ERR_TIMEOUT)||(ret[0][1] == -ERR_TIMEOUT)||(ret[1][0] == -ERR_TIMEOUT)||(ret[1][1] == -ERR_TIMEOUT))
	{

		PHY_ERR("PHY_PageRead: read timeout, 0x%x, 0x%x, 0x%x, 0x%x\n", ret[0][0], ret[0][1], ret[1][0], ret[1][1]);
		result = -ERR_TIMEOUT;
		return result;
	}
	else if((ret[0][0] == -ERR_ECC)||(ret[0][1] == -ERR_ECC)||(ret[1][0] == -ERR_ECC)||(ret[1][1] == -ERR_ECC))
	{

		PHY_ERR("PHY_PageRead: too much ecc err, 0x%x, 0x%x,\n", (__u32)ret[0][0], (__u32)ret[0][1]);
		PHY_ERR("PHY_PageRead: too much ecc err, 0x%x, 0x%x,\n", (__u32)ret[1][0], (__u32)ret[1][1]);
		PHY_ERR("bank %x block 0x%x,page 0x%x \n",(__u32)pPageAdr->BankNum, (__u32)pPageAdr->BlkNum,
					(__u32)pPageAdr->PageNum);
		PHY_ERR("secbitmap low: 0x%x, high 0x%x \n",(__u32)pPageAdr->SectBitmap, (__u32)(pPageAdr->SectBitmap>>32));
		result = -ERR_TIMEOUT; //0;

		if (PHY_PAGE_READ_ECC_ERR_DEBUG_ON)
		{
			PHY_ERR("PHY_PageRead: ecc error, pending...\n");
		}

		return result;
	}
	else if((ret[0][0] == ECC_LIMIT)||(ret[0][1] == ECC_LIMIT)||(ret[1][0] == ECC_LIMIT)||(ret[1][1] == ECC_LIMIT))
	{
		PHY_ERR("PHY_PageRead: ecc limit, 0x%x, 0x%x, 0x%x, 0x%x\n", ret[0][0], ret[0][1], ret[1][0], ret[1][1]);
		PHY_ERR("bank %x block 0x%x,page 0x%x \n",(__u32)pPageAdr->BankNum, (__u32)pPageAdr->BlkNum,
			(__u32)pPageAdr->PageNum);
		PHY_ERR("secbitmap low: 0x%x, high 0x%x \n",(__u32)pPageAdr->SectBitmap, (__u32)(pPageAdr->SectBitmap>>32));
				result = ECC_LIMIT;
		return result;
	}

	return (result);
}


void _cal_addr_in_chip_for_spare(__u32 block, __u32 page, __u32 sector,__u8 *addr, __u8 cycle)
{
	__u32 row;
	__u32 column;
	#if 0
	__u32 ecc_size;

	if(NandStorageInfo.EccMode == 0)
	  ecc_size = 32;
	else if(NandStorageInfo.EccMode == 1)
	  ecc_size = 53;
	else if(NandStorageInfo.EccMode == 2)
	  ecc_size = 60;
	else
	  ecc_size = 32;
	#endif

	//column = (1024 + ecc_size)* (sector>>1);
	column = 1024* (sector>>1);
	row = block * PAGE_CNT_OF_PHY_BLK + page;

	switch(cycle){
		case 1:
			addr[0] = 0x00;
			break;
		case 2:
			addr[0] = column & 0xff;
			addr[1] = (column >> 8) & 0xff;
			break;
		case 3:
			addr[0] = row & 0xff;
			addr[1] = (row >> 8) & 0xff;
			addr[2] = (row >> 16) & 0xff;
			break;
		case 4:
			addr[0] = column && 0xff;
			addr[1] = (column >> 8) & 0xff;
			addr[2] = row & 0xff;
			addr[3] = (row >> 8) & 0xff;
			break;
		case 5:
			addr[0] = column & 0xff;
			addr[1] = (column >> 8) & 0xff;
			addr[2] = row & 0xff;
			addr[3] = (row >> 8) & 0xff;
			addr[4] = (row >> 16) & 0xff;
			break;
		default:
			break;
	}
}


__s32 _read_sectors_for_spare(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__u8 sparebuf[4*2];
	__u8 default_value[16];
	__u8 addr[5];
	__u8 addr1[2],addr2[2];
	__u32 column;
	__u32 list_len,i,j, k;
	__u32 rb;
	__s32 ret, ret1 , free_page_flag;
	__u32 ecc_size;
	__s32 random_seed;
	NFC_CMD_LIST cmd_list[4];

	if(NandStorageInfo.EccMode == 0)
	  ecc_size = 32;
	else if(NandStorageInfo.EccMode == 1)
	  ecc_size = 46;
	else if(NandStorageInfo.EccMode == 2)
	  ecc_size = 54;
	else if(NandStorageInfo.EccMode == 3)
	  ecc_size = 60;
	else
	  ecc_size = 32;

	for(i=0; i<8; i++)
		sparebuf[i] = 0x3e;

	ret = 0;
	ret1 = 0;

	_cal_addr_in_chip(readop->block,readop->page, 0, addr, 5);
	_add_cmd_list(cmd_list,0x00,5,addr, NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE, NDFC_IGNORE);
	if (SUPPORT_MULTI_READ){
	/*send 0x00 + 5addr*/
		list_len = 1;
	}
	else{
	/*send 0x00 + 5addr --- 0x30*/
		list_len = 2;
		_add_cmd_list(cmd_list + 1,0x30,0,NDFC_IGNORE,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE,NDFC_WAIT_RB);
	}

	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	/*wait rb ready*/
	ret = _wait_rb_ready(readop->chip);
	if (ret)
		return ret;

	rb = _cal_real_rb(readop->chip);
	NFC_SelectChip(readop->chip);
	NFC_SelectRb(rb);

	ret = NFC_Read(cmd_list, NULL, NULL, 0 , NDFC_NORMAL_MODE);

	/*send 05 + 2addr- e0 get 512 byte data*/
	list_len = 4;
	for (i = 0; i < 2; i++){
		if (readop->sectorbitmap & ((__u64)1 << i)){

			/*get main data addr*/
			_cal_addr_in_chip_for_spare(0,0,i<<1,addr1,2);
			/*get spare data addr*/
			//column = (1024 + ecc_size)*i +1024;
			column = 512*SECTOR_CNT_OF_SINGLE_PAGE + i*ecc_size;
			addr2[0] = column & 0xff;
			addr2[1] = (column >> 8) & 0xff;

			_add_cmd_list(cmd_list, 0x05, 2, addr1,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
			_add_cmd_list(cmd_list+1,0xe0, 0,NDFC_IGNORE,NDFC_DATA_FETCH,NDFC_MAIN_DATA_FETCH,1024,NDFC_NO_WAIT_RB);
			_add_cmd_list(cmd_list+2,0x05,2,addr2,NDFC_NO_DATA_FETCH,NDFC_IGNORE,NDFC_IGNORE,NDFC_IGNORE);
			_add_cmd_list(cmd_list+3,0xe0, 0,NDFC_IGNORE,NDFC_DATA_FETCH,NDFC_SPARE_DATA_FETCH,NDFC_IGNORE,NDFC_NO_WAIT_RB);
			for(j = 0; j < list_len - 1; j++){
				cmd_list[j].next = &(cmd_list[j+1]);
			}
			if (_wait_rb_ready(readop->chip))
				return ERR_TIMEOUT;

	        if(SUPPORT_READ_RETRY)
	        {
		        if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x60))  //toshiba & Samsung mode & Sandisk mode & micron mode & intel mode
		        {
					RetryCount[NandIndex][readop->chip] = 0;
					if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //Sandisk mode
					{
						if(READ_RETRY_MODE==0x30)//for sandisk 19nm flash
						{
							if((readop->page!=255)&&((readop->page==0)||((readop->page)%2))) //page low or page high
							{
								READ_RETRY_TYPE = 0x301009;
								NFC_ReadRetryInit(READ_RETRY_TYPE);
							}
							else
							{
								READ_RETRY_TYPE = 0x301409;
								NFC_ReadRetryInit(READ_RETRY_TYPE);
							}
						}
					}
				}
                for( k = 0; k<(READ_RETRY_CYCLE+1);k++)
	    		{
	    			if(RetryCount[NandIndex][readop->chip]==(READ_RETRY_CYCLE+1))
	    				RetryCount[NandIndex][readop->chip] = 0;

	    			if(k>0)
	    			{
	    			    if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
	    			    {
	    			        PHY_ERR("[Read_sectors] NFC_ReadRetry fail \n");
	    			        return -1;
	    			    }
	    			}

					if(SUPPORT_RANDOM)
	    			{
	    				random_seed = _cal_random_seed(readop->page);
	    				NFC_SetRandomSeed(random_seed);
	    				NFC_RandomEnable();
	    				free_page_flag = 0;
	    				ret1 = NFC_Read(cmd_list, (__u8 *)(readop->mainbuf)+i*1024,sparebuf+4*i, dma_wait_mode , NDFC_NORMAL_MODE);
	    				if(readop->page == 0)
    					{
    						if((sparebuf[4*i] == 0x0a)&&(sparebuf[4*i+1] == 0x53)&&(sparebuf[4*i+2] == 0xb8)&&(sparebuf[4*i+3] == 0xc2))
    							free_page_flag = 1;

    					}
    					else if((readop->page%128) == 127)
    					{
    						if((sparebuf[4*i] == 0x32)&&(sparebuf[4*i+1] == 0x43)&&(sparebuf[4*i+2] == 0xaa)&&(sparebuf[4*i+3] == 0x4e))
    							free_page_flag = 1;

    					}

    					if(free_page_flag)
    					{
    						ret1 = 0;
    						sparebuf[4*i] = 0xff;
    						sparebuf[4*i+1] = 0xff;
    						sparebuf[4*i+2] = 0xff;
    						sparebuf[4*i+3] = 0xff;
    					}
	    				NFC_RandomDisable();
	    				if(ret1 == -ERR_ECC)
	    					ret1 = NFC_Read(cmd_list, (__u8 *)(readop->mainbuf)+i*1024,sparebuf+4*i, dma_wait_mode , NDFC_NORMAL_MODE);

	    			    	/**************************************************************************************
							* 1. add by Neil, from v2.09
							* 2. if spare area is all 0xff in random disable mode, it means the page is a clear page
							* 3. because in toshiba 24nm nand, too many clear pages are not all 0xff
							***************************************************************************************/
							if((ret1 == -ERR_ECC)&&(sparebuf[4*i]==0xff)&&(sparebuf[4*i+1]==0xff)&&(sparebuf[4*i+2]==0xff)&&(sparebuf[4*i+3]==0xff))
							{
								//PHY_DBG("[Read_sectors_for_spare] find not all 0xff clear page!  chip = %d, block = %d, page = %d\n, i = %d", readop->chip, readop->block, readop->page, i);
								ret1 = 0;
							}
	    			}
	    			else
	    			{
	    				ret1 = NFC_Read(cmd_list, (__u8 *)(readop->mainbuf)+i*1024,sparebuf+4*i, dma_wait_mode , NDFC_NORMAL_MODE);
	    			}

					if((ret1 != -ERR_ECC)||(k==(READ_RETRY_CYCLE)))
					{
						if(k==0)
						{
							break;
						}
						else
						{
							if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x20))  //toshiba mode
						    {
			    			    //exit toshiba readretry
			    				PHY_ResetChip_CurCH(readop->chip);
						    }
							else if((READ_RETRY_MODE>=0x30)&&(READ_RETRY_MODE<0x40))  //sandisk
							{
								NFC_ReadRetry_off(readop->chip);

							}
						    else if((READ_RETRY_MODE>=0x20)&&(READ_RETRY_MODE<0x30))   //samsung mode
						    {
						        NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
						    }
							else if((READ_RETRY_MODE>=0x40)&&(READ_RETRY_MODE<0x50))   //micron mode
						    {
						        NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
						    }
							else if((READ_RETRY_MODE>=0x50)&&(READ_RETRY_MODE<0x60))   //intel mode
						    {
						        NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
						    }

							break;
						}
					}

					RetryCount[NandIndex][readop->chip]++;
				}

    		    if(k>0)
            	{
            		PHY_DBG("[Read_sectors_for_spare] NFC_ReadRetry %d cycles, ch =%d, chip = %d \n", (__u32)k, (__u32)NandIndex, (__u32)readop->chip);
					PHY_DBG("[Read_sectors_for_spare]	block = %d, page = %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)RetryCount[NandIndex][readop->chip]);
            		if(ret1 == -ERR_ECC)
            		{
            		    PHY_DBG("ecc error!\n");
            			//PHY_DBG("spare buf: %x, %x, %x, %x\n", sparebuf[4*i],sparebuf[4*i+1],sparebuf[4*i+2],sparebuf[4*i+3]);
						if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)) //hynix mode
						{
							NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
							RetryCount[NandIndex][readop->chip] = 0;
						}
					}
				}

    			if(ret1 == ECC_LIMIT)
    				ret1 = ECC_LIMIT;
	        }
	        else
	        {
				if(SUPPORT_RANDOM)
	            {
	    			random_seed = _cal_random_seed(readop->page);
	    			NFC_SetRandomSeed(random_seed);
	    			NFC_RandomEnable();
					free_page_flag = 0;
	    			ret1 = NFC_Read(cmd_list, (__u8 *)(readop->mainbuf)+i*1024,sparebuf+4*i, dma_wait_mode , NDFC_NORMAL_MODE);
					if(readop->page == 0)
					{
						if((sparebuf[4*i] == 0x0a)&&(sparebuf[4*i+1] == 0x53)&&(sparebuf[4*i+2] == 0xb8)&&(sparebuf[4*i+3] == 0xc2))
							free_page_flag = 1;

					}
					else if((readop->page%128) == 127)
					{
						if((sparebuf[4*i] == 0x32)&&(sparebuf[4*i+1] == 0x43)&&(sparebuf[4*i+2] == 0xaa)&&(sparebuf[4*i+3] == 0x4e))
							free_page_flag = 1;

					}

					if(free_page_flag)
					{
						ret1 = 0;
						sparebuf[4*i] = 0xff;
						sparebuf[4*i+1] = 0xff;
						sparebuf[4*i+2] = 0xff;
						sparebuf[4*i+3] = 0xff;
					}
	    			NFC_RandomDisable();
	    			if(ret1 == -ERR_ECC)
	    				ret1 = NFC_Read(cmd_list, (__u8 *)(readop->mainbuf)+i*1024,sparebuf+4*i, dma_wait_mode , NDFC_NORMAL_MODE);
				}
	    		else
	    		{
	    			ret1 = NFC_Read(cmd_list, (__u8 *)(readop->mainbuf)+i*1024,sparebuf+4*i, dma_wait_mode , NDFC_NORMAL_MODE);
	    		}


	        }

			ret |= ret1;

			if (dma_wait_mode)
				_pending_dma_irq_sem();

			if (readop->oobbuf){
				MEMCPY((__u8 *)(readop->oobbuf)+4*i,sparebuf+4*i,4);
			}
		}

	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	return ret;
}

__s32  PHY_PageReadSpare(struct __PhysicOpPara_t *pPageAdr)
{
	__s32 ret[2][2] = { {0, 0}, {0, 0}};
	__s32 result = 0;
	__u32 chip;
	__u32 block_in_chip;
	__u32 plane_cnt,i, j, k;
	__u32 bitmap_in_single_page;
	struct boot_physical_param readop;
	__u8 tmp_oob[2][2][16]; //__u8 tmp_oob[2][2][8];
	__u32 bad_flag =0;
	__u32 dbg = 1;
	//unsigned long ts0,te0, ts1,te1;
	//unsigned long ts2[2],te2[2];
	//unsigned long ts3[2],te3[2];
	//unsigned long ts4[2],te4[2];
	//unsigned long ts5[2],te5[2];


	for (i=0; i<2; i++) /*plane*/
		for (j=0; j<2; j++) /*channel*/
			for (k=0; k<16; k++) /*user data*/
				tmp_oob[i][j][k] = 0x3c;

	plane_cnt = SUPPORT_MULTI_PROGRAM ? PLANE_CNT_OF_DIE : 1;

	/*get chip no*/
	chip = _cal_real_chip(pPageAdr->BankNum);
	if (0xff == chip) {
		PHY_ERR("PHY_PageRead : beyond chip count, %d.\n", chip);
		return -ERR_INVALIDPHYADDR;
	}
	/*get block no within chip*/
	block_in_chip = _cal_block_in_chip(pPageAdr->BankNum,pPageAdr->BlkNum);
	if (0xffffffff == block_in_chip) {
		PHY_ERR("PHY_PageReadSpare : beyond block of per chip count, %d.\n", block_in_chip);
		return -ERR_INVALIDPHYADDR;
	}

	for (i=0; i<plane_cnt; i++)
	{
		//ts0 = jiffies();

		/*init single page operation param*/
		readop.chip = chip;
		readop.block = block_in_chip  + i*MULTI_PLANE_BLOCK_OFFSET;
		readop.page = pPageAdr->PageNum;

		for(NandIndex=0; NandIndex<CHANNEL_CNT; NandIndex++)
		{
			//ts2[NandIndex] = jiffies();
			if(i==0)
				_phy_read_status(chip);

			readop.mainbuf = (__u8 *)(pPageAdr->MDataPtr) + 512*SECTOR_CNT_OF_SINGLE_PAGE*(i*CHANNEL_CNT+ NandIndex);
			if (pPageAdr->SDataPtr) {
				readop.oobbuf = tmp_oob[i][NandIndex];
			} else {
				readop.oobbuf = NULL;
			}

			bitmap_in_single_page = FULL_BITMAP_OF_SINGLE_PAGE &(pPageAdr->SectBitmap >>((i*CHANNEL_CNT+ NandIndex)*SECTOR_CNT_OF_SINGLE_PAGE));
			readop.sectorbitmap = bitmap_in_single_page;

			//ts3[NandIndex] = jiffies();
			if (bitmap_in_single_page)
			{
			/*bitmap of this plane is valid */
				//if(bitmap_in_single_page == FULL_BITMAP_OF_SINGLE_PAGE) {
				//	/*align page, use page mode */
				//	ret[NandIndex][i] |= _read_single_page_first(&readop,SUPPORT_DMA_IRQ);
				//} else {
					/*not align page , normal mode*/
					ret[NandIndex][i] |= _read_single_page_spare_first(&readop,SUPPORT_DMA_IRQ);
				//}
			}
			//te3[NandIndex] = jiffies();

			if(NandIndex == (CHANNEL_CNT-1))
				break;

			//te2[NandIndex] = jiffies();
		}
		//te0 = jiffies();

		//ts1 = jiffies();
		for(NandIndex=0; NandIndex<CHANNEL_CNT; NandIndex++)
		{
			//ts4[NandIndex] = jiffies();
			readop.mainbuf = (__u8 *)(pPageAdr->MDataPtr) + 512*SECTOR_CNT_OF_SINGLE_PAGE*(i*CHANNEL_CNT+ NandIndex);
			if (pPageAdr->SDataPtr) {
				readop.oobbuf = tmp_oob[i][NandIndex];
			} else {
				readop.oobbuf = NULL;
			}

			bitmap_in_single_page = FULL_BITMAP_OF_SINGLE_PAGE &(pPageAdr->SectBitmap >>((i*CHANNEL_CNT+ NandIndex)*SECTOR_CNT_OF_SINGLE_PAGE));
			readop.sectorbitmap = bitmap_in_single_page;

			//ts5[NandIndex] = jiffies();
			if (bitmap_in_single_page)
			{
			/*bitmap of this plane is valid */
				//if(bitmap_in_single_page == FULL_BITMAP_OF_SINGLE_PAGE) {
				//	/*align page, use page mode */
				//	ret[NandIndex][i] |= _read_single_page_wait(&readop,SUPPORT_DMA_IRQ);
				//} else {
					/*not align page , normal mode*/
					ret[NandIndex][i] |= _read_single_page_spare_wait(&readop,SUPPORT_DMA_IRQ);
				//}
			}
			//te5[NandIndex] = jiffies();

			if(NandIndex == (CHANNEL_CNT-1))
				break;
			//te4[NandIndex] = jiffies();
		}

		//te1 = jiffies();
	}

	NandIndex = 0;

	//PHY_DBG("read spare: %d us wait: %d us  3-0: %d 3-1: %d  5-0: %d 5-1: %d\n", te0-ts0, te1-ts1, te3[0]-ts3[0], te3[1]-ts3[1], te5[0]-ts5[0], te5[1]-ts5[1]);

	if(pPageAdr->SDataPtr)
	{
		bad_flag = 0;
		for(i=0; i<plane_cnt; i++)
		{
			for(j=0; j<CHANNEL_CNT; j++)
			{
				if((tmp_oob[i][j][0] != 0xff)&&(tmp_oob[i][j][0] != 0x3c))
				{
					bad_flag = 1;
					PHY_DBG("PHY_PageReadSpare bad flag: bank 0x%x  block 0x%x  page 0x%x \n", (__u32)pPageAdr->BankNum,(__u32)pPageAdr->BlkNum,
	    					(__u32)pPageAdr->PageNum);
					PHY_DBG("plane:%d, ch:%d, chip:%x, blk:0x%x, pg:0x%x, 0x%08x 0x%08x 0x%08x 0x%08x\n",
							i, j, readop.chip, readop.block, readop.page,
							*((__u32 *)(&tmp_oob[i][j][0])), *((__u32 *)(&tmp_oob[i][j][4])),
							*((__u32 *)(&tmp_oob[i][j][8])), *((__u32 *)(&tmp_oob[i][j][12])));
				}
			}
		}

		if(bad_flag)
			tmp_oob[0][0][0] = 0;

#if 0
		if (SECTOR_CNT_OF_SINGLE_PAGE == 2) {
			/*physical page size is 1KB
			* if CHANNEL_CNT is 1, tmp_oob[0][1][0~3], tmp_oob[1][1][0~3] are invalid
			*/
			MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 4);
			MEMCPY((__u8 *)pPageAdr->SDataPtr +4, &tmp_oob[0][1][0], 4);
			MEMCPY((__u8 *)pPageAdr->SDataPtr +8, &tmp_oob[1][0][0], 4);
			MEMCPY((__u8 *)pPageAdr->SDataPtr +12, &tmp_oob[1][1][0], 4);
		} else if (SECTOR_CNT_OF_SINGLE_PAGE == 4) {
			/*physical page size is 2KB*/
			MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 8);
			if (CHANNEL_CNT == 1)
				MEMCPY((__u8 *)pPageAdr->SDataPtr +8, &tmp_oob[1][0][0], 8);
			else
				MEMCPY((__u8 *)pPageAdr->SDataPtr +8, &tmp_oob[0][1][0], 8);
		} else {
			/*physical page size is equal to or greater than 4KB*/
			MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 16);
		}
#else
		/*only support physical page size is equal to or greater than 4KB*/
		MEMCPY((__u8 *)pPageAdr->SDataPtr, &tmp_oob[0][0][0], 8);
#endif

		//only for test
#if 0
		i = 1;
		if (bad_flag) { //if (SECTOR_CNT_OF_SINGLE_PAGE >= 8) { //if (bad_flag) { //if (SECTOR_CNT_OF_SINGLE_PAGE >= 8) {
#if 0
			i = 0;
			for (j=0; j<16; j++)
				if (tmp_oob[0][0][j] != tmp_oob[0][1][j]) {
					PHY_ERR("\n\n[PHY_ERR]user data in plane0-ch0 and plane0-ch1 are different!");
					i = 1;
					break;
				}
#endif
			if (i == 1) {
				PHY_DBG("\nplane0-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[0][0][j]);
				PHY_DBG("\nplane0-ch1: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[0][1][j]);
				PHY_DBG("\n");
			}
#if 0
			i = 0;
			for (j=0; j<16; j++)
				if (tmp_oob[1][0][j] != tmp_oob[1][1][j]) {
					PHY_ERR("\n\n[PHY_ERR]user data in plane1-ch0 and plane1-ch1 are different!");
					i = 1;
					break;
				}
#endif
			if (i == 1) {
				PHY_DBG("\nplane1-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[1][0][j]);
				PHY_DBG("\nplane1-ch1: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[1][1][j]);
				PHY_DBG("\n");
			}
#if 0
			i = 0;
			for (j=0; j<16; j++)
				if (tmp_oob[0][0][j] != tmp_oob[1][0][j]) {
					PHY_ERR("\n\n[PHY_ERR]user data in plane0-ch0 and plane1-ch0 are different!");
					i = 1;
					break;
				}
			if (i == 1) {
				PHY_DBG("\nplane0-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[0][0][j]);
				PHY_DBG("\nplane1-ch0: ");
				for (j=0; j<16; j++)
					PHY_DBG("%02x ", tmp_oob[1][0][j]);
				PHY_DBG("\n");
			}
#endif
		}

#endif
	}

	if((ret[0][0] == -ERR_TIMEOUT)||(ret[0][1] == -ERR_TIMEOUT)||(ret[1][0] == -ERR_TIMEOUT)||(ret[1][1] == -ERR_TIMEOUT))
	{

		PHY_ERR("PHY_PageReadSpare: read timeout, 0x%x, 0x%x, 0x%x, 0x%x\n", ret[0][0], ret[0][1], ret[1][0], ret[1][1]);
		result = -ERR_TIMEOUT;
		return result;
	}
	else if((ret[0][0] == -ERR_ECC)||(ret[0][1] == -ERR_ECC)||(ret[1][0] == -ERR_ECC)||(ret[1][1] == -ERR_ECC))
	{

		PHY_ERR("PHY_PageReadSpare: too much ecc err, 0x%x, 0x%x, 0x%x, 0x%x\n", (__u32)ret[0][0], (__u32)ret[0][1], (__u32)ret[1][0], (__u32)ret[1][1]);
		PHY_ERR("bank %x block 0x%x,page 0x%x \n",(__u32)pPageAdr->BankNum, (__u32)pPageAdr->BlkNum,
					(__u32)pPageAdr->PageNum);
		PHY_ERR("secbitmap low: 0x%x, high 0x%x \n",(__u32)pPageAdr->SectBitmap, (__u32)(pPageAdr->SectBitmap>>32));
		result = -ERR_TIMEOUT;    //0

		if (PHY_PAGE_READ_ECC_ERR_DEBUG_ON)
		{
			PHY_ERR("PHY_PageReadSpare: ecc error, pending...\n");
			while(dbg);
		}

		return result;
	}
	else if((ret[0][0] == ECC_LIMIT)||(ret[0][1] == ECC_LIMIT)||(ret[1][0] == ECC_LIMIT)||(ret[1][1] == ECC_LIMIT))
	{
		PHY_ERR("PHY_PageReadSpare: ecc limit, 0x%x, 0x%x, 0x%x, 0x%x\n", ret[0][0], ret[0][1], ret[1][0], ret[1][1]);
		PHY_ERR("bank %x block 0x%x,page 0x%x \n",(__u32)pPageAdr->BankNum, (__u32)pPageAdr->BlkNum,
			(__u32)pPageAdr->PageNum);
		PHY_ERR("secbitmap low: 0x%x, high 0x%x \n",(__u32)pPageAdr->SectBitmap, (__u32)(pPageAdr->SectBitmap>>32));
		result = ECC_LIMIT;
		return result;
	}

	return (result);
}

void PHY_FreePageCheck(struct __PhysicOpPara_t  *pPageAdr)
{
	__u8 oob[8];
	__u32 i;

	struct __PhysicOpPara_t  param = *pPageAdr;
	param.MDataPtr = (__u8 *)PageCachePool.TmpPageCache;
	param.SDataPtr = oob;
	param.SectBitmap = 0x3;


	PHY_PageRead(&param);
	for(i = 0; i < 8; i++)
	{
		if( (i%4 != 3) && (oob[i] != 0xff))
		{
			PHY_ERR("%s : sorry, you can not write in dirty page\n",__FUNCTION__);
			PHY_ERR("oob data : --%x--%x--%x--%x--%x--%x--\n",
				oob[0],oob[1],oob[2],oob[4],oob[5],oob[6]);
			while(1);
		}
	}

	FREE(param.MDataPtr,1024);
}

/*
************************************************************************************************************************
*                       WRITE NAND FLASH PHYSICAL PAGE DATA
*
*Description: Write a page from buffer to a nand flash physical page.
*
*Arguments  : pPageAdr      the po__s32er to the accessed page parameter.
*
*Return     : The result of the page write;
*               = 0     page write successful;
*               > 0     page write successful, but need do some process;
*               < 0     page write failed.
************************************************************************************************************************
*/

__s32  PHY_PageWrite(struct __PhysicOpPara_t  *pPageAdr)
{
	__s32 ret = 0;
	__u32 chip;
	__u32 block_in_chip;
	__u32 plane_cnt,i;
	__u32 program1,program2;
	__u8 default_value[16];
	__u32 rb = 0;
	struct boot_physical_param writeop;
	//unsigned long ts0,te0, ts1,te1;
	//unsigned long ts2[2],te2[2];
	//unsigned long ts3[2],te3[2];
	//unsigned long ts4[2],te4[2];
	//unsigned long ts5[2],te5[2];

	//PHY_FreePageCheck(pPageAdr);

	ret = 0;
	plane_cnt = SUPPORT_MULTI_PROGRAM ? PLANE_CNT_OF_DIE : 1;

	/*get chip no*/
	chip = _cal_real_chip(pPageAdr->BankNum);
	if (0xff == chip){
		PHY_ERR("PHY_PageWrite : beyond chip count, %d\n", chip);
		return -ERR_INVALIDPHYADDR;
	}
	/*get block no within chip*/
	block_in_chip = _cal_block_in_chip(pPageAdr->BankNum,pPageAdr->BlkNum);

	if (0xffffffff == block_in_chip){
		PHY_ERR("PHY_PageWrite : beyond block of per chip count, %d\n", block_in_chip);
		return -ERR_INVALIDPHYADDR;
	}

	//_wait_rb_ready(chip);

	for (i=0; i<plane_cnt; i++)
	{
		//ts0 = jiffies();

		for (NandIndex = 0; NandIndex<CHANNEL_CNT; NandIndex++)
		{
			//ts2[NandIndex] = jiffies();

			if(i == 0)
				_wait_rb_ready_int(chip);
			else
				_wait_rb_ready(chip);

			/*init single page operation param*/
			writeop.chip = chip;
			writeop.block = block_in_chip + i*MULTI_PLANE_BLOCK_OFFSET;
			writeop.page = pPageAdr->PageNum;

			writeop.mainbuf = (__u8 *)(pPageAdr->MDataPtr) + 512*SECTOR_CNT_OF_SINGLE_PAGE*(i*CHANNEL_CNT+ NandIndex);
			if (pPageAdr->SDataPtr) {
				#if 0
				//writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 4*SECTOR_CNT_OF_SINGLE_PAGE*(plane_cnt*NandIndex + i);
				//writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr);
				if (SECTOR_CNT_OF_SINGLE_PAGE == 2) {
					//physical page size is 1KB
					writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 4*(i*CHANNEL_CNT+ NandIndex);
				} else if (SECTOR_CNT_OF_SINGLE_PAGE == 4) {
					//physical page size is 2KB
					writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 8*((i*CHANNEL_CNT+ NandIndex)&0x1);
				} else {
					//PHY_DBG("%s: ================\n", __func__);
					writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr);
				}
				#else
				//only support physical page size >= 4KB.
				writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr);
				#endif
			} else {
				writeop.oobbuf = NULL;
			}

			writeop.sectorbitmap = FULL_BITMAP_OF_SINGLE_PAGE;
			if (i == 0) {
				program1 = 0x80;
				if (SUPPORT_MULTI_PROGRAM)
					program2 = NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[0];
				else
					program2 = 0x10;
			} else {
				program1 = NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[1];
				program2 = 0x10;
			}
			//for hynix 20nm flash,retry value need to write default value before write
			if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)||(READ_RETRY_MODE==0x4)) {
				if(RetryCount[NandIndex][writeop.chip] != 0) {
					rb = _cal_real_rb(writeop.chip);
					NFC_SelectChip(writeop.chip);
					NFC_SelectRb(rb);
					NFC_SetDefaultParam(writeop.chip, default_value, READ_RETRY_TYPE);
					RetryCount[NandIndex][writeop.chip] = 0;
				}
			}

			//ts3[NandIndex] = jiffies();
			ret |= _write_single_page_first(&writeop,program1,program2,SUPPORT_DMA_IRQ,SUPPORT_RB_IRQ);
			//te3[NandIndex] = jiffies();

			if(NandIndex == (CHANNEL_CNT-1))
				break;

			//te2[NandIndex] = jiffies();
		}

		//te0 = jiffies();

		//ts1 = jiffies();
		for (NandIndex=0; NandIndex<CHANNEL_CNT; NandIndex++)
		{
			//ts4[NandIndex] = jiffies();

			/*init single page operation param*/
			writeop.chip = chip;
			writeop.block = block_in_chip + i*MULTI_PLANE_BLOCK_OFFSET;
			writeop.page = pPageAdr->PageNum;

			writeop.mainbuf = (__u8 *)(pPageAdr->MDataPtr) + 512*SECTOR_CNT_OF_SINGLE_PAGE*(i*CHANNEL_CNT+ NandIndex);
			if (pPageAdr->SDataPtr) {
				#if 0
				//writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 4*SECTOR_CNT_OF_SINGLE_PAGE*(plane_cnt*NandIndex + i);
				//writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr);
				if (SECTOR_CNT_OF_SINGLE_PAGE == 2) {
					//physical page size is 1KB
					writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 4*(i*CHANNEL_CNT+ NandIndex);
				} else if (SECTOR_CNT_OF_SINGLE_PAGE == 4) {
					//physical page size is 2KB
					writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr) + 8*((i*CHANNEL_CNT+ NandIndex)&0x1);
				} else {
					//PHY_DBG("%s: ================\n", __func__);
					writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr);
				}
				#else
				//only support physical page size >= 4KB.
				writeop.oobbuf = (__u8 *)(pPageAdr->SDataPtr);
				#endif
			} else {
				writeop.oobbuf = NULL;
			}

			writeop.sectorbitmap = FULL_BITMAP_OF_SINGLE_PAGE;
			if (i == 0) {
				program1 = 0x80;
				if (SUPPORT_MULTI_PROGRAM)
					program2 = NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[0];
				else
					program2 = 0x10;
			} else {
				program1 = NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[1];
				program2 = 0x10;
			}
			//ts5[NandIndex] = jiffies();
			ret |= _write_single_page_wait(&writeop,program1,program2,SUPPORT_DMA_IRQ,SUPPORT_RB_IRQ);
			//te5[NandIndex] = jiffies();

			if(NandIndex == (CHANNEL_CNT-1))
				break;

			//te4[NandIndex] = jiffies();
		}

		//te1 = jiffies();
	}

	NandIndex = 0;

	//PHY_DBG("write: %d us wait: %d us  3-0: %d 3-1: %d  5-0: %d 5-1: %d\n", te0-ts0, te1-ts1, te3[0]-ts3[0], te3[1]-ts3[1], te5[0]-ts5[0], te5[1]-ts5[1]);

	if (ret == -ERR_TIMEOUT)
		PHY_ERR("PHY_PageWrite : write timeout\n");
	return ret;
}

__s32 _single_copy_back(struct __PhysicOpPara_t *pSrcPage, struct __PhysicOpPara_t *pDstPage)
{
	__u8 addr[5];
	__s32 ret;
	__u32 chip;
	__u32 rb;
	__u32 src_block_in_chip,dst_block_in_chip;
	__u32 list_len,i,addr_cycle;
	NFC_CMD_LIST cmd_list[8];

	/*get chip no*/
	chip = _cal_real_chip(pSrcPage->BankNum);
	if (0xff == chip){
		PHY_ERR("single_copy_back : beyond chip count\n");
		return -ERR_INVALIDPHYADDR;
	}
	/*get block no within chip*/
	src_block_in_chip = _cal_block_in_chip(pSrcPage->BankNum,pSrcPage->BlkNum);
	if (0xffffffff == src_block_in_chip){
		PHY_ERR("single_copy_back : beyond block of per chip  count\n");
		return -ERR_INVALIDPHYADDR;
	}
	dst_block_in_chip = _cal_block_in_chip(pDstPage->BankNum,pDstPage->BlkNum);
	if (0xffffffff == dst_block_in_chip){
		PHY_ERR("single_copy_back : beyond block of per chip  count\n");
		return -ERR_INVALIDPHYADDR;
	}


	/*create cmd list*/
	addr_cycle = (SECTOR_CNT_OF_SINGLE_PAGE == 1) ?4:5;
	list_len = 2;
	/*copy back read*/
	_cal_addr_in_chip(src_block_in_chip,pSrcPage->PageNum,0,addr,addr_cycle);
	_add_cmd_list(cmd_list, 0x00,addr_cycle,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list+1,0x35, 0,NDFC_IGNORE,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	_wait_rb_ready(chip);
	rb = _cal_real_rb(chip);
	NFC_SelectChip(chip);
	NFC_SelectRb(rb);
	ret = NFC_CopyBackRead(cmd_list);
	if (ret)
		return -1;
	/*copy back write*/
	_cal_addr_in_chip(dst_block_in_chip,pDstPage->PageNum,0,addr,addr_cycle);
	_add_cmd_list(cmd_list, 0x85,addr_cycle,addr,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
	_add_cmd_list(cmd_list+1,0x10, 0,NDFC_IGNORE,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}
	ret = NFC_CopyBackWrite(cmd_list,0);

	NFC_DeSelectChip(chip);
	NFC_DeSelectRb(rb);
	return ret;
}

__s32 _multi_copy_back(struct __PhysicOpPara_t *pSrcPage, struct __PhysicOpPara_t *pDstPage)
{
	__u8 addr[2][5];
	__s32 ret;
	__u32 chip;
	__u32 rb;
	__u32 src_block_in_chip,dst_block_in_chip;
	__u32 list_len,i,addr_cycle,plane_cnt;
	__u32 program1,program2;
	NFC_CMD_LIST cmd_list[8];

	/*get chip no*/
	chip = _cal_real_chip(pSrcPage->BankNum);
	if (0xff == chip){
		PHY_ERR("single_copy_back : beyond chip count\n");
		return -ERR_INVALIDPHYADDR;
	}
	/*get block no within chip*/
	src_block_in_chip = _cal_block_in_chip(pSrcPage->BankNum,pSrcPage->BlkNum);
	if (0xffffffff == src_block_in_chip){
		PHY_ERR("single_copy_back : beyond block of per chip  count\n");
		return -ERR_INVALIDPHYADDR;
	}
	dst_block_in_chip = _cal_block_in_chip(pDstPage->BankNum,pDstPage->BlkNum);
	if (0xffffffff == dst_block_in_chip){
		PHY_ERR("single_copy_back : beyond block of per chip  count\n");
		return -ERR_INVALIDPHYADDR;
	}

	plane_cnt = SUPPORT_MULTI_PROGRAM ? PLANE_CNT_OF_DIE : 1;
	/*create cmd list*/
	addr_cycle = (SECTOR_CNT_OF_SINGLE_PAGE == 1) ?4:5;
	/*copy back read*/
	if (NandStorageInfo.OptPhyOpPar.MultiPlaneCopyReadCmd[0] == 0x60){

		for(i = 0; i< plane_cnt; i++){
			_cal_addr_in_chip(src_block_in_chip+i*MULTI_PLANE_BLOCK_OFFSET, pSrcPage->PageNum,0,addr[i],addr_cycle);
			_add_cmd_list(cmd_list+i, 0x60,addr_cycle,addr[i],NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
		}
		_add_cmd_list(cmd_list+i,0x35, 0,NDFC_IGNORE,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
		list_len = plane_cnt + 1;
	}
	else{

		for(i = 0; i< plane_cnt; i++){
			_cal_addr_in_chip(src_block_in_chip+i*MULTI_PLANE_BLOCK_OFFSET,pSrcPage->PageNum,0,addr[i],addr_cycle);
			_add_cmd_list(cmd_list+2*i, 0x00,addr_cycle,addr[i],NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
			_add_cmd_list(cmd_list+2*i+1,0x35, 0,NDFC_IGNORE,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
		}
		list_len = plane_cnt * 2;
	}
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}

	_wait_rb_ready(chip);
	rb = _cal_real_rb(chip);
	NFC_SelectChip(chip);
	NFC_SelectRb(rb);
	ret = NFC_CopyBackRead(cmd_list);
	if (ret)
		return -1;

	/*copy back write*/
	for(i = 0; i< plane_cnt; i++){
		_cal_addr_in_chip(dst_block_in_chip+i*MULTI_PLANE_BLOCK_OFFSET,pSrcPage->PageNum,0,addr[i],addr_cycle);
		if (i == 0){
			program1 = NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[0];
			program2 = NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[1];
		}
		else{
			program1 = NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[2];
			program2 = 0x10;
		}
		_add_cmd_list(cmd_list+2*i, program1,addr_cycle,addr[i],NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
		_add_cmd_list(cmd_list+2*i+1,program2, 0,NDFC_IGNORE,NDFC_NO_DATA_FETCH,NDFC_IGNORE, NDFC_IGNORE,NDFC_IGNORE);
	}
	list_len = plane_cnt * 2;
	for(i = 0; i < list_len - 1; i++){
		cmd_list[i].next = &(cmd_list[i+1]);
	}
	ret = NFC_CopyBackWrite(cmd_list,0);
	NFC_DeSelectChip(chip);
	NFC_DeSelectRb(rb);

	return ret;

}

/*
************************************************************************************************************************
*                           PHYSIC PAGE COPY-BACK
*
*Description: copy one physical page from one physical block to another physical block.
*
*Arguments  : pSrcPage      the parameter of the source page which need be copied;
*             pDstPage      the parameter of the destination page which copied to.
*
*Return     : the result of the page copy-back;
*               = 0         page copy-back successful;
*               = -1        page copy-back failed.
************************************************************************************************************************
*/
__s32  PHY_PageCopyback(struct __PhysicOpPara_t *pSrcPage, struct __PhysicOpPara_t *pDstPage)
{
	__s32 ret;

	if ( (!SUPPORT_PAGE_COPYBACK) || ((pSrcPage->PageNum + pDstPage->PageNum) & 0x1) ){
	/*cant not use copyback*/
		pSrcPage->SectBitmap = pDstPage->SectBitmap = FULL_BITMAP_OF_SUPER_PAGE;
		pSrcPage->MDataPtr = pDstPage->MDataPtr = PHY_TMP_PAGE_CACHE;
		pSrcPage->SDataPtr = pDstPage->SDataPtr = PHY_TMP_SPARE_CACHE;
		ret = PHY_PageRead(pSrcPage);

		//add in 2010-06-18 by penggang, the logic layer needn't care the ecc error
		if(ret == -ERR_ECC)
			ret = 0;

		if (ret < 0){

			goto PHY_PageCopyback_exit;
		}
		ret = PHY_PageWrite(pDstPage);
		goto PHY_PageCopyback_exit;
	}

	/*use copyback*/
	if (!SUPPORT_MULTI_PROGRAM)
	/*do not support two-plane copyback*/
		ret = _single_copy_back(pSrcPage,pDstPage);

	else
		ret = _multi_copy_back(pSrcPage,pDstPage);
PHY_PageCopyback_exit:
	if (ret == -ERR_TIMEOUT)
		PHY_ERR("PHY_PageCopyback : copy back timeout\n");
	return ret;
}

__s32 PHY_ScanDDRParam(void)
{
	__u32 i, j, k,chip = 0;
	__u32 good_ddr_param[64];
	__u32 good_ddr_param_cnt;
	__u8* main_buf = (__u8 *)MALLOC(8192);
	__u8  oob_buf[32];
	struct boot_physical_param readop;
	__s32 ret;
	__u32 sum, ddr_param;
	__u32 nand_index_temp=0;

	if(DDR_TYPE)
	{
	    //for(k=0;k<NandStorageInfo.ChipCnt;k++)
	    for(k=0;k<1;k++)
	    {
	        chip = _cal_real_chip(k);
            readop.chip = chip;
            readop.block = 1;
            readop.page = 0;
            readop.sectorbitmap = 0x3;
            readop.mainbuf = main_buf;
            readop.oobbuf = oob_buf;

            for(i=1;i<16;i++) //delay_mode
            {

                good_ddr_param_cnt = 0;
                for(j=0;j<64;j++)
                {
                    PHY_DBG("(%d, %d) ", i, j);
                    NFC_InitDDRParam(chip, ((i<<8)|j));
                    ret = PHY_SimpleRead_1K(&readop);

					if(!ret)
					{
						if((oob_buf[0]==0xff)&&(oob_buf[1]==0xff)&&(oob_buf[2]==0xff)&&(oob_buf[3]==0xff))
						{
							nand_index_temp =NandIndex;
							for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
							{
								NandStorageInfo.FrequencePar = 30;
								NAND_SetClk(NandIndex, NandStorageInfo.FrequencePar, NandStorageInfo.FrequencePar*2);

								if(NandIndex == (CHANNEL_CNT-1))
									break;
							}
							if(((*(volatile __u32 *)(0xf8001400+0x190))>>0x3)&0x1)
						    	NFC_InitDDRParam(0, 0x31f);
							else
								NFC_InitDDRParam(0, 0x339);
							NandIndex = nand_index_temp;
							FREE(main_buf, 8192);
							if(((*(volatile __u32 *)(0xf8001400+0x190))>>0x3)&0x1)
								PHY_ERR("PHY_ScanDDRParam, it is a free chip, then set clk 30MHz,delay chain 0x31f \n");
							else
								PHY_ERR("PHY_ScanDDRParam, it is a free chip, then set clk 30MHz,delay chain 0x339 \n");
    						return 0;
						}
					}

                    if(!ret) //find good ddr param
                    {
                        good_ddr_param[good_ddr_param_cnt] = ((i<<8)|j);
                        good_ddr_param_cnt++;
                        PHY_DBG(" ok\n");
                    }
                    else
                    {
                        PHY_DBG(" fail\n");
                    }
                }

                if(good_ddr_param_cnt)
                    break;
            }

            if(good_ddr_param_cnt)
            {
                sum = 0;
                for(i=0;i<good_ddr_param_cnt;i++)
                    sum += good_ddr_param[i];

                ddr_param = sum/good_ddr_param_cnt;
                NFC_InitDDRParam(chip, ddr_param);

                PHY_DBG("PHY_ScanDDRParam, find a good ddr_param 0x%x in chip %d\n", ddr_param, chip);
            }
            else
            {
                PHY_ERR("PHY_ScanDDRParam, can't find a good ddr_param in chip %d\n", chip);
                FREE(main_buf, 8192);
                return -1;
            }
	    }

    }

    FREE(main_buf, 8192);
    return 0;
}

__s32  PHY_ReadNandUniqueId(__s32 bank, void *pChipID)
{
	__s32 ret, err_flag;
	__u32 i, j,nChip, nRb;
	__u8 *temp_id;


	NFC_CMD_LIST cmd;
	__u8 addr = 0;

	nChip = _cal_real_chip(bank);
	NFC_SelectChip(nChip);
	nRb = _cal_real_rb(nChip);
	NFC_SelectRb(nRb);

	for(i=0;i<16; i++)
	{
		addr = i*32;
		temp_id = (__u8*)(pChipID);
		err_flag = 0;

		_add_cmd_list(&cmd, 0xed,1 , &addr, NDFC_DATA_FETCH, NDFC_IGNORE, 32, NDFC_WAIT_RB);
		ret = NFC_GetUniqueId(&cmd, pChipID);

		for(j=0; j<16;j++)
		{
			if((temp_id[j]^temp_id[j+16]) != 0xff)
			{
				err_flag = 1;
				ret = -1;
				break;
			}
		}

		if(err_flag == 0)
		{
			ret = 0;
			break;
		}

	}

	if(ret)
	{
		for(j=0; j<32;j++)
		{
			temp_id[j] = 0x55;
		}
	}

	PHY_DBG("Nand Unique ID of chip %u is : \n", nChip);
	PHY_DBG("%x, %x, %x, %x\n", temp_id[0],temp_id[1],temp_id[2],temp_id[3]);
	PHY_DBG("%x, %x, %x, %x\n", temp_id[4],temp_id[5],temp_id[6],temp_id[7]);
	PHY_DBG("%x, %x, %x, %x\n", temp_id[8],temp_id[9],temp_id[10],temp_id[11]);
	PHY_DBG("%x, %x, %x, %x\n", temp_id[12],temp_id[13],temp_id[14],temp_id[15]);
	PHY_DBG("\n");
	PHY_DBG("%x, %x, %x, %x\n", temp_id[16],temp_id[17],temp_id[18],temp_id[19]);
	PHY_DBG("%x, %x, %x, %x\n", temp_id[20],temp_id[21],temp_id[22],temp_id[23]);
	PHY_DBG("%x, %x, %x, %x\n", temp_id[24],temp_id[25],temp_id[26],temp_id[27]);
	PHY_DBG("%x, %x, %x, %x\n", temp_id[28],temp_id[29],temp_id[30],temp_id[31]);
	PHY_DBG("\n");

	NFC_DeSelectChip(nChip);


	return ret;
}


