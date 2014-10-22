#ifndef _PM_CONFIG_H
#define _PM_CONFIG_H


/*
* Copyright (c) 2011-2015 yanggq.young@allwinnertech.com
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License version 2 as published by
* the Free Software Foundation.
*/

#include "pm_def_i.h"
#include "mach/platform.h"
#include "mach/memory.h"
#include "asm-generic/sizes.h"
//#include <generated/autoconf.h>
#include "mach/irqs.h"

//#define CHECK_IC_VERSION

//#define RETURN_FROM_RESUME0_WITH_MMU    //suspend: 0xf000, resume0: 0xc010, resume1: 0xf000
//#define RETURN_FROM_RESUME0_WITH_NOMMU // suspend: 0x0000, resume0: 0x4010, resume1: 0x0000
//#define DIRECT_RETURN_FROM_SUSPEND //not support yet
#define ENTER_SUPER_STANDBY    //suspend: 0xf000, resume0: 0x4010, resume1: 0x0000
//#define ENTER_SUPER_STANDBY_WITH_NOMMU //not support yet, suspend: 0x0000, resume0: 0x4010, resume1: 0x0000
//#define WATCH_DOG_RESET

//NOTICE: only need one definiton
#define RESUME_FROM_RESUME1

#ifdef CONFIG_ARCH_SUN4I 
	#define PERMANENT_REG 		(0xf1c20d20)
	#define PERMANENT_REG_PA 	(0x01c20d20)
	#define STANDBY_STATUS_REG 		(0xf1c20d20)
	#define STANDBY_STATUS_REG_PA 		(0x01c20d20)
#elif defined(CONFIG_ARCH_SUN5I)
	#define PERMANENT_REG 		(0xF1c0123c)
	#define PERMANENT_REG_PA 	(0x01c0123c)
	#define STANDBY_STATUS_REG 		(0xf0000740)
	#define STANDBY_STATUS_REG_PA 		(0x00000740)
	//notice: the address is located in the last word of (DRAM_BACKUP_BASE_ADDR + DRAM_BACKUP_SIZE)
	#define SUN5I_STANDBY_STATUS_REG 	(DRAM_BACKUP_BASE_ADDR + (DRAM_BACKUP_SIZE<<2) -0x4)
	#define SUN5I_STANDBY_STATUS_REG_PA 	(DRAM_BACKUP_BASE_ADDR_PA + (DRAM_BACKUP_SIZE<<2) -0x4)
#elif defined(CONFIG_ARCH_SUN8IW1P1)
	#define STANDBY_STATUS_REG 	(0xf1f00100)
	#define STANDBY_STATUS_REG_PA 	(0x01f00100)
	#define STANDBY_STATUS_REG_NUM 	(6)
#elif defined(CONFIG_ARCH_SUN8IW3P1) || defined(CONFIG_ARCH_SUN8IW5P1)  
	#define STANDBY_STATUS_REG 	(0xf1f00100)
	#define STANDBY_STATUS_REG_PA 	(0x01f00100)
	#define STANDBY_STATUS_REG_NUM 	(4)
#elif defined(CONFIG_ARCH_SUN8IW6P1)
#if 1 //for real ic
	#define STANDBY_STATUS_REG_PA 	(SUNXI_R_PRCM_PBASE + 0x1f0)
	#define STANDBY_STATUS_REG 	IO_ADDRESS((STANDBY_STATUS_REG_PA))
#else
	#define STANDBY_STATUS_REG_PA 	(0x40000000 + 0x1f0)
	#define STANDBY_STATUS_REG 	(0xc0000000 + 0x1f0)
#endif
	#define STANDBY_STATUS_REG_NUM 	(4)				//reg1 - reg3 is available.

#elif defined(CONFIG_ARCH_SUN9IW1P1)
	#define STANDBY_STATUS_REG_PA 	(SUNXI_R_PRCM_PBASE + 0x1f0)
	#define STANDBY_STATUS_REG 	IO_ADDRESS((STANDBY_STATUS_REG_PA))
	#define STANDBY_STATUS_REG_NUM 	(4)				//reg1 - reg3 is available.
#endif


#if defined(CONFIG_ARCH_SUN8I) || defined(CONFIG_ARCH_SUN9IW1P1)
#define CORTEX_A7
#endif



#ifdef CONFIG_ARCH_SUN8I
/**start address for function run in sram*/
#define SRAM_FUNC_START    	(0xf0000000)
#define SRAM_FUNC_START_PA 	(0x00000000)
//for mem mapping
#define MEM_SW_VA_SRAM_BASE 	(0x00000000)
#define MEM_SW_PA_SRAM_BASE 	(0x00000000)
//dram area
#define DRAM_BASE_ADDR      	(0xc0000000)
#define DRAM_BASE_ADDR_PA	(0x40000000)
#define DRAM_TRANING_SIZE   	(16)

#define CPU_CLK_REST_DEFAULT_VAL	(0x00010000)	//SRC is HOSC.

#elif defined(CONFIG_ARCH_SUN9IW1P1)
/**start address for function run in sram*/
#define SRAM_FUNC_START    	(0xf0010000)
#define SRAM_FUNC_START_PA 	(0x00010000)
//for mem mapping
#define MEM_SW_VA_SRAM_BASE 	(0x00010000)
#define MEM_SW_PA_SRAM_BASE 	(0x00010000)
#define CPU_PLL_REST_DEFAULT_VAL	(0x02001100)
#define CPU_CLK_REST_DEFAULT_VAL	(0x00000000)	//SRC is HOSC.

#endif


#ifdef CONFIG_ARCH_SUN4I
#define INT_REG_LENGTH		((0x90+0x4)>>2)
#define GPIO_REG_LENGTH		((0x218+0x4)>>2)
#define SRAM_REG_LENGTH		((0x94+0x4)>>2)
#elif defined CONFIG_ARCH_SUN5I
#define INT_REG_LENGTH		((0x94+0x4)>>2)
#define GPIO_REG_LENGTH		((0x218+0x4)>>2)
#define SRAM_REG_LENGTH		((0x94+0x4)>>2)
#endif

//hardware resource description 
#ifdef CONFIG_ARCH_SUN8IW1P1
#define AW_LRADC01_BASE		(SUNXI_LRADC01_PBASE)
#define AW_CCM_BASE		(SUNXI_CCM_PBASE)
#define AW_CCU_UART_PA		(0x01c2006C)
#define AW_CCU_UART_RESET_PA	(0x01c202D8)

//uart&jtag para
#define AW_JTAG_PH_GPIO_PA              (0x01c20800 + 0x100)            //jtag0: PH9-PH12,      bitmap: 0x3,3330
#define AW_JTAG_PF_GPIO_PA              (0x01c20800 + 0xB4)             //jtag0: PF0,PF1,PF3,PF5        bitmap: 0x40,4044;

#define AW_UART_PH_GPIO_PA              (0x01c20800 + 0x104)            //uart0: PH20,PH21,     bitmap: 0x22,0000
#define AW_UART_PF_GPIO_PA              (0x01c20800 + 0xB4)             //uart0: PF2,PF4,               bitmap: 0x04,0400;

#define AW_JTAG_PH_CONFIG_VAL_MASK      (0x000ffff0)
#define AW_JTAG_PH_CONFIG_VAL           (0x00033330)
#define AW_JTAG_PF_CONFIG_VAL_MASK      (0x00f0f0ff)
#define AW_JTAG_PF_CONFIG_VAL           (0x00404044)

#define AW_UART_PH_CONFIG_VAL_MASK      (0x00ff0000)
#define AW_UART_PH_CONFIG_VAL           (0x00220000)
#define AW_UART_PF_CONFIG_VAL_MASK      (0x000F0F00)
#define AW_UART_PF_CONFIG_VAL           (0x00040400)

#define AW_JTAG_GPIO_PA                 (AW_JTAG_PF_GPIO_PA)
#define AW_UART_GPIO_PA                 (AW_UART_PF_GPIO_PA)
#define AW_JTAG_CONFIG_VAL_MASK         AW_JTAG_PF_CONFIG_VAL_MASK
#define AW_JTAG_CONFIG_VAL              AW_JTAG_PF_CONFIG_VAL

#define AW_RTC_BASE		(SUNXI_RTC_PBASE)
#define AW_SRAMCTRL_BASE	(SUNXI_SRAMCTRL_PBASE)
#define GPIO_REG_LENGTH		((0x278+0x4)>>2)
#define CPUS_GPIO_REG_LENGTH	((0x238+0x4)>>2)
#define SRAM_REG_LENGTH		((0x94+0x4)>>2)
#define CCU_REG_LENGTH		((0x308+0x4)>>2)

#elif defined CONFIG_ARCH_SUN8IW3P1
#define AW_LRADC01_BASE		(SUNXI_LRADC_PBASE)
#define AW_CCM_BASE		(SUNXI_CCM_PBASE)
#define AW_CCU_UART_PA		(0x01c2006C)
#define AW_CCU_UART_RESET_PA	(0x01c202D8)

//uart&jtag para
#define AW_JTAG_PH_GPIO_PA              (0x01c20800 + 0x00)            //jtag0: Pa0-Pa3,
#define AW_JTAG_PF_GPIO_PA              (0x01c20800 + 0xB4)             //jtag0: PF0,PF1,PF3,PF5        bitmap: 0x40,4044;

#define AW_UART_PH_GPIO_PA              (0x01c20800 + 0xb4)            //uart0: use pf
#define AW_UART_PF_GPIO_PA              (0x01c20800 + 0xB4)             //uart0: PF2,PF4,               bitmap: 0x04,0400;

#define AW_JTAG_PH_CONFIG_VAL_MASK      (0x0000ffff)
#define AW_JTAG_PH_CONFIG_VAL           (0x00003333)
#define AW_JTAG_PF_CONFIG_VAL_MASK      (0x00f0f0ff)
#define AW_JTAG_PF_CONFIG_VAL           (0x00303033)

#define AW_UART_PH_CONFIG_VAL_MASK      (0x000F0F00)
#define AW_UART_PH_CONFIG_VAL           (0x00030300)
#define AW_UART_PF_CONFIG_VAL_MASK      (0x000F0F00)
#define AW_UART_PF_CONFIG_VAL           (0x00030300)

#define AW_JTAG_GPIO_PA                 (AW_JTAG_PF_GPIO_PA)
#define AW_UART_GPIO_PA                 (AW_UART_PF_GPIO_PA)
#define AW_JTAG_CONFIG_VAL_MASK         AW_JTAG_PF_CONFIG_VAL_MASK
#define AW_JTAG_CONFIG_VAL              AW_JTAG_PF_CONFIG_VAL

#define AW_RTC_BASE		(SUNXI_RTC_PBASE)
#define AW_SRAMCTRL_BASE	(SUNXI_SRAMCTRL_PBASE)
#define GPIO_REG_LENGTH		((0x258+0x4)>>2)
#define CPUS_GPIO_REG_LENGTH	((0x218+0x4)>>2)
#define SRAM_REG_LENGTH		((0x94+0x4)>>2)
#define CCU_REG_LENGTH		((0x2d8+0x4)>>2)
#elif defined CONFIG_ARCH_SUN8IW5P1
#define AW_LRADC01_BASE		(SUNXI_LRADC_PBASE)
#define AW_CCM_BASE		(SUNXI_CCM_PBASE)
#define AW_CCU_UART_PA		(0x01c2006C)			//uart0 gating: bit16, 0: mask, 1: pass
#define AW_CCU_UART_RESET_PA	(0x01c202D8)			//uart0 reset: bit16, 0: reset, 1: de_assert

//uart&jtag para
#define AW_JTAG_PH_GPIO_PA              (0x01c20800 + 0x00)            //jtag0: Pa0-Pa3,
#define AW_JTAG_PF_GPIO_PA              (0x01c20800 + 0xB4)             //jtag0: PF0,PF1,PF3,PF5        bitmap: 0x40,4044;

#define AW_UART_PH_GPIO_PA              (0x01c20800 + 0xb4)            //uart0: use pf
#define AW_UART_PF_GPIO_PA              (0x01c20800 + 0xB4)             //uart0: PF2,PF4,               bitmap: 0x04,0400;

#define AW_JTAG_PH_CONFIG_VAL_MASK      (0x0000ffff)
#define AW_JTAG_PH_CONFIG_VAL           (0x00003333)
#define AW_JTAG_PF_CONFIG_VAL_MASK      (0x00f0f0ff)
#define AW_JTAG_PF_CONFIG_VAL           (0x00303033)

#define AW_UART_PH_CONFIG_VAL_MASK      (0x000F0F00)
#define AW_UART_PH_CONFIG_VAL           (0x00030300)
#define AW_UART_PF_CONFIG_VAL_MASK      (0x000F0F00)
#define AW_UART_PF_CONFIG_VAL           (0x00030300)

#define AW_JTAG_GPIO_PA                 (AW_JTAG_PF_GPIO_PA)
#define AW_UART_GPIO_PA                 (AW_UART_PF_GPIO_PA)
#define AW_JTAG_CONFIG_VAL_MASK         AW_JTAG_PF_CONFIG_VAL_MASK
#define AW_JTAG_CONFIG_VAL              AW_JTAG_PF_CONFIG_VAL

#define AW_RTC_BASE		(SUNXI_RTC_PBASE)
#define AW_SRAMCTRL_BASE	(SUNXI_SRAMCTRL_PBASE)
#define GPIO_REG_LENGTH		((0x258+0x4)>>2)
#define CPUS_GPIO_REG_LENGTH	((0x218+0x4)>>2)
#define SRAM_REG_LENGTH		((0xa4+0x4)>>2)
#define CCU_REG_LENGTH		((0x2d8+0x4)>>2)
#elif defined CONFIG_ARCH_SUN8IW6P1
#define AW_LRADC01_BASE		(0x0)				//notice: fake addr
#define AW_CCM_BASE		(SUNXI_CCM_PBASE)
#define AW_CCM_MOD_BASE		(SUNXI_CCM_PBASE)
#define AW_CCM_PIO_BUS_GATE_REG_OFFSET  (0x68)
#define AW_CCU_UART_PA		(AW_CCM_BASE + 0x6C)			//uart0 gating: bit16, 0: mask, 1: pass
#define AW_CCU_UART_RESET_PA	(AW_CCM_BASE + 0x2D8)			//uart0 reset: bit16, 0: reset, 1: de_assert

//uart&jtag para
#define AW_JTAG_PH_GPIO_PA              (0x01c20800 + 0x24)            //jtag0: Pb0-Pb3,
#define AW_JTAG_PF_GPIO_PA              (0x01c20800 + 0xB4)             //jtag0: PF0,PF1,PF3,PF5        bitmap: 0x40,4044;

#define AW_UART_PH_GPIO_PA              (0x01c20800 + 0x28)            //uart0: use pB
#define AW_UART_PF_GPIO_PA              (0x01c20800 + 0xB4)             //uart0: PF2,PF4,               bitmap: 0x04,0400;

#define AW_JTAG_PH_CONFIG_VAL_MASK      (0x0000ffff)
#define AW_JTAG_PH_CONFIG_VAL           (0x00003333)
#define AW_JTAG_PF_CONFIG_VAL_MASK      (0x00f0f0ff)
#define AW_JTAG_PF_CONFIG_VAL           (0x00303033)

#define AW_UART_PH_CONFIG_VAL_MASK      (0x00000FF0)
#define AW_UART_PH_CONFIG_VAL           (0x00000220)
#define AW_UART_PF_CONFIG_VAL_MASK      (0x000F0F00)
#define AW_UART_PF_CONFIG_VAL           (0x00030300)

#define AW_JTAG_GPIO_PA                 (AW_JTAG_PF_GPIO_PA)
#define AW_UART_GPIO_PA                 (AW_UART_PF_GPIO_PA)
#define AW_JTAG_CONFIG_VAL_MASK         AW_JTAG_PF_CONFIG_VAL_MASK
#define AW_JTAG_CONFIG_VAL              AW_JTAG_PF_CONFIG_VAL

#define AW_SPINLOCK_BASE	(SUNXI_SPINLOCK_PBASE)
#define AW_RTC_BASE		(AW_SRAM_A1_BASE)		//notice: fake addr.
#define AW_SRAMCTRL_BASE	(SUNXI_SRAMCTRL_PBASE)
#define GPIO_REG_LENGTH		((0x31c+0x4)>>2)		//0x24 - 0x31c
#define CPUS_GPIO_REG_LENGTH	((0x300+0x4)>>2)		//
#define SRAM_REG_LENGTH		((0xf0+0x4)>>2)
#define CCU_REG_LENGTH		((0x2d8+0x4)>>2)
#define CCU_MOD_CLK_AHB1_RESET_SPINLOCK	    (AW_CCM_MOD_BASE + 0x2c4)	//bit22, 1:de-assert, 0: assert.
#define CCU_CLK_NRESET			    (0x1<<22)
#define CCU_MOD_CLK_AHB1_GATING_SPINLOCK    (AW_CCM_MOD_BASE + 0x64)	//bit22,1: pass; 0: mask
#define CCU_CLK_ON			    (0x1<<22)
#elif defined CONFIG_ARCH_SUN9IW1P1
#define AW_LRADC01_BASE		(SUNXI_LRADC01_PBASE)
#define AW_CCM_BASE		(SUNXI_CCM_PLL_PBASE)
#define AW_CCM_MOD_BASE		(SUNXI_CCM_MOD_PBASE)
#define AW_SPINLOCK_BASE	(SUNXI_SPINLOCK_PBASE)

#define AW_CCU_UART_PA		(0x06000400 + 0x194)		//bit16: uart0;
#define AW_CCU_UART_RESET_PA	(0x06000400 + 0x1b4)		//bit16: uart0;
#define AW_CCM_PIO_BUS_GATE_REG_OFFSET  (0x190)

//Notice: jtag&uart_ph use the same addr, on sun9i platform.
#define AW_JTAG_PH_GPIO_PA		(0x06000800 + 0x100)		//jtag0: PH8-PH11,	bitmap: 0x2222
#define AW_UART_PF_GPIO_PA		(0x06000800 + 0xB4)		//uart0: PF2,PF4,		bitmap: 0x04,0400;

#define AW_UART_PH_GPIO_PA		(0x06000800 + 0x100)		//uart0: PH12,PH13,	bitmap: 0x22,0000
#define AW_JTAG_PF_GPIO_PA		(0x06000800 + 0xB4)		//jtag0: PF0,PF1,PF3,PF5	bitmap: 0x40,4044;

#define AW_JTAG_PH_CONFIG_VAL_MASK	(0x0000ffff)
#define AW_JTAG_PH_CONFIG_VAL		(0x00002222)
#define AW_JTAG_PF_CONFIG_VAL_MASK	(0x00f0f0ff)
#define AW_JTAG_PF_CONFIG_VAL		(0x00404044)

#define AW_UART_PH_CONFIG_VAL_MASK	(0x00ff0000)
#define AW_UART_PH_CONFIG_VAL		(0x00220000)
#define AW_UART_PF_CONFIG_VAL_MASK	(0x000F0F00)
#define AW_UART_PF_CONFIG_VAL		(0x00040400)

#define AW_JTAG_GPIO_PA			(AW_JTAG_PF_GPIO_PA)	
#define AW_UART_GPIO_PA			(AW_UART_PF_GPIO_PA)	
#define AW_JTAG_CONFIG_VAL_MASK		AW_JTAG_PF_CONFIG_VAL_MASK
#define AW_JTAG_CONFIG_VAL		AW_JTAG_PF_CONFIG_VAL

#define AW_RTC_BASE		(AW_SRAM_A1_BASE)		//notice: fake addr.
#define AW_SRAMCTRL_BASE	(SUNXI_SYS_CTRL_PBASE)
#define GPIO_REG_LENGTH		((0x324+0x4)>>2)
#define CPUS_GPIO_REG_LENGTH	((0x304+0x4)>>2)
#define SRAM_REG_LENGTH		((0xF0+0x4)>>2)
#define CCU_REG_LENGTH		((0x184+0x4)>>2)
#define CCU_MOD_REG_LENGTH	((0x1B4+0x4)>>2)
#define CCU_MOD_CLK_AHB1_RESET_SPINLOCK	    (AW_CCM_MOD_BASE + 0x1a4)
#define CCU_CLK_NRESET			    (0x1<<22)
#define CCU_MOD_CLK_AHB1_GATING_SPINLOCK    (AW_CCM_MOD_BASE + 0x184)
#define CCU_CLK_ON			    (0x1<<22)

#endif

//interrupt src definition.
#if defined(CONFIG_ARCH_SUN9IW1P1)
#define AW_IRQ_TIMER1		(0)
#define AW_IRQ_TOUCHPANEL	(0)
#define AW_IRQ_LRADC		(0)
#define AW_IRQ_NMI			(0)
#elif defined(CONFIG_ARCH_SUN8IW1P1)
#define AW_IRQ_TIMER1		(SUNXI_IRQ_TIMER1	)
#define AW_IRQ_TOUCHPANEL	(SUNXI_IRQ_TOUCHPANEL   )
#define AW_IRQ_LRADC		(SUNXI_IRQ_LRADC        )
#define AW_IRQ_NMI			(SUNXI_IRQ_NMI          )
#elif defined(CONFIG_ARCH_SUN8IW3P1)
#define AW_IRQ_TIMER1		(SUNXI_IRQ_TIMER1	)
#define AW_IRQ_TOUCHPANEL	(0)
#define AW_IRQ_LRADC		(SUNXI_IRQ_LRADC        )
#define AW_IRQ_NMI			(SUNXI_IRQ_NMI          )
#elif defined(CONFIG_ARCH_SUN8IW5P1)
#define AW_IRQ_TIMER1		(SUNXI_IRQ_TIMER1	)
#define AW_IRQ_TOUCHPANEL	(0)
#define AW_IRQ_LRADC		(SUNXI_IRQ_LRADC        )
#define AW_IRQ_NMI		(SUNXI_IRQ_NMI          )
#elif defined(CONFIG_ARCH_SUN8IW6P1)
#define AW_IRQ_TIMER1		(SUNXI_IRQ_TIMER1	)
#define AW_IRQ_TOUCHPANEL	(0)
#define AW_IRQ_LRADC		(SUNXI_IRQ_LRADC        )
#define AW_IRQ_NMI		(SUNXI_IRQ_NMI          )
#endif

//platform independant src config.
#define AW_IRQ_TIMER0		(SUNXI_IRQ_TIMER0	)
#define AW_IRQ_MBOX		(SUNXI_IRQ_MBOX         )

#define AW_SRAM_A1_BASE		(SUNXI_SRAM_A1_PBASE)
#define AW_SRAM_A2_BASE		(SUNXI_SRAM_A2_PBASE)
#define AW_MSGBOX_BASE		(SUNXI_MSGBOX_PBASE)
#define AW_SPINLOCK_BASE	(SUNXI_SPINLOCK_PBASE)
#define AW_PIO_BASE		(SUNXI_PIO_PBASE)
#define AW_R_PRCM_BASE		(SUNXI_R_PRCM_PBASE)
#define AW_R_CPUCFG_BASE	(SUNXI_R_CPUCFG_PBASE)
#define AW_UART0_BASE		(SUNXI_UART0_PBASE)
#define AW_R_PIO_BASE		(SUNXI_R_PIO_PBASE)

#define AW_TWI0_BASE		(SUNXI_TWI0_PBASE)
#define AW_TWI1_BASE		(SUNXI_TWI1_PBASE)
#define AW_TWI2_BASE		(SUNXI_TWI2_PBASE)
#define AW_CPUCFG_P_REG0	(SUNXI_CPUCFG_P_REG0)
#define AW_CPUCFG_GENCTL	(SUNXI_CPUCFG_GENCTL)
#define AW_CPUX_PWR_CLAMP(x)	(SUNXI_CPUX_PWR_CLAMP(x))
#define AW_CPUX_PWR_CLAMP_STATUS(x)	(SUNXI_CPUX_PWR_CLAMP_STATUS(x))
#define AW_CPU_PWROFF_REG	(SUNXI_CPU_PWROFF_REG)

#define SRAM_CTRL_REG1_ADDR_PA 0x01c00004
#define SRAM_CTRL_REG1_ADDR_VA IO_ADDRESS(SRAM_CTRL_REG1_ADDR_PA)
#define DRAM_MEM_PARA_INFO_PA			(SUPER_STANDBY_MEM_BASE)	//0x43010000, 0x43010000+2k;
#define DRAM_MEM_PARA_INFO_SIZE			((SUPER_STANDBY_MEM_SIZE)>>1) 	//DRAM_MEM_PARA_INFO_SIZE = 1K bytes. 

#define DRAM_EXTENDED_STANDBY_INFO_PA		(DRAM_MEM_PARA_INFO_PA + DRAM_MEM_PARA_INFO_SIZE)
#define DRAM_EXTENDED_STANDBY_INFO_SIZE		((SUPER_STANDBY_MEM_SIZE)>>1)

#define RUNTIME_CONTEXT_SIZE 		(14) //r0-r13

#define DRAM_COMPARE_DATA_ADDR 		(0xc0100000) //1Mbytes offset
#define DRAM_COMPARE_SIZE 		(0x10000) //?


#define __AC(X,Y)		(X##Y)
#define _AC(X,Y)		__AC(X,Y)
#define _AT(T,X)		((T)(X))
#define UL(x) 			_AC(x, UL)

#define SUSPEND_FREQ 		(720000)	//720M
#define SUSPEND_DELAY_MS 	(10)


/**-----------------------------stack point address in sram-----------------------------------------*/
#define SP_IN_SRAM		0xf0003ffc //16k
#define SP_IN_SRAM_PA		0x00003ffc //16k
#define SP_IN_SRAM_START	(SRAM_FUNC_START_PA | 0x3c00) //15k  
#endif /*_PM_CONFIG_H*/
