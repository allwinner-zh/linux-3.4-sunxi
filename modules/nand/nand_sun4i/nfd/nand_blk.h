/*
 * Copyright (C) 2013 Allwinnertech, kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/semaphore.h>
#include "../src/include/mbr.h"

struct nand_blk_ops;
struct list_head;
struct semaphore;
struct hd_geometry;

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
	struct request_queue *rq;
	spinlock_t queue_lock;
	struct semaphore nand_ops_mutex;

	struct list_head devs;
	struct module *owner;
};

