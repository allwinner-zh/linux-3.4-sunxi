/*
 * Copyright © 2012, Shuge
 *		Author: Sugar <shugeLinux@gmail.com>
 *
 * 
 */

#ifndef _SUNXI_GETH_REG_H_
#define	_SUNXI_GETH_REG_H_

/******************************************************************************
 *
 * register for gmac core
 *
 *****************************************************************************/
#define GMAC_CONTROL		(0x00) /* Configuration */
#define GMAC_FRAME_FILTER	(0x04) /* Frame Filter */
#define GMAC_HASH_HIGH		(0x08) /* Multicast Hash Table High */
#define GMAC_HASH_LOW		(0x0c) /* Multicast Hash Table Low */
#define GMAC_GMII_ADDR		(0x10) /* MII Address */
#define GMAC_GMII_DATA		(0x14) /* MII Data */
#define GMAC_FLOW_CTRL		(0x18) /* Flow Control */
#define GMAC_INT_STATUS		(0x38) /* Interrupt status register */
#define GMAC_INT_MASK		(0x3c) /* interrupt mask register */
#define GMAC_ADDR_HI(reg)	(0x40 + (reg<<3)) /* upper 16bits of MAC address */
#define GMAC_ADDR_LO(reg)	(0x44 + (reg<<3)) /* lower 32bits of MAC address */
#define GMAC_RGMII_STATUS	(0xD8) /* S/R-GMII status */

#define RGMII_IRQ		0x00000001

/* GMAC_CONTROL value */
#define GMAC_CTL_TC		0x01000000 /* Transmit Configuration in RGMII */
#define GMAC_CTL_WD		0x00800000 /* Watchdog Disable */
#define GMAC_CTL_JD		0x00400000 /* Jabber Disable */
#define GMAC_CTL_BE		0x00200000 /* Frame Burst Enable (only Half) */
#define GMAC_CTL_JE		0x00100000 /* Jumbo Frame Enable */
#define GMAC_CTL_IFG		0x000E0000 /* Inter-Frame Gap */
#define GMAC_CTL_DCRS		0x00010000 /* Disable Carrier Sense During Transmission (only Half) */
#define GMAC_CTL_PS		0x00008000 /* Port Select 0:GMII, 1:MII */
#define GMAC_CTL_FES		0x00004000 /* Indicates the speed in Fast Ethernet(MII) mode */
#define GMAC_CTL_ROD		0x00002000 /* Receive own disable (only half-duplex) */
#define GMAC_CTL_LM		0x00001000 /* Loopback mode */
#define GMAC_CTL_DM		0x00000800 /* Duplex mode */
#define GMAC_CTL_IPC		0x00000400 /* Checksum Offload */
#define GMAC_CTL_DR		0x00000200 /* Retry disable (only half-duplex) */
#define GMAC_CTL_LUD		0x00000100 /* Link Up/Down (only RGMII/SGMII) */
#define GMAC_CTL_ACS		0x00000080 /* Automatic Pad/CRC Stripping */
#define GMAC_CTL_BL		0x00000060 /* Back-off limit.(only half-duplex) */
#define GMAC_CTL_DC		0x00000010 /* Deferral Check.(only half-duplex) */
#define GMAC_CTL_TE		0x00000008 /* Transmit Enable */
#define GMAC_CTL_RE		0x00000004 /* Receiver Enalbe */

#define GMAC_CORE_INIT (GMAC_CTL_JD | GMAC_CTL_PS | GMAC_CTL_ACS | \
			GMAC_CTL_JE | GMAC_CTL_BE)

/* GMAC_FRAME_FILTER  register value */
#define GMAC_FRAME_FILTER_PR	0x00000001	/* Promiscuous Mode */
#define GMAC_FRAME_FILTER_HUC	0x00000002	/* Hash Unicast */
#define GMAC_FRAME_FILTER_HMC	0x00000004	/* Hash Multicast */
#define GMAC_FRAME_FILTER_DAIF	0x00000008	/* DA Inverse Filtering */
#define GMAC_FRAME_FILTER_PM	0x00000010	/* Pass all multicast */
#define GMAC_FRAME_FILTER_DBF	0x00000020	/* Disable Broadcast frames */
#define GMAC_FRAME_FILTER_SAIF	0x00000100	/* Inverse Filtering */
#define GMAC_FRAME_FILTER_SAF	0x00000200	/* Source Address Filter */
#define GMAC_FRAME_FILTER_HPF	0x00000400	/* Hash or perfect Filter */
#define GMAC_FRAME_FILTER_RA	0x80000000	/* Receive all mode */

/* GMAC_FLOW_CTRL register value */
#define GMAC_FLOW_CTRL_PT_MASK	0xffff0000	/* Pause Time Mask */
#define GMAC_FLOW_CTRL_PT_SHIFT	16
#define GMAC_FLOW_CTRL_RFE	0x00000004	/* Rx Flow Control Enable */
#define GMAC_FLOW_CTRL_TFE	0x00000002	/* Tx Flow Control Enable */
#define GMAC_FLOW_CTRL_FCB_BPA	0x00000001	/* Flow Control Busy ... */

/* GMAC_GMII_ADDR value */
#define MII_BUSY		0x00000001
#define MII_WRITE		0x00000002
#define MII_PHY_MASK		0x0000FFC0
#define MII_CR_MASK		0x0000001C

/******************************************************************************
 *
 * register for DMA
 *
 *****************************************************************************/
#define GDMA_BUS_MODE		(0x1000) /* Bus Mode Register */
#define GDMA_XMT_POLL		(0x1004) /* Transmit Poll Demand */
#define GDMA_RCV_POLL		(0x1008) /* Received Poll Demand */
#define GDMA_RCV_LIST		(0x100C) /* Receive List Base */
#define GDMA_XMT_LIST		(0x1010) /* Transmit List Base */
#define GDMA_STATUS		(0x1014) /* Status Register */
#define GDMA_OP_MODE		(0x1018) /* DMA Operational Mode */
#define GDMA_INTR_ENA		(0x101c) /* Interrupt Enable */
#define GDMA_MISSED_FRAME	(0x1020) /* Missed Frame and Buffer Overflow Counter */
#define GDMA_CUR_TX_DESC	(0x1048) /* Current Host Transmit Descriptor */
#define GDMA_CUR_RX_DESC	(0x104C) /* Current Host Received Descriptor */
#define GDMA_CUR_TX_BUF		(0x1050) /* Current Host Transmit Buffer Address */
#define GDMA_CUR_RX_BUF		(0x1054) /* Current Host Received Buffer Address */

/*	GDMA_BUS_MODE value */
#define SOFT_RESET		0x00000001 /* Software reset gdma */
#define BUS_MODE_DA		0x00000002 /* DMA Arbitration */
#define BUS_ADDR_ALIGN		0x02000000 /* Address-Aligned Beats */
#define BUS_MODE_4PBL		0x01000000 /* 4xPBL Mode */
#define BUS_MODE_USP		0x00800000

#define BUS_MODE_RPBL_MASK	0x007E0000
#define BUS_MODE_RPBL_SHIFT	17

#define BUS_MODE_PBL_MASK	0x00003F00
#define BUS_MODE_PBL_SHIFT	8

#define BUS_MODE_FIXBUST	0x00010000

#define BUS_MODE_DSL_MASK	0x0000007C /* Descriptor skip length */
#define BUS_MODE_DSL_SHIFT	2

#define BUS_MODE_RTPR		0x00000C00 /* Rx TX priority ratio */

/* GDMA_STATUS value */
#define GDMA_STAT_GLI		0x04000000
#define GDMA_STAT_NIS		0x00010000	/* Normal Interrupt Summary */
#define GDMA_STAT_AIS		0x00008000	/* Abnormal Interrupt Summary */
#define GDMA_STAT_ERI		0x00004000	/* Early Receive Interrupt */
#define GDMA_STAT_FBI		0x00002000	/* Fatal Bus Error Interrupt */
#define GDMA_STAT_ETI		0x00000400	/* Early Transmit Interrupt */
#define GDMA_STAT_RWT		0x00000200	/* Receive Watchdog Timeout */
#define GDMA_STAT_RPS		0x00000100	/* Receive Process Stopped */
#define GDMA_STAT_RU		0x00000080	/* Receive Buffer Unavailable */
#define GDMA_STAT_RI		0x00000040	/* Receive Interrupt */
#define GDMA_STAT_UNF		0x00000020	/* Transmit Underflow */
#define GDMA_STAT_OVF		0x00000010	/* Receive Overflow */
#define GDMA_STAT_TJT		0x00000008	/* Transmit Jabber Timeout */
#define GDMA_STAT_TU		0x00000004	/* Transmit Buffer Unavailable */
#define GDMA_STAT_TPS		0x00000002	/* Transmit Process Stopped */
#define GDMA_STAT_TI		0x00000001	/* Transmit Interrupt */

#define STATUS_EB_MASK		0x00380000

#define STATUS_TS_MASK		0x00700000
#define STATUS_TS_SHIFT		20
#define TS_STOP			0x00000000
#define TS_FETCH_DESC		0x00100000
#define TS_WAIT_STAT		0x00200000
#define TS_READ_DATA		0x00300000
#define TS_SUSP			0x00600000
#define TS_CLOSE_DESC		0x00700000

#define STATUS_RS_MASK		0x000E0000
#define STATUS_RS_SHIFT		17
#define RS_STOP			0x00000000
#define RS_FETCH_DESC		0x00020000
#define RS_WAIT_STAT		0x00060000
#define RS_SUSP			0x00080000
#define RS_CLOSE_DESC		0x000A0000
#define RS_WRITE_HOST		0x000E0000

/* GDMA_OP_MODE value */
#define OP_MODE_RSF		0x02000000	/* Receive Store and Forward */
#define OP_MODE_DFF		0x01000000 	/* Disable Flushing of Received Frames */
#define OP_MODE_RFA2		0x00800000 	/* MSB of Threshold for Activating Flow Control */
#define OP_MODE_RFD2		0x00400000 	/* MSB of Threshold for Deactivating Flow Control */
#define OP_MODE_TSF		0x00200000 	/* Transmit Store and Forward */
#define OP_MODE_FTF		0x00100000 	/* Flush Transmit FIFO */
#define OP_MODE_TTC_MASK	0x0001C000 	/* Transmit Threshold Control */
#define OP_MODE_TTC_SHIFT	14
enum ttc_control {
	OP_MODE_TTC_64	= 0x00000000,
	OP_MODE_TTC_128 = 0x00004000,
	OP_MODE_TTC_192 = 0x00008000,
	OP_MODE_TTC_256 = 0x0000c000,
	OP_MODE_TTC_40	= 0x00010000,
	OP_MODE_TTC_32	= 0x00014000,
	OP_MODE_TTC_24	= 0x00018000,
	OP_MODE_TTC_16	= 0x0001c000,
};

#define OP_MODE_ST		0x00002000 	/* Start/Stop Transmission Command */
#define OP_MODE_RFD		0x00001800 	/* Threshold for deactivating flow control */
#define OP_MODE_RFA		0x00000600 	/* Threshold for activating flow control */
#define OP_MODE_EFC		0x00000100 	/* Enable HW flow control */
#define OP_MODE_FEF		0x00000080 	/* Forward Error Frames */
#define OP_MODE_FUF		0x00000040 	/* Forward Undersized Frames */
#define OP_MODE_RTC_MASK	0x00000018 	/* Receive Threshold Control */
#define OP_MODE_RTC_SHIFT	3
enum rtc_control {
	OP_MODE_RTC_64	= 0x00000000,
	OP_MODE_RTC_32	= 0x00000008,
	OP_MODE_RTC_96	= 0x00000010,
	OP_MODE_RTC_128	= 0x00000018,
};

#define OP_MODE_OSF		0x00000004 	/* Operate on Second Frame */
#define OP_MODE_SR		0x00000002 	/* Start/Stop Receive */

#define OP_MODE_TC_TX_MASK	0xfffe3fff
#define OP_MODE_TC_RX_MASK	0xffffffe7

/* GDMA interrupt */
#define INTR_ENA_NIE		0x00010000	/* Normal Summary */
#define INTR_ENA_AIE		0x00008000	/* Abnormal Summary */
#define INTR_ENA_ERE 		0x00004000	/* Early Receive */
#define INTR_ENA_FBE 		0x00002000	/* Fatal Bus Error */
#define INTR_ENA_ETE 		0x00000400	/* Early Transmit */
#define INTR_ENA_RWE 		0x00000200	/* Receive Watchdog */
#define INTR_ENA_RSE 		0x00000100	/* Receive Stopped */
#define INTR_ENA_RUE 		0x00000080	/* Receive Buffer Unavailable */
#define INTR_ENA_RIE 		0x00000040	/* Receive Interrupt */
#define INTR_ENA_UNE 		0x00000020	/* Tx Underflow */
#define INTR_ENA_OVE 		0x00000010	/* Receive Overflow */
#define INTR_ENA_TJE 		0x00000008	/* Transmit Jabber */
#define INTR_ENA_TUE 		0x00000004	/* Transmit Buffer Unavailable */
#define INTR_ENA_TSE 		0x00000002	/* Transmit Stopped Enable */
#define INTR_ENA_TIE 		0x00000001	/* Transmit Interrupt Enable */

#define GDMA_INTR_NOR		(INTR_ENA_NIE | INTR_ENA_RIE | INTR_ENA_TIE)
#define GDMA_INTR_ABNOR		(INTR_ENA_AIE | INTR_ENA_FBE | INTR_ENA_UNE)

#define GDMA_DEF_INTR		(GDMA_INTR_NOR | GDMA_INTR_ABNOR)

#define SF_DMA_MODE		1

/* Flow Control defines */
#define FLOW_OFF	0
#define FLOW_RX		1
#define FLOW_TX		2
#define FLOW_AUTO	(FLOW_TX | FLOW_RX)

#define HASH_TABLE_SIZE 64
#define PAUSE_TIME 0x200
#define GMAC_MAX_UNICAST_ADDRESSES	8


/* PHY address */
#define PHY_ADDR		0x01
#define PHY_DM			0x0010
#define PHY_AUTO_NEG		0x0020
#define PHY_POWERDOWN		0x0080
#define PHY_NEG_EN		0x1000

#define MII_CLK			0x00000008
/* bits 4 3 2 | AHB1 Clock	| MDC Clock
 * -------------------------------------------------------
 *      0 0 0 | 60 ~ 100 MHz	| div-42
 *      0 0 1 | 100 ~ 150 MHz	| div-62
 *      0 1 0 | 20 ~ 35 MHz	| div-16
 *      0 1 1 | 35 ~ 60 MHz	| div-26
 *      1 0 0 | 150 ~ 250 MHz	| div-102
 *      1 0 1 | 250 ~ 300 MHz	| div-124
 *      1 1 x | Reserved	|
 */

/* Default tx descriptor */
#define TX_SINGLE_DESC0		0x80000000
#define TX_SINGLE_DESC1		0x63000000

/* Default rx descriptor */
#define RX_SINGLE_DESC0		0x80000000
#define RX_SINGLE_DESC1		0x83000000

typedef union {
	struct {
		/* TDES0 */
		unsigned int deferred:1;	/* Deferred bit (only half-duplex) */
		unsigned int under_err:1;	/* Underflow error */
		unsigned int ex_deferral:1;	/* Excessive deferral */
		unsigned int coll_cnt:4;	/* Collision count */
		unsigned int vlan_tag:1;	/* VLAN Frame */
		unsigned int ex_coll:1;		/* Excessive collision */
		unsigned int late_coll:1;	/* Late collision */
		unsigned int no_carr:1;		/* No carrier */
		unsigned int loss_carr:1;	/* Loss of collision */
		unsigned int ipdat_err:1;	/* IP payload error */
		unsigned int frm_flu:1;		/* Frame flushed */
		unsigned int jab_timeout:1;	/* Jabber timeout */
		unsigned int err_sum:1;		/* Error summary */
		unsigned int iphead_err:1;	/* IP header error */
		unsigned int ttss:1;		/* Transmit time stamp status */
		unsigned int reserved0:13;
		unsigned int own:1;		/* Own bit. CPU:0, DMA:1 */
	} tx;

	/* bits 5 7 0 | Frame status
	 * ----------------------------------------------------------
	 *      0 0 0 | IEEE 802.3 Type frame (length < 1536 octects)
	 *      1 0 0 | IPv4/6 No CSUM errorS.
	 *      1 0 1 | IPv4/6 CSUM PAYLOAD error
	 *      1 1 0 | IPv4/6 CSUM IP HR error
	 *      1 1 1 | IPv4/6 IP PAYLOAD AND HEADER errorS
	 *      0 0 1 | IPv4/6 unsupported IP PAYLOAD
	 *      0 1 1 | COE bypassed.. no IPv4/6 frame
	 *      0 1 0 | Reserved.
	 */
	struct {
		/* RDES0 */
		unsigned int chsum_err:1;	/* Payload checksum error */
		unsigned int crc_err:1;		/* CRC error */
		unsigned int dribbling:1;	/* Dribble bit error */
		unsigned int mii_err:1;		/* Received error (bit3) */
		unsigned int recv_wt:1;		/* Received watchdog timeout */
		unsigned int frm_type:1;	/* Frame type */
		unsigned int late_coll:1;	/* Late Collision */
		unsigned int ipch_err:1;	/* IPv header checksum error (bit7) */
		unsigned int last_desc:1;	/* Laset descriptor */
		unsigned int first_desc:1;	/* First descriptor */
		unsigned int vlan_tag:1;	/* VLAN Tag */
		unsigned int over_err:1;	/* Overflow error (bit11) */
		unsigned int len_err:1;		/* Length error */
		unsigned int sou_filter:1;	/* Source address filter fail */
		unsigned int desc_err:1;	/* Descriptor error */
		unsigned int err_sum:1;		/* Error summary (bit15) */
		unsigned int frm_len:14;	/* Frame length */
		unsigned int des_filter:1;	/* Destination address filter fail */
		unsigned int own:1;		/* Own bit. CPU:0, DMA:1 */
	#define RX_PKT_OK		0x7FFFB77C
	#define RX_LEN			0x3FFF0000
	} rx;

	unsigned int all;
} desc0_u;

typedef union {
	struct {
		/* TDES1 */
		unsigned int buf1_size:11;	/* Transmit buffer1 size */
		unsigned int buf2_size:11;	/* Transmit buffer2 size */
		unsigned int ttse:1;		/* Transmit time stamp enable */
		unsigned int dis_pad:1;		/* Disable pad (bit23) */
		unsigned int adr_chain:1;	/* Second address chained */
		unsigned int end_ring:1;	/* Transmit end of ring */
		unsigned int crc_dis:1;		/* Disable CRC */
		unsigned int cic:2;		/* Checksum insertion control (bit27:28) */
		unsigned int first_sg:1;	/* First Segment */
		unsigned int last_seg:1;	/* Last Segment */
		unsigned int interrupt:1;	/* Interrupt on completion */
	} tx;

	struct {
		/* RDES1 */
		unsigned int buf1_size:11;	/* Received buffer1 size */
		unsigned int buf2_size:11;	/* Received buffer2 size */
		unsigned int reserved1:2;
		unsigned int adr_chain:1;	/* Second address chained */
		unsigned int end_ring:1;		/* Received end of ring */
		unsigned int reserved2:5;
		unsigned int dis_ic:1;		/* Disable interrupt on completion */
	} rx;

	unsigned int all;
} desc1_u;


typedef struct dma_desc {
	desc0_u desc0;
	desc1_u desc1;
	/* The address of buffers */
	unsigned int	desc2;
	/* Next desc's addresss */
	unsigned int	desc3;
} __attribute__((packed)) dma_desc_t;

enum csum_insertion{
	cic_dis		= 0, /* Checksum Insertion Control */
	cic_ip		= 1, /* Only IP header */
	cic_no_pse	= 2, /* IP header but not pseudoheader */
	cic_full	= 3, /* IP header and pseudoheader */
};

#endif
