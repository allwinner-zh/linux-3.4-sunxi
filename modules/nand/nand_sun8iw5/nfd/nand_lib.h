/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NAND_LIB_H__
#define __NAND_LIB_H__

//---------------------------------------------------------------
//  nand driver 版本号
//---------------------------------------------------------------
#define  NAND_VERSION_0                 0x02
#define  NAND_VERSION_1                 0x10

//---------------------------------------------------------------
//  结构体 定义
//---------------------------------------------------------------
typedef struct
{
	__u32		ChannelCnt;
	__u32        ChipCnt;                            //the count of the total nand flash chips are currently connecting on the CE pin
    __u32       ChipConnectInfo;                    //chip connect information, bit == 1 means there is a chip connecting on the CE pin
	__u32		RbCnt;
	__u32		RbConnectInfo;						//the connect  information of the all rb  chips are connected
    __u32        RbConnectMode;						//the rb connect  mode
	__u32        BankCntPerChip;                     //the count of the banks in one nand chip, multiple banks can support Inter-Leave
    __u32        DieCntPerChip;                      //the count of the dies in one nand chip, block management is based on Die
    __u32        PlaneCntPerDie;                     //the count of planes in one die, multiple planes can support multi-plane operation
    __u32        SectorCntPerPage;                   //the count of sectors in one single physic page, one sector is 0.5k
    __u32       PageCntPerPhyBlk;                   //the count of physic pages in one physic block
    __u32       BlkCntPerDie;                       //the count of the physic blocks in one die, include valid block and invalid block
    __u32       OperationOpt;                       //the mask of the operation types which current nand flash can support support
    __u32        FrequencePar;                       //the parameter of the hardware access clock, based on 'MHz'
    __u32        EccMode;                            //the Ecc Mode for the nand flash chip, 0: bch-16, 1:bch-28, 2:bch_32
    __u8        NandChipId[8];                      //the nand chip id of current connecting nand chip
    __u32       ValidBlkRatio;                      //the ratio of the valid physical blocks, based on 1024
	__u32 		good_block_ratio;					//good block ratio get from hwscan
	__u32		ReadRetryType;						//the read retry type
	__u32       DDRType;
	__u32		Reserved[32];
}boot_nand_para_t;

typedef struct boot_flash_info{
	__u32 chip_cnt;
	__u32 blk_cnt_per_chip;
	__u32 blocksize;
	__u32 pagesize;
	__u32 pagewithbadflag; /*bad block flag was written at the first byte of spare area of this page*/
}boot_flash_info_t;


//for simple
struct boot_physical_param{
	__u32   chip; //chip no
	__u32  block; // block no within chip
	__u32  page; // apge no within block
	__u32  sectorbitmap; //done't care
	void   *mainbuf; //data buf
	void   *oobbuf; //oob buf
};

#define ND_MAX_PART_COUNT		15	 									//max part count

struct nand_disk{
	unsigned long size;
	unsigned long offset;
	unsigned char type;
};

//---------------------------------------------------------------
//  函数 定义
//---------------------------------------------------------------

//for logic
extern __s32 LML_Init(void);
extern __s32 LML_Exit(void);
extern __s32 LML_Read(__u32 nLba, __u32 nLength, void* pBuf);
extern __s32 LML_Write(__u32 nLba, __u32 nLength, void* pBuf);
extern __s32 LML_FlushPageCache(void);
extern void  LML_MergeLogBlk_Quit(void);

extern __s32 BMM_GetLogReleasePos(__u32 log_level);
extern __s32 BMM_CheckLastUsedPage(__u32 pos);
extern __s32 BMM_GetLogReleaseLogBlk(__u32 pos);
extern __s32 BMM_RleaseLogBlock(__s32 tmpPst, __s32 start_page, __u32 merge_page_cnt);
extern __s32 BMM_WriteBackAllMapTbl(void);
extern __s32 BMM_GetLogCnt(void);

extern __s32 NAND_CacheFlush(void);
extern __s32 NAND_CacheFlushSingle(void);
extern __s32 NAND_CacheFlushDev(__u32 dev_num);
extern __s32 NAND_CacheFlushLogicBlk(__u32 logicblk);
extern __s32 NAND_CacheRead(__u32 blk, __u32 nblk, void *buf);
extern __s32 NAND_CacheReadSecs(__u32 blk, __u32 nblk, void *buf);
extern __s32 NAND_CacheWrite(__u32 blk, __u32 nblk, void *buf);
extern __s32 NAND_CacheOpen(void);
extern __s32 NAND_CacheClose(void);


//for format
extern __s32 FMT_Init(void);
extern __s32 FMT_Exit(void);
extern __s32 FMT_FormatNand(void);
extern void  ClearNandStruct( void );

//for scan
__s32  SCN_AnalyzeNandSystem(void);

//for physical
extern __s32 PHY_Init(void);
extern __s32 PHY_Exit(void);
extern __s32 PHY_ChangeMode(__u8 serial_mode);
extern __s32 PHY_ScanDDRParam(void);
extern __s32 PHY_SynchBank(__u32 nBank, __u32 bMode);
extern __s32 PHY_ResetChip(__u32 nChip);
extern __s32 PHY_Readretry_reset(void);
extern __s32 PHY_SetDefaultParam(__u32 bank);

//for simplie(boot0)
extern __s32 PHY_SimpleErase(struct boot_physical_param * eraseop);
extern __s32 PHY_SimpleRead(struct boot_physical_param * readop);
extern __s32 PHY_SimpleWrite(struct boot_physical_param * writeop);
extern __s32 PHY_SimpleWrite_1K(struct boot_physical_param * writeop);
extern __s32 PHY_SimpleWrite_Seq(struct boot_physical_param * writeop);
extern __s32 PHY_SimpleRead_Seq(struct boot_physical_param * readop);
extern __s32 PHY_SimpleRead_1K(struct boot_physical_param * readop);
extern __s32 BOOT_AnalyzeNandSystem(void);

//for param get&set
extern __u32 NAND_GetValidBlkRatio(void);
extern __s32 NAND_SetValidBlkRatio(__u32 ValidBlkRatio);
extern __u32 NAND_GetFrequencePar(void);
extern __s32 NAND_SetFrequencePar(__u32 FrequencePar);
extern __u32 NAND_GetNandVersion(void);
extern __s32 NAND_GetParam(boot_nand_para_t * nand_param);
extern __s32 NAND_GetFlashInfo(boot_flash_info_t *info);
extern __u32 NAND_GetDiskSize(void);
extern void  NAND_SetSrcClkName(__u32 pll_name);
extern __u32 NAND_GetChannelCnt(void);
extern __u32 NAND_GetCurrentCH(void);
extern __u32 NAND_SetCurrentCH(__u32 nand_index);
extern __u32 NAND_GetChipConnect(void);
extern __u32 NAND_GetChipCnt(void);
extern __u32 NAND_GetPageSize(void);
extern __u32 NAND_GetLogicPageSize(void);
extern __u32 NAND_GetLogicPageCnt(void);
extern __u32 NAND_GetPageCntPerBlk(void);
extern __u32 NAND_GetBlkCntPerChip(void);
extern __u32 NAND_GetChipCnt(void);
extern __u32 NAND_GetChipConnect(void);
extern __u32 NAND_GetBadBlockFlagPos(void);
extern __u32 NAND_GetReadRetryType(void);
extern __u32 NAND_GetVersion(__u8* nand_version);


//for lsb mode
extern __s32 NFC_LSBEnable(__u32 chip, __u32 read_retry_type);
extern __s32 NFC_LSBDisable(__u32 chip, __u32 read_retry_type);
extern __s32 NFC_LSBInit(__u32 read_retry_type);
extern __s32 NFC_LSBExit(__u32 read_retry_type);

//for rb int
extern void NFC_RbIntEnable(void);
extern void NFC_RbIntDisable(void);
extern void NFC_RbIntClearStatus(void);
extern __u32 NFC_RbIntGetStatus(void);
extern __u32 NFC_GetRbSelect(void);
extern __u32 NFC_GetRbStatus(__u32 rb);
extern __u32 NFC_RbIntOccur(void);

extern void NFC_DmaIntEnable(void);
extern void NFC_DmaIntDisable(void);
extern void NFC_DmaIntClearStatus(void);
extern __u32 NFC_DmaIntGetStatus(void);
extern __u32 NFC_DmaIntOccur(void);

//for mbr
extern int mbr2disks(struct nand_disk* disk_array);

extern int NAND_Print(const char *fmt, ...);


/*
*   Description:
*   1. if u wanna set dma callback hanlder(sleep during dma transfer) to free cpu for other tasks,
*      one must call the interface before nand flash initialization.
      this option is also protected by dma poll method,wakup(succeed or timeout) then check
      dma transfer complete or still running.
*   2. if u use dma poll method,no need to call the interface.
*
*   3. default is unuse dma callback hanlder,that is,dma poll method.
*   4. input para  : 0:dma poll method;  1:dma callback isr,free cpu for other tasks.
*   5. return value: 0:set succeed; -1:set failed.
*/
extern __s32 NAND_SetDrqCbMethod(__u32 used);


#endif  //ifndef __NAND_LOGIC_H__