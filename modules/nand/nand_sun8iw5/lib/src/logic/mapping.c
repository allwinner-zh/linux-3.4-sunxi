/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "../include/nand_drv_cfg.h"
#include "../include/nand_type.h"
#include "../include/nand_physic.h"
#include "../include/nand_logic.h"

extern struct __NandDriverGlobal_t     NandDriverInfo;
extern __u32 DieCntOfNand;
extern struct __SuperPhyBlkType_t tmpMergeFreeBlk;
extern __u32 tmpMergePageNum;
extern __u32 tmpLogicalBlk;


struct __BlkMapTblCachePool_t BlkMapTblCachePool = {0};
struct __PageMapTblCachePool_t PageMapTblCachePool = {0};

void dump(void *buf, __u32 len , __u8 nbyte,__u8 linelen)
{
	__u32 i;
	__u32 tmplen = len/nbyte;

	PRINT("/********************************************/\n");

	for (i = 0; i < tmplen; i++)
	{
		if (nbyte == 1)
			PRINT("%x  ",((__u8 *)buf)[i]);
		else if (nbyte == 2)
			PRINT("%x  ",((__u16 *)buf)[i]);
		else if (nbyte == 4)
			PRINT("%x  ",((__u32 *)buf)[i]);
		else
			break;

		if(i%linelen == (linelen - 1))
			PRINT("\n");
	}

	return;

}

/*
************************************************************************************************************************
*                       CALCULATE THE CHECKSUM FOR A MAPPING TABLE
*
*Description: Calculate the checksum for a mapping table, based on word.
*
*Arguments  : pTblBuf   the pointer to the table data buffer;
*             nLength   the size of the table data, based on word.
*
*Return     : table checksum;
************************************************************************************************************************
*/
static __u32 _GetTblCheckSum(__u32 *pTblBuf, __u32 nLength)
{
    __u32   i;
    __u32   tmpCheckSum = 0;

    for(i= 0; i<nLength; i++)
    {
        tmpCheckSum += pTblBuf[i];
    }

    return tmpCheckSum;
}


/*
************************************************************************************************************************
*                       INIT PAGE MAPPING TABLE CACHE
*
*Description: Init page mapping table cache.
*
*Arguments  : none.
*
*Return     : init result;
*               = 0         init page mapping table cache successful;
*               = -1        init page mapping table cache failed.
************************************************************************************************************************
*/
__s32 PMM_InitMapTblCache(void)
{
    __u32   i;

    PAGE_MAP_CACHE_POOL = &PageMapTblCachePool;

    for(i = 0; i<PAGE_MAP_TBL_CACHE_CNT; i++)
    {
        PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].AccessCnt = 0;
        PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].DirtyFlag = 0;
        PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].LogBlkPst = 0xff;
        PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].ZoneNum = 0xff;
        PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].PageMapTbl = \
                (void *)MALLOC(PAGE_CNT_OF_SUPER_BLK * sizeof(struct __PageMapTblItem_t));
        if (!PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].PageMapTbl)
        {
            return  -ERR_MAPPING;
        }
    }

    return NAND_OP_TRUE;
}


/*
************************************************************************************************************************
*                      CALCUALTE PAGE MAPPING TABLE ACCESS COUNT
*
*Description: Calculate page mapping table access count for table cache switch.
*
*Arguments  : none.
*
*Return     : none.
************************************************************************************************************************
*/
static void _CalPageTblAccessCount(void)
{
    __u32   i;

    for(i=0; i<PAGE_MAP_TBL_CACHE_CNT; i++)
    {
        PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].AccessCnt++;
    }

    PAGE_MAP_CACHE->AccessCnt = 0;
}


/*
************************************************************************************************************************
*                       EXIT PAGE MAPPING TABLE CACHE
*
*Description: Exit page mapping table cache.
*
*Arguments  : none.
*
*Return     : exit result;
*               = 0         exit page mapping table cache successful;
*               = -1        exit page mapping table cache failed.
************************************************************************************************************************
*/
__s32 PMM_ExitMapTblCache(void)
{
    __u32   i;

    for (i = 0; i<PAGE_MAP_TBL_CACHE_CNT; i++)
    {
        FREE(PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].PageMapTbl,PAGE_CNT_OF_SUPER_BLK * sizeof(struct __PageMapTblItem_t));
    }

    return NAND_OP_TRUE;
}


/*the page map table in the cahce pool? cahce hit?*/
static __s32 _page_map_tbl_cache_hit(__u32 nLogBlkPst)
{
    __u32 i;

    for(i=0; i<PAGE_MAP_TBL_CACHE_CNT; i++)
    {
        if((PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].ZoneNum == CUR_MAP_ZONE)\
            && (PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].LogBlkPst == nLogBlkPst))
        {

            PAGE_MAP_CACHE = &(PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i]);
            return NAND_OP_TRUE;
        }
    }

    return NAND_OP_FALSE;

}

/*find post cache, clear cache or LRU cache */
static __u32 _find_page_tbl_post_location(void)
{
    __u32   i, location = 0;
    __u16   access_cnt;

    /*try to find clear cache*/
    for(i=0; i<PAGE_MAP_TBL_CACHE_CNT; i++)
    {
        if(PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].ZoneNum == 0xff)
        {
            return i;
        }
    }

    /*try to find least used cache recently*/
    access_cnt = PAGE_MAP_CACHE_POOL->PageMapTblCachePool[0].AccessCnt;

    for (i = 1; i < PAGE_MAP_TBL_CACHE_CNT; i++){
        if (access_cnt < PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].AccessCnt){
            location = i;
            access_cnt = PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].AccessCnt;
        }
    }

    /*clear access counter*/
    for (i = 0; i < PAGE_MAP_TBL_CACHE_CNT; i++)
        PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].AccessCnt = 0;

    return location;

}

static __s32 _write_back_page_map_tbl(__u32 nLogBlkPst)
{
    __u16 TablePage;
    __u32 TableBlk;
    struct  __NandUserData_t  UserData[2];
    struct  __PhysicOpPara_t  param, tmpPage0;
    struct  __SuperPhyBlkType_t BadBlk,NewBlk;
	__s32 result;


    /*check page poisition, merge if no free page*/
	if((SUPPORT_LOG_BLOCK_MANAGE)&&(LOG_BLK_TBL[nLogBlkPst].LogBlkType == LSB_TYPE))
	{
		TablePage = LOG_BLK_TBL[nLogBlkPst].LastUsedPage + 1;
    	TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;
		DBUG_MSG("[DBUG] _write_back_page_map_tbl, log block: %x, bak log block %x\n", LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum, LOG_BLK_TBL[nLogBlkPst].PhyBlk1.PhyBlkNum);
		DBUG_MSG("[DBUG] _write_back_page_map_tbl, select bak log block\n");
		TablePage = PMM_CalNextLogPage(TablePage);

		if((TablePage >= PAGE_CNT_OF_SUPER_BLK)&&(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex == 0))
		{
			DBUG_MSG("[DBUG] _write_back_page_map_tbl, change to log block 1, phyblock1: %x\n", LOG_BLK_TBL[nLogBlkPst].PhyBlk1.PhyBlkNum);

			LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex = 1;
			TablePage = TablePage - PAGE_CNT_OF_SUPER_BLK;
		}

		if(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex == 1)
    		TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk1.PhyBlkNum;
		else
			TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;

		if (TablePage >= PAGE_CNT_OF_SUPER_BLK){
			//DBUG_INF("[DBUG] _write_back_page_map_tbl, log block full, need merge\n");
	        /*block id full,need merge*/
	        if (LML_MergeLogBlk(SPECIAL_MERGE_MODE,LOG_BLK_TBL[nLogBlkPst].LogicBlkNum)){
	            MAPPING_ERR("write back page tbl : merge err\n");
	            return NAND_OP_FALSE;
	        }
			DBUG_MSG("[DBUG] _write_back_page_map_tbl, log block merge end\n");
	        if (PAGE_MAP_CACHE->ZoneNum != 0xff){
	            /*move merge*/
	            TablePage = LOG_BLK_TBL[nLogBlkPst].LastUsedPage + 1;
	            TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;
				TablePage = PMM_CalNextLogPage(TablePage);
				//DBUG_INF("[DBUG] _write_back_page_map_tbl, after move merge, table block: %x, table page %x\n", TableBlk, TablePage);
	        }
	        else
	            return NAND_OP_TRUE;
	    }
	}
	else
	{
		TablePage = LOG_BLK_TBL[nLogBlkPst].LastUsedPage + 1;
    	TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;

		if (TablePage == PAGE_CNT_OF_SUPER_BLK){
	        /*block id full,need merge*/
	        if (LML_MergeLogBlk(SPECIAL_MERGE_MODE,LOG_BLK_TBL[nLogBlkPst].LogicBlkNum)){
	            MAPPING_ERR("write back page tbl : merge err\n");
	            return NAND_OP_FALSE;
	        }

	        if (PAGE_MAP_CACHE->ZoneNum != 0xff){
	            /*move merge*/
	            TablePage = LOG_BLK_TBL[nLogBlkPst].LastUsedPage + 1;
	            TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;
	        }
	        else
	            return NAND_OP_TRUE;
	    }
	}



rewrite:
//PRINT("-------------------write back page tbl for blk %x\n",TableBlk);
    /*write page map table*/
	if((SUPPORT_LOG_BLOCK_MANAGE)&&(LOG_BLK_TBL[nLogBlkPst].LogBlkType == LSB_TYPE))
	{
		if((TablePage== 0)&&(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex == 1))
	    {
			MEMSET((void *)(&UserData[0]),0xff,sizeof(struct __NandUserData_t) * 2);
	        //log page is the page0 of the logblk1, should copy page0 of logblock0, and skip the page
	        LML_CalculatePhyOpPar(&tmpPage0, CUR_MAP_ZONE, LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum, 0);
	        tmpPage0.SectBitmap = FULL_BITMAP_OF_SUPER_PAGE;
	        tmpPage0.MDataPtr = LML_TEMP_BUF;
	        tmpPage0.SDataPtr = (void *)UserData;
	        result = LML_VirtualPageRead(&tmpPage0);
	        if(result < 0)
	        {
	            LOGICCTL_ERR("[LOGICCTL_ERR] Get log age of data block failed when write logical page, Err:0x%x!\n", result);
	            return -ERR_PHYSIC;
	        }

			//log page is the page0 of the logblk1, should skip the page
			UserData[0].LogType = LSB_TYPE|(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex<<4);
			UserData[1].LogType = LSB_TYPE|(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex<<4);

	        LML_CalculatePhyOpPar(&tmpPage0, CUR_MAP_ZONE, LOG_BLK_TBL[nLogBlkPst].PhyBlk1.PhyBlkNum, 0);
	        tmpPage0.SectBitmap = FULL_BITMAP_OF_SUPER_PAGE;
	        tmpPage0.MDataPtr = LML_TEMP_BUF;
	        tmpPage0.SDataPtr = (void *)UserData;
	        result = LML_VirtualPageWrite(&tmpPage0);

			TablePage++;

	    }
	}

	MEMSET((void *)(&UserData[0]),0xff,sizeof(struct __NandUserData_t) * 2);
    UserData[0].PageStatus = 0xaa;
	if((SUPPORT_LOG_BLOCK_MANAGE)&&(LOG_BLK_TBL[nLogBlkPst].LogBlkType == LSB_TYPE))
	{
		UserData[0].LogType = LSB_TYPE|(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex<<4);
		UserData[1].LogType = LSB_TYPE|(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex<<4);
	}
	else
	{
		UserData[0].LogType = 0xff;
		UserData[1].LogType = 0xff;
	}

	//if((SUPPORT_LOG_BLOCK_MANAGE)&&(LOG_BLK_TBL[nLogBlkPst].LogBlkType == LSB_TYPE)&&(TablePage== 0))
	//{
	//	DBUG_INF("[DBUG] _write_back_page_map_tbl in page0, TablePage: %x, TableBlk: %x\n", TablePage, TableBlk);
	//	DBUG_INF("[DBUG] _write_back_page_map_tbl in page0, logicNum: %x, log0: %x, log1: %x\n", LOG_BLK_TBL[nLogBlkPst].LogicBlkNum,LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum, LOG_BLK_TBL[nLogBlkPst].PhyBlk1.PhyBlkNum);
	//	DBUG_INF("[DBUG] _write_back_page_map_tbl in page0, logicinfo: %x, logicpage: %x\n", UserData[0].LogicInfo, UserData[0].LogicPageNum);
	//	DBUG_INF("[DBUG] _write_back_page_map_tbl in page0, logtype: %x, pagestatus: %x\n", UserData[0].LogType, UserData[0].PageStatus);
	//}

    MEMSET(LML_PROCESS_TBL_BUF,0xff,SECTOR_CNT_OF_SUPER_PAGE * SECTOR_SIZE);

	if(PAGE_CNT_OF_SUPER_BLK >= 512)
	{
		__u32 page;

		for(page = 0; page < PAGE_CNT_OF_SUPER_BLK; page++)
			*((__u16 *)LML_PROCESS_TBL_BUF + page) = PAGE_MAP_TBL[page].PhyPageNum;

		((__u32 *)LML_PROCESS_TBL_BUF)[511] = \
        	_GetTblCheckSum((__u32 *)LML_PROCESS_TBL_BUF, PAGE_CNT_OF_SUPER_BLK*2/(sizeof (__u32)));
	}

	else
	{
		MEMCPY(LML_PROCESS_TBL_BUF, PAGE_MAP_TBL,PAGE_CNT_OF_SUPER_BLK*sizeof(struct __PageMapTblItem_t));
    	((__u32 *)LML_PROCESS_TBL_BUF)[511] = \
        	_GetTblCheckSum((__u32 *)LML_PROCESS_TBL_BUF, PAGE_CNT_OF_SUPER_BLK*sizeof(struct __PageMapTblItem_t)/(sizeof (__u32)));
	}

    param.MDataPtr = LML_PROCESS_TBL_BUF;
    param.SDataPtr = (void *)&UserData;
    param.SectBitmap = FULL_BITMAP_OF_SUPER_PAGE;

//rewrite:
    LML_CalculatePhyOpPar(&param, CUR_MAP_ZONE, TableBlk, TablePage);
    LML_VirtualPageWrite(&param);

    if (NAND_OP_TRUE != PHY_SynchBank(param.BankNum, SYNC_CHIP_MODE)){
        BadBlk.PhyBlkNum = TableBlk;
        if (NAND_OP_TRUE != LML_BadBlkManage(&BadBlk,CUR_MAP_ZONE,TablePage,&NewBlk)){
            MAPPING_ERR("write page map table : bad block mange err after write\n");
            return NAND_OP_FALSE;
        }
        TableBlk = NewBlk.PhyBlkNum;
        LOG_BLK_TBL[nLogBlkPst].PhyBlk = NewBlk;
        goto rewrite;
    }

    LOG_BLK_TBL[nLogBlkPst].LastUsedPage = TablePage;
    PAGE_MAP_CACHE->ZoneNum = 0xff;
    PAGE_MAP_CACHE->LogBlkPst = 0xff;

	if((SUPPORT_LOG_BLOCK_MANAGE)&&(LOG_BLK_TBL[nLogBlkPst].LogBlkType == LSB_TYPE))
		DBUG_MSG("[DBUG] _write_back_page_map_tbl end, lastusedpage: %x, write_index: %x\n", LOG_BLK_TBL[nLogBlkPst].LastUsedPage, LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex);

    return NAND_OP_TRUE;

}

static __s32 _rebuild_page_map_tbl(__u32 nLogBlkPst)
{
    __s32 ret;
    __u16 TablePage;
    __u32 TableBlk, TableBlk1;
    __u16 logicpagenum;
    //__u8  status;
    struct  __NandUserData_t  UserData[2];
    struct  __PhysicOpPara_t  param;

    MEMSET(PAGE_MAP_TBL,0xff, PAGE_CNT_OF_SUPER_BLK*sizeof(struct __PageMapTblItem_t));
    TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;
	TableBlk1 = LOG_BLK_TBL[nLogBlkPst].PhyBlk1.PhyBlkNum;

    param.MDataPtr = LML_PROCESS_TBL_BUF;
    param.SDataPtr = (void *)&UserData;
    param.SectBitmap = 0x3;

	//PRINT("-----------------------rebuild page table for blk %x\n",TableBlk);

    for(TablePage = 0; TablePage < PAGE_CNT_OF_SUPER_BLK; TablePage++){
        LML_CalculatePhyOpPar(&param, CUR_MAP_ZONE, TableBlk, TablePage);
        ret = LML_VirtualPageRead(&param);
        if (ret < 0){
            MAPPING_ERR("rebuild logic block %x page map table : read err\n",LOG_BLK_TBL[nLogBlkPst].LogicBlkNum);
            return NAND_OP_FALSE;
        }

        //status = UserData[0].PageStatus;
        logicpagenum = UserData[0].LogicPageNum;

        //if(((!TablePage || (status == 0x55))) && (logicpagenum != 0xffff) && (logicpagenum < PAGE_CNT_OF_SUPER_BLK)) /*legal page*/
		if((logicpagenum != 0xffff) && (logicpagenum < PAGE_CNT_OF_SUPER_BLK)) /*legal page*/
		{
            PAGE_MAP_TBL[logicpagenum].PhyPageNum = TablePage; /*l2p:logical to physical*/
        }
    }

	if((SUPPORT_LOG_BLOCK_MANAGE)&&(LOG_BLK_TBL[nLogBlkPst].LogBlkType == LSB_TYPE))
	{
		 DBUG_MSG("[DBUG_MSG] _rebuild_page_map_tbl, select bak log block\n");
		 for(TablePage = 0; TablePage < PAGE_CNT_OF_SUPER_BLK; TablePage++){
	        LML_CalculatePhyOpPar(&param, CUR_MAP_ZONE, TableBlk1, TablePage);
	        ret = LML_VirtualPageRead(&param);
	        if (ret < 0){
	            MAPPING_ERR("rebuild logic block %x page map table : read err\n",LOG_BLK_TBL[nLogBlkPst].LogicBlkNum);
	            return NAND_OP_FALSE;
	        }

	        //status = UserData[0].PageStatus;
	        logicpagenum = UserData[0].LogicPageNum;

	        //if(((!TablePage || (status == 0x55))) && (logicpagenum != 0xffff) && (logicpagenum < PAGE_CNT_OF_SUPER_BLK)) /*legal page*/
			if((logicpagenum != 0xffff) && (logicpagenum < PAGE_CNT_OF_SUPER_BLK)) /*legal page*/
			{
	            PAGE_MAP_TBL[logicpagenum].PhyPageNum = TablePage|(0x1U<<15); /*l2p:logical to physical*/
	        }
	    }

	}

    PAGE_MAP_CACHE->DirtyFlag = 1;
	BMM_SetDirtyFlag();

	return NAND_OP_TRUE;
}

static __s32 _read_page_map_tbl(__u32 nLogBlkPst)
{
    __s32 ret;
    __u16 TablePage;
    __u32 TableBlk, checksum;
    __u16 logicpagenum;
    __u8  status;
    struct  __NandUserData_t  UserData[2];
    struct  __PhysicOpPara_t  param;


    /*check page poisition, merge if no free page*/
	if((SUPPORT_LOG_BLOCK_MANAGE)&&(LOG_BLK_TBL[nLogBlkPst].LogBlkType == LSB_TYPE))
	{
		DBUG_MSG("[DBUG_MSG] _read_page_map_tbl, select bak log block\n");
		TablePage = LOG_BLK_TBL[nLogBlkPst].LastUsedPage;

		if(LOG_BLK_TBL[nLogBlkPst].WriteBlkIndex == 1)
    		TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk1.PhyBlkNum;
		else
			TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;
	}
	else
	{
		TablePage = LOG_BLK_TBL[nLogBlkPst].LastUsedPage;
		TableBlk = LOG_BLK_TBL[nLogBlkPst].PhyBlk.PhyBlkNum;
	}

    if (TablePage == 0xffff){
        /*log block is empty*/
        MEMSET(PAGE_MAP_TBL, 0xff,PAGE_CNT_OF_SUPER_BLK*sizeof(struct __PageMapTblItem_t) );
        return NAND_OP_TRUE;
    }

    /*read page map table*/
    param.MDataPtr = LML_PROCESS_TBL_BUF;
    param.SDataPtr = (void *)&UserData;
    param.SectBitmap = 0xf;

    LML_CalculatePhyOpPar(&param, CUR_MAP_ZONE, TableBlk, TablePage);
    ret = LML_VirtualPageRead(&param);

	if(PAGE_CNT_OF_SUPER_BLK >= 512)
	{
		checksum = _GetTblCheckSum((__u32 *)LML_PROCESS_TBL_BUF,  \
                	PAGE_CNT_OF_SUPER_BLK*2/sizeof(__u32));
	}
	else
	{
		checksum = _GetTblCheckSum((__u32 *)LML_PROCESS_TBL_BUF,  \
                	PAGE_CNT_OF_SUPER_BLK*sizeof(struct __PageMapTblItem_t)/sizeof(__u32));
	}

    status = UserData[0].PageStatus;
    logicpagenum = UserData[0].LogicPageNum;

    if((ret < 0) || (status != 0xaa) || (logicpagenum != 0xffff) || (checksum != ((__u32 *)LML_PROCESS_TBL_BUF)[511]))
    {
        if(NAND_OP_TRUE != _rebuild_page_map_tbl(nLogBlkPst))
        {
            MAPPING_ERR("rebuild page map table err\n");
            return NAND_OP_FALSE;
        }
    }
    else
    {
    	if(PAGE_CNT_OF_SUPER_BLK >= 512)
    	{
			__u32 page;

			for(page = 0; page < PAGE_CNT_OF_SUPER_BLK; page++)
				PAGE_MAP_TBL[page].PhyPageNum = *((__u16 *)LML_PROCESS_TBL_BUF + page);
		}
		else
        	MEMCPY(PAGE_MAP_TBL,LML_PROCESS_TBL_BUF, PAGE_CNT_OF_SUPER_BLK*sizeof(struct __PageMapTblItem_t));
    }

    return NAND_OP_TRUE;
}


/*post current zone map table in cache*/
static __s32 _page_map_tbl_cache_post(__u32 nLogBlkPst)
{
    __u8 poisition;
    __u8 i;

    struct __BlkMapTblCache_t *TmpBmt = BLK_MAP_CACHE;

    /*find the cache to be post*/
    poisition = _find_page_tbl_post_location();
    PAGE_MAP_CACHE = &(PAGE_MAP_CACHE_POOL->PageMapTblCachePool[poisition]);

    if (PAGE_MAP_CACHE->DirtyFlag && (PAGE_MAP_CACHE->ZoneNum != 0xff)){
    /*write back page  map table*/
        if (PAGE_MAP_CACHE->ZoneNum != TmpBmt->ZoneNum){
            for (i = 0; i < BLOCK_MAP_TBL_CACHE_CNT; i++)
            {
                if (BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].ZoneNum == PAGE_MAP_CACHE->ZoneNum){
                    BLK_MAP_CACHE = &(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i]);
                    break;
                }
            }

            if (i == BLOCK_MAP_TBL_CACHE_CNT){
                MAPPING_ERR("_page_map_tbl_cache_post : position %d ,page map zone %d,blk map zone %d\n",
							poisition,PAGE_MAP_CACHE->ZoneNum,BLK_MAP_CACHE->ZoneNum);
                return NAND_OP_FALSE;
            }

        }
        /* write back new table in flash if dirty*/
		BMM_SetDirtyFlag();
        if (NAND_OP_TRUE != _write_back_page_map_tbl(PAGE_MAP_CACHE->LogBlkPst)){
            MAPPING_ERR("write back page tbl err\n");
            return NAND_OP_FALSE;
        }

        BLK_MAP_CACHE = TmpBmt;

    }

    PAGE_MAP_CACHE->DirtyFlag = 0;

    /*fetch current page map table*/
    if (NAND_OP_TRUE != _read_page_map_tbl(nLogBlkPst)){
        MAPPING_ERR("read page map tbl err\n");
        return NAND_OP_FALSE;
    }

    PAGE_MAP_CACHE->ZoneNum = CUR_MAP_ZONE;
    PAGE_MAP_CACHE->LogBlkPst = nLogBlkPst;

    return NAND_OP_TRUE;
}

/*
************************************************************************************************************************
*                      SWITCH PAGE MAPPING TABLE
*
*Description: Switch page mapping table cache.
*
*Arguments  : nLogBlkPst    the position of the log block in the log block table.
*
*Return     : switch result;
*               = 0     switch table successful;
*               = -1    switch table failed.
************************************************************************************************************************
*/
__s32 PMM_SwitchMapTbl(__u32 nLogBlkPst)
{
    __s32   result = NAND_OP_TRUE;
    if (NAND_OP_TRUE !=_page_map_tbl_cache_hit(nLogBlkPst))
    {
        result = (_page_map_tbl_cache_post(nLogBlkPst));
    }

    _CalPageTblAccessCount();

    return result;
}


/*
************************************************************************************************************************
*                       INIT BLOCK MAPPING TABLE CACHE
*
*Description: Initiate block mapping talbe cache.
*
*Arguments  : none.
*
*Return     : init result;
*               = 0     init successful;
*               = -1    init failed.
************************************************************************************************************************
*/
__s32 BMM_InitMapTblCache(void)
{
    __u32 i;

    BLK_MAP_CACHE_POOL = &BlkMapTblCachePool;

    BLK_MAP_CACHE_POOL->LogBlkAccessTimer = 0x0;
    BLK_MAP_CACHE_POOL->SuperBlkEraseCnt = 0x0;

    /*init block map table cache*/
    for(i=0; i<BLOCK_MAP_TBL_CACHE_CNT; i++)
    {
        //init the parmater for block mapping table cache management
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].ZoneNum = 0xff;
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DirtyFlag = 0x0;
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].AccessCnt = 0x0;
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].LastFreeBlkPst = 0xff;

        //request buffer for data block table and free block table
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DataBlkTbl = \
                    (struct __SuperPhyBlkType_t *)MALLOC(sizeof(struct __SuperPhyBlkType_t)*BLOCK_CNT_OF_ZONE);
        if(NULL == BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DataBlkTbl)
        {
            MAPPING_ERR("BMM_InitMapTblCache : allocate memory err\n");
            return -ERR_MALLOC;
        }
        //set free block table pointer
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].FreeBlkTbl = \
                    BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DataBlkTbl + DATA_BLK_CNT_OF_ZONE;

        //request buffer for log block table
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].LogBlkTbl =  \
                    (struct __LogBlkType_t *)MALLOC(sizeof(struct __LogBlkType_t)*LOG_BLK_CNT_OF_ZONE);
        if(NULL == BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].LogBlkTbl)
        {
            MAPPING_ERR("BMM_InitMapTblCache : allocate memory err\n");
            FREE(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DataBlkTbl,sizeof(struct __SuperPhyBlkType_t)*BLOCK_CNT_OF_ZONE);
            return -ERR_MALLOC;
        }
    }

    /*init log block access time*/
    MEMSET(BLK_MAP_CACHE_POOL->LogBlkAccessAge, 0x0, MAX_LOG_BLK_CNT);

    return NAND_OP_TRUE;
}


/*
************************************************************************************************************************
*                       CALCULATE BLOCK MAPPING TABLE ACCESS COUNT
*
*Description: Calculate block mapping table access count for cache switch.
*
*Arguments  : none.
*
*Return     : none;
************************************************************************************************************************
*/
static void _CalBlkTblAccessCount(void)
{
    __u32   i;

    for (i=0; i<BLOCK_MAP_TBL_CACHE_CNT; i++)
    {
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].AccessCnt++;
    }

    BLK_MAP_CACHE->AccessCnt = 0;
}


/*
************************************************************************************************************************
*                       BLOCK MAPPING TABLE CACHE EXIT
*
*Description: exit block mapping table cache.
*
*Arguments  : none.
*
*Return     : exit result;
*               = 0     exit successful;
*               = -1    exit failed.
************************************************************************************************************************
*/
__s32 BMM_ExitMapTblCache(void)
{
    __u32 i;

    for (i=0; i<BLOCK_MAP_TBL_CACHE_CNT; i++)
    {

        FREE(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DataBlkTbl,sizeof(struct __SuperPhyBlkType_t)*BLOCK_CNT_OF_ZONE);
        FREE(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].LogBlkTbl,sizeof(struct __LogBlkType_t)*LOG_BLK_CNT_OF_ZONE);
    }

    return NAND_OP_TRUE;
}

/*the zone table in the cahce pool? cahce hit?*/
static __s32 _blk_map_tbl_cache_hit(__u32 nZone)
{
    __u32 i;

    for (i = 0; i < BLOCK_MAP_TBL_CACHE_CNT; i++){
        if (BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].ZoneNum == nZone){
            BLK_MAP_CACHE = &(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i]);
            return NAND_OP_TRUE;
        }
    }

    return NAND_OP_FALSE;

}

/*find post cache, clear cache or LRU cache */
static __u32 _find_blk_tbl_post_location(void)
{
    __u32 i;
    __u8 location;
    __u16 access_cnt ;

    /*try to find clear cache*/
    for (i = 0; i < BLOCK_MAP_TBL_CACHE_CNT; i++)
    {
        if (BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].ZoneNum == 0xff)
            return i;
    }
    /*try to find least used cache recently*/
    location = 0;
    access_cnt = BLK_MAP_CACHE_POOL->BlkMapTblCachePool[0].AccessCnt;

    for (i = 1; i < BLOCK_MAP_TBL_CACHE_CNT; i++){
        if (access_cnt < BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].AccessCnt){
            location = i;
            access_cnt = BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].AccessCnt;
        }
    }

    /*clear access counter*/
    for (i = 0; i < BLOCK_MAP_TBL_CACHE_CNT; i++)
        BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].AccessCnt = 0;

    return location;

}

static __s32 _write_back_all_page_map_tbl(__u8 nZone)
{
    __u32 i;

    for(i=0; i<PAGE_MAP_TBL_CACHE_CNT; i++)
    {
        if((PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].ZoneNum == nZone)\
            && (PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i].DirtyFlag == 1))
        {
            PAGE_MAP_CACHE = &(PAGE_MAP_CACHE_POOL->PageMapTblCachePool[i]);
            if (NAND_OP_TRUE != _write_back_page_map_tbl(PAGE_MAP_CACHE->LogBlkPst))
            {
                MAPPING_ERR("write back all page tbl : write page map table err \n");
                return NAND_OP_FALSE;
            }
            PAGE_MAP_CACHE->DirtyFlag = 0;
        }
    }

    return NAND_OP_TRUE;
}



/*write block map table to flash*/
static __s32 _write_back_block_map_tbl(__u8 nZone)
{
    __s32 TablePage;
    __u32 TableBlk;
    struct  __NandUserData_t  UserData[2];
    struct  __PhysicOpPara_t  param;
    struct __SuperPhyBlkType_t BadBlk,NewBlk;

    /*write back all page map table within this zone*/
    if (NAND_OP_TRUE != _write_back_all_page_map_tbl(nZone)){
        MAPPING_ERR("write back all page map tbl err\n");
        return NAND_OP_FALSE;
    }

    /*set table block number and table page number*/
    TableBlk = NandDriverInfo.ZoneTblPstInfo[nZone].PhyBlkNum;
    TablePage = NandDriverInfo.ZoneTblPstInfo[nZone].TablePst;
    if(TablePage >= PAGE_CNT_OF_SUPER_BLK - 4)
    {
        if(NAND_OP_TRUE != LML_VirtualBlkErase(nZone, TableBlk))
        {
            BadBlk.PhyBlkNum = TableBlk;

            if(NAND_OP_TRUE != LML_BadBlkManage(&BadBlk,CUR_MAP_ZONE,0,&NewBlk))
            {
                MAPPING_ERR("write back block tbl : bad block manage err erase data block\n");
                return NAND_OP_FALSE;
            }

            TableBlk = NewBlk.PhyBlkNum;
        }
        TablePage = -4;
    }

    TablePage += 4;

    //calculate checksum for data block table and free block table
    ((__u32 *)DATA_BLK_TBL)[1023] = \
        _GetTblCheckSum((__u32 *)DATA_BLK_TBL, (DATA_BLK_CNT_OF_ZONE + FREE_BLK_CNT_OF_ZONE));
    //clear full page data
    MEMSET(LML_PROCESS_TBL_BUF, 0xff, SECTOR_CNT_OF_SUPER_PAGE * SECTOR_SIZE);

rewrite:
    /*write back data block and free block map table*/
    MEMSET((void *)&UserData,0xff,sizeof(struct __NandUserData_t) * 2);
    MEMCPY(LML_PROCESS_TBL_BUF,DATA_BLK_TBL,2048);
    /*write page 0, need set spare info*/
    if (TablePage == 0)
    {
        UserData[0].LogicInfo = (1<<14) | ((nZone % ZONE_CNT_OF_DIE) << 10) | 0xaa ;
    }
    UserData[0].PageStatus = 0x55;
    param.MDataPtr = LML_PROCESS_TBL_BUF;
    param.SDataPtr = (void *)&UserData;
    param.SectBitmap = FULL_BITMAP_OF_SUPER_PAGE;
    LML_CalculatePhyOpPar(&param, nZone, TableBlk, TablePage);
    LML_VirtualPageWrite(&param);

    if (NAND_OP_TRUE !=  PHY_SynchBank(param.BankNum, SYNC_CHIP_MODE)){
        BadBlk.PhyBlkNum = TableBlk;
        if (NAND_OP_TRUE != LML_BadBlkManage(&BadBlk,nZone,0,&NewBlk)){
            MAPPING_ERR("write blk map table : bad block mange err after write\n");
            return NAND_OP_FALSE;
        }
        TableBlk = NewBlk.PhyBlkNum;
        TablePage = 0;
        goto rewrite;
    }

    MEMCPY(LML_PROCESS_TBL_BUF, &DATA_BLK_TBL[512], 2048);
    TablePage ++;
    param.MDataPtr = LML_PROCESS_TBL_BUF;
    MEMSET((void *)&UserData,0xff,sizeof(struct __NandUserData_t) * 2);
    UserData[0].PageStatus = 0x55;
    LML_CalculatePhyOpPar(&param, nZone, TableBlk, TablePage);
    LML_VirtualPageWrite(&param);
    if(NAND_OP_TRUE != PHY_SynchBank(param.BankNum, SYNC_CHIP_MODE))
    {
        BadBlk.PhyBlkNum = TableBlk;
        if(NAND_OP_TRUE != LML_BadBlkManage(&BadBlk,nZone,0,&NewBlk))
        {
            MAPPING_ERR("write blk map table : bad block mange err after write\n");
            return NAND_OP_FALSE;
        }
        TableBlk = NewBlk.PhyBlkNum;
        TablePage = 0;
        goto rewrite;
    }


    /*write back log block map table*/
    TablePage++;
    MEMSET(LML_PROCESS_TBL_BUF, 0xff, SECTOR_CNT_OF_SUPER_PAGE * SECTOR_SIZE);
    MEMCPY(LML_PROCESS_TBL_BUF,LOG_BLK_TBL,LOG_BLK_CNT_OF_ZONE*sizeof(struct __LogBlkType_t));
    /*cal checksum*/
    ((__u32 *)LML_PROCESS_TBL_BUF)[511] = \
        _GetTblCheckSum((__u32 *)LML_PROCESS_TBL_BUF, LOG_BLK_CNT_OF_ZONE*sizeof(struct __LogBlkType_t)/sizeof(__u32));
    LML_CalculatePhyOpPar(&param, nZone, TableBlk, TablePage);
    LML_VirtualPageWrite(&param);
    if(NAND_OP_TRUE !=  PHY_SynchBank(param.BankNum, SYNC_CHIP_MODE))
    {
        BadBlk.PhyBlkNum = TableBlk;
        if(NAND_OP_TRUE != LML_BadBlkManage(&BadBlk,nZone,0,&NewBlk))
        {
            MAPPING_ERR("write blk map table : bad block mange err after write\n");
            return NAND_OP_FALSE;
        }
        TableBlk = NewBlk.PhyBlkNum;
        TablePage = 0;
        goto rewrite;
    }

    /*reset zone info*/
    NandDriverInfo.ZoneTblPstInfo[nZone].PhyBlkNum = TableBlk;
    NandDriverInfo.ZoneTblPstInfo[nZone].TablePst = TablePage - 2;

    return NAND_OP_TRUE;
}

/* fetch block map table from flash */
static __s32 _read_block_map_tbl(__u8 nZone)
{
    __s32 TablePage;
    __u32 TableBlk;
    struct  __PhysicOpPara_t  param;

    /*set table block number and table page number*/
    TableBlk = NandDriverInfo.ZoneTblPstInfo[nZone].PhyBlkNum;
    TablePage = NandDriverInfo.ZoneTblPstInfo[nZone].TablePst;

    /*read data block and free block map tbl*/

	param.MDataPtr = LML_PROCESS_TBL_BUF;
    param.SDataPtr = NULL;
    param.SectBitmap = 0xf;
    LML_CalculatePhyOpPar(&param, nZone, TableBlk, TablePage);
    if(LML_VirtualPageRead(&param) < 0)
    {
        MAPPING_ERR("_read_block_map_tbl :read block map table0 err\n");
        return NAND_OP_FALSE;
    }

    MEMCPY(DATA_BLK_TBL,LML_PROCESS_TBL_BUF,2048);

    TablePage++;
    param.MDataPtr = LML_PROCESS_TBL_BUF;
    LML_CalculatePhyOpPar(&param, nZone, TableBlk, TablePage);
    if( LML_VirtualPageRead(&param) < 0)
    {
        MAPPING_ERR("_read_block_map_tbl : read block map table1 err\n");
        return NAND_OP_FALSE;
    }

    MEMCPY(&DATA_BLK_TBL[512],LML_PROCESS_TBL_BUF,2048);
    if(((__u32 *)DATA_BLK_TBL)[1023] != \
        _GetTblCheckSum((__u32 *)DATA_BLK_TBL,(DATA_BLK_CNT_OF_ZONE+FREE_BLK_CNT_OF_ZONE)))
    {
    	MAPPING_ERR("_read_block_map_tbl : read data block map table checksum err\n");
		dump((void*)DATA_BLK_TBL,1024*4,4,8);
		return NAND_OP_FALSE;
    }

    /*read log block table*/
    TablePage++;
    param.MDataPtr = LML_PROCESS_TBL_BUF;
    LML_CalculatePhyOpPar(&param, nZone, TableBlk, TablePage);
    if ( LML_VirtualPageRead(&param) < 0){
        MAPPING_ERR("_read_block_map_tbl : read block map table2 err\n");
        return NAND_OP_FALSE;
    }
    if (((__u32 *)LML_PROCESS_TBL_BUF)[511] != \
        _GetTblCheckSum((__u32 *)LML_PROCESS_TBL_BUF, LOG_BLK_CNT_OF_ZONE*sizeof(struct __LogBlkType_t)/sizeof(__u32)))
    {
    	MAPPING_ERR("_read_block_map_tbl : read log block table checksum err\n");
		dump((void*)LML_PROCESS_TBL_BUF,512*8,2,8);
        return NAND_OP_FALSE;
    }
    MEMCPY(LOG_BLK_TBL,LML_PROCESS_TBL_BUF,LOG_BLK_CNT_OF_ZONE*sizeof(struct __LogBlkType_t));

    return NAND_OP_TRUE;
}

/*post current zone map table in cache*/
static __s32 _blk_map_tbl_cache_post(__u32 nZone)
{
    __u8 poisition;

    /*find the cache to be post*/
    poisition = _find_blk_tbl_post_location();
    BLK_MAP_CACHE = &(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[poisition]);

    /* write back new table in flash if dirty*/
    if (BLK_MAP_CACHE->DirtyFlag){
        if (NAND_OP_TRUE != _write_back_block_map_tbl(CUR_MAP_ZONE)){
            MAPPING_ERR("_blk_map_tbl_cache_post : write back zone tbl err\n");
            return NAND_OP_FALSE;
        }
    }

    /*fetch current zone map table*/
    if (NAND_OP_TRUE != _read_block_map_tbl(nZone)){
        MAPPING_ERR("_blk_map_tbl_cache_post : read zone tbl err\n");
            return NAND_OP_FALSE;
    }
    CUR_MAP_ZONE = nZone;
    BLK_MAP_CACHE->DirtyFlag = 0;

    return NAND_OP_TRUE;
}

/*
************************************************************************************************************************
*                       SWITCH BLOCK MAPPING TABLE
*
*Description: Switch block mapping table.
*
*Arguments  : nZone     zone number which block mapping table need be accessed.
*
*Return     : switch result;
*               = 0     switch successful;
*               = -1    switch failed.
************************************************************************************************************************
*/
__s32 BMM_SwitchMapTbl(__u32 nZone)
{
    __s32   result = NAND_OP_TRUE;

	if(tmpLogicalBlk!= 0xffff)
	{
		PRINT("log release thread is runing!!!!\n");
		while(1);
	}

    if(NAND_OP_TRUE != _blk_map_tbl_cache_hit(nZone))
    {
        MAPPING_DBG("BMM_SwitchMapTbl : post zone %d cache\n",nZone);
		result = (_blk_map_tbl_cache_post(nZone));
    }

    _CalBlkTblAccessCount();

    return result;
}


/*
************************************************************************************************************************
*                           WRITE BACK ALL MAPPING TABLE
*
*Description: Write back all mapping table.
*
*Arguments  : none.
*
*Return     : write table result;
*               = 0     write successful;
*               = -1    write failed.
************************************************************************************************************************
*/
__s32 BMM_WriteBackAllMapTbl(void)
{
     __u8 i;

        /*save current scene*/
        struct __BlkMapTblCache_t *TmpBmt = BLK_MAP_CACHE;
        struct __PageMapTblCache_t *TmpPmt = PAGE_MAP_CACHE;

        for (i = 0; i < BLOCK_MAP_TBL_CACHE_CNT; i++)
        {
            if (BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DirtyFlag){
                   BLK_MAP_CACHE = &(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i]);
                   if (NAND_OP_TRUE != _write_back_block_map_tbl(CUR_MAP_ZONE))
                        return NAND_OP_FALSE;
                    BLK_MAP_CACHE->DirtyFlag = 0;
            }
       }

        /*resore current scene*/
        BLK_MAP_CACHE  = TmpBmt;
        PAGE_MAP_CACHE = TmpPmt;

        return NAND_OP_TRUE;
}

__s32 BMM_MergeAllLogBlock(void)
{
	__u32 tmpZoneNum, ZoneCnt, tmpLogPos;
	__s32 result;

	ZoneCnt = ZONE_CNT_OF_DIE * DieCntOfNand;
	PRINT("BMM_MergeAllLogBlock, ZoneCnt: %x\n", ZoneCnt);

	for(tmpZoneNum=0; tmpZoneNum<ZoneCnt; tmpZoneNum++)
	{
		PRINT("BMM_MergeAllLogBlock, tmpZoneNum: %x\n", tmpZoneNum);

        //swap the block mapping table to ram which is need be accessing currently
        if(tmpLogicalBlk!= 0xffff)
		LML_MergeLogBlk_Quit();
        result = BMM_SwitchMapTbl(tmpZoneNum);
        if(result < 0)
        {
            MAPPING_ERR("[MAPPING_ERR] BMM_MergeAllLogBlock, Switch block mapping table failed when read logical page! Err:0x%x\n", result);
            return -ERR_MAPPING;
        }


		BMM_SetDirtyFlag();

		for(tmpLogPos = 0; tmpLogPos<MAX_LOG_BLK_CNT; tmpLogPos++)
		{
			if(LOG_BLK_TBL[tmpLogPos].LogicBlkNum != 0xffff)
			{
				PRINT("BMM_MergeAllLogBlock, logpos: %x, logblock: %x\n", tmpLogPos, LOG_BLK_TBL[tmpLogPos].LogicBlkNum);
				//need swap the page mapping table to ram which is accessing currently
			    result = PMM_SwitchMapTbl(tmpLogPos);
			    if(result < 0)
			    {
			        MAPPING_ERR("[MAPPING_ERR] BMM_MergeAllLogBlock, Switch page mapping table failed when switch page map table! Err:0x%x\n", result);
			        return -1;
			    }


				result = LML_MergeLogBlk(NORMAL_MERGE_MODE,LOG_BLK_TBL[tmpLogPos].LogicBlkNum);
				if(result<0)
				{
		            MAPPING_ERR("[MAPPING_ERR] BMM_MergeAllLogBlock, merge error! Err:0x%x\n", result);
		            return -ERR_MAPPING;
				}

			}
		}
	}

	PRINT("BMM_MergeAllLogBlock end\n");
	return result;

}


__s32 BMM_GetLogReleasePos(__u32 log_level)
{
	__s32 result;
	__s32 tmpPst=-1, i, ValidLogblkCnt = 0;
        __u16 tmpLogAccessAge = 0xffff;

	 //check if need to release log block
	ValidLogblkCnt = 0;
    for(i=0; i<LOG_BLK_CNT_OF_ZONE; i++)
    {
        if(LOG_BLK_TBL[i].LogicBlkNum != 0xffff)
        {
            ValidLogblkCnt++;
        }
    }

	//valid log block if less than log_level, no need to release log block
	if(ValidLogblkCnt<=log_level)
        return tmpPst;

	//PRINT("BMM_RleaseLogBlock\n");
	BMM_SetDirtyFlag();

	 //check if there is some full log block
    for(i=0; i<LOG_BLK_CNT_OF_ZONE; i++)
    {
        if(LOG_BLK_TBL[i].LastUsedPage == PAGE_CNT_OF_SUPER_BLK-1)
        {
            tmpPst = i;
            break;
        }
    }

    if(tmpPst == -1)
    {
        //there is no full log block, look for an oldest log block to merge
        for(i=0; i<LOG_BLK_CNT_OF_ZONE; i++)
        {
            if((LOG_ACCESS_AGE[i] < tmpLogAccessAge)&&(LOG_BLK_TBL[i].LogicBlkNum != 0xffff))
            {
                tmpLogAccessAge = LOG_ACCESS_AGE[i];
                tmpPst = i;
            }
        }
    }


	//PRINT("BMM_GetLogReleasePos: %d, validlog: %d\n", tmpPst, ValidLogblkCnt);
	//PRINT("nand validlog: %d\n", ValidLogblkCnt);
	#if 0
	for(i=0;i<LOG_BLK_CNT_OF_ZONE; i+=8)
	{
		int j;
		for(j=0; j<8; j++)
		{
			PRINT(" 0x%x", LOG_ACCESS_AGE[i*8+j]);
		}
		PRINT("\n");
	}
	#endif

	return tmpPst;
}

__s32 BMM_GetLogReleaseLogBlk(__u32 pos)
{
	if(pos < MAX_LOG_BLK_CNT)
		return LOG_BLK_TBL[pos].LogicBlkNum;
	else
	{
		PRINT("BMM_GetLogReleaseLogBlk: 0x%x invalid \n", pos);
		return LOG_BLK_TBL[0].LogicBlkNum;
	}

}

__s32 BMM_CheckLastUsedPage(__u32 pos)
{
	if(pos < MAX_LOG_BLK_CNT)
	{
		if(LOG_BLK_TBL[pos].LastUsedPage == (PAGE_CNT_OF_LOGIC_BLK -1))
			return 0;
		else
			return -1;
	}
	else
	{
		PRINT("BMM_CheckLastUsedPage: 0x%x invalid \n", pos);
		return 0;
	}

}



__s32 BMM_GetLogCnt(void)
{

	__s32  i, ValidLogblkCnt = 0;

	 //check if need to release log block
	ValidLogblkCnt = 0;
    for(i=0; i<LOG_BLK_CNT_OF_ZONE; i++)
    {
        if(LOG_BLK_TBL[i].LogicBlkNum != 0xffff)
        {
            ValidLogblkCnt++;
        }
    }

	return ValidLogblkCnt;
}


__s32 BMM_RleaseLogBlock(__s32 tmpPst, __s32 start_page, __u32 merge_page_cnt)
{
	__s32 result;


	//PRINT("BMM_RleaseLogBlock: 0x%x, 0x%x, 0x%x\n", tmpPst, start_page, merge_page_cnt);
	BMM_SetDirtyFlag();

    //switch the page mapping table for merge the log block
    result = PMM_SwitchMapTbl(tmpPst);
    if(result < 0)
    {
        MAPPING_ERR("[MAPPING_ERR] Switch page mapping table failed when create new log block! Err:0x%x\n", result);
        return -1;
    }

    //merge the log block with normal type, to make an empty item
    result = LML_MergeLogBlk_Ext(NORMAL_MERGE_MODE, LOG_BLK_TBL[tmpPst].LogicBlkNum, start_page, merge_page_cnt);
    if(result < 0)
    {
        //merge log block failed, report error
        MAPPING_ERR("[MAPPING_ERR] Merge log block failed when create new log block! Err:0x%x\n", result);
        return -1;
    }

	return (result);

}

static __s32 _write_dirty_flag(__u8 nZone)
{
    __s32 TablePage;
    __u32 TableBlk;
    struct  __PhysicOpPara_t  param;
    struct  __NandUserData_t  UserData[2];

    /*set table block number and table page number*/
    TableBlk = NandDriverInfo.ZoneTblPstInfo[nZone].PhyBlkNum;
    TablePage = NandDriverInfo.ZoneTblPstInfo[nZone].TablePst;

    TablePage += 3;
    MEMSET((void *)&UserData,0xff,sizeof(struct __NandUserData_t) * 2);
    UserData[0].PageStatus = 0x55;
    MEMSET(LML_PROCESS_TBL_BUF,0x55,512);
    param.MDataPtr = LML_PROCESS_TBL_BUF;
    param.SDataPtr = (void *)&UserData;

    LML_CalculatePhyOpPar(&param, nZone, TableBlk, TablePage);
    LML_VirtualPageWrite(&param);
    PHY_SynchBank(param.BankNum, SYNC_CHIP_MODE);

    return NAND_OP_TRUE;

}


/*
************************************************************************************************************************
*                       SET DIRTY FLAG FOR BLOCK MAPPING TABLE
*
*Description: Set dirty flag for block mapping table.
*
*Arguments  : none.
*
*Return     : set dirty flag result;
*               = 0     set dirty flag successful;
*               = -1    set dirty flag failed.
************************************************************************************************************************
*/
__s32 BMM_SetDirtyFlag(void)
{
    if (0 == BLK_MAP_CACHE->DirtyFlag){
       _write_dirty_flag(CUR_MAP_ZONE);
       BLK_MAP_CACHE->DirtyFlag = 1;
    }

    return NAND_OP_TRUE;
}

void BMM_DumpMap(void)
{
	__u32 i;

	for(i=0; i<BLOCK_MAP_TBL_CACHE_CNT; i++)
	{
		PRINT("\n");
		PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		PRINT("data block table:\n");
		dump(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].DataBlkTbl, 4096, 2, 16);
		PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		PRINT("\n");

		PRINT("\n");
		PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		PRINT("log block table:\n");
		dump(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].LogBlkTbl, 1024, 2, 16);
		PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		PRINT("\n");

		PRINT("\n");
		PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		PRINT("free block table:\n");
		dump(BLK_MAP_CACHE_POOL->BlkMapTblCachePool[i].FreeBlkTbl, 1024, 2, 16);
		PRINT("++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
		PRINT("\n");

	}
}




