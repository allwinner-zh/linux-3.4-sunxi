/*
 * Copyright (C) 2013 Allwinnertech, kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/blkpg.h>
#include <linux/spinlock.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/semaphore.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <mach/clock.h>
#include <mach/dma.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <linux/pm.h>
#include "nand_blk.h"
#include "../nandtest/nand_test.h"
#include <mach/sys_config.h>


extern __u32 nand_current_dev_num;
extern int part_secur[MAX_PART_COUNT];
extern __u32 RetryCount[8];

struct nand_disk disk_array[MAX_PART_COUNT];

#define BLK_ERR_MSG_ON
#ifdef  BLK_ERR_MSG_ON
#define dbg_err(fmt, args...) printk("[NAND]"fmt, ## args)
#else
#define dbg_err(fmt, ...)  ({})
#endif


//#define BLK_INFO_MSG_ON
#ifdef  BLK_INFO_MSG_ON
#define dbg_inf(fmt, args...) printk("[NAND]"fmt, ## args)
#else
#define dbg_inf(fmt, ...)  ({})
#endif


#define REMAIN_SPACE 0
#define PART_FREE 0x55
#define PART_DUMMY 0xff
#define PART_READONLY 0x85
#define PART_WRITEONLY 0x86
#define PART_NO_ACCESS 0x87

#define TIMEOUT 				300			// ms

#define NAND_CACHE_FLUSH_EVERY_SEC
#define NAND_CACHE_RW
//#define NAND_LOG_AUTO_MERGE
#define NAND_LOG_RELEASE_LEVEL   4



/**
*USE_BIO_MERGE level description:
*1	:	merge bvc in one bio
*2	:	merge bvc in one bio and merge bios in one request
*/
#define USE_BIO_MERGE			2
#define NAND_TEST_TICK			0

#ifdef NAND_CACHE_FLUSH_EVERY_SEC
static int after_rw = 0;

struct collect_ops{
		unsigned long timeout;
		wait_queue_head_t wait;
		struct completion thread_exit;
		unsigned char quit;
};
struct collect_ops collect_arg;

#endif

//for CLK
extern int NAND_ClkEnable(void);
extern void NAND_ClkDisable(void);

extern int NAND_ClkRequest(__u32 nand_index);
extern void NAND_ClkRelease(__u32 nand_index);
extern int NAND_SetClk(__u32 nand_index, __u32 nand_clk);
extern int NAND_GetClk(__u32 nand_index);

//for DMA
extern int NAND_RequestDMA(void);
extern int NAND_ReleaseDMA(void);
extern void NAND_DMAConfigStart(int rw, unsigned int buff_addr, int len);
extern int NAND_QueryDmaStat(void);
extern int NAND_WaitDmaFinish(void);
//for PIO
extern void NAND_PIORequest(void);
extern void NAND_PIORelease(void);

//for Int
extern void NAND_EnRbInt(void);
extern void NAND_ClearRbInt(void);
extern int NAND_WaitRbReady(void);
extern void NAND_EnDMAInt(void);
extern void NAND_ClearDMAInt(void);
extern void NAND_DMAInterrupt(void);

extern void NAND_RbInterrupt(void);

extern __u32 NAND_GetIOBaseAddr(void);




DEFINE_SEMAPHORE(nand_mutex);
static unsigned char volatile IS_IDLE = 1;
static int nand_flush(struct nand_blk_dev *dev);
static int nand_logrelease(struct nand_blk_dev *dev);

static int nand_flush_force(__u32 dev_num);
static struct nand_state nand_reg_state;

#ifdef __LINUX_NAND_SUPPORT_INT__
spinlock_t     nand_rb_lock;

static irqreturn_t nand_rb_interrupt(int irq, void *dev_id)
{
    unsigned long iflags;

    spin_lock_irqsave(&nand_rb_lock, iflags);
    NAND_RbInterrupt();
    spin_unlock_irqrestore(&nand_rb_lock, iflags);

	return IRQ_HANDLED;
}
#endif

#if USE_BIO_MERGE==0
static int cache_align_page_request(struct nand_blk_ops * nandr, struct nand_blk_dev * dev, struct request * req)
{
	unsigned long start,nsector;
	char *buf;
	__s32 ret;

	int cmd = rq_data_dir(req);

	if(dev->disable_access || ( (cmd == WRITE) && (dev->readonly) ) \
		|| ((cmd == READ) && (dev->writeonly))){
		dbg_err("can not access this part\n");

		return -EIO;
	}


	//for2.6.36
	buf = req->buffer;
	start = blk_rq_pos(req);
	nsector = blk_rq_cur_bytes(req)>>9;

	if ( (start + nsector) > get_capacity(req->rq_disk))
	{
		dbg_err("over the limit of disk\n");

		return -EIO;
	}
	start += dev->off_size;

	switch(cmd) {

	case READ:

		dbg_inf("READ:%lu from %lu\n",nsector,start);

		#ifndef NAND_CACHE_RW
			LML_FlushPageCache();
  		ret = LML_Read(start, nsector, buf);
		#else
			//printk("Rs %lu %lu \n",start, nsector);
      LML_FlushPageCache();
			ret = NAND_CacheRead(start, nsector, buf);
			//printk("Rs %lu %lu \n",start, nsector);
		#endif
		if (ret)
		{
			dbg_err("cache_align_page_request:read err\n");
			return -EIO;

		}
		return 0;


	case WRITE:

		dbg_inf("WRITE:%lu from %lu\n",nsector,start);
		#ifndef NAND_CACHE_RW
			ret = LML_Write(start, nsector, buf);
		#else
			//printk("Ws %lu %lu \n",start, nsector);
			ret = NAND_CacheWrite(start, nsector, buf);
			//printk("We %lu %lu \n",start, nsector);
		#endif
		if (ret)
		{
			dbg_err("cache_align_page_request:write err\n");
			return -EIO;
		}
		return 0;

	default:
		dbg_err("Unknown request \n");
		return -EIO;
	}

}
#endif

#if USE_BIO_MERGE
#define nand_bio_kmap(bio,idx,kmtype)	\
	(page_address(bio_iovec_idx((bio), (idx))->bv_page) +	bio_iovec_idx((bio), (idx))->bv_offset)

static int nand_transfer(struct nand_blk_dev * dev, unsigned long start,unsigned long nsector, char *buf, int cmd)
{
	__s32 ret;

	if(dev->disable_access || ( (cmd == WRITE) && (dev->readonly) ) \
		|| ((cmd == READ) && (dev->writeonly))){
		dbg_err("can not access this part\n");
		return -EIO;
	}
	//printk("[N]start=%lx,nsec=%lx,buffer=%p,cmd=%d\n",start,nsector,buf,cmd);
	start += dev->off_size;

	switch(cmd) {

	case READ:
        //printk("R %lu %lu 0x%x \n",start, nsector, (__u32)buf);
		dbg_inf("READ:%lu from %lu\n",nsector,start);

		#ifndef NAND_CACHE_RW
			LML_FlushPageCache();
  			ret = LML_Read(start, nsector, buf);
		#else
			//printk("Rs %lu %lu \n",start, nsector);
      		LML_FlushPageCache();
			ret = NAND_CacheRead(start, nsector, buf);
			//printk("Rs %lu %lu \n",start, nsector);
		#endif
		if (ret)
		{
			dbg_err("cache_align_page_request:read err\n");
			return -EIO;

		}
		return 0;


	case WRITE:
        //printk("W %lu %lu 0x%x \n",start, nsector, (__u32)buf);
		dbg_inf("WRITE:%lu from %lu\n",nsector,start);
		#ifndef NAND_CACHE_RW
			ret = LML_Write(start, nsector, buf);
		#else
			//printk("Ws %lu %lu \n",start, nsector);
			ret = NAND_CacheWrite(start, nsector, buf);
			//printk("We %lu %lu \n",start, nsector);
		#endif
		if (ret)
		{
			dbg_err("cache_align_page_request:write err\n");
			return -EIO;
		}
		return 0;

	default:
		dbg_err("Unknown request \n");
		return -EIO;
	}

}
#endif
#if NAND_TEST_TICK
static unsigned long nand_rw_time = 0;
#endif
static int nand_blktrans_thread(void *arg)
{
	struct nand_blk_ops *nandr = arg;
	struct request_queue *rq = nandr->rq;
	struct request *req = NULL;
#if NAND_TEST_TICK
	unsigned long tick=0;
#endif
#if USE_BIO_MERGE
	struct req_iterator rq_iter;
	struct bio_vec *bvec;
	unsigned long long sector = ULLONG_MAX;
	unsigned long rq_len = 0;
	char *buffer=NULL;
	int rw_flag = 0;
#endif

	/* we might get involved when memory gets low, so use PF_MEMALLOC */
	current->flags |= PF_MEMALLOC | PF_NOFREEZE;
	daemonize("%sd", nandr->name);

	/* daemonize() doesn't do this for us since some kernel threads
	   actually want to deal with signals. We can't just call
	   exit_sighand() since that'll cause an oops when we finally
	   do exit. */
	spin_lock_irq(&current->sighand->siglock);
	sigfillset(&current->blocked);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	spin_lock_irq(rq->queue_lock);

	while (!nandr->quit) {

		struct nand_blk_dev *dev;
	#if USE_BIO_MERGE == 0
		int res = 0;
	#endif
		DECLARE_WAITQUEUE(wait, current);

		if (!req && !(req = blk_fetch_request(rq))) {
			//printk("[N]wait req\n");
			add_wait_queue(&nandr->thread_wq, &wait);
			set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_irq(rq->queue_lock);
			schedule();
			spin_lock_irq(rq->queue_lock);
			remove_wait_queue(&nandr->thread_wq, &wait);
			continue;
		}

		dev = req->rq_disk->private_data;
	#if USE_BIO_MERGE==1
		//spin_unlock_irq(rq->queue_lock);
		IS_IDLE = 0;
		//printk("[N]request start\n");
		__rq_for_each_bio(rq_iter.bio,req){
			if(!bio_segments(rq_iter.bio)){
				continue;
			}
			//printk("[N]start bio,count=%d\n",(rq_iter.bio)->bi_vcnt);
			sector = (rq_iter.bio)->bi_sector;
			buffer = nand_bio_kmap(rq_iter.bio, (rq_iter.bio)->bi_idx, KM_USER0);
			bio_for_each_segment(bvec, rq_iter.bio, rq_iter.i){
				if(rq_iter.i<(rq_iter.bio)->bi_vcnt-1){
					if(nand_bio_kmap(rq_iter.bio, rq_iter.i+1, KM_USER0) == nand_bio_kmap(rq_iter.bio, rq_iter.i, KM_USER0)+ bvec->bv_len){
						rq_len += bvec->bv_len;
						//printk("[N]go on\n");
					}else{
						rq_len += bvec->bv_len;
						spin_unlock_irq(rq->queue_lock);
						down(&nandr->nand_ops_mutex);
						#if NAND_TEST_TICK
						tick = jiffies;
						nand_transfer(dev, sector, rq_len>>9, buffer, bio_data_dir(rq_iter.bio));
						nand_rw_time += jiffies - tick;
						#else
						nand_transfer(dev, sector, rq_len>>9, buffer, bio_data_dir(rq_iter.bio));
						#endif
						up(&nandr->nand_ops_mutex);
						spin_lock_irq(rq->queue_lock);
						sector += rq_len>>9;
						rq_len = 0;
						buffer = nand_bio_kmap(rq_iter.bio, rq_iter.i+1, KM_USER0);
					}
				}else{
					rq_len += bvec->bv_len;
					spin_unlock_irq(rq->queue_lock);
					down(&nandr->nand_ops_mutex);
					#if NAND_TEST_TICK
					tick = jiffies;
					nand_transfer(dev, sector,  rq_len>>9, buffer, bio_data_dir(rq_iter.bio));
					nand_rw_time += jiffies - tick;
					#else
					nand_transfer(dev, sector,  rq_len>>9, buffer, bio_data_dir(rq_iter.bio));
					#endif
					up(&nandr->nand_ops_mutex);
					spin_lock_irq(rq->queue_lock);
					rq_len=0;
				}
			}
			//printk("[N]end bio...\n");
		}
		//printk("[N]request finished\n");
		#if NAND_TEST_TICK
		printk("[N]ticks=%ld\n",nand_rw_time);
		#endif

		//printk("req flags=%x\n",req->cmd_flags);
		#ifdef NAND_CACHE_FLUSH_EVERY_SEC
		if(req->cmd_flags&REQ_WRITE)
			after_rw = 1;
		if(req->cmd_flags&REQ_SYNC){
			wake_up_interruptible(&collect_arg.wait);
		}
		#endif

		//spin_lock_irq(rq->queue_lock);
		__blk_end_request_all(req,0);
		req = NULL;
	#elif USE_BIO_MERGE==2
		//IS_IDLE = 0;

		rw_flag = req->cmd_flags&REQ_WRITE;

		__rq_for_each_bio(rq_iter.bio,req){
			if(!bio_segments(rq_iter.bio)){
				continue;
			}
			if(unlikely(sector == ULLONG_MAX)){
				/*new bio, no data exists*/
				sector = (rq_iter.bio)->bi_sector;
			}else{
				/*last bio data exists*/
				if((rq_iter.bio)->bi_sector == (sector + (rq_len>>9))){
					//printk("[N]bio merge\n");
				}else{
					/*flush last bio data here*/
					spin_unlock_irq(rq->queue_lock);
					down(&nandr->nand_ops_mutex);
				#if NAND_TEST_TICK
					tick = jiffies;
					nand_current_dev_num = dev->devnum;
					nand_transfer(dev, sector,  rq_len>>9, buffer, rw_flag);
					nand_rw_time += jiffies - tick;
				#else
					nand_current_dev_num = dev->devnum;
					nand_transfer(dev, sector, rq_len>>9, buffer, rw_flag);
				#endif
					up(&nandr->nand_ops_mutex);
					spin_lock_irq(rq->queue_lock);
					/*update new bio*/
					sector = (rq_iter.bio)->bi_sector;
					buffer = 0;
					rq_len = 0;
				}
			}

			bio_for_each_segment(bvec, rq_iter.bio, rq_iter.i){
				if(1){
					if(nand_bio_kmap(rq_iter.bio, rq_iter.i, KM_USER0) == buffer + rq_len){
						/*merge vec*/
						rq_len += bvec->bv_len;
						//printk("[N]merge bvc\n");
					}else{
						/*flush previous data*/
						if(rq_len){
							spin_unlock_irq(rq->queue_lock);
							down(&nandr->nand_ops_mutex);
						#if NAND_TEST_TICK
							tick = jiffies;
							nand_current_dev_num = dev->devnum;
							nand_transfer(dev, sector,  rq_len>>9, buffer, rw_flag);
							nand_rw_time += jiffies - tick;
						#else
							nand_current_dev_num = dev->devnum;
							nand_transfer(dev, sector, rq_len>>9, buffer, rw_flag);
						#endif
							up(&nandr->nand_ops_mutex);
							spin_lock_irq(rq->queue_lock);
						}
						/*update new*/
						sector += rq_len>>9;
						buffer = nand_bio_kmap(rq_iter.bio, rq_iter.i, KM_USER0);
						rq_len = bvec->bv_len;
					}
				}
			}
		}

		if(rq_len){
			spin_unlock_irq(rq->queue_lock);
			down(&nandr->nand_ops_mutex);
		#if NAND_TEST_TICK
			tick = jiffies;
			nand_current_dev_num = dev->devnum;
			nand_transfer(dev, sector,  rq_len>>9, buffer, rw_flag);
			nand_rw_time += jiffies - tick;
		#else
			nand_current_dev_num = dev->devnum;
			nand_transfer(dev, sector, rq_len>>9, buffer, rw_flag);
		#endif
			up(&nandr->nand_ops_mutex);
			spin_lock_irq(rq->queue_lock);
			sector = ULLONG_MAX;
			rq_len = 0;
			buffer = NULL;
		}

		#if NAND_TEST_TICK
		printk("[N]ticks=%ld\n",nand_rw_time);
		#endif


		if((req->cmd_flags&REQ_SYNC)&&(req->cmd_flags&REQ_WRITE)&&(part_secur[dev->devnum]== 1)){
		    //printk("req sync: 0x%x form part: 0x%x \n", req->cmd_flags, dev->devnum);
		    spin_unlock_irq(rq->queue_lock);
			down(&nandr->nand_ops_mutex);
			nand_current_dev_num = dev->devnum;
		    nand_flush_force(nand_current_dev_num);
		    up(&nandr->nand_ops_mutex);
			spin_lock_irq(rq->queue_lock);
		}

		#ifdef NAND_CACHE_FLUSH_EVERY_SEC
		if(rw_flag == REQ_WRITE)
			after_rw = 1;
		if((req->cmd_flags&REQ_SYNC)&&(req->cmd_flags&REQ_WRITE))
			wake_up_interruptible(&collect_arg.wait);

		#endif

		//spin_lock_irq(rq->queue_lock);
		__blk_end_request_all(req,0);
		req = NULL;
	#else
		nandr = dev->nandr;
		spin_unlock_irq(rq->queue_lock);
		down(&nandr->nand_ops_mutex);
		IS_IDLE = 0;

		#ifdef NAND_BIO_ALIGN
			reset(req);
		#endif
		#if NAND_TEST_TICK
		tick = jiffies;
		res = cache_align_page_request(nandr, dev, req);
		nand_rw_time += jiffies - tick;
		#else
		res = cache_align_page_request(nandr, dev, req);
		#endif
		up(&nandr->nand_ops_mutex);
		IS_IDLE = 1;
		spin_lock_irq(rq->queue_lock);

		if(!__blk_end_request_cur(req, res)){
			req = NULL;
			#if NAND_TEST_TICK
			printk("[N]ticks=%ld\n",nand_rw_time);
			#endif
		}
	#endif

	}

	if(req)
		__blk_end_request_all(req, -EIO);
	spin_unlock_irq(rq->queue_lock);

	complete_and_exit(&nandr->thread_exit, 0);

	return 0;
}


static void nand_blk_request(struct request_queue *rq)
{
	struct nand_blk_ops *nandr = rq->queuedata;
	wake_up(&nandr->thread_wq);
}

static int nand_open(struct block_device *bdev, fmode_t mode)
{
	struct nand_blk_dev *dev;
	struct nand_blk_ops *nandr;
	int ret = -ENODEV;

	dev = bdev->bd_disk->private_data;
	nandr = dev->nandr;

	if (!try_module_get(nandr->owner))
		goto out;

	ret = 0;
	if (nandr->open && (ret = nandr->open(dev))) {
		out:
		module_put(nandr->owner);
	}
	return ret;
}
static int nand_release(struct gendisk *disk, fmode_t mode)
{
	struct nand_blk_dev *dev;
	struct nand_blk_ops *nandr;

	int ret = 0;

	dev = disk->private_data;
	nandr = dev->nandr;
	//nand_flush(NULL);
	if (nandr->release)
		ret = nandr->release(dev);

	if (!ret) {
		module_put(nandr->owner);
	}

	return ret;
}


/*filp->f_dentry->d_inode->i_bdev->bd_disk->fops->ioctl(filp->f_dentry->d_inode, filp, cmd, arg);*/
#define DISABLE_WRITE         _IO('V',0)
#define ENABLE_WRITE          _IO('V',1)
#define DISABLE_READ 	     _IO('V',2)
#define ENABLE_READ 	     _IO('V',3)
static int nand_ioctl(struct block_device *bdev, fmode_t mode, unsigned int cmd, unsigned long arg)
{
	struct nand_blk_dev *dev = bdev->bd_disk->private_data;
	struct nand_blk_ops *nandr = dev->nandr;

	switch (cmd) {
	case BLKFLSBUF:
		dbg_err("BLKFLSBUF called!\n");
		if (nandr->flush)
			return nandr->flush(dev);
		/* The core code did the work, we had nothing to do. */
		return 0;

	case HDIO_GETGEO:
		if (nandr->getgeo) {
			struct hd_geometry g;
			int ret;

			memset(&g, 0, sizeof(g));
			ret = nandr->getgeo(dev, &g);
			if (ret)
				return ret;
  			dbg_err("HDIO_GETGEO called!\n");
			g.start = get_start_sect(bdev);
			if (copy_to_user((void __user *)arg, &g, sizeof(g)))
				return -EFAULT;

			return 0;
		}
		return 0;
	case ENABLE_WRITE:
		dbg_err("enable write!\n");
		dev->disable_access = 0;
		dev->readonly = 0;
		set_disk_ro(dev->blkcore_priv, 0);
		return 0;

	case DISABLE_WRITE:
		dbg_err("disable write!\n");
		dev->readonly = 1;
		set_disk_ro(dev->blkcore_priv, 1);
		return 0;

	case ENABLE_READ:
		dbg_err("enable read!\n");
		dev->disable_access = 0;
		dev->writeonly = 0;
		return 0;

	case DISABLE_READ:
		dbg_err("disable read!\n");
		dev->writeonly = 1;
		return 0;
	default:
		return -ENOTTY;
	}
}

struct block_device_operations nand_blktrans_ops = {
	.owner		= THIS_MODULE,
	.open		= nand_open,
	.release	= nand_release,
	.ioctl		= nand_ioctl,
};

void set_part_mod(char *name,int cmd)
{
	struct file *filp = NULL;
	filp = filp_open(name, O_RDWR, 0);
	filp->f_dentry->d_inode->i_bdev->bd_disk->fops->ioctl(filp->f_dentry->d_inode->i_bdev, 0, cmd, 0);
	filp_close(filp, current->files);
}
static int nand_add_dev(struct nand_blk_ops *nandr, struct nand_disk *part)
{
	struct nand_blk_dev *dev;
	struct list_head *this;
	struct gendisk *gd;
	unsigned long temp;

	int last_devnum = -1;

	dev = kmalloc(sizeof(struct nand_blk_dev), GFP_KERNEL);
	if (!dev) {
		dbg_err("dev: out of memory for data structures\n");
		return -1;
	}
	memset(dev, 0, sizeof(*dev));
	dev->nandr = nandr;
	dev->size = part->size;
	dev->off_size = part->offset;
	dev->devnum = -1;

	dev->cylinders = 1024;
	dev->heads = 16;

	temp = dev->cylinders * dev->heads;
	dev->sectors = ( dev->size) / temp;
	if ((dev->size) % temp) {
		dev->sectors++;
		temp = dev->cylinders * dev->sectors;
		dev->heads = (dev->size)  / temp;

		if ((dev->size)   % temp) {
			dev->heads++;
			temp = dev->heads * dev->sectors;
			dev->cylinders = (dev->size)  / temp;
		}
	}

	if (!down_trylock(&nand_mutex)) {
		up(&nand_mutex);
		BUG();
	}

	list_for_each(this, &nandr->devs) {
		struct nand_blk_dev *tmpdev = list_entry(this, struct nand_blk_dev, list);
		if (dev->devnum == -1) {
			/* Use first free number */
			if (tmpdev->devnum != last_devnum+1) {
				/* Found a free devnum. Plug it in here */
				dev->devnum = last_devnum+1;
				list_add_tail(&dev->list, &tmpdev->list);
				goto added;
			}
		} else if (tmpdev->devnum == dev->devnum) {
			/* Required number taken */
			return -EBUSY;
		} else if (tmpdev->devnum > dev->devnum) {
			/* Required number was free */
			list_add_tail(&dev->list, &tmpdev->list);
			goto added;
		}
		last_devnum = tmpdev->devnum;
	}
	if (dev->devnum == -1)
		dev->devnum = last_devnum+1;

	if ((dev->devnum <<nandr->minorbits) > 256) {
		return -EBUSY;
	}

	//init_MUTEX(&dev->sem);
	list_add_tail(&dev->list, &nandr->devs);

 added:

	gd = alloc_disk(1 << nandr->minorbits);
	if (!gd) {
		list_del(&dev->list);
		return -ENOMEM;
	}
	gd->major = nandr->major;
	gd->first_minor = (dev->devnum) << nandr->minorbits;
	gd->fops = &nand_blktrans_ops;

	snprintf(gd->disk_name, sizeof(gd->disk_name),
		 "%s%c", nandr->name, (nandr->minorbits?'a':'0') + dev->devnum);
	//snprintf(gd->devfs_name, sizeof(gd->devfs_name),
	//	 "%s/%c", nandr->name, (nandr->minorbits?'a':'0') + dev->devnum);


	/* 2.5 has capacity in units of 512 bytes while still
	   having BLOCK_SIZE_BITS set to 10. Just to keep us amused. */
	set_capacity(gd, dev->size);

	gd->private_data = dev;
	dev->blkcore_priv = gd;
	gd->queue = nandr->rq;

	/*set rw partition*/
	if(part->type == PART_NO_ACCESS)
		dev->disable_access = 1;

	if(part->type == PART_READONLY)
		dev->readonly = 1;

	if(part->type == PART_WRITEONLY)
		dev->writeonly = 1;

	if (dev->readonly)
		set_disk_ro(gd, 1);
	add_disk(gd);
	return 0;
}

static int nand_remove_dev(struct nand_blk_dev *dev)
{
	struct gendisk *gd;
	gd = dev->blkcore_priv;

	if (!down_trylock(&nand_mutex)) {
		up(&nand_mutex);
		BUG();
	}
	list_del(&dev->list);
	gd->queue = NULL;
	del_gendisk(gd);
	put_disk(gd);
	return 0;
}

#ifdef NAND_CACHE_FLUSH_EVERY_SEC
static int collect_thread(void *tmparg)
{
	unsigned long ret;
	struct collect_ops *arg = tmparg;
	int log_release_flag;

	current->flags |= PF_MEMALLOC | PF_NOFREEZE;
	daemonize("%sd", "nfmt");

	spin_lock_irq(&current->sighand->siglock);
	sigfillset(&current->blocked);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);
#if 0
	while (!arg->quit)
	{
		ret = wait_event_interruptible_timeout(arg->wait, 0, arg->timeout*HZ);
		if (0 ==  ret)
		{
			nand_flush(NULL);
			IS_IDLE = 1;
		}
		arg->timeout = TIMEOUT;
	}
#else
	while (!arg->quit)
	{
		ret = wait_event_interruptible(arg->wait,after_rw);
		if(ret==0)
		{
			do{
				after_rw = 0;
				msleep(arg->timeout);
			}while(after_rw);
			//IS_IDLE = 1;
			nand_flush(NULL);
			//IS_IDLE = 1;

			while(!after_rw)
			{
				log_release_flag = nand_logrelease(NULL);
				if(log_release_flag<=0)
					break;

				msleep(100);
			}
		}
	}
#endif
	complete_and_exit(&arg->thread_exit, 0);
}
#endif

int nand_blk_register(struct nand_blk_ops *nandr)
{
	int i,ret;
	__u32 part_cnt;

	down(&nand_mutex);

	ret = register_blkdev(nandr->major, nandr->name);
	if(ret){
		dbg_err("\nfaild to register blk device\n");
		up(&nand_mutex);
		return -1;
	}


	spin_lock_init(&nandr->queue_lock);
	init_completion(&nandr->thread_exit);
	init_waitqueue_head(&nandr->thread_wq);
	sema_init(&nandr->nand_ops_mutex, 1);

	nandr->rq= blk_init_queue(nand_blk_request, &nandr->queue_lock);
	if (!nandr->rq) {
		unregister_blkdev(nandr->major, nandr->name);
		up(&nand_mutex);
		return  -1;
	}

	ret = elevator_change(nandr->rq, "noop");
	if(ret){
		blk_cleanup_queue(nandr->rq);
		return ret;
	}

	nandr->rq->queuedata = nandr;
	ret = kernel_thread(nand_blktrans_thread, nandr, CLONE_KERNEL);
	if (ret < 0) {
		blk_cleanup_queue(nandr->rq);
		unregister_blkdev(nandr->major, nandr->name);
		up(&nand_mutex);
		return ret;
	}

	#ifdef NAND_CACHE_FLUSH_EVERY_SEC
	/*init wait queue*/
	collect_arg.quit = 0;
	collect_arg.timeout = TIMEOUT;
	init_completion(&collect_arg.thread_exit);
	init_waitqueue_head(&collect_arg.wait);
	ret = kernel_thread(collect_thread, &collect_arg, CLONE_KERNEL);
 	if (ret < 0)
	{
		dbg_err("sorry,thread creat failed\n");
		return 0;
	}
	#endif

	//devfs_mk_dir(nandr->name);
	INIT_LIST_HEAD(&nandr->devs);

	part_cnt = mbr2disks(disk_array);
	for(i = 0 ; i < part_cnt ; i++){
		nandr->add_dev(nandr,&(disk_array[i]));
	}

	up(&nand_mutex);

	return 0;
}


void nand_blk_unregister(struct nand_blk_ops *nandr)
{
	struct list_head *this, *next;
	down(&nand_mutex);
	/* Clean up the kernel thread */
	nandr->quit = 1;
	wake_up(&nandr->thread_wq);
	wait_for_completion(&nandr->thread_exit);

#ifdef NAND_CACHE_FLUSH_EVERY_SEC
	collect_arg.quit =1;
	wake_up(&collect_arg.wait);
	wait_for_completion(&collect_arg.thread_exit);
#endif
	/* Remove it from the list of active majors */


	list_for_each_safe(this, next, &nandr->devs) {
		struct nand_blk_dev *dev = list_entry(this, struct nand_blk_dev, list);
		nandr->remove_dev(dev);
	}

	//devfs_remove(nandr->name);
	blk_cleanup_queue(nandr->rq);

	unregister_blkdev(nandr->major, nandr->name);

	up(&nand_mutex);

	if (!list_empty(&nandr->devs))
		BUG();
}




static int nand_getgeo(struct nand_blk_dev *dev,  struct hd_geometry *geo)
{
	geo->heads = dev->heads;
	geo->sectors = dev->sectors;
	geo->cylinders = dev->cylinders;

	return 0;
}

static struct nand_blk_ops mytr = {
	.name 			=  "nand",
	.major 			= 93,
	.minorbits 		= 3,
	.getgeo 			= nand_getgeo,
	.add_dev			= nand_add_dev,
	.remove_dev 		= nand_remove_dev,
	.flush 			= nand_flush,
	.owner 			= THIS_MODULE,
};


static int nand_flush(struct nand_blk_dev *dev)
{
	if (0 == down_trylock(&mytr.nand_ops_mutex))
	{
		IS_IDLE = 0;
	#ifdef NAND_CACHE_RW
		NAND_CacheFlush();
	#else
		LML_FlushPageCache();
	#endif
		up(&mytr.nand_ops_mutex);
		IS_IDLE = 1;

		dbg_inf("nand_flush \n");
	}

	return 0;
}

static int nand_logrelease(struct nand_blk_dev *dev)
{
    __s32 log_cnt =-1;

    #ifdef NAND_LOG_AUTO_MERGE
	if (0 == down_trylock(&mytr.nand_ops_mutex))
	{
		IS_IDLE = 0;
		log_cnt = BMM_RleaseLogBlock(NAND_LOG_RELEASE_LEVEL);
		up(&mytr.nand_ops_mutex);
		IS_IDLE = 1;
		//PRINT("nand_log_release: %x \n", log_cnt);
		if(log_cnt == NAND_LOG_RELEASE_LEVEL)
			return 0;
		else
			return log_cnt;
	}
	#endif

	return -1;
}

static int nand_flush_force(__u32 dev_num)
{
    //printk("nf \n");
	#ifdef NAND_CACHE_RW
		NAND_CacheFlushDev(dev_num);
	#else
		LML_FlushPageCache();
	#endif
	//printk("e\n");

	return 0;
}



static void nand_flush_all(void)
{
    int     timeout = 0;

    /* wait write finish */
    for(timeout=0; timeout<10; timeout++) {
        if(after_rw) {
            msleep(500);
        }
        else {
            break;
        }
    }
    printk("nand try to shutdown %d time\n", timeout);

    /* get nand ops mutex */
    down(&mytr.nand_ops_mutex);

    #ifdef NAND_CACHE_RW
    NAND_CacheFlush();
    #else
    LML_FlushPageCache();
    #endif
    BMM_WriteBackAllMapTbl();
    printk("Nand flash shutdown ok!\n");
}


int cal_partoff_within_disk(char *name,struct inode *i)
{
	struct gendisk *gd = i->i_bdev->bd_disk;
	int current_minor = MINOR(i->i_bdev->bd_dev)  ;
	int index = current_minor & ((1<<mytr.minorbits) - 1) ;
	if(!index)
		return 0;
	return ( gd->part_tbl->part[ index - 1]->start_sect);
}

#define SW_INT_IRQNO_NAND AW_IRQ_NAND

#ifndef CONFIG_SUN7I_NANDFLASH_TEST
static int  init_blklayer(void)
{
	int ret;
	unsigned long irqflags;
	script_item_value_type_e script_ret;
	script_item_u nand_good_block_ratio;

	ClearNandStruct();

	ret = PHY_Init();
	if (ret) {
		PHY_Exit();
		return -1;
	}

	ret = SCN_AnalyzeNandSystem();
	if (ret < 0)
		return ret;

	//printk("[NAND] nand driver version: 0x%x 0x%x \n", NAND_VERSION_0,NAND_VERSION_1);
#ifdef __LINUX_NAND_SUPPORT_INT__
    NAND_ClearRbInt();
    spin_lock_init(&nand_rb_lock);
	irqflags = IRQF_DISABLED;

	ret=request_irq(SW_INT_IRQNO_NAND, nand_rb_interrupt, irqflags, mytr.name, &mytr);
    if(ret)
	{
	    printk("nand interrupt register error\n");
	    return -EAGAIN;
	}
	else
	{
	    printk("nand interrupt register ok\n");
	}
#endif
	//modify ValidBlkRatio
	script_ret = script_get_item("nand_para","good_block_ratio", &nand_good_block_ratio);
  if (script_ret!=SCIRPT_ITEM_VALUE_TYPE_INT)
  {
    	printk("nand init fetch nand_good_block_ratio failed\n");
  }
  else
  {
        if(nand_good_block_ratio.val <= 0)
        {
            printk("[NAND] use nand_good_block_ratio from default parameter\n");
        }
        else
        {
            printk("[NAND] get nand_good_block_ratio from script: %d \n",nand_good_block_ratio.val);
            NAND_SetValidBlkRatio(nand_good_block_ratio.val);
        }
	}

	ret = PHY_ChangeMode(1);
	if (ret < 0)
		return ret;

    ret = PHY_ScanDDRParam();
    if (ret < 0)
        return ret;

	ret = FMT_Init();
	if (ret < 0)
		return ret;

	ret = FMT_FormatNand();
	if (ret < 0)
		return ret;
	FMT_Exit();

	/*init logic layer*/
	ret = LML_Init();
	if (ret < 0)
		return ret;

	#ifdef NAND_CACHE_RW
		NAND_CacheOpen();
	#endif


	return nand_blk_register(&mytr);
}

static void   exit_blklayer(void)
{
	nand_flush(NULL);
	nand_blk_unregister(&mytr);
	#ifdef NAND_CACHE_RW
		NAND_CacheClose();
	#endif
	LML_Exit();
	FMT_Exit();
	PHY_Exit();
}

#else
static int  init_blklayer(void)
{
	printk("[NAND] for nand test, init_blklayer \n");
	return 0;
}

static void   exit_blklayer(void)
{

}
#endif


#ifdef CONFIG_PM

#ifdef CONFIG_SUN7I_NANDFLASH_TEST
int nand_suspend(struct platform_device *plat_dev, pm_message_t state)
#else
static int nand_suspend(struct platform_device *plat_dev, pm_message_t state)
#endif
{

	int i=0;

	pr_debug("[NAND] nand_suspend \n");

	if(NORMAL_STANDBY== standby_type)
	{
	if(!IS_IDLE){
		for(i=0;i<10;i++){
			msleep(200);
			if(IS_IDLE)
				break;
		}
	}
	if(i==10){
		return -EBUSY;
	}else{
		down(&mytr.nand_ops_mutex);

		NAND_ClkDisable();
		NAND_PIORelease();
	}
	}
	else if(SUPER_STANDBY == standby_type)
	{
	pr_debug("nand super standy mode suspend\n");
		if(!IS_IDLE){
			for(i=0;i<10;i++){
				msleep(200);
				if(IS_IDLE)
					break;
			}
		}
		if(i==10){
			return -EBUSY;
		}else{
		down(&mytr.nand_ops_mutex);

		NAND_ClkDisable();
		NAND_PIORelease();
	}
	for(i=0; i<(NAND_REG_LENGTH); i++){
		nand_reg_state.nand_reg_back[i] = *(volatile u32 *)(NAND_GetIOBaseAddr() + i*0x04);
		//pr_info("reg addr 0x%x : 0x%x \n", i, nand_reg_state.nand_reg_back[i]);
	}
	}

		pr_debug("[NAND] nand_suspend ok \n");

		return 0;
}

#ifdef CONFIG_SUN7I_NANDFLASH_TEST
int nand_resume(struct platform_device *plat_dev)
#else
static int nand_resume(struct platform_device *plat_dev)
#endif
{

    __s32 ret;

	printk(KERN_INFO"[NAND] nand_resume \n");
	if(NORMAL_STANDBY== standby_type){
	NAND_PIORequest();
	NAND_ClkEnable();

	up(&mytr.nand_ops_mutex);
	}else if(SUPER_STANDBY == standby_type){
		int i;
		NAND_PIORequest();
	    NAND_ClkEnable();
        //process for super standby
		//restore reg state
		for(i=0; i<(NAND_REG_LENGTH); i++){
			if(0x9 == i){
				continue;
			}
			*(volatile u32 *)(NAND_GetIOBaseAddr()+ i*0x04) = nand_reg_state.nand_reg_back[i];
		}
        //reset all chip

    	for(i=0; i<8; i++)
        {
			__u32 bank=0;
			if(NAND_GetChipConnect()&(0x1<<i)) //chip valid
            {
                pr_info("nand reset chip %d!\n",i);
                ret = PHY_ResetChip(i);
				PHY_SetDefaultParam(bank);
				RetryCount[i] = 0;
                if(ret)
                    pr_info("nand reset chip %d failed!\n",i);
				bank++;
            }
        }

		//process for super standby
		//restore reg state
		for(i=0; i<(NAND_REG_LENGTH); i++){
			if(0x9 == i){
				continue;
			}
			*(volatile u32 *)(NAND_GetIOBaseAddr() + i*0x04) = nand_reg_state.nand_reg_back[i];
		}

		up(&mytr.nand_ops_mutex);

	}


	return 0;
}

#else

#ifdef CONFIG_SUN7I_NANDFLASH_TEST
int nand_suspend(struct platform_device *plat_dev, pm_message_t state)
#else
static int nand_suspend(struct platform_device *plat_dev, pm_message_t state)
#endif
{
    return 0;
}

#ifdef CONFIG_SUN7I_NANDFLASH_TEST
int nand_resume(struct platform_device *plat_dev)
#else
static int nand_resume(struct platform_device *plat_dev)
#endif
{
    return 0;
}

#endif

static int nand_probe(struct platform_device *plat_dev)
{
	pr_info("benn: nand probe enter\n");
	dbg_inf("nand_probe\n");

	return 0;

}

static int nand_remove(struct platform_device *plat_dev)
{
	return 0;
}

void nand_shutdown(struct platform_device *plat_dev)
{
    printk("[NAND]shutdown\n");
	nand_flush_all();
}

static struct platform_driver nand_driver = {
	.probe = nand_probe,
	.remove = nand_remove,
	.shutdown =  nand_shutdown,
#ifdef CONFIG_PM
	.suspend = nand_suspend,
	.resume = nand_resume,
#endif
	.driver = {
		.name = "sw_nand",
		.owner = THIS_MODULE,
	}
};

/****************************************************************************
 *
 * Module stuff
 *
 ****************************************************************************/

int nand_init(void)
{
	s32 ret;
//	int nand_used = 0;

	script_item_value_type_e script_ret;
	script_item_u nand_used;

    printk("%s,line:%d\n", __func__, __LINE__);
    script_ret = script_get_item("nand_para","nand_used", &nand_used);
    if (script_ret!=SCIRPT_ITEM_VALUE_TYPE_INT)
    {
    	printk("nand init fetch emac using configuration failed\n");

    }

    if(nand_used.val == 0)
    {
        printk("nand driver is disabled \n");
        return 0;
    }


	printk("[NAND]nand driver, init.\n");

	ret = init_blklayer();
	if(ret)
	{
		dbg_err("init_blklayer fail \n");
		return -1;
	}

	ret = platform_driver_register(&nand_driver);
	if(ret)
	{
		dbg_err("platform_driver_register fail \n");
		return -1;
	}
	printk("[NAND]nand driver, ok.\n");
	return 0;
}

void nand_exit(void)
{
 //   s32 ret;
//	int nand_used = 0;
	script_item_value_type_e script_ret;
	script_item_u nand_used;


    script_ret = script_get_item("nand_para","nand_used", &nand_used);
    if (script_ret!=SCIRPT_ITEM_VALUE_TYPE_INT)
    {
    	printk("nand init fetch emac using configuration failed\n");

    }

    if(nand_used.val == 0)
    {
        printk("nand driver is disabled \n");
        return ;
    }

	printk("[NAND]nand driver : bye bye\n");
	platform_driver_unregister(&nand_driver);
	//platform_device_unregister(&nand_device);
	exit_blklayer();

}

//module_init(nand_init);
//module_exit(nand_exit);
MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("nand flash groups");
MODULE_DESCRIPTION ("Generic NAND flash driver code");

