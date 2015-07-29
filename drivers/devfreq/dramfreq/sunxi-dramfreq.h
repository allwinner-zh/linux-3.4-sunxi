/*
 * drivers/devfreq/dramfreq/sunxi-dramfreq.h
 *
 * Copyright (C) 2012 - 2016 Reuuimlla Limited
 * Pan Nan <pannan@reuuimllatech.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#ifndef __DRAM_FREQ_H__
#define __DRAM_FREQ_H__

#include <mach/platform.h>

#define DRAM_MDFS_TABLE_PARA0   (2)
#define DRAM_MDFS_TABLE_PARA1   (10)

typedef struct __DRAM_PARA
{
    //normal configuration
    unsigned int dram_clk;
    unsigned int dram_type;    //dram_type  DDR2: 2  DDR3: 3 LPDDR2: 6 DDR3L: 31
    unsigned int dram_zq;
    unsigned int dram_odt_en;

    //control configuration
    unsigned int dram_para1;
    unsigned int dram_para2;

    //timing configuration
    unsigned int dram_mr0;
    unsigned int dram_mr1;
    unsigned int dram_mr2;
    unsigned int dram_mr3;
    unsigned int dram_tpr0;
    unsigned int dram_tpr1;
    unsigned int dram_tpr2;
    unsigned int dram_tpr3;
    unsigned int dram_tpr4;
    unsigned int dram_tpr5;
    unsigned int dram_tpr6;

    //reserved for future use
    unsigned int dram_tpr7;
    unsigned int dram_tpr8;
    unsigned int dram_tpr9;
    unsigned int dram_tpr10;
    unsigned int dram_tpr11;
    unsigned int dram_tpr12;
    unsigned int dram_tpr13;

    unsigned int high_freq;

    unsigned int table[DRAM_MDFS_TABLE_PARA0][DRAM_MDFS_TABLE_PARA1];
} __dram_para_t;

extern int mdfs_main(__dram_para_t *softfreq_switch);

#endif /* __DRAM_FREQ_H__ */
