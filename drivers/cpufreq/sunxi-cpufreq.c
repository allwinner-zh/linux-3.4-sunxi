/*
 * drivers/cpufreq/sunxi-cpufreq.c
 *
 * Copyright (c) 2012 softwinner.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <mach/sys_config.h>
#include <linux/cpu.h>
#include <asm/cpu.h>
#include <linux/pm.h>
#include <linux/arisc/arisc.h>
#include <linux/clk/sunxi_name.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <mach/platform.h>

#include "sunxi-cpufreq.h"

#ifdef CONFIG_DEBUG_FS
static unsigned long long cpufreq_set_time_usecs = 0;
static unsigned long long cpufreq_get_time_usecs = 0;
#endif

static struct sunxi_cpu_freq_t  cpu_cur;    /* current cpu frequency configuration  */
static unsigned int last_target = ~0;       /* backup last target frequency         */

#if !defined(CONFIG_ARCH_SUN8IW7P1) && !defined(CONFIG_ARCH_SUN8IW8P1)
struct regulator *cpu_vdd;  /* cpu vdd handler   */
#endif
static struct clk *clk_pll; /* pll clock handler */
static struct clk *clk_cpu; /* cpu clock handler */
static struct clk *clk_axi; /* axi clock handler */
static DEFINE_MUTEX(sunxi_cpu_lock);

static unsigned int cpu_freq_max   = SUNXI_CPUFREQ_MAX / 1000;
static unsigned int cpu_freq_min   = SUNXI_CPUFREQ_MIN / 1000;
static unsigned int cpu_freq_ext   = SUNXI_CPUFREQ_MAX / 1000;  // extremity cpu freq
static unsigned int cpu_freq_boot  = SUNXI_CPUFREQ_MAX / 1000;

int sunxi_dvfs_debug = 0;
int sunxi_boot_freq_lock = 0;

#if defined(CONFIG_ARCH_SUN8IW3P1) && defined(CONFIG_DEVFREQ_DRAM_FREQ)
extern int dramfreq_need_suspend;
extern int __ahb_set_rate(unsigned long ahb_freq);
#endif

int table_length_syscfg = 0;
struct cpufreq_dvfs dvfs_table_syscfg[16];

#if defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW8P1)
struct pll_cpu_factor_t {
	u8 factor_n;
	u8 factor_k;
	u8 factor_m;
	u8 factor_p;
};

struct ccu_pll_cpu_ctrl_reg
{
	u32 factor_m:2;   //bit0,  factor M
	u32 reserved0:2;  //bit2,  reserved
	u32 factor_k:2;   //bit4,  factor K
	u32 reserved1:2;  //bit6,  reserved
	u32 factor_n:5;   //bit8,  factor N
	u32 reserved2:3;  //bit13, reserved
	u32 factor_p:2;   //bit16, factor P
	u32 reserved3:6;  //bit18, reserved
	u32 cpu_sdm_en:1; //bit24, cpu sdm en
	u32 reserved4:3;  //bit25, reserved
	u32 lock:1;       //bit28, lock
	u32 reserved5:2;  //bit29, reserved
	u32 pll_en:1;     //bit31, pll enable
} ;

struct ccu_cpu_axi_cfg_reg
{
	u32 axi_div:2;    //bit0,  axi clock div
	u32 reserved0:6;  //bit2,  reserved
	u32 apb_div:2;    //bit8,  apb clock div
	u32 reserved1:6;  //bit10, reserved
	u32 cpu_src:2;    //bit16, cpu clock source select
	u32 reserved2:14; //bit18, reserved
};

static struct ccu_pll_cpu_ctrl_reg *pll_cpu_reg;
static struct ccu_cpu_axi_cfg_reg  *cpu_axi_reg;

static struct pll_cpu_factor_t pll_cpu_table[] =
{   //N  K  M  P
	{ 9, 0, 0, 2},    //Theory Freq1 = 0   , Actual Freq2 = 60  , Index = 0
	{ 9, 0, 0, 2},    //Theory Freq1 = 6   , Actual Freq2 = 60  , Index = 1
	{ 9, 0, 0, 2},    //Theory Freq1 = 12  , Actual Freq2 = 60  , Index = 2
	{ 9, 0, 0, 2},    //Theory Freq1 = 18  , Actual Freq2 = 60  , Index = 3
	{ 9, 0, 0, 2},    //Theory Freq1 = 24  , Actual Freq2 = 60  , Index = 4
	{ 9, 0, 0, 2},    //Theory Freq1 = 30  , Actual Freq2 = 60  , Index = 5
	{ 9, 0, 0, 2},    //Theory Freq1 = 36  , Actual Freq2 = 60  , Index = 6
	{ 9, 0, 0, 2},    //Theory Freq1 = 42  , Actual Freq2 = 60  , Index = 7
	{ 9, 0, 0, 2},    //Theory Freq1 = 48  , Actual Freq2 = 60  , Index = 8
	{ 9, 0, 0, 2},    //Theory Freq1 = 54  , Actual Freq2 = 60  , Index = 9
	{ 9, 0, 0, 2},    //Theory Freq1 = 60  , Actual Freq2 = 60  , Index = 10
	{10, 0, 0, 2},    //Theory Freq1 = 66  , Actual Freq2 = 66  , Index = 11
	{11, 0, 0, 2},    //Theory Freq1 = 72  , Actual Freq2 = 72  , Index = 12
	{12, 0, 0, 2},    //Theory Freq1 = 78  , Actual Freq2 = 78  , Index = 13
	{13, 0, 0, 2},    //Theory Freq1 = 84  , Actual Freq2 = 84  , Index = 14
	{14, 0, 0, 2},    //Theory Freq1 = 90  , Actual Freq2 = 90  , Index = 15
	{15, 0, 0, 2},    //Theory Freq1 = 96  , Actual Freq2 = 96  , Index = 16
	{16, 0, 0, 2},    //Theory Freq1 = 102 , Actual Freq2 = 102 , Index = 17
	{17, 0, 0, 2},    //Theory Freq1 = 108 , Actual Freq2 = 108 , Index = 18
	{18, 0, 0, 2},    //Theory Freq1 = 114 , Actual Freq2 = 114 , Index = 19
	{9 , 0, 0, 1},    //Theory Freq1 = 120 , Actual Freq2 = 120 , Index = 20
	{10, 0, 0, 1},    //Theory Freq1 = 126 , Actual Freq2 = 132 , Index = 21
	{10, 0, 0, 1},    //Theory Freq1 = 132 , Actual Freq2 = 132 , Index = 22
	{11, 0, 0, 1},    //Theory Freq1 = 138 , Actual Freq2 = 144 , Index = 23
	{11, 0, 0, 1},    //Theory Freq1 = 144 , Actual Freq2 = 144 , Index = 24
	{12, 0, 0, 1},    //Theory Freq1 = 150 , Actual Freq2 = 156 , Index = 25
	{12, 0, 0, 1},    //Theory Freq1 = 156 , Actual Freq2 = 156 , Index = 26
	{13, 0, 0, 1},    //Theory Freq1 = 162 , Actual Freq2 = 168 , Index = 27
	{13, 0, 0, 1},    //Theory Freq1 = 168 , Actual Freq2 = 168 , Index = 28
	{14, 0, 0, 1},    //Theory Freq1 = 174 , Actual Freq2 = 180 , Index = 29
	{14, 0, 0, 1},    //Theory Freq1 = 180 , Actual Freq2 = 180 , Index = 30
	{15, 0, 0, 1},    //Theory Freq1 = 186 , Actual Freq2 = 192 , Index = 31
	{15, 0, 0, 1},    //Theory Freq1 = 192 , Actual Freq2 = 192 , Index = 32
	{16, 0, 0, 1},    //Theory Freq1 = 198 , Actual Freq2 = 204 , Index = 33
	{16, 0, 0, 1},    //Theory Freq1 = 204 , Actual Freq2 = 204 , Index = 34
	{17, 0, 0, 1},    //Theory Freq1 = 210 , Actual Freq2 = 216 , Index = 35
	{17, 0, 0, 1},    //Theory Freq1 = 216 , Actual Freq2 = 216 , Index = 36
	{18, 0, 0, 1},    //Theory Freq1 = 222 , Actual Freq2 = 228 , Index = 37
	{18, 0, 0, 1},    //Theory Freq1 = 228 , Actual Freq2 = 228 , Index = 38
	{9 , 0, 0, 0},    //Theory Freq1 = 234 , Actual Freq2 = 240 , Index = 39
	{9 , 0, 0, 0},    //Theory Freq1 = 240 , Actual Freq2 = 240 , Index = 40
	{10, 0, 0, 0},    //Theory Freq1 = 246 , Actual Freq2 = 264 , Index = 41
	{10, 0, 0, 0},    //Theory Freq1 = 252 , Actual Freq2 = 264 , Index = 42
	{10, 0, 0, 0},    //Theory Freq1 = 258 , Actual Freq2 = 264 , Index = 43
	{10, 0, 0, 0},    //Theory Freq1 = 264 , Actual Freq2 = 264 , Index = 44
	{11, 0, 0, 0},    //Theory Freq1 = 270 , Actual Freq2 = 288 , Index = 45
	{11, 0, 0, 0},    //Theory Freq1 = 276 , Actual Freq2 = 288 , Index = 46
	{11, 0, 0, 0},    //Theory Freq1 = 282 , Actual Freq2 = 288 , Index = 47
	{11, 0, 0, 0},    //Theory Freq1 = 288 , Actual Freq2 = 288 , Index = 48
	{12, 0, 0, 0},    //Theory Freq1 = 294 , Actual Freq2 = 312 , Index = 49
	{12, 0, 0, 0},    //Theory Freq1 = 300 , Actual Freq2 = 312 , Index = 50
	{12, 0, 0, 0},    //Theory Freq1 = 306 , Actual Freq2 = 312 , Index = 51
	{12, 0, 0, 0},    //Theory Freq1 = 312 , Actual Freq2 = 312 , Index = 52
	{13, 0, 0, 0},    //Theory Freq1 = 318 , Actual Freq2 = 336 , Index = 53
	{13, 0, 0, 0},    //Theory Freq1 = 324 , Actual Freq2 = 336 , Index = 54
	{13, 0, 0, 0},    //Theory Freq1 = 330 , Actual Freq2 = 336 , Index = 55
	{13, 0, 0, 0},    //Theory Freq1 = 336 , Actual Freq2 = 336 , Index = 56
	{14, 0, 0, 0},    //Theory Freq1 = 342 , Actual Freq2 = 360 , Index = 57
	{14, 0, 0, 0},    //Theory Freq1 = 348 , Actual Freq2 = 360 , Index = 58
	{14, 0, 0, 0},    //Theory Freq1 = 354 , Actual Freq2 = 360 , Index = 59
	{14, 0, 0, 0},    //Theory Freq1 = 360 , Actual Freq2 = 360 , Index = 60
	{15, 0, 0, 0},    //Theory Freq1 = 366 , Actual Freq2 = 384 , Index = 61
	{15, 0, 0, 0},    //Theory Freq1 = 372 , Actual Freq2 = 384 , Index = 62
	{15, 0, 0, 0},    //Theory Freq1 = 378 , Actual Freq2 = 384 , Index = 63
	{15, 0, 0, 0},    //Theory Freq1 = 384 , Actual Freq2 = 384 , Index = 64
	{16, 0, 0, 0},    //Theory Freq1 = 390 , Actual Freq2 = 408 , Index = 65
	{16, 0, 0, 0},    //Theory Freq1 = 396 , Actual Freq2 = 408 , Index = 66
	{16, 0, 0, 0},    //Theory Freq1 = 402 , Actual Freq2 = 408 , Index = 67
	{16, 0, 0, 0},    //Theory Freq1 = 408 , Actual Freq2 = 408 , Index = 68
	{17, 0, 0, 0},    //Theory Freq1 = 414 , Actual Freq2 = 432 , Index = 69
	{17, 0, 0, 0},    //Theory Freq1 = 420 , Actual Freq2 = 432 , Index = 70
	{17, 0, 0, 0},    //Theory Freq1 = 426 , Actual Freq2 = 432 , Index = 71
	{17, 0, 0, 0},    //Theory Freq1 = 432 , Actual Freq2 = 432 , Index = 72
	{18, 0, 0, 0},    //Theory Freq1 = 438 , Actual Freq2 = 456 , Index = 73
	{18, 0, 0, 0},    //Theory Freq1 = 444 , Actual Freq2 = 456 , Index = 74
	{18, 0, 0, 0},    //Theory Freq1 = 450 , Actual Freq2 = 456 , Index = 75
	{18, 0, 0, 0},    //Theory Freq1 = 456 , Actual Freq2 = 456 , Index = 76
	{19, 0, 0, 0},    //Theory Freq1 = 462 , Actual Freq2 = 480 , Index = 77
	{19, 0, 0, 0},    //Theory Freq1 = 468 , Actual Freq2 = 480 , Index = 78
	{19, 0, 0, 0},    //Theory Freq1 = 474 , Actual Freq2 = 480 , Index = 79
	{19, 0, 0, 0},    //Theory Freq1 = 480 , Actual Freq2 = 480 , Index = 80
	{20, 0, 0, 0},    //Theory Freq1 = 486 , Actual Freq2 = 504 , Index = 81
	{20, 0, 0, 0},    //Theory Freq1 = 492 , Actual Freq2 = 504 , Index = 82
	{20, 0, 0, 0},    //Theory Freq1 = 498 , Actual Freq2 = 504 , Index = 83
	{20, 0, 0, 0},    //Theory Freq1 = 504 , Actual Freq2 = 504 , Index = 84
	{21, 0, 0, 0},    //Theory Freq1 = 510 , Actual Freq2 = 528 , Index = 85
	{21, 0, 0, 0},    //Theory Freq1 = 516 , Actual Freq2 = 528 , Index = 86
	{21, 0, 0, 0},    //Theory Freq1 = 522 , Actual Freq2 = 528 , Index = 87
	{21, 0, 0, 0},    //Theory Freq1 = 528 , Actual Freq2 = 528 , Index = 88
	{22, 0, 0, 0},    //Theory Freq1 = 534 , Actual Freq2 = 552 , Index = 89
	{22, 0, 0, 0},    //Theory Freq1 = 540 , Actual Freq2 = 552 , Index = 90
	{22, 0, 0, 0},    //Theory Freq1 = 546 , Actual Freq2 = 552 , Index = 91
	{22, 0, 0, 0},    //Theory Freq1 = 552 , Actual Freq2 = 552 , Index = 92
	{23, 0, 0, 0},    //Theory Freq1 = 558 , Actual Freq2 = 576 , Index = 93
	{23, 0, 0, 0},    //Theory Freq1 = 564 , Actual Freq2 = 576 , Index = 94
	{23, 0, 0, 0},    //Theory Freq1 = 570 , Actual Freq2 = 576 , Index = 95
	{23, 0, 0, 0},    //Theory Freq1 = 576 , Actual Freq2 = 576 , Index = 96
	{24, 0, 0, 0},    //Theory Freq1 = 582 , Actual Freq2 = 600 , Index = 97
	{24, 0, 0, 0},    //Theory Freq1 = 588 , Actual Freq2 = 600 , Index = 98
	{24, 0, 0, 0},    //Theory Freq1 = 594 , Actual Freq2 = 600 , Index = 99
	{24, 0, 0, 0},    //Theory Freq1 = 600 , Actual Freq2 = 600 , Index = 100
	{25, 0, 0, 0},    //Theory Freq1 = 606 , Actual Freq2 = 624 , Index = 101
	{25, 0, 0, 0},    //Theory Freq1 = 612 , Actual Freq2 = 624 , Index = 102
	{25, 0, 0, 0},    //Theory Freq1 = 618 , Actual Freq2 = 624 , Index = 103
	{25, 0, 0, 0},    //Theory Freq1 = 624 , Actual Freq2 = 624 , Index = 104
	{26, 0, 0, 0},    //Theory Freq1 = 630 , Actual Freq2 = 648 , Index = 105
	{26, 0, 0, 0},    //Theory Freq1 = 636 , Actual Freq2 = 648 , Index = 106
	{26, 0, 0, 0},    //Theory Freq1 = 642 , Actual Freq2 = 648 , Index = 107
	{26, 0, 0, 0},    //Theory Freq1 = 648 , Actual Freq2 = 648 , Index = 108
	{27, 0, 0, 0},    //Theory Freq1 = 654 , Actual Freq2 = 672 , Index = 109
	{27, 0, 0, 0},    //Theory Freq1 = 660 , Actual Freq2 = 672 , Index = 110
	{27, 0, 0, 0},    //Theory Freq1 = 666 , Actual Freq2 = 672 , Index = 111
	{27, 0, 0, 0},    //Theory Freq1 = 672 , Actual Freq2 = 672 , Index = 112
	{28, 0, 0, 0},    //Theory Freq1 = 678 , Actual Freq2 = 696 , Index = 113
	{28, 0, 0, 0},    //Theory Freq1 = 684 , Actual Freq2 = 696 , Index = 114
	{28, 0, 0, 0},    //Theory Freq1 = 690 , Actual Freq2 = 696 , Index = 115
	{28, 0, 0, 0},    //Theory Freq1 = 696 , Actual Freq2 = 696 , Index = 116
	{29, 0, 0, 0},    //Theory Freq1 = 702 , Actual Freq2 = 720 , Index = 117
	{29, 0, 0, 0},    //Theory Freq1 = 708 , Actual Freq2 = 720 , Index = 118
	{29, 0, 0, 0},    //Theory Freq1 = 714 , Actual Freq2 = 720 , Index = 119
	{29, 0, 0, 0},    //Theory Freq1 = 720 , Actual Freq2 = 720 , Index = 120
	{15, 1, 0, 0},    //Theory Freq1 = 726 , Actual Freq2 = 768 , Index = 121
	{15, 1, 0, 0},    //Theory Freq1 = 732 , Actual Freq2 = 768 , Index = 122
	{15, 1, 0, 0},    //Theory Freq1 = 738 , Actual Freq2 = 768 , Index = 123
	{15, 1, 0, 0},    //Theory Freq1 = 744 , Actual Freq2 = 768 , Index = 124
	{15, 1, 0, 0},    //Theory Freq1 = 750 , Actual Freq2 = 768 , Index = 125
	{15, 1, 0, 0},    //Theory Freq1 = 756 , Actual Freq2 = 768 , Index = 126
	{15, 1, 0, 0},    //Theory Freq1 = 762 , Actual Freq2 = 768 , Index = 127
	{15, 1, 0, 0},    //Theory Freq1 = 768 , Actual Freq2 = 768 , Index = 128
	{10, 2, 0, 0},    //Theory Freq1 = 774 , Actual Freq2 = 792 , Index = 129
	{10, 2, 0, 0},    //Theory Freq1 = 780 , Actual Freq2 = 792 , Index = 130
	{10, 2, 0, 0},    //Theory Freq1 = 786 , Actual Freq2 = 792 , Index = 131
	{10, 2, 0, 0},    //Theory Freq1 = 792 , Actual Freq2 = 792 , Index = 132
	{16, 1, 0, 0},    //Theory Freq1 = 798 , Actual Freq2 = 816 , Index = 133
	{16, 1, 0, 0},    //Theory Freq1 = 804 , Actual Freq2 = 816 , Index = 134
	{16, 1, 0, 0},    //Theory Freq1 = 810 , Actual Freq2 = 816 , Index = 135
	{16, 1, 0, 0},    //Theory Freq1 = 816 , Actual Freq2 = 816 , Index = 136
	{17, 1, 0, 0},    //Theory Freq1 = 822 , Actual Freq2 = 864 , Index = 137
	{17, 1, 0, 0},    //Theory Freq1 = 828 , Actual Freq2 = 864 , Index = 138
	{17, 1, 0, 0},    //Theory Freq1 = 834 , Actual Freq2 = 864 , Index = 139
	{17, 1, 0, 0},    //Theory Freq1 = 840 , Actual Freq2 = 864 , Index = 140
	{17, 1, 0, 0},    //Theory Freq1 = 846 , Actual Freq2 = 864 , Index = 141
	{17, 1, 0, 0},    //Theory Freq1 = 852 , Actual Freq2 = 864 , Index = 142
	{17, 1, 0, 0},    //Theory Freq1 = 858 , Actual Freq2 = 864 , Index = 143
	{17, 1, 0, 0},    //Theory Freq1 = 864 , Actual Freq2 = 864 , Index = 144
	{18, 1, 0, 0},    //Theory Freq1 = 870 , Actual Freq2 = 912 , Index = 145
	{18, 1, 0, 0},    //Theory Freq1 = 876 , Actual Freq2 = 912 , Index = 146
	{18, 1, 0, 0},    //Theory Freq1 = 882 , Actual Freq2 = 912 , Index = 147
	{18, 1, 0, 0},    //Theory Freq1 = 888 , Actual Freq2 = 912 , Index = 148
	{18, 1, 0, 0},    //Theory Freq1 = 894 , Actual Freq2 = 912 , Index = 149
	{18, 1, 0, 0},    //Theory Freq1 = 900 , Actual Freq2 = 912 , Index = 150
	{18, 1, 0, 0},    //Theory Freq1 = 906 , Actual Freq2 = 912 , Index = 151
	{18, 1, 0, 0},    //Theory Freq1 = 912 , Actual Freq2 = 912 , Index = 152
	{12, 2, 0, 0},    //Theory Freq1 = 918 , Actual Freq2 = 936 , Index = 153
	{12, 2, 0, 0},    //Theory Freq1 = 924 , Actual Freq2 = 936 , Index = 154
	{12, 2, 0, 0},    //Theory Freq1 = 930 , Actual Freq2 = 936 , Index = 155
	{12, 2, 0, 0},    //Theory Freq1 = 936 , Actual Freq2 = 936 , Index = 156
	{19, 1, 0, 0},    //Theory Freq1 = 942 , Actual Freq2 = 960 , Index = 157
	{19, 1, 0, 0},    //Theory Freq1 = 948 , Actual Freq2 = 960 , Index = 158
	{19, 1, 0, 0},    //Theory Freq1 = 954 , Actual Freq2 = 960 , Index = 159
	{19, 1, 0, 0},    //Theory Freq1 = 960 , Actual Freq2 = 960 , Index = 160
	{20, 1, 0, 0},    //Theory Freq1 = 966 , Actual Freq2 = 1008, Index = 161
	{20, 1, 0, 0},    //Theory Freq1 = 972 , Actual Freq2 = 1008, Index = 162
	{20, 1, 0, 0},    //Theory Freq1 = 978 , Actual Freq2 = 1008, Index = 163
	{20, 1, 0, 0},    //Theory Freq1 = 984 , Actual Freq2 = 1008, Index = 164
	{20, 1, 0, 0},    //Theory Freq1 = 990 , Actual Freq2 = 1008, Index = 165
	{20, 1, 0, 0},    //Theory Freq1 = 996 , Actual Freq2 = 1008, Index = 166
	{20, 1, 0, 0},    //Theory Freq1 = 1002, Actual Freq2 = 1008, Index = 167
	{20, 1, 0, 0},    //Theory Freq1 = 1008, Actual Freq2 = 1008, Index = 168
	{21, 1, 0, 0},    //Theory Freq1 = 1014, Actual Freq2 = 1056, Index = 169
	{21, 1, 0, 0},    //Theory Freq1 = 1020, Actual Freq2 = 1056, Index = 170
	{21, 1, 0, 0},    //Theory Freq1 = 1026, Actual Freq2 = 1056, Index = 171
	{21, 1, 0, 0},    //Theory Freq1 = 1032, Actual Freq2 = 1056, Index = 172
	{21, 1, 0, 0},    //Theory Freq1 = 1038, Actual Freq2 = 1056, Index = 173
	{21, 1, 0, 0},    //Theory Freq1 = 1044, Actual Freq2 = 1056, Index = 174
	{21, 1, 0, 0},    //Theory Freq1 = 1050, Actual Freq2 = 1056, Index = 175
	{21, 1, 0, 0},    //Theory Freq1 = 1056, Actual Freq2 = 1056, Index = 176
	{14, 2, 0, 0},    //Theory Freq1 = 1062, Actual Freq2 = 1080, Index = 177
	{14, 2, 0, 0},    //Theory Freq1 = 1068, Actual Freq2 = 1080, Index = 178
	{14, 2, 0, 0},    //Theory Freq1 = 1074, Actual Freq2 = 1080, Index = 179
	{14, 2, 0, 0},    //Theory Freq1 = 1080, Actual Freq2 = 1080, Index = 180
	{22, 1, 0, 0},    //Theory Freq1 = 1086, Actual Freq2 = 1104, Index = 181
	{22, 1, 0, 0},    //Theory Freq1 = 1092, Actual Freq2 = 1104, Index = 182
	{22, 1, 0, 0},    //Theory Freq1 = 1098, Actual Freq2 = 1104, Index = 183
	{22, 1, 0, 0},    //Theory Freq1 = 1104, Actual Freq2 = 1104, Index = 184
	{23, 1, 0, 0},    //Theory Freq1 = 1110, Actual Freq2 = 1152, Index = 185
	{23, 1, 0, 0},    //Theory Freq1 = 1116, Actual Freq2 = 1152, Index = 186
	{23, 1, 0, 0},    //Theory Freq1 = 1122, Actual Freq2 = 1152, Index = 187
	{23, 1, 0, 0},    //Theory Freq1 = 1128, Actual Freq2 = 1152, Index = 188
	{23, 1, 0, 0},    //Theory Freq1 = 1134, Actual Freq2 = 1152, Index = 189
	{23, 1, 0, 0},    //Theory Freq1 = 1140, Actual Freq2 = 1152, Index = 190
	{23, 1, 0, 0},    //Theory Freq1 = 1146, Actual Freq2 = 1152, Index = 191
	{23, 1, 0, 0},    //Theory Freq1 = 1152, Actual Freq2 = 1152, Index = 192
	{24, 1, 0, 0},    //Theory Freq1 = 1158, Actual Freq2 = 1200, Index = 193
	{24, 1, 0, 0},    //Theory Freq1 = 1164, Actual Freq2 = 1200, Index = 194
	{24, 1, 0, 0},    //Theory Freq1 = 1170, Actual Freq2 = 1200, Index = 195
	{24, 1, 0, 0},    //Theory Freq1 = 1176, Actual Freq2 = 1200, Index = 196
	{24, 1, 0, 0},    //Theory Freq1 = 1182, Actual Freq2 = 1200, Index = 197
	{24, 1, 0, 0},    //Theory Freq1 = 1188, Actual Freq2 = 1200, Index = 198
	{24, 1, 0, 0},    //Theory Freq1 = 1194, Actual Freq2 = 1200, Index = 199
	{24, 1, 0, 0},    //Theory Freq1 = 1200, Actual Freq2 = 1200, Index = 200
	{16, 2, 0, 0},    //Theory Freq1 = 1206, Actual Freq2 = 1224, Index = 201
	{16, 2, 0, 0},    //Theory Freq1 = 1212, Actual Freq2 = 1224, Index = 202
	{16, 2, 0, 0},    //Theory Freq1 = 1218, Actual Freq2 = 1224, Index = 203
	{16, 2, 0, 0},    //Theory Freq1 = 1224, Actual Freq2 = 1224, Index = 204
	{25, 1, 0, 0},    //Theory Freq1 = 1230, Actual Freq2 = 1248, Index = 205
	{25, 1, 0, 0},    //Theory Freq1 = 1236, Actual Freq2 = 1248, Index = 206
	{25, 1, 0, 0},    //Theory Freq1 = 1242, Actual Freq2 = 1248, Index = 207
	{25, 1, 0, 0},    //Theory Freq1 = 1248, Actual Freq2 = 1248, Index = 208
	{26, 1, 0, 0},    //Theory Freq1 = 1254, Actual Freq2 = 1296, Index = 209
	{26, 1, 0, 0},    //Theory Freq1 = 1260, Actual Freq2 = 1296, Index = 210
	{26, 1, 0, 0},    //Theory Freq1 = 1266, Actual Freq2 = 1296, Index = 211
	{26, 1, 0, 0},    //Theory Freq1 = 1272, Actual Freq2 = 1296, Index = 212
	{26, 1, 0, 0},    //Theory Freq1 = 1278, Actual Freq2 = 1296, Index = 213
	{26, 1, 0, 0},    //Theory Freq1 = 1284, Actual Freq2 = 1296, Index = 214
	{26, 1, 0, 0},    //Theory Freq1 = 1290, Actual Freq2 = 1296, Index = 215
	{26, 1, 0, 0},    //Theory Freq1 = 1296, Actual Freq2 = 1296, Index = 216
	{27, 1, 0, 0},    //Theory Freq1 = 1302, Actual Freq2 = 1344, Index = 217
	{27, 1, 0, 0},    //Theory Freq1 = 1308, Actual Freq2 = 1344, Index = 218
	{27, 1, 0, 0},    //Theory Freq1 = 1314, Actual Freq2 = 1344, Index = 219
	{27, 1, 0, 0},    //Theory Freq1 = 1320, Actual Freq2 = 1344, Index = 220
	{27, 1, 0, 0},    //Theory Freq1 = 1326, Actual Freq2 = 1344, Index = 221
	{27, 1, 0, 0},    //Theory Freq1 = 1332, Actual Freq2 = 1344, Index = 222
	{27, 1, 0, 0},    //Theory Freq1 = 1338, Actual Freq2 = 1344, Index = 223
	{27, 1, 0, 0},    //Theory Freq1 = 1344, Actual Freq2 = 1344, Index = 224
	{18, 2, 0, 0},    //Theory Freq1 = 1350, Actual Freq2 = 1368, Index = 225
	{18, 2, 0, 0},    //Theory Freq1 = 1356, Actual Freq2 = 1368, Index = 226
	{18, 2, 0, 0},    //Theory Freq1 = 1362, Actual Freq2 = 1368, Index = 227
	{18, 2, 0, 0},    //Theory Freq1 = 1368, Actual Freq2 = 1368, Index = 228
	{19, 2, 0, 0},    //Theory Freq1 = 1374, Actual Freq2 = 1440, Index = 229
	{19, 2, 0, 0},    //Theory Freq1 = 1380, Actual Freq2 = 1440, Index = 230
	{19, 2, 0, 0},    //Theory Freq1 = 1386, Actual Freq2 = 1440, Index = 231
	{19, 2, 0, 0},    //Theory Freq1 = 1392, Actual Freq2 = 1440, Index = 232
	{19, 2, 0, 0},    //Theory Freq1 = 1398, Actual Freq2 = 1440, Index = 233
	{19, 2, 0, 0},    //Theory Freq1 = 1404, Actual Freq2 = 1440, Index = 234
	{19, 2, 0, 0},    //Theory Freq1 = 1410, Actual Freq2 = 1440, Index = 235
	{19, 2, 0, 0},    //Theory Freq1 = 1416, Actual Freq2 = 1440, Index = 236
	{19, 2, 0, 0},    //Theory Freq1 = 1422, Actual Freq2 = 1440, Index = 237
	{19, 2, 0, 0},    //Theory Freq1 = 1428, Actual Freq2 = 1440, Index = 238
	{19, 2, 0, 0},    //Theory Freq1 = 1434, Actual Freq2 = 1440, Index = 239
	{19, 2, 0, 0},    //Theory Freq1 = 1440, Actual Freq2 = 1440, Index = 240
	{20, 2, 0, 0},    //Theory Freq1 = 1446, Actual Freq2 = 1512, Index = 241
	{20, 2, 0, 0},    //Theory Freq1 = 1452, Actual Freq2 = 1512, Index = 242
	{20, 2, 0, 0},    //Theory Freq1 = 1458, Actual Freq2 = 1512, Index = 243
	{20, 2, 0, 0},    //Theory Freq1 = 1464, Actual Freq2 = 1512, Index = 244
	{20, 2, 0, 0},    //Theory Freq1 = 1470, Actual Freq2 = 1512, Index = 245
	{20, 2, 0, 0},    //Theory Freq1 = 1476, Actual Freq2 = 1512, Index = 246
	{20, 2, 0, 0},    //Theory Freq1 = 1482, Actual Freq2 = 1512, Index = 247
	{20, 2, 0, 0},    //Theory Freq1 = 1488, Actual Freq2 = 1512, Index = 248
	{20, 2, 0, 0},    //Theory Freq1 = 1494, Actual Freq2 = 1512, Index = 249
	{20, 2, 0, 0},    //Theory Freq1 = 1500, Actual Freq2 = 1512, Index = 250
	{20, 2, 0, 0},    //Theory Freq1 = 1506, Actual Freq2 = 1512, Index = 251
	{20, 2, 0, 0},    //Theory Freq1 = 1512, Actual Freq2 = 1512, Index = 252
	{15, 3, 0, 0},    //Theory Freq1 = 1518, Actual Freq2 = 1536, Index = 253
	{15, 3, 0, 0},    //Theory Freq1 = 1524, Actual Freq2 = 1536, Index = 254
	{15, 3, 0, 0},    //Theory Freq1 = 1530, Actual Freq2 = 1536, Index = 255
	{15, 3, 0, 0},    //Theory Freq1 = 1536, Actual Freq2 = 1536, Index = 256
};

struct cpufreq_frequency_table sunxi_freq_tbl[] = {
	{ .frequency = 60000  , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 120000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 240000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 312000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 408000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 504000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 600000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 648000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 720000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 816000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 912000 , .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 1008000, .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 1104000, .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 1200000, .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 1344000, .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 1440000, .index = SUNXI_CLK_DIV(1, 3, 0, 0), },
	{ .frequency = 1536000, .index = SUNXI_CLK_DIV(1, 3, 0, 0), },

	/* table end */
	{ .frequency = CPUFREQ_TABLE_END,  .index = 0,              },
};

#else
struct cpufreq_frequency_table sunxi_freq_tbl[] = {
	{ .frequency = 60000  , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 120000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 240000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 312000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 408000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 504000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 600000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
#if defined(CONFIG_ARCH_SUN8IW5P1)
	{ .frequency = 648000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
#endif
	{ .frequency = 720000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 816000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 912000 , .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 1008000, .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 1104000, .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 1200000, .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 1344000, .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 1440000, .index = SUNXI_CLK_DIV(0, 0, 0, 0), },
	{ .frequency = 1536000, .index = SUNXI_CLK_DIV(0, 0, 0, 0), },

	/* table end */
	{ .frequency = CPUFREQ_TABLE_END,  .index = 0,              },
};
#endif

/*
 *check if the cpu frequency policy is valid;
 */
static int sunxi_cpufreq_verify(struct cpufreq_policy *policy)
{
	return 0;
}


#if !defined(CONFIG_ARCH_SUN8IW7P1) && !defined(CONFIG_ARCH_SUN8IW8P1)
/*
 * get the current cpu vdd;
 * return: cpu vdd, based on mv;
 */
int sunxi_cpufreq_getvolt(void)
{
	return regulator_get_voltage(cpu_vdd) / 1000;
}
#endif

/*
 * get the frequency that cpu currently is running;
 * cpu:    cpu number, all cpus use the same clock;
 * return: cpu frequency, based on khz;
 */
static unsigned int sunxi_cpufreq_get(unsigned int cpu)
{
	unsigned int current_freq = 0;
#ifdef CONFIG_DEBUG_FS
	ktime_t calltime = ktime_set(0, 0), delta, rettime;

	calltime = ktime_get();
#endif

	clk_get_rate(clk_pll);
	current_freq = clk_get_rate(clk_cpu) / 1000;

#ifdef CONFIG_DEBUG_FS
	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	cpufreq_get_time_usecs = ktime_to_ns(delta) >> 10;
#endif

	return current_freq;
}


/*
 *show cpu frequency information;
 */
static void sunxi_cpufreq_show(const char *pfx, struct sunxi_cpu_freq_t *cfg)
{
	pr_debug("%s: pll=%u, cpudiv=%u, axidiv=%u\n", pfx, cfg->pll, cfg->div.cpu_div, cfg->div.axi_div);
}


#if defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW8P1)
static int __set_cpufreq_target(struct sunxi_cpu_freq_t *target)
{
	struct pll_cpu_factor_t factor;
	struct ccu_pll_cpu_ctrl_reg tmp_pll_cpu;
	struct ccu_cpu_axi_cfg_reg tmp_cpu_axi_cfg;
	unsigned int index;

	if (!target)
		return -EINVAL;

	tmp_cpu_axi_cfg = *cpu_axi_reg;
	/* try to increase axi divide ratio
	 * the axi_div of target freq should be increase first,
	 * this is to avoid axi bus clock beyond limition.
	 */
	if (tmp_cpu_axi_cfg.axi_div < target->div.axi_div) {
		tmp_cpu_axi_cfg.axi_div = target->div.axi_div;
		*cpu_axi_reg = tmp_cpu_axi_cfg;
	}

	if (target->pll > 1536000000)
		target->pll = 1536000000;
	index = target->pll / 6000000;

	factor.factor_n = pll_cpu_table[index].factor_n;
	factor.factor_k = pll_cpu_table[index].factor_k;
	factor.factor_m = pll_cpu_table[index].factor_m;
	factor.factor_p = pll_cpu_table[index].factor_p;

	if (unlikely(sunxi_dvfs_debug))
		printk("[cpufreq] target: N:%u K:%u M:%u P:%u axi_div:%u\n",
			factor.factor_n, factor.factor_k, factor.factor_m, factor.factor_p,
			target->div.axi_div);

	tmp_pll_cpu = *pll_cpu_reg;
	/* try to increase factor p first */
	if (tmp_pll_cpu.factor_p < factor.factor_p) {
		tmp_pll_cpu.factor_p = factor.factor_p;
		*pll_cpu_reg = tmp_pll_cpu;
		udelay(10);
	}

	/* try to increase factor m first */
	if (tmp_pll_cpu.factor_m < factor.factor_m) {
		tmp_pll_cpu.factor_m = factor.factor_m;
		*pll_cpu_reg = tmp_pll_cpu;
		udelay(10);
	}

	/* write factor n & k */
	tmp_pll_cpu.factor_n = factor.factor_n;
	tmp_pll_cpu.factor_k = factor.factor_k;
	*pll_cpu_reg = tmp_pll_cpu;

	/* wait for lock change first */
	ndelay(100);

#ifdef CONFIG_EVB_PLATFORM
	/* wait for PLL CPU stable */
	while (pll_cpu_reg->lock != 1);
#endif

	//decease factor m
	if (tmp_pll_cpu.factor_m > factor.factor_m) {
		tmp_pll_cpu.factor_m = factor.factor_m;
		*pll_cpu_reg = tmp_pll_cpu;
		udelay(10);
	}

	/* decease factor p */
	if (tmp_pll_cpu.factor_p > factor.factor_p) {
		tmp_pll_cpu.factor_p = factor.factor_p;
		*pll_cpu_reg = tmp_pll_cpu;
		udelay(10);
	}

	/* try to decrease axi divide ratio */
	if (tmp_cpu_axi_cfg.axi_div > target->div.axi_div) {
		tmp_cpu_axi_cfg.axi_div = target->div.axi_div;
		*cpu_axi_reg = tmp_cpu_axi_cfg;
	}

	if (unlikely(sunxi_dvfs_debug))
		printk("[cpufreq] current: N:%u K:%u M:%u P:%u axi_div:%u\n",
			pll_cpu_reg->factor_n, pll_cpu_reg->factor_k, pll_cpu_reg->factor_m,
			pll_cpu_reg->factor_p, cpu_axi_reg->axi_div);

	return 0;
}
#endif


/*
 * adjust the frequency that cpu is currently running;
 * policy:  cpu frequency policy;
 * freq:    target frequency to be set, based on khz;
 * relation:    method for selecting the target requency;
 * return:  result, return 0 if set target frequency successed, else, return -EINVAL;
 * notes:   this function is called by the cpufreq core;
 */
static int sunxi_cpufreq_target(struct cpufreq_policy *policy, __u32 freq, __u32 relation)
{
	int ret = 0;
	unsigned int            index;
	struct sunxi_cpu_freq_t freq_cfg;
	struct cpufreq_freqs    freqs;
#ifdef CONFIG_DEBUG_FS
	ktime_t calltime = ktime_set(0, 0), delta, rettime;
#endif
#ifdef CONFIG_SMP
	int i;
#endif

	mutex_lock(&sunxi_cpu_lock);

	/* avoid repeated calls which cause a needless amout of duplicated
	 * logging output (and CPU time as the calculation process is
	 * done) */
	if (freq == last_target)
		goto out;

	if (unlikely(sunxi_dvfs_debug))
		printk("[cpufreq] request frequency is %u\n", freq);

	if (unlikely(sunxi_boot_freq_lock))
		freq = freq > cpu_freq_boot ? cpu_freq_boot : freq;

	/* try to look for a valid frequency value from cpu frequency table */
	if (cpufreq_frequency_table_target(policy, sunxi_freq_tbl, freq, relation, &index)) {
		CPUFREQ_ERR("try to look for a valid frequency for %u failed!\n", freq);
		ret = -EINVAL;
		goto out;
	}

	/* frequency is same as the value last set, need not adjust */
	if (sunxi_freq_tbl[index].frequency == last_target)
		goto out;

	freq = sunxi_freq_tbl[index].frequency;

	/* update the target frequency */
	freq_cfg.pll = sunxi_freq_tbl[index].frequency * 1000;
	freq_cfg.div = *(struct sunxi_clk_div_t *)&sunxi_freq_tbl[index].index;

	if (unlikely(sunxi_dvfs_debug))
		printk("[cpufreq] target frequency find is %u, entry %u\n", freq_cfg.pll, index);

	/* notify that cpu clock will be adjust if needed */
	if (policy) {
		freqs.cpu = policy->cpu;
		freqs.old = last_target;
		freqs.new = freq;

#ifdef CONFIG_SMP
		/* notifiers */
		for_each_cpu(i, policy->cpus) {
			freqs.cpu = i;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
		}
#else
		cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);
#endif
	}

#ifdef CONFIG_DEBUG_FS
	calltime = ktime_get();
#endif

#if defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW8P1)
	if (__set_cpufreq_target(&freq_cfg)) {
#else
	/* try to set cpu frequency */
	if (arisc_dvfs_set_cpufreq(freq, ARISC_DVFS_PLL1, ARISC_DVFS_SYN, NULL, NULL)) {
#endif
		CPUFREQ_ERR("set cpu frequency to %uMHz failed!\n", freq / 1000);
		/* set cpu frequency failed */
		if (policy) {
			freqs.cpu = policy->cpu;
			freqs.old = freqs.new;
			freqs.new = last_target;

#ifdef CONFIG_SMP
			/* notifiers */
			for_each_cpu(i, policy->cpus) {
				freqs.cpu = i;
				cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
			}
#else
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
#endif
		}

		ret = -EINVAL;
		goto out;
	}

#ifdef CONFIG_DEBUG_FS
	rettime = ktime_get();
	delta = ktime_sub(rettime, calltime);
	cpufreq_set_time_usecs = ktime_to_ns(delta) >> 10;
#endif

	/* notify that cpu clock will be adjust if needed */
	if (policy) {
#ifdef CONFIG_SMP
		/*
		 * Note that loops_per_jiffy is not updated on SMP systems in
		 * cpufreq driver. So, update the per-CPU loops_per_jiffy value
		 * on frequency transition. We need to update all dependent cpus
		 */
		for_each_cpu(i, policy->cpus) {
			per_cpu(cpu_data, i).loops_per_jiffy =
				 cpufreq_scale(per_cpu(cpu_data, i).loops_per_jiffy, freqs.old, freqs.new);
			freqs.cpu = i;
			cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
		}
#else
		cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);
#endif
	}

	last_target = freq;

#if defined(CONFIG_ARCH_SUN8IW3P1) && defined(CONFIG_DEVFREQ_DRAM_FREQ)
	if (!dramfreq_need_suspend) {
		if (freq < 100000) {
			if (__ahb_set_rate(100000000)) {
				CPUFREQ_ERR("set ahb to 100MHz failed!\n");
			}
		} else {
			if (__ahb_set_rate(200000000)) {
				CPUFREQ_ERR("set ahb to 200MHz failed!\n");
			}
		}
	}
#endif

	if (unlikely(sunxi_dvfs_debug)) {
#if !defined(CONFIG_ARCH_SUN8IW7P1) && !defined(CONFIG_ARCH_SUN8IW8P1)
		printk("[cpufreq] DVFS done! Freq[%uMHz] Volt[%umv] ok\n\n", \
			sunxi_cpufreq_get(0) / 1000, sunxi_cpufreq_getvolt());
#else
		printk("[cpufreq] DFS done! Freq[%uMHz] ok\n\n", \
			sunxi_cpufreq_get(0) / 1000);
#endif
	}


out:
	mutex_unlock(&sunxi_cpu_lock);

	return ret;
}


/*
 * get the frequency that cpu average is running;
 * cpu:    cpu number, all cpus use the same clock;
 * return: cpu frequency, based on khz;
 */
static unsigned int sunxi_cpufreq_getavg(struct cpufreq_policy *policy, unsigned int cpu)
{
	return clk_get_rate(clk_cpu) / 1000;
}


/*
 * get a valid frequency from cpu frequency table;
 * target_freq: target frequency to be judge, based on KHz;
 * return: cpu frequency, based on khz;
 */
static unsigned int __get_valid_freq(unsigned int target_freq)
{
	struct cpufreq_frequency_table *tmp = &sunxi_freq_tbl[0];

	while(tmp->frequency != CPUFREQ_TABLE_END){
		if((tmp+1)->frequency <= target_freq)
			tmp++;
		else
			break;
	}

	return tmp->frequency;
}

static int __init_vftable_syscfg(char *tbl_name, int value)
{
	script_item_u val;
	script_item_value_type_e type;
	int i ,ret = -1;
	char name[16] = {0};

	type = script_get_item(tbl_name, "LV_count", &val);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		CPUFREQ_ERR("fetch LV_count from sysconfig failed\n");
		goto fail;
	}

	table_length_syscfg = val.val;
	if(table_length_syscfg >= 16){
		CPUFREQ_ERR("LV_count from sysconfig is out of bounder\n");
		goto fail;
	}

	for (i = 1; i <= table_length_syscfg; i++){
		sprintf(name, "LV%d_freq", i);
		type = script_get_item(tbl_name, name, &val);
		if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
			CPUFREQ_ERR("get LV%d_freq from sysconfig failed\n", i);
			goto fail;
		}
		dvfs_table_syscfg[i-1].freq = val.val / 1000;

		sprintf(name, "LV%d_volt", i);
		type = script_get_item(tbl_name, name, &val);
		if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
			CPUFREQ_ERR("get LV%d_volt from sysconfig failed\n", i);
			goto fail;
		}

#ifdef CONFIG_ARCH_SUN8IW3P1
		if (value) {
			dvfs_table_syscfg[i-1].volt = val.val >= SUNXI_CPUFREQ_VOLT_LIMIT ? val.val : SUNXI_CPUFREQ_VOLT_LIMIT;
		} else {
			dvfs_table_syscfg[i-1].volt = val.val;
		}
#else
		dvfs_table_syscfg[i-1].volt = val.val;
#endif
	}

	return 0;

fail:
	return ret;
}

static void __vftable_show(void)
{
	int i;

	pr_debug("----------------CPU V-F Table-------------\n");
	for(i = 0; i < table_length_syscfg; i++){
		pr_debug("\tfrequency = %4dKHz \tvoltage = %4dmv\n", \
				dvfs_table_syscfg[i].freq, dvfs_table_syscfg[i].volt);
	}
	pr_debug("------------------------------------------\n");
}

/*
 * init cpu max/min frequency from sysconfig;
 * return: 0 - init cpu max/min successed, !0 - init cpu max/min failed;
 */
static int __init_freq_syscfg(char *tbl_name)
{
	int ret = 0;
	script_item_u max, min, boot;
	script_item_value_type_e type;

	type = script_get_item(tbl_name, "max_freq", &max);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		CPUFREQ_ERR("get cpu max frequency from sysconfig failed\n");
		ret = -1;
		goto fail;
	}
	cpu_freq_max = max.val;

	type = script_get_item(tbl_name, "min_freq", &min);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		CPUFREQ_ERR("get cpu min frequency from sysconfig failed\n");
		ret = -1;
		goto fail;
	}
	cpu_freq_min = min.val;

	type = script_get_item(tbl_name, "extremity_freq", &max);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		CPUFREQ_ERR("get cpu extremity frequency from sysconfig failed, use max_freq\n");
		max.val = cpu_freq_max;
	}
	cpu_freq_ext = max.val;

	type = script_get_item(tbl_name, "boot_freq", &boot);
	if (SCIRPT_ITEM_VALUE_TYPE_INT != type) {
		boot.val = cpu_freq_max;
	} else {
		sunxi_boot_freq_lock = 1;
	}
	cpu_freq_boot = boot.val;

	if(cpu_freq_max > SUNXI_CPUFREQ_MAX || cpu_freq_max < SUNXI_CPUFREQ_MIN
		|| cpu_freq_min < SUNXI_CPUFREQ_MIN || cpu_freq_min > SUNXI_CPUFREQ_MAX){
		CPUFREQ_ERR("cpu max or min frequency from sysconfig is more than range\n");
		ret = -1;
		goto fail;
	}

	if(cpu_freq_min > cpu_freq_max){
		CPUFREQ_ERR("cpu min frequency can not be more than cpu max frequency\n");
		ret = -1;
		goto fail;
	}

	if (cpu_freq_ext < cpu_freq_max) {
		CPUFREQ_ERR("cpu ext frequency can not be less than cpu max frequency\n");
		ret = -1;
		goto fail;
	}

	if(cpu_freq_boot > cpu_freq_max || cpu_freq_boot < cpu_freq_min){
		CPUFREQ_ERR("cpu boot frequency can not be more than cpu max frequency\n");
		ret = -1;
		goto fail;
	}

	/* get valid max/min frequency from cpu frequency table */
	cpu_freq_max = __get_valid_freq(cpu_freq_max / 1000);
	cpu_freq_min = __get_valid_freq(cpu_freq_min / 1000);
	cpu_freq_ext = __get_valid_freq(cpu_freq_ext / 1000);
	cpu_freq_boot = __get_valid_freq(cpu_freq_boot / 1000);

	return 0;

fail:
	/* use default cpu max/min frequency */
	cpu_freq_max = SUNXI_CPUFREQ_MAX / 1000;
	cpu_freq_min = SUNXI_CPUFREQ_MIN / 1000;
	cpu_freq_ext = SUNXI_CPUFREQ_MAX / 1000;
	cpu_freq_boot = cpu_freq_max;

	return ret;
}


/*
 * cpu frequency initialise a policy;
 * policy:  cpu frequency policy;
 * result:  return 0 if init ok, else, return -EINVAL;
 */
static int sunxi_cpufreq_init(struct cpufreq_policy *policy)
{
	policy->cur = sunxi_cpufreq_get(0);
	policy->min = cpu_freq_min;
	policy->max = cpu_freq_ext;
	policy->cpuinfo.min_freq = cpu_freq_min;
	policy->cpuinfo.max_freq = cpu_freq_ext;
	policy->cpuinfo.boot_freq = cpu_freq_boot;
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;

	/* feed the latency information from the cpu driver */
	policy->cpuinfo.transition_latency = SUNXI_FREQTRANS_LATENCY;
	cpufreq_frequency_table_get_attr(sunxi_freq_tbl, policy->cpu);

#ifdef CONFIG_SMP
	/*
	 * both processors share the same voltage and the same clock,
	 * but have dedicated power domains. So both cores needs to be
	 * scaled together and hence needs software co-ordination.
	 * Use cpufreq affected_cpus interface to handle this scenario.
	 */
	policy->shared_type = CPUFREQ_SHARED_TYPE_ANY;
	cpumask_copy(policy->cpus, topology_core_cpumask(policy->cpu));
#endif

	return 0;
}


/*
 * get current cpu frequency configuration;
 * cfg:     cpu frequency cofniguration;
 * return:  result;
 */
static int sunxi_cpufreq_getcur(struct sunxi_cpu_freq_t *cfg)
{
	unsigned int    freq, freq0;

	if(!cfg) {
		return -EINVAL;
	}

	cfg->pll = clk_get_rate(clk_pll);
	freq = clk_get_rate(clk_cpu);
	cfg->div.cpu_div = cfg->pll / freq;
	freq0 = clk_get_rate(clk_axi);
	cfg->div.axi_div = freq / freq0;

	return 0;
}


#if defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW8P1)
static int __set_pll_cpu_lock_time(void)
{
	unsigned int value;

	/* modify pll cpu lock time */
	value = readl(SUNXI_CCM_VBASE + 0x204);
	value &= ~(0xffff);
	value |= 0x400;
	writel(value, SUNXI_CCM_VBASE + 0x204);

	return 0;
}
#endif


#ifdef CONFIG_PM

/*
 * cpu frequency configuration suspend;
 */
static int sunxi_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

/*
 * cpu frequency configuration resume;
 */
static int sunxi_cpufreq_resume(struct cpufreq_policy *policy)
{
#if defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW8P1)
	__set_pll_cpu_lock_time();
#endif

	/* invalidate last_target setting */
	last_target = ~0;
	return 0;
}


#else   /* #ifdef CONFIG_PM */

#define sunxi_cpufreq_suspend   NULL
#define sunxi_cpufreq_resume    NULL

#endif  /* #ifdef CONFIG_PM */


static struct cpufreq_driver sunxi_cpufreq_driver = {
	.name       = "cpufreq-sunxi",
	.flags      = CPUFREQ_STICKY,
	.init       = sunxi_cpufreq_init,
	.verify     = sunxi_cpufreq_verify,
	.target     = sunxi_cpufreq_target,
	.get        = sunxi_cpufreq_get,
	.getavg     = sunxi_cpufreq_getavg,
	.suspend    = sunxi_cpufreq_suspend,
	.resume     = sunxi_cpufreq_resume,
};


/*
 * cpu frequency driver init
 */
static int __init sunxi_cpufreq_initcall(void)
{
	int ret = 0;
	char vftbl_name[16] = {0};
	int flag = 0;

#if defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW8P1)
	__set_pll_cpu_lock_time();
	pll_cpu_reg = (struct ccu_pll_cpu_ctrl_reg *)SUNXI_CCM_VBASE;
	cpu_axi_reg = (struct ccu_cpu_axi_cfg_reg *)(SUNXI_CCM_VBASE + 0x50);
#endif

#if defined(CONFIG_ARCH_SUN8IW1P1) || defined(CONFIG_ARCH_SUN8IW3P1)
	clk_pll = clk_get(NULL, PLL1_CLK);
#elif defined(CONFIG_ARCH_SUN8IW5P1) || defined(CONFIG_ARCH_SUN8IW7P1) || defined(CONFIG_ARCH_SUN8IW8P1)
	clk_pll = clk_get(NULL, PLL_CPU_CLK);
#endif

	clk_cpu = clk_get(NULL, CPU_CLK);
	clk_axi = clk_get(NULL, AXI_CLK);

	if (IS_ERR(clk_pll) || IS_ERR(clk_cpu) || IS_ERR(clk_axi)) {
		CPUFREQ_ERR("%s: could not get clock(s)\n", __func__);
		return -ENOENT;
	}

	pr_debug("%s: clocks pll=%lu,cpu=%lu,axi=%lu\n", __func__,
		   clk_get_rate(clk_pll), clk_get_rate(clk_cpu), clk_get_rate(clk_axi));

	/* initialise current frequency configuration */
	sunxi_cpufreq_getcur(&cpu_cur);
	sunxi_cpufreq_show("cur", &cpu_cur);

#if !defined(CONFIG_ARCH_SUN8IW7P1) && !defined(CONFIG_ARCH_SUN8IW8P1)
	cpu_vdd = regulator_get(NULL, SUNXI_CPUFREQ_CPUVDD);
	if (IS_ERR(cpu_vdd)) {
		CPUFREQ_ERR("%s: could not get cpu vdd\n", __func__);
		return -ENOENT;
	}
#endif

#ifdef CONFIG_ARCH_SUN8IW3P1
	writel(BIT(15), IO_ADDRESS(0x01c00024));
	if ((readl(IO_ADDRESS(0x01c00024)) >> 16) == MAGIC0) {
		sprintf(vftbl_name, "dvfs_table");
	} else {
		if (script_is_main_key_exist("dvfs_table_bak")) {
			sprintf(vftbl_name, "dvfs_table_bak");
		} else {
			sprintf(vftbl_name, "dvfs_table");
			flag = 1;
		}
	}
#else
	sprintf(vftbl_name, "dvfs_table");
#endif

	/* init cpu frequency from sysconfig */
	if(__init_freq_syscfg(vftbl_name)) {
		CPUFREQ_ERR("%s, use default cpu max/min frequency, max freq: %uMHz, min freq: %uMHz\n",
					__func__, cpu_freq_max/1000, cpu_freq_min/1000);
	}else{
		pr_debug("%s, get cpu frequency from sysconfig, max freq: %uMHz, min freq: %uMHz\n",
					__func__, cpu_freq_max/1000, cpu_freq_min/1000);
	}

	ret = __init_vftable_syscfg(vftbl_name, flag);
	if (ret) {
		CPUFREQ_ERR("%s get V-F table failed\n", __func__);
		return ret;
	} else {
		__vftable_show();
	}

	/* register cpu frequency driver */
	ret = cpufreq_register_driver(&sunxi_cpufreq_driver);

	return ret;
}
module_init(sunxi_cpufreq_initcall);

/*
 * cpu frequency driver exit
 */
static void __exit sunxi_cpufreq_exitcall(void)
{
#if !defined(CONFIG_ARCH_SUN8IW7P1) && !defined(CONFIG_ARCH_SUN8IW8P1)
	regulator_put(cpu_vdd);
#endif
	clk_put(clk_pll);
	clk_put(clk_cpu);
	clk_put(clk_axi);
	cpufreq_unregister_driver(&sunxi_cpufreq_driver);
}
module_exit(sunxi_cpufreq_exitcall);

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_cpufreq_root;

static int cpufreq_get_time_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%Ld\n", cpufreq_get_time_usecs);
	return 0;
}

static int cpufreq_get_time_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpufreq_get_time_show, inode->i_private);
}

static const struct file_operations cpufreq_get_time_fops = {
	.open = cpufreq_get_time_open,
	.read = seq_read,
};

static int cpufreq_set_time_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%Ld\n", cpufreq_set_time_usecs);
	return 0;
}

static int cpufreq_set_time_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpufreq_set_time_show, inode->i_private);
}

static const struct file_operations cpufreq_set_time_fops = {
	.open = cpufreq_set_time_open,
	.read = seq_read,
};

static int __init cpufreq_debug_init(void)
{
	int err = 0;

	debugfs_cpufreq_root = debugfs_create_dir("cpufreq", 0);
	if (!debugfs_cpufreq_root)
		return -ENOMEM;

	if (!debugfs_create_file("get_time", 0444, debugfs_cpufreq_root, NULL, &cpufreq_get_time_fops)) {
		err = -ENOMEM;
		goto out;
	}

	if (!debugfs_create_file("set_time", 0444, debugfs_cpufreq_root, NULL, &cpufreq_set_time_fops)) {
		err = -ENOMEM;
		goto out;
	}

	return 0;

out:
	debugfs_remove_recursive(debugfs_cpufreq_root);
	return err;
}

static void __exit cpufreq_debug_exit(void)
{
	debugfs_remove_recursive(debugfs_cpufreq_root);
}

late_initcall(cpufreq_debug_init);
module_exit(cpufreq_debug_exit);
#endif /* CONFIG_DEBUG_FS */

MODULE_DESCRIPTION("cpufreq driver for sunxi SOCs");
MODULE_LICENSE("GPL");
