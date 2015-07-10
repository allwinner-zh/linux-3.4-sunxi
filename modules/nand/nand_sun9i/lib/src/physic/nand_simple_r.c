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
#include "../include/nand_drv_cfg.h"
//#include "nand_physic_interface.h"


struct __NandStorageInfo_t NandStorageInfo = {0};
NFC_DDR_INFO NandDdrInfo = {0};
struct __NandPageCachePool_t PageCachePool = {0};
__u32 RetryCount[2][MAX_CHIP_SELECT_CNT] = {{0}, {0}};
const __u16 random_seed[128] = {
    //0        1      2       3        4      5        6       7       8       9
	0x2b75, 0x0bd0, 0x5ca3, 0x62d1, 0x1c93, 0x07e9, 0x2162, 0x3a72, 0x0d67, 0x67f9,
    0x1be7, 0x077d, 0x032f, 0x0dac, 0x2716, 0x2436, 0x7922, 0x1510, 0x3860, 0x5287,
    0x480f, 0x4252, 0x1789, 0x5a2d, 0x2a49, 0x5e10, 0x437f, 0x4b4e, 0x2f45, 0x216e,
    0x5cb7, 0x7130, 0x2a3f, 0x60e4, 0x4dc9, 0x0ef0, 0x0f52, 0x1bb9, 0x6211, 0x7a56,
    0x226d, 0x4ea7, 0x6f36, 0x3692, 0x38bf, 0x0c62, 0x05eb, 0x4c55, 0x60f4, 0x728c,
    0x3b6f, 0x2037, 0x7f69, 0x0936, 0x651a, 0x4ceb, 0x6218, 0x79f3, 0x383f, 0x18d9,
    0x4f05, 0x5c82, 0x2912, 0x6f17, 0x6856, 0x5938, 0x1007, 0x61ab, 0x3e7f, 0x57c2,
    0x542f, 0x4f62, 0x7454, 0x2eac, 0x7739, 0x42d4, 0x2f90, 0x435a, 0x2e52, 0x2064,
    0x637c, 0x66ad, 0x2c90, 0x0bad, 0x759c, 0x0029, 0x0986, 0x7126, 0x1ca7, 0x1605,
    0x386a, 0x27f5, 0x1380, 0x6d75, 0x24c3, 0x0f8e, 0x2b7a, 0x1418, 0x1fd1, 0x7dc1,
    0x2d8e, 0x43af, 0x2267, 0x7da3, 0x4e3d, 0x1338, 0x50db, 0x454d, 0x764d, 0x40a3,
    0x42e6, 0x262b, 0x2d2e, 0x1aea, 0x2e17, 0x173d, 0x3a6e, 0x71bf, 0x25f9, 0x0a5d,
    0x7c57, 0x0fbe, 0x46ce, 0x4939, 0x6b17, 0x37bb, 0x3e91, 0x76db
};

__u32 ndfc_dma_desc_addr = 0;
__u32 ndfc_dma_desc_cpu_addr = 0;
_ndfc_dma_desc_t *ndfc_dma_desc = (void *)0;
_ndfc_dma_desc_t *ndfc_dma_desc_cpu = (void *)0;

extern __u32 ddr_param[8];
__u32 ddr_scan_blk_no[MAX_CHIP_SELECT_CNT] = {0};

/**************************************************************************
************************* add one cmd to cmd list******************************
****************************************************************************/
void _add_cmd_list(NFC_CMD_LIST *cmd,__u32 value,__u32 addr_cycle,__u8 *addr,__u8 data_fetch_flag,
					__u8 main_data_fetch,__u32 bytecnt,__u8 wait_rb_flag)
{
	cmd->addr = addr;
	cmd->addr_cycle = addr_cycle;
	cmd->data_fetch_flag = data_fetch_flag;
	cmd->main_data_fetch = main_data_fetch;
	cmd->bytecnt = bytecnt;
	cmd->value = value;
	cmd->wait_rb_flag = wait_rb_flag;
	cmd->next = NULL;
}

/****************************************************************************
*********************translate (block + page+ sector) into 5 bytes addr***************
*****************************************************************************/
void _cal_addr_in_chip(__u32 block, __u32 page, __u32 sector,__u8 *addr, __u8 cycle)
{
	__u32 row;
	__u32 column;

	column = 512 * sector;
	row = block * PAGE_CNT_OF_PHY_BLK + page;

	switch(cycle) {
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

__u8 _cal_real_chip(__u32 global_bank)
{
	__u8 chip = 0;

	if((RB_CONNECT_MODE == 0)&&(global_bank<=2))
	{
	    if(global_bank)
	    	chip = 7;
	    else
	    	chip = 0;

	    return chip;
	}
	if((RB_CONNECT_MODE == 1)&&(global_bank<=1))
	{
		chip = global_bank;
		return chip;
	}
	if((RB_CONNECT_MODE == 2)&&(global_bank<=2))
	{
		chip = global_bank;
		return chip;
	}
	if((RB_CONNECT_MODE == 3)&&(global_bank<=2))
	{
		chip = global_bank*2;
		return chip;
	}
	if((RB_CONNECT_MODE == 4)&&(global_bank<=4))
	{
		switch(global_bank){
		case 0:
			chip = 0;
			break;
		case 1:
			chip = 2;
			break;
		case 2:
			chip = 1;
			break;
		case 3:
			chip = 3;
			break;
		default :
			chip =0;
			break;
		}

		return chip;
	}
	if((RB_CONNECT_MODE == 5)&&(global_bank<=4))
	{
		chip = global_bank*2;

		return chip;
	}
	if((RB_CONNECT_MODE == 8)&&(global_bank<=8))
	{
		switch(global_bank){
		case 0:
			chip = 0;
			break;
		case 1:
			chip = 2;
			break;
		case 2:
			chip = 1;
			break;
		case 3:
			chip = 3;
			break;
		case 4:
			chip = 4;
			break;
		case 5:
			chip = 6;
			break;
		case 6:
			chip = 5;
			break;
		case 7:
			chip = 7;
			break;
		default :
			chip =0;
			break;
		}

		return chip;
	}

	PHY_ERR("wrong chip number ,rb_mode = %d, bank = %d, chip = %d, chip info = %x\n",RB_CONNECT_MODE, global_bank, chip, CHIP_CONNECT_INFO);

	return 0xff;
}

__u8 _cal_real_rb(__u32 chip)
{
	__u8 rb;

	rb = 0;
	if(RB_CONNECT_MODE == 0)
	{
		rb = 0;
	}
	if(RB_CONNECT_MODE == 1)
	{
		rb = chip;
	}
	if(RB_CONNECT_MODE == 2)
	{
		rb = chip;
	}
	if(RB_CONNECT_MODE == 3)
	{
		rb = chip/2;
	}
	if(RB_CONNECT_MODE == 4)
	{
		rb = chip/2;
	}
	if(RB_CONNECT_MODE == 5)
	{
		rb = (chip/2)%2;
	}
	if(RB_CONNECT_MODE == 8)
	{
		rb = (chip/2)%2;
	}

	if((rb!=0)&&(rb!=1))
	{
	    PHY_ERR("wrong Rb connect Mode, chip = %d ,RbConnectMode = %d \n",chip,RB_CONNECT_MODE);
	    return 0xff;
	}

	return rb;
}

/*******************************************************************
**********************get status**************************************
********************************************************************/
__s32 _read_status(__u32 cmd_value, __u32 nBank)
{
	/*get status*/
	__u8 addr[5];
	__u32 addr_cycle;
	NFC_CMD_LIST cmd_list;

	addr_cycle = 0;

	if(!(cmd_value == 0x70 || cmd_value == 0x71))
	{
        	/* not 0x70 or 0x71, need send some address cycle */
		 if(cmd_value == 0x78)
			addr_cycle = 3;
		 else
			addr_cycle = 1;
		_cal_addr_in_chip(nBank*BLOCK_CNT_OF_DIE,0,0,addr,addr_cycle);
	}
	_add_cmd_list(&cmd_list, cmd_value, addr_cycle, addr, 1,NDFC_IGNORE,1,NDFC_IGNORE);
	return (NFC_GetStatus(&cmd_list));

}

__s32 _phy_read_status(__u32 chip)
{
	__s32 ret = 0,status;
	__u32 rb;
	__u32 cmd_value;
	__s32 timeout = 0xffff;

	ret = 0;

	rb = _cal_real_rb(chip);

	cmd_value = 0x70;

	NFC_SelectChip(chip);
	NFC_SelectRb(rb);

	while(1){
		status = _read_status(cmd_value,chip);
		if (status < 0)
		{
		    PHY_ERR("read status invalid ,channel = %x, chip = %x, cmd value = %x, status = %x\n",NandIndex,chip,cmd_value,status);
		    return status;
		}
		if (status & NAND_STATUS_READY)
			break;

		if (timeout-- < 0){
			PHY_ERR("channel %d: wait nand ready timeout,chip = %x, cmd value = %x, status = %x\n",NandIndex,chip,cmd_value,status);
			return -ERR_TIMEOUT;
		}
	}

	NFC_DeSelectChip(chip);
	NFC_DeSelectRb(rb);

	return ret;
}


/********************************************************************
***************************wait rb ready*******************************
*********************************************************************/
__s32 _wait_rb_ready(__u32 chip)
{
	__s32 timeout = 0xffff;
	__u32 rb;

	rb = _cal_real_rb(chip);

	/*wait rb ready*/
	while((timeout--) && (NFC_CheckRbReady(rb)));
	if (timeout < 0)
	{
		PHY_ERR("_wait_rb_ready time out, ch: 0x%x\n", NandIndex);
		return -ERR_TIMEOUT;
	}

	return 0;
}


__s32 _wait_rb_ready_int(__u32 chip)
{
	__s32 timeout = 0xffff;
	__u32 rb;


	rb = _cal_real_rb(chip);
	NFC_SelectRb(rb);

	if(NFC_CheckRbReady(rb))
	{
		NAND_WaitRbReady();
	}

	/*wait rb ready*/
	while((timeout--) && (NFC_CheckRbReady(rb)));
	if (timeout < 0)
	{
		PHY_ERR("_wait_rb_ready_int time out, ch: 0x%x\n", NandIndex);
		return -ERR_TIMEOUT;
	}

	_phy_read_status(chip);

	return 0;
}


void _pending_dma_irq_sem(void)
{
	return;
}

void _random_seed_init(void)
{

}

__u32 _cal_random_seed(__u32 page)
{
	__u32 randomseed;

	randomseed = random_seed[page%128];

	return randomseed;
}

__s32 _set_nand_feature(__u32 addr, __u8 *p)
{
	__u8 addr_buf[5];
	__u32 addr_cycle;
	NFC_CMD_LIST cmd_list;
	__s32 ret;

	addr_cycle = 1;
	addr_buf[0] = addr;
	_add_cmd_list(&cmd_list, 0xEF, addr_cycle, addr_buf, NDFC_DATA_FETCH, NDFC_IGNORE, 4, NDFC_WAIT_RB);

	ret = NFC_SetFeature(&cmd_list, p);
	if (ret)
		return ret;
	PHY_DBG("set nand feature: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", addr, p[0], p[1], p[2], p[3]);
	return 0;
}

__s32 _get_nand_feature(__u32 addr, __u8 *p)
{
	__u8 addr_buf[5];
	__u32 addr_cycle;
	NFC_CMD_LIST cmd_list;
	__s32 ret;

	addr_cycle = 1;
	addr_buf[0] = addr;
	_add_cmd_list(&cmd_list, 0xEE, addr_cycle, addr_buf, NDFC_DATA_FETCH, NDFC_IGNORE, 4, NDFC_WAIT_RB);

	ret = NFC_GetFeature(&cmd_list, p);
	if (ret)
		return ret;
	PHY_DBG("get nand feature: 0x%x, 0x%x 0x%x 0x%x 0x%x\n", addr, p[0], p[1], p[2], p[3]);
	return 0;
}

__s32 _cal_nand_onfi_timing_mode(__u32 mode, __u32 dclk)
{

	return 0;
}

__s32 _setup_nand_onfi_ddr2_para(NFC_DDR_INFO ddr_info)
{

	return 0;
}

__s32 _setup_nand_onfi_driver_strength(NFC_DDR_INFO ddr_info)
{

	return 0;
}

__s32 _setup_nand_onfi_vendor_specific_feature(NFC_DDR_INFO nand_inf)
{


	return 0;
}

__s32 _change_nand_onfi_timing_mode(__u32 if_type, __u32 timing_mode)
{

	return 0;
}

__s32 _setup_nand_toggle_ddr2_para(NFC_DDR_INFO ddr_info)
{

	return 0;
}

__s32 _setup_nand_toggle_driver_strength(NFC_DDR_INFO ddr_info)
{

	return 0;
}

__s32 _setup_nand_toggle_vendor_specific_feature(NFC_DDR_INFO ddr_info)
{

	return 0;
}

void _setup_ndfc_ddr2_para(NFC_DDR_INFO ddr_info)
{

}

void _set_ndfc_def_timing_param(void)
{
	__u32 reg_val, sync_mode;

	reg_val = NDFC_READ_REG_CTL();
	sync_mode = (reg_val>>18) & 0x3;
	sync_mode |= (((reg_val>>28) & 0x1) <<4);

	if(sync_mode == 0) //async
	{
		reg_val = 0;
		reg_val |= 0x1<<16; //tCCS
		reg_val |= 0x0<<14; //tCLHZ
		reg_val |= 0x0<<12; //tCS
		reg_val |= 0x0<<11; //tCDQSS
		reg_val |= 0x1<<8;  //tCAD
		reg_val |= 0x0<<6;  //tRHW
		reg_val |= 0x0<<4;  //tWHR
		reg_val |= 0x0<<2;  //tADL
		reg_val |= 0x0; //<<0;  //tWB
	}
	else if(sync_mode == 2) //onfi ddr
	{
		reg_val = 0;
		reg_val |= 0x1<<16; //tCCS
		reg_val |= 0x0<<14; //tCLHZ
		reg_val |= 0x0<<12; //tCS
		reg_val |= 0x0<<11; //tCDQSS
		reg_val |= 0x1<<8;  //tCAD
		reg_val |= 0x1<<6;  //tRHW
		reg_val |= 0x0<<4;  //tWHR
		reg_val |= 0x0<<2;  //tADL
		reg_val |= 0x1; //<<0;  //tWB
	}
	else if(sync_mode == 3) //toggle ddr
	{
		reg_val = 0;
		reg_val |= 0x2<<16; //tCCS
		reg_val |= 0x0<<14; //tCLHZ
		reg_val |= 0x0<<12; //tCS
		reg_val |= 0x0<<11; //tCDQSS
		reg_val |= 0x4<<8;  //tCAD
		reg_val |= 0x0<<6;  //tRHW
		reg_val |= 0x0<<4;  //tWHR
		reg_val |= 0x2<<2;  //tADL
		reg_val |= 0x0; //<<0;  //tWB
	}
	else
	{
		//fault
		PHY_ERR("wrong interface , 0x%x\n", sync_mode);
	}

	NDFC_WRITE_REG_TIMING_CFG(reg_val);

	return ;
}

__s32 _check_scan_data(__u32 first_check, __u32 chip, __u32 *scan_good_blk_no)
{
	__s32 ret;
	__u8* main_buf = NULL;
	__u32 b, start_blk = 2, blk_cnt = 10;
	__u8  oob_buf[64];
	struct boot_physical_param readop;
	__u32 buf_size = 16384;

	main_buf = (__u8 *)MALLOC(buf_size);
	//PHY_ERR("_check_scan_data, main buf: 0x%x\n", main_buf);
	if (!main_buf) {
		PHY_ERR("_check_scan_data, main_buf 0x%x is null!\n", main_buf);
		return -1;
	}

	if (first_check)
	{
		for (b=start_blk; b<start_blk+blk_cnt; b++)
		{
		    readop.chip = chip;
		    readop.block = b;
		    readop.page = 0;
		    //readop.sectorbitmap = 0xff; //3;
		    readop.mainbuf = main_buf;
		    readop.oobbuf = oob_buf;

		    ret = PHY_SimpleRead_1st1K(&readop); //read 4KB~8KB
		    if (ret == 0) {
				if ((oob_buf[0]==0xff)&&(oob_buf[1]==0xff)&&(oob_buf[2]==0xff)&&(oob_buf[3]==0xff)) {
					ret = -1;
					PHY_DBG("_check_scan_data, it is a free page(type 0), block %d\n", b);
				} else if ((oob_buf[0]==0x0)&&(oob_buf[1]==0x0)&&(oob_buf[2]==0x0)&&(oob_buf[3]==0x0)) {
					ret = -1;
					PHY_DBG("_check_scan_data, bad block %d !\n", b);
				} else {
					ret = 0;
					PHY_DBG("_check_scan_data, ok, block %d\n", b);

					break;
				}
		    } else {
				#if 1
				if ((oob_buf[0]==0xff)&&(oob_buf[1]==0xff)&&(oob_buf[2]==0xff)&&(oob_buf[3]==0xff)) {
					ret = -1;
					PHY_DBG("_check_scan_data, it is a free page(type 1), block %d\n", b);
				}
				#endif
				PHY_DBG("_check_scan_data, read error, block %d !\n", b);
				ret = -1;
			}
		}

		if (ret == 0) {
			scan_good_blk_no[chip] = b;
			PHY_DBG("_check_scan_data, valid data for ddr scan, block %d\n", scan_good_blk_no[chip]);
		} else {
			PHY_DBG("_check_scan_data, no valid data for ddr scan!\n");
		}
	}
	else
	{
	    readop.chip = chip;
	    readop.block = scan_good_blk_no[chip];
	    readop.page = 0;
	    //readop.sectorbitmap = 0xff; //3;
	    readop.mainbuf = main_buf;
	    readop.oobbuf = oob_buf;

	    ret = PHY_SimpleRead_1st1K(&readop); //read 4KB~8KB
	    if (ret == 0) {
			if ((oob_buf[0]==0xff)&&(oob_buf[1]==0xff)&&(oob_buf[2]==0xff)&&(oob_buf[3]==0xff)) {
				ret = -1;
				PHY_DBG("_check_scan_data, it is a free page(type 0), block %d\n", readop.block);
			} else if ((oob_buf[0]==0x0)&&(oob_buf[1]==0x0)&&(oob_buf[2]==0x0)&&(oob_buf[3]==0x0)) {
				ret = -1;
				PHY_DBG("_check_scan_data, bad block!\n", readop.block);
			} else {
				ret = 0;
				PHY_DBG("_check_scan_data, ok!\n");
			}
	    } else {
			#if 1
			if ((oob_buf[0]==0xff)&&(oob_buf[1]==0xff)&&(oob_buf[2]==0xff)&&(oob_buf[3]==0xff)) {
				ret = -1;
				PHY_DBG("_check_scan_data, it is a free page(type 1), block %d\n", readop.block);
			}
			#endif
			PHY_DBG("_check_scan_data, read error, block %d !\n", readop.block);
			ret = -1;
		}
	}

	FREE(main_buf, buf_size);

	return ret;
}

__s32 _change_nand_parameter(__u32 ddr_type, __u32 pre_ddr_type,
									NFC_DDR_INFO *pddr_info, __u32 dclk)
{
	__s32 ret;
	__u32 bank;
	__u8  chip, rb;
	__u32 ddr_change_mode = 0;
	NFC_DDR_INFO ddr_info = *pddr_info;
	__u32 tmode;
	NFC_CMD_LIST reset_cmd;

	/*check parameter*/
	if  (((ddr_type == ONFI_DDR) || (ddr_type == ONFI_DDR2)) && (pre_ddr_type == SDR)) //Async => ONFI DDR/DDR2
		ddr_change_mode = 1;
	else if ((ddr_type == SDR) && ((pre_ddr_type == ONFI_DDR) || (pre_ddr_type == ONFI_DDR2))) //ONFI DDR/DDR2 => Async
		ddr_change_mode = 2;
	else if (((ddr_type == TOG_DDR) || (ddr_type == TOG_DDR2)) && (pre_ddr_type == SDR)) //Async => Toggle DDR/DDR2
		ddr_change_mode = 3;
	else if ((ddr_type == SDR) && ((pre_ddr_type == TOG_DDR) || (pre_ddr_type == TOG_DDR2))) //Toggle DDR/DDR2 => Async
		ddr_change_mode = 4;
	else if ( ((ddr_type == TOG_DDR) && (pre_ddr_type == TOG_DDR2))
    			|| ((ddr_type == TOG_DDR2) && (pre_ddr_type == TOG_DDR)) )  //Toggle DDR2 <=> Toggle DDR
    	ddr_change_mode = 5;
	else if (ddr_type == pre_ddr_type)
		ddr_change_mode = 6;
	else {
		PHY_ERR("_change_nand_parameter: wrong input para, "
			"ddr_type %d, pre_ddr_type %d\n", ddr_type, pre_ddr_type);
		return -1;
	}

	tmode = _cal_nand_onfi_timing_mode(ddr_type, dclk);
	pddr_info->timing_mode = tmode;
	ddr_info.timing_mode = tmode;

	/*change nand flash parameter*/
	for (bank=0; bank<NandStorageInfo.ChipCnt; bank++)
    {
    	/*get chip no*/
		chip = _cal_real_chip(bank);
		if (0xff == chip) {
			PHY_ERR("_change_nand_parameter: beyond chip count, %d.\n", chip);
			return -ERR_INVALIDPHYADDR;
		}

		rb = _cal_real_rb(chip);
		NFC_SelectChip(chip);
		NFC_SelectRb(rb);

		//PHY_DBG("%s: ch: %d  chip: %d  rb: %d\n", __func__, NandIndex, chip, rb);

		if (ddr_change_mode == 1) {

			/* Async => ONFI DDR/DDR2 */
			if (SUPPORT_DDR2_SPECIFIC_CFG) {
				if (ddr_type == ONFI_DDR2) {
					if ( _setup_nand_onfi_ddr2_para(ddr_info) ) {
						PHY_ERR("_setup_nand_onfi_ddr2_para() failed! %d\n", ddr_change_mode);
						return -1;
					}
				}
			}

			if (SUPPORT_IO_DRIVER_STRENGTH) {
				if ( _setup_nand_onfi_driver_strength(ddr_info) ) {
					PHY_ERR("_setup_nand_onfi_driver_strength() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			if (SUPPORT_VENDOR_SPECIFIC_CFG) {
				if ( _setup_nand_onfi_vendor_specific_feature(ddr_info) ) {
					PHY_ERR("_setup_nand_onfi_vendor_specific_feature() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			if (SUPPORT_CHANGE_ONFI_TIMING_MODE) {
				if (_change_nand_onfi_timing_mode(ddr_info.if_type, ddr_info.timing_mode)) {
					PHY_ERR("_change_nand_onfi_timing_mode() failed\n");
					PHY_ERR("nand flash switch to nv-ddr or nv-ddr2 failed!  %d\n", ddr_change_mode);
					return -1;
				}
			}

		} else if (ddr_change_mode == 2) {

			/* ONFI DDR/DDR2 => Async */
			if (SUPPORT_DDR2_SPECIFIC_CFG) {
				if (pre_ddr_type == ONFI_DDR2) { // clear ddr parameter
					if ( _setup_nand_onfi_ddr2_para(ddr_info) ) {
						PHY_ERR("_setup_nand_onfi_ddr2_para() failed! %d\n", ddr_change_mode);
						return -1;
					}
				}
			}

			if (SUPPORT_IO_DRIVER_STRENGTH) {
				if ( _setup_nand_onfi_driver_strength(ddr_info) ) {
					PHY_ERR("_setup_nand_onfi_driver_strength() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			if (SUPPORT_VENDOR_SPECIFIC_CFG) {
				if ( _setup_nand_onfi_vendor_specific_feature(ddr_info) ) {
					PHY_ERR("_setup_nand_onfi_vendor_specific_feature() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			/* use async reset to aysnc interface */
			_add_cmd_list(&reset_cmd, 0xff, 0 , NDFC_IGNORE, NDFC_NO_DATA_FETCH, NDFC_IGNORE, NDFC_IGNORE, NDFC_WAIT_RB);
			ret = NFC_ResetChip(&reset_cmd);
			if (ret) {
				PHY_ERR("reset chip failed! %d\n", ddr_change_mode);
				return -1;
			}

			/* change to proper timing mode in async interface */
			if (SUPPORT_CHANGE_ONFI_TIMING_MODE) {
				if (_change_nand_onfi_timing_mode(ddr_info.if_type, ddr_info.timing_mode)) {
					PHY_ERR("_change_nand_onfi_timing_mode() failed\n");
					PHY_ERR("nand flash change timing mode at async interface failed!  %d \n", ddr_change_mode);
					return -1;
				}
			}

		} else if (ddr_change_mode == 3) {

			/* Async => Toggle DDR/DDR2 */

			_add_cmd_list(&reset_cmd, 0xff, 0 , NDFC_IGNORE, NDFC_NO_DATA_FETCH, NDFC_IGNORE, NDFC_IGNORE, NDFC_WAIT_RB);
			ret = NFC_ResetChip(&reset_cmd);
			if (ret) {
				PHY_ERR("reset chip failed! %d\n", ddr_change_mode);
				return -1;
			}

			if (SUPPORT_DDR2_SPECIFIC_CFG) {
				if (ddr_type == TOG_DDR2) {
					if ( _setup_nand_toggle_ddr2_para(ddr_info) ) {
						PHY_ERR("set nand onfi ddr2 parameter failed! %d\n", ddr_change_mode);
						return -1;
					}
				}
			}

			if (SUPPORT_IO_DRIVER_STRENGTH) {
				if ( _setup_nand_toggle_driver_strength(ddr_info) ) {
					PHY_ERR("_setup_nand_toggle_driver_strength() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			if (SUPPORT_VENDOR_SPECIFIC_CFG) {
				if ( _setup_nand_toggle_vendor_specific_feature(ddr_info) ){
					PHY_ERR("_setup_nand_toggle_vendor_specific_feature() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

		} else if (ddr_change_mode == 4) {

			/* Toggle DDR/DDR2 => Async */

			if (SUPPORT_DDR2_SPECIFIC_CFG) {
				// clear ddr2 parameter
				if (pre_ddr_type == TOG_DDR2) {
					if ( _setup_nand_toggle_ddr2_para(ddr_info) ) {
						PHY_ERR("set nand onfi ddr2 parameter failed! %d\n", ddr_change_mode);
						return -1;
					}
				}
			}

			if (SUPPORT_IO_DRIVER_STRENGTH) {
				if ( _setup_nand_toggle_driver_strength(ddr_info) ) {
					PHY_ERR("_setup_nand_toggle_driver_strength() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			if (SUPPORT_VENDOR_SPECIFIC_CFG) {
				if ( _setup_nand_toggle_vendor_specific_feature(ddr_info) ){
					PHY_ERR("_setup_nand_toggle_vendor_specific_feature() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

		} else if (ddr_change_mode == 5) {

			/* Toggle DDR2 <=> Toggle DDR */

			if (SUPPORT_DDR2_SPECIFIC_CFG) {
				if ( _setup_nand_toggle_ddr2_para(ddr_info) ) {
					PHY_ERR("set nand onfi ddr2 parameter failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			if (SUPPORT_IO_DRIVER_STRENGTH) {
				if ( _setup_nand_toggle_driver_strength(ddr_info) ) {
					PHY_ERR("_setup_nand_toggle_driver_strength() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

			if (SUPPORT_VENDOR_SPECIFIC_CFG) {
				if ( _setup_nand_toggle_vendor_specific_feature(ddr_info) ){
					PHY_ERR("_setup_nand_toggle_vendor_specific_feature() failed! %d\n", ddr_change_mode);
					return -1;
				}
			}

		} else if (ddr_change_mode == 6) {

			if(ddr_type == SDR) {
				if (SUPPORT_CHANGE_ONFI_TIMING_MODE) {
					if (_change_nand_onfi_timing_mode(ddr_info.if_type, ddr_info.timing_mode)) {
						PHY_ERR("nand flash change timing mode at async interface failed! %d  continue...\n", ddr_change_mode);
						return 1;
					}
				}
			} else if ((ddr_type == ONFI_DDR) || (ddr_type == ONFI_DDR2)) {
				if (SUPPORT_DDR2_SPECIFIC_CFG) {
					if (ddr_type == ONFI_DDR2) {
						if ( _setup_nand_onfi_ddr2_para(ddr_info) ) {
							PHY_ERR("set nand onfi ddr2 parameter failed! %d \n", ddr_change_mode);
							return -1;
						}
					}
				}

				if (SUPPORT_IO_DRIVER_STRENGTH) {
					if ( _setup_nand_onfi_driver_strength(ddr_info) ) {
						PHY_ERR("_setup_nand_onfi_driver_strength() failed! %d \n", ddr_change_mode);
						return -1;
					}
				}

				if (SUPPORT_VENDOR_SPECIFIC_CFG) {
					if ( _setup_nand_onfi_vendor_specific_feature(ddr_info) ){
						PHY_ERR("_setup_nand_onfi_vendor_specific_feature() failed! %d \n", ddr_change_mode);
						return -1;
					}
				}

				if (SUPPORT_CHANGE_ONFI_TIMING_MODE) {
					if ( _change_nand_onfi_timing_mode(ddr_info.if_type, ddr_info.timing_mode) ) {
						PHY_ERR("nand flash change timing mode at nvddr/nvddr2 failed! %d ", ddr_change_mode);
						return -1;
					}
				}

			} else if ((ddr_type == TOG_DDR) || (ddr_type == TOG_DDR2)) {
				if (SUPPORT_DDR2_SPECIFIC_CFG) {
					if (ddr_type == TOG_DDR2) {
						if ( _setup_nand_toggle_ddr2_para(ddr_info) ) {
							PHY_ERR("set nand toggle ddr2 parameter failed! %d \n", ddr_change_mode);
							return -1;
						}
					}
				}

				if (SUPPORT_IO_DRIVER_STRENGTH) {
					if ( _setup_nand_toggle_driver_strength(ddr_info) ) {
						PHY_ERR("_setup_nand_toggle_driver_strength() failed! %d \n", ddr_change_mode);
						return -1;
					}
				}

				if (SUPPORT_VENDOR_SPECIFIC_CFG) {
					if ( _setup_nand_toggle_vendor_specific_feature(ddr_info) ){
						PHY_ERR("_setup_nand_toggle_vendor_specific_feature() failed! %d \n", ddr_change_mode);
						return -1;
					}
				}
			} else {
				PHY_ERR("wrong nand interface type!\n");
			}

		} else {

		}
	}

	return 0;
}

__s32 _get_right_timing_para(NFC_INIT_INFO *nand_info, __u32 *scan_blk_no)
{
	__u32 bank, chip = 0;
	__u32 edo, delay, edo_cnt, delay_cnt;
	__u32 edo_delay[2][3], tmp_edo_delay[3];
	__u32 good_cnt, tmp_good_cnt, store_index;
	__s32 err_flag;
	__u32 good_flag;
	__u32 index, i, j; //m, k,
	NFC_CMD_LIST readid_cmd;
	__u8 readid_addr = 0;
	__u8  tmpChipID[16];
	__u32 param[2];
	__s32 ret;
	__u32 sclk0_bak, sclk1_bak;

	for(i=0; i<2;i++)
	{
		for(j=0;j<3;j++)
			edo_delay[i][j] = 0xffffffff;
	}
	for(j=0; j<3; j++)
		tmp_edo_delay[j] = 0xffffffff;

	good_flag = 0;
	index = 0;
	store_index = 0;
	good_cnt = 0;
	tmp_good_cnt = 0;
	edo_cnt = 0;
	delay_cnt = 0;
	param[0] = 0xffffffff;
	param[1] = 0xffffffff;

	if (nand_info->ddr_type == 0) {
		edo_cnt = 3;
		delay_cnt = 1;

		if (SUPPORT_SCAN_EDO_FOR_SDR_NAND == 0) {
			PHY_DBG("_get_right_timing_para, set edo to 1 for sdr nand.\n");
			nand_info->serial_access_mode = 1;
			return 0;
		}
	} else {
		delay_cnt = 64;
		if (NDFC_VERSION_V2 == NdfcVersion)
			edo_cnt = 32;
		else if (NDFC_VERSION_V1 == NdfcVersion)
			edo_cnt = 16; //32
		else {
			PHY_ERR("wrong ndfc version, %d\n", NdfcVersion);
			return -1;
		}
	}

    //for(k=0;k<NandStorageInfo.ChipCnt;k++)
    for (bank=0; bank<1; bank++)
    {
        chip = _cal_real_chip(bank);

        for (edo=0; edo<edo_cnt; edo++)
        {
        	good_flag = 0;

            for (delay=0; delay<delay_cnt; delay++)
            {
            	NFC_InitDDRParam(chip, ((edo<<8)|delay));
            	if (nand_info->ddr_type == 0)
            		nand_info->serial_access_mode = edo;
            	else {
					nand_info->ddr_edo = edo;
					nand_info->ddr_delay = delay;
            	}
            	NFC_ChangeInterfaceMode(nand_info);

                //read id
                //PHY_ReadNandId(chip, tmpChipID);
            	_add_cmd_list(&readid_cmd, 0x90, 1, &readid_addr, NDFC_DATA_FETCH, NDFC_IGNORE, 6, NDFC_NO_WAIT_RB);
            	ret = NFC_GetId(&readid_cmd, tmpChipID);
            	if (ret) {
            		PHY_ERR("read id failed! ---> continue\n");
            		continue;
            	}

                if ((NandStorageInfo.NandChipId[0] == tmpChipID[0]) && (NandStorageInfo.NandChipId[1] == tmpChipID[1])
                	&& (NandStorageInfo.NandChipId[2] == tmpChipID[2] ) && (NandStorageInfo.NandChipId[3] == tmpChipID[3]) )
				{
					PHY_DBG("{0x%x, 0x%x}: %x %x %x %x \n", edo, delay, tmpChipID[0], tmpChipID[1], tmpChipID[2], tmpChipID[3]);
					err_flag = _check_scan_data(0, chip, scan_blk_no);

					if (err_flag == 0)
					{
						//PHY_DBG("find a good {edo, delay}: 0x%x, 0x%x\n", edo, delay);
						PHY_DBG("_get_right_timing_para, {0x%x, 0x%x}\n", edo, delay);
						good_flag = 1;
						if (nand_info->ddr_type == 0) { //sdr mode

							/*  20130906-Gavin,
								This is for H27UCG8T2BTR special case. When sclk0 is 30MHz and
								timing cfg {edo, delay}={0, 0} also, check id operation and check burned data
								are all right, but this timing cfg is not right for this access frequcency.
								So, add flowing code to exclude this kind of case.
							*/
							if (edo == 0) {
								NAND_GetClk(NandIndex, &sclk0_bak, &sclk1_bak);
								PHY_DBG("sclk0 %d MHz, edo %d\n", sclk0_bak, edo);
								if (sclk0_bak < 12) //less 12MHz
									break;
								else
									good_flag = 0;
							}
							{
								//debug code for H27UCG8T2BTR
								//edo = 1;
								//delay = 0;
								//PHY_DBG("_get_right_timing_para, change edo and delay to {0x%x, 0x%x}\n", edo, delay);
							}
							break;
						}

						if ((index != 0) && (index != 1))
							PHY_ERR("wrong index!\n");
						if (store_index == 0)
						{
							if (edo_delay[index][0]== 0xffffffff) //first found
							{
								edo_delay[index][0] = edo; //k;
								edo_delay[index][1] = delay; //m;
								edo_delay[index][2] = delay; //m;
							}
							else
							{
								edo_delay[index][2] = delay; //m;
							}
							good_cnt++;
						}
						else if (store_index == 1)
						{
							if(tmp_edo_delay[0]== 0xffffffff) //first found
							{
								tmp_edo_delay[0] = edo; //k;
								tmp_edo_delay[1] = delay; //m;
								tmp_edo_delay[2] = delay; //m;
							}
							else
							{
								tmp_edo_delay[2] = delay; //m;
							}
							tmp_good_cnt++;
						}
					}
					else
					{
						/* id is ok, but data is wrong */

						if (good_cnt == 0) {
							store_index = 0;
						} else if (tmp_good_cnt == 0) {
							//store good {edo, delay} to tmp_edo_delay[]
							store_index = 1;
						} else if (tmp_good_cnt > good_cnt) {
							//move tmp_edo_delay[] to edo_delay[][]
							edo_delay[index][0] = tmp_edo_delay[0];
							edo_delay[index][1] = tmp_edo_delay[1];
							edo_delay[index][2] = tmp_edo_delay[2];
							good_cnt = tmp_good_cnt;

							//clear tmp_edo_delay[] for next valid group
							store_index = 1;
							tmp_good_cnt = 0;
							for(j=0;j<3;j++)
								tmp_edo_delay[j] = 0xffffffff;
						} else {
							store_index = 1;
							tmp_good_cnt = 0;
							for(j=0;j<3;j++)
								tmp_edo_delay[j] = 0xffffffff;
						}
					}
				}
				else
				{
					/* read id wrong */

					if (good_cnt == 0) {
						store_index = 0;
					} else if (tmp_good_cnt == 0) {
						//store good {edo, delay} to tmp_edo_delay[]
						store_index = 1;
					} else if (tmp_good_cnt > good_cnt) {
						//move tmp_edo_delay[] to edo_delay[][]
						edo_delay[index][0] = tmp_edo_delay[0];
						edo_delay[index][1] = tmp_edo_delay[1];
						edo_delay[index][2] = tmp_edo_delay[2];
						good_cnt = tmp_good_cnt;

						//clear tmp_edo_delay[] for next valid group
						store_index = 1;
						tmp_good_cnt = 0;
						for(j=0;j<3;j++)
							tmp_edo_delay[j] = 0xffffffff;
					} else {
						store_index = 1;
						tmp_good_cnt = 0;
						for(j=0;j<3;j++)
							tmp_edo_delay[j] = 0xffffffff;
					}
				}
            }

			if (good_flag)
			{
				if (nand_info->ddr_type == 0) //sdr mode
					break;

				if (index == 0) {

					if (good_cnt >= GOOD_DDR_EDO_DELAY_CHAIN_TH) //8 groups of {edo, delay} is enough
						break;

					index = 1;

					store_index = 0;
					good_cnt = 0;
					tmp_good_cnt = 0;
					for(j=0;j<3;j++)
						tmp_edo_delay[j] = 0xffffffff;
				} else
					break;
			}
        }
    }

    if (nand_info->ddr_type == 0) {
    	if (good_flag)
    		nand_info->serial_access_mode = edo;

    	goto RET;
    }

	if ((edo_delay[0][0]==0xffffffff) && (edo_delay[1][0]==0xffffffff)) {
		good_flag = 0;
		PHY_ERR("can't find a good edo, delay chain. index %d:  %d %d %d\n", index, edo_delay[index][0], edo_delay[index][1], edo_delay[index][2]);

	} else if ((edo_delay[0][0]!=0xffffffff) && (edo_delay[1][0]!=0xffffffff)) {
		i = edo_delay[0][2] - edo_delay[0][1];
		j = edo_delay[1][2] - edo_delay[1][1];
		if(j > i)
		{
			param[0] = edo_delay[1][0];
			//param[1] = (edo_delay[1][1]+ edo_delay[1][2])/2 + 1;
			param[1] = (edo_delay[1][2] - edo_delay[1][1])*2/3;
			param[1] += edo_delay[1][1];
			if ((param[1]+4) > edo_delay[1][2])
				param[1] = edo_delay[1][2] - 4;
			if (j >= GOOD_DDR_EDO_DELAY_CHAIN_TH)
				good_flag = 1;
			else
				good_flag = 0;
		}
		else
		{
			param[0] = edo_delay[0][0];
			//param[1] = (edo_delay[0][1]+ edo_delay[0][2])/2 + 1;
			param[1] = (edo_delay[0][2] - edo_delay[0][1])*2/3;
			param[1] += edo_delay[0][1];
			if ((param[1]+4) > edo_delay[0][2])
				param[1] = edo_delay[0][2] - 4;
			if (j >= GOOD_DDR_EDO_DELAY_CHAIN_TH)
				good_flag = 1;
			else
				good_flag = 0;
		}
		PHY_DBG("(0x%x, 0x%x - 0x%x), (0x%x, 0x%x - 0x%x)\n", edo_delay[0][0],edo_delay[0][1],edo_delay[0][2],edo_delay[1][0],edo_delay[1][1],edo_delay[1][2]);
		if (good_flag)
			PHY_DBG("good edo: 0x%x, good delay chain: 0x%x\n", param[0], param[1]);
		else
			PHY_ERR("can't find a good edo, delay chain.\n");
	}

	else if ((edo_delay[0][0]!=0xffffffff) && (edo_delay[1][0]==0xffffffff)) {

		i = edo_delay[0][2] - edo_delay[0][1];

		param[0] = edo_delay[0][0];
		//param[1] = (edo_delay[0][1]+ edo_delay[0][2])/2 + 1;
		param[1] = (edo_delay[0][2] - edo_delay[0][1])*2/3;
		param[1] += edo_delay[0][1];
		if ((param[1]+4) > edo_delay[0][2])
			param[1] = edo_delay[0][2] - 4;

		PHY_DBG("(0x%x, 0x%x - 0x%x) \n", edo_delay[0][0],edo_delay[0][1],edo_delay[0][2]);
		if (i >= GOOD_DDR_EDO_DELAY_CHAIN_TH)
			good_flag = 1;
		else
			good_flag = 0;

		if (good_flag)
			PHY_DBG("good edo: 0x%x, good delay chain: 0x%x\n", param[0], param[1]);
		else
			PHY_ERR("can't find a good edo, delay chain.\n");
	}

	else {
		good_flag = 0;
		PHY_ERR("scan error!!!!!!!\n");
	}

	nand_info->ddr_edo = param[0];
	nand_info->ddr_delay = param[1];
	NFC_InitDDRParam(chip, ((nand_info->ddr_edo<<8)|nand_info->ddr_delay));

RET:
	if (good_flag == 0)
		return -1;

    return 0;
}

__s32 _read_single_page(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret = 0;
	__u32 k = 0;
	__u32 rb;
	__u32 random_seed;
	__u8 sparebuf[4*64];
	__u8 default_value[16];
	__u8 addr[5];
	NFC_CMD_LIST cmd_list[4];
	__u32 list_len;
	__s32 i, n,m; //, err, cnt;

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
        if((READ_RETRY_MODE>=0x10)&&(READ_RETRY_MODE<0x60))  //toshiba & Samsung mode & Sandisk mode & micron mode
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
        for(k=0; k<READ_RETRY_CYCLE+1; k++)
		{
			if(RetryCount[NandIndex][readop->chip]==(READ_RETRY_CYCLE+1))
				RetryCount[NandIndex][readop->chip] = 0;

			if(k>0)
			{
				if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
				{
					PHY_ERR("[Read_single_page] NFC_ReadRetry fail \n");
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
				ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				NFC_RandomDisable();
				if(ret == -ERR_ECC)
				{
					//PHY_DBG("%s: disable randomize and read again(k=%d)...\n", __func__, k);
					ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
				}
				//PHY_DBG("%s: %d ", __func__, ret);
				//for (i=0; i<16; i++)
				//	PHY_DBG("%x ", sparebuf[i]);
				//PHY_DBG("\n");

				/**************************************************************************************
				* 1. add by Neil, from v2.09
				* 2. if spare area is all 0xff in random disable mode, it means the page is a clear page
				* 3. because in toshiba 24nm nand, too many clear pages are not all 0xff
				***************************************************************************************/
				if((ret == -ERR_ECC)&&(sparebuf[0]==0xff)&&(sparebuf[1]==0xff)&&(sparebuf[2]==0xff)&&(sparebuf[3]==0xff)&&(sparebuf[4]==0xff)&&(sparebuf[5]==0xff)&&(sparebuf[6]==0xff)&&(sparebuf[7]==0xff))
				{
					//PHY_DBG("[Read_single_page] find not all 0xff clear page!  chip = %d, block = %d, page = %d\n", readop->chip, readop->block, readop->page);
					ret = 0;
				}

			}
			else
			{
				ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			}

			if((ret != -ERR_ECC)||(k==READ_RETRY_CYCLE))
			{
				if(k==0)
				{
					//PHY_DBG("%s: k=0 exit, ok !!\n", __func__);
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
							//toshiba A19nm
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

			PHY_DBG("[Read_single_page] NFC_ReadRetry %d cycles, ch =%d, chip = %d\n", (__u32)k, (__u32)NandIndex, (__u32)readop->chip);
			PHY_DBG("[Read_single_page]	block = %d, page = %d, RetryCount = %d  \n", (__u32)readop->block, (__u32)readop->page, (__u32)RetryCount[NandIndex][readop->chip]);
			//PHY_DBG("%s: %d ", __func__, ret);
			//for (i=0; i<16; i++)
			//	PHY_DBG("%x ", sparebuf[i]);
			//PHY_DBG("\n");
    		if(ret == -ERR_ECC)
    		{
    		    //PHY_DBG("ecc error--0!\n");
    			//PHY_DBG("spare buf: %x, %x, %x, %x, %x, %x, %x, %x\n", sparebuf[0],sparebuf[1],sparebuf[2],sparebuf[3],sparebuf[4],sparebuf[5],sparebuf[6],sparebuf[7]);
				if((READ_RETRY_MODE==0x2)||(READ_RETRY_MODE==0x3)||(READ_RETRY_MODE==0x4)) //hynix mode
				{
					NFC_SetDefaultParam(readop->chip, default_value, READ_RETRY_TYPE);
					RetryCount[NandIndex][readop->chip] = 0;
					PHY_DBG("ecc error--0!\n");
				}

				else if(0x32 == READ_RETRY_MODE)
				{
					/* for sandisk A19nm */
					for(m=0;m<3;m++)
					{

						RetryCount[NandIndex][readop->chip] = 0;

						for(n=0; n<(READ_RETRY_CYCLE+1); n++)
						{

							if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
							{
								PHY_ERR("[Read_single_page] NFC_ReadRetry fail\n");
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
								ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								NFC_RandomDisable();
								if(ret == -ERR_ECC)
								{
									//PHY_DBG("%s(): disable randomize and read again(k=%d)...\n", __func__, k);
									ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
								}
							}
							else
							{
								ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}
							//if((n==READ_RETRY_CYCLE))
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
						PHY_DBG("ecc error--1!\n");
				}
				else if(0x33 == READ_RETRY_MODE)
				{

					RetryCount[NandIndex][readop->chip] = 0;

					for(n=0; n<(READ_RETRY_CYCLE+1); n++)
					{
						if(NFC_ReadRetry(readop->chip,RetryCount[NandIndex][readop->chip],READ_RETRY_TYPE))
						{
							PHY_ERR("[Read_single_page] NFC_ReadRetry fail\n");
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
							ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							NFC_RandomDisable();
							if(ret == -ERR_ECC)
							{
								//PHY_DBG("%s(): disable randomize and read again(k=%d)...\n", __func__, k);
								ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
							}
						}
						else
						{
							ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
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
			ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
			NFC_RandomDisable();
			if(ret == -ERR_ECC)
				ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		}
		else
		{
			ret = NFC_Read(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
		}

    }

	if (dma_wait_mode)
		_pending_dma_irq_sem();

#if 0
	////////////////////////////////////////////////////////////
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
	////////////////////////////////////////////////////////////////////
#endif

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 4 * 4); //MEMCPY(readop->oobbuf,sparebuf, 2 * 4);
	}

	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}

__s32 _read_single_page_first(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret = 0;
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
		random_seed = _cal_random_seed(readop->page);
		NFC_SetRandomSeed(random_seed);
		NFC_RandomEnable();
		ret = NFC_Read_First(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}
	else
	{
		ret = NFC_Read_First(cmd_list, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	}

	return ret;
}

__s32 _read_single_page_wait(struct boot_physical_param *readop,__u8 dma_wait_mode)
{
	__s32 ret = 0;
	//__s32 i; //, err, cnt;
	__u8 sparebuf[4*64];
	__u32 rb;

	//for (i=0; i<4*64; i++)
	//	sparebuf[i] = 0x66;

	ret = NFC_Read_Wait(NULL, readop->mainbuf, sparebuf, dma_wait_mode , NDFC_PAGE_MODE);
	NFC_RandomDisable();

	if(ret == -ERR_ECC)
	{
		//PHY_DBG("%s: 1st read page: page %d  block %d  chip %d  --ch %d\n", __func__, readop->page, readop->block, readop->chip, NandIndex);
		return(_read_single_page(readop,dma_wait_mode));
	}

	if (dma_wait_mode)
		_pending_dma_irq_sem();

//	if (readop->page == (NandStorageInfo.PageCntPerPhyBlk-1))
//	{
//	    err = 0;
//    	//check default value
//    	cnt = 0;
//		for (i=0; i<5; i++)
//		{
//			if (sparebuf[i] == 0x77) //set in _read_single_page_first()
//				cnt++;
//			else
//				break;
//		}
//		if (cnt == 5)
//			err = 1;
//
//		if(sparebuf[1] == 0xaa)
//		{
//		    if(sparebuf[2] != 0xaa)
//		    {
//		        err = 2;
//		    }
//		    if(sparebuf[3] != 0xff)
//		    {
//		        err = 3;
//		    }
//
//		    if(sparebuf[4] != 0xff)
//		    {
//		        err = 4;
//		    }
//		}
//		else
//		{
//		    err = 5;
//		}
//
//		if (err)
//		{
//			PHY_ERR("%s: %d wrong spare data: page %d  block %d  chip %d  --ch %d\n",__func__, err, readop->page, readop->block, readop->chip, NandIndex);
//			for (i=0; i<16; i++)
//			{
//				PHY_ERR("0x%x ", sparebuf[i]);
//			}
//			PHY_ERR("\n");
//			//return 0;
//		}
//	}

	if (readop->oobbuf){
		MEMCPY(readop->oobbuf,sparebuf, 2 * 4); //MEMCPY(readop->oobbuf,sparebuf, 2 * 4);
	}

	rb = _cal_real_rb(readop->chip);
	NFC_DeSelectChip(readop->chip);
	NFC_DeSelectRb(rb);

	//FREE(sparebuf);
	return ret;
}

/*
************************************************************************************************************************
*                       INIT NAND FLASH DRIVER PHYSICAL MODULE
*
* Description: init nand flash driver physical module.
*
* Aguments   : none
*
* Returns    : the resutl of initial.
*                   = 0     initial successful;
*                   = -1    initial failed.
************************************************************************************************************************
*/
__s32 PHY_Init(void)
{
    __s32 ret=0;
    __u32 i;
	NFC_INIT_INFO nand_info;

	nand_info.bus_width = 0x0;
	nand_info.ce_ctl = 0x0;
	nand_info.ce_ctl1 = 0x0;
	nand_info.debug = 0x0;
	nand_info.pagesize = 4;
	nand_info.rb_sel = 1;
	nand_info.serial_access_mode = 1;
	nand_info.ddr_type = 0;

	//set default channel cnt;
	CHANNEL_CNT = MAX_NFC_CH;
	PHY_DBG("[NAND] channel cnt is %d\n",CHANNEL_CNT);

    //init RetryCount
	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{

		for(i=0; i<MAX_CHIP_SELECT_CNT; i++)
		    RetryCount[NandIndex][i] = 0;

		ret = NFC_Init(&nand_info);

	    PHY_DBG("NFC Randomizer start. \n");
		_random_seed_init();
		NFC_RandomDisable();

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

	return ret;
}

__s32 PHY_GetDefaultParam(__u32 bank)
{
	__u32 i, j, chip = 0, rb = 0;
	__u8 default_value[64];
	__u8 oob_buf[64];
	__u8 *oob, *pdata;
	__s32 ret, otp_ok_flag = 0;
	__u8 *data_ff_buf;
	struct boot_physical_param nand_op;

    chip = _cal_real_chip(bank);
    NFC_SelectChip(chip);
    rb = _cal_real_rb(chip);
    NFC_SelectRb(rb);
    oob = (__u8 *)(oob_buf);

    if (!PageCachePool.PageCache0){
		PageCachePool.PageCache0 = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE * 512);
		if (!PageCachePool.PageCache0)
			return -1;
	}
    pdata = (__u8 *)(PHY_TMP_PAGE_CACHE);

	data_ff_buf = (__u8 *)MALLOC(18048);
	if (!data_ff_buf)
	{

		PHY_ERR("[PHY_GetDefaultParam]:data_ff_buf malloc fail\n");
		return -1;
	}
	for(i=0;i<18048;i++)
	{
		*((__u8 *)data_ff_buf + i) = 0xFF;
	}

	if((__u32)CHANNEL_CNT>2)
		PHY_ERR("[PHY_GetDefaultParam]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);


	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
	    if((READ_RETRY_MODE==2)||(READ_RETRY_MODE==3))
	    {
	        while(1)
	        {
	            otp_ok_flag = 0;
	            for(i = 8; i<12; i++)
	            {
	                nand_op.chip = chip;
	                nand_op.block = i;
	                nand_op.page = 0;
	                nand_op.mainbuf = PHY_TMP_PAGE_CACHE;
	                nand_op.oobbuf = oob_buf;

	                ret = PHY_SimpleRead_CurCH(&nand_op);
	                PHY_DBG("ch: %d, chip %d, block %d, page 0, oob: 0x%x, 0x%x, 0x%x, 0x%x\n",NandIndex,nand_op.chip, nand_op.block, oob[0], oob[1], oob[2], oob[3]);
	                if((ret>=0)&&(oob[0] == 0x00)&&(oob[1] == 0x4F)&&(oob[2] == 0x4F)&&(oob[3] == 0x43))
	                {
	                    otp_ok_flag = 1;
	                    for(j=0;j<64;j++)
	                    {
	                        if((pdata[j] + pdata[64+j])!= 0xff)
	                        {
	                            PHY_DBG("otp data check error!\n");
	                            otp_ok_flag = 0;
	                            break;
	                        }
	                    }
	                    if(otp_ok_flag == 1)
	                    {
	                        PHY_DBG("find good otp value in ch: %d, chip %d, block %d \n", NandIndex, nand_op.chip, nand_op.block);
	                        break;
	                    }

	                }
	            }

	            if(otp_ok_flag)
	            {
	                for(j=0;j<64;j++)
	                    default_value[j] = pdata[j];
	                if((READ_RETRY_MODE==2)||(READ_RETRY_MODE==3))
	                {
	                    PHY_DBG("ch %d, Read Retry value Table from nand otp block:\n", NandIndex);
	                    for(j = 0;j<64; j++)
	                    {
	                        PHY_DBG("0x%x ", pdata[j]);
	                        if(j%8 == 7)
	                            PHY_DBG("\n");
	                    }
	                }
	                NFC_GetOTPValue(chip, default_value, READ_RETRY_TYPE);
					NFC_SetDefaultParam(chip, default_value, READ_RETRY_TYPE);
	                break;
	            }
	            else
	            {
	                PHY_DBG("[PHY_DBG] ch %d, can't get right otp value from nand otp blocks, then use otp command\n", NandIndex);
	                NFC_GetDefaultParam(chip, default_value, READ_RETRY_TYPE);
	    	        NFC_SetDefaultParam(chip, default_value, READ_RETRY_TYPE);
	    	        #if 0
	    	        if((READ_RETRY_MODE==2)||(READ_RETRY_MODE==3))
	                {
	                    PHY_DBG("Read Retry value Table from otp area:\n");
	                    for(i = 0;i<8; i++)
	                    {
	                        PHY_DBG("retry cycle %d: ", i);
	                        for(j=0; j<8;j++)
	                            PHY_DBG("0x%x ", default_value[8*i+j]);
	                        PHY_DBG("\n");
	                    }
	                }
	                #endif

	                for(j=0;j<64;j++)
	                {
	                    pdata[j] = default_value[j];
	                    pdata[64 + j] = 0xff - default_value[j];
	                }

	                oob[0] = 0x00;
	                oob[1] = 0x4F;
	                oob[2] = 0x4F;
	                oob[3] = 0x43;

	                NFC_LSBInit(READ_RETRY_TYPE);
	                NFC_LSBEnable(chip, READ_RETRY_TYPE);
	                for(i = 8; i<12; i++)
	                {
	                    nand_op.chip = chip;
	                    nand_op.block = i;
	                    nand_op.page = 0;
	                    nand_op.mainbuf = PHY_TMP_PAGE_CACHE;
	                    nand_op.oobbuf = oob_buf;

	                    ret = PHY_SimpleErase_CurCH(&nand_op);
	                    if(ret<0)
	                    {
	                        PHY_ERR("erase chip %d, block %d error\n", nand_op.chip, nand_op.block);
	                        continue;
	                    }
	                    ret = PHY_SimpleWrite_CurCH(&nand_op);
	                    if(ret<0)
	                    {
	                        PHY_ERR("write chip %d, block %d, page 0 error\n", nand_op.chip, nand_op.block);
	                        continue;
	                    }
	                }
	                NFC_LSBDisable(chip, READ_RETRY_TYPE);
	                NFC_LSBExit(READ_RETRY_TYPE);

	                PHY_DBG("[PHY_DBG] repair ch %d otp value end\n", NandIndex);
	            }
	        }
	    }
	    else if(READ_RETRY_MODE==4)
		{
			while(1)
			{
				otp_ok_flag = 0;
				for(i = 8; i<12; i++)
				{
					nand_op.chip = chip;
					nand_op.block = i;
					nand_op.page = 0;
					nand_op.mainbuf = PHY_TMP_PAGE_CACHE;
					nand_op.oobbuf = oob_buf;

					ret = PHY_SimpleRead_CurCH(&nand_op);
					PHY_DBG("ch: %d, chip %d, block %d, page 0, oob: 0x%x, 0x%x, 0x%x, 0x%x\n",NandIndex,nand_op.chip, nand_op.block, oob[0], oob[1], oob[2], oob[3]);
					if((ret>=0)&&(oob[0] == 0x00)&&(oob[1] == 0x4F)&&(oob[2] == 0x4F)&&(oob[3] == 0x43))
					{
						otp_ok_flag = 1;
						for(j=0;j<32;j++)
						{
							if((pdata[32+j] + pdata[j])!= 0xff)
							{
								PHY_DBG("otp data check error!\n");
								otp_ok_flag = 0;
								break;
							}
						}

						if(otp_ok_flag == 1)
						{
							PHY_DBG("find good otp value in ch: %d, chip %d, block %d \n", NandIndex, nand_op.chip, nand_op.block);
							break;
						}

					}
				}
				if(otp_ok_flag)
				{
					PHY_DBG("ch %d, Read Retry value Table from nand otp block:\n", NandIndex);

					for(j = 0;j<32; j++)
					{
						PHY_DBG("0x%x ", pdata[j]);
						if(j%8 == 7)
							PHY_DBG("\n");
					}

					NFC_GetOTPValue(chip, pdata, READ_RETRY_TYPE);

					NFC_SetDefaultParam(chip, default_value, READ_RETRY_TYPE);

					break;
				}
				else
				{
					PHY_DBG("[PHY_DBG] ch %d, can't get right otp value from nand otp blocks, then use otp command\n", NandIndex);
					NFC_GetDefaultParam(chip, default_value, READ_RETRY_TYPE);

					NFC_SetDefaultParam(chip, default_value, READ_RETRY_TYPE);

					#if 0
					for (ndie=0; ndie<DIE_CNT_OF_CHIP; ndie++)
					{
						PHY_DBG("die %d Read Retry value Table from otp area:\n",ndie);
						for(i = 0;i<8; i++)
						{
							PHY_DBG("retry cycle %d: ", i);
							for(j=0; j<4;j++)
								PHY_DBG("0x%x ", def_val[8*i+j]);
							PHY_DBG("\n");
						}
					}


					#endif

					for(j=0;j<32;j++)
					{
						pdata[j] = default_value[j];
						//pdata[32*DIE_CNT_OF_CHIP+32*ndie+j] = 0xff - pdata[32*ndie+j];
						pdata[32+j] = 0xff - pdata[j];
					}

					oob[0] = 0x00;
					oob[1] = 0x4F;
					oob[2] = 0x4F;
					oob[3] = 0x43;

					for(i = 8; i<12; i++)
					{
						nand_op.chip = chip;
						nand_op.block = i;

						ret = PHY_SimpleErase_CurCH(&nand_op);
						if(ret<0)
						{
							PHY_ERR("erase chip %d, block %d error\n", nand_op.chip, nand_op.block);
							continue;
						}
						for(j=0;j<3;j++)
						{
							nand_op.page = j;

							if(j<2)
							{
								nand_op.mainbuf = PHY_TMP_PAGE_CACHE;
								nand_op.oobbuf = oob_buf;
								ret = PHY_SimpleWrite_CurCH(&nand_op);
								if(ret<0)
								{
									PHY_ERR("write chip %d, block %d, page %d error\n", nand_op.chip, nand_op.block,nand_op.page);
									continue;
								}
							}
							else
							{
								nand_op.mainbuf = (void *)data_ff_buf;
								nand_op.oobbuf = oob_buf;
								ret = PHY_SimpleWrite_0xFF(&nand_op);
								if(ret<0)
								{
									PHY_ERR("write chip %d, block %d, page %d error\n", nand_op.chip, nand_op.block,nand_op.page);
									continue;
								}
							}

						}
					}

					PHY_DBG("[PHY_DBG] repair ch %d chip %d otp value end\n", NandIndex,chip);
				}
			}
		}
	    else
	    {
	        NFC_GetDefaultParam(chip, default_value, READ_RETRY_TYPE);
			if((READ_RETRY_MODE==0)||(READ_RETRY_MODE==1))  //hynix mode
			{
	            PHY_DBG("NFC_GetDefaultParam: ch: %d, chip: %d, value: 0x%x 0x%x 0x%x 0x%x \n",
	            	NandIndex, chip, default_value[0], default_value[1], default_value[2], default_value[3]);
	        }
		    NFC_SetDefaultParam(chip, default_value, READ_RETRY_TYPE);
		    if((READ_RETRY_MODE==0)||(READ_RETRY_MODE==1))  //hynix mode
	        {
	            PHY_DBG("NFC_SetDefaultParam: ch: %d, chip: %d, value: 0x%x 0x%x 0x%x 0x%x \n",
	            	NandIndex, chip, default_value[0], default_value[1], default_value[2], default_value[3]);
	        }

	    }

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

    return 0;
}

__s32 PHY_SetDefaultParam(__u32 bank)
{
	__u32 chip = 0;
	__u8 default_value[64];

	if((__u32)CHANNEL_CNT>2)
			PHY_ERR("[PHY_SetDefaultParam]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);
	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
		if(SUPPORT_READ_RETRY)
		{
	        chip = _cal_real_chip(bank);
	        NFC_SelectChip(chip);
	        NFC_SetDefaultParam(chip, default_value, READ_RETRY_TYPE);
			if((READ_RETRY_MODE==0)||(READ_RETRY_MODE==1))  //hynix mode
	        {
	            PHY_DBG("NFC_SetDefaultParam: ch: %d, chip: %d, value: 0x%x 0x%x 0x%x 0x%x \n",
	            	NandIndex, chip, default_value[0], default_value[1], default_value[2], default_value[3]);
	        }
	    }

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;
    return 0;
}

__s32 PHY_Readretry_reset(void)
{
	__u32 i;

	if(SUPPORT_READ_RETRY)
	{
		PHY_DBG("NFC Read Retry Reset. \n");
		for(i=0; i<NandStorageInfo.ChipCnt;i++)
		{
			PHY_SetDefaultParam(i);
		}
	}

	if(SUPPORT_READ_RETRY)
	{
		for(i=0;i<MAX_CHIP_SELECT_CNT;i++)
		{
			RetryCount[0][i]=0;
			RetryCount[1][i]=0;
		}
	}
	return 0;
}

__s32 PHY_ChangeMode(__u32 ddr_type, void *pddr_info, __u32 sclk0, __u32 sclk1)
{
	__s32 ret=0;
	__u32 ddr_type_bak; //edo_bak, ddr_type_bak, ddr_edo_bak, ddr_delay_bak;
	__u32 sclk0_bak, sclk1_bak;
	__u32 is_blank_page, timing_change;
	__u32 classic_dclk;
	__u32 chip; //, bank, rb;
	NFC_INIT_INFO nand_info;
	NFC_DDR_INFO ddr_info = *((NFC_DDR_INFO *)pddr_info);
	__u32 reg_val, feature_addr = 0;
	__u8  feature[8] = {0};

	/*memory allocate*/
	if (!PageCachePool.PageCache0){
		PageCachePool.PageCache0 = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		if (!PageCachePool.PageCache0)
			return -1;
		PHY_DBG("PageCachePool.PageCache0: 0x%08x\n", PageCachePool.PageCache0);
	}
	if (!PageCachePool.PageCache1){
		PageCachePool.PageCache1 = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		if (!PageCachePool.PageCache1)
			return -1;
		PHY_DBG("PageCachePool.PageCache1: 0x%08x\n", PageCachePool.PageCache1);
	}
	if (!PageCachePool.PageCache2){
		PageCachePool.PageCache2 = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		if (!PageCachePool.PageCache2)
			return -1;
		PHY_DBG("PageCachePool.PageCache2: 0x%08x\n", PageCachePool.PageCache2);
	}
	if (!PageCachePool.PageCache3){
		PageCachePool.PageCache3 = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		if (!PageCachePool.PageCache3)
			return -1;
		PHY_DBG("PageCachePool.PageCache3: 0x%08x\n", PageCachePool.PageCache3);
	}
	if (!PageCachePool.PageCache4){
		PageCachePool.PageCache4 = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		if (!PageCachePool.PageCache4)
			return -1;
		PHY_DBG("PageCachePool.PageCache4: 0x%08x\n", PageCachePool.PageCache4);
	}

	if (!PageCachePool.SpareCache){
		PageCachePool.SpareCache = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 4);
		if (!PageCachePool.SpareCache)
			return -1;
		PHY_DBG("PageCachePool.SpareCache: 0x%08x\n", PageCachePool.SpareCache);
	}

	if (!PageCachePool.TmpPageCache){
		PageCachePool.TmpPageCache = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		if (!PageCachePool.TmpPageCache)
			return -1;
		PHY_DBG("PageCachePool.TmpPageCache: 0x%08x\n", PageCachePool.TmpPageCache);
	}
	if (!PageCachePool.TmpPageCache1){
		PageCachePool.TmpPageCache1 = (__u8 *)MALLOC(SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		if (!PageCachePool.TmpPageCache1)
			return -1;
		PHY_DBG("PageCachePool.TmpPageCache1: 0x%08x\n", PageCachePool.TmpPageCache1);
	}

	/*memory allocation for dma descriptor*/
	if (NdfcVersion == NDFC_VERSION_V2) {
		if (ndfc_dma_desc_addr == 0) {
			if ( NAND_AllocMemoryForDMADescs(&ndfc_dma_desc_cpu_addr, &ndfc_dma_desc_addr) )
				return -1;

			ndfc_dma_desc = (_ndfc_dma_desc_t *)ndfc_dma_desc_addr;
			ndfc_dma_desc_cpu = (_ndfc_dma_desc_t *)ndfc_dma_desc_cpu_addr;
			PHY_DBG("PHY_ChangeMode: ndfc_dma_desc: cpu: 0x%08x   physic: 0x%08x\n", ndfc_dma_desc_cpu, ndfc_dma_desc);
		}
	}

	nand_info.bus_width = 0x0;
	nand_info.ce_ctl = 0x0;
	nand_info.ce_ctl1 = 0x0;
	nand_info.debug = 0x0;
	nand_info.pagesize = SECTOR_CNT_OF_SINGLE_PAGE;
	nand_info.ddr_type = 0;
	nand_info.serial_access_mode = 0;
	nand_info.ddr_edo = 0;
	nand_info.ddr_delay = 0;

	if((__u32)CHANNEL_CNT>2)
		PHY_ERR("[PHY_ChangeMode]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

	/* prepare to change nand flash interface and onfi timing mode */
	//PHY_ERR("start change nand flash intefface ...\n");
	{
		NandIndex = 0;

		//edo_bak = 0;
		ddr_type_bak = 0;
		//ddr_edo_bak = 0;
		//ddr_delay_bak = 0;
		sclk0_bak = 0;
		sclk1_bak = 0;
		timing_change = 0;
		is_blank_page = 0;

		NFC_GetInterfaceMode(&nand_info);
		ddr_type_bak = nand_info.ddr_type;
		//edo_bak = nand_info.serial_access_mode;
		//ddr_edo_bak = nand_info.ddr_edo;
		//ddr_delay_bak = nand_info.ddr_delay;
		NAND_GetClk(NandIndex, &sclk0_bak, &sclk1_bak);

		if (ddr_type_bak != ddr_type) {
			timing_change = 1;
			PHY_DBG("PHY_ChangeMode: nand interface will be changed! %d -> %d\n",
					ddr_type_bak, ddr_type);
		}

		if (sclk0_bak != sclk0) {
			timing_change = 1;
			PHY_DBG("PHY_ChangeMode: sclk0 will be changed! %d -> %d\n", sclk0_bak, sclk0);
		}

		if ((NdfcVersion == NDFC_VERSION_V2) && (sclk1_bak != sclk1)) {
			timing_change = 1;
			PHY_DBG("PHY_ChangeMode: sclk1 will be changed! %d -> %d\n", sclk1_bak, sclk1);
		}

		/*check blank page??*/
		if (NandIndex == 0) {
			chip = _cal_real_chip(0); //bank = 0;
			PHY_DBG("PHY_ChangeMode: before check blank page, 0x%x, 0x%x, 0x%x, 0x%x\n",
				NDFC_READ_REG_CTL(), NDFC_READ_REG_TIMING_CTL(), NDFC_READ_REG_TIMING_CFG(), ddr_param[0]);
			ret = _check_scan_data(1, chip, ddr_scan_blk_no);
			if (ret < 0) {
				PHY_DBG("PHY_ChangeMode: check blank page error!\n");

				if (ddr_type_bak == SDR) {

					/* if current ddr type is sdr,  use classic clk cfg(30MHz), --->to be compatiable with old driver */
					ret = NAND_SetClk(NandIndex, 30, 30*2);
					if (ret) {
						PHY_ERR("PHY_ChangeMode: change clk to classic"
							"cfg(%d, %d) for sdr mode failed!\n", 30, 30*2);
						return -1;
					}

					nand_info.ddr_type = 0;
					nand_info.serial_access_mode = 1;
					NFC_ChangeInterfaceMode(&nand_info);

					PHY_ERR("PHY_ChangeMode: sdr mode, use classic clk cfg(%d,%d)!\n", 30, 30*2);

					return 0;
				} else {

					/* if current ddr type is not sdr, error, and return fail */
					PHY_ERR("PHY_ChangeMode: current ddr byte is %d. return -1\n", ddr_type_bak);
					return -1;
				}

			} else if (ret == 1) {
				/* check data right, and data is 0xff */
				is_blank_page = 1;
				PHY_ERR("PHY_ChangeMode: no valid burned data!\n");
			} else {
				/* check data right */
				PHY_DBG("PHY_ChangeMode: valid burned data!\n");
			}
		}
	}

	PHY_DBG("PHY_ChangeMode: timing_change %d,  sclk0_bak %d, sclk0 %d!\n", timing_change, sclk0_bak, sclk0);

	/* change interface and timing parameter */
	for(NandIndex = 0; NandIndex<CHANNEL_CNT; NandIndex++)
	{
		/*start change ... */
		if (timing_change) {

			if (is_blank_page) {

				/*blank page, use classic timing parameter and clk cfg(20MHz)*/

				nand_info.ddr_type = ddr_type;
				if (nand_info.ddr_type == SDR)
					classic_dclk = 30;
				else
					classic_dclk = 20;
				PHY_DBG("PHY_ChangeMode: is_blank_page %d, set dclk to classic cfg %d MHz\n",
						is_blank_page, classic_dclk);

				ret = _change_nand_parameter(nand_info.ddr_type, ddr_type_bak, &ddr_info, classic_dclk);
				if (ret) {
					PHY_ERR("PHY_ChangeMode: _change_nand_parameter error"
						"when use clk %dMhz!\n", classic_dclk);
					return -1;
				}

				ret = NAND_SetClk(NandIndex, classic_dclk, classic_dclk*2);
				if (ret) {
					PHY_ERR("PHY_ChangeMode: change clk to classic"
						"cfg(%d, %d) failed!\n", classic_dclk, classic_dclk*2);
					return -1;
				}

				if (nand_info.ddr_type == SDR) {
					nand_info.serial_access_mode = 1;
					PHY_DBG("PHY_ChangeMode: set edo(%d) for ddr_type(%d) at classic clock cfg %d MHz\n",
						nand_info.serial_access_mode, nand_info.ddr_type, classic_dclk);
				} else {
					nand_info.ddr_edo = 0x2;
					nand_info.ddr_delay = 0x1f;

					NFC_InitDDRParam(0, (nand_info.ddr_edo<<8)|(nand_info.ddr_delay));
					PHY_DBG("PHY_ChangeMode: set edo(%d) and delay(%d) for ddr_type(%d) at classic clock cfg %d MHz\n",
						nand_info.ddr_edo, nand_info.ddr_delay, nand_info.ddr_type, classic_dclk);
				}
				NFC_ChangeInterfaceMode(&nand_info);

			} else {

				/* scan rigth timing parameter accroding to burned data */

				nand_info.ddr_type = ddr_type;

				ret = _change_nand_parameter(nand_info.ddr_type, ddr_type_bak, &ddr_info, sclk0);
				if (ret) {
					PHY_ERR("PHY_ChangeMode: _change_nand_parameter error,"
						"ddr_tpye %d, sclk0 %d\n", nand_info.ddr_type, sclk0);
					return -1;
				}

				/* set sclk0 && sclk1 */
				ret = NAND_SetClk(NandIndex, sclk0, sclk1);
				if (ret) {
					PHY_ERR("PHY_ChangeMode: change clk(%d, %d) failed!\n", sclk0, sclk1);
					return -1;
				}

				if (NandIndex == 0) {
					/* get right timing paramter. only get timing paramter for first chip */
					ret = _get_right_timing_para(&nand_info, ddr_scan_blk_no);
					if (ret < 0) {
						/*failed, */
						PHY_ERR("PHY_ChangeMode: get timing para failed!\n");
						return -1;
					}  else {
						if(sclk0 > 30)
						{
							nand_info.serial_access_mode = 2;
							PHY_DBG("PHY_ChangeMode: sclk0 is beyond 30MHz,then set edo to 2\n");
						}

						/* successful, set parameter*/
						NFC_ChangeInterfaceMode(&nand_info);
					}
				} else if (NandIndex == 1) {
					/* the cfg of channel 1 is same as channel 0 */
					NandIndex = 0;
					NFC_GetInterfaceMode(&nand_info);
					NandIndex = 1;
					NFC_ChangeInterfaceMode(&nand_info);
				}

				if ((nand_info.ddr_type == ONFI_DDR2) || (nand_info.ddr_type == TOG_DDR2)) {

					/* get nand flash timing mode feature and show*/
					feature_addr = 0x01; //timing mode
					_get_nand_feature(feature_addr, feature);
					PHY_DBG("current nand flash's timing mode is: %d\n", feature[0]);

					if (SUPPORT_DDR2_SPECIFIC_CFG) {
						/* set ndfc's ddr2 para */
						_setup_ndfc_ddr2_para(ddr_info);
					}

					/* set ndfc's other timing para */
					reg_val = NDFC_READ_REG_CTL(); //cfg for dqs and re
					reg_val |= (0x3U <<29);
					NDFC_WRITE_REG_CTL(reg_val);

					reg_val = NDFC_READ_REG_INT_DEBUG(); //debug reg(addr 0x184)
					reg_val |= (0x1U <<8);
					NDFC_WRITE_REG_INT_DEBUG(reg_val);

					reg_val = 0;             //set timing cfg
					reg_val = 0xffffffff;
					NDFC_WRITE_REG_TIMING_CFG(reg_val);

				}else if (((ddr_type_bak == ONFI_DDR2) || (ddr_type_bak == TOG_DDR2))
					&& (nand_info.ddr_type == SDR)){

					if (SUPPORT_DDR2_SPECIFIC_CFG) {
						NDFC_WRITE_REG_SPEC_CTL(0x0);
					}

		        	{
						//config for dqs and re
						reg_val = NDFC_READ_REG_CTL();
						reg_val &= (~(0x3U <<29));
						NDFC_WRITE_REG_CTL(reg_val);

						//debug reg
						reg_val = NDFC_READ_REG_INT_DEBUG(); //debug reg(addr 0x184)
						reg_val &= (~(0x1U <<8));
						NDFC_WRITE_REG_INT_DEBUG(reg_val);

						/* configure default timing parameter */
						_set_ndfc_def_timing_param();
					}
				}
			}
		}

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

	return ret;
}


/*
************************************************************************************************************************
*                       NAND FLASH DRIVER PHYSICAL MODULE EXIT
*
* Description: nand flash driver physical module exit.
*
* Aguments   : none
*
* Returns    : the resutl of exit.
*                   = 0     exit successful;
*                   = -1    exit failed.
************************************************************************************************************************
*/
__s32 PHY_Exit(void)
{
    __u32 i = 0;
#if 1
	if (PageCachePool.PageCache0){
		FREE(PageCachePool.PageCache0,SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		PageCachePool.PageCache0 = NULL;
	}
	if (PageCachePool.PageCache1){
		FREE(PageCachePool.PageCache1,SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		PageCachePool.PageCache1 = NULL;
	}
	if (PageCachePool.PageCache2){
		FREE(PageCachePool.PageCache2,SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		PageCachePool.PageCache2 = NULL;
	}
	if (PageCachePool.PageCache3){
		FREE(PageCachePool.PageCache3,SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		PageCachePool.PageCache3 = NULL;
	}
	if (PageCachePool.PageCache4){
		FREE(PageCachePool.PageCache4,SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		PageCachePool.PageCache4 = NULL;
	}
	if (PageCachePool.SpareCache){
		FREE(PageCachePool.SpareCache,SECTOR_CNT_OF_SUPER_PAGE*2 * 4);
		PageCachePool.SpareCache = NULL;
	}
	if (PageCachePool.TmpPageCache){
		FREE(PageCachePool.TmpPageCache,SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		PageCachePool.TmpPageCache = NULL;
	}
	if (PageCachePool.TmpPageCache1){
		FREE(PageCachePool.TmpPageCache1,SECTOR_CNT_OF_SUPER_PAGE*2 * 512);
		PageCachePool.TmpPageCache1 = NULL;
	}
#endif
	NAND_FreeMemoryForDMADescs(&ndfc_dma_desc_cpu_addr, &ndfc_dma_desc_addr);
	ndfc_dma_desc = NULL;
	ndfc_dma_desc_cpu = NULL;

	if((__u32)CHANNEL_CNT>2)
		PHY_ERR("[PHY_ChangeMode]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

	if(SUPPORT_READ_RETRY)
	{
	    for(i=0; i<NandStorageInfo.ChipCnt;i++)
        {
            PHY_SetDefaultParam(i);
        }
        NFC_ReadRetryExit(READ_RETRY_TYPE);
	}

	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
		NFC_RandomDisable();
		NFC_Exit();

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;
	return 0;
}


/*
************************************************************************************************************************
*                       RESET ONE NAND FLASH CHIP
*
*Description: Reset the given nand chip;
*
*Arguments  : nChip     the chip select number, which need be reset.
*
*Return     : the result of chip reset;
*               = 0     reset nand chip successful;
*               = -1    reset nand chip failed.
************************************************************************************************************************
*/
__s32 PHY_ResetChip(__u32 nChip)
{
	__s32 ret = 0;
	__s32 timeout = 0xffff;
	NFC_CMD_LIST cmd;

	if((__u32)CHANNEL_CNT>2)
		PHY_ERR("[PHY_ResetChip]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
		NFC_SelectChip(nChip);

		_add_cmd_list(&cmd, 0xff, 0 , NDFC_IGNORE, NDFC_NO_DATA_FETCH, NDFC_IGNORE, NDFC_IGNORE, NDFC_NO_WAIT_RB);
		ret |= NFC_ResetChip(&cmd);

			/*wait rb0 ready*/
		NFC_SelectRb(0);
		while((timeout--) && (NFC_CheckRbReady(0)));
		if (timeout < 0)
			return -ERR_TIMEOUT;

		  /*wait rb0 ready*/
		NFC_SelectRb(1);
		while((timeout--) && (NFC_CheckRbReady(1)));
		if (timeout < 0)
			return -ERR_TIMEOUT;

		_phy_read_status(nChip);

		NFC_DeSelectChip(nChip);

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

	return ret;
}
__s32 PHY_ResetChip_for_init(__u32 nChip)
{
	__s32 ret = 0;
	__s32 timeout = 0x7fff;
	NFC_CMD_LIST cmd;

	if((__u32)CHANNEL_CNT>2)
		PHY_ERR("[PHY_ResetChip]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
		NFC_SelectChip(nChip);

		_add_cmd_list(&cmd, 0xff, 0 , NDFC_IGNORE, NDFC_NO_DATA_FETCH, NDFC_IGNORE, NDFC_IGNORE, NDFC_NO_WAIT_RB);
		ret |= NFC_ResetChip(&cmd);

			/*wait rb0 ready*/
		NFC_SelectRb(0);
		while((timeout--) && (NFC_CheckRbReady(0)));
		if (timeout < 0)
			return -ERR_TIMEOUT;

		  /*wait rb0 ready*/
		NFC_SelectRb(1);
		while((timeout--) && (NFC_CheckRbReady(1)));
		if (timeout < 0)
			return -ERR_TIMEOUT;

		timeout = 0x7fff;
		while(timeout--);

		NFC_DeSelectChip(nChip);

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

	return ret;
}

__s32 PHY_ResetChip_CurCH(__u32 nChip)
{
	__s32 ret = 0;
	__s32 timeout = 0xffff;
	NFC_CMD_LIST cmd;

	NFC_SelectChip(nChip);

	_add_cmd_list(&cmd, 0xff, 0 , NDFC_IGNORE, NDFC_NO_DATA_FETCH, NDFC_IGNORE, NDFC_IGNORE, NDFC_NO_WAIT_RB);
	ret |= NFC_ResetChip(&cmd);

      	/*wait rb0 ready*/
	NFC_SelectRb(0);
	while((timeout--) && (NFC_CheckRbReady(0)));
	if (timeout < 0)
		return -ERR_TIMEOUT;

      /*wait rb0 ready*/
	NFC_SelectRb(1);
	while((timeout--) && (NFC_CheckRbReady(1)));
	if (timeout < 0)
		return -ERR_TIMEOUT;

	_phy_read_status(nChip);

	NFC_DeSelectChip(nChip);

	return ret;
}

__s32 PHY_SyncResetChip_CurCH(__u32 nChip)
{
	__s32 ret = 0;
	__s32 timeout = 0xffff;
	__u32 reg_val, if_type, cmd_val;
	NFC_CMD_LIST cmd;

	NFC_SelectChip(nChip);

	if (SUPPORT_SYNC_RESET_OP) {

		reg_val = NDFC_READ_REG_CTL();
		if_type = ((reg_val>>18) & 3);
		if (NdfcVersion == NDFC_VERSION_V2)
			if_type |= (((reg_val>>28) & 0x1) <<4);

		if ((if_type == ONFI_DDR) || (if_type == ONFI_DDR2))
			cmd_val = 0xfc;  //onfi sync reset cmd
		else
			cmd_val = 0xff;

	} else {
		cmd_val = 0xff;
	}

	_add_cmd_list(&cmd, cmd_val, 0 , NDFC_IGNORE, NDFC_NO_DATA_FETCH, NDFC_IGNORE, NDFC_IGNORE, NDFC_NO_WAIT_RB);
	ret |= NFC_ResetChip(&cmd);

	/*wait rb0 ready*/
	NFC_SelectRb(0);
	while((timeout--) && (NFC_CheckRbReady(0)));
	if (timeout < 0)
		return -ERR_TIMEOUT;

	/*wait rb0 ready*/
	NFC_SelectRb(1);
	while((timeout--) && (NFC_CheckRbReady(1)));
	if (timeout < 0)
		return -ERR_TIMEOUT;

	_phy_read_status(nChip);

	NFC_DeSelectChip(nChip);

	return ret;
}


__s32  PHY_SyncResetChip(__u32 nChip)
{
	__s32 ret = 0;
	__s32 timeout = 0xffff;
	__u32 reg_val, if_type, cmd_val;
	NFC_CMD_LIST cmd;

	if((__u32)CHANNEL_CNT>2)
		PHY_ERR("[PHY_ResetChip]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{

		reg_val = NDFC_READ_REG_CTL();
		if_type = ((reg_val>>18) & 3);
		if_type |= (((reg_val>>28) & 0x1) <<4);
		if ((if_type == ONFI_DDR) || (if_type == ONFI_DDR2))
			cmd_val = 0xfc;  //onfi sync reset cmd
		else
			cmd_val = 0xff;

		_add_cmd_list(&cmd, cmd_val, 0 , NDFC_IGNORE, NDFC_NO_DATA_FETCH, NDFC_IGNORE, NDFC_IGNORE, NDFC_NO_WAIT_RB);
		ret |= NFC_ResetChip(&cmd);

	      	/*wait rb0 ready*/
		NFC_SelectRb(0);
		while((timeout--) && (NFC_CheckRbReady(0)));
		if (timeout < 0)
			return -ERR_TIMEOUT;

	      /*wait rb0 ready*/
		NFC_SelectRb(1);
		while((timeout--) && (NFC_CheckRbReady(1)));
		if (timeout < 0)
			return -ERR_TIMEOUT;

		_phy_read_status(nChip);

		NFC_DeSelectChip(nChip);

		if(NandIndex == (CHANNEL_CNT-1))
		break;
	}

	NandIndex = 0;

	return ret;
}

/*
************************************************************************************************************************
*                       READ NAND FLASH ID
*
*Description: Read nand flash ID from the given nand chip.
*
*Arguments  : nChip         the chip number whoes ID need be read;
*             pChipID       the po__s32er to the chip ID buffer.
*
*Return     : read nand chip ID result;
*               = 0     read chip ID successful, the chip ID has been stored in given buffer;
*               = -1    read chip ID failed.
************************************************************************************************************************
*/
__s32  PHY_ReadNandId(__s32 nChip, void *pChipID)
{
	__s32 i,ret=0;
	__u8  chipid[2][8];
	__u8  *temp_id;
	void *id;
	NFC_CMD_LIST cmd;
	__u8 addr = 0;

	if((__u32)CHANNEL_CNT>2)
			PHY_ERR("[PHY_ReadNandId]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
		NFC_SelectChip(nChip);

		id = (void *)(&chipid[NandIndex][0]);
		//_add_cmd_list(&cmd, 0x90,1 , &addr, NDFC_DATA_FETCH, NDFC_IGNORE, 5, NDFC_NO_WAIT_RB);
		// toshiba 24nm flash has 6 bytes id
		_add_cmd_list(&cmd, 0x90,1 , &addr, NDFC_DATA_FETCH, NDFC_IGNORE, 6, NDFC_NO_WAIT_RB);
		ret |= NFC_GetId(&cmd, id);
		PHY_DBG("[PHY_DBG] CH %d Nand flash chip id is:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				NandIndex, chipid[NandIndex][0],chipid[NandIndex][1],chipid[NandIndex][2],chipid[NandIndex][3], chipid[NandIndex][4],chipid[NandIndex][5]);
		NFC_DeSelectChip(nChip);

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

	if(nChip == 0)
	{
	    for(i=0;i<4;i++)
    	{
    		if(chipid[0][i] != chipid[1][i])
    		{
    			PHY_DBG("nand id of two channel is not the same, set to 1 channel mode\n");
    			CHANNEL_CNT = 1;
    			break;
    		}
    	}
	}

	temp_id = (__u8 *)((__u32)pChipID);
	for(i=0;i<8;i++)
		temp_id[i] = chipid[0][i];

	return ret;
}

/*
************************************************************************************************************************
*                       CHECK WRITE PROTECT STATUS
*
*Description: check the status of write protect.
*
*Arguments  : nChip     the number of chip, which nand chip need be checked.
*
*Return     : the result of status check;
*             = 0       the nand flash is not write proteced;
*             = 1       the nand flash is write proteced;
*             = -1      check status failed.
************************************************************************************************************************
*/
__s32  PHY_CheckWp(__u32 nChip)
{
	__s32 ret=0,status[2];
	__u32 rb;

	for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
	{
		rb = _cal_real_rb(nChip);
		NFC_SelectChip(nChip);
		NFC_SelectRb(rb);

		status[NandIndex] = _read_status(0x70,nChip);
		NFC_DeSelectChip(nChip);
		NFC_DeSelectRb(rb);

		if (status[NandIndex] < 0)
		{
			ret |= status[NandIndex];
			break;
		}

		if (status[NandIndex] & NAND_WRITE_PROTECT){
			ret |= 1;
		}
		else
			ret |= 0;

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

	return ret;
}

void _pending_rb_irq_sem(void)
{
	return;
}

void _do_irq(void)
{

}

__s32 PHY_SimpleRead(struct boot_physical_param *readop)
{
	NandIndex = 0;
	return (_read_single_page(readop,0));
}

__s32 PHY_SimpleRead_CurCH(struct boot_physical_param * readop)
{
	//NandIndex = 0;
	return (_read_single_page(readop,0));
}


__s32 PHY_SimpleRead_2CH(struct boot_physical_param *readop)
{
	__u8 sparebuf[2][128];
	__s32 ret;
	void *tmpspare;

	tmpspare = readop->oobbuf;

	sparebuf[0][0] = 0x3c;
	sparebuf[1][0] = 0x3c;

	NandIndex = 0;
	readop->oobbuf = (void *)(&sparebuf[0][0]);
	ret = _read_single_page(readop,0);
	if(CHANNEL_CNT==2)
	{
		NandIndex = 1;
		readop->oobbuf = (void *)(&sparebuf[1][0]);
		ret |= _read_single_page(readop,0);
	}

	NandIndex = 0;

	if((sparebuf[1][0]!=0xff)&&(sparebuf[1][0]!=0x3c))
		sparebuf[0][0] = sparebuf[1][0];

	readop->oobbuf = tmpspare;

	if(readop->oobbuf)
		MEMCPY(readop->oobbuf, &sparebuf[0][0], 16); //8->16

	return (ret);
}




/*
************************************************************************************************************************
*                       SYNC NAND FLASH PHYSIC OPERATION
*
*Description: Sync nand flash operation, check nand flash program/erase operation status.
*
*Arguments  : nBank     the number of the bank which need be synchronized;
*             bMode     the type of synch,
*                       = 0     sync the chip which the bank belonged to, wait the whole chip
*                               to be ready, and report status. if the chip support cacheprogram,
*                               need check if the chip is true ready;
*                       = 1     only sync the the bank, wait the bank ready and report the status,
*                               if the chip support cache program, need not check if the cache is
*                               true ready.
*
*Return     : the result of synch;
*               = 0     synch nand flash successful, nand operation ok;
*               = -1    synch nand flash failed.
************************************************************************************************************************
*/
__s32 PHY_SynchBank(__u32 nBank, __u32 bMode)
{
	__s32 ret = 0,status;
	__u32 chip;
	__u32 rb;
	__u32 cmd_value;
	__s32 timeout = 0xffff;

	if((__u32)CHANNEL_CNT>2)
		PHY_ERR("[PHY_SynchBank]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

	for(NandIndex=0; NandIndex<CHANNEL_CNT; NandIndex++)
	{
		ret = 0;
		/*get chip no*/
		chip = _cal_real_chip(nBank);
		rb = _cal_real_rb(chip);

		if (0xff == chip){
			PHY_ERR("PHY_SynchBank : beyond chip count\n");
			return -ERR_INVALIDPHYADDR;
		}

		if ( (bMode == 1) && SUPPORT_INT_INTERLEAVE){
			if (nBank%BNK_CNT_OF_CHIP == 0)
				cmd_value = NandStorageInfo.OptPhyOpPar.InterBnk0StatusCmd;
			else
				cmd_value = NandStorageInfo.OptPhyOpPar.InterBnk1StatusCmd;
		}
		else{
			if (SUPPORT_MULTI_PROGRAM)
				cmd_value = NandStorageInfo.OptPhyOpPar.MultiPlaneStatusCmd;
			else
				cmd_value = 0x70;
		}

		/*if support rb irq , last op is erase or write*/
		if (SUPPORT_RB_IRQ)
			_pending_rb_irq_sem();

		_wait_rb_ready_int(chip);

		NFC_SelectChip(chip);
		NFC_SelectRb(rb);

		while(1){
			status = _read_status(cmd_value,nBank%BNK_CNT_OF_CHIP);
			if (status < 0)
			{
			    PHY_ERR("PHY_SynchBank %d: read status invalid ,chip = %x, bank = %x, cmd value = %x, status = %x\n",NandIndex,chip);
			    PHY_ERR("PHY_SynchBank bank = %x, cmd value = %x\n",nBank,cmd_value);
			    PHY_ERR("PHY_SynchBank status = %x\n",status);
			    return status;
			}
			if (status & NAND_STATUS_READY)
				break;

			if (timeout-- < 0){
				PHY_ERR("PHY_SynchBank Nandindex %d: wait nand ready timeout,chip = %x \n",NandIndex,chip);
			    PHY_ERR("PHY_SynchBank bank = %x, cmd value = %x\n",nBank,cmd_value);
			    PHY_ERR("PHY_SynchBank status = %x\n",status);
				return -ERR_TIMEOUT;
			}
		}
		if(status & NAND_OPERATE_FAIL)
		{
		    PHY_ERR("PHY_SynchBank %d: last W/E operation fail,chip = %x\n",NandIndex,chip);
		    PHY_ERR("PHY_SynchBank bank = %x, cmd value = %x\n",nBank,cmd_value);
	            PHY_ERR("PHY_SynchBank status = %x\n",status);
		    ret = NAND_OP_FALSE;
			return ret;
		}

		NFC_DeSelectChip(chip);
		NFC_DeSelectRb(rb);

		if(NandIndex == (CHANNEL_CNT-1))
			break;
	}

	NandIndex = 0;

	return ret;
}
