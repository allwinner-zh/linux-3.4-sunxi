/*
 * Copyright (c) 2011-2015 yanggq.young@allwinnertech.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/syscalls.h>
#include <linux/power/aw_pm.h>

standby_type_e standby_type = NORMAL_STANDBY;
EXPORT_SYMBOL(standby_type);

static int aw_pm_valid(suspend_state_t state)
{
	printk("%s\n", __func__);
    return 1;
}

static int aw_pm_begin(suspend_state_t state)
{
	printk("%s\n", __func__);
	return 0;
}

static int aw_pm_prepare(void)
{
	printk("%s\n", __func__);
    return 0;
}

static int aw_pm_prepare_late(void)
{
	printk("%s\n", __func__);
    return 0;
}

static int aw_pm_enter(suspend_state_t state)
{
	printk("%s\n", __func__);
    cpu_do_idle();
    return 0;
}

static void aw_pm_wake(void)
{
	printk("%s\n", __func__);
    return;
}

static void aw_pm_finish(void)
{
	printk("%s\n", __func__);
    return ;
}

static void aw_pm_end(void)
{
	printk("%s\n", __func__);
    return;
}


static void aw_pm_recover(void)
{
	printk("%s\n", __func__);
    return;
}

/*
    define platform_suspend_ops which is registered into PM core.
*/
static struct platform_suspend_ops aw_pm_ops = {
    .valid = aw_pm_valid,
    .begin = aw_pm_begin,
    .prepare = aw_pm_prepare,
    .prepare_late = aw_pm_prepare_late,
    .enter = aw_pm_enter,
    .wake = aw_pm_wake,
    .finish = aw_pm_finish,
    .end = aw_pm_end,
    .recover = aw_pm_recover,
};

static int __init aw_pm_init(void)
{
	printk("%s\n", __func__);
	suspend_set_ops(&aw_pm_ops);
    return 0;
}

static void aw_pm_exit(void)
{
	printk("%s\n", __func__);
	suspend_set_ops(NULL);
}

module_init(aw_pm_init);
module_exit(aw_pm_exit);

