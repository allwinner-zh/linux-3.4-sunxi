/*
*********************************************************************************************************
*                                                    LINUX-KERNEL
*                                        AllWinner Linux Platform Develop Kits
*                                                   Kernel Module
*
*                                    (c) Copyright 2006-2011, kevin.z China
*                                             All Rights Reserved
*
* File    : standby_clock.c
* By      : kevin.z
* Version : v1.0
* Date    : 2011-5-31 13:40
* Descript: ccmu process for platform standby;
* Update  : date                auther      ver     notes
*********************************************************************************************************
*/

#include "standby_i.h"

#define readb(addr)		(*((volatile unsigned char  *)(addr)))
#define readw(addr)		(*((volatile unsigned short *)(addr)))
#define readl(addr)		(*((volatile unsigned long  *)(addr)))
#define writeb(v, addr)	(*((volatile unsigned char  *)(addr)) = (unsigned char)(v))
#define writew(v, addr)	(*((volatile unsigned short *)(addr)) = (unsigned short)(v))
#define writel(v, addr)	(*((volatile unsigned long  *)(addr)) = (unsigned long)(v))

static void *r_prcm;
static __ccmu_reg_list_t   *CmuReg;
__u32   cpu_ms_loopcnt;

//==============================================================================
// CLOCK SET FOR SYSTEM STANDBY
//==============================================================================



#ifdef CONFIG_ARCH_SUN8I
static __ccmu_pll1_reg0000_t		CmuReg_Pll1Ctl_tmp;
static __ccmu_sysclk_ratio_reg0050_t	CmuReg_SysClkDiv_tmp;

/*
*********************************************************************************************************
*                           standby_clk_init
*
*Description: ccu init for platform standby
*
*Arguments  : none
*
*Return     : result,
*
*Notes      :
*
*********************************************************************************************************
*/
__s32 standby_clk_init(void)
{
    r_prcm = (void *)IO_ADDRESS(AW_R_PRCM_BASE);
    CmuReg = (__ccmu_reg_list_t   *)IO_ADDRESS(AW_CCM_BASE);
    

    return 0;
}


/*
*********************************************************************************************************
*                           standby_clk_exit
*
*Description: ccu exit for platform standby
*
*Arguments  : none
*
*Return     : result,
*
*Notes      :
*
*********************************************************************************************************
*/
__s32 standby_clk_exit(void)
{

    return 0;
}

/*
*********************************************************************************************************
*                                     standby_clk_core2hosc
*
* Description: switch core clock to 24M high osc.
*
* Arguments  : none
*
*********************************************************************************************************
*/
void standby_clk_core2hosc(void)
{	
    CmuReg_SysClkDiv_tmp.dwval = CmuReg->SysClkDiv.dwval; 
    CmuReg_SysClkDiv_tmp.bits.CpuClkSrc = 1;
    CmuReg->SysClkDiv.dwval = CmuReg_SysClkDiv_tmp.dwval;
    return ;
}


/*
*********************************************************************************************************
*                                     standby_clk_ldoenable
*
* Description: enable LDO, ld01, hosc.
*
* Arguments  : none
*
*********************************************************************************************************
*/
void standby_clk_ldoenable(void)
{
	//cpus power domain, offset 0x44, how to enable?
	__u32 tmp;
	tmp = readl(r_prcm + PLL_CTRL_REG1_OFFSET );
	tmp &= ~(0xff000000);
	tmp |= (0xa7000000);
	writel(tmp, r_prcm + PLL_CTRL_REG1_OFFSET);

	//enalbe ldo, ldo1,crystal
	tmp = readl(r_prcm + PLL_CTRL_REG1_OFFSET );
	tmp &= ~(0x00000007);
	tmp |= (0x00000007);
	writel(tmp, r_prcm + PLL_CTRL_REG1_OFFSET);

	//disable change.
	tmp = readl(r_prcm + PLL_CTRL_REG1_OFFSET );
	tmp &= ~(0xff000000);
	writel(tmp, r_prcm + PLL_CTRL_REG1_OFFSET);
	
	return ;
}

/*
*********************************************************************************************************
*                                     standby_clk_pll1enable
*
* Description: enable pll1.
*
* Arguments  : none
*
*********************************************************************************************************
*/
void standby_clk_pll1enable(void)
{
    CmuReg_Pll1Ctl_tmp.dwval = CmuReg->Pll1Ctl.dwval;
    CmuReg_Pll1Ctl_tmp.bits.PLLEn = 1;
    CmuReg->Pll1Ctl.dwval = CmuReg_Pll1Ctl_tmp.dwval;
    return ;
}
#endif

#ifdef CONFIG_ARCH_SUN9IW1
/*
*********************************************************************************************************
*                           standby_clk_init
*
*Description: ccu init for platform standby
*
*Arguments  : none
*
*Return     : result,
*
*Notes      :
*
*********************************************************************************************************
*/
__s32 standby_clk_init(void)
{
   

    return 0;
}


/*
*********************************************************************************************************
*                           standby_clk_exit
*
*Description: ccu exit for platform standby
*
*Arguments  : none
*
*Return     : result,
*
*Notes      :
*
*********************************************************************************************************
*/
__s32 standby_clk_exit(void)
{

    return 0;
}

/*
*********************************************************************************************************
*                                     standby_clk_core2hosc
*
* Description: switch core clock to 24M high osc.
*
* Arguments  : none
*
*********************************************************************************************************
*/
void standby_clk_core2hosc(void)
{	
	return ;
}


/*
*********************************************************************************************************
*                                     standby_clk_ldoenable
*
* Description: enable LDO, ld01, hosc.
*
* Arguments  : none
*
*********************************************************************************************************
*/
void standby_clk_ldoenable(void)
{
	
	return ;
}

/*
*********************************************************************************************************
*                                     standby_clk_pll1enable
*
* Description: enable pll1.
*
* Arguments  : none
*
*********************************************************************************************************
*/
void standby_clk_pll1enable(void)
{
	return ;
}
#endif

