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
#include "../include/mbr.h"

//#define CACHE_DBG

#define NAND_W_CACHE_EN
#define NAND_R_CACHE_EN

#define N_NAND_W_CACHE  16
#define N_NAND_R_CACHE  1


typedef struct
{
	__u8	*data;
	__u32	size;

	__u32	hit_page;
	__u64   secbitmap;

	__u32	access_count;
	__u32   dev_num;
}__nand_cache_t;

__u32 g_w_access_cnt = 0;

__nand_cache_t nand_w_cache[N_NAND_W_CACHE] = {0};
__nand_cache_t nand_r_cache = {0};
__u32 nand_current_dev_num = 0;
__u8 *tmp_cache_buf = 0;
__u8 *CacheMainBuf = 0;
__u8 *CacheSpareBuf = 0;


__u32 _get_valid_bits(__u64 secbitmap)
{
	__u32 validbit = 0;

	while(secbitmap)
	{
		if(secbitmap & (__u64)0x1)
			validbit++;
		secbitmap >>= 1;
	}

	return validbit;
}

__u32 _get_first_valid_bit(__u64 secbitmap)
{
	__u32 firstbit = 0;

	while(!(secbitmap & (__u64)0x1))
	{
		secbitmap >>= 1;
		firstbit++;
	}

	return firstbit;
}

__s32 _flush_w_cache(void)
{
	__u32	i, pos;
	__u32 sec_index;
	__u64 tempsecbitmap;

	for(i = 0; i < N_NAND_W_CACHE; i++)
	{
		if(nand_w_cache[i].hit_page != 0xffffffff)
		{
		#if 0
			if(nand_w_cache[i].secbitmap != FULL_BITMAP_OF_LOGIC_PAGE)
				LML_PageRead(nand_w_cache[i].hit_page,(nand_w_cache[i].secbitmap ^ FULL_BITMAP_OF_LOGIC_PAGE)&FULL_BITMAP_OF_LOGIC_PAGE,nand_w_cache[i].data);

			LML_PageWrite(nand_w_cache[i].hit_page,FULL_BITMAP_OF_LOGIC_PAGE,nand_w_cache[i].data);
		#else
            //PRINT("_fill_nand_cache, LR, 0x%x, 0x%x, 0x%x\n", nand_w_cache[pos].hit_page, nand_w_cache[pos].secbitmap, nand_r_cache.data);
			pos = i;
			if(nand_w_cache[pos].secbitmap != FULL_BITMAP_OF_LOGIC_PAGE)
			{
				sec_index =0;
				tempsecbitmap = nand_w_cache[pos].secbitmap;

				LML_PageRead(nand_w_cache[pos].hit_page,FULL_BITMAP_OF_LOGIC_PAGE,CacheMainBuf);
				while(tempsecbitmap)
				{
					if(tempsecbitmap&((__u64)0x1))
					{
						MEMCPY(CacheMainBuf+ 512*sec_index, nand_w_cache[pos].data + 512*sec_index, 512);
					}
					tempsecbitmap>>=1;
					sec_index++;
				}
				LML_PageWrite(nand_w_cache[pos].hit_page, FULL_BITMAP_OF_LOGIC_PAGE, CacheMainBuf);

			}
			else
			{
			    LML_PageWrite(nand_w_cache[pos].hit_page, FULL_BITMAP_OF_LOGIC_PAGE, nand_w_cache[pos].data);
			}
		#endif

		    #ifdef NAND_R_CACHE_EN
			/*disable read cache with current page*/
			if (nand_r_cache.hit_page == nand_w_cache[i].hit_page){
					nand_r_cache.hit_page = 0xffffffff;
					nand_r_cache.secbitmap = 0;
			}
			#endif

			nand_w_cache[i].hit_page = 0xffffffff;
			nand_w_cache[i].secbitmap = 0;
			nand_w_cache[i].access_count = 0;

		}
	}


	return 0;

}

__s32 _flush_w_cache_simple(__u32 i)
{
    __u32 pos;
    __u32 sec_index;
	__u64 tempsecbitmap;

	if(nand_w_cache[i].hit_page != 0xffffffff)
	{
	    #if 0
		if(nand_w_cache[i].secbitmap != FULL_BITMAP_OF_LOGIC_PAGE)
			LML_PageRead(nand_w_cache[i].hit_page,(nand_w_cache[i].secbitmap ^ FULL_BITMAP_OF_LOGIC_PAGE)&FULL_BITMAP_OF_LOGIC_PAGE,nand_w_cache[i].data);

		LML_PageWrite(nand_w_cache[i].hit_page,FULL_BITMAP_OF_LOGIC_PAGE,nand_w_cache[i].data);
		#else
            //PRINT("_fill_nand_cache, LR, 0x%x, 0x%x, 0x%x\n", nand_w_cache[pos].hit_page, nand_w_cache[pos].secbitmap, nand_r_cache.data);
			pos = i;
			if(nand_w_cache[pos].secbitmap != FULL_BITMAP_OF_LOGIC_PAGE)
			{
				sec_index =0;
				tempsecbitmap = nand_w_cache[pos].secbitmap;

				LML_PageRead(nand_w_cache[pos].hit_page,FULL_BITMAP_OF_LOGIC_PAGE,CacheMainBuf);
				while(tempsecbitmap)
				{
					if(tempsecbitmap&((__u64)0x1))
					{
						MEMCPY(CacheMainBuf+ 512*sec_index, nand_w_cache[pos].data + 512*sec_index, 512);
					}
					tempsecbitmap>>=1;
					sec_index++;
				}
				LML_PageWrite(nand_w_cache[pos].hit_page, FULL_BITMAP_OF_LOGIC_PAGE, CacheMainBuf);

			}
			else
			{
			    LML_PageWrite(nand_w_cache[pos].hit_page, FULL_BITMAP_OF_LOGIC_PAGE, nand_w_cache[pos].data);
			}
		#endif

		#ifdef NAND_R_CACHE_EN
		/*disable read cache with current page*/
		if (nand_r_cache.hit_page == nand_w_cache[i].hit_page){
				nand_r_cache.hit_page = 0xffffffff;
				nand_r_cache.secbitmap = 0;
		}
		#endif

		nand_w_cache[i].hit_page = 0xffffffff;
		nand_w_cache[i].secbitmap = 0;
		nand_w_cache[i].access_count = 0;
		nand_w_cache[i].dev_num= 0xffffffff;


	}

	return 0;

}


__s32 NAND_CacheFlush(void)
{
	//__u32	i;
    #ifdef NAND_W_CACHE_EN
	_flush_w_cache();
	#endif

	return 0;

}

__s32 NAND_CacheFlushSingle(void)
{
	//__u32	i;
    #ifdef NAND_W_CACHE_EN
	__u32 access_cnt = nand_w_cache[0].access_count;
	__u32 i, pos,pos_2;

	pos = 0;

	for(i = 0;i < N_NAND_W_CACHE; i++)
	{
		if(nand_w_cache[i].hit_page != 0xffffffff)
		{
			pos = i;
			access_cnt = nand_w_cache[pos].access_count;
			break;
		}
		else
		{
			pos = 0xffffffff;
		}
	}
	if(0xffffffff == pos)
		return 0;

	pos_2 = pos;
	for (i = pos+1; i < N_NAND_W_CACHE; i++)
	{
		if(nand_w_cache[i].hit_page!=0xffffffff)
		{
			if (access_cnt > nand_w_cache[i].access_count)
			{
				pos_2 = i;
				access_cnt = nand_w_cache[i].access_count;
			}
		}
	}



	_flush_w_cache_simple(pos_2);
	#endif

	return 0;

}


__s32 NAND_CacheFlushDev(__u32 dev_num)
{
	__u32	i;

	#ifdef NAND_W_CACHE_EN
	//printk("Nand Flush Dev: 0x%x\n", dev_num);
	for(i = 0; i < N_NAND_W_CACHE; i++)
	{
		if(nand_w_cache[i].dev_num == dev_num)
		{
			//printk("nand flush cache 0x%x\n", i);
			_flush_w_cache_simple(i);
		}


	}
	#endif

	return 0;

}

__s32 NAND_CacheFlushLogicBlk(__u32 logicblk)
{
	__u32	i;

	#ifdef NAND_W_CACHE_EN
	//printk("Nand Flush Dev: 0x%x\n", dev_num);
	for(i = 0; i < N_NAND_W_CACHE; i++)
	{
		if(nand_w_cache[i].hit_page!= 0xffffffff)
		{
			if(nand_w_cache[i].hit_page / PAGE_CNT_OF_LOGIC_BLK == logicblk)
			//printk("nand flush cache 0x%x\n", i);
			_flush_w_cache_simple(i);
		}


	}
	#endif

	return 0;

}




void _get_data_from_w_cache(__u32 page, __u64 secbitmap, void *buf)
{
	__u32 i,j;
	__u32 sec;
	//__u32 page;
	__u64 SecBitmap_tmp;

	for (i = 0; i < N_NAND_W_CACHE; i++)
	{
		if(nand_w_cache[i].hit_page == page)
		{
			SecBitmap_tmp = secbitmap&nand_w_cache[i].secbitmap;
			if(!SecBitmap_tmp)
				break;

			for(j=0;j<SECTOR_CNT_OF_LOGIC_PAGE;j++)
			{
				if(((__u64)1<<j)&SecBitmap_tmp)
					MEMCPY((__u8 *)buf + j*512, nand_w_cache[i].data + j*512, 512);
			}
		}
	}

}

__u32 _get_data_from_r_cache(__u32 blk, __u32 nblk, void *buf)
{
	__u32 i;
	__u32 sec;
	__u32 page,SecWithinPage;
	__u64 SecBitmap;

	if((blk/SECTOR_CNT_OF_LOGIC_PAGE)!=((blk+nblk-1)/SECTOR_CNT_OF_LOGIC_PAGE))  //multi page
	{
		LOGICCTL_ERR("multi page read, 0x%x, 0x%x, 0x%x\n", blk, nblk, (__u32)buf);
		return -1;
	}


	if((blk/SECTOR_CNT_OF_LOGIC_PAGE)!= nand_r_cache.hit_page) //r_cache miss
	{
		//PRINT("r cache miss, hitpage:0x%x, current page:0x%x\n", nand_r_cache.hit_page, sec/SECTOR_CNT_OF_LOGIC_PAGE);
		return -1;
	}

	for(sec = blk; sec < blk + nblk; sec++)
	{
		SecWithinPage = sec % SECTOR_CNT_OF_LOGIC_PAGE;
		SecBitmap = ((__u64)1<< SecWithinPage);
		page = sec / SECTOR_CNT_OF_LOGIC_PAGE;

		if ((nand_r_cache.hit_page == page) && (nand_r_cache.secbitmap & SecBitmap))
		{
			MEMCPY((__u8 *)buf + (sec - blk) * 512, nand_r_cache.data + SecWithinPage * 512,512);//SecWithinPage
			//break;
		}

	}

	return 0;
}

__s32 _get_all_data_from_cache(__u32 blk, __u32 nblk, void *buf)
{
	__u32 i;
	__u32 sec;
	__u32 page,SecWithinPage;
	__u64 SecBitmap =0, tmpSecBitmap_r = 0, tmpSecBitmap_w = 0;
	__s32 r_cache_hit_page_index = -1, w_cache_hit_page_index = -1;


	if((blk/SECTOR_CNT_OF_LOGIC_PAGE)!=((blk+nblk-1)/SECTOR_CNT_OF_LOGIC_PAGE))  //multi page
	{
		LOGICCTL_ERR("multi page read, 0x%x, 0x%x, 0x%x\n", blk, nblk, (__u32)buf);
		return -1;
	}

	page = blk/SECTOR_CNT_OF_LOGIC_PAGE;


	if(page== nand_r_cache.hit_page) //r_cache miss
	{
		r_cache_hit_page_index = 0;
		tmpSecBitmap_r = nand_r_cache.secbitmap;
	}

	for(i=0;i<N_NAND_W_CACHE;i++)
	{
		if(page== nand_w_cache[i].hit_page)
		{
			w_cache_hit_page_index = i;
			tmpSecBitmap_w = nand_w_cache[i].secbitmap;
			break;
		}

	}

	for(sec = blk; sec < blk + nblk; sec++)
	{
		SecWithinPage = sec % SECTOR_CNT_OF_LOGIC_PAGE;
		SecBitmap = ((__u64)1 << SecWithinPage);

		//if(((SecBitmap&tmpSecBitmap_r)&&(SecBitmap&tmpSecBitmap_w)))
		//	PRINT("_get_all_data_from_cache warning, both rcache and wcache hit page0x%x\n", page);
		if(!((SecBitmap&tmpSecBitmap_r)||(SecBitmap&tmpSecBitmap_w)))
			return -1;
	}

	for(sec = blk; sec < blk + nblk; sec++)
	{
		SecWithinPage = sec % SECTOR_CNT_OF_LOGIC_PAGE;
		SecBitmap = ((__u64)1 << SecWithinPage);
		page = sec / SECTOR_CNT_OF_LOGIC_PAGE;


		if(w_cache_hit_page_index>=0) //get data from w cache
		{
			if ((nand_w_cache[w_cache_hit_page_index].hit_page == page) && (nand_w_cache[w_cache_hit_page_index].secbitmap & SecBitmap))
			{
				MEMCPY((__u8 *)buf + (sec - blk) * 512, nand_w_cache[w_cache_hit_page_index].data + SecWithinPage * 512,512);//SecWithinPage
				continue;
			}
		}
		else if ((nand_r_cache.hit_page == page) && (nand_r_cache.secbitmap & SecBitmap))  //get data from r cache
		{
			MEMCPY((__u8 *)buf + (sec - blk) * 512, nand_r_cache.data + SecWithinPage * 512,512);//SecWithinPage
			continue;
		}
		else
		{
			LOGICCTL_ERR("_get_all_data_from_cache error, no data in cache of sec %d, page: 0x%x\n", SecWithinPage, page);
			return -1;
		}


	}

	return 0;
}




void _get_one_page(__u32 page,__u64 SecBitmap,__u8 *data)
{
	__u32 i;
	__u8 *tmp = data;


	if(page == nand_r_cache.hit_page)
	{
		for(i = 0;i < SECTOR_CNT_OF_LOGIC_PAGE; i++)
		{
			if(SecBitmap & ((__u64)1<<i))
			{
				MEMCPY(tmp + (i<<9),nand_r_cache.data + (i<<9),512);
			}
		}
	}

	else
	{
		if(SecBitmap == FULL_BITMAP_OF_LOGIC_PAGE)
		{

			//PRINT("0 %s,%x\n",__func__,(__u32)tmp);
			LML_PageRead(page,FULL_BITMAP_OF_LOGIC_PAGE,tmp);
		}
		else
		{


			//PRINT("1 %s,%x\n",__func__,(__u32)nand_r_cache.data);
			LML_PageRead(page,FULL_BITMAP_OF_LOGIC_PAGE,nand_r_cache.data);
			nand_r_cache.hit_page = page;
			nand_r_cache.secbitmap = FULL_BITMAP_OF_LOGIC_PAGE;

			for(i = 0;i < SECTOR_CNT_OF_LOGIC_PAGE; i++)
			{
				if(SecBitmap & ((__u64)1<<i))
				{
					MEMCPY(tmp + (i<<9),nand_r_cache.data + (i<<9),512);
				}
			}
		}
	}

	//SecBitmap = 0;
}


void _get_sectors(__u32 page,__u64 SecBitmap,__u8 *data)
{
#if 1
    __u8 *tmp = data;

	LML_PageRead(page,SecBitmap,tmp);
#else
    __u32 i;
	__u8 *tmp = data;

	if(SecBitmap == FULL_BITMAP_OF_LOGIC_PAGE)
	{
		LML_PageRead(page,FULL_BITMAP_OF_LOGIC_PAGE,tmp);
	}
	else
	{
		LML_PageRead(page,FULL_BITMAP_OF_LOGIC_PAGE,CacheMainBuf);

		for(i = 0;i < SECTOR_CNT_OF_LOGIC_PAGE; i++)
		{
			if(SecBitmap & ((__u64)1<<i))
			{
				MEMCPY(tmp + (i<<9),CacheMainBuf + (i<<9),512);
			}
		}
	}

#endif
}


__s32 NAND_CacheRead(__u32 blk, __u32 nblk, void *buf)
{
	__u32	nSector,StartSec;
	__u32	page;
	__u32	SecWithinPage;
	__u64	SecBitmap;
	__u8 	*pdata;

	if((blk>=DiskSize)||(blk+nblk>DiskSize))
	{
		LOGICCTL_ERR("NAND_CacheRead beyond nand disk size, blk: 0x%x, nblk:0x%x, buf: 0x%x\n", blk, nblk, (__u32)buf);
		return -1;
	}


	nSector 	= nblk;
	StartSec 	= blk;
	SecBitmap 	= 0;
	page 		= 0xffffffff;
	pdata		= (__u8 *)buf;

	/*combind sectors to pages*/
	while(nSector)
	{
		SecWithinPage = StartSec % SECTOR_CNT_OF_LOGIC_PAGE;
		SecBitmap |= ((__u64)1 << SecWithinPage);
		page = StartSec / SECTOR_CNT_OF_LOGIC_PAGE;

		/*close page if last sector*/
		if (SecWithinPage == (SECTOR_CNT_OF_LOGIC_PAGE - 1))
		{

			__u8 *tmp = pdata + 512 - 512*_get_valid_bits(SecBitmap) - 512 * _get_first_valid_bit(SecBitmap);
			_get_one_page(page, SecBitmap, tmp);
			_get_data_from_w_cache(page, SecBitmap, tmp);
			SecBitmap = 0;
		}

		/*reset variable*/
		nSector--;
		StartSec++;
		pdata += 512;
	}

	/*fill opened page*/
	if (SecBitmap)
	{
		__u8	*tmp = pdata - 512*_get_valid_bits(SecBitmap) - 512 * _get_first_valid_bit(SecBitmap);
		_get_one_page(page, SecBitmap, tmp);
		_get_data_from_w_cache(page, SecBitmap, tmp);
		SecBitmap = 0;
	}

	/*renew data from cache*/
	//_get_data_from_cache(blk,nblk,buf);

	return 0;

}

__s32 NAND_CacheReadSecs(__u32 blk, __u32 nblk, void *buf)
{
	__u32	nSector,StartSec;
	__u32	page;
	__u64	SecBitmap;
	__u32   SecWithinPage;
	__u8 	*pdata;
	__u8    *tmp;


	//PRINT("CRS, 0x%x, 0x%x, 0x%x\n", (__u32)blk, (__u32)nblk, (__u32)buf);

	if(_get_all_data_from_cache(blk,nblk,buf)==0) //get all data from r cache
	{
		return 0;
	}


	nSector 	= nblk;
	StartSec 	= blk;
	SecBitmap 	= 0;
	page 		= 0xffffffff;
	pdata		= (__u8 *)buf;

	/*combind sectors to pages*/
	while(nSector)
	{
		SecWithinPage = StartSec % SECTOR_CNT_OF_LOGIC_PAGE;
		SecBitmap |= ((__u64)1 << SecWithinPage);
		page = StartSec / SECTOR_CNT_OF_LOGIC_PAGE;

		/*close page if last sector*/
		if (SecWithinPage == (SECTOR_CNT_OF_LOGIC_PAGE - 1))
		{
		    tmp = pdata + 512 - 512*_get_valid_bits(SecBitmap) - 512 * _get_first_valid_bit(SecBitmap);
			_get_sectors(page,SecBitmap, tmp);
			_get_data_from_w_cache(page,SecBitmap, tmp);
			SecBitmap = 0;
		}

		/*reset variable*/
		nSector--;
		StartSec++;
		pdata += 512;
	}

	/*fill opened page*/
	if (SecBitmap)
	{
	    tmp = pdata - 512*_get_valid_bits(SecBitmap) - 512 * _get_first_valid_bit(SecBitmap);
		_get_sectors(page,SecBitmap, tmp);
		_get_data_from_w_cache(page,SecBitmap, tmp);
		SecBitmap = 0;
	}

	/*renew data from cache*/
	//_get_data_from_w_cache(blk,nblk,buf);

    	//PRINT("CRE\n");
	return 0;

}



__s32 _fill_nand_cache(__u32 page, __u64 secbitmap, __u8 *pdata)
{
	__u8	hit;
	__u8	i;
	__u8 	pos = 0xff;
    __u32 sec_index;
	__u64 tempsecbitmap;



	g_w_access_cnt++;

	hit = 0;

	for (i = 0; i < N_NAND_W_CACHE; i++)
	{
		/*merge data if cache hit*/
		if (nand_w_cache[i].hit_page == page){
			hit = 1;
			MEMCPY(nand_w_cache[i].data + 512 * _get_first_valid_bit(secbitmap),pdata, 512 * _get_valid_bits(secbitmap));
			nand_w_cache[i].secbitmap |= secbitmap;
			nand_w_cache[i].access_count = g_w_access_cnt;
			pos = i;
			break;
		}
	}

	/*post data if cache miss*/
	if (!hit)
	{
		/*find cache to post*/
		for (i = 0; i < N_NAND_W_CACHE; i++)
		{
			if (nand_w_cache[i].hit_page == 0xffffffff)
			{
				pos = i;
				break;
			}
		}
		//no wr_cache
		if (pos == 0xff)
		{
			__u32 access_cnt = nand_w_cache[0].access_count;
			pos = 0;

			for (i = 1; i < N_NAND_W_CACHE; i++)
			{
				if (access_cnt > nand_w_cache[i].access_count)
				{
					pos = i;
					access_cnt = nand_w_cache[i].access_count;
				}

				if((nand_w_cache[i].hit_page == page-1)&&(page>0))
				{
				    pos = i;
				    break;
				}
			}

#if 0
			if(nand_w_cache[pos].secbitmap != FULL_BITMAP_OF_LOGIC_PAGE)
				LML_PageRead(nand_w_cache[pos].hit_page,nand_w_cache[pos].secbitmap ^ FULL_BITMAP_OF_LOGIC_PAGE,nand_w_cache[pos].data);

			LML_PageWrite(nand_w_cache[pos].hit_page, FULL_BITMAP_OF_LOGIC_PAGE, nand_w_cache[pos].data);
#else
            //PRINT("_fill_nand_cache, LR, 0x%x, 0x%x, 0x%x\n", nand_w_cache[pos].hit_page, nand_w_cache[pos].secbitmap, nand_r_cache.data);
			if(nand_w_cache[pos].secbitmap != FULL_BITMAP_OF_LOGIC_PAGE)
			{
				sec_index =0;
				tempsecbitmap = nand_w_cache[pos].secbitmap;

				LML_PageRead(nand_w_cache[pos].hit_page,FULL_BITMAP_OF_LOGIC_PAGE,CacheMainBuf);
				while(tempsecbitmap)
				{
					if(tempsecbitmap&((__u64)0x1))
					{
						MEMCPY(CacheMainBuf+ 512*sec_index, nand_w_cache[pos].data + 512*sec_index, 512);
					}
					tempsecbitmap>>=1;
					sec_index++;
				}
				LML_PageWrite(nand_w_cache[pos].hit_page, FULL_BITMAP_OF_LOGIC_PAGE, CacheMainBuf);

			}
			else
			{
			    LML_PageWrite(nand_w_cache[pos].hit_page, FULL_BITMAP_OF_LOGIC_PAGE, nand_w_cache[pos].data);
			}
#endif

			nand_w_cache[pos].access_count = 0;

			//add by penggang
			/*disable read cache with current page*/
			#ifdef NAND_R_CACHE_EN
			if (nand_r_cache.hit_page == nand_w_cache[pos].hit_page){
				nand_r_cache.hit_page = 0xffffffff;
				nand_r_cache.secbitmap = 0;
			}
			#endif

		}

		/*merge data*/
		MEMCPY(nand_w_cache[pos].data + 512 * _get_first_valid_bit(secbitmap),pdata, 512 * _get_valid_bits(secbitmap));
		nand_w_cache[pos].hit_page = page;
		nand_w_cache[pos].secbitmap = secbitmap;
		nand_w_cache[pos].access_count = g_w_access_cnt;
		nand_w_cache[i].dev_num= nand_current_dev_num;

	}

	if (g_w_access_cnt == 0)
	{
		for (i = 0; i < N_NAND_W_CACHE; i++)
			nand_w_cache[i].access_count = 0;
		g_w_access_cnt = 1;
		nand_w_cache[pos].access_count = g_w_access_cnt;
	}

	return 0;
}

__s32 NAND_CacheWrite(__u32 blk, __u32 nblk, void *buf)
{
	__u32	nSector,StartSec;
	__u32	page;
	__u32	SecWithinPage;
	__u64	SecBitmap;
	__u32	i;
	__u8 	*pdata;
	//__u32  hit = 0;

	if((blk>=DiskSize)||(blk+nblk>DiskSize))
	{
		LOGICCTL_ERR("NAND_CacheWrite beyond nand disk size, blk: 0x%x, nblk:0x%x, buf: 0x%x\n", blk, nblk, (__u32)buf);
		return -1;
	}

	nSector 	= nblk;
	StartSec 	= blk;
	SecBitmap 	= 0;
	page 		= 0xffffffff;
	pdata		= (__u8 *)buf;

	/*combind sectors to pages*/
	while(nSector)
	{
		SecWithinPage = StartSec % SECTOR_CNT_OF_LOGIC_PAGE;
		SecBitmap |= ((__u64)1 << SecWithinPage);
		page = StartSec / SECTOR_CNT_OF_LOGIC_PAGE;


		/*close page if last sector*/
		if (SecWithinPage == (SECTOR_CNT_OF_LOGIC_PAGE - 1))
		{
			/*write to nand flash if align one logic page*/
			if(SecBitmap == FULL_BITMAP_OF_LOGIC_PAGE)
			{
				/*disable write cache with current page*/
				for (i = 0; i < N_NAND_W_CACHE; i++)
				{
					if(nand_w_cache[i].hit_page == page)
					{
						nand_w_cache[i].hit_page = 0xffffffff;
						nand_w_cache[i].secbitmap = 0;
						nand_w_cache[i].dev_num= 0xffffffff;
						//hit=1;
					}
					//else if((nand_w_cache[i].hit_page == page-1)&&(page>0))
					else if(((nand_w_cache[i].hit_page/PAGE_CNT_OF_LOGIC_BLK)== (page/PAGE_CNT_OF_LOGIC_BLK))&&(page>0))
					{
					    _flush_w_cache_simple(i);
					}


				}
				/*disable read cache with current page*/
				#ifdef NAND_R_CACHE_EN
				if (nand_r_cache.hit_page == page){
					nand_r_cache.hit_page = 0xffffffff;
					nand_r_cache.secbitmap = 0;
				}
				#endif

                //if(!hit)
                //    _flush_w_cache();

				LML_PageWrite(page,FULL_BITMAP_OF_LOGIC_PAGE,pdata + 512 - 512*SECTOR_CNT_OF_LOGIC_PAGE);
			}

			/*fill to cache if unalign one logic page*/
			else
				_fill_nand_cache(page, SecBitmap, pdata + 512 - 512*_get_valid_bits(SecBitmap));

			SecBitmap = 0;
		}


		/*reset variable*/
		nSector--;
		StartSec++;
		pdata += 512;
	}

	/*fill opened page*/
	if (SecBitmap)
		_fill_nand_cache(page,SecBitmap,pdata - 512*_get_valid_bits(SecBitmap));

	return 0;
}


__s32 NAND_CacheOpen(void)
{
	__u32 i;

	g_w_access_cnt = 0;
	CacheMainBuf = MALLOC(512 * SECTOR_CNT_OF_LOGIC_PAGE);
	if(!CacheMainBuf)
	{
		LOGICCTL_ERR(" NAND malloc CacheMainBuf error\n");
		//while(1);
	}
	else
	{
		LOGICCTL_DBG(" NAND malloc CacheMainBuf 0x%x ok\n", CacheMainBuf);
	}

	CacheSpareBuf = MALLOC(1024);
	if(!CacheSpareBuf)
	{
		LOGICCTL_ERR(" NAND malloc CacheSpareBuf error\n");
		//while(1);
	}


	for(i = 0; i < N_NAND_W_CACHE; i++)
	{
		nand_w_cache[i].size = 512 * SECTOR_CNT_OF_LOGIC_PAGE;
		nand_w_cache[i].data = MALLOC(nand_w_cache[i].size);
		nand_w_cache[i].hit_page = 0xffffffff;
		nand_w_cache[i].secbitmap = 0;
		nand_w_cache[i].access_count = 0;
		nand_w_cache[i].dev_num= 0xffffffff;
	}

	nand_r_cache.size = 512 * SECTOR_CNT_OF_LOGIC_PAGE;
	nand_r_cache.data = MALLOC(nand_r_cache.size);
	nand_r_cache.hit_page = 0xffffffff;
	nand_r_cache.secbitmap = 0;
	nand_r_cache.access_count = 0;
	nand_r_cache.dev_num= 0xffffffff;

    tmp_cache_buf = MALLOC(512 * SECTOR_CNT_OF_LOGIC_PAGE);

	return 0;
}

__s32 NAND_CacheClose(void)
{
	__u32 i;

	NAND_CacheFlush();
	FREE(CacheMainBuf, 512 * SECTOR_CNT_OF_LOGIC_PAGE);
	FREE(CacheSpareBuf, 1024);

		for(i = 0; i < N_NAND_W_CACHE; i++)
			FREE(nand_w_cache[i].data,nand_w_cache[i].size);

	FREE(nand_r_cache.data,nand_r_cache.size);
	FREE(tmp_cache_buf,512 * SECTOR_CNT_OF_LOGIC_PAGE);
	return 0;
}
