/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/semaphore.h>
#include "nand_lib.h"

#define __FPGA_TEST__
#define __LINUX_NAND_SUPPORT_INT__
#define __LINUX_SUPPORT_DMA_INT__
#define __LINUX_SUPPORT_RB_INT__
#define MAX_NAND_DEV                16


#if defined CONFIG_ARCH_SUN8IW1P1
#define  SUN8IW1P1
#elif defined CONFIG_ARCH_SUN8IW3P1
#define  SUN8IW3P1
#elif defined CONFIG_ARCH_SUN8IW5P1
#define  SUN8IW5P1
#elif defined CONFIG_ARCH_SUN8IW6P1
#define  SUN8IW6P1
#elif defined CONFIG_ARCH_SUN8IW7P1
#define  SUN8IW7P1
#elif defined CONFIG_ARCH_SUN9IW1P1
#define  SUN9IW1P1
#else
#error "please select a platform\n"
#endif


struct nand_blk_ops;
struct list_head;
struct semaphore;
struct hd_geometry;

#define NAND_REG_LENGTH 	(0xC4>>2)
struct nand_blk_dev{
	struct nand_blk_ops *nandr;
	struct list_head list;

	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;

	int devnum;
	unsigned long size;
	unsigned long off_size;
	int readonly;
	int writeonly;
	int disable_access;
	void *blkcore_priv;
};
struct nand_blk_ops{
	/* blk device ID */
	char *name;
	int major;
	int minorbits;

	/* add/remove nandflash devparts,use gendisk */
	int (*add_dev)(struct nand_blk_ops *nandr, struct nand_disk *part);
	int (*remove_dev)(struct nand_blk_dev *dev);

	/* Block layer ioctls */
	int (*getgeo)(struct nand_blk_dev *dev, struct hd_geometry *geo);
	int (*flush)(struct nand_blk_dev *dev);

	/* Called with mtd_table_mutex held; no race with add/remove */
	int (*open)(struct nand_blk_dev *dev);
	int (*release)(struct nand_blk_dev *dev);

	/* synchronization variable */
	struct completion thread_exit;
	int quit;
	wait_queue_head_t thread_wq;
	struct request_queue *rq[MAX_NAND_DEV];
	spinlock_t queue_lock;
	struct semaphore nand_ops_mutex;

	struct list_head devs;
	struct module *owner;
};

struct nand_state{
	u32 nand_reg_back[2][NAND_REG_LENGTH];
};
