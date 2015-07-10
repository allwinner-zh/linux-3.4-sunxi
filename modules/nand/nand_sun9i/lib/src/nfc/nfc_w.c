/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "../include/nfc.h"
#include "../include/nfc_reg.h"
#include "../include/nand_physic.h"
#include "../include/nand_simple.h"
//#include "nand_physic_interface.h"

extern __u32 pagesize;
extern __s32 _wait_cmdfifo_free(void);
extern __s32 _wait_cmd_finish(void);
extern void _dma_config_start(__u8 rw, __u32 buff_addr, __u32 len);
extern __s32 _wait_dma_end(__u8 rw, __u32 buff_addr, __u32 len);
extern __s32 _check_ecc(__u32 eblock_cnt);
extern void _disable_ecc(void);
extern void _enable_ecc(__u32 pipline);
extern __s32 _enter_nand_critical(void);
extern __s32 _exit_nand_critical(void);
extern void _set_addr(__u8 *addr, __u8 cnt);
extern __s32 nfc_set_cmd_register(NFC_CMD_LIST *cmd);
//extern __s32 _read_in_page_mode(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode);
//extern __s32 _read_in_page_mode_first(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode);
//extern __s32 _read_in_page_mode_wait(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode);
//extern __s32 _read_in_normal_mode(NFC_CMD_LIST  *rcmd, __u8 *mainbuf, __u8 *sparebuf,__u8 dma_wait_mode);
extern void nfc_repeat_mode_enable(void);
extern void nfc_repeat_mode_disable(void);

//extern __u8 read_retry_reg_adr[READ_RETRY_MAX_REG_NUM];
//extern __u8 read_retry_default_val[2][MAX_CHIP_SELECT_CNT][READ_RETRY_MAX_REG_NUM];
//extern __s16 read_retry_val[READ_RETRY_MAX_CYCLE][READ_RETRY_MAX_REG_NUM];
//extern __u8 hynix_read_retry_otp_value[2][MAX_CHIP_SELECT_CNT][8][8];
extern __u8 read_retry_mode;
extern __u8 read_retry_cycle;
extern __u8 read_retry_reg_num;

extern __s32 _vender_get_param(__u8 *para, __u8 *addr, __u32 count);
extern __s32 _vender_set_param(__u8 *para, __u8 *addr, __u32 count);
//extern __s32 _vender_pre_condition(void);
//extern __s32 _vender_get_param_otp_hynix(__u8 *para, __u8 *addr, __u32 count);

__u8 lsb_mode_reg_adr[LSB_MODE_MAX_REG_NUM] = {0};
__u8 lsb_mode_default_val[LSB_MODE_MAX_REG_NUM] = {0};
__u8 lsb_mode_val[LSB_MODE_MAX_REG_NUM] = {0};
__u8 lsb_mode_reg_num = 0;

#if 0
static void dumphex32(char* name, char* base, int len)
{
	__u32 i;

	PHY_ERR("dump %s registers:", name);
	for (i=0; i<len*4; i+=4) {
		if (!(i&0xf))
			PHY_ERR("\n0x%p : ", base + i);
		PHY_ERR("0x%08x ", *((volatile unsigned int *)(base + i)));
	}
	PHY_ERR("\n");
}
#endif

/*after send write or erase command, must wait rb from ready to busy, then can send status command
	because nfc not do this, so software delay by xr, 2009-3-25*/

void _wait_twb(void)
{
/*
	__u32 timeout = 800;

	while ( (timeout--) && !(NFC_READ_REG(NFC_REG_ST) & NFC_CMD_FIFO_STATUS));
*/
}

__s32 NFC_GetUniqueId(NFC_CMD_LIST  *idcmd ,__u8 *idbuf)
{
	__u32 i;
	__s32 ret;

	_enter_nand_critical();
    nfc_repeat_mode_enable();

	ret = nfc_set_cmd_register(idcmd);
	if (ret){
		_exit_nand_critical();
		return ret;
	}

	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();

	/*get 5 bytes id value*/
	for (i = 0; i < 32; i++){
		*(idbuf + i) = NDFC_READ_RAM0_B(i);
	}

    nfc_repeat_mode_disable();
	_exit_nand_critical();
	return ret;

}

/*******************************************************************************
*								NFC_Write
*
* Description 	: write one page data into flash in single plane mode.
* Arguments	: *wcmd	-- the write command sequence list head¡£
*			  *mainbuf	-- point to data buffer address, 	it must be four bytes align.
*                     *sparebuf	-- point to spare buffer address.
*                     dma_wait_mode	-- how to deal when dma start, 0 = wait till dma finish,
							    1 = dma interrupt was set and now sleep till interrupt occurs.
*			  rb_wait_mode -- 0 = do not care rb, 1 = set rb interrupt and do not wait rb ready.
*			  page_mode  -- 0 = common command, 1 = page command.
* Returns		: 0 = success.
			  -1 = fail.
* Notes		: the unit must be page, so if  page_mode is not 1, return fail,the function exits without checking status,
			  if the commands do not fetch data,ecc is not neccesary.
********************************************************************************/

__s32 NFC_Write( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode,
				    __u8 page_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	__u32 program_cmd,random_program_cmd;
	NFC_CMD_LIST *cur_cmd,*program_addr_cmd;
	__u32 blk_cnt, blk_mask;

	if (page_mode == 0){
		return -1;
	}

	ret = 0;
	_enter_nand_critical();

	/*write in page_mode*/
	program_addr_cmd = wcmd;
	cur_cmd = wcmd;
	cur_cmd = cur_cmd->next;
	random_program_cmd = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	program_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	_dma_config_start(1, (__u32)mainbuf, pagesize);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	/*set NFC_REG_CNT*/
    NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (program_cmd & 0xff);
	cfg |= ((random_program_cmd & 0xff) << 8);
	NDFC_WRITE_REG_WCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NDFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(pagesize/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = pagesize/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_first: wrong ndfc version, %d\n", NdfcVersion);

	/*set user data*/
	for (i = 0; i < pagesize/1024;  i++){
		NDFC_WRITE_REG_USER_DATA((i), *((__u32 *)sparebuf + i) );
	}

	/*set addr*/
	_set_addr(program_addr_cmd->addr,program_addr_cmd->addr_cycle);

	/////////////////////////////////////////////////////

	//////////////////////////////////////////////////////

	/*set NFC_REG_CMD*/
	cfg  = 0;
	 /*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= program_addr_cmd->value;
	cfg |= ( (program_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_DATA_SWAP_METHOD);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_DATA_SWAP_METHOD | NFC_WAIT_FLAG);
	cfg |= ((__u32)0x2 << 30);//page command
	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NDFC_WRITE_REG_CMD(cfg);
#if 1
    NAND_WaitDmaFinish();
    ret = _wait_dma_end(1, (__u32)mainbuf, pagesize);
	if (ret)
		return ret;

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*disable ecc*/
	_disable_ecc();

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
#endif

	return ret;
}

__s32 NFC_Write_First( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode,
				    __u8 page_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	__u32 program_cmd,random_program_cmd;
	NFC_CMD_LIST *cur_cmd,*program_addr_cmd;
	__u32 blk_cnt, blk_mask;

	if (page_mode == 0){
		return -1;
	}

	ret = 0;
	_enter_nand_critical();

	/*write in page_mode*/
	program_addr_cmd = wcmd;
	cur_cmd = wcmd;
	cur_cmd = cur_cmd->next;
	random_program_cmd = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	program_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	_dma_config_start(1, (__u32)mainbuf, pagesize);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	/*set NFC_REG_CNT*/
    NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (program_cmd & 0xff);
	cfg |= ((random_program_cmd & 0xff) << 8);
	NDFC_WRITE_REG_WCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NDFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(pagesize/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = pagesize/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_first: wrong ndfc version, %d\n", NdfcVersion);

	/*set user data*/
	for (i = 0; i < pagesize/1024;  i++){
		NDFC_WRITE_REG_USER_DATA((i), *((__u32 *)sparebuf + i) );
	}

	/*set addr*/
	_set_addr(program_addr_cmd->addr,program_addr_cmd->addr_cycle);

	/////////////////////////////////////////////////////

	//////////////////////////////////////////////////////

	/*set NFC_REG_CMD*/
	cfg  = 0;
	 /*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= program_addr_cmd->value;
	cfg |= ( (program_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_DATA_SWAP_METHOD);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_DATA_SWAP_METHOD | NFC_WAIT_FLAG);
	cfg |= ((__u32)0x2 << 30);//page command
	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NDFC_WRITE_REG_CMD(cfg);
#if 0
    NAND_WaitDmaFinish();
    ret = _wait_dma_end(1, (__u32)mainbuf, pagesize);
	if (ret)
		return ret;

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*disable ecc*/
	_disable_ecc();

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
#endif

	return ret;
}

__s32 NFC_Write_Wait( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode,
				    __u8 page_mode)
{
	__s32 ret;

	NAND_WaitDmaFinish();
    ret = _wait_dma_end(1, (__u32)mainbuf, pagesize);
	if (ret)
		return ret;

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*disable ecc*/
	_disable_ecc();

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
	return ret;
}


__s32 NFC_Write_Seq( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode,
				    __u8 page_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	__u32 page_size_temp, ecc_mode_temp,page_size_set,ecc_set;
	__u32 program_cmd,random_program_cmd;
	NFC_CMD_LIST *cur_cmd,*program_addr_cmd;
	__u32 blk_cnt, blk_mask;

	if (page_mode == 0){
		return -1;
	}

	ret = 0;
	_enter_nand_critical();

	/*write in page_mode*/
	program_addr_cmd = wcmd;
	cur_cmd = wcmd;
	cur_cmd = cur_cmd->next;
	random_program_cmd = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	program_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//set pagesize to 8K for burn boot0 lsb mode
    page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (0x3<<8));
	page_size_set = 8192;  //fix pagesize 8k to burn boot0.bin for hynix 2x and 2y nm flash ///??????

	_dma_config_start(1, (__u32)mainbuf, page_size_set);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	/*set NFC_REG_CNT*/
    NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (program_cmd & 0xff);
	cfg |= ((random_program_cmd & 0xff) << 8);
	NDFC_WRITE_REG_WCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NDFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(page_size_set/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = page_size_set/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_first: wrong ndfc version, %d\n", NdfcVersion);

	/*set user data*/
	for (i = 0; i < page_size_set/1024;  i++){
		NDFC_WRITE_REG_USER_DATA((i), *((__u32 *)sparebuf + i) );
	}

	/*set addr*/
	_set_addr(program_addr_cmd->addr,program_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	 /*set sequence mode*/
	cfg |= 0x1<<25;
	cfg |= program_addr_cmd->value;
	cfg |= ( (program_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command


	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to specified ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
	if(ecc_mode_temp>=4)  //change for hynix 2y nm flash
		ecc_set = 0x4;
	else//change for hynix 2x nm flash
		ecc_set = 0x1;
	NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_set<<12) ));

	NDFC_WRITE_REG_CMD(cfg);

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(1, (__u32)mainbuf, page_size_set);
	if (ret)
		return ret;

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*disable ecc*/
	_disable_ecc();

	/*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

	/*set pagesize to original value*/
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
	return ret;
}

__s32 NFC_Write_Seq_16K( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode,
				    __u8 page_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	__u32 page_size_temp, ecc_mode_temp,page_size_set,ecc_set;
	__u32 program_cmd,random_program_cmd;
	NFC_CMD_LIST *cur_cmd,*program_addr_cmd;
	__u32 blk_cnt, blk_mask;

	if (page_mode == 0){
		return -1;
	}

	ret = 0;
	_enter_nand_critical();

	/*write in page_mode*/
	program_addr_cmd = wcmd;
	cur_cmd = wcmd;
	cur_cmd = cur_cmd->next;
	random_program_cmd = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	program_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//set pagesize to 16K for burn boot0 hynix16nm lsb mode
    page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (0x4<<8));
	page_size_set = 16384;  //fix pagesize 16k to burn boot0.bin for hynix 16 nm flash ///??????

	_dma_config_start(1, (__u32)mainbuf, page_size_set);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	/*set NFC_REG_CNT*/
    NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (program_cmd & 0xff);
	cfg |= ((random_program_cmd & 0xff) << 8);
	NDFC_WRITE_REG_WCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NDFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(page_size_set/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = page_size_set/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_first: wrong ndfc version, %d\n", NdfcVersion);

	/*set user data*/
	for (i = 0; i < page_size_set/1024;  i++){
		NDFC_WRITE_REG_USER_DATA((i), *((__u32 *)sparebuf + i) );
	}

	/*set addr*/
	_set_addr(program_addr_cmd->addr,program_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	 /*set sequence mode*/
	cfg |= 0x1<<25;
	cfg |= program_addr_cmd->value;
	cfg |= ( (program_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command


	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to specified ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
	ecc_set = 0x6;
	NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_set<<12) ));

	NDFC_WRITE_REG_CMD(cfg);

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(1, (__u32)mainbuf, page_size_set);
	if (ret)
		return ret;

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*disable ecc*/
	_disable_ecc();

	/*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

	/*set pagesize to original value*/
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
	return ret;
}

__s32 NFC_Write_0xFF( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode,
				    __u8 page_mode)
{
	__s32 ret;
	__u32 i,j;
	__u32 cfg;
	__u32 program_cmd,random_program_cmd;
	NFC_CMD_LIST *cur_cmd,*program_addr_cmd;
	__u32 col_addr;
	__u8 addr_buf_col[2]={0};


	ret = 0;
	_enter_nand_critical();

	/*write in page_mode*/
	program_addr_cmd = wcmd;
	cur_cmd = wcmd;
	cur_cmd = cur_cmd->next;
	random_program_cmd = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	program_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	/*disable ecc*/
	_disable_ecc();

	col_addr = 0;

	for(i=0;i<18;i++)
	{
		if(i==0)
		{
			_dma_config_start(1, (__u32)mainbuf, 1024);

			/*wait cmd fifo free*/
			ret = _wait_cmdfifo_free();
			if (ret){
				_exit_nand_critical();
				return ret;
			}
			/*set NFC_REG_CNT*/
		    NDFC_WRITE_REG_CNT(1024);

			/*set addr*/
			_set_addr(program_addr_cmd->addr,program_addr_cmd->addr_cycle);

			/*set NFC_REG_CMD*/
			cfg  = 0;

			cfg |= program_addr_cmd->value;
			cfg |= ( (program_addr_cmd->addr_cycle - 1) << 16);
			cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_DATA_SWAP_METHOD);


			NDFC_WRITE_REG_CMD(cfg);

		    NAND_WaitDmaFinish();
		    ret = _wait_dma_end(1, (__u32)mainbuf, 1024);
			if (ret)
				return ret;

			_wait_twb();
			_wait_cmdfifo_free();
			_wait_cmd_finish();
		}
		else if(i==17)
		{
			_dma_config_start(1, (__u32)mainbuf+i*1024, 640);

			/*wait cmd fifo free*/
			ret = _wait_cmdfifo_free();
			if (ret){
				_exit_nand_critical();
				return ret;
			}
			/*set NFC_REG_CNT*/
		    NDFC_WRITE_REG_CNT(640);

			for(j=0; j< 2; j++)
			{
				addr_buf_col[j] = col_addr & 0xff;
				col_addr >>= 8;
			}

			/*set addr*/
			_set_addr(&addr_buf_col[0],2);

			/*set NFC_REG_CMD*/
			cfg  = 0;

			cfg |= random_program_cmd;
			cfg |= ( 1 << 16);

			cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_DATA_SWAP_METHOD);


			NDFC_WRITE_REG_CMD(cfg);

		    NAND_WaitDmaFinish();
		    ret = _wait_dma_end(1, (__u32)mainbuf+i*1024, 640);
			if (ret)
				return ret;

			_wait_twb();
			_wait_cmdfifo_free();
			_wait_cmd_finish();
		}
		else
		{
			_dma_config_start(1, (__u32)mainbuf+i*1024, 1024);

			/*wait cmd fifo free*/
			ret = _wait_cmdfifo_free();
			if (ret){
				_exit_nand_critical();
				return ret;
			}
			/*set NFC_REG_CNT*/
		    NDFC_WRITE_REG_CNT(1024);


			for(j=0; j< 2; j++)
			{
				addr_buf_col[j] = col_addr & 0xff;
				col_addr >>= 8;
			}

			/*set addr*/
			_set_addr(&addr_buf_col[0],2);

			/*set NFC_REG_CMD*/
			cfg  = 0;

			cfg |= random_program_cmd;
			cfg |= ( 1 << 16);

			cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_DATA_SWAP_METHOD);


			NDFC_WRITE_REG_CMD(cfg);

		    NAND_WaitDmaFinish();
		    ret = _wait_dma_end(1, (__u32)mainbuf+i*1024, 1024);
			if (ret)
				return ret;

			_wait_twb();
			_wait_cmdfifo_free();
			_wait_cmd_finish();
		}
		col_addr += 1024;
	}

	cfg  = 0;
	cfg |= program_cmd;
	cfg |= (NDFC_SEND_CMD1 | NDFC_WAIT_FLAG);
	NDFC_WRITE_REG_CMD(cfg);

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
	return ret;
}


__s32 NFC_Write_CFG( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode, struct boot_ndfc_cfg *ndfc_cfg)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	__u32 ecc_mode_temp,page_size_set,ecc_set;
	__u32 program_cmd,random_program_cmd;
	NFC_CMD_LIST *cur_cmd,*program_addr_cmd;
	__u32 blk_cnt, blk_mask;

	//PHY_ERR("write: ecc %d, page size %d, seq %d\n", ndfc_cfg->ecc_mode, ndfc_cfg->page_size_kb, ndfc_cfg->sequence_mode);

	if (page_mode == 0){
		PHY_ERR("page_mode == 0, return -1\n");
		return -1;
	}

	ret = 0;
	_enter_nand_critical();

	/*write in page_mode*/
	program_addr_cmd = wcmd;
	cur_cmd = wcmd;
	cur_cmd = cur_cmd->next;
	random_program_cmd = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	program_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

    /*page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	if(ndfc_cfg->page_size_kb  ==  1) 	    page_size_mode = 0x0;   //1K
	else if (ndfc_cfg->page_size_kb  ==  2) page_size_mode = 0x1;   //2K
	else if (ndfc_cfg->page_size_kb  ==  4) page_size_mode = 0x2;   //4K
	else if (ndfc_cfg->page_size_kb  ==  8) page_size_mode = 0x3;   //8K
	else if (ndfc_cfg->page_size_kb  == 16) page_size_mode = 0x4;   //16K
	else if (ndfc_cfg->page_size_kb  == 32) page_size_mode = 0x5;   //32K
	else
		return -1;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (page_size_mode<<8));*/
	page_size_set = ndfc_cfg->page_size_kb * 1024;

	_dma_config_start(1, (__u32)mainbuf, page_size_set);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	/*set NFC_REG_CNT*/
    NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (program_cmd & 0xff);
	cfg |= ((random_program_cmd & 0xff) << 8);
	NDFC_WRITE_REG_WCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NDFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(page_size_set/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = page_size_set/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("NFC_Write_CFG: wrong ndfc version, %d\n", NdfcVersion);

	/*set user data*/
	for (i = 0; i < page_size_set/1024;  i++){
		NDFC_WRITE_REG_USER_DATA((i), *((__u32 *)sparebuf + i) );
	}

	/*set addr*/
	_set_addr(program_addr_cmd->addr,program_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	 /*set sequence mode*/
	if (ndfc_cfg->sequence_mode)
		cfg |= 0x1<<25;
	cfg |= program_addr_cmd->value;
	cfg |= ( (program_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command


	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to specified ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
	ecc_set = ndfc_cfg->ecc_mode;
	NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_set<<12) ));

	NDFC_WRITE_REG_CMD(cfg);
	//PHY_ERR("write: 0x%x 0x%x 0x%x\n\n", NDFC_READ_REG_CTL(), NDFC_READ_REG_ECC_CTL(), NDFC_READ_REG_CMD());

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(1, (__u32)mainbuf, page_size_set);
	if (ret)
		return ret;

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*disable ecc*/
	_disable_ecc();

	/*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

	/*set pagesize to original value*/
	//NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
	return ret;
}

__s32 NFC_Write_1K( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode,
				    __u8 page_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	__u32 page_size_temp, ecc_mode_temp;
	__u32 program_cmd,random_program_cmd;
	NFC_CMD_LIST *cur_cmd,*program_addr_cmd;

	if (page_mode == 0){
		return -1;
	}

	ret = 0;
	_enter_nand_critical();

	/*write in page_mode*/
	program_addr_cmd = wcmd;
	cur_cmd = wcmd;
	cur_cmd = cur_cmd->next;
	random_program_cmd = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	program_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//set pagesize to 1K
    page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (0x0<<8));

	_dma_config_start(1, (__u32)mainbuf, 1024);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	/*set NFC_REG_CNT*/
    NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (program_cmd & 0xff);
	cfg |= ((random_program_cmd & 0xff) << 8);
	NDFC_WRITE_REG_WCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(1024/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2){
		NDFC_WRITE_REG_BLOCK_MASK(0x1U);
	} else
		PHY_ERR("NFC_Write_1K: wrong ndfc veresion, %d\n", NdfcVersion);

	/*set user data*/
	for (i = 0; i < 1024/1024;  i++){
		NDFC_WRITE_REG_USER_DATA((i), *((__u32 *)sparebuf + i) );
	}

	/*set addr*/
	_set_addr(program_addr_cmd->addr,program_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	 /*set sequence mode*/
	cfg |= 0x1<<25;
	cfg |= program_addr_cmd->value;
	cfg |= ( (program_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_ACCESS_DIR | NFC_DATA_TRANS | NFC_SEND_CMD | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_ACCESS_DIR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command
	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to 64-bit/72-bit ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
    if (NdfcVersion == NDFC_VERSION_V1) {
		NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(0x8<<12) ));
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(0x9<<12) ));
	} else
		PHY_ERR("NFC_Write_1K: wrong ndfc veresion, %d\n", NdfcVersion);
	NDFC_WRITE_REG_CMD(cfg);

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(1, (__u32)mainbuf, 1024);
	if (ret)
		return ret;

	_wait_twb();
	_wait_cmdfifo_free();
	_wait_cmd_finish();

	/*disable ecc*/
	_disable_ecc();

	/*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

	/*set pagesize to original value*/
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();
	return ret;
}

/*******************************************************************************
*								NFC_Erase
*
* Description 	: erase one block in signle plane mode or multi plane mode.
* Arguments	: *ecmd	-- the erase command sequence list head
*			  rb_wait_mode  -- 0 = do not care rb, 1 = set rb interrupt and do not wait rb ready.
* Returns		: 0 = success.
			  -1 = fail.
* Notes		: the unit must be page, so if  page_mode is not 1, return fail,the function exits without checking status,
			  if the commands do not fetch data,ecc is not neccesary.
********************************************************************************/
__s32 NFC_Erase(NFC_CMD_LIST  *ecmd, __u8 rb_wait_mode)
{
	__s32 ret;

	_enter_nand_critical();

	ret = nfc_set_cmd_register(ecmd);
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	_wait_twb();
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();

	_exit_nand_critical();

	return ret;
}

/*******************************************************************************
*								NFC_CopyBackRead
*
* Description 	: copyback read one page data inside flash in single plane mode or multi plane mode
* Arguments	: *crcmd	-- the copyback read command sequence list head.
* Returns		: 0 = success.
			  -1 = fail.
* Notes		: the unit must be page.
********************************************************************************/
__s32 NFC_CopyBackRead(NFC_CMD_LIST  *crcmd)
{
	__s32 ret;

	_enter_nand_critical();
	ret = nfc_set_cmd_register(crcmd);
	if (ret){
		_exit_nand_critical();
		return ret;
	}

	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();

	_exit_nand_critical();

	return ret;
}
//#pragma arm section code="NFC_CopyBackWrite"
/*******************************************************************************
*								NFC_CopyBackWrite
*
* Description 	: copyback write one page data inside flash in single plane mode or multi plane mode
* Arguments	: *cwcmd	-- the copyback read command sequence list head.
 			  rb_wait_mode  -- 0 = do not care rb, 1 = set rb interrupt and do not wait rb ready.
* Returns		: 0 = success.
			  -1 = fail.
* Notes		: the unit must be page.
********************************************************************************/
__s32 NFC_CopyBackWrite(NFC_CMD_LIST  *cwcmd, __u8 rb_wait_mode)
{
	__s32 ret;

	_enter_nand_critical();

	ret = nfc_set_cmd_register(cwcmd);
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	_wait_twb();
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();

	_exit_nand_critical();

	return ret;
}

__s32 _read_in_page_mode_seq(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	__u32 page_size_temp, ecc_mode_temp, page_size_set, ecc_set;
	__u32 blk_cnt, blk_mask;

	ret = 0;
	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	cur_cmd = cur_cmd->next;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	read_data_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//set pagesize to 8K
	page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (0x3<<8));
	page_size_set = 8192;

	_dma_config_start(0, (__u32)mainbuf, page_size_set);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(page_size_set/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = page_size_set/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_1K: wrong ndfc version, %d\n", NdfcVersion);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command


	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to specified ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
	if(ecc_mode_temp>=4)  //change for hynix 2y nm flash
		ecc_set = 0x4;
	else//change for hynix 2x nm flash
		ecc_set = 0x1;
	NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_set<<12) ));

	NDFC_WRITE_REG_CMD(cfg);

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, page_size_set);
	if (ret)
		return ret;

	/*wait cmd fifo free and cmd finish*/
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if (ret){
		_disable_ecc();
		return ret;
	}
	/*get user data*/
	for (i = 0; i < page_size_set/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(page_size_set/1024);
	_disable_ecc();

    /*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

    /*set pagesize to original value*/
    NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	return ret;
}

__s32 _read_in_page_mode_seq_16k(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	__u32 page_size_temp, ecc_mode_temp, page_size_set, ecc_set;
	__u32 blk_cnt, blk_mask;

	ret = 0;
	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	cur_cmd = cur_cmd->next;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	read_data_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//set pagesize to 8K
	page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (0x4<<8));
	page_size_set = 16384;

	_dma_config_start(0, (__u32)mainbuf, page_size_set);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(page_size_set/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = page_size_set/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_1K: wrong ndfc version, %d\n", NdfcVersion);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command


	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to specified ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;

	ecc_set = 0x6;
	NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_set<<12) ));

	NDFC_WRITE_REG_CMD(cfg);

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, page_size_set);
	if (ret)
		return ret;

	/*wait cmd fifo free and cmd finish*/
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if (ret){
		_disable_ecc();
		return ret;
	}
	/*get user data*/
	for (i = 0; i < page_size_set/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(page_size_set/1024);
	_disable_ecc();

    /*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

    /*set pagesize to original value*/
    NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	return ret;
}


__s32 _read_in_page_mode_cfg(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode, struct boot_ndfc_cfg *ndfc_cfg)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	__u32 ecc_mode_temp, page_size_set, ecc_set;
	__u32 blk_cnt, blk_mask;

	//PHY_ERR("read: ecc %d, page size %d, seq %d\n", ndfc_cfg->ecc_mode, ndfc_cfg->page_size_kb, ndfc_cfg->sequence_mode);

	ret = 0;
	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	cur_cmd = cur_cmd->next;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	read_data_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	/*page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	if(ndfc_cfg->page_size_kb  ==  1) 	    page_size_mode = 0x0;   //1K
	else if (ndfc_cfg->page_size_kb  ==  2) page_size_mode = 0x1;   //2K
	else if (ndfc_cfg->page_size_kb  ==  4) page_size_mode = 0x2;   //4K
	else if (ndfc_cfg->page_size_kb  ==  8) page_size_mode = 0x3;   //8K
	else if (ndfc_cfg->page_size_kb  == 16) page_size_mode = 0x4;   //16K
	else if (ndfc_cfg->page_size_kb  == 32) page_size_mode = 0x5;   //32K
	else
		return -1;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (page_size_mode<<8));*/
	page_size_set = ndfc_cfg->page_size_kb * 1024;

	_dma_config_start(0, (__u32)mainbuf, page_size_set);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(page_size_set/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = page_size_set/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_1K: wrong ndfc version, %d\n", NdfcVersion);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	if (ndfc_cfg->sequence_mode)
		cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command


	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to specified ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
    ecc_set = ndfc_cfg->ecc_mode;
	NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_set<<12) ));

	NDFC_WRITE_REG_CMD(cfg);
	//PHY_ERR("read: 0x%x 0x%x 0x%x\n\n", NDFC_READ_REG_CTL(), NDFC_READ_REG_ECC_CTL(), NDFC_READ_REG_CMD());

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, page_size_set);
	if (ret)
		return ret;

	/*wait cmd fifo free and cmd finish*/
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if (ret){
		_disable_ecc();
		return ret;
	}
	/*get user data*/
	for (i = 0; i < page_size_set/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(page_size_set/1024);
	_disable_ecc();

    /*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

    /*set pagesize to original value*/
    //NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	return ret;
}


__s32 _read_in_page_mode_1K(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	__u32 page_size_temp, ecc_mode_temp;

	ret = 0;
	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	cur_cmd = cur_cmd->next;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	read_data_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//set pagesize to 1K
	page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (0x0<<8));

	_dma_config_start(0, (__u32)mainbuf, 1024);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(1024/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		NDFC_WRITE_REG_BLOCK_MASK(0x1U);
	} else
		PHY_ERR("_read_in_page_mode_1K: wrong ndfc version, %d\n", NdfcVersion);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command

	if (1024/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to 64-bit/72-bit ecc*/
    ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
	if (NdfcVersion == NDFC_VERSION_V1) {
		NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(0x8<<12) ));
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(0x9<<12) ));
	} else
		PHY_ERR("_read_in_page_mode_1K: wrong ndfc version, %d\n", NdfcVersion);
	NDFC_WRITE_REG_CMD(cfg);

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, 1024);
	if (ret)
		return ret;

	/*wait cmd fifo free and cmd finish*/
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if (ret){
		_disable_ecc();
		return ret;
	}
	/*get user data*/
	for (i = 0; i < 1024/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(pagesize/1024);
	_disable_ecc();

	/*set ecc to original value*/
	NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

    /*set pagesize to original value*/
    NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	return ret;
}

__s32 _read_in_page_mode_1st_1K_normal_ecc(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	//__u32 page_size_temp, ecc_mode_temp;
	__u32 data_size = 4096;

	if (pagesize >= 8192)
		data_size = 8192;
	else
		data_size = pagesize;

	ret = 0;
	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	cur_cmd = cur_cmd->next;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	read_data_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//set pagesize to 1K
	//page_size_temp = (NDFC_READ_REG_CTL() & 0xf00)>>8;
	//NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE))| (0x0<<8));

	_dma_config_start(0, (__u32)mainbuf, data_size); //1024-->4096

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(data_size/1024); //1024-->4096
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		__u32 blk_cnt = data_size/1024;
		__u32 blk_mask = 0;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK( blk_mask ); //0x1U-->0xF
	} else
		PHY_ERR("_read_in_page_mode_1st_1K_normal_ecc: wrong ndfc version, %d\n", NdfcVersion);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command

	//if (1024/1024 == 1)
	//	cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);

	/*set ecc to 64-bit ecc*/
    //ecc_mode_temp = (NDFC_READ_REG_ECC_CTL() & 0xf000)>>12;
	//NDFC_WRITE_REG_ECC_CTL(((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(0x8<<12) ));

	NDFC_WRITE_REG_CMD(cfg);

    NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, data_size); //1024-->4096
	if (ret)
		return ret;

	/*wait cmd fifo free and cmd finish*/
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if (ret){
		_disable_ecc();
		return ret;
	}
	/*get user data*/
	for (i = 0; i < data_size/1024;  i++){  //
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(data_size/1024); //ret = _check_ecc(pagesize/1024);
	_disable_ecc();

	/*set ecc to original value*/
	//NDFC_WRITE_REG_ECC_CTL((NDFC_READ_REG_ECC_CTL() & (~NDFC_ECC_MODE))|(ecc_mode_temp<<12));

    /*set pagesize to original value*/
    //NDFC_WRITE_REG_CTL(((NDFC_READ_REG_CTL()) & (~NDFC_PAGE_SIZE)) | (page_size_temp<<8));

	return ret;
}

__s32 _read_in_page_mode_spare(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;

	ret = 0;
	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	cur_cmd = cur_cmd->next;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	read_data_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//_dma_config_start(0, (__u32)mainbuf, 4096);  //2048 --> 4096

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(4096/1024); //2048/1024 --> 4096/1024
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		NDFC_WRITE_REG_BLOCK_MASK(0xF);
	} else
		PHY_ERR("_read_in_page_mode_spare: wrong ndfc version, %d\n", NdfcVersion);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*Gavin-20130619, clear user data*/
	for (i=0; i<16; i++)
		NDFC_WRITE_REG_USER_DATA((i), 0x99999999);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG);
	cfg |= ((__u32)0x2 << 30);//page command

	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NDFC_WRITE_REG_CMD(cfg);

	//NAND_WaitDmaFinish();//
	//ret = _wait_dma_end(0, (__u32)mainbuf, 2048); //2048 --> 4096
	//if (ret)
	//	return ret;

	/*wait cmd fifo free and cmd finish*/
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if (ret){
		_disable_ecc();
		return ret;
	}
	/*get user data*/
	for (i = 0; i < 4096/1024;  i++){ //2048/1024 --> 4096/1024
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/////////////////////////////
	/////////////////////////////

	/*ecc check and disable ecc*/
	ret = _check_ecc(4096/1024); //2048/1024 --> 4096/1024
	_disable_ecc();

	return ret;
}

__s32 _read_in_page_mode_spare_first(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	__s32 i;

	ret = 0;
	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	cur_cmd = cur_cmd->next;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	read_data_cmd = cur_cmd->value;

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) | NDFC_RAM_METHOD);

	//_dma_config_start(0, (__u32)mainbuf, 4096); //2048 --> 4096

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	if (NdfcVersion == NDFC_VERSION_V1) {
		/*set NFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(4096/1024); //2048/1024 --> 4096/1024
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		NDFC_WRITE_REG_BLOCK_MASK(0xF);
	} else
		PHY_ERR("_read_in_page_mode_spare_first: wrong ndfc versin, %d\n", NdfcVersion);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*Gavin-20130619, clear user data*/
	for (i=0; i<16; i++)
		NDFC_WRITE_REG_USER_DATA((i), 0x99999999);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	//cfg |= (NFC_SEND_ADR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG );
	cfg |= ((__u32)0x2 << 30);//page command

	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NDFC_WRITE_REG_CMD(cfg);

	return ret;
}

__s32 _read_in_page_mode_spare_wait(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 i,ret;
	//__u32 reg_val, page, err, cnt;
	//__u8 *pspare;

	ret = 0;

	//NAND_WaitDmaFinish();//
	//ret = _wait_dma_end(0, (__u32)mainbuf, 4096); //2048 --> 4096
	//if (ret)
	//	return ret;

	/*wait cmd fifo free and cmd finish*/
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if (ret){
		_disable_ecc();
		return ret;
	}
	/*get user data*/
	for (i = 0; i < 4096/1024;  i++) { //2048/1024 --> 4096/1024
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/////////////////////////////
	/////////////////////////////

	/*ecc check and disable ecc*/
	ret = _check_ecc(4096/1024); //2048/1024 --> 4096/1024
	_disable_ecc();

	return ret;
}


__s32 NFC_Read_Seq(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{
	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_seq(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();

	return ret;
}

__s32 NFC_Read_Seq_16K(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{
	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_seq_16k(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();

	return ret;
}


__s32 NFC_Read_CFG(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode, struct boot_ndfc_cfg *ndfc_cfg)
{
	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_cfg(rcmd, mainbuf,sparebuf, dma_wait_mode, ndfc_cfg);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();

	return ret;
}

__s32 NFC_Read_1K(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{
	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_1K(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();

	return ret;
}

__s32 NFC_Read_1st_1K_Normal_Ecc(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{
	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_1st_1K_normal_ecc(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();

	return ret;
}

__s32 NFC_Read_Spare(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{
	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_spare(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();


	return ret;
}
__s32 NFC_Read_Spare_First(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{
	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_spare_first(rcmd, mainbuf,sparebuf, dma_wait_mode);

	return ret;
}

__s32 NFC_Read_Spare_Wait(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{
	__s32 ret ;

	ret = _read_in_page_mode_spare_wait(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL((NDFC_READ_REG_CTL()) & (~NDFC_RAM_METHOD));

	_exit_nand_critical();

	return ret;
}

__s32 _read_in_page_mode_sectors(void)
{
	return 0;
}


__s32 NFC_LSBInit(__u32 read_retry_type)
{
	return 0;
}

__s32 LSB_GetDefaultParam(__u32 chip,__u8* default_value, __u32 read_retry_type)
{

	return 0;
}

__s32 LSB_SetDefaultParam(__u32 chip,__u8* default_value, __u32 read_retry_type)
{

	return 0;
}

__s32 NFC_LSBEnable(__u32 chip, __u32 read_retry_type)
{

    return 0;
}

__s32 NFC_LSBDisable(__u32 chip, __u32 read_retry_type)
{

    return 0;
}

__s32 NFC_LSBExit(__u32 read_retry_type)
{

	return 0;
}

