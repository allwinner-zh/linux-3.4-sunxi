/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include "../include/nand_drv_cfg.h"
#include "../include/nfc.h"
#include "../include/nfc_reg_v1.h"
#include "../include/nfc_reg_v2.h"
#include "../include/nfc_reg.h"

/*
==================================================================================
	            NDFC v1                     -->         NDFC v2
----------------------------------------------------------------------------------
1  (+0x0000)NDFC_REG_CTL_v1                 --> (+0x0000)NDFC_REG_CTL_v2
2  (+0x0004)NDFC_REG_ST_v1                  --> (+0x0004)NDFC_REG_ST_v2
3  (+0x0008)NDFC_REG_INT_v1                 --> (+0x0008)NDFC_REG_INT_v2
4  (+0x000C)NDFC_REG_TIMING_CTL_v1          --> (+0x000C)NDFC_REG_TIMING_CTL_v2
5  (+0x0010)NDFC_REG_TIMING_CFG_v1          --> (+0x0010)NDFC_REG_TIMING_CFG_v2
6  (+0x0014)NDFC_REG_ADDR_LOW_v1            --> (+0x0014)NDFC_REG_ADDR_LOW_v2
7  (+0x0018)NDFC_REG_ADDR_HIGH_v1           --> (+0x0018)NDFC_REG_ADDR_HIGH_v2
8  (+0x001C)NDFC_REG_SECTOR_NUM_v1
9                                 		 	--> (+0x001C)NDFC_REG_BLOCK_MASK_v2
10 (+0x0020)NDFC_REG_CNT_v1                 --> (+0x0020)NDFC_REG_CNT_v2
11 (+0x0024)NDFC_REG_CMD_v1                 --> (+0x0024)NDFC_REG_CMD_v2
12 (+0x0028)NDFC_REG_RCMD_SET_v1            --> (+0x0028)NDFC_REG_RCMD_SET_v2
13 (+0x002C)NDFC_REG_WCMD_SET_v1            --> (+0x002C)NDFC_REG_WCMD_SET_v2
14 (+0x0300)NDFC_REG_IO_DATA_v1             --> (+0x0300)NDFC_REG_IO_DATA_v2
15 (+0x0034)NDFC_REG_ECC_CTL_v1             --> (+0x0034)NDFC_REG_ECC_CTL_v2
16 (+0x0038)NDFC_REG_ECC_ST_v1
17                                 		 	--> (+0x0038)NDFC_REG_ERR_ST_v2
18 										 	--> (+0x003C)NDFC_REG_PAT_ST_v2
19 (+0x003C)NDFC_REG_DEBUG_v1				--> (+0x0040)NDFC_REG_DEBUG_v2
20                                          --> (+0x0044)NDFC_REG_RDATA_ST_v2
21 (+0x0040)NDFC_REG_ECC_CNT0_v1            --> (+0x0050)NDFC_REG_ECC_CNT0_v2
22 (+0x0044)NDFC_REG_ECC_CNT1_v1            --> (+0x0054)NDFC_REG_ECC_CNT1_v2
23 (+0x0048)NDFC_REG_ECC_CNT2_v1            --> (+0x0058)NDFC_REG_ECC_CNT2_v2
24 (+0x004C)NDFC_REG_ECC_CNT3_v1            --> (+0x005C)NDFC_REG_ECC_CNT3_v2
25										 	--> (+0x0060)NDFC_REG_ECC_CNT4_v2
26										 	--> (+0x0064)NDFC_REG_ECC_CNT5_v2
27										 	--> (+0x0068)NDFC_REG_ECC_CNT6_v2
28										 	--> (+0x006C)NDFC_REG_ECC_CNT7_v2
29 (+0x0050)NDFC_REG_USER_DATA_v1(sct_num)	--> (+0x0070)NDFC_REG_USER_DATA_v2(blk_num)
30 (+0x0090)NDFC_REG_EFNAND_STATUS_v1		--> (+0x00F0)NDFC_REG_EFNAND_STATUS_v2
31 (+0x00A0)NDFC_REG_SPARE_AREA_v1	        --> (+0x00F4)NDFC_REG_SPARE_AREA_v2
32 (+0x00A4)NDFC_REG_PATTERN_ID_v1          -->
33 										 	--> (+0x00F8)NDFC_REG_PATTERN_ID_0_v2
34           								--> (+0x00FC)NDFC_REG_PATTERN_ID_1_v2
35 (+0x00C0)NDFC_REG_MDMA_ADDR_v1           -->
36 (+0x00C4)NDFC_REG_DMA_CNT_v1             -->
37                                          --> (+0x0100)NDFC_REG_o_SPEC_CTL_v2
38										 	--> (+0x0200)NDFC_REG_DMA_DL_BASE_v2
39										 	--> (+0x0204)NDFC_REG_DMA_INT_STA_v2
40            							 	--> (+0x0208)NDFC_REG_DMA_INT_MASK_v2
41            							 	--> (+0x020C)NDFC_REG_DMA_CUR_DESC_v2
42 										 	--> (+0x0210)NDFC_REG_DMA_CUR_BUF_v2
43 (+0x0400)NDFC_RAM0_BASE_v1				--> (+0x0400)NDFC_RAM0_BASE_v2
44 (+0x0800)NDFC_RAM1_BASE_v1				--> (+0x0400)NDFC_RAM1_BASE_v2
44                                                                   --> (+0x0400)NDFC_REG_INT_DEBUG_v2
==================================================================================
*/

__u32 NdfcVersion = 0;
__u32 NdfcDmaMode = 0;

__s32 ndfc_init_version(void)
{
	NdfcVersion = NAND_GetNdfcVersion();
	if ((NdfcVersion != 1) && (NdfcVersion != 2)) {
		PHY_ERR("ndfc_init_version: wrong ndfc version, %d\n", NdfcVersion);
		return -1;
	} else {
		PHY_DBG("ndfc version: %d \n", NdfcVersion);
		return 0;
	}
}

__s32 ndfc_init_dma_mode(void)
{
	NdfcDmaMode = NAND_GetNdfcDmaMode();
	if ((NdfcVersion == 1) || (NdfcVersion == 2)) {
		if ((NdfcDmaMode != 0) && (NdfcDmaMode != 1)) {
			PHY_DBG("ndfc_init_dma_mode, wrong dma mode %d for ndfc version %d\n", NdfcDmaMode, NdfcVersion);
			return -1;
		} else {
			PHY_DBG("ndfc dma mode: %s\n", (NdfcDmaMode==0) ? "General DMA" : "MBUS DMA");
			return 0;
		}
	} else {
		PHY_ERR("ndfc_init_dma_mode: wrong ndfc version, %d\n", NdfcVersion);
		return -1;
	}
}

//======================================================================================
//                      1 - REG_CTL
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_CTL(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_CTL_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_CTL_v2;
	else {
		PHY_ERR("NDFC_READ_REG_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_CTL(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_CTL_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_CTL_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      2 - REG_ST
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ST(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ST_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ST_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ST(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ST_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ST_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      3 - REG_INT
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_INT(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_INT_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_INT_v2;
	else {
		PHY_ERR("NDFC_READ_REG_INT(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_INT(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_INT_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_INT_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_INT(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      4 - REG_TIMING_CTL
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_TIMING_CTL(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_TIMING_CTL_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_TIMING_CTL_v2;
	else {
		PHY_ERR("NDFC_READ_REG_TIMING_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_TIMING_CTL(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_TIMING_CTL_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_TIMING_CTL_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_TIMING_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      5 - REG_TIMING_CFG
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_TIMING_CFG(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_TIMING_CFG_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_TIMING_CFG_v2;
	else {
		PHY_ERR("NDFC_READ_REG_TIMING_CFG(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_TIMING_CFG(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_TIMING_CFG_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_TIMING_CFG_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_TIMING_CFG(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      6 - REG_ADDR_LOW
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ADDR_LOW(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ADDR_LOW_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ADDR_LOW_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ADDR_LOW(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ADDR_LOW(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ADDR_LOW_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ADDR_LOW_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ADDR_LOW(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      7 - REG_ADDR_HIGH
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ADDR_HIGH(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ADDR_HIGH_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ADDR_HIGH_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ADDR_HIGH(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ADDR_HIGH(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ADDR_HIGH_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ADDR_HIGH_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ADDR_HIGH(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      8 - REG_SECTOR_NUM
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_SECTOR_NUM(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_SECTOR_NUM_v1;
	else {
		PHY_ERR("NDFC_READ_REG_SECTOR_NUM(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_SECTOR_NUM(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_SECTOR_NUM_v1 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_SECTOR_NUM(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      9 - REG_BLOCK_MASK
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_BLOCK_MASK(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_BLOCK_MASK_v2;
	else {
		PHY_ERR("NDFC_READ_REG_BLOCK_MASK(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_BLOCK_MASK(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_BLOCK_MASK_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_BLOCK_MASK(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      10 - REG_CNT
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_CNT(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_CNT_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_CNT_v2;
	else {
		PHY_ERR("NDFC_READ_REG_CNT(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_CNT(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_CNT_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_CNT_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_CNT(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      11 - REG_CMD
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_CMD(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_CMD_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_CMD_v2;
	else {
		PHY_ERR("NDFC_READ_REG_CMD(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_CMD(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_CMD_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_CMD_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_CMD(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      12 - REG_RCMD_SET
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_RCMD_SET(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_RCMD_SET_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_RCMD_SET_v2;
	else {
		PHY_ERR("NDFC_READ_REG_RCMD_SET(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_RCMD_SET(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_RCMD_SET_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_RCMD_SET_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_RCMD_SET(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      13 - REG_WCMD_SET
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_WCMD_SET(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_WCMD_SET_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_WCMD_SET_v2;
	else {
		PHY_ERR("NDFC_READ_REG_WCMD_SET(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_WCMD_SET(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_WCMD_SET_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_WCMD_SET_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_WCMD_SET(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      14 - REG_IO_DATA
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_IO_DATA(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_IO_DATA_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_IO_DATA_v2;
	else {
		PHY_ERR("NDFC_READ_REG_IO_DATA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_IO_DATA(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_IO_DATA_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_IO_DATA_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_IO_DATA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      15 - REG_ECC_CTL
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CTL(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ECC_CTL_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CTL_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CTL(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ECC_CTL_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CTL_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      16 - REG_ECC_ST
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_ST(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ECC_ST_v1;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_ST(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ECC_ST_v1 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      17 - REG_ERR_ST
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ERR_ST(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ERR_ST_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ERR_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ERR_ST(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ERR_ST_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ERR_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      18 - REG_PAT_ST
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_PAT_ST(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_PAT_ST_v2;
	else {
		PHY_ERR("NDFC_READ_REG_PAT_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_PAT_ST(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_PAT_ST_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_PAT_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      19 - REG_DEBUG
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_DEBUG(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_DEBUG_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_DEBUG_v2;
	else {
		PHY_ERR("NDFC_READ_REG_DEBUG(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_DEBUG(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_DEBUG_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_DEBUG_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DEBUG(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      20 - REG_DEBUG
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_RDATA_ST(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_DEBUG_v2;
	else {
		PHY_ERR("NDFC_READ_REG_RDATA_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_RDATA_ST(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_DEBUG_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_RDATA_ST(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      21 - REG_ECC_CNT0
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT0(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ECC_CNT0_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT0_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT0(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT0(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ECC_CNT0_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT0_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT0(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      22 - REG_ECC_CNT1
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT1(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ECC_CNT1_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT1_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT1(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT1(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ECC_CNT1_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT1_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT1(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      23 - REG_ECC_CNT2
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT2(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ECC_CNT2_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT2_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT2(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT2(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ECC_CNT2_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT2_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT2(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      24 - REG_ECC_CNT3
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT3(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_ECC_CNT3_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT3_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT3(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT3(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_ECC_CNT3_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT3_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT3(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      25 - REG_ECC_CNT4
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT4(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT4_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT4(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT4(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT4_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT4(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      26 - REG_ECC_CNT5
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT5(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT5_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT5(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT5(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT5_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT5(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      27 - REG_ECC_CNT6
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT6(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT6_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT6(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT6(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT6_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT6(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      28 - REG_ECC_CNT7
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_ECC_CNT7(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_ECC_CNT7_v2;
	else {
		PHY_ERR("NDFC_READ_REG_ECC_CNT7(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_ECC_CNT7(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_ECC_CNT7_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_ECC_CNT7(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      29 - REG_USER_DATA
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_USER_DATA(__u32 sect_num)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_USER_DATA_v1(sect_num);
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_USER_DATA_v2(sect_num);
	else {
		PHY_ERR("NDFC_READ_REG_USER_DATA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_USER_DATA(__u32 sect_num, __u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_USER_DATA_v1(sect_num) = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_USER_DATA_v2(sect_num) = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_USER_DATA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      30 - REG_EFNAND_STATUS
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_EFNAND_STATUS(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_EFNAND_STATUS_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_EFNAND_STATUS_v2;
	else {
		PHY_ERR("NDFC_READ_REG_EFNAND_STATUS(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_EFNAND_STATUS(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_EFNAND_STATUS_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_EFNAND_STATUS_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_EFNAND_STATUS(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      31 - REG_SPARE_AREA
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_SPARE_AREA(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_SPARE_AREA_v1;
	else if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_SPARE_AREA_v2;
	else {
		PHY_ERR("NDFC_READ_REG_SPARE_AREA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_SPARE_AREA(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_SPARE_AREA_v1 = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_SPARE_AREA_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_SPARE_AREA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      32 - REG_PATTERN_ID
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_PATTERN_ID(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_PATTERN_ID_v1;
	else {
		PHY_ERR("NDFC_READ_REG_PATTERN_ID(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_PATTERN_ID(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_PATTERN_ID_v1 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_PATTERN_ID(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      33 - REG_PATTERN_ID_0
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_PATTERN_ID_0(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_PATTERN_ID_0_v2;
	else {
		PHY_ERR("NDFC_READ_REG_PATTERN_ID_0(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_PATTERN_ID_0(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_PATTERN_ID_0_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_PATTERN_ID_0(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      34 - REG_PATTERN_ID_1
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_PATTERN_ID_1(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_PATTERN_ID_1_v2;
	else {
		PHY_ERR("NDFC_READ_REG_PATTERN_ID_1(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_PATTERN_ID_1(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_PATTERN_ID_1_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_PATTERN_ID_1(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      35 - REG_MDMA_ADDR
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_MDMA_ADDR(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_MDMA_ADDR_v1;
	else {
		PHY_ERR("NDFC_READ_REG_MDMA_ADDR(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_MDMA_ADDR(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_MDMA_ADDR_v1 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_MDMA_ADDR(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      36 - REG_DMA_CNT
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_DMA_CNT(void)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return NDFC_REG_DMA_CNT_v1;
	else {
		PHY_ERR("NDFC_READ_REG_DMA_CNT(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_DMA_CNT(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		NDFC_REG_DMA_CNT_v1 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DMA_CNT(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      37 - REG_SPEC_CTL
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_SPEC_CTL(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_SPEC_CTL_v2;
	else {
		PHY_ERR("NDFC_READ_REG_SPEC_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_SPEC_CTL(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_SPEC_CTL_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_SPEC_CTL(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      38 - REG_DMA_DL_BASE
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_DMA_DL_BASE(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_DMA_DL_BASE_v2;
	else {
		PHY_ERR("NDFC_READ_REG_DMA_DL_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_DMA_DL_BASE(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_DMA_DL_BASE_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DMA_DL_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      39 - REG_DMA_INT_STA
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_DMA_INT_STA(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_DMA_INT_STA_v2;
	else {
		PHY_ERR("NDFC_READ_REG_DMA_INT_STA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_DMA_INT_STA(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_DMA_INT_STA_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DMA_INT_STA(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      40 - REG_DMA_INT_MASK
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_DMA_INT_MASK(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_DMA_INT_MASK_v2;
	else {
		PHY_ERR("NDFC_READ_REG_DMA_INT_MASK(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_DMA_INT_MASK(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_DMA_INT_MASK_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DMA_INT_MASK(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      41 - REG_DMA_CUR_DESC
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_DMA_CUR_DESC(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_DMA_CUR_DESC_v2;
	else {
		PHY_ERR("NDFC_READ_REG_DMA_CUR_DESC(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_DMA_CUR_DESC(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_DMA_CUR_DESC_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DMA_CUR_DESC(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      42 - REG_DMA_CUR_BUF
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_DMA_CUR_BUF(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_DMA_CUR_BUF_v2;
	else {
		PHY_ERR("NDFC_READ_REG_DMA_CUR_BUF(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_DMA_CUR_BUF(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_DMA_CUR_BUF_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DMA_CUR_BUF(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      43 - RAM0_BASE
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_RAM0_W(__u32 off)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return (*((volatile __u32 *)(NDFC_RAM0_BASE_v1 + off)));
	else if (NdfcVersion == NDFC_VERSION_V2)
		return (*((volatile __u32 *)(NDFC_RAM0_BASE_v2 + off)));
	else {
		PHY_ERR("NDFC_READ_RAM0_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

__u8 NDFC_READ_RAM0_B(__u32 off)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return (*((volatile __u8 *)(NDFC_RAM0_BASE_v1 + off)));
	else if (NdfcVersion == NDFC_VERSION_V2)
		return (*((volatile __u8 *)(NDFC_RAM0_BASE_v2 + off)));
	else {
		PHY_ERR("NDFC_READ_RAM0_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_RAM0_W(__u32 off, __u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		(*((volatile __u32 *)(NDFC_RAM0_BASE_v1 + off))) = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		(*((volatile __u32 *)(NDFC_RAM0_BASE_v2 + off))) = val;
	else {
		PHY_ERR("NDFC_WRITE_RAM0_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}

void NDFC_WRITE_RAM0_B(__u32 off, __u8 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		(*((volatile __u8 *)(NDFC_RAM0_BASE_v1 + off))) = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		(*((volatile __u8 *)(NDFC_RAM0_BASE_v2 + off))) = val;
	else {
		PHY_ERR("NDFC_WRITE_RAM0_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      44 - RAM1_BASE
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_RAM1_W(__u32 off)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return (*((volatile __u32 *)(NDFC_RAM1_BASE_v1 + off)));
	else if (NdfcVersion == NDFC_VERSION_V2)
		return (*((volatile __u32 *)(NDFC_RAM1_BASE_v2 + off)));
	else {
		PHY_ERR("NDFC_READ_RAM1_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

__u8 NDFC_READ_RAM1_B(__u32 off)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		return (*((volatile __u8 *)(NDFC_RAM1_BASE_v1 + off)));
	else if (NdfcVersion == NDFC_VERSION_V2)
		return (*((volatile __u8 *)(NDFC_RAM1_BASE_v2 + off)));
	else {
		PHY_ERR("NDFC_READ_RAM1_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_RAM1_W(__u32 off, __u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		(*((volatile __u32 *)(NDFC_RAM1_BASE_v1 + off))) = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		(*((volatile __u32 *)(NDFC_RAM1_BASE_v2 + off))) = val;
	else {
		PHY_ERR("NDFC_WRITE_RAM1_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}

void NDFC_WRITE_RAM1_B(__u32 off, __u8 val)
{
	if (NdfcVersion == NDFC_VERSION_V1)
		(*((volatile __u8 *)(NDFC_RAM1_BASE_v1 + off))) = val;
	else if (NdfcVersion == NDFC_VERSION_V2)
		(*((volatile __u8 *)(NDFC_RAM1_BASE_v2 + off))) = val;
	else {
		PHY_ERR("NDFC_WRITE_RAM1_BASE(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================


//======================================================================================
//                      42 - REG_DMA_CUR_BUF
//--------------------------------------------------------------------------------------
__u32 NDFC_READ_REG_INT_DEBUG(void)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		return NDFC_REG_INT_DEBUG_v2;
	else {
		PHY_ERR("NDFC_READ_REG_DMA_CUR_BUF(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
		return 0;
	}
}

void NDFC_WRITE_REG_INT_DEBUG(__u32 val)
{
	if (NdfcVersion == NDFC_VERSION_V2)
		NDFC_REG_INT_DEBUG_v2 = val;
	else {
		PHY_ERR("NDFC_WRITE_REG_DMA_CUR_BUF(): wrong ndfc version, %d ----> while(1)\n", NdfcVersion);
		//while(1);
	}
	return ;
}
//======================================================================================

