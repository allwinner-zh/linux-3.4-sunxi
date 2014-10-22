/*
 * arch/arm/mach-sunxi/include/mach/sun8i/platsmp.h
 *
 * Copyright(c) 2013-2015 Allwinnertech Co., Ltd.
 *      http://www.allwinnertech.com
 *
 * Author: liugang <liugang@allwinnertech.com>
 *
 * sun8i smp ops header file
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __SUN8I_PLAT_SMP_H
#define __SUN8I_PLAT_SMP_H

#if defined(CONFIG_ARCH_SUN8IW6P1) || defined(CONFIG_ARCH_SUN8IW9P1)
#include <asm/smp_plat.h>
static inline void enable_cpu(int cpu_nr)
{
	unsigned int cluster;
	unsigned int cpu;
	unsigned int mpidr;
	unsigned int value;

	mpidr   = cpu_logical_map(cpu_nr);
	cpu     = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

	/* step1: power switch on */
		writel(0x00, SUNXI_R_PRCM_VBASE + SUNXI_CPU_PWR_CLAMP(cluster, cpu));
		while(0x00 != readl(SUNXI_R_PRCM_VBASE + SUNXI_CPU_PWR_CLAMP(cluster, cpu))) {
			;
		}
		mdelay(5);

		/* step2: power gating off */
		value = readl(SUNXI_R_PRCM_VBASE + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		value &= (~(0x1<<cpu));
		writel(value, SUNXI_R_PRCM_VBASE + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		mdelay(2);

		/* step3: clear reset */
		value  = readl(SUNXI_R_CPUCFG_VBASE + SUNXI_CLUSTER_PWRON_RESET(cluster));
		value |= (1<<cpu);
		writel(value, SUNXI_R_CPUCFG_VBASE + SUNXI_CLUSTER_PWRON_RESET(cluster));

		/* step4: core reset */
		value  = readl(SUNXI_CPUXCFG_VBASE + SUNXI_CPU_RST_CTRL(cluster));
		value |= (1<<cpu);
		writel(value, SUNXI_CPUXCFG_VBASE + SUNXI_CPU_RST_CTRL(cluster));

}

static inline void disable_cpu(int cpu_nr)
{
    unsigned int cluster;
	unsigned int cpu;
	unsigned int mpidr;
	unsigned int value;

	mpidr   = cpu_logical_map(cpu_nr);
	cpu     = MPIDR_AFFINITY_LEVEL(mpidr, 0);
	cluster = MPIDR_AFFINITY_LEVEL(mpidr, 1);

		/* step1: core deassert */
		value  = readl(SUNXI_CPUXCFG_VBASE + SUNXI_CPU_RST_CTRL(cluster));
		value &= ~(1 << cpu);
		writel(value, SUNXI_CPUXCFG_VBASE + SUNXI_CPU_RST_CTRL(cluster));

		/* step2: deassert */
		value  = readl(SUNXI_R_CPUCFG_VBASE + SUNXI_CLUSTER_PWRON_RESET(cluster));
		value &= ~(1 << cpu);
		writel(value, SUNXI_R_CPUCFG_VBASE + SUNXI_CLUSTER_PWRON_RESET(cluster));


		/* step3: enable power gating off */
		mdelay(2);
		value = readl(SUNXI_R_PRCM_VBASE + SUNXI_CLUSTER_PWROFF_GATING(cluster));
		value |= 0x1<<cpu;
		writel(value, SUNXI_R_PRCM_VBASE + SUNXI_CLUSTER_PWROFF_GATING(cluster));

	/* step1: power switch on */
		mdelay(5);
		writel(0xff, SUNXI_R_PRCM_VBASE + SUNXI_CPU_PWR_CLAMP(cluster, cpu));
		mdelay(1);
}

#else

static inline void enable_cpu(int cpu)
{
	u32 pwr_reg;

	/* step1: Assert nCOREPORESET LOW and hold L1RSTDISABLE LOW.
	          Ensure DBGPWRDUP is held LOW to prevent any external
	          debug access to the processor.
	*/
	/* assert cpu core reset */
	writel(0, (void *)(SUNXI_R_CPUCFG_VBASE + CPUX_RESET_CTL(cpu)));
	/* L1RSTDISABLE hold low */
	pwr_reg = readl((void *)(SUNXI_R_CPUCFG_VBASE + SUNXI_CPUCFG_GENCTL));
	pwr_reg &= ~(1<<cpu);
	writel(pwr_reg, (void *)(SUNXI_R_CPUCFG_VBASE + SUNXI_CPUCFG_GENCTL));
	udelay(10);

#if defined CONFIG_ARCH_SUN8IW1 || defined CONFIG_ARCH_SUN8IW5
	/* step2: release power clamp */
	writel(0xFE, (void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPUX_PWR_CLAMP(cpu)));
	udelay(20);
	writel(0xF8, (void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPUX_PWR_CLAMP(cpu)));
	udelay(10);
	writel(0xE0, (void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPUX_PWR_CLAMP(cpu)));
	udelay(10);
	writel(0x80, (void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPUX_PWR_CLAMP(cpu)));
	udelay(10);
	writel(0x00, (void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPUX_PWR_CLAMP(cpu)));
	udelay(20);
	while(0x00 != readl((void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPUX_PWR_CLAMP(cpu)))) {
		;
	}
#endif

	/* step3: clear power-off gating */
	pwr_reg = readl((void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPU_PWROFF_REG));
	pwr_reg &= ~(0x00000001<<cpu);
	writel(pwr_reg, (void *)(SUNXI_R_PRCM_VBASE + SUNXI_CPU_PWROFF_REG));
	udelay(20);

	/* step4: de-assert core reset */
	writel(3, (void *)(SUNXI_R_CPUCFG_VBASE + CPUX_RESET_CTL(cpu)));
}

static inline void disable_cpu(int cpu)
{
	u32 pwr_reg;

#if defined CONFIG_ARCH_SUN8IW3
	writel(0, (void *)(SUNXI_R_CPUCFG_VBASE + CPUX_RESET_CTL(cpu)));
#endif

	/* step9: set up power-off signal */
	pwr_reg = readl(IO_ADDRESS(SUNXI_R_PRCM_PBASE) + SUNXI_CPU_PWROFF_REG);
	pwr_reg |= (1<<cpu);
	writel(pwr_reg, IO_ADDRESS(SUNXI_R_PRCM_PBASE) + SUNXI_CPU_PWROFF_REG);
	udelay(20);

#if defined CONFIG_ARCH_SUN8IW1 || defined CONFIG_ARCH_SUN8IW5
	/* step10: active the power output clamp */
	writel(0xff, IO_ADDRESS(SUNXI_R_PRCM_PBASE) + SUNXI_CPUX_PWR_CLAMP(cpu));
	udelay(30);
	while(0xff != readl(IO_ADDRESS(SUNXI_R_PRCM_PBASE) + SUNXI_CPUX_PWR_CLAMP(cpu))) {
		;
	}
#endif
}

#endif

/*
 * set the sencodary cpu boot entry address.
 */
static inline void sunxi_set_secondary_entry(void *entry)
{
#if defined(CONFIG_ARCH_SUN8IW6P1) || defined(CONFIG_ARCH_SUN8IW9P1)
	writel((u32)entry, (void *)(SUNXI_R_CPUCFG_VBASE + PRIVATE_REG0));
#else
	writel((u32)entry, (void *)(SUNXI_R_CPUCFG_VBASE + SUNXI_CPUCFG_P_REG0));
#endif
}

/*
 * get the sencodary cpu boot entry address.
 */
static inline void *sunxi_get_secondary_entry(void)
{
#if defined(CONFIG_ARCH_SUN8IW6P1) || defined(CONFIG_ARCH_SUN8IW9P1)
	return (void *)readl(SUNXI_R_CPUCFG_VBASE + PRIVATE_REG0);
#else
	return (void *)readl(SUNXI_R_CPUCFG_VBASE + SUNXI_CPUCFG_P_REG0);
#endif
}

#endif /* __SUN8I_PLAT_SMP_H */
