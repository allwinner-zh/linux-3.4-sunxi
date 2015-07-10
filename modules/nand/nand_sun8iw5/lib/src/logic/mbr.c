/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "../include/mbr.h"
#include "../include/nand_type.h"
#include "../include/nand_drv_cfg.h"
#include "../include/nand_logic.h"
#include "../include/nand_format.h"
#include "../include/nand_scan.h"
#include "../include/nand_physic.h"
#include "../include/nfc.h"


ND_MBR *mbr = {0};

int part_secur[ND_MAX_PART_COUNT] = {0};

extern struct nand_disk nand_disk_array[ND_MAX_PART_COUNT];
extern __u32 nand_part_cnt;


typedef struct tag_CRC32_DATA
{
	__u32 CRC;				//int的大小是32位
	__u32 CRC_32_Tbl[256];	//用来保存码表
}CRC32_DATA_t;

CRC32_DATA_t nand_crc32 = {0};

__u32 calc_crc32(void * buffer, __u32 length)
{
	__u32 i, j;
		//
	__u32 CRC32 = 0xffffffff; //设置初始值
	nand_crc32.CRC = 0;

	for( i = 0; i < 256; ++i)//用++i以提高效率
	{
		nand_crc32.CRC = i;
		for( j = 0; j < 8 ; ++j)
		{
			//这个循环实际上就是用"计算法"来求取CRC的校验码
			if(nand_crc32.CRC & 1)
				nand_crc32.CRC = (nand_crc32.CRC >> 1) ^ 0xEDB88320;
			else //0xEDB88320就是CRC-32多项表达式的值
				nand_crc32.CRC >>= 1;
		}
		nand_crc32.CRC_32_Tbl[i] = nand_crc32.CRC;
	}

	CRC32 = 0xffffffff; //设置初始值
    for( i = 0; i < length; ++i)
    {
        CRC32 = nand_crc32.CRC_32_Tbl[(CRC32^((unsigned char*)buffer)[i]) & 0xff] ^ (CRC32>>8);
    }
    //return CRC32;
	return CRC32^0xffffffff;
}

__s32 _get_mbr(void)
{
	__u32 	i;
	__s32  mbr_get_sucess = 0;

	/*request mbr space*/
	mbr = MALLOC(sizeof(ND_MBR));
	if(mbr == NULL)
	{
		PRINT("%s : request memory fail\n",__FUNCTION__);
		return -1;
	}

	/*get mbr from nand device*/
	for(i = 0; i < ND_MBR_COPY_NUM; i++)
	{
		if(LML_Read((ND_MBR_START_ADDRESS + ND_MBR_SIZE*i)/512,ND_MBR_SIZE/512,mbr) == 0)
		{
			/*checksum*/
			if(*(__u32 *)mbr == calc_crc32((__u32 *)mbr + 1,ND_MBR_SIZE - 4))
			{
				mbr_get_sucess = 1;
				break;
			}
		}
	}

	if(mbr_get_sucess)
		return 0;
	else
		return -1;

}

__s32 _free_mbr(void)
{
	if(mbr)
	{
		FREE(mbr,sizeof(ND_MBR));
		mbr = 0;
	}

	return 0;
}

int mbr2disks(struct nand_disk* disk_array)
{
	int part_cnt = 0;

	//PRINT("mbr2disks, nand update disk info to system!\n");
	//PRINT("nand total disk size: 0x%x sec, %dM \n", DiskSize, DiskSize/2048);
	//查找出所有的LINUX盘符
	for(part_cnt = 0; part_cnt < nand_part_cnt && part_cnt < NAND_MAX_PART_CNT; part_cnt++)
	{
        disk_array[part_cnt].offset =nand_disk_array[part_cnt].offset;
		disk_array[part_cnt].size = nand_disk_array[part_cnt].size;
		//PRINT("part %d: offset = %x, size = %x\n", part_cnt, disk_array[part_cnt].offset, disk_array[part_cnt].size);
	}

	return nand_part_cnt;
}

int NAND_PartInit(void)
{
	int part_cnt = 0;
	int part_index;
	struct __NandPartTable_t NandPartTable;

	if(_get_mbr()){
			PRINT("get mbr error\n" );
		goto mbr_error;
	}
	part_index = 0;

	for(part_cnt = 0; part_cnt<ND_MAX_PART_COUNT; part_cnt++)
		part_secur[part_index] = 0;

	//查找出所有的LINUX盘符
	for(part_cnt = 0; part_cnt < mbr->PartCount && part_cnt < ND_MAX_PART_COUNT; part_cnt++)
	{
	    //if((mbr->array[part_cnt].user_type == 2) || (mbr->array[part_cnt].user_type == 0))
	    {
			//PRINT("The %d disk name = %s, class name = %s, disk size = %d\n", part_index, mbr->array[part_cnt].name,
			//			mbr->array[part_cnt].classname, mbr->array[part_cnt].lenlo);

	        nand_disk_array[part_index].offset = mbr->array[part_cnt].addrlo;
			nand_disk_array[part_index].size = mbr->array[part_cnt].lenlo;
			part_secur[part_index] = mbr->array[part_cnt].user_type;
			part_index ++;
			DBUG_MSG("The %d disk offset = %x\n", part_index - 1, nand_disk_array[part_index - 1].offset);
			DBUG_MSG("The %d disk size = %x\n", part_index - 1, nand_disk_array[part_index - 1].size);
	    }
	}
	nand_disk_array[part_index - 1].size = DiskSize - mbr->array[mbr->PartCount - 1].addrlo;
	_free_mbr();
	DBUG_MSG("The %d disk size = %x\n", part_index - 1, nand_disk_array[part_index - 1].size);
	DBUG_MSG("part total count = %d\n", part_index);

	nand_part_cnt = part_index;

	NandPartTable.magic = NAND_PART_TABLE_MAGIC;
	NandPartTable.part_cnt = part_index;
	for(part_cnt = 0; part_cnt<NandPartTable.part_cnt; part_cnt++)
	{
		NandPartTable.start_sec[part_cnt] = nand_disk_array[part_cnt].offset;
		NandPartTable.sec_cnt[part_cnt] = nand_disk_array[part_cnt].size;
		NandPartTable.part_type[part_cnt] = part_secur[part_cnt];
	}

	NAND_SetPartInfo(&NandPartTable);

	return part_index;

mbr_error:
#if 1
	if(part_index == 0)
	{
		PRINT("get mbr error, set 1 part as nanda \n" );
		part_index = 0;

		for(part_cnt = 0; part_cnt<ND_MAX_PART_COUNT; part_cnt++)
			part_secur[part_index] = 0;

	    part_cnt = 1;
	    nand_part_cnt = 1;
	    nand_disk_array[0].offset = 0;
	    nand_disk_array[0].size = DiskSize;
	    return part_cnt;
	}
#endif

	return part_index;
}

