/*
************************************************************************************************************************
*                                                  NAND BSP for sun
*                                 NAND hardware registers definition and BSP interfaces
*                             				Copyright(C), 2006-2008, uLIVE
*											       All Rights Reserved
*
* File Name : nfc_reg_v1.h

* Author : Gavin.Wen
*
* Version : 1.0.0
*
* Date : 2013.07.18
*
* Description : This file provides definition for NDFC's registers. This file is only
*			applicable to NDFC v1.0.
* 
* Others : None at present.
*
*
* History :
*
*  <Author>        <time>       <version>      <description>
*
* Gavin.Wen      2013.07.18       1.1.0        build the file
*
************************************************************************************************************************
*/

#ifndef _NDFC_REG_V1_H_
#define _NDFC_REG_V1_H_ 

#include "nfc.h"

/* offset */
#define NDFC_REG_o_CTL_v1              0x0000
#define NDFC_REG_o_ST_v1               0x0004
#define NDFC_REG_o_INT_v1              0x0008
#define NDFC_REG_o_TIMING_CTL_v1       0x000C
#define NDFC_REG_o_TIMING_CFG_v1       0x0010
#define NDFC_REG_o_ADDR_LOW_v1         0x0014
#define NDFC_REG_o_ADDR_HIGH_v1        0x0018
#define NDFC_REG_o_SECTOR_NUM_v1       0x001C
#define NDFC_REG_o_CNT_v1              0x0020
#define NDFC_REG_o_CMD_v1              0x0024
#define NDFC_REG_o_RCMD_SET_v1         0x0028
#define NDFC_REG_o_WCMD_SET_v1         0x002C
#define NDFC_REG_o_IO_DATA_v1          0x0300
#define NDFC_REG_o_ECC_CTL_v1          0x0034
#define NDFC_REG_o_ECC_ST_v1           0x0038
#define NDFC_REG_o_DEBUG_v1            0x003C
#define NDFC_REG_o_ECC_CNT0_v1         0x0040
#define NDFC_REG_o_ECC_CNT1_v1         0x0044
#define NDFC_REG_o_ECC_CNT2_v1         0x0048
#define NDFC_REG_o_ECC_CNT3_v1         0x004C
#define NDFC_REG_o_USER_DATA_BASE_v1   0x0050
#define NDFC_REG_o_EFNAND_STATUS_v1    0x0090
#define NDFC_REG_o_SPARE_AREA_v1       0x00A0
#define NDFC_REG_o_PATTERN_ID_v1       0x00A4
#define NDFC_REG_o_MDMA_ADDR_v1        0x00C0
#define NDFC_REG_o_DMA_CNT_v1          0x00C4
#define NDFC_o_RAM0_BASE_v1            0x0400
#define NDFC_o_RAM1_BASE_v1            0x0800

/* registers */
#define NDFC_REG_CTL_v1                __NDFC_REG( NDFC_REG_o_CTL_v1             )
#define NDFC_REG_ST_v1                 __NDFC_REG( NDFC_REG_o_ST_v1              )
#define NDFC_REG_INT_v1                __NDFC_REG( NDFC_REG_o_INT_v1             )
#define NDFC_REG_TIMING_CTL_v1         __NDFC_REG( NDFC_REG_o_TIMING_CTL_v1      )
#define NDFC_REG_TIMING_CFG_v1         __NDFC_REG( NDFC_REG_o_TIMING_CFG_v1      )
#define NDFC_REG_ADDR_LOW_v1           __NDFC_REG( NDFC_REG_o_ADDR_LOW_v1        )
#define NDFC_REG_ADDR_HIGH_v1          __NDFC_REG( NDFC_REG_o_ADDR_HIGH_v1       )
#define NDFC_REG_SECTOR_NUM_v1         __NDFC_REG( NDFC_REG_o_SECTOR_NUM_v1      )
#define NDFC_REG_CNT_v1                __NDFC_REG( NDFC_REG_o_CNT_v1             )
#define NDFC_REG_CMD_v1                __NDFC_REG( NDFC_REG_o_CMD_v1             )
#define NDFC_REG_RCMD_SET_v1           __NDFC_REG( NDFC_REG_o_RCMD_SET_v1        )
#define NDFC_REG_WCMD_SET_v1           __NDFC_REG( NDFC_REG_o_WCMD_SET_v1        )
#define NDFC_REG_IO_DATA_v1            __NDFC_REG( NDFC_REG_o_IO_DATA_v1         )
#define NDFC_REG_ECC_CTL_v1            __NDFC_REG( NDFC_REG_o_ECC_CTL_v1         )
#define NDFC_REG_ECC_ST_v1             __NDFC_REG( NDFC_REG_o_ECC_ST_v1          )
#define NDFC_REG_ECC_CNT0_v1           __NDFC_REG( NDFC_REG_o_ECC_CNT0_v1        )
#define NDFC_REG_ECC_CNT1_v1           __NDFC_REG( NDFC_REG_o_ECC_CNT1_v1        )
#define NDFC_REG_ECC_CNT2_v1           __NDFC_REG( NDFC_REG_o_ECC_CNT2_v1        )
#define NDFC_REG_ECC_CNT3_v1           __NDFC_REG( NDFC_REG_o_ECC_CNT3_v1        )
#define NDFC_REG_DEBUG_v1              __NDFC_REG( NDFC_REG_o_DEBUG_v1           )
#define NDFC_REG_USER_DATA_v1(sect_num) __NDFC_REG( NDFC_REG_o_USER_DATA_BASE_v1 + 4 * (sect_num) )
#define NDFC_REG_EFNAND_STATUS_v1      __NDFC_REG( NDFC_REG_o_EFNAND_STATUS_v1   )
#define NDFC_REG_SPARE_AREA_v1         __NDFC_REG( NDFC_REG_o_SPARE_AREA_v1      )
#define NDFC_REG_PATTERN_ID_v1         __NDFC_REG( NDFC_REG_o_PATTERN_ID_v1      )
#define NDFC_REG_MDMA_ADDR_v1          __NDFC_REG( NDFC_REG_o_MDMA_ADDR_v1       )
#define NDFC_REG_DMA_CNT_v1            __NDFC_REG( NDFC_REG_o_DMA_CNT_v1         )
#define NDFC_RAM0_BASE_v1              ( NAND_IO_BASE + NDFC_o_RAM0_BASE_v1      )
#define NDFC_RAM1_BASE_v1              ( NAND_IO_BASE + NDFC_o_RAM1_BASE_v1      )
	
#endif //_NDFC_REG_V1_H_
	
