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
*			applicable to NDFC v2.0.
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
#ifndef _NDFC_REG_V2_H_
#define _NDFC_REG_V2_H_

#include "nfc.h"

/* offset */
#define NDFC_REG_o_CTL_v2              0x0000
#define NDFC_REG_o_ST_v2               0x0004
#define NDFC_REG_o_INT_v2              0x0008
#define NDFC_REG_o_TIMING_CTL_v2       0x000C
#define NDFC_REG_o_TIMING_CFG_v2       0x0010
#define NDFC_REG_o_ADDR_LOW_v2         0x0014
#define NDFC_REG_o_ADDR_HIGH_v2        0x0018
#define NDFC_REG_o_BLOCK_MASK_v2       0x001c //#define NFC_REG_o_SECTOR_NUM       0x001C
#define NDFC_REG_o_CNT_v2              0x0020
#define NDFC_REG_o_CMD_v2              0x0024
#define NDFC_REG_o_RCMD_SET_v2         0x0028
#define NDFC_REG_o_WCMD_SET_v2         0x002C
#define NDFC_REG_o_IO_DATA_v2          0x0300
#define NDFC_REG_o_ECC_CTL_v2          0x0034
#define NDFC_REG_o_ERR_ST_v2           0x0038 //#define NFC_REG_o_ECC_ST_v2           0x0038
#define NDFC_REG_o_PAT_ST_v2           0x003C
#define NDFC_REG_o_DEBUG_v2            0x0040

#define NDFC_REG_o_ECC_CNT_x_BASE_v2   0x0050
#define NDFC_REG_o_ECC_CNT0_v2         0x0050
#define NDFC_REG_o_ECC_CNT1_v2         0x0054
#define NDFC_REG_o_ECC_CNT2_v2         0x0058
#define NDFC_REG_o_ECC_CNT3_v2         0x005C
#define NDFC_REG_o_ECC_CNT4_v2         0x0060
#define NDFC_REG_o_ECC_CNT5_v2         0x0064
#define NDFC_REG_o_ECC_CNT6_v2         0x0068
#define NDFC_REG_o_ECC_CNT7_v2         0x006C

#define NDFC_REG_o_USER_DATA_x_BASE_v2 0x0070 //#define NFC_REG_o_USER_DATA_BASE   0x0050

#define NDFC_REG_o_EFNAND_STATUS_v2    0x00F0
#define NDFC_REG_o_SPARE_AREA_v2       0x00F4

#define NDFC_REG_o_PATTERN_ID_0_v2     0x00F8
#define NDFC_REG_o_PATTERN_ID_1_v2     0x00FC

#define NDFC_REG_o_SPEC_CTL_v2         0x0100

#define NDFC_REG_o_DMA_DL_BASE_v2      0x0200
#define NDFC_REG_o_DMA_INT_STA_v2      0x0204
#define NDFC_REG_o_DMA_INT_MASK_v2     0x0208
#define NDFC_REG_o_DMA_CUR_DESC_v2     0x020C
#define NDFC_REG_o_DMA_CUR_BUF_v2      0x0210

#define NDFC_o_RAM0_BASE_v2            0x0400
#define NDFC_o_RAM1_BASE_v2            0x0800

#define NDFC_REG_o_INT_DEBUG_v2        0x0184

//#define NFC_REG_o_MDMA_ADDR         0x00C0
//#define NFC_REG_o_DMA_CNT           0x00C4
//#define NFC_REG_o_DMA_CNT           0x0214


  /* registers */
#define NDFC_REG_CTL_v2                __NDFC_REG( NDFC_REG_o_CTL_v2           )
#define NDFC_REG_ST_v2                 __NDFC_REG( NDFC_REG_o_ST_v2            )
#define NDFC_REG_INT_v2                __NDFC_REG( NDFC_REG_o_INT_v2           )
#define NDFC_REG_TIMING_CTL_v2         __NDFC_REG( NDFC_REG_o_TIMING_CTL_v2    )
#define NDFC_REG_TIMING_CFG_v2         __NDFC_REG( NDFC_REG_o_TIMING_CFG_v2    )
#define NDFC_REG_ADDR_LOW_v2           __NDFC_REG( NDFC_REG_o_ADDR_LOW_v2      )
#define NDFC_REG_ADDR_HIGH_v2          __NDFC_REG( NDFC_REG_o_ADDR_HIGH_v2     )
#define NDFC_REG_BLOCK_MASK_v2         __NDFC_REG( NDFC_REG_o_BLOCK_MASK_v2    )
#define NDFC_REG_CNT_v2                __NDFC_REG( NDFC_REG_o_CNT_v2           )
#define NDFC_REG_CMD_v2                __NDFC_REG( NDFC_REG_o_CMD_v2           )
#define NDFC_REG_RCMD_SET_v2           __NDFC_REG( NDFC_REG_o_RCMD_SET_v2      )
#define NDFC_REG_WCMD_SET_v2           __NDFC_REG( NDFC_REG_o_WCMD_SET_v2      )
#define NDFC_REG_IO_DATA_v2            __NDFC_REG( NDFC_REG_o_IO_DATA_v2       )
#define NDFC_REG_ECC_CTL_v2            __NDFC_REG( NDFC_REG_o_ECC_CTL_v2       )
#define NDFC_REG_ERR_ST_v2             __NDFC_REG( NDFC_REG_o_ERR_ST_v2        )
#define NDFC_REG_PAT_ST_v2             __NDFC_REG( NDFC_REG_o_PAT_ST_v2        )
#define NDFC_REG_DEBUG_v2              __NDFC_REG( NDFC_REG_o_DEBUG_v2         )
#define NDFC_REG_SPEC_CTL_v2           __NDFC_REG( NDFC_REG_o_SPEC_CTL_v2      )
#define NDFC_REG_ECC_CNT0_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT0_v2      )
#define NDFC_REG_ECC_CNT1_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT1_v2      )
#define NDFC_REG_ECC_CNT2_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT2_v2      )
#define NDFC_REG_ECC_CNT3_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT3_v2      )
#define NDFC_REG_ECC_CNT4_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT4_v2      )
#define NDFC_REG_ECC_CNT5_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT5_v2      )
#define NDFC_REG_ECC_CNT6_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT6_v2      )
#define NDFC_REG_ECC_CNT7_v2           __NDFC_REG( NDFC_REG_o_ECC_CNT7_v2      )
#define NDFC_REG_USER_DATA_v2(blk_num) __NDFC_REG( NDFC_REG_o_USER_DATA_x_BASE_v2 + 4 * (blk_num) )
#define NDFC_REG_EFNAND_STATUS_v2      __NDFC_REG( NDFC_REG_o_EFNAND_STATUS_v2 )
#define NDFC_REG_SPARE_AREA_v2         __NDFC_REG( NDFC_REG_o_SPARE_AREA_v2    )
#define NDFC_REG_PATTERN_ID_0_v2       __NDFC_REG( NDFC_REG_o_PATTERN_ID_0_v2  )
#define NDFC_REG_PATTERN_ID_1_v2       __NDFC_REG( NDFC_REG_o_PATTERN_ID_1_v2  )
#define NDFC_REG_SPEC_CTL_v2		   __NDFC_REG( NDFC_REG_o_SPEC_CTL_v2      )
#define NDFC_REG_DMA_DL_BASE_v2        __NDFC_REG( NDFC_REG_o_DMA_DL_BASE_v2   )
#define NDFC_REG_DMA_INT_STA_v2        __NDFC_REG( NDFC_REG_o_DMA_INT_STA_v2   )
#define NDFC_REG_DMA_INT_MASK_v2       __NDFC_REG( NDFC_REG_o_DMA_INT_MASK_v2  )
#define NDFC_REG_DMA_CUR_DESC_v2 	   __NDFC_REG( NDFC_REG_o_DMA_CUR_DESC_v2  )
#define NDFC_REG_DMA_CUR_BUF_v2        __NDFC_REG( NDFC_REG_o_DMA_CUR_BUF_v2   )
#define NDFC_RAM0_BASE_v2              ( NAND_IO_BASE + NDFC_o_RAM0_BASE_v2    )
#define NDFC_RAM1_BASE_v2              ( NAND_IO_BASE + NDFC_o_RAM1_BASE_v2    )
#define NDFC_REG_INT_DEBUG_v2          __NDFC_REG( NDFC_REG_o_INT_DEBUG_v2     )


#define nfc_read_w(n)   	(*((volatile unsigned int   *)(n)))          /* word input */
#define nfc_write_w(n,c) 	(*((volatile unsigned int   *)(n)) = (c))    /* word output */

#if 0
#define NFC_ERR_CNT_REG(blk_num)          (NAND_IO_BASE + NFC_REG_o_ECC_CNT_x_BASE + (((blk_num)&0x1F)>>2)*0x4)
#define GET_ERR_CNT(blk_num)              ((nfc_read_w(NFC_ERR_CNT_REG(blk_num))>>((((blk_num)&0x1F)%4)*8))&0xFF)

#define NFC_USER_DATA_REG(blk_num)        (NFC_USER_DATA_x_BASE + ((blk_num)&0x1F)*0x4)
#define GET_USER_DATA(blk_num)            (nfc_read_w(NFC_USER_DATA_REG(blk_num)))
#define SET_USER_DATA(blk_num, udata)     ( nfc_write_w(NFC_USER_DATA_REG(blk_num), udata) )
#endif


#define NDFC_DESC_FIRST_FLAG    (0x1<<3)
#define NDFC_DESC_LAST_FLAG     (0x1<<2)
#define NDFC_DESC_BSIZE(bsize)  ((bsize)&0xFFFF)  //??? 64KB
typedef struct {
	__u32 	cfg;
	__u32 	bcnt;
	__u32	buff; /*buffer address*/
	struct _ndfc_dma_desc_t *next; /*pointer to next descriptor*/
} _ndfc_dma_desc_t;

extern _ndfc_dma_desc_t *ndfc_dma_desc ;
extern _ndfc_dma_desc_t *ndfc_dma_desc_cpu ;

#endif //_NDFC_REG_V2_H_

