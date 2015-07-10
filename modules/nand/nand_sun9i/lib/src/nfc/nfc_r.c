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
//#include "nand_physic_interface.h"

__u32 NandIOBase[2] = {0, 0};
__u32 NandIndex = 0;
__u32 nand_reg_address = 0;
__u32 nand_board_version = 0;
__u32 pagesize = 0;

__u32 Retry_value_ok_flag = 0;


volatile __u32 irq_value = 0;


__u8 read_retry_reg_adr[READ_RETRY_MAX_REG_NUM] = {0};
__u8 read_retry_default_val[2][MAX_CHIP_SELECT_CNT][READ_RETRY_MAX_REG_NUM] = {0};
__s16 read_retry_val[READ_RETRY_MAX_CYCLE][READ_RETRY_MAX_REG_NUM] = {0};
__u8 hynix_read_retry_otp_value[2][MAX_CHIP_SELECT_CNT][8][8] = {0};
__u8 read_retry_mode = {0};
__u8 read_retry_cycle = {0};
__u8 read_retry_reg_num = {0};

__u8 hynix16nm_read_retry_otp_value[2][MAX_CHIP_SELECT_CNT][8][4] = {0};

const __s16 para0[6][4] = {	0
					};
const __s16 para1[6][4] = {	0
					};
const __s16 para0x10[5] = {0};
const __s16 para0x11[7][5] = {	0
					};
const __s16 para0x20[15][4] ={0
	                    };
const __s16 param0x30low[16][2] ={0
									};
const __s16 param0x30high[20][2] ={0
									};
const __s16 param0x31[9][3] =	   {0
									};
const __s16 param0x32[32][4] ={0
									};
const	__s16 param0x40[10] = {0};
const	__s16 param0x41[12] = {0};

const	__s16 param0x50[7] = {0};
__u32 ddr_param[8] = {0};

/*
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
*/

void NFC_InitDDRParam(__u32 chip, __u32 param)
{
    if(chip<8)
        ddr_param[chip] = param;
}

void nfc_repeat_mode_enable(void)
{
    __u32 reg_val;

	reg_val = NDFC_READ_REG_CTL();
	if(((reg_val>>18)&0x3)>1)   //ddr type
	{
    	reg_val |= 0x1<<20;
    	NDFC_WRITE_REG_CTL(reg_val);
    }
}

void nfc_repeat_mode_disable(void)
{
    __u32 reg_val;

    reg_val = NDFC_READ_REG_CTL();
	if(((reg_val>>18)&0x3)>1)   //ddr type
	{
    	reg_val &= (~(0x1<<20));
    	NDFC_WRITE_REG_CTL(reg_val);
    }
}

/*******************wait nfc********************************************/
__s32 _wait_cmdfifo_free(void)
{
	__s32 timeout = 0xffff;

	while ( (timeout--) && (NDFC_READ_REG_ST() & NDFC_CMD_FIFO_STATUS) );
	if (timeout <= 0)
	{
	    PHY_ERR("nand _wait_cmdfifo_free time out, status:0x%x\n", NDFC_READ_REG_ST());

		NAND_DumpReg();

		return -ERR_TIMEOUT;
    }
	return 0;
}

__s32 _wait_cmd_finish(void)
{
	__s32 timeout = 0xffff;

	while( (timeout--) && !(NDFC_READ_REG_ST()& NDFC_CMD_INT_FLAG) );
	if (timeout <= 0)
	{
	    PHY_ERR("nand _wait_cmd_finish time out, NandIndex %d, status:0x%x\n", (__u32)NandIndex, NDFC_READ_REG_ST());

		NAND_DumpReg();

        return -ERR_TIMEOUT;
   }

	NDFC_WRITE_REG_ST(NDFC_READ_REG_ST() & NDFC_CMD_INT_FLAG);

	return 0;
}

void _show_desc_list_cfg(void)
{

#if 0
	__u32 ind;
	_ndfc_dma_desc_t *pdesc;

	ind = 0;
	//pdesc = (_ndfc_dma_desc_t *)NFC_READ_REG(NFC_REG_DMA_DL_BASE);
	pdesc = &ndfc_dma_desc_cpu[0];

	do {
		PHY_DBG("desc %d - 0x%08x:  cfg: 0x%04x,  size: 0x%04x,  buff: 0x%08x,  next: 0x%x\n",
			ind, (__u32)pdesc, pdesc->cfg, pdesc->bcnt, pdesc->buff, (__u32)(pdesc->next));

		if (pdesc->cfg & NDFC_DESC_LAST_FLAG)
			break;

		ind++;
		if (ind > 4) {
			PHY_ERR("wrong desc list cfg.\n");
			break;
		}
		pdesc = &ndfc_dma_desc_cpu[ind];
	} while(1);
#endif

	return ;
}

__u32 nand_dma_addr[2][5] = {{0}, {0}};
void _dma_config_start(__u8 rw, __u32 buff_addr, __u32 len)
{
	__u32 reg_val;

	if ( NdfcVersion == NDFC_VERSION_V1 ) {

		if (NdfcDmaMode == 1) {
			/*
				MBUS DMA
			*/
			NAND_CleanFlushDCacheRegion(buff_addr, len);

			nand_dma_addr[NandIndex][0] = NAND_DMASingleMap(rw, buff_addr, len);

			//set mbus dma mode
			reg_val = NDFC_READ_REG_CTL();
			reg_val &= (~(0x1<<15));
			NDFC_WRITE_REG_CTL(reg_val);
			NDFC_WRITE_REG_MDMA_ADDR(nand_dma_addr[NandIndex][0]);
			NDFC_WRITE_REG_DMA_CNT(len);
		} else if (NdfcDmaMode == 0) {
			/*
				General DMA
			*/
			NAND_CleanFlushDCacheRegion(buff_addr, len);

			reg_val = NDFC_READ_REG_CTL();
			reg_val |=(0x1 << 15);
			NDFC_WRITE_REG_CTL(reg_val);
			NDFC_WRITE_REG_DMA_CNT(len);

			nand_dma_addr[NandIndex][0] = NAND_DMASingleMap(rw, buff_addr, len);
			//NDFC_WRITE_REG_MDMA_ADDR(nand_dma_addr[NandIndex][0]);
			//PHY_ERR("buff_addr 0x%x  nand_dma_addr[NandIndex][0] 0x%x\n", buff_addr, nand_dma_addr[NandIndex][0]);
			nand_dma_config_start( rw, nand_dma_addr[NandIndex][0], len);
		} else {
			PHY_ERR("_dma_config_start, wrong dma mode, %d\n", NdfcDmaMode);
		}

	} else if ( NdfcVersion == NDFC_VERSION_V2 ) {

		if (NdfcDmaMode == 1) {

			if (buff_addr & 0x3) {
				PHY_ERR("_dma_config_start: buff addr(0x%x) is not 32bit aligned, "
						"and it will be clipped to 0x%x", buff_addr, (buff_addr & 0x3));
			}
			NAND_CleanFlushDCacheRegion(buff_addr, len);
			nand_dma_addr[NandIndex][0] = NAND_DMASingleMap(rw, buff_addr, len);

			reg_val = NDFC_READ_REG_CTL();
			reg_val &= (~(0x1<<15));
			NDFC_WRITE_REG_CTL(reg_val);

			ndfc_dma_desc_cpu[0].bcnt = 0;
			ndfc_dma_desc_cpu[0].bcnt |= NDFC_DESC_BSIZE(len);
			ndfc_dma_desc_cpu[0].buff = nand_dma_addr[NandIndex][0]; //buff_addr;

			ndfc_dma_desc_cpu[0].cfg = 0;
			ndfc_dma_desc_cpu[0].cfg |= NDFC_DESC_FIRST_FLAG;
			ndfc_dma_desc_cpu[0].cfg |= NDFC_DESC_LAST_FLAG;

			ndfc_dma_desc_cpu[0].next = (struct _ndfc_dma_desc_t *)&(ndfc_dma_desc[0]);

			NAND_CleanFlushDCacheRegion((__u32)&(ndfc_dma_desc_cpu[0]), sizeof(ndfc_dma_desc_cpu[0]));
			NDFC_WRITE_REG_DMA_DL_BASE( (__u32)ndfc_dma_desc );

			_show_desc_list_cfg();
		} else {
			PHY_ERR("_dma_config_start, wrong dma mode, %d\n", NdfcDmaMode);
		}
	} else {
		PHY_ERR("_dma_config_start: wrong ndfc version, %d\n", NdfcVersion);
	}

	return ;
}

__s32 _wait_dma_end(__u8 rw, __u32 buff_addr, __u32 len)
{
	__s32 timeout = 0xffffff;
	_ndfc_dma_desc_t *pdesc;

	while ( (timeout--) && (!(NDFC_READ_REG_ST() & NDFC_DMA_INT_FLAG)) );
	if (timeout <= 0)
	{
	    PHY_ERR("nand _wait_dma_end time out, NandIndex: 0x%x, rw: 0x%x, status:0x%x\n", NandIndex, (__u32)rw, NDFC_READ_REG_ST());

	    if ( NdfcVersion == NDFC_VERSION_V1 ) {
	    	PHY_ERR("DMA addr: 0x%x, DMA len: 0x%x\n", NDFC_READ_REG_MDMA_ADDR(), NDFC_READ_REG_DMA_CNT());

			NAND_DumpReg();

	    	return -ERR_TIMEOUT;

	    } else if ( NdfcVersion == NDFC_VERSION_V2 ) {
	    	pdesc = &ndfc_dma_desc_cpu[0];
	    	PHY_ERR("DMA addr: 0x%x, DMA len: 0x%x, Desc CFG: 0x%x\n", pdesc->buff, pdesc->bcnt, pdesc->cfg);

	    	NAND_DumpReg();
	    	_show_desc_list_cfg();

			return -ERR_TIMEOUT;

	    } else {
	    	PHY_ERR("_dma_config_start: wrong ndfc version, %d\n", NdfcVersion);
	    }
    }

	NDFC_WRITE_REG_ST(NDFC_READ_REG_ST() & NDFC_DMA_INT_FLAG);
	NAND_DMASingleUnmap(rw, nand_dma_addr[NandIndex][0], len);

	return 0;
}

void _dma_config_start_v2(__u8 rw, __u32 buf_cnt, __u32 buf_addr[], __u32 buf_size[])
{
	__s32 i;
	__u32 reg_val;

	if ( NdfcVersion == NDFC_VERSION_V2)
	{
		if (buf_cnt > 4)
		{
			PHY_ERR("buf cnt error: %d\n", buf_cnt);
		}

		reg_val = NDFC_READ_REG_CTL();
		reg_val &= (~(0x1<<15));
		NDFC_WRITE_REG_CTL(reg_val);

		for (i=0; i<buf_cnt; i++)
		{
			if (buf_addr[i] & 0x3) {
				PHY_ERR("%s: buff addr(0x%x) is not 32bit aligned, "
						"and it will be clipped to 0x%x ----- while(1)", __func__, buf_addr[i], (buf_addr[i] & 0x3));
				while(1);
			}
			if (buf_size[i]%1024) {
				PHY_ERR("%s: wrong buffer size: 0x%x\n ---- while(1)\n", __func__, buf_size[i]);
				while(1);
			}

			NAND_CleanFlushDCacheRegion(buf_addr[i], buf_size[i]);
			nand_dma_addr[NandIndex][i] = NAND_DMASingleMap(rw, buf_addr[i], buf_size[i]);

			ndfc_dma_desc_cpu[i].bcnt = 0;
			ndfc_dma_desc_cpu[i].bcnt |= NDFC_DESC_BSIZE(buf_size[i]);
			ndfc_dma_desc_cpu[i].buff = nand_dma_addr[NandIndex][i]; //buf_addr[i];
			ndfc_dma_desc_cpu[i].cfg = 0;

			ndfc_dma_desc_cpu[i].next = 0;
			ndfc_dma_desc_cpu[i].next = (struct _ndfc_dma_desc_t *)&(ndfc_dma_desc[i+1]);
		}

		ndfc_dma_desc_cpu[0].cfg = 0;
		ndfc_dma_desc_cpu[buf_cnt-1].cfg = 0;
		ndfc_dma_desc_cpu[0].cfg |= NDFC_DESC_FIRST_FLAG;
		ndfc_dma_desc_cpu[buf_cnt-1].cfg |= NDFC_DESC_LAST_FLAG;

		ndfc_dma_desc_cpu[buf_cnt-1].next = (struct _ndfc_dma_desc_t *)&(ndfc_dma_desc[0]);

		NAND_CleanFlushDCacheRegion((__u32)&(ndfc_dma_desc_cpu[0]), sizeof(ndfc_dma_desc_cpu[0])*buf_cnt);
		NDFC_WRITE_REG_DMA_DL_BASE( (__u32)ndfc_dma_desc );
		_show_desc_list_cfg();
	} else
		PHY_ERR("_dma_config_start_v2: wrong ndfc version, %d\n", NdfcVersion);
}

__s32 _wait_dma_end_v2(__u8 rw, __u32 buf_cnt, __u32 buf_addr[], __u32 buf_size[])
{
	__s32 i, timeout = 0xfffff;
	_ndfc_dma_desc_t *pdesc;

    while ( (timeout--) && (!(NDFC_READ_REG_ST() & NDFC_DMA_INT_FLAG)) );
	if (timeout <= 0)
	{
	    if ( NdfcVersion == NDFC_VERSION_V2 ) {
	    	pdesc = &ndfc_dma_desc_cpu[0];
	    	PHY_ERR("DMA addr: 0x%x, DMA len: 0x%x, Desc CFG: 0x%x\n", pdesc->buff, pdesc->bcnt, pdesc->cfg);

			NAND_DumpReg();

	    	_show_desc_list_cfg();

			return -ERR_TIMEOUT;

	    } else {
	    	PHY_ERR("_dma_config_start: wrong ndfc version, %d\n", NdfcVersion);
	    }
	}


	NDFC_WRITE_REG_ST( NDFC_READ_REG_ST() & NDFC_DMA_INT_FLAG);

	for (i=0; i<buf_cnt; i++)
	{
		NAND_DMASingleUnmap(rw, nand_dma_addr[NandIndex][i], buf_size[i]);
	}

	return 0;
}

__s32 _reset(void)
{
	__u32 cfg;

	__s32 timeout = 0xffff;

	PHY_DBG("Reset NDFC %d\n", NandIndex);

	/*reset NFC*/
	cfg = NDFC_READ_REG_CTL();
	cfg |= NDFC_RESET;
	NDFC_WRITE_REG_CTL(cfg);
	//waiting reset operation end
	while((timeout--) && (NDFC_READ_REG_CTL()&NDFC_RESET));
	if (timeout <= 0)
	{
	    PHY_ERR("nand _reset time out, status:0x%x\n", NDFC_READ_REG_ST());
		return -ERR_TIMEOUT;
    }
	return 0;
}

/***************ecc function*****************************************/
__s32 _check_ecc(__u32 eblock_cnt)
{
	__u32 i;
	__u32 ecc_mode;
	__u32 max_ecc_bit_cnt = 16;
	__u32 cfg;
	__u32 ecc_cnt_w[8];//ecc_cnt_w[4];
	__u8 *ecc_cnt;
	__u8 ecc_tab[12] = {16, 24, 28, 32, 40, 48, 56, 60, 64, 72};

	ecc_mode = (NDFC_READ_REG_ECC_CTL()>>12)&0xf;
	max_ecc_bit_cnt = ecc_tab[ecc_mode];

	//check ecc errro
	if (NdfcVersion == NDFC_VERSION_V1)
		cfg = NDFC_READ_REG_ECC_ST()&0xffff;
	else if (NdfcVersion == NDFC_VERSION_V2)
		cfg = NDFC_READ_REG_ERR_ST();
	else {
		PHY_ERR("_check_ecc: wrong ndfc version, %d\n", NdfcVersion);
		cfg = 0;
	}
	for (i = 0; i < eblock_cnt; i++)
	{
		if (cfg & (1<<i))
			return -ERR_ECC;
	}

    //check ecc limit
    ecc_cnt_w[0]= NDFC_READ_REG_ECC_CNT0();
    ecc_cnt_w[1]= NDFC_READ_REG_ECC_CNT1();
    ecc_cnt_w[2]= NDFC_READ_REG_ECC_CNT2();
    ecc_cnt_w[3]= NDFC_READ_REG_ECC_CNT3();
    if (NdfcVersion == NDFC_VERSION_V2) {
		ecc_cnt_w[4]= NDFC_READ_REG_ECC_CNT4();
		ecc_cnt_w[5]= NDFC_READ_REG_ECC_CNT5();
		ecc_cnt_w[6]= NDFC_READ_REG_ECC_CNT6();
		ecc_cnt_w[7]= NDFC_READ_REG_ECC_CNT7();
    }

    ecc_cnt = (__u8 *)((__u32)(ecc_cnt_w));
	for (i = 0; i < eblock_cnt; i++)
	{
		if((max_ecc_bit_cnt - 4) <= ecc_cnt[i])
			return ECC_LIMIT;
	}

	return 0;
}

void _disable_ecc(void)
{
	__u32 cfg = NDFC_READ_REG_ECC_CTL();
	cfg &= ( (~NDFC_ECC_EN)&0xffffffff );
	NDFC_WRITE_REG_ECC_CTL(cfg);
}

void _enable_ecc(__u32 pipline)
{
	__u32 cfg = NDFC_READ_REG_ECC_CTL();
	if (pipline == 1)
		cfg |= NDFC_ECC_PIPELINE;
	else
		cfg &= ((~NDFC_ECC_PIPELINE)&0xffffffff);


	/*after erased, all data is 0xff, but ecc is not 0xff,
			so ecc asume it is right*/
	//if random open, disable exception
	if(cfg&(0x1<<9))
	    cfg &= (~(0x1<<4));
	else
	    cfg |= (1 << 4);

	//cfg |= (1 << 1); 16 bit ecc

	cfg |= NDFC_ECC_EN;
	NDFC_WRITE_REG_ECC_CTL(cfg);
}

__s32 _enter_nand_critical(void)
{
	return 0;
}

__s32 _exit_nand_critical(void)
{
	return 0;
}

void _set_addr(__u8 *addr, __u8 cnt)
{
	__u32 i;
	__u32 addr_low = 0;
	__u32 addr_high = 0;

	for (i = 0; i < cnt; i++) {
		if (i < 4)
			addr_low |= (addr[i] << (i*8) );
		else
			addr_high |= (addr[i] << ((i - 4)*8));
	}

	NDFC_WRITE_REG_ADDR_LOW(addr_low);
	NDFC_WRITE_REG_ADDR_HIGH(addr_high);
}

__s32 _read_in_page_mode(NFC_CMD_LIST *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
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
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() | NDFC_RAM_METHOD);

	/*set dma and run*/
	_dma_config_start(0, (__u32)mainbuf, pagesize);

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
		/*set NDFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(pagesize/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = pagesize/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode: wrong ndfc version, %d\n", NdfcVersion);

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
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command

	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NDFC_WRITE_REG_CMD(cfg);

#if 1
    NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, pagesize);
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
	if (pagesize < 4096) {
		PHY_ERR("%s: wrong page size: %d\n", __func__, pagesize);
	}
	for (i = 0; i < pagesize/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(pagesize/1024);
	_disable_ecc();
#endif
	return ret;
}

__s32 _read_in_page_mode_first(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	__u32 blk_cnt, blk_mask;
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
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() | NDFC_RAM_METHOD);

	/*set dma and run*/
	_dma_config_start(0, (__u32)mainbuf, pagesize);

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
		/*set NDFC_REG_SECTOR_NUM*/
		NDFC_WRITE_REG_SECTOR_NUM(pagesize/1024);
	} else if (NdfcVersion == NDFC_VERSION_V2) {
		/*set NFC_REG_BLOCK_MASK*/
		blk_cnt = pagesize/1024;
		blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
		NDFC_WRITE_REG_BLOCK_MASK(blk_mask);
	} else
		PHY_ERR("_read_in_page_mode_first: wrong ndfc version, %d\n", NdfcVersion);

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
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command

	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NDFC_WRITE_REG_CMD(cfg);
#if 0
    NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, pagesize);
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
	for (i = 0; i < pagesize/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA(i);
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(pagesize/1024);
	_disable_ecc();
#endif
	return ret;
}


__s32 _read_in_page_mode_wait(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
{
	__s32 ret;
	__s32 i;

	NAND_WaitDmaFinish();
    ret = _wait_dma_end(0, (__u32)mainbuf, pagesize);
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
	if (pagesize < 4096) {
		PHY_ERR("%s: wrong page size: %d\n", __func__, pagesize);
	}
	for (i = 0; i < pagesize/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA(i);
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(pagesize/1024);
	_disable_ecc();

	return ret;
}

__u32 _cal_first_valid_bit(__u32 secbitmap)
{
	__u32 firstbit = 0;

	while(!(secbitmap & 0x1))
	{
		secbitmap >>= 1;
		firstbit++;
	}

	return firstbit;
}

__u32 _cal_valid_bits(__u32 secbitmap)
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

__u32 _check_continuous_bits(__u32 secbitmap)
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

__s32 _check_secbitmap_bit(__u32 secbitmap, __u32 firstbit, __u32 validbit)
{
	__u32 i = 0;

    for(i=firstbit; i<firstbit+validbit; i++)
    {
        if(!(secbitmap&(0x1<<i)))
        {
            //PHY_ERR("secbitmap 0x%x not seq!\n", secbitmap);
            //return -1;
            //while(1);
        }
    }

	return 0;
}

__s32 _read_secs_in_page_mode(NFC_CMD_LIST  *rcmd,void *mainbuf, void *cachebuf, void *sparebuf, __u32 secbitmap)
{
	__s32 ret,ret1, ecc_flag = 0;
	__s32 i, j, k;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd; //, *read_addr_cmd;
	__u32 random_read_cmd0,random_read_cmd1; //read_data_cmd,
	__u32 eccblkstart, eccblkcnt, eccblksecmap;
	__u32 spareoffsetbak;
	__u8  spairsizetab[9] = {32, 46, 54, 60, 74, 88, 102, 110, 116};
	__u32 ecc_mode, spare_size;
	__u8  addr[2] = {0, 0};
	__u32 col_addr;
	__u32 eccblk_zone_list[4][3]={{0}, {0},{0},{0}}; //[0]: starteccblk; [1]: eccblkcnt; [2]: secbitmap;
	__u32 eccblk_zone_cnt = 0;
	__u32 firstbit, validbit;
	//__u32 dma_buf;
	__u32 *oobbuf = NULL;
	__u32 blk_cnt, blk_mask;

	if(sparebuf)
	{
	    oobbuf = (__u32 *)sparebuf;
	    oobbuf[0] = 0x12345678;
	    oobbuf[1] = 0x12345678;
	}

	firstbit = _cal_first_valid_bit(secbitmap);
	validbit = _cal_valid_bits(secbitmap);

	ecc_mode = (NDFC_READ_REG_CTL()>>12)&0xf;
	spare_size = spairsizetab[ecc_mode];


	/*set NFC_REG_CNT*/
	NDFC_WRITE_REG_CNT(1024);


	//read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (random_read_cmd1 & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NDFC_WRITE_REG_RCMD_SET(cfg);

	//access NFC internal RAM by DMA bus
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() | NDFC_RAM_METHOD);

	if(firstbit%2) //head part
	{
		eccblk_zone_cnt = 0;
		eccblk_zone_list[eccblk_zone_cnt][0] = firstbit/2;
		eccblk_zone_list[eccblk_zone_cnt][1] = 1;
		eccblk_zone_list[eccblk_zone_cnt][2] = 0x2;

		//updata value
		firstbit += 1;
		validbit -= 1;
		eccblk_zone_cnt ++;
	}

	if(validbit/2) //alingn part
	{
		eccblk_zone_list[eccblk_zone_cnt][0] = firstbit/2;
		eccblk_zone_list[eccblk_zone_cnt][1] = validbit/2;
		eccblk_zone_list[eccblk_zone_cnt][2] = 0xffffffff;

		//updata value
		firstbit += 2*eccblk_zone_list[eccblk_zone_cnt][1];
		validbit -= 2*eccblk_zone_list[eccblk_zone_cnt][1];
		eccblk_zone_cnt ++;

	}

	if(validbit>0)  //tail part
	{
		eccblk_zone_list[eccblk_zone_cnt][0] = firstbit/2;
		eccblk_zone_list[eccblk_zone_cnt][1] = 1;
		eccblk_zone_list[eccblk_zone_cnt][2] = 0x1;
		eccblk_zone_cnt ++;

	}

	//for read user data
	if((sparebuf)&&(eccblk_zone_cnt==1)&&(eccblk_zone_list[0][1]==1))
	{
		eccblk_zone_list[0][1]=2;
		eccblk_zone_list[0][2]=0x3;

	}

#if 0
	{
		PHY_DBG("read sectors: bitmap: 0x%x, mainbuf: 0x%x, sparebuf: 0x%x\n", secbitmap, mainbuf, sparebuf);
		for(j=0;j<eccblk_zone_cnt; j++)
		{
			PHY_DBG("  %d, eccblkstart: %d, eccblkcnt: %d \n", j, eccblk_zone_list[j][0], eccblk_zone_list[j][1], eccblk_zone_list[j][2]);
			PHY_DBG("     eccblkmap: 0x%x,\n", eccblk_zone_list[j][2]);
		}
	}
#endif

	for(j=0;j<eccblk_zone_cnt;j++)
	{
		eccblkstart = eccblk_zone_list[j][0];
		eccblkcnt = eccblk_zone_list[j][1];
		eccblksecmap = eccblk_zone_list[j][2];

		//PRINT(" start read %d, eccblkstart: 0x%x, eccblkcnt: 0x%x\n", j, eccblkstart, eccblkcnt);
		//PRINT("     eccblksecmap: 0x%x\n", eccblksecmap);

		//NK page mode
		if((eccblkstart==0)&&(eccblksecmap == 0xffffffff))
		{
		    //PRINT(" NK mode\n");
			ret = 0;
			ret1 = 0;

			spareoffsetbak = NDFC_READ_REG_SPARE_AREA();
			NDFC_WRITE_REG_SPARE_AREA(pagesize + spare_size*eccblkstart);
			col_addr = 1024*eccblkstart;
			addr[0] = col_addr&0xff;
			addr[1] = (col_addr>>8)&0xff;

			/*set dma and run*/
			_dma_config_start(0, (__u32)mainbuf + eccblkstart*1024, eccblkcnt*1024);
			//PRINT("  dmabuf: 0x%x, dmacnt: 0x%x\n", (__u32)mainbuf + eccblkstart*1024, eccblkcnt*1024);

			/*wait cmd fifo free*/
			ret = _wait_cmdfifo_free();
			if (ret)
			{
			    PHY_ERR(" _read_secs_in_page_mode error, NK mode, cmdfifo full \n");
				while(1);
				return ret;
            }

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

			/*set addr*/
			_set_addr(addr,2);

			/*set NFC_REG_CMD*/
			cfg  = 0;
			cfg |= random_read_cmd0;
			/*set sequence mode*/
			//cfg |= 0x1<<25;
			cfg |= ( (rcmd->addr_cycle - 1) << 16);
			cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
			cfg |= ((__u32)0x2 << 30);//page command

			/*enable ecc*/
			_enable_ecc(1);
			NDFC_WRITE_REG_CMD(cfg);

			NAND_WaitDmaFinish();
			/*if dma mode is wait*/
			if(1){
				ret1 = _wait_dma_end(0, (__u32)mainbuf + eccblkstart*1024, eccblkcnt*1024);
				if (ret1)
				{
				    PHY_ERR(" _read_secs_in_page_mode error, NK mode, dma not finish \n");
				    while(1);
					return ret1;
				}
			}

			/*wait cmd fifo free and cmd finish*/
			ret = _wait_cmdfifo_free();
			ret |= _wait_cmd_finish();
			if (ret){
				_disable_ecc();
				NDFC_WRITE_REG_SPARE_AREA(spareoffsetbak);
				//PRINT(" _read_secs_in_page_mode error, NK mode, cmd not finish \n");
				while(1);
				return ret;
			}
			/*get user data*/
			if(sparebuf)
			{
			    for (i = 0; i < eccblkcnt;  i++){
			        if(oobbuf[i%2] == 0x12345678)
			            oobbuf[i%2] = NDFC_READ_REG_USER_DATA((i));
    			}
			}


			/*ecc check and disable ecc*/
			ret = _check_ecc(eccblkcnt);
			ecc_flag |= ret;
			_disable_ecc();
			NDFC_WRITE_REG_SPARE_AREA(spareoffsetbak);


		}
		else  //1K page mode
		{
		    //PRINT(" 1K mode\n");
			for(k=0;k<eccblkcnt;k++)
			{
			    //PRINT("k= %d\n", k);
			    //PRINT("eccblk_index: %d\n", eccblkstart+k);
				ret = 0;
				ret1 = 0;

				spareoffsetbak = NDFC_READ_REG_SPARE_AREA();
				NDFC_WRITE_REG_SPARE_AREA(pagesize + spare_size*(eccblkstart+k));
				col_addr = 1024*(eccblkstart+k);
				addr[0] = col_addr&0xff;
				addr[1] = (col_addr>>8)&0xff;

				/*set dma and run*/
				if((eccblksecmap==0xffffffff)||((eccblksecmap>>k*2) == 0x3))
				{
					_dma_config_start(0, (__u32)mainbuf + 1024*(eccblkstart+k), 1024);
					//PRINT("  dmabuf: 0x%x, dmacnt: 0x%x\n", (__u32)mainbuf + 1024*(eccblkstart+k), 1024);
				}
				else
				{
					_dma_config_start(0, (__u32)cachebuf + 1024*(eccblkstart+k), 1024);
					//PRINT("  dmabuf: 0x%x, dmacnt: 0x%x\n", (__u32)cachebuf + 1024*(eccblkstart+k), 1024);
	                	}

				/*wait cmd fifo free*/
				ret = _wait_cmdfifo_free();
				if (ret)
				{
				    PHY_ERR(" _read_secs_in_page_mode error, 1K mode, cmdfifo full \n");
				    while(1);
					return ret;
				}
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

				/*set addr*/
				_set_addr(addr,2);

				/*set NFC_REG_CMD*/
				cfg  = 0;
				cfg |= random_read_cmd0;
				/*set sequence mode*/
				//cfg |= 0x1<<25;
				cfg |= ( (rcmd->addr_cycle - 1) << 16);
				cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
				cfg |= ((__u32)0x2 << 30);//page command

				/*enable ecc*/
				_enable_ecc(1);
				NDFC_WRITE_REG_CMD(cfg);

				/* don't user dma int in  1K mode */
				//NAND_WaitDmaFinish();
				/*if dma mode is wait*/
				if(1){
					if((eccblksecmap==0xffffffff)||((eccblksecmap>>k*2) == 0x3))
					{
						ret1 |= _wait_dma_end(0, (__u32)mainbuf + 1024*(eccblkstart+k), 1024);
						//PRINT("  dmabuf: 0x%x, dmacnt: 0x%x\n", (__u32)mainbuf + 1024*(eccblkstart+k), 1024);
					}
					else
					{
						ret1 |= _wait_dma_end(0, (__u32)cachebuf + 1024*(eccblkstart+k), 1024);
						//PRINT("  dmabuf: 0x%x, dmacnt: 0x%x\n", (__u32)cachebuf + 1024*(eccblkstart+k), 1024);
		                	}

					if (ret1)
					{
					    PHY_ERR(" _read_secs_in_page_mode error, 1K mode, dma not finish \n");
				        while(1);
						return ret1;
					}
				}

				/*wait cmd fifo free and cmd finish*/
				ret = _wait_cmdfifo_free();
				ret |= _wait_cmd_finish();
				if (ret){
					_disable_ecc();
					NDFC_WRITE_REG_SPARE_AREA(spareoffsetbak);
			        PHY_ERR(" _read_secs_in_page_mode error, 1K mode, cmd not finish \n");
				    while(1);
					return ret;
				}
				/*get user data*/
				if(sparebuf)
	        		{
	    		        if(oobbuf[(eccblkstart+k)%2] == 0x12345678)
	    		            oobbuf[(eccblkstart+k)%2] = NDFC_READ_REG_USER_DATA((0));
	        		}

				/*ecc check and disable ecc*/
				ret = _check_ecc(1);
				ecc_flag |= ret;
				_disable_ecc();
				NDFC_WRITE_REG_SPARE_AREA(spareoffsetbak);


				//copy main data
				if(!((eccblksecmap==0xffffffff)||((eccblksecmap>>k*2) == 0x3)))
				{
					if((eccblksecmap>>k*2)==0x1)
					    MEMCPY((__u8 *)mainbuf + 1024*(eccblkstart+k), (__u8 *)cachebuf + 1024*(eccblkstart+k), 512);
					else if((eccblksecmap>>k*2)==0x2)
						MEMCPY((__u8 *)mainbuf + 1024*(eccblkstart+k)+512, (__u8 *)cachebuf + 1024*(eccblkstart+k)+512, 512);
					//else if((eccblksecmap>>k*2)==0x3)
					//    MEMCPY((__u32)mainbuf + 1024*(eccblkstart+k), (__u32)cachebuf + 1024*(eccblkstart+k), 1024);
				}
				//else
				//    MEMCPY((__u32)mainbuf + 1024*(eccblkstart+k), (__u32)cachebuf + 1024*(eccblkstart+k), 1024);
			}
		}
	}

	NDFC_WRITE_REG_RCMD_SET(0x003005e0);

	return ecc_flag;
}


__s32 _read_eccblks_in_page_mode(NFC_CMD_LIST *rcmd, void *sparebuf, __u32 eccblkmask, __u32 dst_buf_cnt,
										__u32 dst_buf_addr[], __u32 dst_buf_size[])
{
	__s32 ret;
	__s32 i;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	//__u32 blk_cnt, blk_mask;

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

	/*set dma and run*/
	//_dma_config_start(0, (__u32)mainbuf, pagesize);
	_dma_config_start_v2(0, dst_buf_cnt, dst_buf_addr, dst_buf_size);

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

	/*set NFC_REG_BLOCK_MASK*/
	//blk_cnt = pagesize/1024;
	//blk_mask = ((1<<(blk_cnt - 1)) | ((1<<(blk_cnt - 1)) - 1));
	//NFC_WRITE_REG(NFC_REG_BLOCK_MASK, blk_mask);
	NDFC_WRITE_REG_BLOCK_MASK(eccblkmask);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*Gavin-20130619, clear user data*/
	for (i=0; i<16; i++)
		NDFC_WRITE_REG_USER_DATA((i), 0x88888888);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NDFC_SEND_ADR | NDFC_DATA_TRANS | NDFC_SEND_CMD1 | NDFC_SEND_CMD2 | NDFC_WAIT_FLAG | NDFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command

	if (pagesize/1024 == 1)
		cfg |= NDFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NDFC_WRITE_REG_CMD(cfg);
#if 1
    NAND_WaitDmaFinish();
    //ret = _wait_dma_end(0, (__u32)mainbuf, pagesize);
    ret = _wait_dma_end_v2(0, dst_buf_cnt, dst_buf_addr, dst_buf_size);
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
	if (pagesize < 4096) {
		PHY_ERR("%s: wrong page size: %d\n", __func__, pagesize);
	}
	for (i = 0; i < pagesize/1024;  i++){
		*(((__u32*) sparebuf)+i) = NDFC_READ_REG_USER_DATA((i));
	}

	/*ecc check and disable ecc*/
	ret = _check_ecc(pagesize/1024);
	_disable_ecc();
#endif
	return ret;
}

__s32 _read_secs_in_page_mode_v2(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u32 secbitmap)
{
	__s32 i, ret;
	__u32 *tmpsparebuf, *tmpbuf1, *tmpbuf3; //*tmpbuf2,
	__u32 fir_sect;
	__u32 sect_cnt;
	__u32 last_sect;
	//u32 spare_buf[64];
	//u32 *main_buf = (u32 *)0xeee;
	__u32 ind, last_ind;
	__u32 pos, mask, bitcnt;
	__u32 dst_buf_cnt;
	__u32 dst_buf_size[4];
	__u32 dst_buf_addr[4];
	__u32 main_data_size;

	__u32 fir_spare_sect = 0; //first 4 ecc blk;
	__u32 spare_sect_cnt = 8;
	__u32 last_spare_sect = fir_spare_sect+spare_sect_cnt-1;
	__u32 spare_ecc_blk_cnt = (spare_sect_cnt/2);
	__u32 spare_ecc_blk_mask = (1U<<(spare_ecc_blk_cnt-1)) | ((1U<<(spare_ecc_blk_cnt-1)) - 1);

	__u32 total_size, goal_size;
	__u8 *psrc, *pdst;


	fir_sect = _cal_first_valid_bit(secbitmap);
	sect_cnt = _cal_valid_bits(secbitmap);
	if (_check_secbitmap_bit(secbitmap, fir_sect, sect_cnt))
	{
		//PHY_ERR("sect bit map error: 0x%x\n", secbitmap);
		//return -ERR_TIMEOUT;
	}
	last_sect = fir_sect + sect_cnt - 1;

	tmpsparebuf = (__u32 *)(PageCachePool.PageCache4);
	tmpbuf1 = (__u32 *)(PageCachePool.PageCache1);
	//tmpbuf2 = (__u32 *)(PageCachePool.PageCache2);
	tmpbuf3 = (__u32 *)(PageCachePool.PageCache3);

	main_data_size = sect_cnt*512;
	goal_size = main_data_size;

	/*
	**********************************************
	*  Allocate dma buffer according sector bitmap
	***********************************************
	*/
	if (last_sect <= last_spare_sect)
	{
		mask = spare_ecc_blk_mask;

		dst_buf_cnt = 0;
		ind = fir_sect;
		if (fir_sect%0x2)
		{
			pos = ind/2;
			//mask = (1U << pos);

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * (pos+1);
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf1;

			ind++;
		}
		else if (fir_sect)
		{
			pos = ind/2;

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * pos;
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf1;
		}

		last_ind = last_sect;
		bitcnt = 0;
		while (ind < last_ind)
		{
			pos = ind/2;
			//mask |= (1U << pos);

			ind += 2;
			bitcnt++;
		}
		if (bitcnt)
		{
			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * bitcnt;
			if (fir_sect%0x2)
				dst_buf_addr[dst_buf_cnt-1] = (__u32)mainbuf + 512;
			else
				dst_buf_addr[dst_buf_cnt-1] = (__u32)mainbuf;
		}

		if ((last_sect%0x2) == 0)
		{
			pos = last_sect/2;
			//mask = (1U << pos);

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * (spare_ecc_blk_cnt-pos); //(4-pos) -->
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf3;
		}
		else
		{
			pos = last_sect/2;
			if (pos < 3)
			{
				dst_buf_cnt++;
				dst_buf_size[dst_buf_cnt-1] = 1024 * (spare_ecc_blk_cnt-pos-1); //(4-pos-1)
				dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf3;
			}
		}

		//PHY_DBG("all sects are in spare data area: mask 0x%x\n", mask);
		total_size = 0;
		for (i=0; i<dst_buf_cnt; i++)
		{
			//PHY_DBG("buf %d: size 0x%x    addr 0x%x\n", i, dst_buf_size[i], dst_buf_addr[i]);
			total_size += dst_buf_size[i];
		}
		//PHY_DBG("total size: 0x%x    goal size: 0x%x\n", total_size, goal_size);
	}
	else if (fir_sect > last_spare_sect)
	{
		mask = spare_ecc_blk_mask;
		dst_buf_cnt = 1;
		dst_buf_size[dst_buf_cnt-1] = spare_ecc_blk_cnt * 1024;////4*1024 -->
		dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpsparebuf;


		ind = fir_sect;
		if (fir_sect%0x2)
		{
			pos = ind/2;
			mask |= (1U << pos);

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024;
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf1;

			ind++;
		}


		last_ind = last_sect;
		bitcnt = 0;
		while(ind < last_ind)
		{
			pos = ind/2;
			mask |= (1U << pos);

			ind += 2;
			bitcnt++;
		}
		if (bitcnt)
		{
			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * bitcnt;
			if (fir_sect%0x2)
				dst_buf_addr[dst_buf_cnt-1] = (__u32)mainbuf + 512;
			else
				dst_buf_addr[dst_buf_cnt-1] = (__u32)mainbuf;
		}

		if ((last_sect%0x2) == 0)
		{
			pos = last_sect/2;
			mask |= (1U << pos);

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024;
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf3;
		}

		//PHY_DBG("mask 0x%x\n", mask);
		total_size = 0;
		for (i=0; i<dst_buf_cnt; i++)
		{
			//PHY_DBG("buf %d: size 0x%x    addr 0x%x\n", i, dst_buf_size[i], dst_buf_addr[i]);
			total_size += dst_buf_size[i];
		}
		//PHY_DBG("total size: 0x%x    goal size: 0x%x\n", total_size, goal_size);
	}
	else //( (fir_sect<=last_spare_sect) && (last_sect>last_spare_sect))
	{
		mask = spare_ecc_blk_mask;

		dst_buf_cnt = 0;
		ind = fir_sect;
		if (fir_sect%0x2)
		{
			pos = ind/2;
			mask |= (1U << pos);

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * (pos+1);
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf1;

			ind++;
		}
		else if (fir_sect)
		{
			pos = ind/2;

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * pos;
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf1;
		}

		last_ind = last_sect;
		bitcnt = 0;
		while(ind < last_ind)
		{
			pos = ind/2;
			mask |= (1U << pos);

			ind += 2;
			bitcnt++;
		}
		if (bitcnt)
		{
			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024 * bitcnt;
			if (fir_sect%0x2)
				dst_buf_addr[dst_buf_cnt-1] = (__u32)mainbuf + 512;
			else
				dst_buf_addr[dst_buf_cnt-1] = (__u32)mainbuf;
		}

		if ((last_sect%0x2) == 0)
		{
			pos = last_sect/2;
			mask |= (1U << pos);

			dst_buf_cnt++;
			dst_buf_size[dst_buf_cnt-1] = 1024;
			dst_buf_addr[dst_buf_cnt-1] = (__u32)tmpbuf3;
		}

		//PHY_DBG("mask 0x%x\n", mask);
		total_size = 0;
		for (i=0; i<dst_buf_cnt; i++)
		{
			//PHY_DBG("buf %d: size 0x%x    addr 0x%x\n", i, dst_buf_size[i], dst_buf_addr[i]);
			total_size += dst_buf_size[i];
		}
		//PHY_DBG("total size: 0x%x    goal size: 0x%x\n", total_size, goal_size);
	}


	/*
	***********************************************
	*  read main data and spare data
	***********************************************
	*/

	{
		ret = _read_eccblks_in_page_mode(rcmd, sparebuf, mask, dst_buf_cnt, dst_buf_addr, dst_buf_size);
	}

	/*
	***********************************************
	*  do some memory copy according sector bitmap
	***********************************************
	*/
	//memcpy for main data
	if (fir_sect > last_spare_sect)
	{
		if (fir_sect%2)
		{
			psrc = (__u8 *)(dst_buf_addr[1] + dst_buf_size[1] - 512);
			pdst = (__u8 *)(mainbuf) ;
			for (i=0; i<512; i++)
				pdst[i] = psrc[i];
		}
	}
	else
	{
		if (fir_sect%2)
		{
			psrc = (__u8 *)(dst_buf_addr[0] + dst_buf_size[0] - 512);
			pdst = (__u8 *)(mainbuf) ;
			for (i=0; i<512; i++)
				pdst[i] = psrc[i];
		}
	}

	if ((last_sect%2) == 0)
	{
		psrc = (__u8 *)(dst_buf_addr[dst_buf_cnt-1]);
		pdst = (__u8 *)((__u32)mainbuf + main_data_size - 512);
		for (i=0; i<512; i++)
			pdst[i] = psrc[i];
	}

	return ret;
}


/*******************************************************************************
*								NFC_Read
*
* Description 	: read some sectors data from flash in single plane mode.
* Arguments	: *rcmd	-- the read command sequence list head.
*			  *mainbuf	-- point to data buffer address, 	it must be four bytes align.
*                     *sparebuf	-- point to spare buffer address.
*                     dma_wait_mode	-- how to deal when dma start, 0 = wait till dma finish,
							    1 = dma interrupt was set and now sleep till interrupt occurs.
*			  page_mode  -- 0 = normal command, 1 = page mode
* Returns		: 0 = success.
			  1 = success & ecc limit.
			  -1 = too much ecc err.
* Notes		:  if align page data required£¬page command mode is used., if the commands do
			   not fetch data£¬ecc is not neccesary.
********************************************************************************/
__s32 NFC_Read(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{

	__u32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));

	_exit_nand_critical();


	return ret;
}

__s32 NFC_Read_First(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{

	__s32 ret ;

	_enter_nand_critical();

	ret = _read_in_page_mode_first(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
//	NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) & (~NFC_RAM_METHOD));

//	_exit_nand_critical();


	return ret;
}


__s32 NFC_Read_Wait(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode )
{

	__s32 ret ;

//	_enter_nand_critical();

	ret = _read_in_page_mode_wait(rcmd, mainbuf,sparebuf, dma_wait_mode);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));

	_exit_nand_critical();


	return ret;
}

__s32 NFC_ReadSecs(NFC_CMD_LIST  *rcmd, void *mainbuf,  void *cachebuf, void *sparebuf,__u32 secbitmap )
{

	__s32 ret ;

	_enter_nand_critical();

	PHY_ERR("[NAND ERROR] function --NFC_ReadSecs-- is called!\n");

	ret = _read_secs_in_page_mode(rcmd, mainbuf,cachebuf, sparebuf, secbitmap);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));

	_exit_nand_critical();


	return ret;
}

__s32 NFC_ReadSecs_v2(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u32 secbitmap)
{

	__s32 ret ;

	_enter_nand_critical();

	//PHY_ERR("%s is called...!\n", __func__);

	ret = _read_secs_in_page_mode_v2(rcmd, mainbuf, sparebuf, secbitmap);

	/*switch to ahb*/
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));

	_exit_nand_critical();


	return ret;
}

/*finish the comand list */
__s32 nfc_set_cmd_register(NFC_CMD_LIST *cmd)
{
	__u32 cfg;
	__s32 ret;

	NFC_CMD_LIST *cur_cmd = cmd;
	while(cur_cmd != NULL){
		/*wait cmd fifo free*/
		ret = _wait_cmdfifo_free();
		if (ret)
			return ret;

		cfg = 0;
		/*set addr*/
		if (cur_cmd->addr_cycle){
			_set_addr(cur_cmd->addr,cur_cmd->addr_cycle);
			cfg |= ( (cur_cmd->addr_cycle - 1) << 16);
			cfg |= NDFC_SEND_ADR;
		}

		/*set NFC_REG_CMD*/
		/*set cmd value*/
		cfg |= cur_cmd->value;
		/*set sequence mode*/
		//cfg |= 0x1<<25;
		/*wait rb?*/
		if (cur_cmd->wait_rb_flag){
			cfg |= NDFC_WAIT_FLAG;
		}
		if (cur_cmd->data_fetch_flag){
			NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));
			cfg |= NDFC_DATA_TRANS;
			NDFC_WRITE_REG_CNT(cur_cmd->bytecnt);
		}
		/*send command*/
		cfg |= NDFC_SEND_CMD1;
		NDFC_WRITE_REG_CMD(cfg);
		cur_cmd = cur_cmd ->next;
	}
	return 0;
}

__s32 NFC_SetRandomSeed(__u32 random_seed)
{
	__u32 cfg;


	  cfg = NDFC_READ_REG_ECC_CTL();
	  cfg &= 0x0000ffff;
	  cfg |= (random_seed<<16);
	  NDFC_WRITE_REG_ECC_CTL(cfg);

	return 0;
}

__s32 NFC_RandomEnable(void)
{
	__u32 cfg;

	cfg = NDFC_READ_REG_ECC_CTL();
	cfg |= (0x1<<9);
	NDFC_WRITE_REG_ECC_CTL(cfg);

	return 0;
}

__s32 NFC_RandomDisable(void)
{
	__u32 cfg;

	cfg = NDFC_READ_REG_ECC_CTL();
	cfg &= (~(0x1<<9));
	NDFC_WRITE_REG_ECC_CTL(cfg);

	return 0;
}



/*******************************************************************************
*								NFC_GetId
*
* Description 	: get chip id.
* Arguments	: *idcmd	-- the get id command sequence list head.

* Returns		: 0 = success.
			  -1 = fail.
* Notes		:
********************************************************************************/
__s32 NFC_GetId(NFC_CMD_LIST  *idcmd ,__u8 *idbuf)
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
	for (i = 0; i < 6; i++){
		*(idbuf + i) = NDFC_READ_RAM0_B(i);
	}

    nfc_repeat_mode_disable();

	_exit_nand_critical();
	return ret;
}

__s32 NFC_NormalCMD(NFC_CMD_LIST  *cmd_list)
{
	//__u32 i;
	__s32 ret;

	_enter_nand_critical();

    nfc_repeat_mode_enable();
	ret = nfc_set_cmd_register(cmd_list);
	if (ret){
		_exit_nand_critical();
		return ret;
	}

	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();

    nfc_repeat_mode_disable();

	_exit_nand_critical();
	return ret;
}



/*******************************************************************************
*								NFC_GetStatus
*
* Description 	: get status.
* Arguments	: *scmd	-- the get status command sequence list head.

* Returns		: status result
* Notes		: some cmd must be sent with addr.
********************************************************************************/
__s32 NFC_GetStatus(NFC_CMD_LIST  *scmd)
{
	__s32 ret;

	_enter_nand_critical();
	nfc_repeat_mode_enable();
	ret = nfc_set_cmd_register(scmd);
	if (ret){
		_exit_nand_critical();
		return ret;
	}

	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if(ret){
		_exit_nand_critical();
		return ret;
	}

    nfc_repeat_mode_disable();
	_exit_nand_critical();
	return (NDFC_READ_RAM0_B(0));

}
/*******************************************************************************
*								NFC_ResetChip
*
* Description 	: reset nand flash.
* Arguments	: *reset_cmd	-- the reset command sequence list head.

* Returns		: sucess or fail
* Notes		:
********************************************************************************/
__s32 NFC_ResetChip(NFC_CMD_LIST *reset_cmd)

{
	__s32 ret;

	_enter_nand_critical();

	PHY_DBG("NFC_ResetChip: 0x%x, 0x%x 0x%x\n", NDFC_READ_REG_CTL(), NDFC_READ_REG_TIMING_CTL(), NDFC_READ_REG_TIMING_CFG());
	PHY_DBG("NFC_ResetChip: 0x%x, ch: %d\n", reset_cmd->value, NandIndex);

	ret = nfc_set_cmd_register(reset_cmd);
	if (ret){
		_exit_nand_critical();
		return ret;
	}
	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	_exit_nand_critical();
	return ret;
}

/*******************************************************************************
*								NFC_SetFeature
*
* Description 	: set feature.
* Arguments	: *set_feature_cmd	-- the set feature command sequence list head.

* Returns		: sucess or fail
* Notes		:
********************************************************************************/
__s32 NFC_SetFeature(NFC_CMD_LIST *set_feature_cmd, __u8 *feature)
{
	__s32 ret;
	__u32 cfg;

	_enter_nand_critical();
	nfc_repeat_mode_enable();

	/* access NFC internal RAM by AHB bus */
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));

#if 0
	// nfc_set_cmd_register() don't support write data
	ret = nfc_set_cmd_register(set_feature_cmd);
	if (ret){
		_exit_nand_critical();
		return ret;
	}
#endif

	/* set data */
	NDFC_WRITE_RAM0_B(0, feature[0]);
	NDFC_WRITE_RAM0_B(1, feature[1]);
	NDFC_WRITE_RAM0_B(2, feature[2]);
	NDFC_WRITE_RAM0_B(3, feature[3]);
	NDFC_WRITE_REG_CNT(4);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	cfg = 0;
	/*set addr*/
	if (set_feature_cmd->addr_cycle){
		_set_addr(set_feature_cmd->addr,set_feature_cmd->addr_cycle);
		cfg |= ( (set_feature_cmd->addr_cycle - 1) << 16);
		cfg |= NDFC_SEND_ADR;
	}

	cfg |= NDFC_ACCESS_DIR | NDFC_WAIT_FLAG | NDFC_DATA_TRANS | NDFC_SEND_CMD1;
	cfg |= (set_feature_cmd->value & 0xff);

	/*set command io */
	NDFC_WRITE_REG_CMD(cfg);

	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if(ret){
		_exit_nand_critical();
		return ret;
	}

    nfc_repeat_mode_disable();
	_exit_nand_critical();
	return ret;
}

/*******************************************************************************
*								NFC_GetFeature
*
* Description 	: get feature.
* Arguments	: *reset_cmd	-- the get feature command sequence list head.

* Returns		: sucess or fail
* Notes		:
********************************************************************************/
__s32 NFC_GetFeature(NFC_CMD_LIST *get_feature_cmd, __u8 *feature)
{
	__s32 ret;
	__u32 cfg;

	_enter_nand_critical();
	nfc_repeat_mode_enable();

	/* access NFC internal RAM by AHB bus */
	NDFC_WRITE_REG_CTL(NDFC_READ_REG_CTL() & (~NDFC_RAM_METHOD));

	/* set data cnt*/
	NDFC_WRITE_REG_CNT(4);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	cfg = 0;
	/*set addr*/
	if (get_feature_cmd->addr_cycle){
		_set_addr(get_feature_cmd->addr,get_feature_cmd->addr_cycle);
		cfg |= ( (get_feature_cmd->addr_cycle - 1) << 16);
		cfg |= NDFC_SEND_ADR;
	}

	cfg |= NDFC_WAIT_FLAG | NDFC_DATA_TRANS | NDFC_SEND_CMD1; //NDFC_ACCESS_DIR
	cfg |= (get_feature_cmd->value & 0xff);

	/*set command io */
	NDFC_WRITE_REG_CMD(cfg);


	ret = _wait_cmdfifo_free();
	ret |= _wait_cmd_finish();
	if(ret) {
		_exit_nand_critical();
		return ret;
	}

	/* get data */
	feature[0] = NDFC_READ_RAM0_B(0);
	feature[1] = NDFC_READ_RAM0_B(1);
	feature[2] = NDFC_READ_RAM0_B(2);
	feature[3] = NDFC_READ_RAM0_B(3);

    nfc_repeat_mode_disable();
	_exit_nand_critical();
	return 0;
}

/*******************************************************************************
*								NFC_SelectChip
*
* Description 	: enable chip ce.
* Arguments	: chip	-- chip no.

* Returns		: 0 = sucess -1 = fail
* Notes		:
********************************************************************************/
__s32 NFC_SelectChip( __u32 chip)
{
	__u32 cfg;


    cfg = NDFC_READ_REG_CTL();
    cfg &= ( (~NDFC_CE_SEL) & 0xffffffff);
    cfg |= ((chip & 0x7) << 24);
#if 0
    if(((read_retry_mode == 0)||(read_retry_mode == 1))&&(read_retry_cycle))
        cfg |= (0x1<<6);
#endif
    NDFC_WRITE_REG_CTL(cfg);

    if((cfg>>18)&0x3) //ddr nand
    {
        //set ddr param
        NDFC_WRITE_REG_TIMING_CTL(ddr_param[0]);
    }

	return 0;
}

/*******************************************************************************
*								NFC_SelectRb
*
* Description 	: select rb.
* Arguments	: rb	-- rb no.

* Returns		: 0 = sucess -1 = fail
* Notes		:
********************************************************************************/
__s32 NFC_SelectRb( __u32 rb)
{
	__s32 cfg;

	cfg = NDFC_READ_REG_CTL();
	cfg &= ( (~NDFC_RB_SEL) & 0xffffffff);
	cfg |= ((rb & 0x1) << 3);
	NDFC_WRITE_REG_CTL(cfg);

	return 0;
}

__s32 NFC_DeSelectChip( __u32 chip)
{
#if 0
    __u32 cfg;

    if(((read_retry_mode == 0)||(read_retry_mode == 1))&&(read_retry_cycle))
    {
        cfg = NDFC_READ_REG_CTL();
        cfg &= (~(0x1<<6));
        NDFC_WRITE_REG_CTL(cfg);
    }
#endif
	return 0;
}

__s32 NFC_DeSelectRb( __u32 rb)
{
	return 0;
}


/*******************************************************************************
*								NFC_CheckRbReady
*
* Description 	: check rb if ready.
* Arguments	: rb	-- rb no.

* Returns		: 0 = sucess -1 = fail
* Notes		:
********************************************************************************/
__s32 NFC_CheckRbReady( __u32 rb)
{
	__s32 ret;
	__u32 cfg = NDFC_READ_REG_ST();

	cfg &= (NDFC_RB_STATE0 << (rb & 0x3));
	if (cfg)
		ret = 0;
	else
		ret = -1;

	return ret;
}

/*******************************************************************************
*								NFC_ChangeMode
*
* Description 	: change serial access mode when clock change.
* Arguments	: nand_info -- structure with flash bus width,pagesize ,serial access mode and other configure parametre
*
* Returns		: 0 = sucess -1 = fail
* Notes		: NFC must be reset before seial access mode changes.
********************************************************************************/
__s32 NFC_ChangMode(NFC_INIT_INFO *nand_info)
{
	__u32 cfg;

	pagesize = nand_info->pagesize * 512;

	/*reset nfc*/
	_reset();

	/*set NFC_REG_CTL*/
	cfg = 0;
	cfg |= NDFC_EN;
	cfg |= ( (nand_info->bus_width & 0x1) << 2);
	cfg |= ( (nand_info->ce_ctl & 0x1) << 6);
	cfg |= ( (nand_info->ce_ctl1 & 0x1) << 7);
	if(nand_info->pagesize == 2 )            /*  1K  */
	   cfg |= ( 0x0 << 8 );
	else if(nand_info->pagesize == 4 )       /*  2K  */
	   cfg |= ( 0x1 << 8 );
	else if(nand_info->pagesize == 8 )       /*  4K  */
	   cfg |= ( 0x2 << 8 );
    else if(nand_info->pagesize == 16 )       /*  8K  */
	   cfg |= ( 0x3 << 8 );
	else if(nand_info->pagesize == 32 )       /*  16K  */
	   cfg |= ( 0x4 << 8 );
	else                                      /* default 4K */
	   cfg |= ( 0x2 << 8 );
	//if (first_change)
	//	cfg |= ((nand_info->ddr_type & 0x3) << 18);   //set ddr type
	cfg |= ((nand_info->debug & 0x1) << 31);
	NDFC_WRITE_REG_CTL(cfg);

	/*set NFC_SPARE_AREA */
	NDFC_WRITE_REG_SPARE_AREA(pagesize);

	return 0;
}

void NFC_GetInterfaceMode(NFC_INIT_INFO *nand_info)
{
	__u32 cfg = 0;

	/* ddr type */
	cfg = NDFC_READ_REG_CTL();
	nand_info->ddr_type = (cfg>>18) & 0x3;
	if (NdfcVersion == NDFC_VERSION_V2)
		nand_info->ddr_type |= (((cfg>>28) & 0x1) <<4);

	/* edo && delay */
	cfg = NDFC_READ_REG_TIMING_CTL();
	nand_info->serial_access_mode = (cfg>>8) & 0x3;
	nand_info->ddr_edo            = (cfg>>8) & 0xf;
	nand_info->ddr_delay          = cfg & 0x3f;

	return ;
}

/*
* Arguments	: nand_info -- structure with flash bus width,pagesize ,serial access mode and other configure parametre
*
*/
void NFC_ChangeInterfaceMode(NFC_INIT_INFO *nand_info)
{
	__u32 cfg = 0;

	if (NdfcVersion == NDFC_VERSION_V1) {
		if ((nand_info->ddr_type == 0x12) || (nand_info->ddr_type == 0x13)) {
			PHY_ERR("NFC_ChangMode: current ndfc don't support ddr2 interface, %x -> 0x!\n",
				nand_info->ddr_type, nand_info->ddr_type&0x3);
			nand_info->ddr_type &= 0x3;
		}
	}

	/* ddr type */
	cfg = NDFC_READ_REG_CTL();
	cfg &= ~(0x3U<<18);
	if (NdfcVersion == NDFC_VERSION_V2)
		cfg &= ~(0x1<<28);
	cfg |= (nand_info->ddr_type&0x3)<<18;
	if (NdfcVersion == NDFC_VERSION_V2)
		cfg |= ((nand_info->ddr_type>>4)&0x1)<<28;
	NDFC_WRITE_REG_CTL(cfg);

	/* edo && delay */
	cfg = NDFC_READ_REG_TIMING_CTL();
	if (nand_info->ddr_type == 0) {
		cfg &= ~((0xf<<8) | 0x3f);
		cfg |= (nand_info->serial_access_mode<<8);
		NDFC_WRITE_REG_TIMING_CTL(cfg);
	} else {
		cfg &= ~((0xf<<8) | 0x3f);
		cfg |= (nand_info->ddr_edo <<8);
		cfg |= nand_info->ddr_delay;
		NDFC_WRITE_REG_TIMING_CTL(cfg);
	}

	/*
		 ndfc's timing cfg
		 1. default value: 0x95
		 2. bit-16, tCCS=1 for micron l85a, nvddr-100mhz
	 */
	NDFC_WRITE_REG_TIMING_CFG(0x10095);

	return ;
}

__s32 NFC_SetEccMode(__u8 ecc_mode)
{
    __u32 cfg = NDFC_READ_REG_ECC_CTL();

    cfg &= ((~NDFC_ECC_MODE)&0xffffffff);
    cfg |= (NDFC_ECC_MODE & (ecc_mode<<12));

	NDFC_WRITE_REG_ECC_CTL(cfg);

	return 0;
}

__s32 NFC_GetEccMode(void)
{
 	return ((NDFC_READ_REG_ECC_CTL()>>12) & 0xf);
}
/*******************************************************************************
*								NFC_Init
*
* Description 	: init hardware, set NFC, set TIMING, request dma .
* Arguments	: nand_info -- structure with flash bus width,pagesize ,serial access mode and other configure parametre

* Returns		: 0 = sucess -1 = fail
* Notes		: .
********************************************************************************/
__s32 NFC_Init(NFC_INIT_INFO *nand_info )
{
	__s32 ret;
    __s32 i;

	if(NandIndex == 0)
	{
		//PHY_DBG("[NAND] nand driver version: 0x%x, 0x%x 0x%x 14:00\n", NAND_VERSION_0, NAND_VERSION_1, NAND_DRV_DATE);
	}

    //init ddr_param
    for(i=0;i<8;i++)
	{
		if(((*(volatile __u32 *)(0xf8001400+0x190))>>0x3)&0x1)
    		ddr_param[i] = 0x31f;
		else
			ddr_param[i] = 0x339;
	}
    NandIOBase[0] = (__u32)NAND_IORemap(NAND_IO_BASE_ADDR0, 4096);
    NandIOBase[1] = (__u32)NAND_IORemap(NAND_IO_BASE_ADDR1, 4096);

    if ( ndfc_init_version() )
    	return -1;

    if ( ndfc_init_dma_mode() )
    	return -1;

    //init pin
    NAND_PIORequest(NandIndex);

	//request general dma channel
	if (NdfcDmaMode == 0) {
		if( 0 != nand_request_dma() ) {
			PHY_ERR("request dma fail!\n");
			return -1;
		} else
			PHY_DBG("request general dma channel ok!\n");
	}

    //init clk
    NAND_ClkRequest(NandIndex);
    NAND_SetClk(NandIndex, 10, 10*2);

	if(NAND_GetVoltage())
		return -1;

    //init dma
	NFC_SetEccMode(0);

	/*init nand control machine*/
	ret = NFC_ChangMode( nand_info );
	NFC_ChangeInterfaceMode( nand_info );

	return ret;
}

/*******************************************************************************
*								NFC_Exit
*
* Description 	: free hardware resource, free dma , disable NFC.
* Arguments	: nand_info -- structure with flash bus width,pagesize ,serial access mode and other configure parametre

* Returns		: 0 = sucess -1 = fail
* Notes		: .
********************************************************************************/
void NFC_Exit( void )
{
	__u32 cfg;
	/*disable NFC*/
	cfg = NDFC_READ_REG_CTL();
	cfg &= ((~NDFC_EN) & 0xffffffff);
	NDFC_WRITE_REG_CTL(cfg);

	 //init clk
    NAND_ClkRelease(NandIndex);

	NAND_ReleaseDMA(NandIndex);

    //init pin
    NAND_PIORelease(NandIndex);

	NAND_ReleaseVoltage();

}

/*******************************************************************************
*								NFC_QueryINT
*
* Description 	: get nand interrupt info.
* Arguments	:
* Returns		: interrupt no. 0 = RB_B2R,1 = SINGLE_CMD_FINISH,2 = DMA_FINISH,
								5 = MULTI_CMD_FINISH
* Notes		:
********************************************************************************/
__s32 _vender_get_param(__u8 *para, __u8 *addr, __u32 count)
{

	return 0;
}

__s32 _vender_set_param(__u8 *para, __u8 *addr, __u32 count)
{

	return 0;
}

__s32 _vender_pre_condition(void)
{

	return 0;
}

__s32 _vender_get_param_otp_hynix(__u8 *para, __u8 *addr, __u32 count)
{

	return 0;
}

__s32 _major_check_byte(__u8 *out, __u32 mode, __u32 level, __u8 *in, __u8 *in_inverse, __u32 len)
{
	return 0;
}


__s32 _get_read_retry_cfg(__u8 *rr_cnt, __u8 *rr_reg_cnt, __u8 *rr_tab, __u8 *otp)
{

	return 0;
}

__s32 _read_otp_info_hynix(__u32 chip, __u8 *otp_chip)
{

	return 0;
}

__s32 _get_rr_value_otp_hynix(__u32 nchip)
{

	return 0;
}

//for offset from defaul value
__s32 NFC_ReadRetry(__u32 chip, __u32 retry_count, __u32 read_retry_type)
{

	return 0;
}

__s32 NFC_ReadRetryInit(__u32 read_retry_type)
{


	return 0;
}

void NFC_GetOTPValue(__u32 chip, __u8* otp_value, __u32 read_retry_type)
{

}

__s32 NFC_GetDefaultParam(__u32 chip,__u8* default_value, __u32 read_retry_type)
{

    return 0;
}

__s32 NFC_SetDefaultParam(__u32 chip,__u8* default_value,__u32 read_retry_type)
{

	return 0;
}


__s32 NFC_ReadRetry_off(__u32 chip) //sandisk readretry exit
{

	return 0;
}

__s32 NFC_ReadRetry_Prefix_Sandisk_A19(void)
{

	return 0;
}

__s32 NFC_ReadRetry_Enable_Sandisk_A19(void)
{

	return 0;
}


__s32 NFC_DSP_ON_Sandisk_A19(void)
{

	return 0;
}

__s32 NFC_Test_Mode_Entry_Sandisk(void)
{

	return 0;
}

__s32 NFC_Test_Mode_Exit_Sandisk(void)
{

	return 0;
}

__s32 NFC_Change_LMFLGFIX_NEXT_Sandisk(__u8 para)
{


	return 0;
}



__s32 NFC_ReadRetryExit(__u32 read_retry_type)
{
	return 0;
}


void NFC_RbIntEnable(void)
{
    //enable interrupt
	//NFC_WRITE_REG(NFC_REG_INT, NFC_READ_REG(NFC_REG_INT)|NFC_B2R_INT_ENABLE);
	NDFC_WRITE_REG_INT(NDFC_B2R_INT_ENABLE);
}

void NFC_RbIntDisable(void)
{
    //disable rb interrupt
	//NFC_WRITE_REG(NFC_REG_INT, NFC_READ_REG(NFC_REG_INT)&(~NFC_B2R_INT_ENABLE));
	NDFC_WRITE_REG_INT(0);
}

void NFC_RbIntClearStatus(void)
{
    //clear interrupt
	NDFC_WRITE_REG_ST(NDFC_RB_B2R);
}

__u32 NFC_RbIntGetStatus(void)
{
    //clear interrupt
	return (NDFC_READ_REG_ST()&NDFC_RB_B2R);
}

__u32 NFC_RbIntOccur(void)
{
	return ((NDFC_READ_REG_ST()&NDFC_RB_B2R)&&(NDFC_READ_REG_INT()&NDFC_B2R_INT_ENABLE));
}

__u32 NFC_GetRbSelect(void)
{
    return (( NDFC_READ_REG_CTL() & NDFC_RB_SEL ) >>3);
}

__u32 NFC_GetRbStatus(__u32 rb)
{
    if(rb == 0)
        return (NDFC_READ_REG_ST() & NDFC_RB_STATE0);
    else if(rb == 1)
        return (NDFC_READ_REG_ST() & NDFC_RB_STATE1);
    else
        return 0;
}

void NFC_DmaIntEnable(void)
{
    //enable interrupt
	//NFC_WRITE_REG(NFC_REG_INT, NFC_READ_REG(NFC_REG_INT)|NFC_DMA_INT_ENABLE);
	NDFC_WRITE_REG_INT(NDFC_DMA_INT_ENABLE);

}


void NFC_DmaIntDisable(void)
{
    //disable dma interrupt
	//NFC_WRITE_REG(NFC_REG_INT, NFC_READ_REG(NFC_REG_INT)&(~NFC_DMA_INT_ENABLE));
	NDFC_WRITE_REG_INT(0);
}

void NFC_DmaIntClearStatus(void)
{
    //clear interrupt
	NDFC_WRITE_REG_ST(NDFC_DMA_INT_FLAG);
}

__u32 NFC_DmaIntGetStatus(void)
{
    //clear interrupt
	return ( NDFC_READ_REG_ST()&NDFC_DMA_INT_FLAG );
}

__u32 NFC_DmaIntOccur(void)
{
	return ((NDFC_READ_REG_ST()&NDFC_DMA_INT_FLAG) && (NDFC_READ_REG_INT()&NDFC_DMA_INT_ENABLE));
}

//fix a80 brom tlc->slc mode  bug
__s32 NFC_samsung_slc_mode_exit(void)
{
	return 0;
}




