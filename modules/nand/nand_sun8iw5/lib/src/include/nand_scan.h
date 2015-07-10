/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __NAND_SCAN_H__
#define __NAND_SCAN_H__

#include "nand_type.h"
#include "nand_physic.h"

//==============================================================================
//  define nand flash manufacture ID number
//==============================================================================

#define TOSHIBA_NAND            0x98                //Toshiba nand flash manufacture number
#define SAMSUNG_NAND            0xec                //Samsunt nand flash manufacture number
#define HYNIX_NAND              0xad                //Hynix nand flash manufacture number
#define MICRON_NAND             0x2c                //Micron nand flash manufacture number
#define ST_NAND                 0x20                //ST nand flash manufacture number
#define INTEL_NAND              0x89                //Intel nand flash manufacture number
#define SPANSION_NAND           0x01                //spansion nand flash manufacture number
#define POWER_NAND              0x92                //power nand flash manufacture number
#define SANDISK                 0x45                //sandisk nand flash manufacture number


//==============================================================================
//  define the function __s32erface for nand storage scan module
//==============================================================================

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
__s32  SCN_AnalyzeNandSystem(void);

__u32 NAND_GetValidBlkRatio(void);
__s32 NAND_SetValidBlkRatio(__u32 ValidBlkRatio);
__u32 NAND_GetFrequencePar(void);
__s32 NAND_SetFrequencePar(__u32 FrequencePar);
__u32 NAND_GetNandVersion(void);
__s32 NAND_GetParam(boot_nand_para_t * nand_param);

#endif  //ifndef __NAND_SCAN_H__
