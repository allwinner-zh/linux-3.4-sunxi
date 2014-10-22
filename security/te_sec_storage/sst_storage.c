/* secure storage driver for sunxi platform 
 *
 * Copyright (C) 2014 Allwinner Ltd. 
 *
 * Author:
 *	Ryan Chen <ryanchen@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/crc32.h>

#include "sst.h"
#include "sst_debug.h"


#define SEC_BLK_SIZE	(4096)
#define EMMC_SEC_STORE	"/data/oem_secure_store"

static char *oem_path="/dev/block/by-name/bootloader";
static char file[128];

/*
 *  secure storage map
 *
 *  section 0:
 *		name1:length1
 *		name2:length2
 *			...
 *	section 1:
 *		data1 ( name1 data )
 *	section 2 :
 *		data2 ( name2 data ) 
 *			... 
 */


static unsigned char secure_storage_map[SEC_BLK_SIZE] = {0};
static unsigned int  secure_storage_inited = 0;

#ifndef OEM_STORE_IN_FS

static struct secblc_op_t secblk_op ;
static int __oem_read(
		int id,
		char *buf, 
		ssize_t len
		)
{	
	struct file *fd;
	//ssize_t ret;
	int ret = -1;
	mm_segment_t old_fs ;
	char *filename = oem_path ;

	if(!filename || !buf){
		dprintk("- filename/buf NULL\n");
		return (-EINVAL);
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = filp_open(filename, O_RDONLY, 0);

	if(IS_ERR(fd)) {
		dprintk(" -file open fail\n");
		return -1;
	}
	do{
		if ((fd->f_op == NULL) || (fd->f_op->unlocked_ioctl == NULL))
		{
			dprintk(" -file can't to open!!\n");
			break;
		} 	
		secblk_op.item = id ;
		secblk_op.buf = buf;
		secblk_op.len = len ;
		
		dprintk("oem read parameter: cmd %x, arg %x", 
				SECBLK_READ, (unsigned int)(&secblk_op));
		ret = fd->f_op->unlocked_ioctl(
				fd,
				SECBLK_READ,
				(unsigned int)(&secblk_op));			

	}while(false);

	filp_close(fd, NULL);

	set_fs(old_fs);
	
	dprintk("__oem_read:%d\n ",ret);

	return ret ;
}
static int __oem_write(
		int	id, 
		char *buf, 
		ssize_t len
		)
{	
	struct file *fd;
	//ssize_t ret;
	int ret = -1;
	mm_segment_t old_fs = get_fs();
	char *filename = oem_path ;

	if(!filename || !buf){
		dprintk("- filename/buf NULL\n");
		return (-EINVAL);
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = filp_open(filename, O_WRONLY|O_CREAT, 0666);

	if(IS_ERR(fd)) {
		dprintk(" -file open fail\n");
		return -1;
	}
	do{
		if ((fd->f_op == NULL) || (fd->f_op->unlocked_ioctl == NULL))
		{
			dprintk(" -file can't to write!!\n");
			break;
		} 

		secblk_op.item = id ;
		secblk_op.buf = buf;
		secblk_op.len = len ;
		
		ret = fd->f_op->unlocked_ioctl(
				fd,
				SECBLK_WRITE,
				(unsigned int)(&secblk_op));			

	}while(false);

	vfs_fsync(fd, 0);

	filp_close(fd, NULL);

	set_fs(old_fs);
	dprintk("__oem_read:%d\n ",ret);
	return ret ;
}

#endif

static int sunxi_secstorage_read(int item, unsigned char *buf, unsigned int len)
{
#ifdef OEM_STORE_IN_FS 
	int ret ;
	struct secure_storage_t * sst ;

	sst =  sst_get_aw_storage();
	sst_memset(file,0, 128);
	sprintf(file, "%s/08%d.bin", EMMC_SEC_STORE, item);
	ret=(sst->st_operate->read((st_path_t)file, buf, len , USER_DATA) );
	return (ret != len);
#else
	return __oem_read(item, buf, len);
#endif
}

static int sunxi_secstorage_write(int item, unsigned char *buf, unsigned int len)
{
#ifdef OEM_STORE_IN_FS 
	int ret ;
	struct secure_storage_t * sst ;

	sst =  sst_get_aw_storage();
	sst_memset(file,0, 128);
	sprintf(file, "%s/08%d.bin", EMMC_SEC_STORE, item);
	ret= (sst->st_operate->write((st_path_t)file, buf, len , USER_DATA) );
	return (ret != len );

#else
	return __oem_write(item, buf, len);
#endif
}
/*
 * Map format:
 *		name:length\0
 *		name:length\0
 */
static int __probe_name_in_map(unsigned char *buffer, const char *item_name, int *len)
{
	unsigned char *buf_start = buffer;
	int   index = 1;
	char  name[64], length[32];
	int   i,j, name_len ;

	dprintk("__probe_name_in_map\n");

	while(*buf_start != '\0')
	{
		sst_memset(name, 0, 64);
		sst_memset(length, 0, 32);
		i=0;
		while(buf_start[i] != ':')
		{
			name[i] = buf_start[i];
			i ++;
		}
		name_len=i ;
		i ++;j=0;
		while( (buf_start[i] != ' ') && (buf_start[i] != '\0') )
		{
			length[j] = buf_start[i];
			i ++;j++;
		}

		if(memcmp(item_name, name,name_len ) ==0 )
		{
			buf_start += strlen(item_name) + 1;
			*len = simple_strtoul((const char *)length, NULL, 10);

			return index;
		}
		index ++;
		buf_start += strlen((const char *)buf_start) + 1;
	}

	return -1;
}

static int __fill_name_in_map(unsigned char *buffer, const char *item_name, int length)
{
	unsigned char *buf_start = buffer;
	int   index = 1;
	int   name_len;

	while(*buf_start != '\0')
	{
		dprintk("name in map %s\n", buf_start);

		name_len = 0;
		while(buf_start[name_len] != ':')
			name_len ++;
		if(!memcmp((const char *)buf_start, item_name, name_len))
		{
			return index;
		}
		index ++;
		buf_start += strlen((const char *)buf_start) + 1;
	}
	if(index >= 32)
		return -1;

	sprintf((char *)buf_start, "%s:%d", item_name, length);

	return index;
}

int sunxi_secure_storage_init(void)
{
	int ret;

	if(!secure_storage_inited)
	{
		ret = sunxi_secstorage_read(0, secure_storage_map, SEC_BLK_SIZE);
		if(ret < 0)
		{
			dprintk("get secure storage map err\n");

			return -1;
		}
		else if(ret > 0)
		{
			dprintk("the secure storage map is empty\n");
			sst_memset(secure_storage_map, 0, SEC_BLK_SIZE);
		}
	}

	secure_storage_inited = 1;

	return 0;
}

int sunxi_secure_storage_exit(void)
{
	int ret;

	if(!secure_storage_inited)
	{
		dprintk("%s err: secure storage has not been inited\n", __func__);

		return -1;
	}
	ret = sunxi_secstorage_write(0, secure_storage_map, SEC_BLK_SIZE);
	if(ret)
	{
		dprintk("write secure storage map\n");

		return -1;
	}

	return 0;
}

int sunxi_secure_storage_probe(const char *item_name)
{
	int ret;
	int len;

	if(!secure_storage_inited)
	{
		dprintk("%s err: secure storage has not been inited\n", __func__);

		return -1;
	}
	ret = __probe_name_in_map(secure_storage_map, item_name, &len);
	if(ret < 0)
	{
		dprintk("no item name %s in the map\n", item_name);

		return -1;
	}

	return ret;
}

int sunxi_secure_storage_read(const char *item_name, char *buffer, int buffer_len, int *data_len)
{
	int ret, index;
	int len_in_store;
	unsigned char * buffer_to_sec ;

	dprintk("secure storage read %s \n", item_name);
	if(!secure_storage_inited)
	{
		dprintk("%s err: secure storage has not been inited\n", __func__);
		return -1;
	}
	
	buffer_to_sec = (unsigned char *)kmalloc(SEC_BLK_SIZE,GFP_KERNEL);
	if(!buffer_to_sec ){
		dprintk("%s out of memory",__func__);
		return -1 ;
	}

	index = __probe_name_in_map(secure_storage_map, item_name, &len_in_store);
	if(index < 0)
	{
		dprintk("no item name %s in the map\n", item_name);
		kfree(buffer_to_sec);
		return -1;
	}
	sst_memset(buffer, 0, buffer_len);
	ret = sunxi_secstorage_read(index, buffer_to_sec, SEC_BLK_SIZE);
	if(ret)
	{
		dprintk("read secure storage block %d name %s err\n", index, item_name);
		kfree(buffer_to_sec);
		return -1;
	}
	if(len_in_store > buffer_len)
	{
		sst_memcpy(buffer, buffer_to_sec, buffer_len);
	}
	else
	{
		sst_memcpy(buffer, buffer_to_sec, len_in_store);
	}
	*data_len = len_in_store;

	dprintk("secure storage read %s done\n",item_name);

	kfree(buffer_to_sec);
	return 0;
}

/*Add new item to secure storage*/
int sunxi_secure_storage_write(const char *item_name, char *buffer, int length)
{
	int ret, index;

	if(!secure_storage_inited)
	{
		dprintk("%s err: secure storage has not been inited\n", __func__);

		return -1;
	}
	index = __fill_name_in_map(secure_storage_map, item_name, length);
	if(index < 0)
	{
		dprintk("write secure storage block %d name %s overrage\n", index, item_name);

		return -1;
	}

	ret = sunxi_secstorage_write(index, (unsigned char *)buffer, SEC_BLK_SIZE);
	if(ret)
	{
		dprintk("write secure storage block %d name %s err\n", index, item_name);

		return -1;
	}
	dprintk("write secure storage: %d ok\n", index);

	return 0;
}

/*Store source data to secure_object struct
 *
 * src		: payload data to secure_object
 * name		: input payloader data name
 * tagt		: taregt secure_object
 * len		: input payload data length
 * retLen	: target secure_object length
 * */
int wrap_secure_object(void * src, char *name,  unsigned int len, void * tagt,  unsigned int *retLen )
{
	store_object_t *obj;

	if(len >MAX_STORE_LEN){
		dprintk("Input length larger then secure object payload size\n");
		return -1 ;
	}

	obj = (store_object_t *) tagt ;
	*retLen= sizeof( store_object_t );

	obj->magic = STORE_OBJECT_MAGIC ;
	strncpy( obj->name, name, 64 );
	obj->re_encrypt = 0 ;
	obj->version = 0;
	obj->id = 0;
	sst_memset(obj->reserved, 0, sizeof(obj->reserved) );
	obj->actual_len = len ;
	sst_memcpy( obj->data, src, len);
	
	obj->crc = ~crc32_le(~0 , (const unsigned char *)obj, sizeof(*obj)-4 );

	return 0 ;
}

/*
 * Test data and function
 */ 
static unsigned char hdmi_data[]={
	0x44,0xe5,0xb5,0xac,0x2b,0x53,0xbc,0xb9,0xbf,0x89,0x67,0x96,0x1e,0xbb,0xbd,0xfb
};

static unsigned char widevine_data[] ={
	0xeb,0x8f,0x55,0x26,0x0d,0x7a,0xab,0xf3,0x58,0x3b,0xf9,0xc0,0x5e,0x12,0x79,0x85
};

static unsigned char sec_buf[SEC_BLK_SIZE];
static int secure_object_op_test(void)
{
	unsigned char * tagt ;
	unsigned int LEN = SEC_BLK_SIZE , retLen ;
	int ret ;
	sunxi_secure_storage_init();
	tagt = (unsigned char *)kzalloc(LEN,GFP_KERNEL);
	if(!tagt){
		dprintk("out of memory\n");
		return -1 ;
	}

	sst_memset(tagt,0, LEN);
	ret = wrap_secure_object( hdmi_data, "HDMI",  
							sizeof(hdmi_data), tagt, &retLen ) ;
	if( ret <0){
		dprintk("Error: wrap secure object fail\n");
		kfree(tagt);
		return -1 ;
	}

	ret = sunxi_secure_storage_write( "HDMI" , tagt, retLen )	;
	if(ret <0){
		dprintk("Error: store HDMI object fail\n");
		kfree(tagt);
		return -1 ;
	}


	ret = sunxi_secure_storage_read( "HDMI" , sec_buf, SEC_BLK_SIZE , &retLen )	;
	if(ret <0){
		dprintk("Error: store HDMI object read fail\n");
		kfree(tagt);
		return -1 ;
	}
	
	if( memcmp(tagt, sec_buf, retLen ) !=0 ){
		dprintk("Error: HDMI write/read fail\n");
		return -1 ;
	}
	

	sst_memset(tagt,0, LEN);
	ret = wrap_secure_object(widevine_data, "Widevine",  
							sizeof(widevine_data), tagt, &retLen ) ;
	if( ret <0){
		dprintk("Error: wrap secure object fail\n");
		kfree(tagt);
		return -1 ;
	}

	ret = sunxi_secure_storage_write( "Widevine" , tagt, retLen )	;
	if(ret <0){
		dprintk("Error: store Widevine object fail\n");
		kfree(tagt);
		return -1 ;
	}

	ret = sunxi_secure_storage_read( "Widevine" , sec_buf, SEC_BLK_SIZE , &retLen )	;
	if(ret <0){
		dprintk("Error: store Widevine object read fail\n");
		kfree(tagt);
		return -1 ;
	}
	
	if( memcmp(tagt, sec_buf, retLen ) !=0 ){
		dprintk("Error: Widevine write/read fail\n");
		kfree(tagt);
		return -1 ;
	}

	sunxi_secure_storage_exit();
		
	kfree(tagt);
	return 0 ;
}
static unsigned char buffer[SEC_BLK_SIZE];
static int clear_secure_store(int index)
{
	sst_memset( buffer, 0, SEC_BLK_SIZE);

	if(index == 0xffff){
		int i =0 ;
		dprintk("clean whole secure store");
		for( ; i<32 ;i++){
			dprintk("..");
			sunxi_secstorage_write(i, (unsigned char *)buffer, SEC_BLK_SIZE);
			dprintk("clearn %d done\n",i);
		}

	}else{
		dprintk("clean secure store %d\n", index);
		dprintk("..");
		sunxi_secstorage_write(index, (unsigned char *)buffer, SEC_BLK_SIZE);
		dprintk("clearn %d done\n",index);
	}
	return 0 ;
}

int secure_store_in_fs_test(void)
{
	clear_secure_store(0);
	clear_secure_store(1);
	clear_secure_store(2);
	secure_object_op_test();
	return 0 ;
}


