/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "../include/nfc.h"

__u32 NandIOBase[2] = {0, 0};
__u32 NandIndex = 0;
__u32 nand_reg_address = 0;
__u32	nand_board_version = 0;
__u32 	pagesize = 0;

volatile __u32 irq_value = 0;


__u8 read_retry_reg_adr[READ_RETRY_MAX_REG_NUM] = {0};
__u8 read_retry_default_val[2][MAX_CHIP_SELECT_CNT][READ_RETRY_MAX_REG_NUM] = {0};
__s16 read_retry_val[READ_RETRY_MAX_CYCLE][READ_RETRY_MAX_REG_NUM] = {0};
__u8 hynix_read_retry_otp_value[2][MAX_CHIP_SELECT_CNT][8][8] = {0};
__u8 read_retry_mode = {0};
__u8 read_retry_cycle = {0};
__u8 read_retry_reg_num = {0};

const __s16 para0[6][4] = {	0
					};
const __s16 para1[6][4] = {	0
					};

const   __s16 para0x10[5] = {0};

const __s16 para0x11[7][5] = {0
					};
const   __s16 para0x20[15][4] ={0
	                    };

const	__s16 param0x30low[16][2] ={0
									};
const	__s16 param0x30high[20][2] ={0
									};
const	__s16 param0x31[9][3] =	   {0
									};
const	__s16 param0x32[19][4] ={0
									};

const	__s16 param0x40[10] = {0};

const	__s16 param0x50[7] = {0};
__u32 ddr_param[8] = {0};

static void dumphex32(char* name, char* base, int len)
{
	__u32 i;

	PRINT("dump %s registers:", name);
	for (i=0; i<len*4; i+=4) {
		if (!(i&0xf))
			PRINT("\n0x%p : ", base + i);
		PRINT("0x%08x ", *((volatile unsigned int *)(base + i)));
	}
	PRINT("\n");
}

void NFC_InitDDRParam(__u32 chip, __u32 param)
{
    if(chip<8)
        ddr_param[chip] = param;
}

void nfc_repeat_mode_enable(void)
{
    __u32 reg_val;


	reg_val = NFC_READ_REG(NFC_REG_CTL);
	if(((reg_val>>18)&0x3)>1)   //ddr type
	{
    	reg_val |= 0x1<<20;
    	NFC_WRITE_REG(NFC_REG_CTL, reg_val);
    }

}

void nfc_repeat_mode_disable(void)
{
    __u32 reg_val;

    reg_val = NFC_READ_REG(NFC_REG_CTL);
	if(((reg_val>>18)&0x3)>1)   //ddr type
	{
    	reg_val &= (~(0x1<<20));
    	NFC_WRITE_REG(NFC_REG_CTL, reg_val);
    }
}

/*******************wait nfc********************************************/
__s32 _wait_cmdfifo_free(void)
{
	__s32 timeout = 0xffff;

	while ( (timeout--) && (NFC_READ_REG(NFC_REG_ST) & NFC_CMD_FIFO_STATUS) );
	if (timeout <= 0)
	{
	    PRINT("nand _wait_cmdfifo_free time out, status:0x%x\n", NFC_READ_REG(NFC_REG_ST));

      	dumphex32("nand0 reg", (char*)0xf1c03000, 60);

      	dumphex32("nand1 reg", (char*)0xf1c05000, 60);

      	dumphex32("gpio reg", (char*)0xf1c20848, 50);


		return -ERR_TIMEOUT;
    }
	return 0;
}

__s32 _wait_cmd_finish(void)
{
	__s32 timeout = 0xffff;
	while( (timeout--) && !(NFC_READ_REG(NFC_REG_ST) & NFC_CMD_INT_FLAG) );
	if (timeout <= 0)
	{
	    PRINT("nand _wait_cmd_finish time out, NandIndex %d, status:0x%x\n", (__u32)NandIndex, NFC_READ_REG(NFC_REG_ST));

      	dumphex32("nand0 reg", (char*)0xf1c03000, 60);

      	dumphex32("nand1 reg", (char*)0xf1c05000, 60);

      	dumphex32("gpio reg", (char*)0xf1c20848, 50);

        return -ERR_TIMEOUT;
   }
	NFC_WRITE_REG(NFC_REG_ST, NFC_READ_REG(NFC_REG_ST) & NFC_CMD_INT_FLAG);
	return 0;
}

__u32 nand_dma_addr[2] = {0, 0};
void _dma_config_start(__u8 rw, __u32 buff_addr, __u32 len)
{
	__u32 reg_val;

	NAND_CleanFlushDCacheRegion(buff_addr, len);

	nand_dma_addr[NandIndex] = NAND_DMASingleMap(rw, buff_addr, len);

	//set mbus dma mode
	reg_val = NFC_READ_REG(NFC_REG_CTL);
	reg_val &= (~(0x1<<15));
	NFC_WRITE_REG(NFC_REG_CTL, reg_val);
	NFC_WRITE_REG(NFC_REG_MDMA_ADDR, nand_dma_addr[NandIndex]);
	NFC_WRITE_REG(NFC_REG_DMA_CNT, len);
}

__s32 _wait_dma_end(__u8 rw, __u32 buff_addr, __u32 len)
{

	__s32 timeout = 0xffffff;

	while ( (timeout--) && (!(NFC_READ_REG(NFC_REG_ST) & NFC_DMA_INT_FLAG)) );
	if (timeout <= 0)
	{
	    PRINT("nand _wait_dma_end time out, NandIndex: 0x%x, rw: 0x%x, status:0x%x\n", NandIndex, (__u32)rw, NFC_READ_REG(NFC_REG_ST));
		PRINT("DMA addr: 0x%x, DMA len: 0x%x\n", NFC_READ_REG(NFC_REG_MDMA_ADDR), NFC_READ_REG(NFC_REG_DMA_CNT));

      	dumphex32("nand0 reg", (char*)0xf1c03000, 60);

      	dumphex32("nand1 reg", (char*)0xf1c05000, 60);

      	dumphex32("gpio reg", (char*)0xf1c20848, 50);

		return -ERR_TIMEOUT;
    }

	NFC_WRITE_REG(NFC_REG_ST, NFC_READ_REG(NFC_REG_ST) & NFC_DMA_INT_FLAG);

    	NAND_DMASingleUnmap(rw, nand_dma_addr[NandIndex], len);

	return 0;
}

__s32 _reset(void)
{
	__u32 cfg;

	__s32 timeout = 0xffff;

	/*reset NFC*/
	cfg = NFC_READ_REG(NFC_REG_CTL);
	cfg |= NFC_RESET;
	NFC_WRITE_REG(NFC_REG_CTL, cfg);
	//waiting reset operation end
	while((timeout--) && (NFC_READ_REG(NFC_REG_CTL) & NFC_RESET));
	if (timeout <= 0)
	{
	    PRINT("nand _reset time out, status:0x%x\n", NFC_READ_REG(NFC_REG_ST));
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
	__u32 ecc_cnt_w[4];
	__u8 *ecc_cnt;
	__u8 ecc_tab[9] = {16, 24, 28, 32, 40, 48, 56, 60, 64};

	ecc_mode = (NFC_READ_REG(NFC_REG_ECC_CTL)>>12)&0xf;
	max_ecc_bit_cnt = ecc_tab[ecc_mode];

	//check ecc errro
	cfg = NFC_READ_REG(NFC_REG_ECC_ST)&0xffff;
	for (i = 0; i < eblock_cnt; i++)
	{
		if (cfg & (1<<i))
			return -ERR_ECC;
	}

    //check ecc limit
    ecc_cnt_w[0]= NFC_READ_REG(NFC_REG_ECC_CNT0);
    ecc_cnt_w[1]= NFC_READ_REG(NFC_REG_ECC_CNT1);
    ecc_cnt_w[2]= NFC_READ_REG(NFC_REG_ECC_CNT2);
    ecc_cnt_w[3]= NFC_READ_REG(NFC_REG_ECC_CNT3);

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
	__u32 cfg = NFC_READ_REG(NFC_REG_ECC_CTL);
	cfg &= ( (~NFC_ECC_EN)&0xffffffff );
	NFC_WRITE_REG(NFC_REG_ECC_CTL, cfg);
}

void _enable_ecc(__u32 pipline)
{
	__u32 cfg = NFC_READ_REG(NFC_REG_ECC_CTL);
	if (pipline ==1 )
		cfg |= NFC_ECC_PIPELINE;
	else
		cfg &= ((~NFC_ECC_PIPELINE)&0xffffffff);


	/*after erased, all data is 0xff, but ecc is not 0xff,
			so ecc asume it is right*/
	//if random open, disable exception
	if(cfg&(0x1<<9))
	    cfg &= (~(0x1<<4));
	else
	    cfg |= (1 << 4);

	//cfg |= (1 << 1); 16 bit ecc

	cfg |= NFC_ECC_EN;
	NFC_WRITE_REG(NFC_REG_ECC_CTL, cfg);
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

	for (i = 0; i < cnt; i++){
		if (i < 4)
			addr_low |= (addr[i] << (i*8) );
		else
			addr_high |= (addr[i] << ((i - 4)*8));
	}

	NFC_WRITE_REG(NFC_REG_ADDR_LOW, addr_low);
	NFC_WRITE_REG(NFC_REG_ADDR_HIGH, addr_high);
}

__s32 _read_in_page_mode(NFC_CMD_LIST  *rcmd,void *mainbuf,void *sparebuf,__u8 dma_wait_mode)
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
	NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) | NFC_RAM_METHOD);

	/*set dma and run*/
	_dma_config_start(0, (__u32)mainbuf, pagesize);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NFC_WRITE_REG(NFC_REG_CNT,1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NFC_WRITE_REG(NFC_REG_RCMD_SET, cfg);

	/*set NFC_REG_SECTOR_NUM*/
	NFC_WRITE_REG(NFC_REG_SECTOR_NUM, pagesize/1024);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NFC_SEND_ADR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command

	if (pagesize/1024 == 1)
		cfg |= NFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NFC_WRITE_REG(NFC_REG_CMD,cfg);
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
	for (i = 0; i < pagesize/1024;  i++){
		*(((__u32*) sparebuf)+i) = NFC_READ_REG(NFC_REG_USER_DATA(i));
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
	NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) | NFC_RAM_METHOD);

	/*set dma and run*/
	_dma_config_start(0, (__u32)mainbuf, pagesize);

	/*wait cmd fifo free*/
	ret = _wait_cmdfifo_free();
	if (ret)
		return ret;

	/*set NFC_REG_CNT*/
	NFC_WRITE_REG(NFC_REG_CNT,1024);

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (read_data_cmd & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NFC_WRITE_REG(NFC_REG_RCMD_SET, cfg);

	/*set NFC_REG_SECTOR_NUM*/
	NFC_WRITE_REG(NFC_REG_SECTOR_NUM, pagesize/1024);

	/*set addr*/
	_set_addr(read_addr_cmd->addr,read_addr_cmd->addr_cycle);

	/*set NFC_REG_CMD*/
	cfg  = 0;
	cfg |= read_addr_cmd->value;
	/*set sequence mode*/
	//cfg |= 0x1<<25;
	cfg |= ( (read_addr_cmd->addr_cycle - 1) << 16);
	cfg |= (NFC_SEND_ADR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
	cfg |= ((__u32)0x2 << 30);//page command

	if (pagesize/1024 == 1)
		cfg |= NFC_SEQ;

	/*enable ecc*/
	_enable_ecc(1);
	NFC_WRITE_REG(NFC_REG_CMD,cfg);
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
		*(((__u32*) sparebuf)+i) = NFC_READ_REG(NFC_REG_USER_DATA(i));
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
	for (i = 0; i < pagesize/1024;  i++){
		*(((__u32*) sparebuf)+i) = NFC_READ_REG(NFC_REG_USER_DATA(i));
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

__u32 _check_secbitmap_bit(__u32 secbitmap, __u32 firstbit, __u32 validbit)
{
	__u32 i = 0;

    for(i=firstbit; i<firstbit+validbit; i++)
    {
        if(!(secbitmap&(0x1<<i)))
        {
            PRINT("secbitmap 0x%x not seq!\n", secbitmap);
            while(1);
        }
    }

	return 0;
}

__s32 _read_secs_in_page_mode(NFC_CMD_LIST  *rcmd,void *mainbuf, void *cachebuf, void *sparebuf, __u32 secbitmap)
{
	__s32 ret,ret1, ecc_flag = 0;
	__s32 i, j, k;
	__u32 cfg;
	NFC_CMD_LIST *cur_cmd,*read_addr_cmd;
	__u32 read_data_cmd,random_read_cmd0,random_read_cmd1;
	__u32 eccblkstart, eccblkcnt, eccblksecmap;
	__u32 spareoffsetbak;
	__u8  spairsizetab[9] = {32, 46, 54, 60, 74, 88, 102, 110, 116};
	__u32 ecc_mode, spare_size;
	__u8  addr[2] = {0, 0};
	__u32 col_addr;
	__u32 eccblk_zone_list[4][3]={0}; //[0]: starteccblk; [1]: eccblkcnt; [2]: secbitmap;
	__u32 eccblk_zone_cnt = 0;
	__u32 firstbit, validbit;
	__u32 dma_buf;
	__u32 *oobbuf;

	if(sparebuf)
	{
	    oobbuf = (__u32 *)sparebuf;
	    oobbuf[0] = 0x12345678;
	    oobbuf[1] = 0x12345678;
	}

	firstbit = _cal_first_valid_bit(secbitmap);
	validbit = _cal_valid_bits(secbitmap);

	ecc_mode = (NFC_READ_REG(NFC_REG_ECC_CTL)>>12)&0xf;
	spare_size = spairsizetab[ecc_mode];


	/*set NFC_REG_CNT*/
	NFC_WRITE_REG(NFC_REG_CNT,1024);


	read_addr_cmd = rcmd;
	cur_cmd = rcmd;
	random_read_cmd0 = cur_cmd->value;
	cur_cmd = cur_cmd->next;
	random_read_cmd1 = cur_cmd->value;

	/*set NFC_REG_RCMD_SET*/
	cfg = 0;
	cfg |= (random_read_cmd1 & 0xff);
	cfg |= ((random_read_cmd0 & 0xff) << 8);
	cfg |= ((random_read_cmd1 & 0xff) << 16);
	NFC_WRITE_REG(NFC_REG_RCMD_SET, cfg);

	//access NFC internal RAM by DMA bus
	NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) | NFC_RAM_METHOD);

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
		PRINT("read sectors: bitmap: 0x%x, mainbuf: 0x%x, sparebuf: 0x%x\n", secbitmap, mainbuf, sparebuf);
		for(j=0;j<eccblk_zone_cnt; j++)
		{
			PRINT("  %d, eccblkstart: %d, eccblkcnt: %d \n", j, eccblk_zone_list[j][0], eccblk_zone_list[j][1], eccblk_zone_list[j][2]);
			PRINT("     eccblkmap: 0x%x,\n", eccblk_zone_list[j][2]);
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

			spareoffsetbak = NFC_READ_REG(NFC_REG_SPARE_AREA);
			NFC_WRITE_REG(NFC_REG_SPARE_AREA, pagesize + spare_size*eccblkstart);
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
			    PRINT(" _read_secs_in_page_mode error, NK mode, cmdfifo full \n");
				while(1);
				return ret;
            }
			/*set NFC_REG_SECTOR_NUM*/
			NFC_WRITE_REG(NFC_REG_SECTOR_NUM, eccblkcnt);

			/*set addr*/
			_set_addr(addr,2);

			/*set NFC_REG_CMD*/
			cfg  = 0;
			cfg |= random_read_cmd0;
			/*set sequence mode*/
			//cfg |= 0x1<<25;
			cfg |= ( (rcmd->addr_cycle - 1) << 16);
			cfg |= (NFC_SEND_ADR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
			cfg |= ((__u32)0x2 << 30);//page command

			/*enable ecc*/
			_enable_ecc(1);
			NFC_WRITE_REG(NFC_REG_CMD,cfg);

			NAND_WaitDmaFinish();
			/*if dma mode is wait*/
			if(1){
				ret1 = _wait_dma_end(0, (__u32)mainbuf + eccblkstart*1024, eccblkcnt*1024);
				if (ret1)
				{
				    PRINT(" _read_secs_in_page_mode error, NK mode, dma not finish \n");
				    while(1);
					return ret1;
				}
			}

			/*wait cmd fifo free and cmd finish*/
			ret = _wait_cmdfifo_free();
			ret |= _wait_cmd_finish();
			if (ret){
				_disable_ecc();
				NFC_WRITE_REG(NFC_REG_SPARE_AREA, spareoffsetbak);
				//PRINT(" _read_secs_in_page_mode error, NK mode, cmd not finish \n");
				while(1);
				return ret;
			}
			/*get user data*/
			if(sparebuf)
			{
			    for (i = 0; i < eccblkcnt;  i++){
			        if(oobbuf[i%2] == 0x12345678)
			            oobbuf[i%2] = NFC_READ_REG(NFC_REG_USER_DATA(i));
    			}
			}


			/*ecc check and disable ecc*/
			ret = _check_ecc(eccblkcnt);
			ecc_flag |= ret;
			_disable_ecc();
			NFC_WRITE_REG(NFC_REG_SPARE_AREA, spareoffsetbak);


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

				spareoffsetbak = NFC_READ_REG(NFC_REG_SPARE_AREA);
				NFC_WRITE_REG(NFC_REG_SPARE_AREA, pagesize + spare_size*(eccblkstart+k));
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
				    PRINT(" _read_secs_in_page_mode error, 1K mode, cmdfifo full \n");
				    while(1);
					return ret;
	                }
				/*set NFC_REG_SECTOR_NUM*/
				NFC_WRITE_REG(NFC_REG_SECTOR_NUM, 1);

				/*set addr*/
				_set_addr(addr,2);

				/*set NFC_REG_CMD*/
				cfg  = 0;
				cfg |= random_read_cmd0;
				/*set sequence mode*/
				//cfg |= 0x1<<25;
				cfg |= ( (rcmd->addr_cycle - 1) << 16);
				cfg |= (NFC_SEND_ADR | NFC_DATA_TRANS | NFC_SEND_CMD1 | NFC_SEND_CMD2 | NFC_WAIT_FLAG | NFC_DATA_SWAP_METHOD);
				cfg |= ((__u32)0x2 << 30);//page command

				/*enable ecc*/
				_enable_ecc(1);
				NFC_WRITE_REG(NFC_REG_CMD,cfg);

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
					    PRINT(" _read_secs_in_page_mode error, 1K mode, dma not finish \n");
				        while(1);
						return ret1;
					}
				}

				/*wait cmd fifo free and cmd finish*/
				ret = _wait_cmdfifo_free();
				ret |= _wait_cmd_finish();
				if (ret){
					_disable_ecc();
					NFC_WRITE_REG(NFC_REG_SPARE_AREA, spareoffsetbak);
			        PRINT(" _read_secs_in_page_mode error, 1K mode, cmd not finish \n");
				    while(1);
					return ret;
				}
				/*get user data*/
				if(sparebuf)
	        		{
	    		        if(oobbuf[(eccblkstart+k)%2] == 0x12345678)
	    		            oobbuf[(eccblkstart+k)%2] = NFC_READ_REG(NFC_REG_USER_DATA(0));
	        		}

				/*ecc check and disable ecc*/
				ret = _check_ecc(1);
				ecc_flag |= ret;
				_disable_ecc();
				NFC_WRITE_REG(NFC_REG_SPARE_AREA, spareoffsetbak);


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

	NFC_WRITE_REG(NFC_REG_RCMD_SET, 0x003005e0);

	return ecc_flag;
}


/*******************************************************************************
*								NFC_Read
*
* Description 	: read some sectors data from flash in single plane mode.
* Arguments	: *rcmd	-- the read command sequence list head¡£
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
	NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) & (~NFC_RAM_METHOD));

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
	NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) & (~NFC_RAM_METHOD));

	_exit_nand_critical();


	return ret;
}

__s32 NFC_ReadSecs(NFC_CMD_LIST  *rcmd, void *mainbuf,  void *cachebuf, void *sparebuf,__u32 secbitmap )
{

	__s32 ret ;

	_enter_nand_critical();

	PRINT("[NAND ERROR] function --NFC_ReadSecs-- is called!\n");

	ret = _read_secs_in_page_mode(rcmd, mainbuf,cachebuf, sparebuf, secbitmap);

	/*switch to ahb*/
	NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) & (~NFC_RAM_METHOD));

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
			cfg |= NFC_SEND_ADR;
		}

		/*set NFC_REG_CMD*/
		/*set cmd value*/
		cfg |= cur_cmd->value;
		/*set sequence mode*/
		//cfg |= 0x1<<25;
		/*wait rb?*/
		if (cur_cmd->wait_rb_flag){
			cfg |= NFC_WAIT_FLAG;
		}
		if (cur_cmd->data_fetch_flag){
			NFC_WRITE_REG(NFC_REG_CTL, (NFC_READ_REG(NFC_REG_CTL)) & (~NFC_RAM_METHOD));
			cfg |= NFC_DATA_TRANS;
			NFC_WRITE_REG(NFC_REG_CNT, cur_cmd->bytecnt);
		}
		/*send command*/
		cfg |= NFC_SEND_CMD1;
		NFC_WRITE_REG(NFC_REG_CMD, cfg);
		cur_cmd = cur_cmd ->next;
	}
	return 0;
}

__s32 NFC_SetRandomSeed(__u32 random_seed)
{
	__u32 cfg;


	  cfg = NFC_READ_REG(NFC_REG_ECC_CTL);
	  cfg &= 0x0000ffff;
	  cfg |= (random_seed<<16);
	  NFC_WRITE_REG(NFC_REG_ECC_CTL,cfg);

	return 0;
}

__s32 NFC_RandomEnable(void)
{
	__u32 cfg;


	cfg = NFC_READ_REG(NFC_REG_ECC_CTL);
	cfg |= (0x1<<9);
	NFC_WRITE_REG(NFC_REG_ECC_CTL,cfg);


	return 0;
}

__s32 NFC_RandomDisable(void)
{
	__u32 cfg;


	cfg = NFC_READ_REG(NFC_REG_ECC_CTL);
	cfg &= (~(0x1<<9));
	NFC_WRITE_REG(NFC_REG_ECC_CTL,cfg);


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
		*(idbuf + i) = NFC_READ_RAM_B(NFC_RAM0_BASE+i);
	}

    nfc_repeat_mode_disable();

	_exit_nand_critical();
	return ret;
}

__s32 NFC_NormalCMD(NFC_CMD_LIST  *cmd_list)
{
	__u32 i;
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
	return (NFC_READ_RAM_B(NFC_RAM0_BASE));

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


    cfg = NFC_READ_REG(NFC_REG_CTL);
    cfg &= ( (~NFC_CE_SEL) & 0xffffffff);
    cfg |= ((chip & 0x7) << 24);
#if 0
    if(((read_retry_mode == 0)||(read_retry_mode == 1))&&(read_retry_cycle))
        cfg |= (0x1<<6);
#endif
    NFC_WRITE_REG(NFC_REG_CTL,cfg);

    if((cfg>>18)&0x3) //ddr nand
    {
        //set ddr param
        NFC_WRITE_REG(NFC_REG_TIMING_CTL,ddr_param[0]);
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


	  cfg = NFC_READ_REG(NFC_REG_CTL);
	  cfg &= ( (~NFC_RB_SEL) & 0xffffffff);
	  cfg |= ((rb & 0x1) << 3);
	  NFC_WRITE_REG(NFC_REG_CTL,cfg);

	  return 0;

}





__s32 NFC_DeSelectChip( __u32 chip)
{
#if 0
    __u32 cfg;

    if(((read_retry_mode == 0)||(read_retry_mode == 1))&&(read_retry_cycle))
    {
        cfg = NFC_READ_REG(NFC_REG_CTL);
        cfg &= (~(0x1<<6));
        NFC_WRITE_REG(NFC_REG_CTL,cfg);
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
	__u32 cfg = NFC_READ_REG(NFC_REG_ST);


	cfg &= (NFC_RB_STATE0 << (rb & 0x3));

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

* Returns		: 0 = sucess -1 = fail
* Notes		: NFC must be reset before seial access mode changes.
********************************************************************************/
__s32 NFC_ChangMode(NFC_INIT_INFO *nand_info )
{
	__u32 cfg;

	pagesize = nand_info->pagesize * 512;

	/*reset nfc*/
	_reset();

	/*set NFC_REG_CTL*/
	cfg = 0;
	cfg |= NFC_EN;
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
	cfg |= ((nand_info->ddr_type & 0x3) << 18);   //set ddr type
	cfg |= ((nand_info->debug & 0x1) << 31);
	NFC_WRITE_REG(NFC_REG_CTL,cfg);

	/*set NFC_TIMING */
	cfg = 0;
	if((nand_info->ddr_type & 0x3) == 0)
	    cfg |=((nand_info->serial_access_mode & 0x1) & 0xf)<<8;
	else if((nand_info->ddr_type & 0x3) == 2)
	{
	    cfg |= 0x3f;
	    cfg |= 0x3<<8;
    }
    else if((nand_info->ddr_type & 0x3) == 3)
	{
	    cfg |= 0x1f;
	    cfg |= 0x2<<8;
	}
	NFC_WRITE_REG(NFC_REG_TIMING_CTL,cfg);
	NFC_WRITE_REG(NFC_REG_TIMING_CFG,0xff);
	/*set NFC_SPARE_AREA */
	NFC_WRITE_REG(NFC_REG_SPARE_AREA, pagesize);

	return 0;
}

__s32 NFC_SetEccMode(__u8 ecc_mode)
{
    __u32 cfg = NFC_READ_REG(NFC_REG_ECC_CTL);


    cfg &=	((~NFC_ECC_MODE)&0xffffffff);
    cfg |= (NFC_ECC_MODE & (ecc_mode<<12));

	NFC_WRITE_REG(NFC_REG_ECC_CTL, cfg);

	return 0;
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
	    PRINT("[NAND] nan d driver(A%d) version: 0x%x, 0x%x\n",PLATFORM, NAND_VERSION_0, NAND_VERSION_1);
		PRINT("data: %x %d\n",NAND_DRV_DATE,TIME);

	}

    //init ddr_param
    for(i=0;i<8;i++)
        ddr_param[i] = 0x21f;

    NandIOBase[0] = (__u32)NAND_IORemap(NAND_IO_BASE_ADDR0, 4096);
    NandIOBase[1] = (__u32)NAND_IORemap(NAND_IO_BASE_ADDR1, 4096);

    //init pin
    NAND_PIORequest(NandIndex);

    //init clk
    NAND_ClkRequest(NandIndex);
    NAND_SetClk(NandIndex, 10,20);


    //init dma
	NFC_SetEccMode(0);

	/*init nand control machine*/
	ret = NFC_ChangMode( nand_info);

	/*request special dma*/

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
	cfg = NFC_READ_REG(NFC_REG_CTL);
	cfg &= ( (~NFC_EN) & 0xffffffff);
	NFC_WRITE_REG(NFC_REG_CTL,cfg);

	 //init clk
    NAND_ClkRelease(NandIndex);

    //init pin
    NAND_PIORelease(NandIndex);

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

__s32 NFC_ReadRetry_0x32_UpperPage(void)
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
	NFC_WRITE_REG(NFC_REG_INT, NFC_B2R_INT_ENABLE);
}

void NFC_RbIntDisable(void)
{
    //disable rb interrupt
	//NFC_WRITE_REG(NFC_REG_INT, NFC_READ_REG(NFC_REG_INT)&(~NFC_B2R_INT_ENABLE));
	NFC_WRITE_REG(NFC_REG_INT, 0);
}

void NFC_RbIntClearStatus(void)
{
    //clear interrupt
	NFC_WRITE_REG(NFC_REG_ST,NFC_RB_B2R);
}

__u32 NFC_RbIntGetStatus(void)
{
    //clear interrupt
	return (NFC_READ_REG(NFC_REG_ST)&NFC_RB_B2R);
}

__u32 NFC_RbIntOccur(void)
{
	return ((NFC_READ_REG(NFC_REG_ST)&NFC_RB_B2R)&&(NFC_READ_REG(NFC_REG_INT)&NFC_B2R_INT_ENABLE));
}

__u32 NFC_GetRbSelect(void)
{
    return (( NFC_READ_REG(NFC_REG_CTL) & NFC_RB_SEL ) >>3);
}

__u32 NFC_GetRbStatus(__u32 rb)
{
    if(rb == 0)
        return (NFC_READ_REG(NFC_REG_ST) & NFC_RB_STATE0);
    else if(rb == 1)
        return (NFC_READ_REG(NFC_REG_ST) & NFC_RB_STATE1);
    else
        return 0;
}

void NFC_DmaIntEnable(void)
{
    //enable interrupt
	//NFC_WRITE_REG(NFC_REG_INT, NFC_READ_REG(NFC_REG_INT)|NFC_DMA_INT_ENABLE);
	NFC_WRITE_REG(NFC_REG_INT, NFC_DMA_INT_ENABLE);

}


void NFC_DmaIntDisable(void)
{
    //disable dma interrupt
	//NFC_WRITE_REG(NFC_REG_INT, NFC_READ_REG(NFC_REG_INT)&(~NFC_DMA_INT_ENABLE));
	NFC_WRITE_REG(NFC_REG_INT, 0);
}

void NFC_DmaIntClearStatus(void)
{
    //clear interrupt
	NFC_WRITE_REG(NFC_REG_ST,NFC_DMA_INT_FLAG);
}

__u32 NFC_DmaIntGetStatus(void)
{
    //clear interrupt
	return (NFC_READ_REG(NFC_REG_ST)&NFC_DMA_INT_FLAG);
}

__u32 NFC_DmaIntOccur(void)
{
	return ((NFC_READ_REG(NFC_REG_ST)&NFC_DMA_INT_FLAG)&&(NFC_READ_REG(NFC_REG_INT)&NFC_DMA_INT_ENABLE));
}





