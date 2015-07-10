/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __NAND_OSAL_H__
#define __NAND_OSAL_H__

typedef signed char __s8;
typedef unsigned char __u8;

typedef signed short __s16;
typedef unsigned short __u16;

typedef signed int __s32;
typedef unsigned int __u32;

typedef signed long long __s64;
typedef unsigned long long __u64;


typedef unsigned char   uchar;
typedef unsigned short  uint16;
typedef unsigned int    uint32;
typedef  int            sint32;
typedef unsigned long long         uint64;
typedef short           sint16;
typedef unsigned char   UINT8;
typedef unsigned int    UINT32;
typedef  signed int     SINT32;

#define __OS_NAND_DBG__

#define NULL ((void *)0)

#define __OS_LINUX_SYSTEM__
#define __OS_USE_VADDR__

#define __OS_NAND_SUPPORT_RB_INT__
#define __OS_NAND_SUPPORT_DMA_INT__


#define __FPGA_TEST__

extern void start_time(void);
extern int end_time(int data,int time,int par);

extern void *NAND_IORemap(unsigned int base_addr, unsigned int size);

//USE_SYS_CLK
extern int NAND_ClkRequest(__u32 nand_index);
extern void NAND_ClkRelease(__u32 nand_index);
extern int NAND_SetClk(__u32 nand_index, __u32 nand_clk0, __u32 nand_clk1);
extern int NAND_GetClk(__u32 nand_index, __u32 *pnand_clk0, __u32 *pnand_clk1);

extern __s32 NAND_CleanFlushDCacheRegion(__u32 buff_addr, __u32 len);
extern __u32 NAND_DMASingleMap(__u32 rw, __u32 buff_addr, __u32 len);
extern __u32 NAND_DMASingleUnmap(__u32 rw, __u32 buff_addr, __u32 len);
extern __s32 NAND_AllocMemoryForDMADescs(__u32 *cpu_addr, __u32 *phy_addr);
extern __s32 NAND_FreeMemoryForDMADescs(__u32 *cpu_addr, __u32 *phy_addr);
extern int nand_dma_config_start(__u32 rw, int addr,__u32 length);
extern int nand_request_dma(void);
extern int NAND_ReleaseDMA(__u32 nand_index);
//extern int NAND_GetVoltage(void);
//extern int NAND_ReleaseVoltage(void);



extern __u32 NAND_VA_TO_PA(__u32 buff_addr);

extern void NAND_PIORequest(__u32 nand_index);
extern __s32 NAND_PIOFuncChange_DQSc(__u32 nand_index, __u32 en);
extern __s32 NAND_PIOFuncChange_REc(__u32 nand_index, __u32 en);
extern void NAND_PIORelease(__u32 nand_index);


extern void NAND_EnRbInt(void);
extern void NAND_ClearRbInt(void);
extern int NAND_WaitRbReady(void);
extern __s32 NAND_WaitDmaFinish(void);

extern void NAND_Memset(void* pAddr, unsigned char value, unsigned int len);
extern void NAND_Memcpy(void* pAddr_dst, void* pAddr_src, unsigned int len);
extern void* NAND_Malloc(unsigned int Size);
extern void NAND_Free(void *pAddr, unsigned int Size);
extern int NAND_Print(const char *fmt, ...);
extern int NAND_Print_DBG(const char *fmt, ...);


extern __u32 NAND_GetIOBaseAddrCH0(void);
extern __u32 NAND_GetIOBaseAddrCH1(void);
extern __u32 NAND_GetNdfcVersion(void);
extern __u32 NAND_GetNdfcDmaMode(void);
extern __u32 NAND_GetMaxChannelCnt(void);
extern __u32 NAND_GetNandIDNumCtrl(void);
extern __u32 NAND_GetNandExtPara(__u32 para_num);
extern void NAND_DumpReg(void);
extern void NAND_ShowEnv(__u32 type, char *name, __u32 len, __u32 *val);
extern void NAND_Print_Version(void);

extern int NAND_PhysicLockInit(void);
extern int NAND_PhysicLock(void);
extern int NAND_PhysicUnLock(void);
extern int NAND_PhysicLockExit(void);
extern int NAND_IS_Secure_sys(void);


#define NAND_IO_BASE_ADDR0		( NAND_GetIOBaseAddrCH0() )
#define NAND_IO_BASE_ADDR1		( NAND_GetIOBaseAddrCH1() )

#define MAX_NFC_CH               (NAND_GetMaxChannelCnt())

#define PLATFORM				(NAND_GetPlatform())


//define the memory set interface
#define MEMSET(x,y,z)            			NAND_Memset((x),(y),(z))

//define the memory copy interface
#define MEMCPY(x,y,z)                   	NAND_Memcpy((x),(y),(z))

//define the memory alocate interface
#define MALLOC(x)                       	NAND_Malloc((x))

//define the memory release interface
#define FREE(x,size)                    	NAND_Free((x),(size))

//define the message print interface
#define PRINT								NAND_Print
#define PRINT_DBG								NAND_Print_DBG




#endif //__NAND_OSAL_H__
