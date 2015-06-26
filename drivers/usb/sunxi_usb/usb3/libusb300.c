#include "osal.h"
#include <linux/delay.h>

int (*usb300_main)(int type, uintptr_t* para) = NULL;

void bsp_usb300_init(void)
{
	memcpy((void *)USB300_MAIN_START, (void *)usb300_mc_table, sizeof(usb300_mc_table));

	usb300_main = (int (*)(int type, uintptr_t* para))USB300_MAIN_START;

	return;
}

int bsp_usb300_main(unsigned int type, uintptr_t* para)
{
	int ret = -1;

	if (usb300_main)
		ret = usb300_main(type, para);

	return ret;
}

/*core*/
unsigned int get_sunxi_gctl_regs(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(GET_SUNXI_GCTL_REGS_T, para);

}
unsigned int get_sunxi_dctl_tstctrl_mark(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(GET_SUNXI_DCTL_TSTCTRL_MARK_T, para);
}

unsigned int get_sunxi_dsts_usblnkst_state(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(GET_SUNXI_DSTS_USBLNKST_STATE_T, para);
}

int sunxi_otgc_open_phy(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_OTGC_OPEN_PHY_T, para);
}

int sunxi_otgc_close_phy(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_OTGC_CLOSE_PHY_T, para);
}

void sunxi_set_mode_ex(void __iomem *regs, unsigned int mode)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)mode;
	bsp_usb300_main(SUNXI_SET_MODE_EX_T, para);
}

void sunxi_core_soft_reset(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	bsp_usb300_main(SUNXI_CORE_SOFT_RESET_SET_T, para);
	msleep(5);
	bsp_usb300_main(SUNXI_CORE_SOFT_RESET_CLEAR_T, para);
}

void sunxi_set_gevntadrlo(void __iomem *regs, int n, unsigned int value)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)n;
	para[2] = (uintptr_t)value;
	bsp_usb300_main(SUNXI_SET_GEVNTADRLO_T, para);
}


void sunxi_set_gevntadrhi(void __iomem *regs, int n, unsigned int value)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)n;
	para[2] = (uintptr_t)value;
	bsp_usb300_main(SUNXI_SET_GEVNTADRHI_T, para);
}

void sunxi_set_gevntsiz(void __iomem *regs, int n, unsigned int value)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)n;
	para[2] = (uintptr_t)value;
	bsp_usb300_main(SUNXI_SET_GEVNTSIZ_T, para);
}

void sunxi_set_gevntcount(void __iomem *regs, int n, unsigned int value)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)n;
	para[2] = (uintptr_t)value;
	bsp_usb300_main(SUNXI_SET_GEVNTCOUNT_T, para);
}

void sunxi_event_buffers_cleanup_ex(void __iomem *regs , unsigned int n)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)n;
	bsp_usb300_main(SUNXI_EVENT_BUFFERS_CLEANUP_EX_T, para);
}

unsigned int sunxi_get_hwparams0(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS0_T, para);
}

unsigned int sunxi_get_hwparams1(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS1_T, para);
}

unsigned int sunxi_get_hwparams2(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS2_T, para);
}

unsigned int sunxi_get_hwparams3(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS3_T, para);
}

unsigned int sunxi_get_hwparams4(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS4_T, para);
}

unsigned int sunxi_get_hwparams5(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS5_T, para);
}

unsigned int sunxi_get_hwparams6(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS6_T, para);
}

unsigned int sunxi_get_hwparams7(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS7_T, para);
}

unsigned int sunxi_get_hwparams8(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_HWPARAMS8_T, para);
}

unsigned int sunxi_get_gsnpsid(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_GSNPSID_T, para);
}

unsigned int sunxi_get_dctl(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_DCTL_T, para);
}

void sunxi_set_dctl(void __iomem *regs, unsigned int value)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)value;
	bsp_usb300_main(SUNXI_SET_DCTL_T, para);
}

void sunxi_set_dctl_csftrst(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	bsp_usb300_main(SUNXI_SET_DCTL_CSFTRST_T, para);
}

unsigned int sunxi_is_dctl_csftrst(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_IS_DCTL_CSFTRST_T, para);
}

unsigned int sunxi_get_gctl(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_GCTL_T, para);
}

void sunxi_gctl_int(void __iomem *regs, unsigned int value, unsigned int revision)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)value;
	para[2] = (uintptr_t)revision;
	bsp_usb300_main(SUNXI_GCTL_INT_T, para);
}


/*gadget*/
int sunxi_gadget_enable_all_irq(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GADGET_ENABLE_ALL_IRQ_T, para);
}

void disable_sunxi_all_irq(void __iomem *regs)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	bsp_usb300_main(DISABLE_SUNXI_ALL_IRQ_T, para);
}

unsigned int sunxi_get_ram1_depth(unsigned int value)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)value;
	return bsp_usb300_main(SUNXI_GET_RAM1_DEPTH_T, para);
}

unsigned int sunxi_get_mdwidth(unsigned int value)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)value;
	return bsp_usb300_main(SUNXI_GET_MDWIDTH_T, para);
}

void sunxi_set_gtxfifosiz(void __iomem *regs, unsigned int fifo_number,  unsigned int fifo_size)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)fifo_number;
	para[2] = (uintptr_t)fifo_size;
	bsp_usb300_main(SUNXI_SET_GTXFIFOSIZ_T, para);
}

const char *sunxi_gadget_ep_cmd_string(u8 cmd)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (u8)cmd;
	return (const char *)bsp_usb300_main(SUNXI_GADGET_EP_CMD_STRING_T, para);
}


void sunxi_set_param(void __iomem *regs, unsigned ep,  unsigned int param0, unsigned int param1, unsigned int param2)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)ep;
	para[2] = (uintptr_t)param0;
	para[3] = (uintptr_t)param1;
	para[4] = (uintptr_t)param2;
	bsp_usb300_main(SUNXI_SET_PARAM_T, para);
}

int sunxi_send_gadget_ep_cmd_ex(void __iomem *regs, unsigned ep, unsigned cmd)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)ep;
	para[2] = (uintptr_t)cmd;
	return bsp_usb300_main(SUNXI_SEND_GADGET_EP_CMD_EX_T, para);
}

unsigned int sunxi_get_param0(int type, int maxp, int maxburst)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)type;
	para[2] = (uintptr_t)maxp;
	para[3] = (uintptr_t)maxburst;
	return bsp_usb300_main(SUNXI_GET_PARAM0_T, para);
}

unsigned int sunxi_set_param1_en(void)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	return bsp_usb300_main(SUNXI_SET_PARAM1_EN_T, para);
}

unsigned int sunxi_set_param1_bulk(void)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	return bsp_usb300_main(SUNXI_SET_PARAM1_BULK_T, para);
}

unsigned int sunxi_set_param1_iso(void)
{
	uintptr_t para[5];

	memset(para, 0, 5*sizeof(uintptr_t));
	return bsp_usb300_main(SUNXI_SET_PARAM1_ISO_T, para);
}

unsigned int  sunxi_depcfg_ep_number(u8 number)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (u8)number;
	return bsp_usb300_main(SUNXI_DEPCFG_EP_NUMBER_T, para);
}

unsigned int  sunxi_depcfg_fifo_number(u8 number)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (u8)number;
	return bsp_usb300_main(SUNXI_DEPCFG_FIFO_NUMBER_T, para);
}

unsigned int  sunxi_depcfg_binterval(u8 bInterval)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (u8)bInterval;
	return bsp_usb300_main(SUNXI_DEPCFG_BINTERVAL_T, para);
}

unsigned int  sunxi_depcfg_num_xfer_res(unsigned int value)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)value;
	return bsp_usb300_main(SUNXI_DEPCFG_NUM_XFER_RES_T, para);
}

void sunxi_enable_dalepena_ep(void __iomem *regs, u8 number)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));

	para[0] = (uintptr_t)regs;
	para[1] = (u8)number;
	bsp_usb300_main(SUNXI_ENABLE_DALEPENA_EP_T, para);
	return;
}

void sunxi_disanble_dalepena_ep(void __iomem *regs, u8 number)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));

	para[0] = (uintptr_t)regs;
	para[1] = (u8)number;
	bsp_usb300_main(SUNXI_DISANBLE_DALEPENA_EP_T, para);
}

void sunxi_set_speed(void __iomem *regs, unsigned int speed)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)speed;
	bsp_usb300_main(SUNXI_SET_SPEED_T, para);
}

unsigned int sunxi_gadget_get_frame_ex(void __iomem *regs)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GADGET_GET_FRAME_EX_T, para);
}

unsigned int sunxi_get_dsts(void __iomem *regs)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_DSTS_T, para);
}

unsigned int sunxi_get_speed(void __iomem *regs)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_SPEED_T, para);
}

unsigned int sunxi_is_superspeed(void __iomem *regs)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_IS_SUPERSPEED_T, para);
}

unsigned int sunxi_get_usblnkst(void __iomem *regs)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	return bsp_usb300_main(SUNXI_GET_USBLNKST_T, para);
}

void sunxi_clear_ulstchngreq(void __iomem *regs, unsigned int reg)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)reg;
	bsp_usb300_main(SUNXI_CLEAR_ULSTCHNGREQ_T, para);
}

unsigned int sunxi_dsts_usblnkst(unsigned int reg)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)reg;
	return bsp_usb300_main(SUNXI_DSTS_USBLNKST_T, para);
}

void sunxi_gadget_run_stop_ex(void __iomem *regs, int is_on)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)is_on;
	bsp_usb300_main(SUNXI_GADGET_RUN_STOP_EX_T, para);
}

void sunxi_gadget_usb3_phy_power_ex(void __iomem *regs, int on)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)on;
	bsp_usb300_main(SUNXI_GADGET_USB3_PHY_POWER_EX_T, para);
}

void sunxi_gadget_usb2_phy_power_ex(void __iomem *regs, int on)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)on;
	bsp_usb300_main(SUNXI_GADGET_USB2_PHY_POWER_EX_T, para);
}

void sunxi_clear_dcfg_tstctrl_mask(void __iomem *regs)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	bsp_usb300_main(SUNXI_CLEAR_DCFG_TSTCTRL_MASK_T, para);
}

void sunxi_clear_dcfg_devaddr_mask(void __iomem *regs)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	bsp_usb300_main(SUNXI_CLEAR_DCFG_DEVADDR_MASK_T, para);
}

void sunxi_update_ram_clk_sel_ex(void __iomem *regs, unsigned int speed)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)speed;
	bsp_usb300_main(SUNXI_UPDATE_RAM_CLK_SEL_EX_T, para);
}

unsigned int  sunxi_link_state_u1u2(void __iomem *regs, unsigned int u1u2)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)u1u2;
	return bsp_usb300_main(SUNXI_LINK_STATE_U1U2_T, para);
}

unsigned int sunxi_get_gevntcount(void __iomem *regs, unsigned int buf)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)buf;
	return bsp_usb300_main(SUNXI_GET_GEVNTCOUNT_T, para);
}

void sunxi_set_dcfg_devaddr(void __iomem *regs, unsigned int value)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)value;
	bsp_usb300_main(SUNXI_SET_DCFG_DEVADDR_T, para);
}

unsigned int sunxi_gadget_ep_get_transfer_index(void __iomem *regs, u8 number)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)number;
	return bsp_usb300_main(SUNXI_GADGET_EP_GET_TRANSFER_INDEX_T, para);
}

const char *sunxi_gadget_event_string(u8 event)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (u8)event;
	return (const char *)bsp_usb300_main(SUNXI_GADGET_EVENT_STRING_T, para);
}

const char *sunxi_ep_event_string(u8 event)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (u8)event;
	return (const char *)bsp_usb300_main(SUNXI_EP_EVENT_STRING_T, para);
}

unsigned int sunxi_is_revision_183A(unsigned int revision)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)revision;
	return bsp_usb300_main(SUNXI_IS_REVISION_183A_T, para);
}

unsigned int sunxi_is_revision_188A(unsigned int revision)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)revision;
	return bsp_usb300_main(SUNXI_IS_REVISION_188A_T, para);
}

unsigned int sunxi_is_revision_190A(unsigned int revision)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[1] = (uintptr_t)revision;
	return bsp_usb300_main(SUNXI_IS_REVISION_190A_T, para);
}

int sunxi_gadget_set_test_mode(void __iomem *regs, int mode)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)mode;
	return bsp_usb300_main(SUNXI_GADGET_SET_TEST_MODE_T, para);
}

int sunxi_gadget_set_link_state(void __iomem *regs, unsigned int state)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	para[0] = (uintptr_t)regs;
	para[1] = (uintptr_t)state;
	return bsp_usb300_main(SUNXI_GADGET_SET_LINK_STATE_T, para);
}

int sunxi_otgc_base(void)
{
	uintptr_t para[5];
	memset(para, 0, 5*sizeof(uintptr_t));
	return bsp_usb300_main(SUNXI_OTGC_BASE_T, para);
}



