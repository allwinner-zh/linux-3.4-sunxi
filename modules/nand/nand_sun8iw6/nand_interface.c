/*
 * Copyright (C) 2013 Allwinnertech
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include "include/nand_include.h"

static int __init init_nand_libmodule(void)
{
	//printk("hello:%s,%d\n", __func__, __LINE__);
	//printk("hello:%s,%d\n", __func__, __LINE__);
	nand_init();
	return 0;
}
module_init(init_nand_libmodule);

static void __exit exit_nand_libmodule(void)
{
	nand_exit();
}
module_exit(exit_nand_libmodule);

MODULE_AUTHOR("Soft-Reuuimlla");
MODULE_DESCRIPTION("User mode CEDAR device interface");
MODULE_LICENSE("GPL");
