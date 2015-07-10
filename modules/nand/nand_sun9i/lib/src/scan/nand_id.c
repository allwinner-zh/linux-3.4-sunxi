/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include "../include/nand_scan.h"

//==============================================================================
// define the optional operation parameter for different kindes of nand flash
//==============================================================================

//the physical architecture parameter for Samsung 2K page SLC nand flash
static struct __OptionalPhyOpPar_t PhysicArchiPara0 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x81},           //multi-plane program command
    {0x00, 0x00, 0x35},     //multi-plane page copy-back read command
    {0x85, 0x11, 0x81},     //multi-plane page copy-back program command
    0x70,                   //multi-plane operation status read command
    0xf1,                   //inter-leave bank0 operation status read command
    0xf2,                   //inter-leave bank1 operation status read command
    0x01,                   //bad block flag position, in the fist 2 page
    1                       //multi-plane block address offset
};

//the physical architecture parameter for Samsung 4K page SLC nand flash
static struct __OptionalPhyOpPar_t PhysicArchiPara1 =
{
    {0x60, 0x30},           //multi-plane read command
    {0x11, 0x81},           //multi-plane program command
    {0x60, 0x60, 0x35},     //multi-plane page copy-back read command
    {0x85, 0x11, 0x81},     //multi-plane page copy-back program command
    0x70,                   //multi-plane operation status read command
    0xf1,                   //inter-leave bank0 operation status read command
    0xf2,                   //inter-leave bank1 operation status read command
    0x00,                   //bad block flag position, in the fist page
    1                       //multi-plane block address offset
};

//the physical architecture parameter for Samsung 2K page MLC nand flash
static struct __OptionalPhyOpPar_t PhysicArchiPara2 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x81},           //multi-plane program command
    {0x00, 0x00, 0x35},     //multi-plane page copy-back read command
    {0x85, 0x11, 0x81},     //multi-plane page copy-back program command
    0x70,                   //multi-plane operation status read command
    0xf1,                   //inter-leave bank0 operation status read command
    0xf2,                   //inter-leave bank1 operation status read command
    0x02,                   //bad block flag position, in the last page
    1                       //multi-plane block address offset
};

//the physical architecture parameter for Samsung 4K page MLC nand flash
static struct __OptionalPhyOpPar_t PhysicArchiPara3 =
{
    {0x60, 0x60},           //multi-plane read command
    {0x11, 0x81},           //multi-plane program command
    {0x60, 0x60, 0x35},     //multi-plane page copy-back read command
    {0x85, 0x11, 0x81},     //multi-plane page copy-back program command
    0x70,                   //multi-plane operation status read command
    0xf1,                   //inter-leave bank0 operation status read command
    0xf2,                   //inter-leave bank1 operation status read command
    0x02,                   //bad block flag position, in the last page
    1                       //multi-plane block address offset
};

//the physical architecture parameter for Micon nand flash
static struct __OptionalPhyOpPar_t PhysicArchiPara4 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x80},           //multi-plane program command
    {0x00, 0x00, 0x35},     //multi-plane page copy-back read command
    {0x85, 0x11, 0x80},     //multi-plane page copy-back program command
    0x70,                   //multi-plane operation status read command
    0x78,                   //inter-leave bank0 operation status read command
    0x78,                   //inter-leave bank1 operation status read command
    0x01,                   //bad block flag position, in the fist 2 page
    1                       //multi-plane block address offset
};

//the physical architecture parameter for Toshiba SLC nand flash
static struct __OptionalPhyOpPar_t PhysicArchiPara5 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x80},           //multi-plane program command
    {0x00, 0x00, 0x30},     //multi-plane page copy-back read command
    {0x8c, 0x11, 0x8c},     //multi-plane page copy-back program command
    0x71,                   //multi-plane operation status read command
    0x70,                   //inter-leave bank0 operation status read command
    0x70,                   //inter-leave bank1 operation status read command
    0x00,                   //bad block flag position, in the fist page
    0                       //multi-plane block address offset
};

//the physical architecture parameter for Toshiba MLC nand flash which multi-plane offset is 1024
static struct __OptionalPhyOpPar_t PhysicArchiPara6 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x80},           //multi-plane program command
    {0x00, 0x00, 0x30},     //multi-plane page copy-back read command
    {0x8c, 0x11, 0x8c},     //multi-plane page copy-back program command
    0x71,                   //multi-plane operation status read command
    0x70,                   //inter-leave bank0 operation status read command
    0x70,                   //inter-leave bank1 operation status read command
    0x00,                   //bad block flag position, in the fist page
    1024                    //multi-plane block address offset
};

//the physical architecture parameter for Toshiba MLC nand flash which multi-plane offset is 2048
static struct __OptionalPhyOpPar_t PhysicArchiPara7 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x80},           //multi-plane program command
    {0x00, 0x00, 0x30},     //multi-plane page copy-back read command
    {0x8c, 0x11, 0x8c},     //multi-plane page copy-back program command
    0x71,                   //multi-plane operation status read command
    0x70,                   //inter-leave bank0 operation status read command
    0x70,                   //inter-leave bank1 operation status read command
    0x00,                   //bad block flag position, in the fist page
    2048                    //multi-plane block address offset
};

static struct __OptionalPhyOpPar_t PhysicArchiPara8 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x80},           //multi-plane program command
    {0x00, 0x00, 0x30},     //multi-plane page copy-back read command
    {0x8c, 0x11, 0x8c},     //multi-plane page copy-back program command
    0x71,                   //multi-plane operation status read command
    0x70,                   //inter-leave bank0 operation status read command
    0x70,                   //inter-leave bank1 operation status read command
    0x02,                   //bad block flag position, in the last page
    1                       //multi-plane block address offset
};

static struct __OptionalPhyOpPar_t PhysicArchiPara9 =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x81},           //multi-plane program command
    {0x00, 0x00, 0x30},     //multi-plane page copy-back read command
    {0x8c, 0x11, 0x8c},     //multi-plane page copy-back program command
    0x71,                   //multi-plane operation status read command
    0x70,                   //inter-leave bank0 operation status read command
    0x70,                   //inter-leave bank1 operation status read command
    0x02,                   //bad block flag position, in the last page
    1                       //multi-plane block address offset
};

static struct __OptionalPhyOpPar_t DefualtPhysicArchiPara =
{
    {0x00, 0x30},           //multi-plane read command
    {0x11, 0x81},           //multi-plane program command
    {0x00, 0x00, 0x35},     //multi-plane page copy-back read command
    {0x85, 0x11, 0x81},     //multi-plane page copy-back program command
    0x70,                   //multi-plane operation status read command
    0xf1,                   //inter-leave bank0 operation status read command
    0xf2,                   //inter-leave bank1 operation status read command
    0x00,                   //bad block flag position, in the fist 2 page
    1                       //multi-plane block address offset
};

//==============================================================================
// define the ddr parameter for different kindes of nand flash
//==============================================================================
static struct __NfcInitDdrInfo DefDDRInfo = {
	0, //en_dqs_c;
	0, //en_re_c;
	0, //odt;
	0, //en_ext_verf;
	0, //dout_re_warmup_cycle;
	0, //din_dqs_warmup_cycle;
	0, //output_driver_strength;
	0, //rb_pull_down_strength;
} ;

static struct __NfcInitDdrInfo DDRInfo1 = {
	0, //en_dqs_c;
	0, //en_re_c;
	0, //odt;
	0, //en_ext_verf;
	0, //dout_re_warmup_cycle;
	0, //din_dqs_warmup_cycle;
	2, //output_driver_strength;
	2, //rb_pull_down_strength;
} ;

/*
	LSB Page Type: OpOpt[19:16]  (.OperationOpt)
	0x0 : hynix 26nm, 20nm; micron 256 page;
	0x1 : samsung & toshiba 128 page;
	0x2 : micron 20nm, 256 page; (20nm)
	0x3 : hynix 16nm, 128 page per block
	0x4 : hynix 16nm, 256 page per block
	0x5 : micrion l85a, 512 page per block(20nm)
	0x6 : micrion l95b, 512 page per block(16nm)
	0x6~0xF : Reserved
*/

//==============================================================================
// define the physical architecture parameter for all kinds of nand flash
//==============================================================================

//==============================================================================
//============================ SAMSUNG NAND FLASH ==============================
//==============================================================================
struct __NandPhyInfoPar_t SamsungNandTbl[] =
{
    //                NAND_CHIP_ID                     DieCnt SecCnt  PagCnt   BlkCnt      OpOpt    DatBlk  Freq   EccMode ReadRetry  DDRType    OperationPar       DDRInfo
    //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xec, 0xd5, 0x14, 0xb6, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     4096,   0x00000008,   896,    30,     0,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000011},   // K9GAG08
    { {0xec, 0xd7, 0x55, 0xb6, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     4096,   0x00000028,   896,    30,     0,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000012},   // K9LBG08
    { {0xec, 0xd7, 0xd5, 0x29, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     4096,   0x00000028,   896,    30,     0,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000013},   // K9LBG08
    { {0xec, 0xd7, 0x94, 0x72, 0xff, 0xff, 0xff, 0xff }, 1,    16,     128,     4096,   0x00000008,   896,    30,     2,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000014},   // K9GBG08
    { {0xec, 0xd5, 0x98, 0x71, 0xff, 0xff, 0xff, 0xff }, 1,     8,     256,     2048,   0x00000008,   896,    30,     3,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000015},   // K9AAG08

    { {0xec, 0xd5, 0x94, 0x29, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     4096,   0x00000008,   896,    30,     0,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000016},   // K9GAG08U0D
    { {0xec, 0xd5, 0x84, 0x72, 0xff, 0xff, 0xff, 0xff }, 1,    16,     128,     2048,   0x00011000,   896,    24,     2,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000017 },   // K9GAG08U0E
    { {0xec, 0xd5, 0x94, 0x76, 0x54, 0xff, 0xff, 0xff }, 1,    16,     128,     2048,   0x00011408,   896,    30,     2,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000018 },   // K9GAG08U0E
    { {0xec, 0xd3, 0x84, 0x72, 0xff, 0xff, 0xff, 0xff }, 1,    16,     128,     1024,   0x00000000,   896,    24,     2,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x000019 },   // K9G8G08U0C
	{ {0xec, 0xd7, 0x94, 0x76, 0xff, 0xff, 0xff, 0xff }, 1,    16,     128,     4096,   0x00011088,   896,    30,     3,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x00001a },   // K9GBG08U0A
	{ {0xec, 0xd7, 0x94, 0x7A, 0xff, 0xff, 0xff, 0xff }, 1,    16,     128,     4096,   0x00011088,   896,    30,     3,       0,        0,     &PhysicArchiPara3, &DefDDRInfo,  0x00001b },   // K9GBG08U0A
	{ {0xec, 0xde, 0xd5, 0x7A, 0x58, 0xff, 0xff, 0xff }, 2,    16,	   128, 	4096,   0x00011888,   896,	  30,	  3,	   0,		 0, 	&PhysicArchiPara3, &DefDDRInfo,  0x00001c },   // K9LCG08U0A

	{ {0xec, 0xd7, 0x94, 0x7A, 0x54, 0xc3, 0xff, 0xff }, 1,    16,     128,     4096,   0x20000088,   896,    60,     1,       0,        3,     &PhysicArchiPara3, &DefDDRInfo,  0x00001d },   // toogle nand 1.0
	{ {0xec, 0xd7, 0x14, 0x76, 0x54, 0xc2, 0xff, 0xff }, 1,    16,     128,     4096,   0x20011088,   896,    30,     3,       0,        3,     &PhysicArchiPara3, &DefDDRInfo,  0x000022 },   // K9GBGD8U0M
    //----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,     0,       0,        0,             0,              0,       0xffffff },   // NULL
};


//==============================================================================
//============================= HYNIX NAND FLASH ===============================
//==============================================================================
struct __NandPhyInfoPar_t HynixNandTbl[] =
{
    //                NAND_CHIP_ID                     DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //---------------------------------------------------------------------------------------------------------------------------------------------------------------------

    //---------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xad, 0xd3, 0x14, 0xb6, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     2048,   0x00000008,   896,    30,     0,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x010014 },   // H27U8G8T2B
    { {0xad, 0xd5, 0x14, 0xb6, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     4096,   0x00000008,   896,    30,     0,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x010015 },   // H27UAG8T2M, H27UBG8U5M
    { {0xad, 0xd7, 0x55, 0xb6, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     4096,   0x00000008,   896,    30,     0,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x010016 },   // H27UCG8V5M
    //---------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xad, 0xd5, 0x94, 0x25, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     4096,   0x00000008,   896,    30,     2,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x010017 },   // H27UBG8U5A
    { {0xad, 0xd7, 0x95, 0x25, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     4096,   0x00000008,   896,    30,     2,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x010018 },   // H27UCG8V5A
    { {0xad, 0xd5, 0x95, 0x25, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     4096,   0x00000008,   896,    30,     2,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x010019 },   // H27UCG8VFA
    { {0xad, 0xd5, 0x94, 0x9A, 0xff, 0xff, 0xff, 0xff }, 1,    16,     256,     1024,   0x00001000,   896,    30,     2,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x01001a },   // H27UAG8T2B
    { {0xad, 0xd7, 0x94, 0x9A, 0xff, 0xff, 0xff, 0xff }, 1,    16,     256,     2048,   0x00001008,   896,    30,     2,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x01001b },   // H27UBG8T2A H27UCG8U5(D)A H27UDG8VF(D)A
    { {0xad, 0xde, 0xd5, 0x9A, 0xff, 0xff, 0xff, 0xff }, 2,    16,     256,     2048,   0x00001008,   896,    30,     2,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x01001c },   // H27UDG8V5A
    { {0xad, 0xd7, 0x94, 0x25, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     8192,   0x00001008,   896,    30,     2,      0,         0,    &PhysicArchiPara3, &DefDDRInfo,  0x01001d },   // H27UBG8T2M
    //---------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,     0,      0,         0,               0,            0,      0xffffff },
};


//==============================================================================
//============================= TOSHIBA NAND FLASH =============================
//==============================================================================
struct __NandPhyInfoPar_t ToshibaNandTbl[] =
{
    //                    NAND_CHIP_ID                 DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0x98, 0xd3, 0x94, 0xba, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     2048,   0x00000008,   896,    20,     0,     0,          0,   &PhysicArchiPara6, &DefDDRInfo,  0x020008 },   // TC58NVG3D1DTG00
    { {0x98, 0xd7, 0x95, 0xba, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     8192,   0x00000008,   896,    30,     2,     0,          0,   &PhysicArchiPara7, &DefDDRInfo,  0x020009 },   // TC58NVG6D1DTG20
    { {0x98, 0xd5, 0x94, 0xba, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     4096,   0x00000008,   896,    30,     2,     0,          0,   &PhysicArchiPara7, &DefDDRInfo,  0x02000a },    // TH58NVG5D1DTG20
    { {0x98, 0xd5, 0x94, 0x32, 0xff, 0xff, 0xff, 0xff }, 1,    16,     128,     2048,   0x00000008,   896,    25,     1,     0,          0,   &PhysicArchiPara8, &DefDDRInfo,  0x02000b },    // TH58NVG4D2ETA20 TH58NVG4D2FTA20 TH58NVG5D2ETA00
    { {0x98, 0xd7, 0x94, 0x32, 0xff, 0xff, 0xff, 0xff }, 1,    16,     128,     4096,   0x00000008,   896,    25,     2,     0,          0,   &PhysicArchiPara8, &DefDDRInfo,  0x02000c },    // TH58NVG5D2FTA00 TH58NVG6D2FTA20
    { {0x98, 0xd7, 0x95, 0x32, 0xff, 0xff, 0xff, 0xff }, 2,    16,     128,     4096,   0x00000008,   896,    25,     1,     0,          0,   &PhysicArchiPara8, &DefDDRInfo,  0x02000d },    // TH58NVG6D2ETA20

    //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	{ {0x98, 0xd7, 0xd4, 0x32, 0x76, 0x55, 0xff, 0xff }, 1,    16,     128,     4096,   0x00000088,   896,    30,     2,     0,          0,   &PhysicArchiPara8, &DefDDRInfo,  0x020015 },    // TH58NVG6E2FTA00
	{ {0x98, 0xd3, 0x91, 0x26, 0x76, 0xff, 0xff, 0xff }, 1,     8,      64,     4096,   0x00088,   896,    30,     3, 	 0,       0,   &PhysicArchiPara9,    &DefDDRInfo,     0x020019 },    // TC58NVG3S0HTAI0

	//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,     0,         0,      0,          0,             0,         0xffffff},   // NULL
};


//==============================================================================
//============================= MICON NAND FLASH ===============================
//==============================================================================
struct __NandPhyInfoPar_t MicronNandTbl[] =
{
    //                   NAND_CHIP_ID                  DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0x2c, 0xd5, 0x94, 0x3e, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     4096,   0x00000008,   896,    30,     0,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x030009 },   // MT29F16G08MAA, MT29F32G08QAA, JS29F32G08AAM, JS29F32G08CAM
    { {0x2c, 0xd5, 0xd5, 0x3e, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     4096,   0x00000008,   896,    30,     0,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x03000a },   // MT29F64G08TAA, JS29F64G08FAM
    //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0x2c, 0xd7, 0x94, 0x3e, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     8192,   0x00000208,   896,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x03000b },   // MT29F32G08CBAAA,MT29F64G08CFAAA
    { {0x2c, 0xd7, 0xd5, 0x3e, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     4096,   0x00000008,   896,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x03000c },   // MT29F64G08CTAA
    { {0x2c, 0xd9, 0xd5, 0x3e, 0xff, 0xff, 0xff, 0xff }, 2,     8,     128,     8192,   0x00000008,   896,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x03000d },   // MT29F128G08,
    { {0x2c, 0x68, 0x04, 0x46, 0xff, 0xff, 0xff, 0xff }, 1,     8,     256,     4096,   0x00001208,   896,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x03000e },   // MT29F32G08CBABA
    { {0x2c, 0x88, 0x05, 0xC6, 0xff, 0xff, 0xff, 0xff }, 2,     8,     256,     4096,   0x00001208,   896,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x03000f },   // MT29F128G08CJABA
    { {0x2c, 0x88, 0x04, 0x4B, 0xff, 0xff, 0xff, 0xff }, 1,    16,     256,     4096,   0x00001208,   896,    40,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x030010 },   // MT29F64G08CBAAA
    { {0x2c, 0x68, 0x04, 0x4A, 0xff, 0xff, 0xff, 0xff }, 1,     8,     256,     4096,   0x00001208,   896,    40,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x030011 },   // MT29F32G08CBACA
    { {0x2c, 0x48, 0x04, 0x4A, 0xff, 0xff, 0xff, 0xff }, 1,     8,     256,     2048,   0x00001208,   896,    40,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x030012 },   // MT29F16G08CBACA
    { {0x2c, 0x48, 0x04, 0x46, 0xff, 0xff, 0xff, 0xff }, 1,     8,     256,     2048,   0x00001208,   896,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x030013 },   // MT29F16G08CBABA
    { {0x2c, 0x88, 0x24, 0x4B, 0xff, 0xff, 0xff, 0xff }, 1,    16,     256,     4096,   0x00001208,   870,    40,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x030016 },   // MT29F64G08CBAAA(special for customer's nand)
	//--------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x0000,        0,      0,     0,     0,          0,            0,            0,        0xffffff },   // NULL
};


//==============================================================================
//============================= INTEL NAND FLASH ===============================
//==============================================================================
struct __NandPhyInfoPar_t IntelNandTbl[] =
{
    //                 NAND_CHIP_ID                    DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	{ {0x89, 0xd7, 0x94, 0x3e, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     8192,   0x00000008,   864,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x040002 },   // MLC32GW8IMA,MLC64GW8IMA, 29F32G08AAMD2, 29F64G08CAMD2
	{ {0x89, 0xd5, 0x94, 0x3e, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     4096,   0x00000008,   864,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x040003 },   // 29F32G08CAMC1
	{ {0x89, 0xd7, 0xd5, 0x3e, 0xff, 0xff, 0xff, 0xff }, 1,     8,     128,     8192,   0x00000008,   864,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x040004 },   // 29F64G08FAMC1
	{ {0x89, 0x68, 0x04, 0x46, 0xff, 0xff, 0xff, 0xff }, 1,     8,     256,     4096,   0x00000208,   864,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x040005 },   // 29F32G08AAMDB
	{ {0x89, 0x88, 0x24, 0x4B, 0xff, 0xff, 0xff, 0xff }, 1,    16,     256,     4096,   0x1f000208,   864,    40,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x040006 },   // 29F64G08CBAAA 29F64G083AME1 29F64G08ACME3
	{ {0x89, 0xA8, 0x25, 0xCB, 0xff, 0xff, 0xff, 0xff }, 2,    16,     256,     4096,   0x00000208,   864,    30,     2,     0,          0,   &PhysicArchiPara4, &DefDDRInfo,  0x040007 },   // ?
	//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	{ {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,      0,     0,         0,          0,              0,        0xffffff },   // NULL
};


//==============================================================================
//=============================== ST NAND FLASH ================================
//==============================================================================
struct __NandPhyInfoPar_t StNandTbl[] =
{
    //              NAND_CHIP_ID                       DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,    0,       0,         0,           0,            0,         0xffffff },   // NULL
};

//==============================================================================
//============================ SPANSION NAND FLASH ==============================
//==============================================================================
struct __NandPhyInfoPar_t SpansionNandTbl[] =
{
    //                   NAND_CHIP_ID                  DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,     0,      0,         0,          0,              0,        0xffffff },   // NULL
};

//==============================================================================
//============================ POWER NAND FLASH ==============================
//==============================================================================
struct __NandPhyInfoPar_t PowerNandTbl[] =
{
    //                   NAND_CHIP_ID                 DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,     0,      0,         0,          0,                0,      0xffffff },   // NULL
};


//==============================================================================
//============================ SANDDISK NAND FLASH ==============================
//==============================================================================
struct __NandPhyInfoPar_t SandiskNandTbl[] =
{
	//					 NAND_CHIP_ID				  DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
	//------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------
	{ {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0, 	0,		 0, 	   0,	0x00000000, 	0,	   0,	  0,	  0,         0,		   0,              	  0,       0xffffff },	 // NULL
};


//==============================================================================
//============================= DEFAULT NAND FLASH =============================
//==============================================================================
struct __NandPhyInfoPar_t DefaultNandTbl[] =
{
    //                    NAND_CHIP_ID                DieCnt SecCnt  PagCnt   BlkCnt     OpOpt      DatBlk  Freq   EccMode ReadRetry DDRType   OperationPar       DDRInfo
    //-----------------------------------------------------------------------------------------------------------------------
    { {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,     0,       0,        0,   0x00000000,     0,     0,     0,           0,    0,   &DefualtPhysicArchiPara, &DefDDRInfo, 0xffffff}, //default
};

