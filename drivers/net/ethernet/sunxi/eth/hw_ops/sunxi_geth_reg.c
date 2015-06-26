/*******************************************************************************
 * Copyright © 2012-2014, Shuge
 *		Author: Sugar <shugeLinux@gmail.com>
 *
 * This file is provided under a dual BSD/GPL license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 ********************************************************************************/

#include "sunxi_geth_reg.h"
#include "../sunxi_geth_status.h"

#ifdef DEBUG
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/printk.h>
#define sunxi_printk(fmt, ...)	printk(KERN_ERR fmt, #__VA_ARGS__)
#else
#define sunxi_printk(fmt, ...) {}
#endif

#define writel(v,a)	(*(volatile unsigned int *)(a) = (v))
#define readl(a)	(*(volatile unsigned int *)(a))

#define DISCARD_FRAME	-1
#define GOOD_FRAME	0
#define CSUM_NONE	2
#define LLC_SNAP	4

struct geth_ops {
	/* Core operations */
	int (*mac_reset)(void *base, void (*delay)(int), int n);
	int (*mac_init)(void *base, int txmode, int rxmode);
	void (*set_umac)(void *base, unsigned char *addr, unsigned int index);
	void (*hash_filter)(void *iobase, unsigned long low, unsigned long high);
	void (*set_filter)(void *iobase, unsigned flags);
	void (*get_umac)(void *base, unsigned char *addr, unsigned int index);
	void (*set_link_mode)(void *base, int duplex, int speed);
	void (*flow_ctrl)(void *base, int duplex, int fc, int pause);
	void (*rx_tx_en)(void *base);
	void (*rx_tx_dis)(void *base);
	int (*mdio_read)(void *base, int phyaddr, int phyreg);
	int (*mdio_write)(void *base, int phyaddr, int phyreg, unsigned short data);
	int (*mdio_reset)(void *base);
	void (*mac_loopback)(void *base, int enable);

	/* Descriptor operations */
	void (*buf_set)(struct dma_desc *desc, unsigned long paddr, int len);
	int (*buf_get_addr)(struct dma_desc *desc);
	int (*buf_get_len)(struct dma_desc *desc);
	void (*set_own)(struct dma_desc *desc);
	int (*get_own)(struct dma_desc *desc);
	void (*desc_init)(struct dma_desc *desc);
	int (*rx_frame_len)(struct dma_desc *desc);
	int (*get_rx_status)(struct dma_desc *desc, struct geth_extra_stats *x);
	int (*get_tx_status)(struct dma_desc *desc, struct geth_extra_stats *x);
	void (*tx_close)(struct dma_desc *first, struct dma_desc *end, int csum);
	int (*get_tx_ls) (struct dma_desc *p);

	/* DMA operations */
	int (*dma_init)(void *base);
	void (*dma_int_dis)(void *base);
	void (*dma_int_en)(void *base);
	int (*int_status)(void *base, struct geth_extra_stats *x);
	void (*start_tx)(void *iobase, unsigned long txbase);
	void (*stop_tx)(void *iobase);
	void (*poll_tx)(void *iobase);
	void (*start_rx)(void *iobase, unsigned long rxbase);
	void (*stop_rx)(void *iobase);
	void (*poll_rx)(void *iobase);
};

struct gethdev {
	void *iobase;
	unsigned int ver;
	unsigned int mdc_div;
	struct geth_ops *ops;
};

static struct gethdev hwdev;

/******************************************************************************
 *	1633 & 1639 & 1651 operations
 *****************************************************************************/
static int mac_reset_v1(void *iobase, void (*mdelay)(int), int n)
{
	unsigned int value;

	/* DMA SW reset */
	value = readl(iobase + GDMA_BUS_MODE);
	value |= SOFT_RESET;
	writel(value, iobase + GDMA_BUS_MODE);

	mdelay(n);

	return !!(readl(iobase + GDMA_BUS_MODE) & SOFT_RESET);
}

static int mac_init_v1(void *base, int txmode, int rxmode)
{
	unsigned int value; 

	hwdev.ops->dma_init(base);

	/* Initialize the core component */
	value = readl(base + GMAC_CONTROL);
	value |= GMAC_CORE_INIT | GMAC_CTL_IPC;	/* Add support checksum & CRC */
	writel(value, base + GMAC_CONTROL);

	/* Mask GMAC interrupts */
	writel(0x207, base + GMAC_INT_MASK);
	//writel((4 << 20), base + GMAC_GMII_ADDR);

	/* Set the Rx&Tx mode */
	value = readl(base + GDMA_OP_MODE);
	if (txmode == SF_DMA_MODE) {
		/* Transmit COE type 2 cannot be done in cut-through mode. */
		value |= OP_MODE_TSF;
		/* Operating on second frame increase the performance
		 * especially when transmit store-and-forward is used.*/
		value |= OP_MODE_OSF;
	} else {
		value &= ~OP_MODE_TSF;
		value &= OP_MODE_TC_TX_MASK;
		/* Set the transmit threshold */
		if (txmode <= 32)
			value |= OP_MODE_TTC_32;
		else if (txmode <= 64)
			value |= OP_MODE_TTC_64;
		else if (txmode <= 128)
			value |= OP_MODE_TTC_128;
		else if (txmode <= 192)
			value |= OP_MODE_TTC_192;
		else
			value |= OP_MODE_TTC_256;
	}

	if (rxmode == SF_DMA_MODE) {
		value |= OP_MODE_RSF;
	} else {
		value &= ~OP_MODE_RSF;
		value &= OP_MODE_TC_RX_MASK;
		if (rxmode <= 32)
			value |= OP_MODE_RTC_32;
		else if (rxmode <= 64)
			value |= OP_MODE_RTC_64;
		else if (rxmode <= 96)
			value |= OP_MODE_RTC_96;
		else
			value |= OP_MODE_RTC_128;
	}

	writel(value, base + GDMA_OP_MODE);

	return 0;
}

static void hash_filter_v1(void *iobase, unsigned long low, unsigned long high)
{
	writel(high, iobase + GMAC_HASH_HIGH);
	writel(low, iobase + GMAC_HASH_LOW);
}

static void set_filter_v1(void *iobase, unsigned flags)
{
	writel(flags, iobase + GMAC_FRAME_FILTER);
}


static void set_umac_v1(void *base, unsigned char *addr, unsigned int index)
{
	unsigned long data;

	data = (addr[5] << 8) | addr[4];
	writel(data, base + GMAC_ADDR_HI(index));
	data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	writel(data, base + GMAC_ADDR_LO(index));
}

static void get_umac_v1(void *base, unsigned char *addr, unsigned int index)
{
	unsigned int hi_addr, lo_addr;

	/* Read the MAC address from the hardware */
	hi_addr = readl(base + GMAC_ADDR_HI(index));
	lo_addr = readl(base + GMAC_ADDR_LO(index));

	/* Extract the MAC address from the high and low words */
	addr[0] = lo_addr & 0xff;
	addr[1] = (lo_addr >> 8) & 0xff;
	addr[2] = (lo_addr >> 16) & 0xff;
	addr[3] = (lo_addr >> 24) & 0xff;
	addr[4] = hi_addr & 0xff;
	addr[5] = (hi_addr >> 8) & 0xff;
}

static int mdio_read_v1(void *iobase, int phyaddr, int phyreg)
{
	unsigned int value = 0;

	value |= ((hwdev.mdc_div & 0x07) << 20);
	value |= (((phyaddr << 11) & (0x0000F800)) |
			((phyreg << 6) & (0x000007C0)) |
			MII_BUSY);

	while (((readl(iobase + GMAC_GMII_ADDR)) & MII_BUSY) == 1);

	writel(value, iobase + GMAC_GMII_ADDR);
	while (((readl(iobase + GMAC_GMII_ADDR)) & MII_BUSY) == 1);

	return (int)readl(iobase + GMAC_GMII_DATA);
}

static int mdio_write_v1(void *iobase, int phyaddr, int phyreg, unsigned short data)
{
	unsigned int value = 0;

	value |= ((hwdev.mdc_div & 0x07) << 20);
	value |= (((phyaddr << 11) & (0x0000F800)) |
			((phyreg << 6) & (0x000007C0))) |
			MII_WRITE | MII_BUSY;

	/* Wait until any existing MII operation is complete */
	while (((readl(iobase + GMAC_GMII_ADDR)) & MII_BUSY) == 1);

	/* Set the MII address register to write */
	writel(data, iobase + GMAC_GMII_DATA);
	writel(value, iobase + GMAC_GMII_ADDR);

	/* Wait until any existing MII operation is complete */
	while (((readl(iobase + GMAC_GMII_ADDR)) & MII_BUSY) == 1);

	return 0;
}

static int mdio_reset_v1(void *iobase)
{
	writel((4 << 2), iobase + GMAC_GMII_ADDR);
	return 0;
}

static void mac_loopback_v1(void *iobase, int enable)
{
	int reg;

	reg = readl(iobase + GMAC_CONTROL);
	if (enable)
		reg |= GMAC_CTL_LM;
	else
		reg &= ~GMAC_CTL_LM;
	writel(reg, iobase + GMAC_CONTROL);
}

static void set_link_mode_v1(void *iobase, int duplex, int speed)
{
	unsigned int ctrl = readl(iobase + GMAC_CONTROL);

	if (!duplex)
		ctrl &= ~GMAC_CTL_DM;
	else
		ctrl |= GMAC_CTL_DM;


	switch (speed) {
	case 1000:
		ctrl &= ~GMAC_CTL_PS;
		break;
	case 100:
	case 10:
	default:
		ctrl |= GMAC_CTL_PS;
		if (speed == 100)
			ctrl |= GMAC_CTL_FES;
		else
			ctrl &= ~GMAC_CTL_FES;
		break;
	}

	writel(ctrl, iobase + GMAC_CONTROL);
}

static void flow_ctrl_v1(void *iobase, int duplex, int fc, int pause_time)
{
	unsigned int flow = 0;

	if (fc & FLOW_RX) {
		flow |= GMAC_FLOW_CTRL_RFE;
	}
	if (fc & FLOW_TX) {
		flow |= GMAC_FLOW_CTRL_TFE;
	}

	if (duplex) {
		flow |= (pause_time << GMAC_FLOW_CTRL_PT_SHIFT);
	}

	writel(flow, iobase + GMAC_FLOW_CTRL);
}

static void rx_tx_en_v1(void *iobase)
{
	unsigned long value;

	value = readl(iobase + GMAC_CONTROL);
	value |= GMAC_CTL_TE | GMAC_CTL_RE;
	writel(value, iobase + GMAC_CONTROL);
}

static void rx_tx_dis_v1(void *iobase)
{
	unsigned long value;

	value = readl(iobase + GMAC_CONTROL);
	value &= ~(GMAC_CTL_TE | GMAC_CTL_RE);
	writel(value, iobase + GMAC_CONTROL);
}

/*
 * Descriptor operations
 */
static void desc_buf_set_v1(struct dma_desc *desc, unsigned long paddr, int len)
{
	desc->desc1.all &= (~((1<<11) - 1));
	desc->desc1.all |= (len & ((1<<11) - 1));
	desc->desc2 = paddr;
}

static int desc_buf_get_addr_v1(struct dma_desc *desc)
{
	return desc->desc2;
}

static int desc_buf_get_len_v1(struct dma_desc *desc)
{
	return (desc->desc1.all & ((1 << 11) - 1));
}

static void desc_set_own_v1(struct dma_desc *desc)
{
	desc->desc0.all |= 0x80000000;
}

static int desc_get_own_v1(struct dma_desc *desc)
{
	return desc->desc0.all & 0x80000000;
}

/*
 * TODO: use the virt_addr fiel of dma_desc to find next one
 */
static void desc_init_v1(struct dma_desc *desc)
{
	desc->desc1.all = 0;
	desc->desc2  = 0;

	desc->desc1.all |= (1 << 24);
}

static int desc_get_rx_status_v1(struct dma_desc *desc, struct geth_extra_stats *x)
{
	int ret = good_frame;

	if (desc->desc0.rx.last_desc == 0) {
		sunxi_printk("[debug_sugar]: ==========It is the last_desc........\n");
		return discard_frame;
	}

	if (desc->desc0.rx.err_sum) {
		if (desc->desc0.rx.desc_err)
			x->rx_desc++;

		if (desc->desc0.rx.sou_filter)
			x->sa_filter_fail++;

		if (desc->desc0.rx.over_err)
			x->overflow_error++;

		if (desc->desc0.rx.ipch_err)
			x->ipc_csum_error++;

		if (desc->desc0.rx.late_coll)
			x->rx_collision++;

		if (desc->desc0.rx.crc_err)
			x->rx_crc++;

		ret = discard_frame;
	}

	if (desc->desc0.rx.len_err) {
		ret = discard_frame;
	}
	if (desc->desc0.rx.mii_err) {
		ret = discard_frame;
	}

	return ret;
}

static int desc_rx_frame_len_v1(struct dma_desc *desc)
{
	return desc->desc0.rx.frm_len;
}

static int desc_get_tx_status_v1(struct dma_desc *desc, struct geth_extra_stats *x)
{
	int ret = 0;

	if (desc->desc0.tx.err_sum) {
		if (desc->desc0.tx.under_err) {
			x->tx_underflow++;
		}
		if (desc->desc0.tx.no_carr) {
			x->tx_carrier++;
		}
		if (desc->desc0.tx.loss_carr) {
			x->tx_losscarrier++;
		}

#if 0
		if ((desc->desc0.tx.ex_deferral) ||
			     (desc->desc0.tx.ex_coll) ||
			     (desc->desc0.tx.late_coll))
			stats->collisions += desc->desc0.tx.coll_cnt;
#endif
		ret = -1;
	}

	if (desc->desc0.tx.vlan_tag) {
		x->tx_vlan++;
	}

	if (desc->desc0.tx.deferred)
		x->tx_deferred++;

	return ret;
}


static void desc_tx_close_v1(struct dma_desc *first, struct dma_desc *end, int csum)
{
	struct dma_desc *desc = first;
	first->desc1.tx.first_sg = 1;
	end->desc1.tx.last_seg = 1;
	end->desc1.tx.interrupt = 1;

	if (csum)
		do {
			desc->desc1.tx.cic = 3;
			desc++;
		} while(desc <= end);
}

static int desc_get_tx_ls_v1(struct dma_desc *desc)
{
	return desc->desc1.tx.last_seg;
}

/*
 * DMA operations
 */
static int dma_init_v1(void *iobase)
{
	unsigned int value;
	int pbl = 2;

#if 0
	int limit;
	/* DMA SW reset */
	value = readl(iobase + GDMA_BUS_MODE);
	value |= SOFT_RESET;
	writel(value, iobase + GDMA_BUS_MODE);
	limit = 1000;
	while (limit) {
		if (!(readl(iobase + GDMA_BUS_MODE) & SOFT_RESET))
			break;
	}

	/* -EBUSY */
	if (limit < 0)
		return -16;
#endif

	value = BUS_MODE_FIXBUST | BUS_MODE_4PBL |
	    ((pbl << BUS_MODE_PBL_SHIFT) |
	     (pbl << BUS_MODE_RPBL_SHIFT));

#ifdef CONFIG_GMAC_DA
	value |= BUS_MODE_DA;	/* Rx has priority over tx */
#endif
	writel(value, iobase + GDMA_BUS_MODE);

	/* Mask interrupts by writing to CSR7 */
	writel(GDMA_DEF_INTR, iobase + GDMA_INTR_ENA);

	return 0;
}

static void dma_int_dis_v1(void *iobase)
{
	writel(0, iobase + GDMA_INTR_ENA);
}

static void dma_int_en_v1(void *iobase)
{
	writel(GDMA_DEF_INTR, iobase + GDMA_INTR_ENA);
}
static int int_status_v1(void *iobase, struct geth_extra_stats *x)
{
	int ret = 0;
	/* read the status register (CSR5) */
	unsigned int intr_status;

	intr_status = readl(iobase + GMAC_RGMII_STATUS);
	if (intr_status & RGMII_IRQ)
		readl(iobase + GMAC_RGMII_STATUS);

	intr_status = readl(iobase + GDMA_STATUS);

	/* ABNORMAL interrupts */
	if (intr_status & GDMA_STAT_AIS) {
		if (intr_status & GDMA_STAT_UNF) {
			ret = tx_hard_error_bump_tc;
			x->tx_undeflow_irq++;
		}
		if (intr_status & GDMA_STAT_TJT) {
			x->tx_jabber_irq++;
		}
		if (intr_status & GDMA_STAT_OVF) {
			x->rx_overflow_irq++;
		}
		if (intr_status & GDMA_STAT_RU) {
			x->rx_buf_unav_irq++;
		}
		if (intr_status & GDMA_STAT_RPS) {
			x->rx_process_stopped_irq++;
		}
		if (intr_status & GDMA_STAT_RWT) {
			x->rx_watchdog_irq++;
		}
		if (intr_status & GDMA_STAT_ETI) {
			x->tx_early_irq++;
		}
		if (intr_status & GDMA_STAT_TPS) {
			x->tx_process_stopped_irq++;
			ret = tx_hard_error;
		}
		if (intr_status & GDMA_STAT_FBI) {
			x->fatal_bus_error_irq++;
			ret = tx_hard_error;
		}
	}
	/* TX/RX NORMAL interrupts */
	if (intr_status & GDMA_STAT_NIS) {
		x->normal_irq_n++;
		if ((intr_status & GDMA_STAT_RI) ||
			 (intr_status & (GDMA_STAT_TI)))
				ret = handle_tx_rx;
	}
#if 0
	/* Optional hardware blocks, interrupts should be disabled */
	if (intr_status & (GDMA_STAT_GLI))
		pr_info("%s: unexpected status %08x\n", __func__, intr_status);
#endif
	/* Clear the interrupt by writing a logic 1 to the CSR5[15-0] */

	writel((intr_status & 0x1ffff), iobase + GDMA_STATUS);

	return ret;
}

static void start_tx_v1(void *iobase, unsigned long txbase)
{
	unsigned int value;
	
	/* Write the base address of Tx descriptor lists into registers */
	writel(txbase, iobase + GDMA_XMT_LIST);

	value = readl(iobase + GDMA_OP_MODE);
	value |= OP_MODE_ST;
	writel(value, iobase + GDMA_OP_MODE);
}

static void stop_tx_v1(void *iobase)
{
	unsigned int value = readl(iobase + GDMA_OP_MODE);
	value &= ~OP_MODE_ST;
	writel(value, iobase + GDMA_OP_MODE);
}

static void poll_tx_v1(void *iobase)
{
	writel(1, iobase + GDMA_XMT_POLL);
}

static void start_rx_v1(void *iobase, unsigned long rxbase)
{
	unsigned int value;

	/* Write the base address of Rx descriptor lists into registers */
	writel(rxbase, iobase + GDMA_RCV_LIST);

	value = readl(iobase + GDMA_OP_MODE);
	value |= OP_MODE_SR;
	writel(value, iobase + GDMA_OP_MODE);
}

static void stop_rx_v1(void *iobase)
{
	unsigned int value;

	value = readl(iobase + GDMA_OP_MODE);
	value &= ~OP_MODE_SR;
	writel(value, iobase + GDMA_OP_MODE);
}

static void poll_rx_v1(void *iobase)
{
	writel(1, iobase + GDMA_RCV_POLL);
}

static struct geth_ops geth_hw_ops_v1 = {
	/* Core operations */
	.mac_reset = mac_reset_v1,
	.mac_init = mac_init_v1,
	.hash_filter = hash_filter_v1,
	.set_filter = set_filter_v1,
	.set_umac = set_umac_v1,
	.get_umac = get_umac_v1,
	.set_link_mode = set_link_mode_v1,
	.flow_ctrl = flow_ctrl_v1,
	.rx_tx_en = rx_tx_en_v1,
	.rx_tx_dis = rx_tx_dis_v1,
	.mdio_read = mdio_read_v1,
	.mdio_write = mdio_write_v1,
	.mdio_reset = mdio_reset_v1,
	.mac_loopback = mac_loopback_v1,

	/* Descriptor operations */
	.buf_set = desc_buf_set_v1,
	.buf_get_addr = desc_buf_get_addr_v1,
	.buf_get_len = desc_buf_get_len_v1,
	.set_own = desc_set_own_v1,
	.get_own = desc_get_own_v1,
	.desc_init = desc_init_v1,
	.get_rx_status = desc_get_rx_status_v1,
	.get_tx_status = desc_get_tx_status_v1,
	.rx_frame_len = desc_rx_frame_len_v1,
	.tx_close = desc_tx_close_v1,
	.get_tx_ls = desc_get_tx_ls_v1,

	/* DMA operations */
	.dma_init = dma_init_v1,
	.dma_int_dis = dma_int_dis_v1,
	.dma_int_en = dma_int_en_v1,
	.int_status = int_status_v1,
	.start_tx = start_tx_v1,
	.stop_tx = stop_tx_v1,
	.poll_tx = poll_tx_v1,
	.start_rx = start_rx_v1,
	.stop_rx = stop_rx_v1,
	.poll_rx = poll_rx_v1,
};


/******************************************************************************
 *	1673 operations
 *****************************************************************************/
#define GETH_BASIC_CTL0		0x00
#define GETH_BASIC_CTL1		0x04
#define GETH_INT_STA		0x08
#define GETH_INT_EN		0x0C
#define GETH_TX_CTL0		0x10
#define GETH_TX_CTL1		0x14
#define GETH_TX_FLOW_CTL	0x1C
#define GETH_TX_DESC_LIST	0x20
#define GETH_RX_CTL0		0x24
#define GETH_RX_CTL1		0x28
#define GETH_RX_DESC_LIST	0x34
#define GETH_RX_FRM_FLT		0x38
#define GETH_RX_HASH0		0x40
#define GETH_RX_HASH1		0x44
#define GETH_MDIO_ADDR		0x48
#define GETH_MDIO_DATA		0x4C
#define GETH_ADDR_HI(reg)	(0x50 + ((reg) << 3))
#define GETH_ADDR_LO(reg)	(0x54 + ((reg) << 3))
#define GETH_TX_DMA_STA		0xB0
#define GETH_TX_CUR_DESC	0xB4
#define GETH_TX_CUR_BUF		0xB8
#define GETH_RX_DMA_STA		0xC0
#define GETH_RX_CUR_DESC	0xC4
#define GETH_RX_CUR_BUF		0xC8
#define GETH_RGMII_STA		0xD0

#define	CTL0_LM			0x02 
#define CTL0_DM			0x01
#define CTL0_SPEED		0x04

#define BURST_LEN		0x3F000000
#define RX_TX_PRI		0x02
#define SOFT_RST		0x01

#define TX_FLUSH		0x01
#define TX_MD			0x02
#define TX_NEXT_FRM		0x04
#define TX_TH			0x0700

#define RX_FLUSH		0x01
#define RX_MD			0x02
#define RX_RUNT_FRM		0x04
#define RX_TH			0x0030

#define TX_INT			0x00001
#define TX_STOP_INT		0x00002
#define TX_UA_INT		0x00004
#define TX_TOUT_INT		0x00008
#define TX_UNF_INT		0x00010
#define TX_EARLY_INT		0x00020
#define RX_INT			0x00100
#define RX_UA_INT		0x00200
#define RX_STOP_INT		0x00400
#define RX_TOUT_INT		0x00800
#define RX_OVF_INT		0x01000
#define RX_EARLY_INT		0x02000
#define LINK_STA_INT		0x10000

static int mac_reset_v2(void *iobase, void (*mdelay)(int), int n)
{
	unsigned int value;

	/* DMA SW reset */
	value = readl(iobase + GETH_BASIC_CTL1);
	value |= SOFT_RST;
	writel(value, iobase + GETH_BASIC_CTL1);

	mdelay(n);

	return !!(readl(iobase + GETH_BASIC_CTL1) & SOFT_RST);
}

static int mac_init_v2(void *base, int txmode, int rxmode)
{
	unsigned int value; 

	hwdev.ops->dma_init(base);

	/* Initialize the core component */
	value = readl(base + GETH_TX_CTL0);
	value |= (1 << 30);	/* Jabber Disable */
	writel(value, base + GETH_TX_CTL0);

	value = readl(base + GETH_RX_CTL0);
	value |= (1 << 27);	/* Enable CRC & IPv4 Header Checksum */
	value |= (1 << 28);	/* Automatic Pad/CRC Stripping */
	value |= (1 << 29);	/* Jumbo Frame Enable */
	writel(value, base + GETH_RX_CTL0);

	/* Mask GMAC interrupts */
	//writel(0x207, base + GMAC_INT_MASK);
	writel((4 << 20), base + GMAC_GMII_ADDR); /* MDC_DIV_RATIO */

	/* Set the Rx&Tx mode */
	value = readl(base + GETH_TX_CTL1);
	if (txmode == SF_DMA_MODE) {
		/* Transmit COE type 2 cannot be done in cut-through mode. */
		value |= TX_MD;
		/* Operating on second frame increase the performance
		 * especially when transmit store-and-forward is used.*/
		value |= TX_NEXT_FRM;
	} else {
		value &= ~TX_MD;
		value &= ~TX_TH;
		/* Set the transmit threshold */
		if (txmode <= 64)
			value |= 0x00000000;
		else if (txmode <= 128)
			value |= 0x00000100;
		else if (txmode <= 192)
			value |= 0x00000200;
		else
			value |= 0x00000300;
	}
	writel(value, base + GETH_TX_CTL1);

	value = readl(base + GETH_RX_CTL1);
	if (rxmode == SF_DMA_MODE) {
		value |= RX_MD;
	} else {
		value &= ~RX_MD;
		value &= ~RX_TH;
		if (rxmode <= 32)
			value |= 0x10;
		else if (rxmode <= 64)
			value |= 0x00;
		else if (rxmode <= 96)
			value |= 0x20;
		else
			value |= 0x30;
	}
	writel(value, base + GETH_RX_CTL1);

	return 0;
}

static void hash_filter_v2(void *iobase, unsigned long low, unsigned long high)
{
	writel(high, iobase + GETH_RX_HASH0);
	writel(low, iobase + GETH_RX_HASH1);
}

static void set_filter_v2(void *iobase, unsigned flags)
{
	int tmp_flags;

	tmp_flags = readl(iobase + GETH_RX_FRM_FLT);

	tmp_flags |= ((flags >> 31) |
			((flags >> 9) & 0x00000002) |
			((flags << 1) & 0x00000010) |
			((flags >> 3) & 0x00000060) |
			((flags << 7) & 0x00000300) |
			((flags << 6) & 0x00003000) |
			((flags << 12) & 0x00030000) |
			(flags << 31));

	writel(tmp_flags, iobase + GETH_RX_FRM_FLT);
}


static void set_umac_v2(void *base, unsigned char *addr, unsigned int index)
{
	unsigned long data;

	data = (addr[5] << 8) | addr[4];
	writel(data, base + GETH_ADDR_HI(index));
	data = (addr[3] << 24) | (addr[2] << 16) | (addr[1] << 8) | addr[0];
	writel(data, base + GETH_ADDR_LO(index));
}

static void get_umac_v2(void *base, unsigned char *addr, unsigned int index)
{
	unsigned int hi_addr, lo_addr;

	/* Read the MAC address from the hardware */
	hi_addr = readl(base + GETH_ADDR_HI(index));
	lo_addr = readl(base + GETH_ADDR_LO(index));

	/* Extract the MAC address from the high and low words */
	addr[0] = lo_addr & 0xff;
	addr[1] = (lo_addr >> 8) & 0xff;
	addr[2] = (lo_addr >> 16) & 0xff;
	addr[3] = (lo_addr >> 24) & 0xff;
	addr[4] = hi_addr & 0xff;
	addr[5] = (hi_addr >> 8) & 0xff;
}

static int mdio_read_v2(void *iobase, int phyaddr, int phyreg)
{
	unsigned int value = 0;

	/* Mask the MDC_DIV_RATIO */
	value |= ((hwdev.mdc_div & 0x07) << 20);
	value |= (((phyaddr << 12) & (0x0001F000)) |
			((phyreg << 4) & (0x000007F0)) |
			MII_BUSY);

	while (((readl(iobase + GETH_MDIO_ADDR)) & MII_BUSY) == 1);

	writel(value, iobase + GETH_MDIO_ADDR);
	while (((readl(iobase + GETH_MDIO_ADDR)) & MII_BUSY) == 1);

	return (int)readl(iobase + GETH_MDIO_DATA);
}

static int mdio_write_v2(void *iobase, int phyaddr, int phyreg, unsigned short data)
{
	unsigned int value;

	value = ((0x07 << 20) & readl(iobase + GETH_MDIO_ADDR)) | (hwdev.mdc_div << 20);
	value |= (((phyaddr << 12) & (0x0001F000)) |
			((phyreg << 4) & (0x000007F0))) |
			MII_WRITE | MII_BUSY;


	/* Wait until any existing MII operation is complete */
	while (((readl(iobase + GETH_MDIO_ADDR)) & MII_BUSY) == 1);

	/* Set the MII address register to write */
	writel(data, iobase + GETH_MDIO_DATA);
	writel(value, iobase + GETH_MDIO_ADDR);

	/* Wait until any existing MII operation is complete */
	while (((readl(iobase + GETH_MDIO_ADDR)) & MII_BUSY) == 1);

	return 0;
}

static int mdio_reset_v2(void *iobase)
{
	writel((4 << 2), iobase + GETH_MDIO_ADDR);
	return 0;
}

static void mac_loopback_v2(void *iobase, int enable)
{
	int reg;

	reg = readl(iobase + GETH_BASIC_CTL0);
	if (enable)
		reg |= 0x02;
	else
		reg &= ~0x02;
	writel(reg, iobase + GETH_BASIC_CTL0);
}

static void set_link_mode_v2(void *iobase, int duplex, int speed)
{
	unsigned int ctrl = readl(iobase + GETH_BASIC_CTL0);

	if (!duplex)
		ctrl &= ~CTL0_DM;
	else
		ctrl |= CTL0_DM;


	switch (speed) {
	case 1000:
		ctrl &= ~0x0C;
		break;
	case 100:
	case 10:
	default:
		ctrl |= 0x08;
		if (speed == 100)
			ctrl |= 0x04;
		else
			ctrl &= ~0x04;
		break;
	}

	writel(ctrl, iobase + GETH_BASIC_CTL0);
}

static void flow_ctrl_v2(void *iobase, int duplex, int fc, int pause_time)
{
	unsigned int flow = 0;

	if (fc & FLOW_RX) {
		flow = readl(iobase + GETH_RX_CTL0);
		flow |= 0x10000;
		writel(flow, iobase + GETH_RX_CTL0);
	}

	if (fc & FLOW_TX) {
		flow = readl(iobase + GETH_TX_FLOW_CTL);
		flow |= 0x00001;
		writel(flow, iobase + GETH_TX_FLOW_CTL);
	}

	if (duplex) {
		flow = readl(iobase + GETH_TX_FLOW_CTL);
		flow |= (pause_time << 4);
		writel(flow, iobase + GETH_TX_FLOW_CTL);
	}

}

static void rx_tx_en_v2(void *iobase)
{
	unsigned long value;

	value = readl(iobase + GETH_TX_CTL0);
	value |= (1 << 31);
	writel(value, iobase + GETH_TX_CTL0);

	value = readl(iobase + GETH_RX_CTL0);
	value |= (1 << 31);
	writel(value, iobase + GETH_RX_CTL0);
}

static void rx_tx_dis_v2(void *iobase)
{
	unsigned long value;

	value = readl(iobase + GETH_TX_CTL0);
	value &= ~(1 << 31);
	writel(value, iobase + GETH_TX_CTL0);

	value = readl(iobase + GETH_RX_CTL0);
	value &= ~(1 << 31);
	writel(value, iobase + GETH_RX_CTL0);
}

/*
 * Descriptor operations
 */
static void desc_buf_set_v2(struct dma_desc *desc, unsigned long paddr, int len)
{
	desc->desc1.all &= (~((1<<11) - 1));
	desc->desc1.all |= (len & ((1<<11) - 1));
	desc->desc2 = paddr;
}

static int desc_buf_get_addr_v2(struct dma_desc *desc)
{
	return desc->desc2;
}

static int desc_buf_get_len_v2(struct dma_desc *desc)
{
	return (desc->desc1.all & ((1 << 11) - 1));
}

static void desc_set_own_v2(struct dma_desc *desc)
{
	desc->desc0.all |= 0x80000000;
}

static int desc_get_own_v2(struct dma_desc *desc)
{
	return desc->desc0.all & 0x80000000;
}

/*
 * TODO: use the virt_addr fiel of dma_desc to find next one
 */
static void desc_init_v2(struct dma_desc *desc)
{
	desc->desc1.all = 0;
	desc->desc2  = 0;

	desc->desc1.all |= (1 << 24);
}

static int desc_get_rx_status_v2(struct dma_desc *desc, struct geth_extra_stats *x)
{
	int ret = good_frame;

	if (desc->desc0.rx.last_desc == 0) {
		sunxi_printk("[debug_sugar]: ==========It is the last_desc........\n");
		return discard_frame;
	}

	if (desc->desc0.rx.err_sum) {
		if (desc->desc0.rx.desc_err)
			x->rx_desc++;

		if (desc->desc0.rx.sou_filter)
			x->sa_filter_fail++;

		if (desc->desc0.rx.over_err)
			x->overflow_error++;

		if (desc->desc0.rx.ipch_err)
			x->ipc_csum_error++;

		if (desc->desc0.rx.late_coll)
			x->rx_collision++;

		if (desc->desc0.rx.crc_err)
			x->rx_crc++;

		ret = discard_frame;
	}

	if (desc->desc0.rx.len_err) {
		ret = discard_frame;
	}
	if (desc->desc0.rx.mii_err) {
		ret = discard_frame;
	}

	return ret;
}

static int desc_rx_frame_len_v2(struct dma_desc *desc)
{
	return desc->desc0.rx.frm_len;
}

static int desc_get_tx_status_v2(struct dma_desc *desc, struct geth_extra_stats *x)
{
	int ret = 0;

	if (desc->desc0.tx.under_err) {
		x->tx_underflow++;
		ret = -1;
	}
	if (desc->desc0.tx.no_carr) {
		x->tx_carrier++;
		ret = -1;
	}
	if (desc->desc0.tx.loss_carr) {
		x->tx_losscarrier++;
		ret = -1;
	}

#if 0
	if ((desc->desc0.tx.ex_deferral) ||
			(desc->desc0.tx.ex_coll) ||
			(desc->desc0.tx.late_coll))
		stats->collisions += desc->desc0.tx.coll_cnt;
#endif

	if (desc->desc0.tx.deferred)
		x->tx_deferred++;

	return ret;
}


static void desc_tx_close_v2(struct dma_desc *first, struct dma_desc *end, int csum)
{
	struct dma_desc *desc = first;
	first->desc1.tx.first_sg = 1;
	end->desc1.tx.last_seg = 1;
	end->desc1.tx.interrupt = 1;

	if (csum)
		do {
			desc->desc1.tx.cic = 3;
			desc++;
		} while(desc <= end);
}

static int desc_get_tx_ls_v2(struct dma_desc *desc)
{
	return desc->desc1.tx.last_seg;
}

/*
 * DMA operations
 */
static int dma_init_v2(void *iobase)
{
	unsigned int value;

	/* Burst should be 8 */
	value = (8 << 24);

#ifdef CONFIG_GMAC_DA
	value |= RX_TX_PRI;	/* Rx has priority over tx */
#endif
	writel(value, iobase + GETH_BASIC_CTL1);

	/* Mask interrupts by writing to CSR7 */
	writel(RX_INT | TX_INT | TX_UNF_INT, iobase + GETH_INT_EN);

	return 0;
}

static void dma_int_dis_v2(void *iobase)
{
	writel(0, iobase + GETH_INT_EN);
}

static void dma_int_en_v2(void *iobase)
{
	writel(RX_INT | TX_INT | TX_UNF_INT, iobase + GETH_INT_EN);
}
static int int_status_v2(void *iobase, struct geth_extra_stats *x)
{
	int ret = 0;
	/* read the status register (CSR5) */
	unsigned int intr_status;

	intr_status = readl(iobase + GETH_RGMII_STA);
	if (intr_status & RGMII_IRQ)
		readl(iobase + GETH_RGMII_STA);

	intr_status = readl(iobase + GETH_INT_STA);

	/* ABNORMAL interrupts */
	if (intr_status & TX_UNF_INT) {
		ret = tx_hard_error_bump_tc;
		x->tx_undeflow_irq++;
	}
	if (intr_status & TX_TOUT_INT) {
		x->tx_jabber_irq++;
	}
	if (intr_status & RX_OVF_INT) {
		x->rx_overflow_irq++;
	}
	if (intr_status & RX_UA_INT) {
		x->rx_buf_unav_irq++;
	}
	if (intr_status & RX_STOP_INT) {
		x->rx_process_stopped_irq++;
	}
	if (intr_status & RX_TOUT_INT) {
		x->rx_watchdog_irq++;
	}
	if (intr_status & TX_EARLY_INT) {
		x->tx_early_irq++;
	}
	if (intr_status & TX_STOP_INT) {
		x->tx_process_stopped_irq++;
		ret = tx_hard_error;
	}

	/* TX/RX NORMAL interrupts */
	if (intr_status & (TX_INT | RX_INT |
			RX_EARLY_INT | TX_UA_INT)) {
		x->normal_irq_n++;
		if (intr_status & (TX_INT | RX_INT))
				ret = handle_tx_rx;
	}
	/* Clear the interrupt by writing a logic 1 to the CSR5[15-0] */
	writel(intr_status & 0x3FFF, iobase + GETH_INT_STA);

	return ret;
}

static void start_tx_v2(void *iobase, unsigned long txbase)
{
	unsigned int value;
	
	/* Write the base address of Tx descriptor lists into registers */
	writel(txbase, iobase + GETH_TX_DESC_LIST);

	value = readl(iobase + GETH_TX_CTL1);
	value |= 0x40000000;
	writel(value, iobase + GETH_TX_CTL1);
}

static void stop_tx_v2(void *iobase)
{
	unsigned int value = readl(iobase + GETH_TX_CTL1);
	value &= ~0x40000000;
	writel(value, iobase + GETH_TX_CTL1);
}

static void poll_tx_v2(void *iobase)
{
	unsigned int value;

	value = readl(iobase + GETH_TX_CTL1);
	writel(value | 0x80000000, iobase + GETH_TX_CTL1);
}

static void start_rx_v2(void *iobase, unsigned long rxbase)
{
	unsigned int value;

	/* Write the base address of Rx descriptor lists into registers */
	writel(rxbase, iobase + GETH_RX_DESC_LIST);

	value = readl(iobase + GETH_RX_CTL1);
	value |= 0x40000000;
	writel(value, iobase + GETH_RX_CTL1);
}

static void stop_rx_v2(void *iobase)
{
	unsigned int value;

	value = readl(iobase + GETH_RX_CTL1);
	value &= ~0x40000000;
	writel(value, iobase + GETH_RX_CTL1);
}

static void poll_rx_v2(void *iobase)
{
	unsigned int value;

	value = readl(iobase + GETH_RX_CTL1);
	writel(value | 0x80000000, iobase + GETH_RX_CTL1);
}

static struct geth_ops geth_hw_ops_v2 = {
	/* Core operations */
	.mac_reset = mac_reset_v2,
	.mac_init = mac_init_v2,
	.hash_filter = hash_filter_v2,
	.set_filter = set_filter_v2,
	.set_umac = set_umac_v2,
	.get_umac = get_umac_v2,
	.set_link_mode = set_link_mode_v2,
	.flow_ctrl = flow_ctrl_v2,
	.rx_tx_en = rx_tx_en_v2,
	.rx_tx_dis = rx_tx_dis_v2,
	.mdio_read = mdio_read_v2,
	.mdio_write = mdio_write_v2,
	.mdio_reset = mdio_reset_v2,
	.mac_loopback = mac_loopback_v2,

	/* Descriptor operations */
	.buf_set = desc_buf_set_v2,
	.buf_get_addr = desc_buf_get_addr_v2,
	.buf_get_len = desc_buf_get_len_v2,
	.set_own = desc_set_own_v2,
	.get_own = desc_get_own_v2,
	.desc_init = desc_init_v2,
	.get_rx_status = desc_get_rx_status_v2,
	.get_tx_status = desc_get_tx_status_v2,
	.rx_frame_len = desc_rx_frame_len_v2,
	.tx_close = desc_tx_close_v2,
	.get_tx_ls = desc_get_tx_ls_v2,

	/* DMA operations */
	.dma_init = dma_init_v2,
	.dma_int_dis = dma_int_dis_v2,
	.dma_int_en = dma_int_en_v2,
	.int_status = int_status_v2,
	.start_tx = start_tx_v2,
	.stop_tx = stop_tx_v2,
	.poll_tx = poll_tx_v2,
	.start_rx = start_rx_v2,
	.stop_rx = stop_rx_v2,
	.poll_rx = poll_rx_v2,
};

/***************************************************************************
 * External interface
 **************************************************************************/
/*
 * Set a ring desc buffer.
 */
void desc_init_chain(struct dma_desc *desc, unsigned long addr,
			unsigned int size)
{
	/*
	 * In chained mode the desc3 points to the next element in the ring.
	 * The latest element has to point to the head.
	 */
	int i;
	struct dma_desc *p = desc;
	unsigned long dma_phy = addr;

	for (i = 0; i < (size - 1); i++) {
		dma_phy += sizeof(struct dma_desc);
		p->desc3 = (unsigned int)dma_phy;
		/* Chain mode */
		p->desc1.all |= (1 << 24);
		p++;
	}
	p->desc1.all |= (1 << 24);
	p->desc3 = (unsigned int)addr;
}



int sunxi_mdio_read(void *iobase, int phyaddr, int phyreg)
{
	return hwdev.ops->mdio_read(iobase, phyaddr, phyreg);
}

int sunxi_mdio_write(void *iobase, int phyaddr, int phyreg, unsigned short data)
{
	return hwdev.ops->mdio_write(iobase, phyaddr, phyreg, data);
}

int sunxi_mdio_reset(void *iobase)
{
	return hwdev.ops->mdio_reset(iobase);
}

void sunxi_set_link_mode(void *iobase, int duplex, int speed)
{
	hwdev.ops->set_link_mode(iobase, duplex, speed);
}

void sunxi_mac_loopback(void *iobase, int enable)
{
	hwdev.ops->mac_loopback(iobase, enable);
}

void sunxi_flow_ctrl(void *iobase, int duplex, int fc, int pause)
{
	hwdev.ops->flow_ctrl(iobase, duplex, fc, pause);
}

int sunxi_int_status(void *iobase, void *x)
{
	return hwdev.ops->int_status(iobase, (struct geth_extra_stats *)x);
}

void sunxi_start_rx(void *iobase, unsigned long rxbase)
{
	hwdev.ops->start_rx(iobase, rxbase);
}

void sunxi_stop_rx(void *iobase)
{
	hwdev.ops->stop_rx(iobase);
}

void sunxi_start_tx(void *iobase, unsigned long txbase)
{
	hwdev.ops->start_tx(iobase, txbase);
}

void sunxi_stop_tx(void *iobase)
{
	hwdev.ops->stop_tx(iobase);
}

int sunxi_mac_init(void *iobase, int txmode, int rxmode)
{
	return hwdev.ops->mac_init(iobase, txmode, rxmode);
}

void sunxi_hash_filter(void *iobase, unsigned long low, unsigned long high)
{
	hwdev.ops->hash_filter(iobase, low, high);
}

void sunxi_set_filter(void *iobase, unsigned long flags)
{
	hwdev.ops->set_filter(iobase, flags);
}

void sunxi_set_umac(void *iobase, unsigned char *addr, int index)
{
	hwdev.ops->set_umac(iobase, addr, index);
}

void sunxi_mac_enable(void *iobase)
{
	hwdev.ops->rx_tx_en(iobase);
}

void sunxi_mac_disable(void *iobase)
{
	hwdev.ops->rx_tx_dis(iobase);
}

void sunxi_tx_poll(void *iobase)
{
	hwdev.ops->poll_tx(iobase);
}

void sunxi_rx_poll(void *iobase)
{
	hwdev.ops->poll_rx(iobase);
}

void sunxi_int_enable(void *iobase)
{
	hwdev.ops->dma_int_en(iobase);
}

void sunxi_int_disable(void *iobase)
{
	hwdev.ops->dma_int_dis(iobase);
}

void desc_buf_set(struct dma_desc *desc, unsigned long paddr, int size)
{
	hwdev.ops->buf_set(desc, paddr, size);
}

void desc_set_own(struct dma_desc *desc)
{
	hwdev.ops->set_own(desc);
}


void desc_tx_close(struct dma_desc *first, struct dma_desc *end, int csum_insert)
{
	hwdev.ops->tx_close(first, end, csum_insert);
}

void desc_init(struct dma_desc *desc)
{
	hwdev.ops->desc_init(desc);
}

int desc_get_tx_status(struct dma_desc *desc, struct geth_extra_stats *x)
{
	return hwdev.ops->get_tx_status(desc, x);
}

int desc_buf_get_len(struct dma_desc *desc)
{
	return hwdev.ops->buf_get_len(desc);
}

int desc_buf_get_addr(struct dma_desc *desc)
{
	return hwdev.ops->buf_get_addr(desc);
}

int desc_rx_frame_len(struct dma_desc *desc)
{
	return hwdev.ops->rx_frame_len(desc);
}

int desc_get_rx_status(struct dma_desc *desc, struct geth_extra_stats *x)
{
	return hwdev.ops->get_rx_status(desc, x);
}

int desc_get_own(struct dma_desc *desc)
{
	return hwdev.ops->get_own(desc);
}

int desc_get_tx_ls(struct dma_desc *desc)
{
	return hwdev.ops->get_tx_ls(desc);
}

int sunxi_geth_register(void *iobase, int version, unsigned int div)
{
	if (!version)
		hwdev.ops = &geth_hw_ops_v1;
	else
		hwdev.ops = &geth_hw_ops_v2;

	hwdev.ver = version;
	hwdev.iobase = iobase;
	hwdev.mdc_div = div;

	return 0;
}

int sunxi_mac_reset(void *iobase, void (*delay)(int), int n)
{
	/* Reset all components */
	return hwdev.ops->mac_reset(iobase, delay, n);
}
