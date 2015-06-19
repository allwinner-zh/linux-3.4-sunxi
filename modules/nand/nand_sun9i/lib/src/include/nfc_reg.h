/*
************************************************************************************************************************
*                                                  NAND BSP for sun
*                                 NAND hardware registers definition and BSP interfaces
*                             				Copyright(C), 2006-2008, uLIVE
*											       All Rights Reserved
*
* File Name : nfc_reg.h

* Author : Gavin.Wen
*
* Version : 1.0.0
*
* Date : 2013.07.19
*
* Description : This file provides functions to read and write NDFC's registers.
*			
* 
* Others : None at present.
*
*
* History :
*
*  <Author>        <time>       <version>      <description>
*
* Gavin.Wen      2013.07.19       1.0.0        build the file
*
************************************************************************************************************************
*/

#ifndef __NDFC_REG_H
#define __NDFC_REG_H

#include "nfc_reg_v1.h"
#include "nfc_reg_v2.h"


#define NDFC_VERSION_V1 	(1)
#define NDFC_VERSION_V2 	(2)

extern __u32 NdfcVersion ;
extern __u32 NdfcDmaMode ;


/*define bit use in NFC_CTL*/
#define NDFC_EN					(1 << 0)
#define NDFC_RESET				(1 << 1)
#define NDFC_BUS_WIDYH			(1 << 2)
#define NDFC_RB_SEL				(1 << 3)
#define NDFC_CE_SEL				(7 << 24)
#define NDFC_CE_CTL				(1 << 6)
#define NDFC_CE_CTL1			(1 << 7)
#define NDFC_PAGE_SIZE			(0xf << 8)
//#define NDFC_SAM				(1 << 12) //20130703-Gavin
#define NDFC_RAM_METHOD			(1 << 14)
#define NDFC_DEBUG_CTL			(1 << 31)

/*define	bit use in NFC_ST*/
#define NDFC_RB_B2R				(1 << 0)
#define NDFC_CMD_INT_FLAG		(1 << 1)
#define NDFC_DMA_INT_FLAG		(1 << 2)
#define NDFC_CMD_FIFO_STATUS	(1 << 3)
#define NDFC_STA				(1 << 4)
//#define NDFC_NATCH_INT_FLAG	(1 << 5) //20130703-Gavin
#define NDFC_RB_STATE0			(1 << 8)
#define NDFC_RB_STATE1			(1 << 9)
#define NDFC_RB_STATE2			(1 << 10)
#define NDFC_RB_STATE3			(1 << 11)

/*define bit use in NFC_INT*/
#define NDFC_B2R_INT_ENABLE		(1 << 0)
#define NDFC_CMD_INT_ENABLE		(1 << 1)
#define NDFC_DMA_INT_ENABLE		(1 << 2)


/*define bit use in NFC_CMD*/
#define NDFC_CMD_LOW_BYTE		(0xff << 0)
//#define NDFC_CMD_HIGH_BYTE	(0xff << 8) //20130703-Gavin
#define NDFC_ADR_NUM			(0x7 << 16)
#define NDFC_SEND_ADR			(1 << 19)
#define NDFC_ACCESS_DIR			(1 << 20)
#define NDFC_DATA_TRANS			(1 << 21)
#define NDFC_SEND_CMD1			(1 << 22)
#define NDFC_WAIT_FLAG			(1 << 23)
#define NDFC_SEND_CMD2			(1 << 24)
#define NDFC_SEQ				(1 << 25)
#define NDFC_DATA_SWAP_METHOD	(1 << 26)
#define NDFC_ROW_AUTO_INC		(1 << 27)
#define NDFC_SEND_CMD3          (1 << 28)
#define NDFC_SEND_CMD4          (1 << 29)
#define NDFC_CMD_TYPE			(3 << 30)

/* define bit use in NFC_RCMD_SET*/
#define NDFC_READ_CMD			(0xff<< 0)
#define NDFC_RANDOM_READ_CMD0 	(0xff << 8)
#define NDFC_RANDOM_READ_CMD1 	(0xff << 16)

/*define bit use in NFC_WCMD_SET*/
#define NDFC_PROGRAM_CMD		(0xff << 0)
#define NDFC_RANDOM_WRITE_CMD	(0xff << 8)
#define NDFC_READ_CMD0			(0xff << 16)
#define NDFC_READ_CMD1	        (0xff << 24)

/*define bit use in NFC_ECC_CTL*/
#define NDFC_ECC_EN				(1 << 0)
#define NDFC_ECC_PIPELINE		(1 << 3)
#define NDFC_ECC_EXCEPTION      (1 << 4)
#define NDFC_ECC_BLOCK_SIZE		(1 << 5)
#define NDFC_RANDOM_EN          (1 << 9 )
#define NDFC_RANDOM_DIRECTION   (1 << 10 )
#define NDFC_ECC_MODE			(0xf << 12)
#define NDFC_RANDOM_SEED        (0x7fff << 16))

#define NDFC_IRQ_MAJOR		    13
/*cmd flag bit*/
#define NDFC_PAGE_MODE  		0x1
#define NDFC_NORMAL_MODE  		0x0

#define NDFC_DATA_FETCH 		0x1
#define NDFC_NO_DATA_FETCH 		0x0
#define NDFC_MAIN_DATA_FETCH 	0x1
#define NDFC_SPARE_DATA_FETCH	0x0
#define NDFC_WAIT_RB			0x1
#define NDFC_NO_WAIT_RB			0x0
#define NDFC_IGNORE				0x0

#define NDFC_INT_RB				0
#define NDFC_INT_CMD			1
#define NDFC_INT_DMA			2
#define NDFC_INT_BATCh			5

__s32 ndfc_init_version(void);
__s32 ndfc_init_dma_mode(void);

__u32 NDFC_READ_REG_CTL(void);
void NDFC_WRITE_REG_CTL(__u32 val);
__u32 NDFC_READ_REG_ST(void);
void NDFC_WRITE_REG_ST(__u32 val);
__u32 NDFC_READ_REG_INT(void);
void NDFC_WRITE_REG_INT(__u32 val);
__u32 NDFC_READ_REG_TIMING_CTL(void);
void NDFC_WRITE_REG_TIMING_CTL(__u32 val);
__u32 NDFC_READ_REG_TIMING_CFG(void);
void NDFC_WRITE_REG_TIMING_CFG(__u32 val);
__u32 NDFC_READ_REG_ADDR_LOW(void);
void NDFC_WRITE_REG_ADDR_LOW(__u32 val);
__u32 NDFC_READ_REG_ADDR_HIGH(void);
void NDFC_WRITE_REG_ADDR_HIGH(__u32 val);
__u32 NDFC_READ_REG_SECTOR_NUM(void);
void NDFC_WRITE_REG_SECTOR_NUM(__u32 val);
__u32 NDFC_READ_REG_BLOCK_MASK(void);
void NDFC_WRITE_REG_BLOCK_MASK(__u32 val);
__u32 NDFC_READ_REG_CNT(void);
void NDFC_WRITE_REG_CNT(__u32 val);
__u32 NDFC_READ_REG_CMD(void);
void NDFC_WRITE_REG_CMD(__u32 val);
__u32 NDFC_READ_REG_RCMD_SET(void);
void NDFC_WRITE_REG_RCMD_SET(__u32 val);
__u32 NDFC_READ_REG_WCMD_SET(void);
void NDFC_WRITE_REG_WCMD_SET(__u32 val);
__u32 NDFC_READ_REG_IO_DATA(void);
void NDFC_WRITE_REG_IO_DATA(__u32 val);
__u32 NDFC_READ_REG_ECC_CTL(void);
void NDFC_WRITE_REG_ECC_CTL(__u32 val);
__u32 NDFC_READ_REG_ECC_ST(void);
void NDFC_WRITE_REG_ECC_ST(__u32 val);
__u32 NDFC_READ_REG_ERR_ST(void);
void NDFC_WRITE_REG_ERR_ST(__u32 val);
__u32 NDFC_READ_REG_PAT_ST(void);
void NDFC_WRITE_REG_PAT_ST(__u32 val);
__u32 NDFC_READ_REG_DEBUG(void);
void NDFC_WRITE_REG_DEBUG(__u32 val);
__u32 NDFC_READ_REG_RDATA_ST(void);
void NDFC_WRITE_REG_RDATA_ST(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT0(void);
void NDFC_WRITE_REG_ECC_CNT0(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT1(void);
void NDFC_WRITE_REG_ECC_CNT1(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT2(void);
void NDFC_WRITE_REG_ECC_CNT2(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT3(void);
void NDFC_WRITE_REG_ECC_CNT3(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT4(void);
void NDFC_WRITE_REG_ECC_CNT4(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT5(void);
void NDFC_WRITE_REG_ECC_CNT5(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT6(void);
void NDFC_WRITE_REG_ECC_CNT6(__u32 val);
__u32 NDFC_READ_REG_ECC_CNT7(void);
void NDFC_WRITE_REG_ECC_CNT7(__u32 val);
__u32 NDFC_READ_REG_USER_DATA(__u32 sect_num);
void NDFC_WRITE_REG_USER_DATA(__u32 sect_num, __u32 val);
__u32 NDFC_READ_REG_EFNAND_STATUS(void);
void NDFC_WRITE_REG_EFNAND_STATUS(__u32 val);
__u32 NDFC_READ_REG_SPARE_AREA(void);
void NDFC_WRITE_REG_SPARE_AREA(__u32 val);
__u32 NDFC_READ_REG_PATTERN_ID(void);
void NDFC_WRITE_REG_PATTERN_ID(__u32 val);
__u32 NDFC_READ_REG_PATTERN_ID_0(void);
void NDFC_WRITE_REG_PATTERN_ID_0(__u32 val);
__u32 NDFC_READ_REG_PATTERN_ID_1(void);
void NDFC_WRITE_REG_PATTERN_ID_1(__u32 val);
__u32 NDFC_READ_REG_MDMA_ADDR(void);
void NDFC_WRITE_REG_MDMA_ADDR(__u32 val);
__u32 NDFC_READ_REG_DMA_CNT(void);
void NDFC_WRITE_REG_DMA_CNT(__u32 val);
__u32 NDFC_READ_REG_SPEC_CTL(void);
void NDFC_WRITE_REG_SPEC_CTL(__u32 val);
__u32 NDFC_READ_REG_DMA_DL_BASE(void);
void NDFC_WRITE_REG_DMA_DL_BASE(__u32 val);
__u32 NDFC_READ_REG_DMA_INT_STA(void);
void NDFC_WRITE_REG_DMA_INT_STA(__u32 val);
__u32 NDFC_READ_REG_DMA_INT_MASK(void);
void NDFC_WRITE_REG_DMA_INT_MASK(__u32 val);
__u32 NDFC_READ_REG_DMA_CUR_DESC(void);
void NDFC_WRITE_REG_DMA_CUR_DESC(__u32 val);
__u32 NDFC_READ_REG_DMA_CUR_BUF(void);
void NDFC_WRITE_REG_DMA_CUR_BUF(__u32 val);

__u32 NDFC_READ_RAM0_W(__u32 off);
__u8 NDFC_READ_RAM0_B(__u32 off);
void NDFC_WRITE_RAM0_W(__u32 off, __u32 val);
void NDFC_WRITE_RAM0_B(__u32 off, __u8 val);
__u32 NDFC_READ_RAM1_W(__u32 off);
__u8 NDFC_READ_RAM1_B(__u32 off);
void NDFC_WRITE_RAM1_W(__u32 off, __u32 val);
void NDFC_WRITE_RAM1_B(__u32 off, __u8 val);
__u32 NDFC_READ_REG_INT_DEBUG(void);
void NDFC_WRITE_REG_INT_DEBUG(__u32 val);

#endif //__NDFC_REG_H
