/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include "../include/nand_drv_cfg.h"
#include "../include/nand_scan.h"
#include "../include/nfc.h"
#include "../include/nfc_reg.h"
#include "../include/nand_simple.h"

//#include "nand_physic_interface.h"


extern struct __NandStorageInfo_t  NandStorageInfo;

extern struct __NandPhyInfoPar_t SamsungNandTbl;
extern struct __NandPhyInfoPar_t HynixNandTbl;
extern struct __NandPhyInfoPar_t ToshibaNandTbl;
extern struct __NandPhyInfoPar_t MicronNandTbl;
extern struct __NandPhyInfoPar_t IntelNandTbl;
extern struct __NandPhyInfoPar_t StNandTbl;
extern struct __NandPhyInfoPar_t DefaultNandTbl;
extern struct __NandPhyInfoPar_t SpansionNandTbl;
extern struct __NandPhyInfoPar_t PowerNandTbl;
extern struct __NandPhyInfoPar_t SandiskNandTbl;
extern __u32 NandIndex;
//extern __u32 MaxBlkEraseTimes;
extern __u32 Retry_value_ok_flag;

__u32 MaxBlkEraseTimes;

__u32 NandIDNumber = 0xffffff;
__u32 NandSupportTwoPlaneOp = 0;
__u32 CurrentDriverTwoPlaneOPCfg = 0;


__u32 NAND_GetPageSize(void)
{
	return (NandStorageInfo.SectorCntPerPage *512);
}

__u32 NAND_GetLogicPageSize(void)
{
	return (SECTOR_CNT_OF_SUPER_PAGE*512);
}

__u32 NAND_GetPageCntPerBlk(void)
{
	return NandStorageInfo.PageCntPerPhyBlk;
}

__u32 NAND_GetBlkCntPerChip(void)
{
	return NandStorageInfo.BlkCntPerDie * NandStorageInfo.DieCntPerChip;
}

__u32 NAND_GetChipCnt(void)
{
	return NandStorageInfo.ChipCnt;
}

__u32 NAND_GetChipConnect(void)
{
	return NandStorageInfo.ChipConnectInfo;
}

__u32 NAND_GetBadBlockFlagPos(void)
{
	return 2;
}

__u32 NAND_GetReadRetryType(void)
{
	return NandStorageInfo.ReadRetryType;
}

__u32 NAND_GetValidBlkRatio(void)
{
    return NandStorageInfo.ValidBlkRatio;
}

__s32 NAND_SetValidBlkRatio(__u32 ValidBlkRatio)
{
    NandStorageInfo.ValidBlkRatio = (__u16)ValidBlkRatio;
    return 0;

}

__u32 NAND_GetFrequencePar(void)
{
    return NandStorageInfo.FrequencePar;
}

__s32 NAND_SetFrequencePar(__u32 FrequencePar)
{
    NandStorageInfo.FrequencePar = (__u8)FrequencePar;
    return 0;

}

__s32 NAND_GetBlkCntOfDie(void)
{
	return NandStorageInfo.BlkCntPerDie;
}

__s32 NAND_GetDieSkipFlag(void)
{
	return (NandStorageInfo.OperationOpt&(0x1<<11));
}

__u32 NAND_GetChannelCnt(void)
{
	return NandStorageInfo.ChannelCnt;
}

__u32 NAND_GetOperationOpt(void)
{
	return NandStorageInfo.OperationOpt;
}

__u32 NAND_GetCurrentCH(void)
{
	return NandIndex;
}

__u32 NAND_SetCurrentCH(__u32 nand_index)
{
	NandIndex = nand_index;
	return NandIndex;
}


__u32 NAND_GetNandVersion(void)
{
    __u32 nand_version;

	nand_version = 0;
	nand_version |= 0xff;
	nand_version |= 0x00<<8;
	nand_version |= NAND_VERSION_0<<16;
	nand_version |= NAND_VERSION_1<<24;

	return nand_version;
}

__u32 NAND_GetVersion(__u8* nand_version)
{
	__u32 ret;

	ret = NAND_GetNandVersion();
	*(__u32 *)nand_version = ret;

	return ret;
}

__u32 NAND_GetNandVersionDate(void)
{
	return 0x20121209;
}


__s32 NAND_GetParam(boot_nand_para_t * nand_param)
{
	__u32 i;

	nand_param->ChannelCnt         =   NandStorageInfo.ChannelCnt        ;
	nand_param->ChipCnt            =   NandStorageInfo.ChipCnt           ;
	nand_param->ChipConnectInfo    =   NandStorageInfo.ChipConnectInfo   ;
	nand_param->RbCnt              =   NandStorageInfo.RbCnt             ;
	nand_param->RbConnectInfo      =   NandStorageInfo.RbConnectInfo     ;
	nand_param->RbConnectMode      =   NandStorageInfo.RbConnectMode     ;
	nand_param->BankCntPerChip     =   NandStorageInfo.BankCntPerChip    ;
	nand_param->DieCntPerChip      =   NandStorageInfo.DieCntPerChip     ;
	nand_param->PlaneCntPerDie     =   NandStorageInfo.PlaneCntPerDie    ;
	nand_param->SectorCntPerPage   =   NandStorageInfo.SectorCntPerPage  ;
	nand_param->PageCntPerPhyBlk   =   NandStorageInfo.PageCntPerPhyBlk  ;
	nand_param->BlkCntPerDie       =   NandStorageInfo.BlkCntPerDie      ;
	nand_param->OperationOpt       =   NandStorageInfo.OperationOpt      ;
	nand_param->FrequencePar       =   NandStorageInfo.FrequencePar      ;
	nand_param->EccMode            =   NandStorageInfo.EccMode           ;
	nand_param->ValidBlkRatio      =   NandStorageInfo.ValidBlkRatio     ;
	nand_param->good_block_ratio   =   NandStorageInfo.ValidBlkRatio     ;
	nand_param->ReadRetryType      =   NandStorageInfo.ReadRetryType     ;
	nand_param->DDRType            =   NandStorageInfo.DDRType           ;

	PHY_ERR("DDRType: %d\n", nand_param->DDRType);
	if ((nand_param->DDRType == 0x2) || (nand_param->DDRType == 0x12)) {
		PHY_ERR("set ddrtype 0, freq 30Mhz\n");
		nand_param->DDRType = 0;
		nand_param->FrequencePar = 30;
	} else if (nand_param->DDRType == 0x13) {
		PHY_ERR("set ddrtype 3, freq 30Mhz\n");
		nand_param->DDRType = 3;
		nand_param->FrequencePar = 30;
	}

	for(i =0; i<8; i++)
	    nand_param->NandChipId[i]   =   NandStorageInfo.NandChipId[i];



	return 0;
}

__s32 NAND_GetFlashInfo(boot_flash_info_t *param)
{
	param->chip_cnt	 		= NandStorageInfo.ChipCnt;
	param->blk_cnt_per_chip = NandStorageInfo.BlkCntPerDie * NandStorageInfo.DieCntPerChip;
	param->blocksize 		= SECTOR_CNT_OF_SINGLE_PAGE * PAGE_CNT_OF_PHY_BLK;
	param->pagesize  		= SECTOR_CNT_OF_SINGLE_PAGE;
	param->pagewithbadflag  = NandStorageInfo.OptPhyOpPar.BadBlockFlagPosition;

	return 0;
}

/*
	0: no physic arch info;
	1: ok;
	-1: fail;
*/
__s32 _GetOldPhysicArch(void *phy_arch, __u32 *good_blk_no)
{
	__s32 ret, ret2 = 0;
	__u32 b, chip = 0;
	__u32 start_blk=12, blk_cnt=100;
	__u8 oob[128];
	struct __NandStorageInfo_t *parch;
	struct boot_physical_param nand_op;

	//parch = (struct __NandStorageInfo_t *)PHY_TMP_PAGE_CACHE;
	parch = (struct __NandStorageInfo_t *)MALLOC(64*1024);
	if(!parch)
	{
		PRINT("%s,malloc fail\n",__func__);
		ret2 = -1;
		goto EXIT;
	}

	for (b=start_blk; b<start_blk+blk_cnt; b++)
	{
		nand_op.chip = chip;
		nand_op.block = b;
		nand_op.page = 0;
		nand_op.mainbuf = (void *)parch;
		nand_op.oobbuf = oob;

		ret = PHY_SimpleRead(&nand_op); //PHY_SimpleRead_CurCH(&nand_op);
		PHY_DBG("_GetOldPhysicArch: ch: %d, chip %d, block %d, page 0, "
				"oob: 0x%x, 0x%x, 0x%x, 0x%x\n",NandIndex,nand_op.chip, nand_op.block,
				oob[0], oob[1], oob[2], oob[3]);
		if (ret>=0)
		{
			if (oob[0]==0x00)
			{
				if ((oob[1]==0x50) && (oob[2]==0x48) && (oob[3]==0x59) && (oob[4]==0x41) && (oob[5]==0x52) && (oob[6]==0x43) && (oob[7]==0x48))
				{
					//*((struct __NandStorageInfo_t *)phy_arch) = *parch;
					if((parch->PlaneCntPerDie !=1) && (parch->PlaneCntPerDie !=2))
					{
						PHY_DBG("_GetOldPhysicArch: get old physic arch ok,but para error: 0x%x 0x%x!\n", parch->OperationOpt, parch->PlaneCntPerDie);
					}
					else
					{
						MEMCPY(phy_arch, parch, sizeof(struct __NandStorageInfo_t));
						PHY_DBG("_GetOldPhysicArch: get old physic arch ok, 0x%x 0x%x!\n", parch->OperationOpt, parch->PlaneCntPerDie);
						ret2 = 1;
						break;
					}
				}
				else
				{
					PHY_DBG("_GetOldPhysicArch: mark bad block!\n");
				}
			}
			else if(oob[0]==0xff)
			{
				PHY_DBG("_GetOldPhysicArch: find a good block, but no physic arch info.\n");
				ret2 = 2;//blank page
				break;
			}
			else
			{
				PHY_DBG("_GetOldPhysicArch: unkonwn1!\n");
			}
		}
		else
		{
			if (oob[0]==0xff)
			{
				PHY_DBG("_GetOldPhysicArch: blank block!\n");
				ret2 = 2;
				break;
			}
			else if (oob[0]==0)
			{
				PHY_DBG("_GetOldPhysicArch: bad block!\n");
			}
			else
			{
				PHY_DBG("_GetOldPhysicArch: unkonwn2!\n");
			}
		}
	}

	if (b == (start_blk+blk_cnt)) {
		ret2 = -1;
		*good_blk_no = 0;
	} else
		*good_blk_no = b;

EXIT:
    FREE(parch, 64*1024);

	return ret2;
}

__s32 _SetNewPhysicArch(void *phy_arch)
{
	__s32 ret;
	__u32 i, b, p, chip = 0;
	__u32 start_blk=12, blk_cnt=100;
	__u8 oob[128];
	__u32 good_blk_no;
	struct __NandStorageInfo_t *parch;
	struct __NandStorageInfo_t arch_tmp = {0};
	struct boot_physical_param nand_op;

    //parch = (struct __NandStorageInfo_t *)PHY_TMP_PAGE_CACHE;
	parch = (struct __NandStorageInfo_t *)MALLOC(64*1024);

	/* in order to get good block, get old physic arch info */
	ret = _GetOldPhysicArch(&arch_tmp, &good_blk_no);
	if (ret == -1) {
		/* can not find good block */
		PHY_ERR("_SetNewPhysicArch: can not find good block: 12~112\n");
		FREE(parch, 64*1024);
		return ret;
	}

	PHY_DBG("_SetNewPhysicArch: write physic arch to blk %d...\n", good_blk_no);

	for (b=good_blk_no; b<start_blk+blk_cnt; b++)
	{
		nand_op.chip = chip;
		nand_op.block = b;
		nand_op.page = 0;
		nand_op.mainbuf = (void *)parch; //PHY_TMP_PAGE_CACHE;
		nand_op.oobbuf = oob;

		ret = PHY_SimpleErase(&nand_op); //PHY_SimpleErase_CurCH(&nand_op);
		if (ret<0)
		{
			PHY_ERR("_SetNewPhysicArch: erase chip %d, block %d error\n", nand_op.chip, nand_op.block);

			for (i=0; i<128; i++)
		    	oob[i] = 0x0;

			for (p=0; p<NandStorageInfo.PageCntPerPhyBlk; p++)
			{
				nand_op.page = p;
				ret = PHY_SimpleWrite(&nand_op); //PHY_SimpleWrite_CurCH(&nand_op);
				if (ret<0) {
					PHY_ERR("_SetNewPhysicArch: mark bad block, write chip %d, block %d, page %d error\n", nand_op.chip, nand_op.block, nand_op.page);
				}
			}
		}
		else
		{
			PHY_DBG("_SetNewPhysicArch: erase block %d ok.\n", b);

			for (i=0; i<128; i++)
		    	oob[i] = 0x88;
		    oob[0] = 0x00; //bad block flag
			oob[1] = 0x50; //80; //'P'
			oob[2] = 0x48; //72; //'H'
			oob[3] = 0x59; //89; //'Y'
			oob[4] = 0x41; //65; //'A'
			oob[5] = 0x52; //82; //'R'
			oob[6] = 0x43; //67; //'C'
			oob[7] = 0x48; //72; //'H'

			//MEMSET(parch, 0x0, 1024);
			MEMCPY(parch, phy_arch, sizeof(struct __NandStorageInfo_t));
			//*parch = *((struct __NandStorageInfo_t *)phy_arch);
			for (p=0; p<NandStorageInfo.PageCntPerPhyBlk; p++)
			{
				nand_op.page = p;
				ret = PHY_SimpleWrite(&nand_op); //PHY_SimpleWrite_CurCH(&nand_op);
				if(ret<0)
				{
					PHY_ERR("_SetNewPhysicArch: write chip %d, block %d, page %d error\n", nand_op.chip, nand_op.block, nand_op.page);
					FREE(parch, 64*1024);
					return -1;
				}
			}
			break;
		}
	}

	PHY_DBG("_SetNewPhysicArch: ============\n");
    ret = _GetOldPhysicArch(&arch_tmp, &good_blk_no);
	if (ret == -1) {
		/* can not find good block */
		PHY_ERR("_SetNewPhysicArch: can not find good block: 12~112\n");
		FREE(parch, 64*1024);
		return ret;
	}

	FREE(parch, 64*1024);

	return 0;
}


__s32 _UpdateExtMultiPlanePara(void)
{
	__u32 id_number_ctl;
	__u32 script_twoplane, para;

	id_number_ctl = NAND_GetNandIDNumCtrl();
	if (0x0 != id_number_ctl)
	{
		if (id_number_ctl & (1U<<1))//bit 1, set twoplane para
		{
			para = NAND_GetNandExtPara(1);
			if(0xffffffff != para) //get script success
			{
				if(((para & 0xffffff) == NandIDNumber) || ((para & 0xffffff) == 0xeeeeee))
				{
					script_twoplane = (para >> 24) & 0xff;
					PHY_DBG("_UpdateExtMultiPlanePara: get twoplane para from script success: ", script_twoplane);
					if (script_twoplane == 1) {
						PHY_DBG("%d\n", script_twoplane);
						if (NandSupportTwoPlaneOp) {
							//PHY_DBG("NAND_UpdatePhyArch: current nand support two plane op!\n");
							NandStorageInfo.PlaneCntPerDie = 2;
							NandStorageInfo.OperationOpt |= NAND_MULTI_READ;
				       	 	NandStorageInfo.OperationOpt |= NAND_MULTI_PROGRAM;
						} else {
							PHY_DBG("_UpdateExtMultiPlanePara: current nand do not support two plane op, set to 0!\n");
							NandStorageInfo.PlaneCntPerDie = 1;
							NandStorageInfo.OperationOpt &= ~NAND_MULTI_READ;
				       	 	NandStorageInfo.OperationOpt &= ~NAND_MULTI_PROGRAM;
						}
					} else if (script_twoplane == 0){
						PHY_DBG("%d\n", script_twoplane);
						NandStorageInfo.PlaneCntPerDie = 1;
						NandStorageInfo.OperationOpt &= ~NAND_MULTI_READ;
			       	 	NandStorageInfo.OperationOpt &= ~NAND_MULTI_PROGRAM;
					} else {
						PHY_DBG("%d, wrong parameter(0,1)\n", script_twoplane);
						return -1;
					}
				}
				else
				{
					PHY_ERR("_UpdateExtMultiPlanePara: wrong id number, 0x%x/0x%x\n", (para & 0xffffff), NandIDNumber);
					return -1;
				}
			}
			else
			{
				PHY_ERR("_UpdateExtMultiPlanePara: wrong two plane para, 0x%x\n", para);
				return -1;
			}
		}
		else
		{
			PHY_ERR("_UpdateExtMultiPlanePara: wrong id ctrl number: %d/%d\n", id_number_ctl, (1U<<1));
			return -1;
		}
	}
	else
	{
		PHY_ERR("_UpdateExtMultiPlanePara: no para.\n");
		return -1;
	}

	return 0;
}

__s32 _UpdateExtAccessFreqPara(void)
{
	__u32 id_number_ctl;
	__u32 script_frequence, para;

	id_number_ctl = NAND_GetNandIDNumCtrl();
	if (0x0 != id_number_ctl) {
		if (id_number_ctl & (1U<<0)) {//bit 0, set freq para
			para = NAND_GetNandExtPara(0);
			if(0xffffffff != para) { //get script success
				if(((para & 0xffffff) == NandIDNumber) || ((para & 0xffffff) == 0xeeeeee)) {

					script_frequence = (para >> 24) & 0xff;
					if( (script_frequence > 10)&&(script_frequence < 100) ) {
						NandStorageInfo.FrequencePar = script_frequence;
						PHY_ERR("_UpdateExtAccessFreqPara: update freq from script, %d\n", script_frequence);
					} else {
						PHY_ERR("_UpdateExtAccessFreqPara: wrong freq, %d\n",script_frequence);
						return -1;
					}

				} else {
					PHY_ERR("_UpdateExtAccessFreqPara: wrong id number, 0x%x/0x%x\n", (para & 0xffffff), NandIDNumber);
					return -1;
				}
			} else {
				PHY_ERR("_UpdateExtAccessFreqPara: wrong freq para, 0x%x\n", para);
				return -1;
			}
		} else {
			PHY_ERR("_UpdateExtAccessFreqPara: wrong id ctrl number, %d/%d.\n", id_number_ctl, (1U<<0));
			return -1;
		}
	} else {
		PHY_DBG("_UpdateExtAccessFreqPara: no para.\n");
		return -1;
	}

	return 0;
}

__s32 NAND_UpdatePhyArch(void)
{
	__s32 ret = 0;

	/*
	 * when erase chip during update firmware, it means that we will ignore previous
	 * physical archtecture, erase all good blocks and write new data.
	 *
	 * we should write new physical architecture to block 12~12+100 for next update.
	 */
	ret = _UpdateExtMultiPlanePara();
	if (ret<0)
	{
		if (CurrentDriverTwoPlaneOPCfg) {
			NandStorageInfo.PlaneCntPerDie = 2;
			NandStorageInfo.OperationOpt |= NAND_MULTI_READ;
       	 	NandStorageInfo.OperationOpt |= NAND_MULTI_PROGRAM;
		} else {
			NandStorageInfo.PlaneCntPerDie = 1;
			NandStorageInfo.OperationOpt &= ~NAND_MULTI_READ;
	   	 	NandStorageInfo.OperationOpt &= ~NAND_MULTI_PROGRAM;
		}
		PHY_ERR("NAND_UpdatePhyArch: get script error, use current driver cfg!\n");
	}
	PHY_ERR("NAND_UpdatePhyArch: before set new arch: 0x%x 0x%x.\n", NandStorageInfo.OperationOpt, NandStorageInfo.PlaneCntPerDie);
	ret = _SetNewPhysicArch(&NandStorageInfo);
	if (ret<0) {
		PHY_ERR("NAND_UpdatePhyArch: write physic arch to nand failed!\n");
	}

	return ret;
}

__s32 NAND_ReadPhyArch(void)
{
	__s32 ret = 0;
    struct __NandStorageInfo_t old_storage_info = {0};
    __u32 good_blk_no;

	ret = _GetOldPhysicArch(&old_storage_info, &good_blk_no);
	if ( ret == 1 ) {
		PHY_ERR("NAND_ReadPhyArch: get old physic arch ok, use old cfg, now:0x%x 0x%x - old:0x%x 0x%x!\n",
			NandStorageInfo.PlaneCntPerDie, NandStorageInfo.OperationOpt,
			old_storage_info.PlaneCntPerDie, old_storage_info.OperationOpt);
		NandStorageInfo.PlaneCntPerDie = old_storage_info.PlaneCntPerDie;
		if (NandStorageInfo.PlaneCntPerDie == 1) {
		    NandStorageInfo.OperationOpt &= ~NAND_MULTI_READ;
   	 	    NandStorageInfo.OperationOpt &= ~NAND_MULTI_PROGRAM;
		}
		if (NandStorageInfo.PlaneCntPerDie == 2) {
		    NandStorageInfo.OperationOpt |= NAND_MULTI_READ;
   	 	    NandStorageInfo.OperationOpt |= NAND_MULTI_PROGRAM;
		}
	} else if(ret == 2) {
		PHY_ERR("NAND_ReadPhyArch: blank page!\n");
	} else{
		PHY_ERR("NAND_ReadPhyArch: get para error!\n");
	}
	return ret;
}

/*
************************************************************************************************************************
*                           SEARCH NAND PHYSICAL ARCHITECTURE PARAMETER
*
*Description: Search the nand flash physical architecture parameter from the parameter table
*             by nand chip ID.
*
*Arguments  : pNandID           the pointer to nand flash chip ID;
*             pNandArchiInfo    the pointer to nand flash physical architecture parameter.
*
*Return     : search result;
*               = 0     search successful, find the parameter in the table;
*               < 0     search failed, can't find the parameter in the table.
************************************************************************************************************************
*/
__s32 _SearchNandArchi(__u8 *pNandID, struct __NandPhyInfoPar_t *pNandArchInfo)
{
    __s32 i=0, j=0, k=0;
    __u32 id_match_tbl[3]={0xffff, 0xffff, 0xffff};
    __u32 id_bcnt;
    struct __NandPhyInfoPar_t *tmpNandManu;

    //analyze the manufacture of the nand flash
    switch(pNandID[0])
    {
        //manufacture is Samsung, search parameter from Samsung nand table
        case SAMSUNG_NAND:
            tmpNandManu = &SamsungNandTbl;
            break;

        //manufacture is Hynix, search parameter from Hynix nand table
        case HYNIX_NAND:
            tmpNandManu = &HynixNandTbl;
            break;

        //manufacture is Micron, search parameter from Micron nand table
        case MICRON_NAND:
            tmpNandManu = &MicronNandTbl;
            break;

        //manufacture is Intel, search parameter from Intel nand table
        case INTEL_NAND:
            tmpNandManu = &IntelNandTbl;
            break;

        //manufacture is Toshiba, search parameter from Toshiba nand table
        case TOSHIBA_NAND:
            tmpNandManu = &ToshibaNandTbl;
            break;

        //manufacture is St, search parameter from St nand table
        case ST_NAND:
            tmpNandManu = &StNandTbl;
            break;

		//manufacture is Spansion, search parameter from Spansion nand table
        case SPANSION_NAND:
            tmpNandManu = &SpansionNandTbl;
            break;

       //manufacture is power, search parameter from Spansion nand table
        case POWER_NAND:
            tmpNandManu = &PowerNandTbl;
            break;

	   //manufacture is sandisk, search parameter from sandisk nand table
        case SANDISK:
            tmpNandManu = &SandiskNandTbl;
            break;

        //manufacture is unknown, search parameter from default nand table
        default:
            tmpNandManu = &DefaultNandTbl;
            break;
    }

    //search the nand architecture parameter from the given manufacture nand table by nand ID
    while(tmpNandManu[i].NandID[0] != 0xff)
    {
        //compare 6 byte id
        id_bcnt = 1;
        for(j=1; j<6; j++)
        {
            //0xff is matching all ID value
            if((pNandID[j] != tmpNandManu[i].NandID[j]) && (tmpNandManu[i].NandID[j] != 0xff))
            break;

            if(tmpNandManu[i].NandID[j] != 0xff)
                id_bcnt++;
        }

        if(j == 6)
        {
             /*4 bytes of the nand chip ID are all matching, search parameter successful*/
            if(id_bcnt == 4)
                id_match_tbl[0] = i;
            else if(id_bcnt == 5)
                id_match_tbl[1] = i;
            else if(id_bcnt == 6)
                id_match_tbl[2] = i;
        }

        //prepare to search the next table item
        i++;
    }

    for(k=2; k>=0;k--)
    {

        if(id_match_tbl[k]!=0xffff)
        {
            i= id_match_tbl[k];
            MEMCPY(pNandArchInfo,tmpNandManu+i,sizeof(struct __NandPhyInfoPar_t));
            return 0;
        }
    }

    //search nand architecture parameter failed
    return -1;
}


/*
************************************************************************************************************************
*                           ANALYZE NAND FLASH STORAGE SYSTEM
*
*Description: Analyze nand flash storage system, generate the nand flash physical
*             architecture parameter and connect information.
*
*Arguments  : none
*
*Return     : analyze result;
*               = 0     analyze successful;
*               < 0     analyze failed, can't recognize or some other error.
************************************************************************************************************************
*/
__s32  SCN_AnalyzeNandSystem(void)
{
    __s32 i, result, ret;
    __u8  tmpChipID[8];
	__u8  uniqueID[32];
    struct __NandPhyInfoPar_t tmpNandPhyInfo;
    __u32 val[8];

    //init nand flash storage information to default value
    NandStorageInfo.ChannelCnt = MAX_NFC_CH;
    NandStorageInfo.ChipCnt = 1;
    NandStorageInfo.ChipConnectInfo = 1;
    NandStorageInfo.RbConnectMode= 1;
    NandStorageInfo.RbCnt= 1;
    NandStorageInfo.RbConnectInfo= 1;
    NandStorageInfo.BankCntPerChip = 1;
    NandStorageInfo.DieCntPerChip = 1;
    NandStorageInfo.PlaneCntPerDie = 1;
    NandStorageInfo.SectorCntPerPage = 4;
    NandStorageInfo.PageCntPerPhyBlk = 64;
    NandStorageInfo.BlkCntPerDie = 1024;
    NandStorageInfo.OperationOpt = 0;
    NandStorageInfo.FrequencePar = 10;
    NandStorageInfo.EccMode = 0;
	NandStorageInfo.ReadRetryType= 0;

    //reset the nand flash chip on boot chip select
    result = PHY_ResetChip(BOOT_CHIP_SELECT_NUM);
    result |= PHY_SynchBank(BOOT_CHIP_SELECT_NUM, SYNC_CHIP_MODE);
    if(result)
    {
        SCAN_ERR("[SCAN_ERR] Reset boot nand flash chip failed!\n");
        return -1;
    }

    //read nand flash chip ID from boot chip
    result = PHY_ReadNandId(BOOT_CHIP_SELECT_NUM, tmpChipID);
    if(result)
    {
        SCAN_ERR("[SCAN_ERR] Read chip ID from boot chip failed!\n");
        return -1;
    }
    SCAN_DBG("[SCAN_DBG] Nand flash chip id is:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
            tmpChipID[0],tmpChipID[1],tmpChipID[2],tmpChipID[3], tmpChipID[4],tmpChipID[5]);

    //search the nand flash physical architecture parameter by nand ID
    result = _SearchNandArchi(tmpChipID, &tmpNandPhyInfo);
    if(result)
    {
        SCAN_ERR("[SCAN_ERR] search nand physical architecture parameter failed!\n");

		return -1;
    }

    //set the nand flash physical architecture parameter
    NandStorageInfo.BankCntPerChip = tmpNandPhyInfo.DieCntPerChip;
    NandStorageInfo.DieCntPerChip = tmpNandPhyInfo.DieCntPerChip;
    NandStorageInfo.PlaneCntPerDie = 2;
    NandStorageInfo.SectorCntPerPage = tmpNandPhyInfo.SectCntPerPage;
    NandStorageInfo.PageCntPerPhyBlk = tmpNandPhyInfo.PageCntPerBlk;
    NandStorageInfo.BlkCntPerDie = tmpNandPhyInfo.BlkCntPerDie;
    NandStorageInfo.OperationOpt = tmpNandPhyInfo.OperationOpt;
    NandStorageInfo.FrequencePar = tmpNandPhyInfo.AccessFreq;
    NandStorageInfo.EccMode = tmpNandPhyInfo.EccMode;
    NandStorageInfo.NandChipId[0] = tmpNandPhyInfo.NandID[0];
    NandStorageInfo.NandChipId[1] = tmpNandPhyInfo.NandID[1];
    NandStorageInfo.NandChipId[2] = tmpNandPhyInfo.NandID[2];
    NandStorageInfo.NandChipId[3] = tmpNandPhyInfo.NandID[3];
    NandStorageInfo.NandChipId[4] = tmpNandPhyInfo.NandID[4];
    NandStorageInfo.NandChipId[5] = tmpNandPhyInfo.NandID[5];
    NandStorageInfo.NandChipId[6] = tmpNandPhyInfo.NandID[6];
    NandStorageInfo.NandChipId[7] = tmpNandPhyInfo.NandID[7];
    NandStorageInfo.ValidBlkRatio = tmpNandPhyInfo.ValidBlkRatio;
	NandStorageInfo.ReadRetryType = tmpNandPhyInfo.ReadRetryType;
	NandStorageInfo.DDRType       = tmpNandPhyInfo.DDRType;
    //set the optional operation parameter
    NandStorageInfo.OptPhyOpPar.MultiPlaneReadCmd[0] = tmpNandPhyInfo.OptionOp->MultiPlaneReadCmd[0];
    NandStorageInfo.OptPhyOpPar.MultiPlaneReadCmd[1] = tmpNandPhyInfo.OptionOp->MultiPlaneReadCmd[1];
    NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[0] = tmpNandPhyInfo.OptionOp->MultiPlaneWriteCmd[0];
    NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[1] = tmpNandPhyInfo.OptionOp->MultiPlaneWriteCmd[1];
    NandStorageInfo.OptPhyOpPar.MultiPlaneCopyReadCmd[0] = tmpNandPhyInfo.OptionOp->MultiPlaneCopyReadCmd[0];
    NandStorageInfo.OptPhyOpPar.MultiPlaneCopyReadCmd[1] = tmpNandPhyInfo.OptionOp->MultiPlaneCopyReadCmd[1];
    NandStorageInfo.OptPhyOpPar.MultiPlaneCopyReadCmd[2] = tmpNandPhyInfo.OptionOp->MultiPlaneCopyReadCmd[2];
    NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[0] = tmpNandPhyInfo.OptionOp->MultiPlaneCopyWriteCmd[0];
    NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[1] = tmpNandPhyInfo.OptionOp->MultiPlaneCopyWriteCmd[1];
    NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[2] = tmpNandPhyInfo.OptionOp->MultiPlaneCopyWriteCmd[2];
    NandStorageInfo.OptPhyOpPar.MultiPlaneStatusCmd = tmpNandPhyInfo.OptionOp->MultiPlaneStatusCmd;
    NandStorageInfo.OptPhyOpPar.InterBnk0StatusCmd = tmpNandPhyInfo.OptionOp->InterBnk0StatusCmd;
    NandStorageInfo.OptPhyOpPar.InterBnk1StatusCmd = tmpNandPhyInfo.OptionOp->InterBnk1StatusCmd;
    NandStorageInfo.OptPhyOpPar.BadBlockFlagPosition = tmpNandPhyInfo.OptionOp->BadBlockFlagPosition;
    NandStorageInfo.OptPhyOpPar.MultiPlaneBlockOffset = tmpNandPhyInfo.OptionOp->MultiPlaneBlockOffset;

    /* set max block erase cnt and enable read reclaim flag */
    MaxBlkEraseTimes = tmpNandPhyInfo.MaxBlkEraseTimes;
	/* in order to support to parse external script, record id ctl number */
    NandIDNumber = tmpNandPhyInfo.Idnumber;

    /* record current nand flash whether support two plane program */
    if ( NandStorageInfo.OperationOpt & NAND_MULTI_PROGRAM)
    	NandSupportTwoPlaneOp = 1;
    else
    	NandSupportTwoPlaneOp = 0;

	/* record current driver cfg for two plane operation */
	if (CFG_SUPPORT_MULTI_PLANE_PROGRAM == 0)
		CurrentDriverTwoPlaneOPCfg = 0;
	else {
		if (NandSupportTwoPlaneOp)
			CurrentDriverTwoPlaneOPCfg = 1;
		else
			CurrentDriverTwoPlaneOPCfg = 0;
	}
    PHY_DBG("[SCAN_DBG] NandTwoPlaneOp: %d, DriverTwoPlaneOPCfg: %d, 0x%x \n",
    	NandSupportTwoPlaneOp, CurrentDriverTwoPlaneOPCfg, ((NandIDNumber<<4)^0xffffffff));

    /* update access frequency from script */
    if (SUPPORT_UPDATE_EXTERNAL_ACCESS_FREQ) {
	    _UpdateExtAccessFreqPara();
	}

    //ddr info
    #if 0
    NandDdrInfo.if_type = tmpNandPhyInfo.DDRType;
    NandDdrInfo.timing_mode = 0;
    NandDdrInfo.en_dqs_c = tmpNandPhyInfo.InitDdrInfo->en_dqs_c;
    NandDdrInfo.en_re_c = tmpNandPhyInfo.InitDdrInfo->en_re_c;
    NandDdrInfo.odt = tmpNandPhyInfo.InitDdrInfo->odt;
    NandDdrInfo.en_ext_verf = tmpNandPhyInfo.InitDdrInfo->en_ext_verf;
    NandDdrInfo.dout_re_warmup_cycle = tmpNandPhyInfo.InitDdrInfo->dout_re_warmup_cycle;
    NandDdrInfo.din_dqs_warmup_cycle = tmpNandPhyInfo.InitDdrInfo->din_dqs_warmup_cycle;
    NandDdrInfo.output_driver_strength = tmpNandPhyInfo.InitDdrInfo->output_driver_strength;
    NandDdrInfo.rb_pull_down_strength = tmpNandPhyInfo.InitDdrInfo->rb_pull_down_strength;
    #else
    NandDdrInfo.if_type = tmpNandPhyInfo.DDRType;
    NandDdrInfo.timing_mode = 0;
    NandDdrInfo.en_dqs_c = 0; //tmpNandPhyInfo.InitDdrInfo->en_dqs_c;
    NandDdrInfo.en_re_c = 0; //tmpNandPhyInfo.InitDdrInfo->en_re_c;
    NandDdrInfo.odt = tmpNandPhyInfo.InitDdrInfo->odt;
    NandDdrInfo.en_ext_verf = 0; //tmpNandPhyInfo.InitDdrInfo->en_ext_verf;
    NandDdrInfo.dout_re_warmup_cycle = tmpNandPhyInfo.InitDdrInfo->dout_re_warmup_cycle;
    NandDdrInfo.din_dqs_warmup_cycle = tmpNandPhyInfo.InitDdrInfo->din_dqs_warmup_cycle;
    NandDdrInfo.output_driver_strength = tmpNandPhyInfo.InitDdrInfo->output_driver_strength;
    NandDdrInfo.rb_pull_down_strength = tmpNandPhyInfo.InitDdrInfo->rb_pull_down_strength;
    #endif


    if(!CFG_SUPPORT_RANDOM)
    {
        NandStorageInfo.OperationOpt &= ~NAND_RANDOM;
    }

    if(!CFG_SUPPORT_READ_RETRY)
    {
        NandStorageInfo.OperationOpt &= ~NAND_READ_RETRY;
    }

    if(!CFG_SUPPORT_ALIGN_NAND_BNK)
    {
        NandStorageInfo.OperationOpt |= NAND_PAGE_ADR_NO_SKIP;
    }

	if(!CFG_SUPPORT_CHECK_WRITE_SYNCH)
    {
        NandStorageInfo.OperationOpt &= ~NAND_FORCE_WRITE_SYNC;
    }

    if(!SUPPORT_INT_INTERLEAVE)
    {
        NandStorageInfo.BankCntPerChip = 1;
    }

	//process the rb connect infomation
    for(i=1; i<MAX_CHIP_SELECT_CNT; i++)
    {
        //reset current nand flash chip
        PHY_ResetChip_for_init((__u32)i);

        //read the nand chip ID from current nand flash chip
        PHY_ReadNandId((__u32)i, tmpChipID);
        //check if the nand flash id same as the boot chip
        if((tmpChipID[0] == NandStorageInfo.NandChipId[0]) && (tmpChipID[1] == NandStorageInfo.NandChipId[1])
            && (tmpChipID[2] == NandStorageInfo.NandChipId[2]) && (tmpChipID[3] == NandStorageInfo.NandChipId[3])
            && ((tmpChipID[4] == NandStorageInfo.NandChipId[4])||(NandStorageInfo.NandChipId[4]==0xff))
            && ((tmpChipID[5] == NandStorageInfo.NandChipId[5])||(NandStorageInfo.NandChipId[5]==0xff)))
        {
            NandStorageInfo.ChipCnt++;
            NandStorageInfo.ChipConnectInfo |= (1<<i);
        }
    }

    //process the rb connect infomation
    {
        NandStorageInfo.RbConnectMode = 0xff;

        if((NandStorageInfo.ChipCnt == 1) && (NandStorageInfo.ChipConnectInfo & (1<<0)))
        {
             NandStorageInfo.RbConnectMode =1;
        }
        else if(NandStorageInfo.ChipCnt == 2)
        {
    	      if((NandStorageInfo.ChipConnectInfo & (1<<0)) && (NandStorageInfo.ChipConnectInfo & (1<<1)))
		    NandStorageInfo.RbConnectMode =2;
	      else if((NandStorageInfo.ChipConnectInfo & (1<<0)) && (NandStorageInfo.ChipConnectInfo & (1<<2)))
		    NandStorageInfo.RbConnectMode =3;
		else if((NandStorageInfo.ChipConnectInfo & (1<<0)) && (NandStorageInfo.ChipConnectInfo & (1<<7)))
		    NandStorageInfo.RbConnectMode =0; 	//special use, only one rb

        }

        else if(NandStorageInfo.ChipCnt == 4)
        {
    	      if((NandStorageInfo.ChipConnectInfo & (1<<0)) && (NandStorageInfo.ChipConnectInfo & (1<<1))
			  	&&  (NandStorageInfo.ChipConnectInfo & (1<<2)) &&  (NandStorageInfo.ChipConnectInfo & (1<<3)) )
		    NandStorageInfo.RbConnectMode =4;
	      else if((NandStorageInfo.ChipConnectInfo & (1<<0)) && (NandStorageInfo.ChipConnectInfo & (1<<2))
			  	&&  (NandStorageInfo.ChipConnectInfo & (1<<4)) &&  (NandStorageInfo.ChipConnectInfo & (1<<6)) )
		    NandStorageInfo.RbConnectMode =5;
        }
        else if(NandStorageInfo.ChipCnt == 8)
        {
	      NandStorageInfo.RbConnectMode =8;
        }

		if( NandStorageInfo.RbConnectMode == 0xff)
		{
        	    SCAN_ERR("%s : check nand rb connect fail, ChipCnt =  %x, ChipConnectInfo = %x \n",__FUNCTION__, NandStorageInfo.ChipCnt, NandStorageInfo.ChipConnectInfo);
        	    return -1;
		}
    }


    {
		//for 2ch, secbitmap limit __u64
		if( (!CFG_SUPPORT_MULTI_PLANE_PROGRAM)
			|| ((SECTOR_CNT_OF_SINGLE_PAGE == 32)&&(NandStorageInfo.ChannelCnt == 2)) )
		{
			NandStorageInfo.OperationOpt &= ~NAND_MULTI_READ;
			NandStorageInfo.OperationOpt &= ~NAND_MULTI_PROGRAM;
		}
		//process the plane count of a die and the bank count of a chip
		if(!SUPPORT_MULTI_PROGRAM)
		{
			NandStorageInfo.PlaneCntPerDie = 1;
		}
    }

    //for 2ch, secbitmap limit __u64
	if( (!CFG_SUPPORT_INT_INTERLEAVE)
		||((SECTOR_CNT_OF_SINGLE_PAGE == 32)&&(NandStorageInfo.ChannelCnt == 2)) )
    {
        NandStorageInfo.OperationOpt &= ~NAND_INT_INTERLEAVE;
    }

    //process the external inter-leave operation
    if(CFG_SUPPORT_EXT_INTERLEAVE)
    {
        if(NandStorageInfo.ChipCnt > 1)
        {
            NandStorageInfo.OperationOpt |= NAND_EXT_INTERLEAVE;
        }
    }
    else
    {
        NandStorageInfo.OperationOpt &= ~NAND_EXT_INTERLEAVE;
    }

	if(SUPPORT_READ_UNIQUE_ID)
	{
		for(i=0; i<NandStorageInfo.ChipCnt; i++)
		{
			PHY_ReadNandUniqueId(i, uniqueID);
		}
	}

	if (NdfcVersion == NDFC_VERSION_V2) {
		/* the REc and CE2 are locate at same pin. if this pin has been used as a chip seclect
		function CE2,  forbid to switch this pin to function REc. */
		if (NandStorageInfo.ChipConnectInfo & (0x1U<<2)) {
			NandDdrInfo.en_re_c = 0;
		}
		/* the DQSc and CE3 are locate at same pin. if pin pin has been used as a chip seclect
		function CE3, 	disable to switch this pin to function DQSc. */
		if (NandStorageInfo.ChipConnectInfo & (0x1U<<3)) {
			NandDdrInfo.en_dqs_c = 0;
		}

		if (NandDdrInfo.en_re_c) {
			if (NAND_PIOFuncChange_REc(NandIndex, 1)) {
				SCAN_DBG("enable REc fail!\n");
				NandDdrInfo.en_re_c = 0;
			}
		}
		if (NandDdrInfo.en_dqs_c) {
			if (NAND_PIOFuncChange_DQSc(NandIndex, 1)) {
				SCAN_DBG("enable DQSc fail!\n");
				NandDdrInfo.en_dqs_c = 0;
			}
		}
	}


	/*configure page size, ecc mode...*/
	{
		NFC_INIT_INFO nand_info;
		nand_info.bus_width = 0x0;
		nand_info.ce_ctl = 0x0;
		nand_info.ce_ctl1 = 0x0;
		nand_info.debug = 0x0;
		nand_info.pagesize = SECTOR_CNT_OF_SINGLE_PAGE;
		nand_info.rb_sel = 1;
		nand_info.serial_access_mode = 1;
		nand_info.ddr_type = DDR_TYPE;
		if((__u32)CHANNEL_CNT>2)
			PHY_ERR("[PHY_GetDefaultParam]:Invalid nand CHANNEL_CNT :0x%x\n", CHANNEL_CNT);

		for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
		{
			NFC_ChangMode(&nand_info);
			NFC_SetEccMode(ECC_MODE);
			if(NandIndex == (CHANNEL_CNT-1))
				break;
		}
		NandIndex = 0;

		if (NDFC_VERSION_V1 == NdfcVersion) {
			/* ndfc version support nv-ddr2 and toggle ddr2 */
			if (DDR_TYPE == TOG_DDR2)
				DDR_TYPE = TOG_DDR;
			if (DDR_TYPE == ONFI_DDR2)
				DDR_TYPE = ONFI_DDR;
			if (NAND_ACCESS_FREQUENCE > 80)
				NAND_ACCESS_FREQUENCE = 80;
		}

		if ( SUPPORT_TOGGLE_ONLY ) {

			/* init toggle ddr interface with classic clk cfg(20MHz) */

			PHY_DBG("SCN_AnalyzeNandSystem: init toggle interface!\n");
			if ((DDR_TYPE == TOG_DDR) || (DDR_TYPE == TOG_DDR2))
			{
				for(NandIndex = 0; NandIndex<CHANNEL_CNT;NandIndex++)
				{
					ret = NAND_SetClk(NandIndex, 30, 30*2);
					if (ret) {
						PHY_ERR("SCN_AnalyzeNandSystem: init toggle interface with classic"
							"clk cfg(%d, %d) failed!\n", 30, 30*2);
						return -1;
					}

					nand_info.ddr_type = TOG_DDR;
					//nand_info.ddr_edo = 0x2;
					//nand_info.ddr_delay = 0x1f;

					nand_info.ddr_edo = 0x3;
					if(((*(volatile __u32 *)(0xf8001400+0x190))>>0x3)&0x1)
						nand_info.ddr_delay = 0x1f;
					else
						nand_info.ddr_delay = 0x39;

					NFC_InitDDRParam(0, (nand_info.ddr_edo<<8)|(nand_info.ddr_delay));
					NFC_ChangeInterfaceMode(&nand_info);

					if(NandIndex == (CHANNEL_CNT-1))
						break;
				}
				NandIndex = 0;
			}
		}
	}

	/* allocate some memory buffer and change nand flash interface */
	{
		PHY_ChangeMode(DDR_TYPE, (void *)&NandDdrInfo, NAND_ACCESS_FREQUENCE, NAND_ACCESS_FREQUENCE*2);
	}

	if (SUPPORT_READ_RETRY)
	{
	    PHY_DBG("NFC Read Retry Init. \n");
		Retry_value_ok_flag = 0;
		NFC_ReadRetryInit(READ_RETRY_TYPE);

		for(i=0; i<NandStorageInfo.ChipCnt;i++)
	    {
	        PHY_GetDefaultParam(i);
			Retry_value_ok_flag = 1;
	    }
	}

	if (SUPPORT_UPDATE_WITH_OLD_PHYSIC_ARCH)
	{
		NAND_ReadPhyArch();
	}

    if (NandStorageInfo.PlaneCntPerDie == 2)
	{
		//for 2ch, secbitmap limit __u64
		if ((SECTOR_CNT_OF_SINGLE_PAGE == 32)&&(NandStorageInfo.ChannelCnt == 2))
		{
			NandStorageInfo.PlaneCntPerDie = 1;
			NandStorageInfo.OperationOpt &= ~NAND_MULTI_READ;
			NandStorageInfo.OperationOpt &= ~NAND_MULTI_PROGRAM;
			PHY_ERR("nand scan: chan %d, sector of single page %d, force to single plane: %d 0x%x\n",
					NandStorageInfo.ChannelCnt, SECTOR_CNT_OF_SINGLE_PAGE,
					NandStorageInfo.PlaneCntPerDie, NandStorageInfo.OperationOpt);
		}
    }

    //print nand flash physical architecture parameter
    SCAN_DBG("\n\n");
    SCAN_DBG("[SCAN_DBG] ==============Nand Architecture Parameter==============\n");
    SCAN_DBG("[SCAN_DBG]    Nand Chip ID:         0x%x 0x%x\n",
        (NandStorageInfo.NandChipId[0] << 0) | (NandStorageInfo.NandChipId[1] << 8)
        | (NandStorageInfo.NandChipId[2] << 16) | (NandStorageInfo.NandChipId[3] << 24),
        (NandStorageInfo.NandChipId[4] << 0) | (NandStorageInfo.NandChipId[5] << 8)
        | (NandStorageInfo.NandChipId[6] << 16) | (NandStorageInfo.NandChipId[7] << 24));
    SCAN_DBG("[SCAN_DBG]    Nand Channel Count:   0x%x\n", NandStorageInfo.ChannelCnt);
    SCAN_DBG("[SCAN_DBG]    Nand Chip Count:      0x%x\n", NandStorageInfo.ChipCnt);
    SCAN_DBG("[SCAN_DBG]    Nand Chip Connect:    0x%x\n", NandStorageInfo.ChipConnectInfo);
	SCAN_DBG("[SCAN_DBG]    Nand Rb Connect Mode:      0x%x\n", NandStorageInfo.RbConnectMode);
    SCAN_DBG("[SCAN_DBG]    Sector Count Of Page: 0x%x\n", NandStorageInfo.SectorCntPerPage);
    SCAN_DBG("[SCAN_DBG]    Page Count Of Block:  0x%x\n", NandStorageInfo.PageCntPerPhyBlk);
    SCAN_DBG("[SCAN_DBG]    Block Count Of Die:   0x%x\n", NandStorageInfo.BlkCntPerDie);
    SCAN_DBG("[SCAN_DBG]    Plane Count Of Die:   0x%x\n", NandStorageInfo.PlaneCntPerDie);
    SCAN_DBG("[SCAN_DBG]    Die Count Of Chip:    0x%x\n", NandStorageInfo.DieCntPerChip);
    SCAN_DBG("[SCAN_DBG]    Bank Count Of Chip:   0x%x\n", NandStorageInfo.BankCntPerChip);
    SCAN_DBG("[SCAN_DBG]    Optional Operation:   0x%x\n", NandStorageInfo.OperationOpt);
    SCAN_DBG("[SCAN_DBG]    Access Frequency:     0x%x\n", NandStorageInfo.FrequencePar);
    SCAN_DBG("[SCAN_DBG]    ECC Mode:             0x%x\n", NandStorageInfo.EccMode);
	SCAN_DBG("[SCAN_DBG]    Read Retry Type:      0x%x\n", NandStorageInfo.ReadRetryType);
	SCAN_DBG("[SCAN_DBG]    DDR Type:             0x%x\n", NandStorageInfo.DDRType);
    SCAN_DBG("[SCAN_DBG] =======================================================\n\n");

    //print nand flash optional operation parameter
    SCAN_DBG("[SCAN_DBG] ==============Optional Operaion Parameter==============\n");
    SCAN_DBG("[SCAN_DBG]    MultiPlaneReadCmd:      0x%x, 0x%x\n",
        NandStorageInfo.OptPhyOpPar.MultiPlaneReadCmd[0],NandStorageInfo.OptPhyOpPar.MultiPlaneReadCmd[1]);
    SCAN_DBG("[SCAN_DBG]    MultiPlaneWriteCmd:     0x%x, 0x%x\n",
        NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[0],NandStorageInfo.OptPhyOpPar.MultiPlaneWriteCmd[1]);
    SCAN_DBG("[SCAN_DBG]    MultiPlaneCopyReadCmd:  0x%x, 0x%x, 0x%x\n",
        NandStorageInfo.OptPhyOpPar.MultiPlaneCopyReadCmd[0],NandStorageInfo.OptPhyOpPar.MultiPlaneCopyReadCmd[1],
        NandStorageInfo.OptPhyOpPar.MultiPlaneCopyReadCmd[2]);
    SCAN_DBG("[SCAN_DBG]    MultiPlaneCopyWriteCmd: 0x%x, 0x%x, 0x%x\n",
        NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[0], NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[1],
        NandStorageInfo.OptPhyOpPar.MultiPlaneCopyWriteCmd[2]);
    SCAN_DBG("[SCAN_DBG]    MultiPlaneStatusCmd:    0x%x\n", NandStorageInfo.OptPhyOpPar.MultiPlaneStatusCmd);
    SCAN_DBG("[SCAN_DBG]    InterBnk0StatusCmd:     0x%x\n", NandStorageInfo.OptPhyOpPar.InterBnk0StatusCmd);
    SCAN_DBG("[SCAN_DBG]    InterBnk1StatusCmd:     0x%x\n", NandStorageInfo.OptPhyOpPar.InterBnk1StatusCmd);
    SCAN_DBG("[SCAN_DBG]    BadBlockFlagPosition:   0x%x\n", NandStorageInfo.OptPhyOpPar.BadBlockFlagPosition);
    SCAN_DBG("[SCAN_DBG]    MultiPlaneBlockOffset:  0x%x\n", NandStorageInfo.OptPhyOpPar.MultiPlaneBlockOffset);
    SCAN_DBG("[SCAN_DBG] =======================================================\n");

	//NandHw_DbgInfo(0);

	val[0] = (NandStorageInfo.NandChipId[0] << 0) | (NandStorageInfo.NandChipId[1] << 8)
        	| (NandStorageInfo.NandChipId[2] << 16) | (NandStorageInfo.NandChipId[3] << 24);
	val[1] = (NandStorageInfo.NandChipId[4] << 0) | (NandStorageInfo.NandChipId[5] << 8)
        	| (NandStorageInfo.NandChipId[6] << 16) | (NandStorageInfo.NandChipId[7] << 24);
    val[2] = NandStorageInfo.OperationOpt;
    val[3] = NandStorageInfo.ReadRetryType;
    val[4] = (NandStorageInfo.EccMode & 0xffff) | ((NandStorageInfo.DDRType & 0xffff) << 16);
	NAND_ShowEnv(0, "nand info", 5, val);

    return 0;
}

