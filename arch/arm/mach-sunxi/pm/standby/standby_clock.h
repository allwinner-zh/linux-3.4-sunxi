/*
*********************************************************************************************************
*                                                    LINUX-KERNEL
*                                        AllWinner Linux Platform Develop Kits
*                                                   Kernel Module
*
*                                    (c) Copyright 2006-2011, kevin.z China
*                                             All Rights Reserved
*
* File    : standby_clock.h
* By      : kevin.z
* Version : v1.0
* Date    : 2011-5-31 21:05
* Descript:
* Update  : date                auther      ver     notes
*********************************************************************************************************
*/
#ifndef __STANDBY_CLOCK_H__
#define __STANDBY_CLOCK_H__

#include "standby_cfg.h"
#include "standby_i.h"

struct sun4i_clk_div_t {
    __u32   cpu_div:4;      /* division of cpu clock, divide core_pll */
    __u32   axi_div:4;      /* division of axi clock, divide cpu clock*/
    __u32   ahb_div:4;      /* division of ahb clock, divide axi clock*/
    __u32   apb_div:4;      /* division of apb clock, divide ahb clock*/
    __u32   reserved:16;
};

#define PLL_CTRL_REG0_OFFSET	(0x40)
#define PLL_CTRL_REG1_OFFSET	(0x44)

__s32 standby_clk_init(void);
__s32 standby_clk_exit(void);
void standby_clk_core2hosc(void);
void standby_clk_pll1enable(void);
void standby_clk_ldoenable(void);
extern __u32   cpu_ms_loopcnt;

#endif  /* __STANDBY_CLOCK_H__ */

