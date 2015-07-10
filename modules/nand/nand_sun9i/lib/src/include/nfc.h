/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#ifndef _NFC_H_
#define _NFC_H_

#include "nand_drv_cfg.h"

extern __u32 NandIOBase[2];
extern __u32 NandIndex;
#define NAND_IO_BASE    (NandIOBase[NandIndex])
#define __NDFC_REG(x)    (*(volatile unsigned int   *)(NAND_IO_BASE + x))
/*
*********************************************************************************************************
*   Nand Flash Controller define          < maintained by Richard >
*********************************************************************************************************
*/

typedef struct cmd_list{
	struct	cmd_list *next;
	__u8	*addr;
	__u8	addr_cycle;
	__u8	data_fetch_flag;
	__u8	main_data_fetch;
	__u8	wait_rb_flag;
	__u32 	bytecnt;
	__u32	value;
}NFC_CMD_LIST;

typedef struct NFC_init_info{
	__u8	bus_width;// bus width 8 bit
	__u8	rb_sel; // ready busy
	__u8	ce_ctl; // chip select
	__u8	ce_ctl1;
	__u8	pagesize; // 1024 ,2048 ,
	__u8	debug;
	__u8    ddr_type;
	__u8	serial_access_mode; // SAM0 SAM1
	__u32	ddr_edo;
	__u32	ddr_delay;
} NFC_INIT_INFO;

typedef enum NFC_if_type{
	SDR = 0,
	ONFI_DDR = 0x2,
	ONFI_DDR2 = 0x12,
	TOG_DDR = 0x3,
	TOG_DDR2 = 0x13,
} NFC_IF_TYPE;

typedef struct NFC_ddr_info {
	__u32 if_type;
	__u32 timing_mode;

	__u32 en_dqs_c;
	__u32 en_re_c;
	__u32 odt;
	__u32 en_ext_verf;
	__u32 dout_re_warmup_cycle;
	__u32 din_dqs_warmup_cycle;
	__u32 output_driver_strength;
	__u32 rb_pull_down_strength;
} NFC_DDR_INFO;

struct boot_ndfc_cfg {
	__u8 page_size_kb;
	__u8 ecc_mode;
	__u8 sequence_mode;
	__u8 res[5];
};

extern NFC_DDR_INFO NandDdrInfo;

__s32 NFC_ReadRetryInit(__u32 read_retry_type);
__s32 NFC_ReadRetryExit(__u32 read_retry_type);
__s32 NFC_ReadRetry_Prefix_Sandisk_A19(void);
__s32 NFC_DSP_ON_Sandisk_A19(void);
__s32 NFC_Test_Mode_Entry_Sandisk(void);
__s32 NFC_Test_Mode_Exit_Sandisk(void);
__s32 NFC_Change_LMFLGFIX_NEXT_Sandisk(__u8 para);
__s32 NFC_ReadRetry_Enable_Sandisk_A19(void);
__s32 NFC_GetDefaultParam(__u32 chip, __u8 *defautl_value, __u32 read_retry_type);
void NFC_GetOTPValue(__u32 chip, __u8* otp_value, __u32 read_retry_type);
__s32 NFC_SetDefaultParam(__u32 chip, __u8 *defautl_value, __u32 read_retry_type);
__s32 NFC_ReadRetry(__u32 chip, __u32 retry_count, __u32 read_retry_type);
__s32 NFC_LSBEnable(__u32 chip, __u32 read_retry_type);
__s32 NFC_LSBDisable(__u32 chip, __u32 read_retry_type);
__s32 NFC_LSBInit(__u32 read_retry_type);
__s32 NFC_LSBExit(__u32 read_retry_type);
__s32 NFC_SetRandomSeed(__u32 random_seed);
__s32 NFC_RandomEnable(void);
__s32 NFC_RandomDisable(void);
__s32 NFC_Init(NFC_INIT_INFO * nand_info);
void NFC_Exit(void);
__s32 NFC_Read(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_First(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_Wait(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_1K(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_1st_1K_Normal_Ecc(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_Seq(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_CFG(NFC_CMD_LIST  *rcmd, void *mainbuf, void *sparebuf, __u8 dma_wait_mode,__u8 page_mode, struct boot_ndfc_cfg *ndfc_cfg);
__s32 NFC_Read_Spare(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_Spare_First(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Read_Spare_Wait(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Write(NFC_CMD_LIST * wcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode);
__s32 NFC_Write_First(NFC_CMD_LIST * wcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode);
__s32 NFC_Write_Wait(NFC_CMD_LIST * wcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode);
__s32 NFC_Write_Seq(NFC_CMD_LIST * wcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode);
__s32 NFC_Write_CFG( NFC_CMD_LIST  *wcmd, void *mainbuf, void *sparebuf,  __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode, struct boot_ndfc_cfg *ndfc_cfg);
__s32 NFC_Write_1K(NFC_CMD_LIST * wcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode);
__s32 NFC_Erase(NFC_CMD_LIST * ecmd, __u8 rb_wait_mode);
__s32 NFC_CopyBackRead(NFC_CMD_LIST * crcmd);
__s32 NFC_CopyBackWrite(NFC_CMD_LIST * cwcmd, __u8 rb_wait_mode);
__s32 NFC_GetId(NFC_CMD_LIST * idcmd, __u8 * idbuf);
__s32 NFC_GetUniqueId(NFC_CMD_LIST * idcmd, __u8 * idbuf);
__s32 NFC_SelectChip(__u32 chip);
__s32 NFC_DeSelectChip(__u32 chip);
__s32 NFC_SelectRb(__u32 rb);
__s32 NFC_DeSelectRb(__u32 rb);
__s32 NFC_GetStatus(NFC_CMD_LIST * scmd);
__s32 NFC_CheckRbReady(__u32 rb);
__s32 NFC_ChangMode(NFC_INIT_INFO * nand_info);
__s32 NFC_SetEccMode(__u8 ecc_mode);
__s32 NFC_ResetChip(NFC_CMD_LIST * reset_cmd);
__s32 NFC_GetFeature(NFC_CMD_LIST *get_feature_cmd, __u8 *feature);
__s32 NFC_SetFeature(NFC_CMD_LIST *set_feature_cmd, __u8 *feature);
__s32 NFC_ReadRetry_off(__u32 chip); //sandisk readretry exit
__u32 NFC_QueryINT(void);
void NFC_EnableInt(__u8 minor_int);
void NFC_DisableInt(__u8 minor_int);
void NFC_InitDDRParam(__u32 chip, __u32 param);
void NFC_ChangeInterfaceMode(NFC_INIT_INFO *nand_info);
void NFC_GetInterfaceMode(NFC_INIT_INFO *nand_info);
__s32 NFC_Read_Seq_16K(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Write_Seq_16K(NFC_CMD_LIST * wcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode);
__s32 NFC_Read_0xFF(NFC_CMD_LIST * rcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 page_mode);
__s32 NFC_Write_0xFF(NFC_CMD_LIST * wcmd, void * mainbuf, void * sparebuf, __u8 dma_wait_mode, __u8 rb_wait_mode, __u8 page_mode);
__s32 NFC_samsung_slc_mode_exit(void);




//#define NFC_READ_REG(reg)   		(reg)
//#define NFC_WRITE_REG(reg,data) 	(reg) = (data)

#define ERR_ECC 	12
#define ECC_LIMIT 	10
#define ERR_TIMEOUT 14
#define READ_RETRY_MAX_TYPE_NUM 5
#define READ_RETRY_MAX_REG_NUM	16
#define READ_RETRY_MAX_CYCLE	20
#define LSB_MODE_MAX_REG_NUM	8

#ifdef USE_PHYSICAL_ADDRESS
#define NFC_IS_SDRAM(addr)			((addr >= DRAM_BASE)?1:0)
#else
#define NFC_IS_SDRAM(addr)			( ((addr >= DRAM_BASE))&&(addr < SRAM_BASE)?1:0)
#endif

#endif    // #ifndef _NFC_H_
