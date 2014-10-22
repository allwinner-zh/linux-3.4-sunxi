/*************************************************************************/ /*!
@File
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

/* This is a DC driver for Intel's Series 5/6 video hardware. Series 5 is also
 * known as Ironlake by Intel, Series 6 as Sandy Bridge.
 *
 * This driver intentionally does not layer on top of the i915 DRM driver. The
 * primary reason for this is that the DRM driver does not cleanly export the
 * functionality of the cursor and sprite (video overlay) layers in a way that
 * the services DC API can use them. For Android HWC, we need full control.
 *
 * At the moment this driver only supports a handful of CEA HDMI modes (720p,
 * 1080p, 720p50, 1080p50) and only on the HDMID port. It assumes primary
 * video appears on pipe A, and pipe B is left unconfigured.
 *
 * At the moment this driver does the bare minimum to take over from the BIOS.
 * In contrast, the Linux DRM driver assumes nothing about initial hardware
 * state and does a full reset and reprogram. Eventually, we may need to do
 * the same.
 *
 * This driver was implemented using the Ironlake PRMs from:
 *   http://intellinuxgraphics.org/
 *
 * Register dumps from the DRM i915 driver (using Linux ftrace feature) were
 * also used to decode poorly documented or undocumented functionality.
 */

#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vgaarb.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/fb.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include "kerneldisplay.h"
#include "imgpixfmts_km.h"
#include "img_types.h"

/* for MODULE_LICENSE */
#include "pvrmodule.h"

#ifndef CONFIG_FB
#error dc_intel needs Linux framebuffer support. Enable it in your kernel.
#endif

#define DRVNAME					"dc_intel"
#define DC_PHYS_HEAP_ID			0
#define MAX_COMMANDS_IN_FLIGHT	2
#define NUM_PREFERRED_BUFFERS	2

#define DC_INTEL_MAX_SUPPORTED_MODES 4

#define DC_INTEL_BUS_ID	0
#define DC_INTEL_DEV_ID	2
#define DC_INTEL_FUNC	0

/* Approximately 16:9 */
#define FALLBACK_PHYSICAL_WIDTH		533
#define FALLBACK_PHYSICAL_HEIGHT	300
#define FALLBACK_DPI                240

/* Primary + Sprite + Cursor */
#define MAX_OVERLAY_PIPES 3

/* Write register without tracing */
#define DCI_WR_NOTRACE(reg, x) \
	iowrite32(x, pvRegisters + DC_INTEL_##reg)

/* Read register without tracing */
#define DCI_RR_NOTRACE(reg) \
	ioread32(pvRegisters + DC_INTEL_##reg)

#if defined(DEBUG)

static IMG_BOOL gbTraceRegs;

#define DCI_STR(reg) #reg

/* Write register */
#define DCI_WR(reg, x) \
	do { \
		unsigned int w = (x); \
		iowrite32(w, pvRegisters + DC_INTEL_##reg); \
		if(gbTraceRegs) \
			pr_err("write reg=0x%8x value=0x%8x " DCI_STR(DC_INTEL_##reg), DC_INTEL_##reg, w); \
	} while(0)

/* Read register */
#define DCI_RR(reg) \
	({ \
		unsigned int r = ioread32(pvRegisters + DC_INTEL_##reg); \
		if(gbTraceRegs) \
			pr_err("read  reg=0x%8x value=0x%8x " DCI_STR(DC_INTEL_##reg), DC_INTEL_##reg, r); \
		(r); \
	 })

#else /* defined(DEBUG) */

#define DCI_WR(reg, x)	DCI_WR_NOTRACE(reg, x)
#define DCI_RR(reg)		DCI_RR_NOTRACE(reg)

#endif /* defined(DEBUG) */

/* Reads to post writes shouldn't be traced */
#define DCI_PR(reg) \
	DCI_RR_NOTRACE(reg)

/* Write PIPEA register */
#define DCI_A_WR(reg, x) \
	DCI_WR(PIPEA_##reg, x)

/* Write PIPEA display timing register */
#define DCI_A_WDTR(reg, x, y) \
	DCI_WR(PIPEA_##reg, ((x) - 1) | (((y) - 1) << 16))

/* Write PRIMARYA register */
#define DCI_PA_WR(reg, x) \
	DCI_WR(PRIMARYA_##reg, x)

/* Write SPRITEA register */
#define DCI_SA_WR(reg, x) \
	DCI_WR(SPRITEA_##reg, x)

/* Write CURSORA register */
#define DCI_CA_WR(reg, x) \
	DCI_WR(CURSORA_##reg, x)

/* Read PIPEA register */
#define DCI_A_RR(reg) \
	DCI_RR(PIPEA_##reg)

/* Read on PIPEA to post writes shouldn't be traced */
#define DCI_A_PR(reg) \
	DCI_PR(PIPEA_##reg)

/* Read PRIMARYA register */
#define DCI_PA_RR(reg) \
	DCI_RR(PRIMARYA_##reg)

/* Read SPRITEA register */
#define DCI_SA_RR(reg) \
	DCI_RR(SPRITEA_##reg)

/* Read CURSORA register */
#define DCI_CA_RR(reg) \
	DCI_RR(CURSORA_##reg)

/* Read on PRIMARYA to post writes shouldn't be traced */
#define DCI_PA_PR(reg) \
	DCI_PR(PRIMARYA_##reg)

/* Read on SPRITEA to post writes shouldn't be traced */
#define DCI_SA_PR(reg) \
	DCI_PR(SPRITEA_##reg)

/* Read on CURSORA to post writes shouldn't be traced */
#define DCI_CA_PR(reg) \
	DCI_PR(CURSORA_##reg)

/* Read masked bits from a previously read register */
#define DCI_RR_BITS(val, reg, bits) \
	(((val) & DC_INTEL_##reg##_##bits##_MASK) >> \
	 DC_INTEL_##reg##_##bits##_SHIFT)

/* Re-write all bits except mask to a register */
#define DCI_WR_MASK_BITS(reg, val, bits) \
	DCI_WR(reg, (val) & ~DC_INTEL_##reg##_##bits##_MASK)

/* Re-write all bits including extra bits to a register */
#define DCI_WR_OR_BITS(reg, val, bits) \
	DCI_WR(reg, (val) | DC_INTEL_##reg##_##bits##_MASK)

/* Start register defines */

#define DC_INTEL_PGTBL_CTL								0x02020
#define DC_INTEL_PGTBL_CTL_SIZE_SHIFT					1
#define DC_INTEL_PGTBL_CTL_SIZE_MASK					0x0000000E

#define DC_INTEL_HWSTAM									0x02098

#define DC_INTEL_PGTBL_CTL2								0x020C4
#define DC_INTEL_PGTBL_CTL2_ENABLE_SHIFT				0
#define DC_INTEL_PGTBL_CTL2_ENABLE_MASK					0x00000001

#define DC_INTEL_CPU_VGACNTRL							0x41000
#define DC_INTEL_VGA_SR_INDEX							0x3c4
#define DC_INTEL_VGA_SR_DATA							0x3c5

#define DC_INTEL_DEIMR									0x44004
#define DC_INTEL_DEIIR									0x44008
#define DC_INTEL_DEIER									0x4400c

#define DC_INTEL_PIPEA_LGC_PALETTE						0x4A000

#define DC_INTEL_PIPEA_HTOTAL							0x60000
#define DC_INTEL_PIPEA_HBLANK							0x60004
#define DC_INTEL_PIPEA_HSYNC							0x60008
#define DC_INTEL_PIPEA_VTOTAL							0x6000C
#define DC_INTEL_PIPEA_VBLANK							0x60010
#define DC_INTEL_PIPEA_VSYNC							0x60014
#define DC_INTEL_PIPEA_PIPESRC							0x6001C
#define DC_INTEL_PIPEA_VSYNCSHIFT						0x60028
#define DC_INTEL_PIPEA_PIPE_DATA_M1						0x60030
#define DC_INTEL_PIPEA_PIPE_DATA_N1						0x60034
#define DC_INTEL_PIPEA_PIPE_LINK_M1						0x60040
#define DC_INTEL_PIPEA_PIPE_LINK_N1						0x60044

#define DC_INTEL_PIPEA_FDI_TX_CTL						0x60100
#define DC_INTEL_PIPEA_FDI_TX_CTL_ENABLE_SHIFT			31
#define DC_INTEL_PIPEA_FDI_TX_CTL_ENABLE_MASK			0x80000000
#define DC_INTEL_PIPEA_FDI_TX_CTL_PLL_ENABLE_SHIFT		14
#define DC_INTEL_PIPEA_FDI_TX_CTL_PLL_ENABLE_MASK		0x00004000

#define DC_INTEL_PIPEA_PF_WIN_SZ						0x68074
#define DC_INTEL_PIPEA_PF_CTL							0x68080

#define DC_INTEL_PIPEA_PIPECONF							0x70008
#define DC_INTEL_PIPEA_PIPECONF_ENABLE_SHIFT			31
#define DC_INTEL_PIPEA_PIPECONF_ENABLE_MASK				0x80000000
#define DC_INTEL_PIPEA_PIPECONF_STATE_SHIFT				30
#define DC_INTEL_PIPEA_PIPECONF_STATE_MASK				0x40000000
#define DC_INTEL_PIPEA_PIPECONF_BPC_SHIFT				5
#define DC_INTEL_PIPEA_PIPECONF_BPC_MASK				0x000000E0

#define DC_INTEL_CURSORA_CURCNTR						0x70080
#define DC_INTEL_CURSORA_CURCNTR_CURSORMODE_SHIFT		0
#define DC_INTEL_CURSORA_CURCNTR_CURSORMODE_MASK		0x00000007
#define DC_INTEL_CURSORA_CURCNTR_CURSORMODE2_SHIFT		5
#define DC_INTEL_CURSORA_CURCNTR_CURSORMODE2_MASK		0x00000020
#define DC_INTEL_CURSORA_CURCNTR_GAMMA_ENABLE_SHIFT		26
#define DC_INTEL_CURSORA_CURCNTR_GAMMA_ENABLE_MASK		0x04000000

#define DC_INTEL_CURSORA_CURBASE						0x70084
#define DC_INTEL_CURSORA_CURPOS							0x70088
#define DC_INTEL_CURSORA_CURVGAPOPUPBASE				0x7008C

#define DC_INTEL_PRIMARYA_DSPCNTR						0x70180
#define DC_INTEL_PRIMARYA_DSPCNTR_PIXELFORMAT_SHIFT		26
#define DC_INTEL_PRIMARYA_DSPCNTR_PIXELFORMAT_MASK		0x3C000000
#define DC_INTEL_PRIMARYA_DSPCNTR_GAMMA_ENABLE_SHIFT	30
#define DC_INTEL_PRIMARYA_DSPCNTR_GAMMA_ENABLE_MASK		0x40000000
#define DC_INTEL_PRIMARYA_DSPCNTR_ENABLE_SHIFT			31
#define DC_INTEL_PRIMARYA_DSPCNTR_ENABLE_MASK			0x80000000

#define DC_INTEL_PRIMARYA_DSPLINOFF						0x70184
#define DC_INTEL_PRIMARYA_DSPSTRIDE						0x70188
#define DC_INTEL_PRIMARYA_DSPSURF						0x7019C

#define DC_INTEL_SPRITEA_DVSCNTR						0x72180
#define DC_INTEL_SPRITEA_DVSCNTR_YUVBYTEORDER_SHIFT		16
#define DC_INTEL_SPRITEA_DVSCNTR_YUVBYTEORDER_MASK		0x00030000
#define DC_INTEL_SPRITEA_DVSCNTR_RB_SWAP_SHIFT			20
#define DC_INTEL_SPRITEA_DVSCNTR_RB_SWAP_MASK			0x00100000
#define DC_INTEL_SPRITEA_DVSCNTR_PIXELFORMAT_SHIFT		25
#define DC_INTEL_SPRITEA_DVSCNTR_PIXELFORMAT_MASK		0x06000000
#define DC_INTEL_SPRITEA_DVSCNTR_GAMMA_ENABLE_SHIFT		30
#define DC_INTEL_SPRITEA_DVSCNTR_GAMMA_ENABLE_MASK		0x40000000
#define DC_INTEL_SPRITEA_DVSCNTR_ENABLE_SHIFT			31
#define DC_INTEL_SPRITEA_DVSCNTR_ENABLE_MASK			0x80000000

#define DC_INTEL_SPRITEA_DVSLINOFF						0x72184
#define DC_INTEL_SPRITEA_DVSSTRIDE						0x72188
#define DC_INTEL_SPRITEA_DVSPOS							0x7218C
#define DC_INTEL_SPRITEA_DVSSIZE						0x72190
#define DC_INTEL_SPRITEA_DVSSURF						0x7219C
#define DC_INTEL_SPRITEA_DVSSCALE						0x72204

#define DC_INTEL_SOUTH_CHICKEN1							0xC2000
#define DC_INTEL_PIPEA_FDI_RX_CHICKEN					0xC200C

#define DC_INTEL_GMBUS_PORT_HDMIC_OFFSET				0xC501C
#define DC_INTEL_GMBUS_PORT_HDMID_OFFSET				0xC5024
#define DC_INTEL_GMBUS0_MMIO_OFFSET						0xC5100

#define DC_INTEL_PIPEA_PCH_DPLL							0xC6014
#define DC_INTEL_PIPEA_PCH_DPLL_VCO_ENABLE_SHIFT		31
#define DC_INTEL_PIPEA_PCH_DPLL_VCO_ENABLE_MASK			0x80000000

#define DC_INTEL_PIPEA_PCH_FP0							0xC6040
#define DC_INTEL_PIPEA_PCH_FP1							0xC6044
#define DC_INTEL_PIPEA_TRANS_HTOTAL						0xE0000
#define DC_INTEL_PIPEA_TRANS_HBLANK						0xE0004
#define DC_INTEL_PIPEA_TRANS_HSYNC						0xE0008
#define DC_INTEL_PIPEA_TRANS_VTOTAL						0xE000C
#define DC_INTEL_PIPEA_TRANS_VBLANK						0xE0010
#define DC_INTEL_PIPEA_TRANS_VSYNC						0xE0014
#define DC_INTEL_PIPEA_TRANS_VSYNCSHIFT					0xE0028

#define DC_INTEL_HDMIC_CTL								0xE1150
#define DC_INTEL_HDMIC_CTL_PORT_DETECTED_SHIFT			2
#define DC_INTEL_HDMIC_CTL_PORT_DETECTED_MASK			0x00000004
#define DC_INTEL_HDMIC_CTL_ENABLE_SHIFT					31
#define DC_INTEL_HDMIC_CTL_ENABLE_MASK					0x80000000

#define DC_INTEL_HDMID_CTL								0xE1160
#define DC_INTEL_HDMID_CTL_PORT_DETECTED_SHIFT			2
#define DC_INTEL_HDMID_CTL_PORT_DETECTED_MASK			0x00000004
#define DC_INTEL_HDMID_CTL_ENABLE_SHIFT					31
#define DC_INTEL_HDMID_CTL_ENABLE_MASK					0x80000000

#define DC_INTEL_PIPEA_FDI_RX_CTL						0xF000C
#define DC_INTEL_PIPEA_FDI_RX_CTL_ENABLE_SHIFT			31
#define DC_INTEL_PIPEA_FDI_RX_CTL_ENABLE_MASK			0x80000000
#define DC_INTEL_PIPEA_FDI_RX_CTL_PLL_ENABLE_SHIFT		13
#define DC_INTEL_PIPEA_FDI_RX_CTL_PLL_ENABLE_MASK		0x00002000

#define DC_INTEL_PIPEA_FDI_RX_IIR						0xF0014
#define DC_INTEL_PIPEA_FDI_RX_IMR						0xF0018
#define DC_INTEL_PIPEA_FDI_RX_TUSIZE1					0xF0030

#define DC_INTEL_I2C_RISEFALL_TIME						10

#define DC_INTEL_GPIO_CLOCK_DIR_MASK					(1 << 0)
#define DC_INTEL_GPIO_CLOCK_DIR_IN						(0 << 1)
#define DC_INTEL_GPIO_CLOCK_DIR_OUT						(1 << 1)
#define DC_INTEL_GPIO_CLOCK_VAL_MASK					(1 << 2)
#define DC_INTEL_GPIO_CLOCK_VAL_IN						(1 << 4)

#define DC_INTEL_GPIO_DATA_DIR_MASK						(1 << 8)
#define DC_INTEL_GPIO_DATA_DIR_IN						(0 << 9)
#define DC_INTEL_GPIO_DATA_DIR_OUT						(1 << 9)
#define DC_INTEL_GPIO_DATA_VAL_MASK						(1 << 10)
#define DC_INTEL_GPIO_DATA_VAL_IN						(1 << 12)

#define DC_INTEL_EDID_LENGTH							0x80
#define DC_INTEL_DDC_ADDR								0x50

/* FDI RX/TX training register values */
#define DC_INTEL_FDI_LINK_TRAIN_PATTERN_1				(0 << 28)
#define DC_INTEL_FDI_LINK_TRAIN_PATTERN_2				(1 << 28)
#define DC_INTEL_FDI_LINK_TRAIN_NONE					(3 << 28)

/* Cougar/Panther Point train bits */
#define DC_INTEL_FDI_LINK_TRAIN_PATTERN_1_CPT			(0 << 8)
#define DC_INTEL_FDI_LINK_TRAIN_PATTERN_2_CPT			(1 << 8)
#define DC_INTEL_FDI_LINK_TRAIN_NONE_CPT				(3 << 8)

/* SNB-B workaround bits */
#define DC_INTEL_FDI_LINK_TRAIN_400MV_0DB_SNB_B			(0    << 22)
#define DC_INTEL_FDI_LINK_TRAIN_400MV_6DB_SNB_B			(0x3A << 22)
#define DC_INTEL_FDI_LINK_TRAIN_600MV_3_5DB_SNB_B		(0x39 << 22)
#define DC_INTEL_FDI_LINK_TRAIN_800MV_0DB_SNB_B			(0x38 << 22)
#define DC_INTEL_FDI_LINK_TRAIN_VOL_EMP_MASK			(0x3F << 22)

/* Chicken bits for SOUTH_CHICKEN1 */
#define DC_INTEL_FDIA_PHASE_SYNC_SHIFT_OVR				(1 << 19)
#define DC_INTEL_FDIA_PHASE_SYNC_SHIFT_EN				(1 << 18)

#define DC_INTEL_FDI_TX_ENHANCE_FRAME_ENABLE			(1 << 18)

#define DC_INTEL_FDI_RX_ENHANCE_FRAME_ENABLE			(1 << 6)
#define DC_INTEL_FDI_RX_BIT_LOCK						(1 << 8)
#define DC_INTEL_FDI_RX_SYMBOL_LOCK						(1 << 9)

#define DC_INTEL_IRQ_MASTER_IRQ_CONTROL					(1 << 31)
#define DC_INTEL_IRQ_SPRITEA_FLIP_DONE					(1 << 28)
#define DC_INTEL_IRQ_PLANEA_FLIP_DONE					(1 << 26)
#define DC_INTEL_IRQ_GTT_FAULT							(1 << 24)
#define DC_INTEL_IRQ_PIPEA_VBLANK						(1 << 7)
#define DC_INTEL_IRQ_PIPEA_VSYNC						(1 << 3)

#define DC_INTEL_FLUSH_RETRIES							10
#define DC_INTEL_REG_VBLANK_ASSERT_TIMEOUT_MS			50

typedef enum
{
	DC_INTEL_TYPE_UNKNOWN		= 0,
	DC_INTEL_TYPE_IRONLAKE		= 1,
	DC_INTEL_TYPE_SANDYBRIDGE	= 2,
}
DC_INTEL_TYPE;

/* NOTE: These fields are all in pages, not bytes */
typedef struct
{
	/* Absolute offset of zone start in Gtt PTEs */
	IMG_UINT32 ui32Offset;

	/* Total Gtt PTEs available to this zone */
	IMG_UINT32 ui32Total;

	/* Offset of last written zone PTE relative to start of zone */
	IMG_UINT32 ui32WriteOffset;
}
DC_INTEL_GTT_ZONE;

typedef struct
{
	/* GTT page table entries */
	IMG_SYS_PHYADDR			*psPTEs;

	/* Number of usable GTT page table entries */
	IMG_UINT32				ui32NumMappableGttEntries;

	/* Base address of stolen memory region */
	IMG_SYS_PHYADDR			sStolenBase;

	/* All stolen memory allocations are mapped in this zone */
	DC_INTEL_GTT_ZONE		sStolenZone;

	/* Zone for dynamic mappings */
	DC_INTEL_GTT_ZONE		sDynamicZone;
}
DC_INTEL_GTT_CONTEXT;

typedef struct
{
	IMG_HANDLE				hSrvHandle;

	/* Workqueue for display flip processing */
	struct workqueue_struct	*psFlipWQ;
	struct work_struct		sFlipWorker;

	/* Intel graphics PCI device */
	struct pci_dev			*psPciDev;

	/* Display MMIO register bar */
	void					*pvRegisters;

	/* Ironlake or Sandy Bridge */
	DC_INTEL_TYPE			eDeviceType;

	/* GTT configuration object */
	DC_INTEL_GTT_CONTEXT	sGtt;

	/* Linux framebuffer object */
	struct fb_info			*psLINFBInfo;

	/* Pixel format of primary surface */
	IMG_UINT32				ePixFormat;
}
DC_INTEL_DEVICE;

typedef struct
{
	DC_INTEL_DEVICE	*psDeviceData;
}
DC_INTEL_CONTEXT;

typedef struct
{
	DC_INTEL_CONTEXT		*psDeviceContext;
	IMG_UINT32				ui32Width;
	IMG_UINT32				ui32Height;
	IMG_UINT32				ePixFormat;
	IMG_UINT32				ui32ByteStride;

	/* For non-imported allocations, this is non-zero after BufferAlloc().
	 * For imported allocations, this is non-zero between BufferMap()
	 * and BufferUnmap().
	 */
	IMG_UINT32				ui32ByteSize;

	/* For non-imported allocations, this is always NULL.
	 * For imported allocations, this is non-NULL after BufferImport().
	 */
	IMG_HANDLE				hImport;

	/* List of physical addresses for the allocation (import only) */
	IMG_DEV_PHYADDR			*asPhyAddrs;

	/* Gtt handle for the allocation's mapping */
	IMG_UINT32				ui32GttPTEOffset;
}
DC_INTEL_BUFFER;

typedef struct
{
	IMG_BOOL				bEnabled;
	PVRSRV_SURFACE_RECT		sCrop;
	PVRSRV_SURFACE_RECT		sDisplay;
	IMG_PIXFMT				ePixFormat;
	IMG_UINT32				ui32ByteStride;
	IMG_UINT32				ui32GttPTEOffset;
}
DC_INTEL_PLANE_CONFIG;

typedef enum
{
	DC_INTEL_FLIP_CONFIG_DISPLAYED,
	DC_INTEL_FLIP_CONFIG_PROGRAMMED,
	DC_INTEL_FLIP_CONFIG_PENDING,
}
DC_INTEL_FLIP_CONFIG_STATE;

typedef struct _DC_INTEL_FLIP_CONFIG_
{
	struct _DC_INTEL_FLIP_CONFIG_ *psNext;

	DC_INTEL_FLIP_CONFIG_STATE	eState;

	DC_INTEL_PLANE_CONFIG		sPrimary;
	DC_INTEL_PLANE_CONFIG		sSprite;
	DC_INTEL_PLANE_CONFIG		sCursor;

	IMG_HANDLE					hConfigData;
}
DC_INTEL_FLIP_CONFIG;

/* First 11 bytes of the 18 byte EDID DTD (Detailed Timing Descriptor). */

typedef struct
{
	IMG_UINT16	ui16Clock;
	IMG_UINT8	ui8HActiveLow;
	IMG_UINT8	ui8HBlankLow;
	IMG_UINT8	ui8HActiveBlankHigh;
	IMG_UINT8	ui8VActiveLow;
	IMG_UINT8	ui8VBlankLow;
	IMG_UINT8	ui8VActiveBlankHigh;
	IMG_UINT8	ui8HSyncOffsetLow;
	IMG_UINT8	ui8HSyncWidthLow;
	IMG_UINT8	ui8VSyncOffsetWidthLow;
	IMG_UINT8	ui8HVSyncOffsetWidthHigh;
}
DC_INTEL_DTD;

typedef enum
{
	DC_INTEL_CEA_MODE_720p		= 4,
	DC_INTEL_CEA_MODE_1080p		= 16,
	DC_INTEL_CEA_MODE_720p50	= 19,
	DC_INTEL_CEA_MODE_1080p50	= 31,
}
DC_INTEL_CEA_MODE;

static struct
{
	IMG_BOOL			bSupported;
	DC_INTEL_CEA_MODE	eMode;
	DC_INTEL_DTD		sDTD;
}
gasSupportedMode[DC_INTEL_MAX_SUPPORTED_MODES];

static IMG_UINT32 gui32NumSupportedModes;

static IMG_CHAR *mode;

static DC_INTEL_DEVICE *gpsDeviceData;

static DEFINE_MUTEX(gsFlipConfigMutex);

static DC_INTEL_FLIP_CONFIG *gpsConfigQueue;

static void ConfigQueueAppend(DC_INTEL_FLIP_CONFIG *psConfig)
{
	DC_INTEL_FLIP_CONFIG *psItem;

	if(!gpsConfigQueue)
	{
		gpsConfigQueue = psConfig;
		return;
	}

	psItem = gpsConfigQueue;
	while(psItem->psNext)
		psItem = psItem->psNext;

	psItem->psNext = psConfig;
	psConfig->psNext = NULL;
}

static IMG_BOOL ConfigQueueSanityCheck(void)
{
	IMG_BOOL bSawDisplayed = IMG_FALSE, bSawProgrammed = IMG_FALSE;
	IMG_BOOL bSawPending = IMG_FALSE;
	DC_INTEL_FLIP_CONFIG *psItem;

	for(psItem = gpsConfigQueue; psItem; psItem = psItem->psNext)
	{
		switch(psItem->eState)
		{
			case DC_INTEL_FLIP_CONFIG_DISPLAYED:
				if(bSawProgrammed || bSawPending)
				{
					pr_err("Saw programmed/pending queue items before displayed");
					return IMG_FALSE;
				}
				if(bSawDisplayed)
				{
					pr_err("Saw more than one queue item marked displayed");
					return IMG_FALSE;
				}
				bSawDisplayed = IMG_TRUE;
				break;

			case DC_INTEL_FLIP_CONFIG_PROGRAMMED:
				if(bSawPending)
				{
					pr_err("Saw pending queue items before programmed");
					return IMG_FALSE;
				}
				if(bSawProgrammed)
				{
					pr_err("Saw more than one queue item marked programmed");
					return IMG_FALSE;
				}
				bSawProgrammed = IMG_TRUE;
				break;

			case DC_INTEL_FLIP_CONFIG_PENDING:
				if(!bSawProgrammed)
				{
					pr_err("Saw pending before programmed");
					return IMG_FALSE;
				}
				if(bSawPending)
				{
					pr_err("Saw more than one queue item marked pending");
					/* Soft failure -- we can handle this */
				}
				bSawPending = IMG_TRUE;
				break;
		}
	}

	return IMG_TRUE;
}

static inline IMG_UINT32 IMGPixFmtGetBPP(IMG_PIXFMT ePixFormat)
{
	switch(ePixFormat)
	{
		case IMG_PIXFMT_B5G6R5_UNORM:
		case IMG_PIXFMT_YUYV:
		case IMG_PIXFMT_UYVY:
		case IMG_PIXFMT_YVYU:
		case IMG_PIXFMT_VYUY:
			return 16;
		case IMG_PIXFMT_B8G8R8A8_UNORM:
		case IMG_PIXFMT_R8G8B8A8_UNORM:
		case IMG_PIXFMT_B8G8R8X8_UNORM:
		case IMG_PIXFMT_R8G8B8X8_UNORM:
			return 32;
		default:
			return 0;
	}
}

static const IMG_UINT16 gaui16PciDeviceIdMap[][2] =
{
	{ 0x0042,	DC_INTEL_TYPE_IRONLAKE },
	{ 0x0112,	DC_INTEL_TYPE_SANDYBRIDGE },
	{ 0x0,		DC_INTEL_TYPE_UNKNOWN },
};

static void I2C_Reset(void *pvRegisters)
{
	DCI_WR_NOTRACE(GMBUS0_MMIO_OFFSET, 0);
}

static void I2C_SetData(void *pvRegisters, int iStateHigh)
{
	u32 u32DataBits;

	if(iStateHigh)
		u32DataBits = DC_INTEL_GPIO_DATA_DIR_IN |
					  DC_INTEL_GPIO_DATA_DIR_MASK;
	else
		u32DataBits = DC_INTEL_GPIO_DATA_DIR_OUT |
					  DC_INTEL_GPIO_DATA_DIR_MASK |
					  DC_INTEL_GPIO_DATA_VAL_MASK;

	DCI_WR_NOTRACE(GMBUS_PORT_HDMID_OFFSET, u32DataBits);
	DCI_RR_NOTRACE(GMBUS_PORT_HDMID_OFFSET);
}

static void I2C_SetClock(void *pvRegisters, int iStateHigh)
{
	u32 u32ClockBits;

	if(iStateHigh)
		u32ClockBits = DC_INTEL_GPIO_CLOCK_DIR_IN |
					   DC_INTEL_GPIO_CLOCK_DIR_MASK;
	else
		u32ClockBits = DC_INTEL_GPIO_CLOCK_DIR_OUT |
					   DC_INTEL_GPIO_CLOCK_DIR_MASK |
					   DC_INTEL_GPIO_CLOCK_VAL_MASK;

	DCI_WR_NOTRACE(GMBUS_PORT_HDMID_OFFSET, u32ClockBits);
	DCI_RR_NOTRACE(GMBUS_PORT_HDMID_OFFSET);
}

static int I2C_GetData(void *pvRegisters)
{
	DCI_WR_NOTRACE(GMBUS_PORT_HDMID_OFFSET, DC_INTEL_GPIO_DATA_DIR_MASK);
	DCI_WR_NOTRACE(GMBUS_PORT_HDMID_OFFSET, 0);
	return (DCI_RR_NOTRACE(GMBUS_PORT_HDMID_OFFSET) & DC_INTEL_GPIO_DATA_VAL_IN) != 0;
}

static int I2C_GetClock(void *pvRegisters)
{
	DCI_WR_NOTRACE(GMBUS_PORT_HDMID_OFFSET, DC_INTEL_GPIO_CLOCK_DIR_MASK);
	DCI_WR_NOTRACE(GMBUS_PORT_HDMID_OFFSET, 0);
	return (DCI_RR_NOTRACE(GMBUS_PORT_HDMID_OFFSET) & DC_INTEL_GPIO_CLOCK_VAL_IN) != 0;
}

static int I2C_PreXFer(struct i2c_adapter *psAdapter)
{
	struct i2c_algo_bit_data *psAlgoBitData = psAdapter->algo_data;

	I2C_Reset(psAlgoBitData->data);
	I2C_SetData(psAlgoBitData->data, 1);
	I2C_SetClock(psAlgoBitData->data, 1);
	udelay(DC_INTEL_I2C_RISEFALL_TIME);
	return 0;
}

static void I2C_PostXFer(struct i2c_adapter *psAdapter)
{
	struct i2c_algo_bit_data *psAlgoBitData = psAdapter->algo_data;

	I2C_SetData(psAlgoBitData->data, 1);
	I2C_SetClock(psAlgoBitData->data, 1);
}

static IMG_BOOL
DownloadEDIDBlock(struct i2c_adapter *psAdapter, unsigned char ucStart,
				  unsigned char szEDIDBlock[DC_INTEL_EDID_LENGTH])
{
	struct i2c_msg sMsgs[] =
	{
		/* Send (offset = 0) */
		{
			.addr	= DC_INTEL_DDC_ADDR,
			.flags	= 0,
			.len	= 1,
			.buf	= &ucStart,
		},
		/* Receive */
		{
			.addr	= DC_INTEL_DDC_ADDR,
			.flags	= I2C_M_RD,
			.len	= DC_INTEL_EDID_LENGTH,
			.buf	= szEDIDBlock,
		},
	};

	if(i2c_transfer(psAdapter, sMsgs, 2) != 2)
		return IMG_FALSE;

	return IMG_TRUE;
}

static void
FBModeLineToDTD(DC_INTEL_DTD *psDTD, int xres, int yres, int pixclock,
				int left, int right, int upper, int lower,
				int hsync, int vsync)
{
	int hblank = left + hsync + right;
	int vblank = upper + vsync + lower;

	/* Convert frequency in picoseconds into 10kHz units */
	psDTD->ui16Clock =
		(IMG_UINT16)(100000000UL / (unsigned int)pixclock);

	psDTD->ui8HActiveLow = xres & 0xff;
	psDTD->ui8HBlankLow = hblank & 0xff;
	psDTD->ui8HActiveBlankHigh =
		(((xres >> 8) & 0xf) << 4) | ((hblank >> 8) & 0xf);

	psDTD->ui8VActiveLow = yres & 0xff;
	psDTD->ui8VBlankLow = vblank & 0xff;
	psDTD->ui8VActiveBlankHigh =
		(((yres >> 8) & 0xf) << 4) | ((vblank >> 8) & 0xf);

	psDTD->ui8HSyncOffsetLow = right & 0xff;
	psDTD->ui8HSyncWidthLow = hsync & 0xff;
	psDTD->ui8VSyncOffsetWidthLow =
		((lower & 0xf) << 4) | (vsync & 0xf);
	psDTD->ui8HVSyncOffsetWidthHigh =
		((right & 0x300) >> 2) | ((hsync & 0x300) >> 4) |
		((lower & 0x30)  >> 2) | ((vsync & 0x30)  >> 4);
}

static void
DTDToFBModeLine(DC_INTEL_DTD *psDTD, int *xres, int *yres, int *pixclock,
				int *left, int *right, int *upper, int *lower,
				int *hsync, int *vsync)
{
	int hblank = psDTD->ui8HBlankLow |
				 ((psDTD->ui8HActiveBlankHigh & 0xf) << 8);
	int vblank = psDTD->ui8VBlankLow |
				 ((psDTD->ui8VActiveBlankHigh & 0xf) << 8);

	*pixclock = (100000000L / (int)psDTD->ui16Clock);

	*xres = psDTD->ui8HActiveLow |
			(((psDTD->ui8HActiveBlankHigh >> 4) & 0xf) << 8);
	*yres = psDTD->ui8VActiveLow |
			(((psDTD->ui8VActiveBlankHigh >> 4) & 0xf) << 8);

	*right = psDTD->ui8HSyncOffsetLow +
			 (((psDTD->ui8HVSyncOffsetWidthHigh >> 6) & 0x3) << 8);
	*lower = ((psDTD->ui8VSyncOffsetWidthLow >> 4) & 0xf) +
			 (((psDTD->ui8HVSyncOffsetWidthHigh >> 2) & 0x3) << 4);

	*hsync = psDTD->ui8HSyncWidthLow +
			 (((psDTD->ui8HVSyncOffsetWidthHigh >> 4) & 0x3) << 8);
	*vsync = (psDTD->ui8VSyncOffsetWidthLow & 0xf) +
			 ((psDTD->ui8HVSyncOffsetWidthHigh & 0x3) << 4);

	*left = hblank - *hsync - *right;
	*upper = vblank - *vsync - *lower;
}

static void
CEAVideoModeToDTD(DC_INTEL_CEA_MODE eCEAVideoMode, DC_INTEL_DTD *psDTD)
{
	/* Linux's cea_modes[] db has bugs and is incomplete.
	 * If this is fixed, this code should be migrated to use it.
	 */
	switch(eCEAVideoMode)
	{
		case DC_INTEL_CEA_MODE_720p:
			FBModeLineToDTD(psDTD, 1280, 720, 13468, 220, 110, 20, 5, 40, 5);
			break;
		case DC_INTEL_CEA_MODE_1080p:
			FBModeLineToDTD(psDTD, 1920, 1080, 6734, 148, 88, 36, 4, 44, 5);
			break;
		case DC_INTEL_CEA_MODE_720p50:
			FBModeLineToDTD(psDTD, 1280, 720, 13468, 220, 440, 20, 5, 40, 5);
			break;
		case DC_INTEL_CEA_MODE_1080p50:
			FBModeLineToDTD(psDTD, 1920, 1080, 6734, 148, 528, 36, 4, 44, 5);
			break;
	}
}

static int PopulateDTDModesFromEDID(void *pvRegisters)
{
	int err, i, j, iExtensions, iBlockType, iBlockSize = 0;
	unsigned char szEDIDBlock[DC_INTEL_EDID_LENGTH];
	unsigned char ucStart = 0, ucChecksum = 0;

	const unsigned char szExpectedHeader[] =
		{ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

	struct i2c_algo_bit_data sAlgoBitData =
	{
		.setsda		= I2C_SetData,
		.setscl		= I2C_SetClock,
		.getsda		= I2C_GetData,
		.getscl		= I2C_GetClock,
		.pre_xfer	= I2C_PreXFer,
		.post_xfer	= I2C_PostXFer,
		.udelay		= DC_INTEL_I2C_RISEFALL_TIME,
		.timeout	= usecs_to_jiffies(2200),
		.data		= pvRegisters,
	};

	struct i2c_adapter sAdapter =
	{
		.owner		= THIS_MODULE,
		.class		= I2C_CLASS_DDC,
		.name		= DRVNAME " HDMI I2C",
		.algo_data	= &sAlgoBitData,
	};

	err = i2c_bit_add_bus(&sAdapter);
	if(err)
	{
		pr_err("Failed to set up i2c adapter.");
		goto err_out;
	}

	I2C_Reset(pvRegisters);

	if(!DownloadEDIDBlock(&sAdapter, ucStart, szEDIDBlock))
	{
		pr_err("EDID I2C transfer failed.");
		err = -EINVAL;
		goto err_i2c_del_adapter;
	}

	/* Check the EDID header block */
	if(memcmp(szExpectedHeader, szEDIDBlock, 7))
	{
		pr_err("EDID header missing. Wrong i2c port?");
		err = -EINVAL;
		goto err_i2c_del_adapter;
	}

	/* Check the checksum */
	for(i = 0; i < DC_INTEL_EDID_LENGTH; i++)
		ucChecksum += szEDIDBlock[i];
	if(ucChecksum)
	{
		pr_err("EDID primary block checksum was non-zero.");
		err = -EINVAL;
		goto err_i2c_del_adapter;
	}

	/* Find the first CEA EDID extension block. */
	iExtensions = szEDIDBlock[0x7e];
	for(i = 0; i < iExtensions; i++)
	{
		ucStart += DC_INTEL_EDID_LENGTH;
		if(!DownloadEDIDBlock(&sAdapter, ucStart, szEDIDBlock))
		{
			pr_err("EDID I2C transfer failed.");
			err = -EINVAL;
			goto err_i2c_del_adapter;
		}

		/* If it's not a "CEA EDID Timing Extension Version 3"
		 * block, we don't care.
		 */
		if(szEDIDBlock[0] != 0x02 || szEDIDBlock[1] != 0x03)
		{
			pr_err("Skipping unrecognized EDID extension block.");
			continue;
		}

		break;
	}

	/* Check the extension block checksum */
	ucChecksum = 0;
	for(i = 0; i < DC_INTEL_EDID_LENGTH; i++)
		ucChecksum += szEDIDBlock[i];
	if(ucChecksum)
	{
		pr_err("EDID CEA extension block checksum was non-zero.");
		err = -EINVAL;
		goto err_i2c_del_adapter;
	}

	if(szEDIDBlock[0x02] < 0x04 ||
	   szEDIDBlock[0x02] > DC_INTEL_EDID_LENGTH - 18 - 1)
	{
		pr_err("EDID CEA extension block DTD offset is invalid.");
		err = -EINVAL;
		goto err_i2c_del_adapter;
	}

	/* No DBC available, so we have to skip processing it */
	if(szEDIDBlock[0x02] == 0x04)
	{
		pr_err("Monitor lacks Data Block Collection, which is not supported.");
		err = -EINVAL;
		goto err_i2c_del_adapter;
	}

	/* The Data Block Collection exists between 0x4 and
	 * the start of the DTD.
	 */
	for(i = 0x4; i < szEDIDBlock[0x02]; i += iBlockSize + 1)
	{
		iBlockSize = (szEDIDBlock[i] >> 0) & 31;
		iBlockType = (szEDIDBlock[i] >> 5) & 7;

		/* Skip any non Video Data Blocks */
		if(iBlockType != 2)
			continue;

		if(i + iBlockSize > szEDIDBlock[0x02])
		{
			pr_err("Bad EDID CEA Video Data Block size.");
			err = -EINVAL;
			goto err_i2c_del_adapter;
		}

		/* Found a valid Video Data Block */
		break;
	}

	if(i >= szEDIDBlock[0x02])
	{
		pr_err("EDID CEA Video Data Block not found");
		goto err_i2c_del_adapter;
	}

	/* Walk the mode list backwards. We'll filter the list for modes we
	 * understand, and stop on the first one we find. As the larger
	 * modes (more likely to be "native") tend to be listed lower down
	 * in the list, walking backwards makes it more likely we'll pick a
	 * native mode.
	 */
	for(j = i + iBlockSize; j > i; j--)
	{
		DC_INTEL_CEA_MODE eMode = szEDIDBlock[j] & 127;

		if(eMode != DC_INTEL_CEA_MODE_1080p50 &&
		   eMode != DC_INTEL_CEA_MODE_720p50  &&
		   eMode != DC_INTEL_CEA_MODE_1080p   &&
		   eMode != DC_INTEL_CEA_MODE_720p)
			continue;

		gasSupportedMode[gui32NumSupportedModes].bSupported = IMG_TRUE;
		gasSupportedMode[gui32NumSupportedModes].eMode = eMode;

		CEAVideoModeToDTD(eMode,
			&gasSupportedMode[gui32NumSupportedModes].sDTD);

		gui32NumSupportedModes++;
		if(gui32NumSupportedModes == DC_INTEL_MAX_SUPPORTED_MODES)
			break;
	}

	err = 0;
err_i2c_del_adapter:
	i2c_del_adapter(&sAdapter);
err_out:
	return err;
}

static void
DTDAndBppFromModeString(char *szModeString, DC_INTEL_DTD *psDTD,
						IMG_PIXFMT *pePixFormat)
{
	long ulX = 0, ulY = 0, ulBpp = 0, ulHz = 0;
	const char *szX, *szY, *szBpp, *szHz;
	int i;
	DC_INTEL_CEA_MODE eMode;

	szX   = strsep(&szModeString, "x-@");
	szY   = strsep(&szModeString, "x-@");
	szBpp = strsep(&szModeString, "x-@");
	szHz  = strsep(&szModeString, "x-@");

	if(!szX || !szY || !szBpp || !szHz)
		goto skip_mode_select;

	kstrtol(szX,   10, &ulX);
	kstrtol(szY,   10, &ulY);
	kstrtol(szBpp, 10, &ulBpp);
	kstrtol(szHz,  10, &ulHz);

	if(     ulX == 1920 && ulY == 1080 && ulHz == 60)
		eMode = DC_INTEL_CEA_MODE_1080p;
	else if(ulX == 1920 && ulY == 1080 && ulHz == 50)
		eMode = DC_INTEL_CEA_MODE_1080p50;
	else if(ulX == 1280 && ulY == 720  && ulHz == 60)
		eMode = DC_INTEL_CEA_MODE_720p;
	else if(ulX == 1280 && ulY == 720  && ulHz == 50)
		eMode = DC_INTEL_CEA_MODE_720p50;
	else
		goto skip_mode_select;

	for(i = 0; i < DC_INTEL_MAX_SUPPORTED_MODES; i++)
	{
		if(!gasSupportedMode[i].bSupported)
			continue;
		if(gasSupportedMode[i].eMode == eMode)
			break;
	}

	if(i == DC_INTEL_MAX_SUPPORTED_MODES)
		goto skip_mode_select;

	switch(ulBpp)
	{
		case 16:
			*pePixFormat = IMG_PIXFMT_B5G6R5_UNORM;
			break;
		case 32:
			*pePixFormat = IMG_PIXFMT_B8G8R8X8_UNORM;
			break;
		case 23:
			*pePixFormat = IMG_PIXFMT_R8G8B8X8_UNORM;
			break;
		default:
			goto skip_mode_select;
	}

	*psDTD = gasSupportedMode[i].sDTD;
	return;

skip_mode_select:
#if defined(PVR_ANDROID_HAS_SW_INCOMPATIBLE_FRAMEBUFFER)
//	*pePixFormat = IMG_PIXFMT_R8G8B8X8_UNORM;
	*pePixFormat = IMG_PIXFMT_B5G6R5_UNORM;
#else
	*pePixFormat = IMG_PIXFMT_B8G8R8X8_UNORM;
#endif
	*psDTD = gasSupportedMode[0].sDTD;
}

static void DisableVGAPlane(struct pci_dev *psPciDev, void *pvRegisters)
{
	u8 ui8sr1;

	vga_get_uninterruptible(psPciDev, VGA_RSRC_LEGACY_IO);

	outb(1, DC_INTEL_VGA_SR_INDEX);
	ui8sr1 = inb(DC_INTEL_VGA_SR_DATA);
	outb(ui8sr1 | (1 << 5), DC_INTEL_VGA_SR_DATA);

	vga_put(psPciDev, VGA_RSRC_LEGACY_IO);

	udelay(300);

	DCI_WR(CPU_VGACNTRL, (1 << 31));
	DCI_PR(CPU_VGACNTRL);
}

static IMG_BOOL
SetModeFromDTD(void *pvRegisters, DC_INTEL_DTD *psDTD, IMG_UINT32 *pui32Lanes)
{
	IMG_UINT32 ui32Lanes, ui32LinkClock, ui32FP, ui32DPLL, ui32Bpp;
	IMG_UINT32 ui32GMCHm, ui32GMCHn, ui32LINKm, ui32LINKn;
	IMG_BOOL bRet = IMG_FALSE;

	/* Convert the weirdly packed EDID DTD format into Intel type */
	IMG_UINT32 ui32Clock = psDTD->ui16Clock * 10;
	IMG_UINT16 ui16HActive =
		psDTD->ui8HActiveLow |
		(((psDTD->ui8HActiveBlankHigh >> 4) & 0xf) << 8);
	IMG_UINT16 ui16VActive =
		psDTD->ui8VActiveLow |
		(((psDTD->ui8VActiveBlankHigh >> 4) & 0xf) << 8);
	IMG_UINT16 ui16HBlank =
		psDTD->ui8HBlankLow | ((psDTD->ui8HActiveBlankHigh & 0xf) << 8);
	IMG_UINT16 ui16VBlank =
		psDTD->ui8VBlankLow | ((psDTD->ui8VActiveBlankHigh & 0xf) << 8);
	IMG_UINT16 ui16HSyncOffset =
		psDTD->ui8HSyncOffsetLow +
		(((psDTD->ui8HVSyncOffsetWidthHigh >> 6) & 0x3) << 8);
	IMG_UINT16 ui16VSyncOffset =
		((psDTD->ui8VSyncOffsetWidthLow >> 4) & 0xf) +
		(((psDTD->ui8HVSyncOffsetWidthHigh >> 2) & 0x3) << 4);
	IMG_UINT16 ui16HSyncWidth =
		psDTD->ui8HSyncWidthLow +
		(((psDTD->ui8HVSyncOffsetWidthHigh >> 4) & 0x3) << 8);
	IMG_UINT16 ui16VSyncWidth =
		(psDTD->ui8VSyncOffsetWidthLow & 0xf) +
		((psDTD->ui8HVSyncOffsetWidthHigh & 0x3) << 4);

	/* On Series 5 and newer, the pipe PLL code is much more complicated,
	 * as you need to compute the number of memory pipes (PCIe pipes?)
	 * required as well as satisfying a clock equation for getting the
	 * target pixel clock.
	 *
	 * A real driver would do this all properly, but since this is just
	 * example code and we don't want to complicate the example with
	 * minutiae, most of it is hard-coded.
	 */ 

	/* On Series 5 the link clock can change, but on current hardware
	 * it's still 2.7GHz, just like every other Intel chipset.
	 *
	 * So, hardcode the link clock to 2.7GHz. Use 10kHz units, as that's
	 * what all the registers want.
	 */
	ui32LinkClock = 270000;

	/* The FP register contains the PLL clock multiplier coefficients.
	 *
	 * The Intel documentation goes into some detail about how these
	 * constraint based (each component has a minimum and maximum
	 * value) and explains the equation that must be satisfied.
	 *
	 * Because we want to avoid too much fixed function maths in the
	 * driver, we've dumped the value after mode setting with Linux's
	 * Intel DRM driver. The value has been constant in our testing.
	 *
	 * This value is (1 << clock->n) << 16 | clock->m1 << 8 | clock->m2
	 *
	 * IOW `n' is 2, `m1' is 16 and `m2' is 7.
	 */
	ui32FP = 0x21007;

	/* The DPLL register contains the PLL clock divisors and some control
	 * bits. The bits that tend to be set are:
	 *
	 *  Bits  0 -  7 DPLL_FPA1_P1_POST_DIV_SHIFT  (stock clock divisor)
	 *  Bits 16 - 23 DPLL_FPA01_P1_POST_DIV_SHIFT (throttled clock divisor)
	 *  Bit  26      DPLLB_MODE_DAC_SERIAL        (for HDMI)
	 *  Bit  30      DPLL_DVO_HIGH_SPEED          (??)
	 *
	 * Because we need clock->p1, clock->p2, we'll hard-code these values,
	 * based on the mode's target clock.
	 */
	ui32DPLL = (1 << 26) | (1 << 30);
	switch(ui32Clock)
	{
		case 148500: /* 1080p / 1080p50 */
			ui32DPLL |= (2 << 0);
			ui32DPLL |= (2 << 16);
			break;
		case 74250:  /*  720p / 720p50  */
			ui32DPLL |= (8 << 0);
			ui32DPLL |= (8 << 16);
			break;
		default:
			pr_err("Mode set failed (unknown target clock).");
			goto err_out;
	}
	
	/* Could get the the panel bpp from EDID */
	ui32Bpp = 24;

	/* Figure out the number of lanes we'll need to achieve the target
	 * clock. The 21/20 thing is to ask for 5% more. This is poorly
	 * documented in the PRMs we could find, but the Intel DRM driver is
	 * doing it; apparently it improves stability when spectrum spread
	 * is in use.
	 */
	ui32Lanes = (ui32Clock * ui32Bpp * 21 / 20) / (ui32LinkClock * 8) + 1;

	/* The m1n1 data/link registers need to be programmed with raw clock
	 * values and a bit of magic.
	 *
	 * TU      Always (64 - 1). Or'ed with GMCH M.
	 * GMCH M  Target pixel clock * bits per pixel (kHz).
	 * GMCH N  Link clock / 10000 (10kHz units) * 8 (8 bits in a byte?) *
	 *         number of active lanes.
	 * LINK M  Target pixel clock
	 * LINK N  Link clock / 10000 (10kHz units).
	 */
	ui32GMCHm = ((64 - 1) << 25) | (ui32Clock * ui32Bpp);
	ui32GMCHn = ui32LinkClock * 8 * ui32Lanes;
	ui32LINKm = ui32Clock;
	ui32LINKn = ui32LinkClock;

	/* Program FP0 */
	DCI_A_WR(PCH_FP0, ui32FP);

	/* Write DPLL first with clock divisor bits */
	DCI_A_WR(PCH_DPLL, ui32DPLL);
	DCI_A_PR(PCH_DPLL);
	udelay(150);

	/* Write DPLL again */
	DCI_A_WR(PCH_DPLL, ui32DPLL);
	DCI_A_PR(PCH_DPLL);
	udelay(150);

	/* Write DPLL again ?? */
	DCI_A_WR(PCH_DPLL, ui32DPLL);

	/* Program FP1 */
	DCI_A_WR(PCH_FP1, ui32FP);

	/* ?? */
	DCI_A_WR(VSYNCSHIFT, 0);

	/* Horizontal timings */
	DCI_A_WDTR(HTOTAL, ui16HActive, ui16HActive + ui16HBlank);
	DCI_A_WDTR(HBLANK, ui16HActive, ui16HActive + ui16HBlank);
	DCI_A_WDTR(HSYNC, ui16HActive + ui16HSyncOffset,
					  ui16HActive + ui16HSyncOffset + ui16HSyncWidth);

	/* Vertical timings */
	DCI_A_WDTR(VTOTAL, ui16VActive, ui16VActive + ui16VBlank);
	DCI_A_WDTR(VBLANK, ui16VActive, ui16VActive + ui16VBlank);
	DCI_A_WDTR(VSYNC, ui16VActive + ui16VSyncOffset,
					  ui16VActive + ui16VSyncOffset + ui16VSyncWidth);

	/* Source dimensions */
	DCI_A_WDTR(PIPESRC, ui16VActive, ui16HActive);

	DCI_A_WR(PIPE_DATA_M1, ui32GMCHm);
	DCI_A_WR(PIPE_DATA_N1, ui32GMCHn);
	DCI_A_WR(PIPE_LINK_M1, ui32LINKm);
	DCI_A_WR(PIPE_LINK_N1, ui32LINKn);

	DCI_A_WR(PIPECONF, 0);
	DCI_A_PR(PIPECONF);

	/* Could wait for vblank instead */
	msleep(DC_INTEL_REG_VBLANK_ASSERT_TIMEOUT_MS);

	/* Enable plane(s) gamma correction */
	DCI_WR_OR_BITS(PRIMARYA_DSPCNTR, 0, GAMMA_ENABLE);
	DCI_PA_PR(DSPCNTR);
	DCI_WR_OR_BITS(SPRITEA_DVSCNTR,  0, GAMMA_ENABLE);
	DCI_SA_PR(DVSCNTR);

	*pui32Lanes = ui32Lanes;
	bRet = IMG_TRUE;
err_out:
	return bRet;
}

static void
ApplyPrimaryConfig(void *pvRegisters, DC_INTEL_PLANE_CONFIG *psConfig)
{
	IMG_UINT32 ui32RegVal, ui32IntelPixFmt;

	if(!psConfig->bEnabled)
	{
		ui32RegVal = DCI_PA_RR(DSPCNTR);
		if(DCI_RR_BITS(ui32RegVal, PRIMARYA_DSPCNTR, ENABLE))
			DCI_WR_MASK_BITS(PRIMARYA_DSPCNTR, ui32RegVal, ENABLE);
		return;
	}

	switch(psConfig->ePixFormat)
	{
		case IMG_PIXFMT_B5G6R5_UNORM:
			ui32IntelPixFmt = 5;  /* 0101b == 16-bit BGRX */
			break;
		case IMG_PIXFMT_B8G8R8A8_UNORM:
		case IMG_PIXFMT_B8G8R8X8_UNORM:
			ui32IntelPixFmt = 6;  /* 0110b == 32-bit BGRX */
			break;
		case IMG_PIXFMT_R8G8B8A8_UNORM:
		case IMG_PIXFMT_R8G8B8X8_UNORM:
			ui32IntelPixFmt = 14; /* 1110b == 32-bit RGBX */
			break;
		default:
			BUG();
	}

	ui32RegVal = DCI_PA_RR(DSPCNTR);

	if(!DCI_RR_BITS(ui32RegVal, PRIMARYA_DSPCNTR, ENABLE))
		ui32RegVal |= DC_INTEL_PRIMARYA_DSPCNTR_ENABLE_MASK;

	ui32RegVal &= ~DC_INTEL_PRIMARYA_DSPCNTR_PIXELFORMAT_MASK;
	ui32RegVal |=
		ui32IntelPixFmt << DC_INTEL_PRIMARYA_DSPCNTR_PIXELFORMAT_SHIFT;

	/* Ironlake workaround bit (must be enabled) */
	ui32RegVal |= 0x1 << 14;

	DCI_PA_WR(DSPCNTR, ui32RegVal);

	/* Stride in bytes */
	DCI_PA_WR(DSPSTRIDE, psConfig->ui32ByteStride);

	/* Display width/height, crop width/height, display x/y offsets are
	 * meaningless for the Primary, which always fills the whole screen.
	 *
	 * However, if the caller passes in an over-sized surface, the crop
	 * x/y offset can still be utilized.
	 */

	/* Offset vs DSPSURF to start of plane memory */
	DCI_PA_WR(DSPLINOFF,
		(psConfig->sCrop.i32YOffset * psConfig->ui32ByteStride) +
		(psConfig->sCrop.i32XOffset *
			IMGPixFmtGetBPP(psConfig->ePixFormat) / 8));

	/* Plane GTT offset */
	DCI_PA_WR(DSPSURF, psConfig->ui32GttPTEOffset << PAGE_SHIFT);

	/* Post the configuration */
	DCI_PA_PR(DSPSURF);
}

static void
ApplySpriteConfig(void *pvRegisters, DC_INTEL_PLANE_CONFIG *psConfig)
{
	IMG_UINT32 ui32IntelPixFmt = 0, ui32YUVByteOrder = 0, ui32RBSwap = 0;
	IMG_UINT32 ui32RegVal;

	if(!psConfig->bEnabled)
	{
		ui32RegVal = DCI_PA_RR(DSPCNTR);
		if(DCI_RR_BITS(ui32RegVal, SPRITEA_DVSCNTR, ENABLE))
			DCI_WR_MASK_BITS(SPRITEA_DVSCNTR, ui32RegVal, ENABLE);

		/* Disabling the sprite layer is a little more involved */
		DCI_SA_WR(DVSSCALE, 0);
		DCI_SA_WR(DVSSURF,  0);
		DCI_SA_PR(DVSSURF);
		return;
	}

	switch(psConfig->ePixFormat)
	{
		case IMG_PIXFMT_R8G8B8A8_UNORM:
		case IMG_PIXFMT_R8G8B8X8_UNORM:
			/* RB swapping isn't supported on Ironlake */
			//ui32RBSwap = 1;
		case IMG_PIXFMT_B8G8R8A8_UNORM:
		case IMG_PIXFMT_B8G8R8X8_UNORM:
			ui32IntelPixFmt = 2;  /* 11b == 32-bit BGRX */
			break;
		case IMG_PIXFMT_YUYV:
			ui32YUVByteOrder = 0;
			break;
		case IMG_PIXFMT_UYVY:
			ui32YUVByteOrder = 1;
			break;
		case IMG_PIXFMT_YVYU:
			ui32YUVByteOrder = 2;
			break;
		case IMG_PIXFMT_VYUY:
			ui32YUVByteOrder = 3;
			break;
		default:
			BUG();
	}

	ui32RegVal = DCI_SA_RR(DVSCNTR);

	if(!DCI_RR_BITS(ui32RegVal, SPRITEA_DVSCNTR, ENABLE))
		ui32RegVal |= DC_INTEL_SPRITEA_DVSCNTR_ENABLE_MASK;

	ui32RegVal &= ~(DC_INTEL_SPRITEA_DVSCNTR_PIXELFORMAT_MASK  |
					DC_INTEL_SPRITEA_DVSCNTR_YUVBYTEORDER_MASK |
					DC_INTEL_SPRITEA_DVSCNTR_RB_SWAP_MASK);
	ui32RegVal |=
		(ui32IntelPixFmt  << DC_INTEL_SPRITEA_DVSCNTR_PIXELFORMAT_SHIFT)  |
		(ui32YUVByteOrder << DC_INTEL_SPRITEA_DVSCNTR_YUVBYTEORDER_SHIFT) |
		(ui32RBSwap       << DC_INTEL_SPRITEA_DVSCNTR_RB_SWAP_SHIFT);

	/* Ironlake workaround bit (must be enabled) */
	ui32RegVal |= 0x1 << 14;

	DCI_SA_WR(DVSCNTR, ui32RegVal);

	/* Stride in bytes */
	DCI_SA_WR(DVSSTRIDE, psConfig->ui32ByteStride);

	/* Position */
	DCI_SA_WR(DVSPOS, psConfig->sDisplay.i32XOffset |
					  (psConfig->sDisplay.i32YOffset << 16));

	/* Offset vs DVSSURF to start of plane memory */
	DCI_SA_WR(DVSLINOFF,
		(psConfig->sCrop.i32YOffset * psConfig->ui32ByteStride) +
		(psConfig->sCrop.i32XOffset *
			IMGPixFmtGetBPP(psConfig->ePixFormat) / 8));

	/* Size */
	DCI_SA_WR(DVSSIZE, (psConfig->sDisplay.sDims.ui32Width - 1) |
					   ((psConfig->sDisplay.sDims.ui32Height - 1) << 16));

	/* Scaling (optionally) */
	if((psConfig->sCrop.sDims.ui32Width !=
	    psConfig->sDisplay.sDims.ui32Width) ||
	   (psConfig->sCrop.sDims.ui32Height !=
	    psConfig->sDisplay.sDims.ui32Height))
	{
		DCI_SA_WR(DVSSCALE, (1 << 31) |
							(psConfig->sCrop.sDims.ui32Height - 1) |
							((psConfig->sCrop.sDims.ui32Width - 1) << 16));
	}
	else
	{
		DCI_SA_WR(DVSSCALE, 0);
	}

	/* Plane GTT offset */
	DCI_SA_WR(DVSSURF, psConfig->ui32GttPTEOffset << PAGE_SHIFT);

	/* Post the configuration */
	DCI_SA_PR(DVSSURF);
}

static void
ApplyCursorConfig(void *pvRegisters, DC_INTEL_PLANE_CONFIG *psConfig)
{
	IMG_UINT32 ui32WidthHeight = psConfig->ui32ByteStride / 4;
	IMG_UINT32 ui32CursorMode, ui32RegVal = 0;

	if(!psConfig->bEnabled)
	{
		DCI_CA_WR(CURCNTR, 0);
		DCI_CA_WR(CURPOS,  0);
		DCI_CA_WR(CURBASE, 0);
		DCI_CA_PR(CURBASE);
		return;
	}

	switch(psConfig->ePixFormat)
	{
		case IMG_PIXFMT_R8G8B8A8_UNORM:
		case IMG_PIXFMT_R8G8B8X8_UNORM:
			/* Above are not actually supported by hardware.
			 * Make sure X is written as 0xFF or you won't see anything.
			 */
		case IMG_PIXFMT_B8G8R8A8_UNORM:
		case IMG_PIXFMT_B8G8R8X8_UNORM:
			break;
		default:
			BUG();
	}

	switch(ui32WidthHeight)
	{
		case 64:
			ui32CursorMode = 7;	/* 111b == 64x64 32bpp BGRA */
			break;
		case 128:
			ui32CursorMode = 2; /* 010b == 128x128 32bpp BGRA */
			break;
		case 256:
			ui32CursorMode = 3; /* 011b == 256x256 32bpp BGRA */
			break;
		default:
			BUG();
	}

	/* Gamma enable */
	ui32RegVal |= DC_INTEL_CURSORA_CURCNTR_GAMMA_ENABLE_MASK;

	/* Cursor Mode Select, part 1 */
	ui32RegVal |= DC_INTEL_CURSORA_CURCNTR_CURSORMODE2_MASK;

	/* Cursor Mode Select, part 2 */
	ui32RegVal |= ui32CursorMode << DC_INTEL_CURSORA_CURCNTR_CURSORMODE_SHIFT;

	DCI_CA_WR(CURCNTR, ui32RegVal);

	/* Position */
	ui32RegVal = 0;

	if(psConfig->sDisplay.i32XOffset >= 0)
		ui32RegVal |=   psConfig->sDisplay.i32XOffset;
	else
		ui32RegVal |=  -psConfig->sDisplay.i32XOffset | 0x8000;

	if(psConfig->sDisplay.i32YOffset >= 0)
		ui32RegVal |=   psConfig->sDisplay.i32YOffset << 16;
	else
		ui32RegVal |= (-psConfig->sDisplay.i32YOffset | 0x8000) << 16;

	DCI_CA_WR(CURPOS, ui32RegVal);

	/* Plane GTT offset */
	DCI_CA_WR(CURBASE, psConfig->ui32GttPTEOffset << PAGE_SHIFT);

	/* Post the configuration */
	DCI_CA_PR(CURBASE);
}

static void
ApplyPlanesConfig(void *pvRegisters, DC_INTEL_FLIP_CONFIG *psConfig)
{
	ApplyPrimaryConfig(pvRegisters, &psConfig->sPrimary);
	ApplySpriteConfig(pvRegisters, &psConfig->sSprite);
	ApplyCursorConfig(pvRegisters, &psConfig->sCursor);
}

static IMG_BOOL
FlipConfigQueue(void *pvRegisters, DC_INTEL_FLIP_CONFIG sConfig)
{
	DC_INTEL_FLIP_CONFIG *psConfig, *psItem;
	IMG_BOOL bRet = IMG_FALSE;

	psConfig = kmalloc(sizeof(DC_INTEL_FLIP_CONFIG), GFP_KERNEL);
	if(!psConfig)
	{
		pr_err("Failed to allocate on-screen config during enable.");
		goto err_out;
	}

	*psConfig = sConfig;

	mutex_lock(&gsFlipConfigMutex);

	if(!ConfigQueueSanityCheck())
		goto err_unlock;

	for(psItem = gpsConfigQueue; psItem; psItem = psItem->psNext)
		if(psItem->eState == DC_INTEL_FLIP_CONFIG_PENDING ||
		   psItem->eState == DC_INTEL_FLIP_CONFIG_PROGRAMMED)
			break;

	if(!psItem)
	{
		/* If there's no register program pending, we can go ahead
		 * and program synchronously in this function.
		 */
		ApplyPlanesConfig(pvRegisters, psConfig);
		psConfig->eState = DC_INTEL_FLIP_CONFIG_PROGRAMMED;
	}
	else
	{
		/* Otherwise we're backed up behind other programmings, so
		 * we'll have to wait for the flip worker to sort us out.
		 */
		psConfig->eState = DC_INTEL_FLIP_CONFIG_PENDING;
	}

	ConfigQueueAppend(psConfig);
	bRet = IMG_TRUE;
err_unlock:
	mutex_unlock(&gsFlipConfigMutex);
err_out:
	return bRet;
}

static IMG_BOOL FlipConfigFlush(void)
{
	int iTries;

	for(iTries = 0; iTries < DC_INTEL_FLUSH_RETRIES; iTries++)
	{
		mutex_lock(&gsFlipConfigMutex);

		if(!gpsConfigQueue->psNext)
		{
			if(gpsConfigQueue->eState == DC_INTEL_FLIP_CONFIG_DISPLAYED)
			{
				mutex_unlock(&gsFlipConfigMutex);
				break;
			}
		}

		mutex_unlock(&gsFlipConfigMutex);

		msleep(10);
	}

	return (iTries == DC_INTEL_FLUSH_RETRIES) ? IMG_FALSE : IMG_TRUE;
}

static IMG_BOOL
EnableDisplayEngine(void *pvRegisters, DC_INTEL_TYPE eDeviceType,
					IMG_UINT32 ui32Lanes, DC_INTEL_FLIP_CONFIG sConfig)
{
	IMG_UINT32 ui32RegVal;
	int i;

	IMG_UINT32 ui32DisplayMask =
		DC_INTEL_IRQ_MASTER_IRQ_CONTROL |
		DC_INTEL_IRQ_SPRITEA_FLIP_DONE |
		DC_INTEL_IRQ_PLANEA_FLIP_DONE |
		DC_INTEL_IRQ_GTT_FAULT;

	/* Write TU bits for error detection */
	DCI_A_WR(FDI_RX_TUSIZE1, (64 - 1) << 25);

	/* Enable RX PLL */
	ui32RegVal = DCI_A_RR(FDI_RX_CTL);
	/* magic numbers */
    ui32RegVal &= ~((7 << 19) | (7 << 16));
	ui32RegVal |= (ui32Lanes - 1) << 19;
	ui32RegVal |= DCI_RR_BITS(DCI_RR(PIPEA_PIPECONF), PIPEA_PIPECONF, BPC) << 11;
	DCI_WR_OR_BITS(PIPEA_FDI_RX_CTL, ui32RegVal, PLL_ENABLE);
	DCI_A_PR(FDI_RX_CTL);

	udelay(200);

	/* TX PLL is always enabled on Ironlake */
	ui32RegVal = DCI_A_RR(FDI_TX_CTL);
	if(!DCI_RR_BITS(ui32RegVal, PIPEA_FDI_TX_CTL, PLL_ENABLE))
	{
		DCI_WR_OR_BITS(PIPEA_FDI_TX_CTL, ui32RegVal, PLL_ENABLE);
		DCI_A_PR(FDI_TX_CTL);

		udelay(100);
	}

	/* The BIOS loads a weird non-linear palette. Upload a fresh one. */
	for(i = 0; i < 256; i++)
		DCI_A_WR(LGC_PALETTE + i * 4, (i << 16) | (i << 8) | i);

	ui32RegVal = DCI_A_RR(PIPECONF);
	if(!DCI_RR_BITS(ui32RegVal, PIPEA_PIPECONF, ENABLE))
	{
		DCI_WR_OR_BITS(PIPEA_PIPECONF, ui32RegVal, ENABLE);

		/* Could wait for vblank */
		msleep(DC_INTEL_REG_VBLANK_ASSERT_TIMEOUT_MS);
	}

	ui32RegVal = DCI_A_RR(FDI_RX_IMR);
	ui32RegVal &= ~DC_INTEL_FDI_RX_BIT_LOCK;
	ui32RegVal &= ~DC_INTEL_FDI_RX_SYMBOL_LOCK;
	DCI_A_WR(FDI_RX_IMR, ui32RegVal);
	DCI_A_PR(FDI_RX_IMR);

	udelay(150);

	ui32RegVal = DCI_A_RR(FDI_TX_CTL);
	/* magic numbers */
    ui32RegVal &= ~(7 << 19);
	ui32RegVal |= (ui32Lanes - 1) << 19;
	ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE;
	ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_1;

	if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
	{
		ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_VOL_EMP_MASK;
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_400MV_0DB_SNB_B;
	}

	DCI_WR_OR_BITS(PIPEA_FDI_TX_CTL, ui32RegVal, ENABLE);
	DCI_A_PR(FDI_TX_CTL);

	ui32RegVal = DCI_A_RR(FDI_RX_CTL);

	if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
	{
		ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE_CPT;
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_1_CPT;
	}
	else
	{
		ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE;
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_1;
	}

	DCI_WR_OR_BITS(PIPEA_FDI_RX_CTL, ui32RegVal, ENABLE);
	DCI_A_PR(FDI_RX_CTL);

	udelay(150);

	if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
	{
		ui32RegVal = DCI_RR(SOUTH_CHICKEN1);
		ui32RegVal |= DC_INTEL_FDIA_PHASE_SYNC_SHIFT_OVR;
		DCI_WR(SOUTH_CHICKEN1, ui32RegVal);
		ui32RegVal |= DC_INTEL_FDIA_PHASE_SYNC_SHIFT_EN;
		DCI_WR(SOUTH_CHICKEN1, ui32RegVal);
		DCI_PR(SOUTH_CHICKEN1);
	}
	else
	{
		/* Ibex Peak (series 5) workaround */
		DCI_A_WR(FDI_RX_CHICKEN, (1 << 1));
		DCI_A_WR(FDI_RX_CHICKEN, DCI_A_RR(FDI_RX_CHICKEN) | (1 << 0));
	}

	for(i = 0; i < 4; i++)
	{
		if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
		{
			static IMG_UINT32 ui32VolEmpLevels[4] =
			{
				DC_INTEL_FDI_LINK_TRAIN_400MV_0DB_SNB_B,
				DC_INTEL_FDI_LINK_TRAIN_400MV_6DB_SNB_B,
				DC_INTEL_FDI_LINK_TRAIN_600MV_3_5DB_SNB_B,
				DC_INTEL_FDI_LINK_TRAIN_800MV_0DB_SNB_B,
			};
 
			ui32RegVal = DCI_A_RR(FDI_TX_CTL);
			ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_VOL_EMP_MASK;
			ui32RegVal |= ui32VolEmpLevels[i];
			DCI_A_WR(FDI_TX_CTL, ui32RegVal);
			DCI_A_PR(FDI_TX_CTL);
			udelay(500);
		}

		ui32RegVal = DCI_A_RR(FDI_RX_IIR);
		if(ui32RegVal & DC_INTEL_FDI_RX_BIT_LOCK)
		{
			DCI_A_WR(FDI_RX_IIR, ui32RegVal);
			break;
		}
	}

	if(i == 4)
	{
		pr_err("Failed to complete FDI train 1.");
		return IMG_FALSE;
	}

	ui32RegVal = DCI_A_RR(FDI_TX_CTL);
	ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE;
	ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_2;

	if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
	{
		ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_VOL_EMP_MASK;
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_400MV_0DB_SNB_B;
	}

	DCI_A_WR(FDI_TX_CTL, ui32RegVal);
	DCI_A_PR(FDI_TX_CTL);

	ui32RegVal = DCI_A_RR(FDI_RX_CTL);

	if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
	{
		ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE_CPT;
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_2_CPT;
	}
	else
	{
		ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE;
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_2;
	}

	DCI_A_WR(FDI_RX_CTL, ui32RegVal);
	DCI_A_PR(FDI_RX_CTL);

	udelay(150);

	for(i = 0; i < 4; i++)
	{
		if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
		{
			static IMG_UINT32 ui32VolEmpLevels[4] =
			{
				DC_INTEL_FDI_LINK_TRAIN_400MV_0DB_SNB_B,
				DC_INTEL_FDI_LINK_TRAIN_400MV_6DB_SNB_B,
				DC_INTEL_FDI_LINK_TRAIN_600MV_3_5DB_SNB_B,
				DC_INTEL_FDI_LINK_TRAIN_800MV_0DB_SNB_B,
			};
 
			ui32RegVal = DCI_A_RR(FDI_TX_CTL);
			ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_VOL_EMP_MASK;
			ui32RegVal |= ui32VolEmpLevels[i];
			DCI_A_WR(FDI_TX_CTL, ui32RegVal);
			DCI_A_PR(FDI_TX_CTL);
			udelay(500);
		}

		ui32RegVal = DCI_A_RR(FDI_RX_IIR);
		if(ui32RegVal & DC_INTEL_FDI_RX_SYMBOL_LOCK)
		{
			DCI_A_WR(FDI_RX_IIR, ui32RegVal);
			break;
		}
	}

	if(i == 4)
	{
		pr_err("Failed to complete FDI train 2.");
		return IMG_FALSE;
	}

	DCI_WR_OR_BITS(PIPEA_PCH_DPLL, DCI_A_RR(PCH_DPLL), VCO_ENABLE);
	DCI_A_PR(PCH_DPLL);

	udelay(200);

	/* Integration point: SNB transcoder code */

	/* Copy pipe timings to transcoder */
	DCI_A_WR(TRANS_HTOTAL,		DCI_A_RR(HTOTAL));
	DCI_A_WR(TRANS_HBLANK,		DCI_A_RR(HBLANK));
	DCI_A_WR(TRANS_HSYNC,		DCI_A_RR(HSYNC));
	DCI_A_WR(TRANS_VTOTAL,		DCI_A_RR(VTOTAL));
	DCI_A_WR(TRANS_VBLANK,		DCI_A_RR(VBLANK));
	DCI_A_WR(TRANS_VSYNC,		DCI_A_RR(VSYNC));
	DCI_A_WR(TRANS_VSYNCSHIFT,	DCI_A_RR(VSYNCSHIFT));

	/* FDI normal train */
	ui32RegVal = DCI_A_RR(FDI_TX_CTL);
	ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE;
	ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_NONE |
				  DC_INTEL_FDI_TX_ENHANCE_FRAME_ENABLE;
	DCI_A_WR(FDI_TX_CTL, ui32RegVal);
	DCI_A_PR(FDI_TX_CTL);

	ui32RegVal = DCI_A_RR(FDI_RX_CTL);

	if(eDeviceType == DC_INTEL_TYPE_SANDYBRIDGE)
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_NONE_CPT;
	else
		ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_NONE;

	ui32RegVal |= DC_INTEL_FDI_RX_ENHANCE_FRAME_ENABLE;
	DCI_A_WR(FDI_RX_CTL, ui32RegVal);
	DCI_A_PR(FDI_RX_CTL);

	udelay(1000);

	/* Enable display interrupts for the events we care about */
	DCI_WR(DEIIR, DCI_RR(DEIIR));
	DCI_WR(DEIMR, ~ui32DisplayMask);
	DCI_WR(DEIER, ui32DisplayMask | DC_INTEL_IRQ_PIPEA_VBLANK);
	DCI_PR(DEIER);

	FlipConfigQueue(pvRegisters, sConfig);
	if(!FlipConfigFlush())
	{
		pr_err("Timed out waiting for flip to flush");
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static IMG_BOOL
DisableDisplayEngine(void *pvRegisters,
					 DC_INTEL_TYPE eDeviceType, IMG_BOOL bHaveInterrupts)
{
	DC_INTEL_FLIP_CONFIG sConfig = {};
	IMG_UINT32 ui32RegVal;
	int i;

	if(bHaveInterrupts)
	{
		FlipConfigQueue(pvRegisters, sConfig);
		FlipConfigFlush();
	}
	else
	{
		ApplyPlanesConfig(pvRegisters, &sConfig);
		msleep(DC_INTEL_REG_VBLANK_ASSERT_TIMEOUT_MS);
	}

	/* If anything left the display interrupts enabled, disable them */
	DCI_WR(HWSTAM, 0xFFFFFFFF);
	DCI_WR(DEIMR, 0xFFFFFFFF);
	DCI_WR(DEIER, 0x0);
	DCI_WR(DEIIR, DCI_RR(DEIIR));

	ui32RegVal = DCI_RR(PIPEA_PIPECONF);
	if(DCI_RR_BITS(ui32RegVal, PIPEA_PIPECONF, ENABLE))
		DCI_WR_MASK_BITS(PIPEA_PIPECONF, ui32RegVal, ENABLE);

	/* Wait until pipe disable asserts */
	for(i = 0; i < DC_INTEL_REG_VBLANK_ASSERT_TIMEOUT_MS; i++)
	{
		if(!DCI_RR_BITS(DCI_RR(PIPEA_PIPECONF), PIPEA_PIPECONF, STATE))
			break;
		msleep(1);
	}

	if(i == DC_INTEL_REG_VBLANK_ASSERT_TIMEOUT_MS)
		return IMG_FALSE;

	DCI_A_WR(PF_CTL, 0);
	DCI_A_WR(PF_WIN_SZ, 0);

	DCI_WR_MASK_BITS(PIPEA_FDI_TX_CTL, DCI_A_RR(FDI_TX_CTL), ENABLE);
	DCI_A_PR(FDI_TX_CTL);

	ui32RegVal = DCI_A_RR(FDI_RX_CTL);
	/* Magic numbers */
	ui32RegVal &= ~(7 << 16);
	ui32RegVal |= DCI_RR_BITS(DCI_RR(PIPEA_PIPECONF), PIPEA_PIPECONF, BPC) << 11;
	DCI_WR_MASK_BITS(PIPEA_FDI_RX_CTL, ui32RegVal, ENABLE);
	DCI_A_PR(FDI_RX_CTL);

	udelay(100);

	/* Ibex Peak (series 5) workaround */
	DCI_A_WR(FDI_RX_CHICKEN, (1 << 1));
	DCI_A_WR(FDI_RX_CHICKEN, DCI_A_RR(FDI_RX_CHICKEN) & ~(1 << 0));

	ui32RegVal = DCI_A_RR(FDI_TX_CTL);
	ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE;
	ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_1;
	DCI_A_WR(FDI_TX_CTL, ui32RegVal);
	DCI_A_PR(FDI_TX_CTL);

	ui32RegVal = DCI_A_RR(FDI_RX_CTL);
	ui32RegVal &= ~DC_INTEL_FDI_LINK_TRAIN_NONE;
	ui32RegVal |= DC_INTEL_FDI_LINK_TRAIN_PATTERN_1;
	/* Magic numbers */
	ui32RegVal &= ~(7 << 16);
	ui32RegVal |= DCI_RR_BITS(DCI_RR(PIPEA_PIPECONF), PIPEA_PIPECONF, BPC) << 11;
	DCI_A_WR(FDI_RX_CTL, ui32RegVal);
	DCI_A_PR(FDI_RX_CTL);

	udelay(100);

	DCI_WR_MASK_BITS(PIPEA_PCH_DPLL, DCI_A_RR(PCH_DPLL), VCO_ENABLE);
	DCI_A_PR(PCH_DPLL);

	udelay(200);

	return IMG_TRUE;
}

static DC_INTEL_TYPE GetDevice(IMG_UINT16 ui16DevId)
{
	int i;

	for(i = 0; gaui16PciDeviceIdMap[i][0] != 0; i++)
		if(gaui16PciDeviceIdMap[i][0] == ui16DevId)
			return (DC_INTEL_TYPE)gaui16PciDeviceIdMap[i][1];

	return DC_INTEL_TYPE_UNKNOWN;
}

static void
GetStolenAndGttPageTableSizeILK(IMG_UINT16 ui16RegVal,
								IMG_UINT32 *pui32StolenSize,
								IMG_UINT32 *pui32PageTableSize)
{
	*pui32StolenSize = (1 << ((ui16RegVal & 0xF0) >> 4)) * 1024 * 1024;

	/* Check the page table size -- block anything that isn't 2MB.
	 *
	 * This is because there is a weird mapping between the bits in
	 * the config word and the bits that have to be written to PGTBL.
	 */
	if(((ui16RegVal & 0x700) >> 8) == 3)
		*pui32PageTableSize = 2 * 1024 * 1024;
	else
		*pui32PageTableSize = 0;
}

static IMG_BOOL CheckGttSetupILK(void *pvRegisters)
{
	IMG_UINT ui32RegVal;

	/* Disable PPGTT */
	DCI_WR_MASK_BITS(PGTBL_CTL2, DCI_RR(PGTBL_CTL2), ENABLE);

	/* Write back to the GTT page table size to activate */
	ui32RegVal = DCI_RR(PGTBL_CTL);
	ui32RegVal &= ~DC_INTEL_PGTBL_CTL_SIZE_MASK;
	ui32RegVal |= (4 << DC_INTEL_PGTBL_CTL_SIZE_SHIFT);
	DCI_WR(PGTBL_CTL, ui32RegVal);

	/* Make sure it stuck to the 2MB setting */
	if(DCI_RR_BITS(DCI_RR(PGTBL_CTL), PGTBL_CTL, SIZE) != 4)
	{
		pr_err("Failed to enable page tables with 2MB size.");
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static void
GetStolenAndGttPageTableSizeSNB(IMG_UINT16 ui16RegVal,
								IMG_UINT32 *pui32StolenSize,
								IMG_UINT32 *pui32PageTableSize)
{
	switch((ui16RegVal & 0xF8) >> 3)
	{
		default: *pui32StolenSize =   0;               break;
		case 1:  *pui32StolenSize =  32 * 1024 * 1024; break;
		case 2:  *pui32StolenSize =  64 * 1024 * 1024; break;
		case 3:  *pui32StolenSize =  96 * 1024 * 1024; break;
		case 4:  *pui32StolenSize = 128 * 1024 * 1024; break;
		case 5:  *pui32StolenSize = 160 * 1024 * 1024; break;
		case 6:  *pui32StolenSize = 192 * 1024 * 1024; break;
		case 7:  *pui32StolenSize = 224 * 1024 * 1024; break;
		case 8:  *pui32StolenSize = 256 * 1024 * 1024; break;
		/* There are more cases above 256MB we don't handle */
	}

	/* Check the page table size -- block anything that isn't 2MB.
	 *
	 * This is because there is a weird mapping between the bits in
	 * the config word and the bits that have to be written to PGTBL.
	 */
	if(((ui16RegVal & 0x300) >> 8) == 2)
		*pui32PageTableSize = 2 * 1024 * 1024;
	else
		*pui32PageTableSize = 0;
}

static IMG_BOOL IsHDMIDDetectedAndEnabled(void *pvRegisters)
{
	IMG_UINT32 ui32RegVal = DCI_RR(HDMID_CTL);

	if(!DCI_RR_BITS(ui32RegVal, HDMID_CTL, PORT_DETECTED))
	{
		pr_err("HDMID port not detected. Not supported.");
		return IMG_FALSE;
	}

	if(!DCI_RR_BITS(ui32RegVal, HDMID_CTL, ENABLE))
	{
		pr_err("HDMID port not enabled. Not supported.");
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

static void
GetDTDWidthHeight(DC_INTEL_DTD *psDTD,
				  IMG_UINT32 *pui32Width, IMG_UINT32 *pui32Height)
{
	*pui32Width  = psDTD->ui8HActiveLow |
				   (((psDTD->ui8HActiveBlankHigh >> 4) & 0xf) << 8);
	*pui32Height = psDTD->ui8VActiveLow |
				   (((psDTD->ui8VActiveBlankHigh >> 4) & 0xf) << 8);
}

static inline void
GttInsertPageTableEntry(DC_INTEL_GTT_CONTEXT *psGtt, IMG_UINT32 ui32Offset,
						IMG_SYS_PHYADDR sPhyAddr)
{
	IMG_UINTPTR_T uiAddr = sPhyAddr.uiAddr;
	/* Should be 0xff0 on SNB */
	uiAddr |= (uiAddr >> 28) & 0xf0;
	/* Just 0x1 on ILK */
	uiAddr |= 0x1 | 0x2;
	writel(uiAddr, &psGtt->psPTEs[ui32Offset]);
}

static inline void
GttRemovePageTableEntry(DC_INTEL_GTT_CONTEXT *psGtt, IMG_UINT32 ui32Offset)
{
	writel(0, &psGtt->psPTEs[ui32Offset]);
}

static inline void
GttFinalizePageTableUpdate(DC_INTEL_GTT_CONTEXT *psGtt,
						   IMG_UINT32 ui32LastOffset)
{
	readl(&psGtt->psPTEs[ui32LastOffset]);
}

static IMG_UINT32
GttGetValidEntries(DC_INTEL_GTT_CONTEXT *psGtt, DC_INTEL_GTT_ZONE *psZ)
{
	IMG_UINT32 i, ui32Valid = 0;

	for(i = 0; i < psZ->ui32Total; i++)
		if(readl(&psGtt->psPTEs[psZ->ui32Offset + i]) & 0x1)
			ui32Valid++;

	return ui32Valid;
}

static void
GttInitPageTables(DC_INTEL_GTT_CONTEXT *psGtt)
{
	IMG_UINT32 i;

	/* Populate first part of GTT with the first part of the stolen
	 * region. This mapping is just intended for the Linux framebuffer
	 * carveout -- the rest of the stolen region is zoned.
	 *
	 * Normally the BIOS has already done this for us, but in case
	 * it hasn't, we'll do it again.
	 */
	for(i = 0; i < psGtt->sStolenZone.ui32Offset; i++)
	{
		IMG_SYS_PHYADDR sPhyAddr = {
			.uiAddr = psGtt->sStolenBase.uiAddr + ((IMG_UINT64)i * PAGE_SIZE),
		};
		GttInsertPageTableEntry(psGtt, i, sPhyAddr);
	}

	/* Zero the rest */
	for(; i < psGtt->ui32NumMappableGttEntries; i++)
		GttRemovePageTableEntry(psGtt, i);

	GttFinalizePageTableUpdate(psGtt, i - 1);

	/* Read back the page tables to ensure the zone counts are zero */
	BUG_ON(GttGetValidEntries(psGtt, &psGtt->sStolenZone) != 0);
	BUG_ON(GttGetValidEntries(psGtt, &psGtt->sDynamicZone) != 0);
}

static IMG_UINT32
GttMapDevicePagePhyAddrs(DC_INTEL_GTT_CONTEXT *psGtt, DC_INTEL_GTT_ZONE *psZ,
						 IMG_DEV_PHYADDR *asPhyAddrs, IMG_UINT32 ui32NumPages)
{
	IMG_UINT32 i, ui32AllocOffset = 0;

	BUG_ON(ui32NumPages == 0);

	if(psZ->ui32WriteOffset + ui32NumPages > psZ->ui32Total)
	{
//		pr_err("Exhausted Gtt zone, starting from beginning.");
		psZ->ui32WriteOffset = 0;
	}

	ui32AllocOffset = psZ->ui32Offset + psZ->ui32WriteOffset;

	for(i = 0; i < ui32NumPages; i++)
	{
		IMG_SYS_PHYADDR sPhyAddr = {
			.uiAddr = (IMG_UINTPTR_T)asPhyAddrs[i].uiAddr
		};
		GttInsertPageTableEntry(psGtt, ui32AllocOffset + i, sPhyAddr);
	}
	GttFinalizePageTableUpdate(psGtt, ui32AllocOffset + i - 1);

	psZ->ui32WriteOffset += ui32NumPages;
	return ui32AllocOffset;
}

static void
GttUnmapDevicePagePhyAddrs(DC_INTEL_GTT_CONTEXT *psGtt, DC_INTEL_GTT_ZONE *psZ,
						   IMG_UINT32 ui32GttPTEOffset, IMG_UINT32 ui32NumPages)
{
	IMG_UINT32 i;

	BUG_ON(ui32NumPages == 0);

	if(ui32GttPTEOffset < psZ->ui32Offset || ui32NumPages > psZ->ui32Total)
	{
		pr_err("Invalid Gtt alloc handle.");
		return;
	}

	for(i = 0; i < ui32NumPages; i++)
		GttRemovePageTableEntry(psGtt, ui32GttPTEOffset + i);
	GttFinalizePageTableUpdate(psGtt, ui32GttPTEOffset + i - 1);

	/* For now we don't have a proper region allocator, so we just fill up
	 * address space contiguously until exhausted, and don't attempt to
	 * re-use holes. As a special case, if the zone has no mapped pages,
	 * we know it's safe to reset the WriteOffset watermark to zero.
	 */
	if(!GttGetValidEntries(psGtt, psZ))
		psZ->ui32WriteOffset = 0;
}

static void FlipWorker(struct work_struct *work)
{
	DC_INTEL_FLIP_CONFIG *psItem;

	mutex_lock(&gsFlipConfigMutex);

	if(!gpsConfigQueue)
	{
		pr_err("Nothing in the queue? This shouldn't happen.");
		goto skip_remaining;
	}

	BUG_ON(!ConfigQueueSanityCheck());

	psItem = gpsConfigQueue;
	while(psItem)
	{
		DC_INTEL_FLIP_CONFIG *psNext = psItem->psNext;

		switch(psItem->eState)
		{
			/* If the front of the queue is a DISPLAYED buffer, it's coming
			 * off the screen and we can retire its configuration.
			 */
			case DC_INTEL_FLIP_CONFIG_DISPLAYED:
				if(psItem->hConfigData)
					DCDisplayConfigurationRetired(psItem->hConfigData);
				gpsConfigQueue = psNext;
				kfree(psItem);
				break;

			/* If it's a PROGRAMMED buffer, promote it to DISPLAYED */
			case DC_INTEL_FLIP_CONFIG_PROGRAMMED:
				psItem->eState = DC_INTEL_FLIP_CONFIG_DISPLAYED;
				break;

			/* If it's a PENDING buffer, program it, and promote it */
			case DC_INTEL_FLIP_CONFIG_PENDING:
				ApplyPlanesConfig(gpsDeviceData->pvRegisters, psItem);
				psItem->eState = DC_INTEL_FLIP_CONFIG_PROGRAMMED;

				/* Only process one PENDING buffer at a time */
				goto skip_remaining;
		}

		psItem = psNext;
	}

skip_remaining:
	mutex_unlock(&gsFlipConfigMutex);
}

static irqreturn_t IRQHandler(int irq, void *arg)
{
	DC_INTEL_DEVICE *psDeviceData = arg;
	void *pvRegisters = psDeviceData->pvRegisters;
	static IMG_UINT32 ui32NumGttFaults;
	IMG_UINT32 ui32DEIER, ui32DEIIR;
	int err = IRQ_NONE;

	ui32DEIER = DCI_RR(DEIER);
	DCI_WR(DEIER, ui32DEIER & ~DC_INTEL_IRQ_MASTER_IRQ_CONTROL);
	DCI_PR(DEIER);

	ui32DEIIR = DCI_RR(DEIIR);
	if(ui32DEIIR == 0)
		goto skip_handler;

	err = IRQ_HANDLED;

	if(ui32DEIIR & (DC_INTEL_IRQ_PLANEA_FLIP_DONE |
					DC_INTEL_IRQ_SPRITEA_FLIP_DONE))
	{
		queue_work(psDeviceData->psFlipWQ, &psDeviceData->sFlipWorker);
	}

	if(ui32DEIIR & DC_INTEL_IRQ_GTT_FAULT)
	{
		pr_err("GTT page fault raised (premature unmap of buffer?)");
		ui32NumGttFaults++;
	}

	if(ui32NumGttFaults == 10)
	{
		DCI_WR(DEIMR, DCI_RR(DEIMR) |  DC_INTEL_IRQ_GTT_FAULT);
		ui32DEIER &= ~DC_INTEL_IRQ_GTT_FAULT;
	}

	/* Nothing to do yet */
	DCI_WR(DEIIR, ui32DEIIR);

skip_handler:
	DCI_WR(DEIER, ui32DEIER);
	DCI_PR(DEIER);
	return err;
}

static void DestroyFB(struct fb_info *psInfo)
{
	fb_dealloc_cmap(&psInfo->cmap);
	iounmap(psInfo->screen_base);
	framebuffer_release(psInfo);
}

static int
SetColReg(unsigned iRegNo, unsigned iRed, unsigned iGreen,
		  unsigned iBlue, unsigned iTransp, struct fb_info *psInfo)
{
    if(iRegNo >= psInfo->cmap.len)
        return 1;

    if(iRegNo < 16)
	{
		iRed   >>= 16 - psInfo->var.red.length;
		iGreen >>= 16 - psInfo->var.green.length;
		iBlue  >>= 16 - psInfo->var.blue.length;
		((u32 *)(psInfo->pseudo_palette))[iRegNo] =
			(iRed   << psInfo->var.red.offset)   |
			(iGreen << psInfo->var.green.offset) |
			(iBlue  << psInfo->var.blue.offset);
    }

    return 0;
}

static int
RegisterFramebuffer(struct pci_dev *psPciDev, DC_INTEL_DTD *psDTD,
					IMG_PIXFMT ePixFormat, struct fb_info **ppsInfo,
					IMG_UINT32 ui32StolenSize)
{
	struct fb_info *psInfo;
	int err = -ENOMEM;

	static struct fb_ops sOps =
	{
		.owner			= THIS_MODULE,
		.fb_destroy		= DestroyFB,
		.fb_setcolreg	= SetColReg,
		.fb_fillrect	= cfb_fillrect,
		.fb_copyarea	= cfb_copyarea,
		.fb_imageblit	= cfb_imageblit,
	};

	struct fb_fix_screeninfo sFix =
	{
		.id				= DRVNAME,
		.type			= FB_TYPE_PACKED_PIXELS,
		.accel			= FB_ACCEL_NONE,
		.visual			= FB_VISUAL_TRUECOLOR,
	};

	struct fb_var_screeninfo sVar =
	{
		.activate		= FB_ACTIVATE_NOW,
		.vmode			= FB_VMODE_NONINTERLACED,
		.bits_per_pixel	= IMGPixFmtGetBPP(ePixFormat),
		.width			= FALLBACK_PHYSICAL_WIDTH,
		.height			= FALLBACK_PHYSICAL_HEIGHT,
	};

	DTDToFBModeLine(psDTD, &sVar.xres, &sVar.yres, &sVar.pixclock,
					&sVar.left_margin,  &sVar.right_margin,
					&sVar.upper_margin, &sVar.lower_margin,
					&sVar.hsync_len,    &sVar.vsync_len);

	sVar.xres_virtual = sVar.xres;
	sVar.yres_virtual = sVar.yres;

	sFix.line_length = sVar.xres * IMGPixFmtGetBPP(ePixFormat) / 8;

	switch(ePixFormat)
	{
		case IMG_PIXFMT_B8G8R8X8_UNORM:
			sVar.blue.length	= 8;
			sVar.green.length	= 8;
			sVar.red.length		= 8;
			sVar.blue.offset	= 0;
			sVar.green.offset	= 8;
			sVar.red.offset		= 16;
			break;

		case IMG_PIXFMT_R8G8B8X8_UNORM:
			sVar.red.length		= 8;
			sVar.green.length	= 8;
			sVar.blue.length	= 8;
			sVar.red.offset		= 0;
			sVar.green.offset	= 8;
			sVar.blue.offset	= 16;
			break;

		case IMG_PIXFMT_B5G6R5_UNORM:
			sVar.blue.length	= 5;
			sVar.green.length	= 6;
			sVar.red.length		= 5;
			sVar.blue.offset	= 0;
			sVar.green.offset	= 5;
			sVar.red.offset		= 11;
			break;

		default:
			BUG();
	}

	/* We must access the stolen region through the GTT host port. */
	sFix.smem_start = pci_resource_start(psPciDev, 2);
	sFix.smem_len = ui32StolenSize;

	psInfo = framebuffer_alloc(sizeof(u32) * 16, &psPciDev->dev);
	if(!psInfo)
	{
		pr_err("Failed to allocate framebuffer info.");
		goto err_out;
	}

	/* Use the private data we allocated as the pseudo palette */
	psInfo->pseudo_palette = psInfo->par;

	psInfo->apertures = alloc_apertures(1);
	if(!psInfo->apertures)
	{
		pr_err("Failed to allocate framebuffer aperture.");
		goto err_release;
	}

	psInfo->apertures->ranges[0].base = sFix.smem_start;
	psInfo->apertures->ranges[0].size = sFix.smem_len;

	psInfo->screen_base = ioremap_wc(sFix.smem_start, sFix.smem_len);
	if(!psInfo->screen_base)
	{
		pr_err("Failed to create CPU mapping of framebuffer.");
		goto err_release;
	}

	psInfo->fbops = &sOps;
	psInfo->var   = sVar;
	psInfo->fix   = sFix;
	psInfo->flags = FBINFO_FLAG_DEFAULT;

	err = fb_alloc_cmap(&psInfo->cmap, 256, 0);
	if(err)
	{
		pr_err("Failed to allocate character map.");
		goto err_iounmap;
	}

	err = register_framebuffer(psInfo);
	if(err)
	{
		pr_err("Failed to register framebuffer.");
		goto err_dealloc_cmap;
	}

	*ppsInfo = psInfo;
err_out:
	return err;

err_dealloc_cmap:
	fb_dealloc_cmap(&psInfo->cmap);
err_iounmap:
	iounmap(psInfo->screen_base);
err_release:
	framebuffer_release(psInfo);
	goto err_out;
}

static DC_INTEL_PLANE_CONFIG
GetPrimaryConfigFromFBVar(struct fb_var_screeninfo *psVar,
						  IMG_PIXFMT ePixFormat)
{
	return (DC_INTEL_PLANE_CONFIG){
		.bEnabled			= IMG_TRUE,
		.ePixFormat			= ePixFormat,
		.ui32ByteStride		= psVar->xres * IMGPixFmtGetBPP(ePixFormat) / 8,
		.ui32GttPTEOffset	= 0,
		.sCrop.sDims.ui32Width		= psVar->xres,
		.sCrop.sDims.ui32Height		= psVar->yres,
		.sDisplay.sDims.ui32Width	= psVar->xres,
		.sDisplay.sDims.ui32Height	= psVar->yres,
	};
}

static
DC_INTEL_BUFFER *CreateBuffer(DC_INTEL_CONTEXT *psDeviceContext,
							  IMG_UINT32 ui32Width,
							  IMG_UINT32 ui32Height,
							  IMG_PIXFMT ePixFormat,
							  IMG_UINT32 ui32BPP,
							  IMG_UINT32 ui32ByteStride)
{
	DC_INTEL_BUFFER *psBuffer = IMG_NULL;

	if(IMGPixFmtGetBPP(ePixFormat) == 0)
	{
		pr_err("%s: ePixFormat '%u' not DC_INTEL compatible\n", __func__,
			   ePixFormat);
		goto err_out;
	}

	if(!ui32Width || !ui32Height || !ui32ByteStride || !ui32BPP)
	{
		pr_err("%s: wrong buffer configuration "
			   "(w=%u, h=%u, s=%u, bpp=%u)\n", __func__,
			   ui32Width, ui32Height, ui32ByteStride, ui32BPP);
		goto err_out;
	}

	if(ui32Width * ui32BPP > ui32ByteStride)
	{
		pr_err("%s: stride to low for buffer configuration "
			   "(w=%u, h=%u, s=%u, bpp=%u)\n", __func__,
			   ui32Width, ui32Height, ui32ByteStride, ui32BPP);
		goto err_out;
	}

	psBuffer = kmalloc(sizeof(DC_INTEL_BUFFER), GFP_KERNEL);
	if(!psBuffer)
		goto err_out;

	psBuffer->psDeviceContext	= psDeviceContext;
	psBuffer->ui32Width			= ui32Width;
	psBuffer->ui32Height		= ui32Height;
	psBuffer->ePixFormat		= ePixFormat;
	psBuffer->ui32ByteStride	= ui32ByteStride;

	/* These might be set up later */
	psBuffer->ui32ByteSize		= 0;
	psBuffer->hImport			= IMG_NULL;
	psBuffer->ui32GttPTEOffset	= 0;

err_out:
	return psBuffer;
}

static
IMG_VOID DC_INTEL_GetInfo(IMG_HANDLE hDeviceData,
						  DC_DISPLAY_INFO *psDisplayInfo)
{
	PVR_UNREFERENCED_PARAMETER(hDeviceData);

	strncpy(psDisplayInfo->szDisplayName, DRVNAME " 1", DC_NAME_SIZE);

	psDisplayInfo->ui32MinDisplayPeriod	= 0;
	psDisplayInfo->ui32MaxDisplayPeriod	= 1;
	psDisplayInfo->ui32MaxPipes			= MAX_OVERLAY_PIPES;
	psDisplayInfo->bUnlatchedSupported	= IMG_FALSE;
}

static
PVRSRV_ERROR DC_INTEL_PanelQueryCount(IMG_HANDLE hDeviceData,
										IMG_UINT32 *pui32NumPanels)
{
	PVR_UNREFERENCED_PARAMETER(hDeviceData);
	*pui32NumPanels = 1;
	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_INTEL_PanelQuery(IMG_HANDLE hDeviceData,
								 IMG_UINT32 ui32PanelsArraySize,
								 IMG_UINT32 *pui32NumPanels,
								 PVRSRV_PANEL_INFO *psPanelInfo)
{
	DC_INTEL_DEVICE *psDeviceData = hDeviceData;
	struct fb_var_screeninfo *psVar = &psDeviceData->psLINFBInfo->var;

	*pui32NumPanels = 1;

	psPanelInfo[0].sSurfaceInfo.sFormat.ePixFormat = psDeviceData->ePixFormat;
	psPanelInfo[0].sSurfaceInfo.sDims.ui32Width    = psVar->xres;
	psPanelInfo[0].sSurfaceInfo.sDims.ui32Height   = psVar->yres;

	psPanelInfo[0].ui32RefreshRate = 1000000000LU /
		((psVar->upper_margin + psVar->lower_margin +
		  psVar->yres + psVar->vsync_len) *
		 (psVar->left_margin  + psVar->right_margin +
		  psVar->xres + psVar->hsync_len) *
		 (psVar->pixclock / 1000));

	psPanelInfo[0].ui32XDpi =
		((int)psVar->width > 0) ? 254000 / psVar->width * psVar->xres / 10000 : FALLBACK_DPI;
	psPanelInfo[0].ui32YDpi	=
		((int)psVar->height > 0) ? 254000 / psVar->height * psVar->yres / 10000 : FALLBACK_DPI;

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_INTEL_FormatQuery(IMG_HANDLE hDeviceData,
								  IMG_UINT32 ui32NumFormats,
								  PVRSRV_SURFACE_FORMAT *pasFormat,
								  IMG_UINT32 *pui32Supported)
{
	int i;

	for(i = 0; i < ui32NumFormats; i++)
	{
		pui32Supported[i] = 0;

		if(IMGPixFmtGetBPP(pasFormat[i].ePixFormat))
			pui32Supported[i]++;
	}

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_INTEL_DimQuery(IMG_HANDLE hDeviceData,
							   IMG_UINT32 ui32NumDims,
							   PVRSRV_SURFACE_DIMS *psDim,
							   IMG_UINT32 *pui32Supported)
{
	DC_INTEL_DEVICE *psDeviceData = hDeviceData;
	struct fb_var_screeninfo *psVar = &psDeviceData->psLINFBInfo->var;
	int i;

	for(i = 0; i < ui32NumDims; i++)
	{
		pui32Supported[i] = 0;

		if(psDim[i].ui32Width  == psVar->xres &&
		   psDim[i].ui32Height == psVar->yres)
			pui32Supported[i]++;
	}

	return PVRSRV_OK;
}

static
PVRSRV_ERROR DC_INTEL_ContextCreate(IMG_HANDLE hDeviceData,
								    IMG_HANDLE *hDisplayContext)
{
	DC_INTEL_CONTEXT *psDeviceContext;
	PVRSRV_ERROR eError = PVRSRV_OK;

	psDeviceContext = kzalloc(sizeof(DC_INTEL_CONTEXT), GFP_KERNEL);
	if(!psDeviceContext)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	psDeviceContext->psDeviceData = hDeviceData;
	*hDisplayContext = psDeviceContext;

err_out:
	return eError;
}

static PVRSRV_ERROR ConfigureCheckPrimary(IMG_UINT32 ui32DisplayWidth,
										  IMG_UINT32 ui32DisplayHeight,
										  PVRSRV_SURFACE_RECT *psCrop,
										  PVRSRV_SURFACE_RECT *psDisplay,
										  DC_INTEL_BUFFER *psBuffer)
{
	/* Check the pixel format is compatible */
	switch(psBuffer->ePixFormat)
	{
		case IMG_PIXFMT_B5G6R5_UNORM:
		case IMG_PIXFMT_B8G8R8A8_UNORM:
		case IMG_PIXFMT_B8G8R8X8_UNORM:
		case IMG_PIXFMT_R8G8B8A8_UNORM:
		case IMG_PIXFMT_R8G8B8X8_UNORM:
			break;
		default:
			return PVRSRV_ERROR_UNSUPPORTED_PIXEL_FORMAT;
	}

	/* The Primary must map every screen pixel, so we need to make
	 * sure the display rectangle is doing this.
	 */
	if(psDisplay->i32XOffset != 0 ||
	   psDisplay->i32YOffset != 0 ||
	   psDisplay->sDims.ui32Width  != ui32DisplayWidth ||
	   psDisplay->sDims.ui32Height != ui32DisplayHeight)
	{
		pr_err("[PRIMARY] Invalid display rectangle {%d,%d,%u,%u}",
			   psDisplay->i32XOffset, psDisplay->i32YOffset,
			   psDisplay->sDims.ui32Width, psDisplay->sDims.ui32Height);
		return PVRSRV_ERROR_DC_INVALID_DISPLAY_RECT;
	}

	/* The Primary doesn't support scaling. We can support crops of
	 * over-sized surfaces, as long as the total crop size matches the
	 * screen size.
	 */
	if(psCrop->sDims.ui32Width  != ui32DisplayWidth ||
	   psCrop->sDims.ui32Height != ui32DisplayHeight)
	{
		pr_err("[PRIMARY] Invalid crop rectangle {%d,%d,%u,%u}",
			   psCrop->i32XOffset, psCrop->i32YOffset,
			   psCrop->sDims.ui32Width, psCrop->sDims.ui32Height);
		return PVRSRV_ERROR_DC_INVALID_CROP_RECT;
	}

	/* The HWC should have checked this, but just to be sure we don't
	 * trigger Gtt page faults, make sure the buffers are big enough
	 * for the configured display/crop.
	 */
	if(psBuffer->ui32Width  != psCrop->sDims.ui32Width ||
	   psBuffer->ui32Height != psCrop->sDims.ui32Height)
	{
		pr_err("[PRIMARY] Invalid buffer dimensions {%u,%u}",
			   psBuffer->ui32Width,
			   psBuffer->ui32Height);
		return PVRSRV_ERROR_DC_INVALID_BUFFER_DIMS;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR ConfigureCheckSprite(IMG_UINT32 ui32DisplayWidth,
										 IMG_UINT32 ui32DisplayHeight,
										 PVRSRV_SURFACE_RECT *psCrop,
										 PVRSRV_SURFACE_RECT *psDisplay,
										 DC_INTEL_BUFFER *psBuffer)
{
	IMG_UINT32 ui32BPP = IMGPixFmtGetBPP(psBuffer->ePixFormat);

	/* Check the pixel format is compatible */
	switch(psBuffer->ePixFormat)
	{
		case IMG_PIXFMT_R8G8B8A8_UNORM:
		case IMG_PIXFMT_R8G8B8X8_UNORM:
		case IMG_PIXFMT_B8G8R8A8_UNORM:
		case IMG_PIXFMT_B8G8R8X8_UNORM:
		case IMG_PIXFMT_YUYV:
		case IMG_PIXFMT_UYVY:
		case IMG_PIXFMT_YVYU:
		case IMG_PIXFMT_VYUY:
			break;
		default:
			return PVRSRV_ERROR_UNSUPPORTED_PIXEL_FORMAT;
	}

	/* The Sprite can scale, but only if a line of source pixels is <4KB. */
	if((psCrop->sDims.ui32Width  != psDisplay->sDims.ui32Width ||
	    psCrop->sDims.ui32Height != psDisplay->sDims.ui32Height) &&
	   ((psCrop->sDims.ui32Width * ui32BPP / 8) > 4096))
	{
		pr_err("[SPRITE] Invalid crop width %u", psCrop->sDims.ui32Width);
		return PVRSRV_ERROR_DC_INVALID_CROP_RECT;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR ConfigureCheckCursor(IMG_UINT32 ui32DisplayWidth,
										 IMG_UINT32 ui32DisplayHeight,
										 PVRSRV_SURFACE_RECT *psCrop,
										 PVRSRV_SURFACE_RECT *psDisplay,
										 DC_INTEL_BUFFER *psBuffer)
{
	/* Check the pixel format is compatible */
	switch(psBuffer->ePixFormat)
	{
		case IMG_PIXFMT_R8G8B8A8_UNORM:
		case IMG_PIXFMT_R8G8B8X8_UNORM:
		case IMG_PIXFMT_B8G8R8A8_UNORM:
		case IMG_PIXFMT_B8G8R8X8_UNORM:
			break;
		default:
			return PVRSRV_ERROR_UNSUPPORTED_PIXEL_FORMAT;
	}

	if(psCrop->i32XOffset != 0 || psCrop->i32YOffset != 0)
	{
		pr_err("[CURSOR] Crop cannot be offset {%d,%d}",
			   psCrop->i32XOffset, psCrop->i32YOffset);
		return PVRSRV_ERROR_DC_INVALID_CROP_RECT;
	}

	if(psCrop->sDims.ui32Width  != psDisplay->sDims.ui32Width ||
	   psCrop->sDims.ui32Height != psDisplay->sDims.ui32Height)
	{
		pr_err("[CURSOR] Crop {%ux%u} != Display {%ux%u}",
			   psCrop->sDims.ui32Width,
			   psCrop->sDims.ui32Height,
			   psDisplay->sDims.ui32Width,
			   psDisplay->sDims.ui32Height);
		return PVRSRV_ERROR_DC_INVALID_CONFIG;
	}

	/* We won't check for square-ness, as it doesn't matter if we
	 * have pixels left over, but we must check that the "real" width
	 * and stride are OK.
	 */
	if((psBuffer->ui32Width != psBuffer->ui32ByteStride / 4) ||
	   (psBuffer->ui32Width != 64 &&
	    psBuffer->ui32Width != 128 &&
	    psBuffer->ui32Width != 256))
	{
		pr_err("[CURSOR] Invalid buffer width or stride {%u,%u}",
			   psBuffer->ui32Width,
			   psBuffer->ui32ByteStride);
		return PVRSRV_ERROR_DC_INVALID_BUFFER_DIMS;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR
DC_INTEL_ContextConfigureCheck(IMG_HANDLE hDisplayContext,
							   IMG_UINT32 ui32PipeCount,
							   PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
							   IMG_HANDLE *ahBuffers)
{
	DC_INTEL_CONTEXT *psDeviceContext = hDisplayContext;
	DC_INTEL_DEVICE *psDeviceData = psDeviceContext->psDeviceData;
	IMG_BOOL abOverlayConfigured[MAX_OVERLAY_PIPES] = {};
	IMG_UINT32 i, ui32DisplayWidth, ui32DisplayHeight;
	PVRSRV_ERROR eError;

	if(ui32PipeCount > MAX_OVERLAY_PIPES)
	{
		eError = PVRSRV_ERROR_DC_TOO_MANY_PIPES;
		goto err_out;
	}

	if(!ahBuffers)
	{
		eError = PVRSRV_ERROR_DC_INVALID_CONFIG;
		goto err_out;
	}

	ui32DisplayWidth  = psDeviceData->psLINFBInfo->var.xres;
	ui32DisplayHeight = psDeviceData->psLINFBInfo->var.yres;

	for(i = 0; i < ui32PipeCount; i++)
	{
		typedef PVRSRV_ERROR (*pfnPlaneCheck_t)(IMG_UINT32, IMG_UINT32,
												PVRSRV_SURFACE_RECT *,
												PVRSRV_SURFACE_RECT *,
												DC_INTEL_BUFFER *);
		pfnPlaneCheck_t pfnPlaneCheck;

		if(pasSurfAttrib[i].ui32Custom >= MAX_OVERLAY_PIPES)
		{
			pr_err("%s: Bad overlay index=%u", __func__,
				   pasSurfAttrib[i].ui32Custom);
			eError = PVRSRV_ERROR_DC_INVALID_CUSTOM;
			goto err_out;
		}

		if(abOverlayConfigured[pasSurfAttrib[i].ui32Custom] == IMG_TRUE)
		{
			pr_err("%s: Overlay index=%u configured more than once", __func__,
				   pasSurfAttrib[i].ui32Custom);
			eError = PVRSRV_ERROR_DC_INVALID_CUSTOM;
			goto err_out;
		}

		abOverlayConfigured[pasSurfAttrib[i].ui32Custom] = IMG_TRUE;

		if(pasSurfAttrib[i].ui32Transform)
		{
			pr_err("%s: Non-zero transforms unsupported\n", __func__);
			eError = PVRSRV_ERROR_DC_INVALID_TRANSFORM;
			goto err_out;
		}

		if(pasSurfAttrib[i].sDisplay.sDims.ui32Width > ui32DisplayWidth)
		{
			pr_err("%s: Bad Width=%u\n", __func__,
				   pasSurfAttrib[i].sDisplay.sDims.ui32Width);
			eError = PVRSRV_ERROR_DC_INVALID_DISPLAY_RECT;
			goto err_out;
		}

		if(pasSurfAttrib[i].sDisplay.sDims.ui32Height > ui32DisplayHeight)
		{
			pr_err("%s: Bad Height=%u\n", __func__,
				   pasSurfAttrib[i].sDisplay.sDims.ui32Height);
			eError = PVRSRV_ERROR_DC_INVALID_DISPLAY_RECT;
			goto err_out;
		}

		if(pasSurfAttrib[i].sDisplay.i32XOffset >= (IMG_INT32)ui32DisplayWidth)
		{
			pr_err("%s: Bad XOffset=%d\n", __func__,
				   pasSurfAttrib[i].sDisplay.i32XOffset);
			eError = PVRSRV_ERROR_DC_INVALID_DISPLAY_RECT;
			goto err_out;
		}

		if(pasSurfAttrib[i].sDisplay.i32YOffset >= (IMG_INT32)ui32DisplayHeight)
		{
			pr_err("%s: Bad YOffset=%u\n", __func__,
				   pasSurfAttrib[i].sDisplay.i32YOffset);
			eError = PVRSRV_ERROR_DC_INVALID_DISPLAY_RECT;
			goto err_out;
		}

		if(pasSurfAttrib[i].ui8PlaneAlpha != 0xff)
		{
			pr_err("%s: Bad PlaneAlpha=%u\n", __func__,
				   pasSurfAttrib[i].ui8PlaneAlpha);
			eError = PVRSRV_ERROR_DC_INVALID_PLANE_ALPHA;
			goto err_out;
		}

		if(pasSurfAttrib[i].ui32Custom == 2)
			pfnPlaneCheck = ConfigureCheckCursor;
		else if(pasSurfAttrib[i].ui32Custom == 1)
			pfnPlaneCheck = ConfigureCheckSprite;
		else
			pfnPlaneCheck = ConfigureCheckPrimary;

		eError = pfnPlaneCheck(ui32DisplayWidth, ui32DisplayHeight,
							   &pasSurfAttrib[i].sCrop,
							   &pasSurfAttrib[i].sDisplay,
							   ahBuffers[i]);
		if(eError)
			goto err_out;
	}

	eError = PVRSRV_OK;
err_out:
	return eError;
}

static
IMG_VOID DC_INTEL_ContextConfigure(IMG_HANDLE hDisplayContext,
								   IMG_UINT32 ui32PipeCount,
								   PVRSRV_SURFACE_CONFIG_INFO *pasSurfAttrib,
								   IMG_HANDLE *ahBuffers,
								   IMG_UINT32 ui32DisplayPeriod,
								   IMG_HANDLE hConfigData)
{
	DC_INTEL_CONTEXT *psDeviceContext = hDisplayContext;
	DC_INTEL_DEVICE *psDeviceData = psDeviceContext->psDeviceData;
	DC_INTEL_FLIP_CONFIG sConfig = {};
	IMG_UINT32 i;

	PVR_UNREFERENCED_PARAMETER(ui32DisplayPeriod);

	/* If the pipe count is zero, we're tearing down. Don't record
	 * any new configurations, and pan the display back to the
	 * Linux primary surface. This should unblock any previously
	 * programmed DC configurations.
	 *
	 * The flush is needed here because if this cleanup was triggered
	 * by resman, it will quickly follow up with BufferRelease, and
	 * we can avoid Gtt page faults being raised by flushing out
	 * any dynamic mappings.
	 */
	if(ui32PipeCount == 0)
	{
		sConfig.sPrimary =
			GetPrimaryConfigFromFBVar(&psDeviceData->psLINFBInfo->var,
									  psDeviceData->ePixFormat);
		FlipConfigQueue(psDeviceData->pvRegisters, sConfig);
		FlipConfigFlush();
		DCDisplayConfigurationRetired(hConfigData);
		return;
	}

	for(i = 0; i < ui32PipeCount; i++)
	{
		DC_INTEL_BUFFER *psBuffer = ahBuffers[i];
		DC_INTEL_PLANE_CONFIG *psConfig;

		switch(pasSurfAttrib[i].ui32Custom)
		{
			default:
				psConfig = &sConfig.sPrimary;
				break;
			case 1:
				psConfig = &sConfig.sSprite;
				break;
			case 2:
				psConfig = &sConfig.sCursor;
				break;
		}

		psConfig->bEnabled			= IMG_TRUE;
		psConfig->ePixFormat		= psBuffer->ePixFormat;
		psConfig->ui32ByteStride	= psBuffer->ui32ByteStride;
		psConfig->sCrop				= pasSurfAttrib[i].sCrop;
		psConfig->sDisplay			= pasSurfAttrib[i].sDisplay;
		psConfig->ui32GttPTEOffset	= psBuffer->ui32GttPTEOffset;
	}

	sConfig.hConfigData = hConfigData;
	FlipConfigQueue(psDeviceData->pvRegisters, sConfig);
}

static
IMG_VOID DC_INTEL_ContextDestroy(IMG_HANDLE hDisplayContext)
{
	DC_INTEL_CONTEXT *psDeviceContext = hDisplayContext;
	kfree(psDeviceContext);
}

static
PVRSRV_ERROR DC_INTEL_BufferAlloc(IMG_HANDLE hDisplayContext,
								  DC_BUFFER_CREATE_INFO *psCreateInfo,
								  IMG_DEVMEM_LOG2ALIGN_T *puiLog2PageSize,
								  IMG_UINT32 *pui32PageCount,
								  IMG_UINT32 *pui32PhysHeapID,
								  IMG_UINT32 *pui32ByteStride,
								  IMG_HANDLE *phBuffer)
{
	DC_INTEL_CONTEXT *psDeviceContext = hDisplayContext;
	PVRSRV_SURFACE_INFO *psSurfInfo = &psCreateInfo->sSurface;
	PVRSRV_ERROR eError = PVRSRV_OK;
	DC_INTEL_BUFFER *psBuffer;

	psBuffer = CreateBuffer(psDeviceContext,
							psSurfInfo->sDims.ui32Width,
							psSurfInfo->sDims.ui32Height,
							psCreateInfo->sSurface.sFormat.ePixFormat,
							psCreateInfo->ui32BPP,
							psSurfInfo->sDims.ui32Width *
								psCreateInfo->ui32BPP);
	if(!psBuffer)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	psBuffer->ui32ByteSize =
		PAGE_ALIGN(psBuffer->ui32ByteStride * psBuffer->ui32Height);

	*puiLog2PageSize = PAGE_SHIFT;
	*pui32PageCount	 = psBuffer->ui32ByteSize >> PAGE_SHIFT;
	*pui32PhysHeapID = DC_PHYS_HEAP_ID;
	*pui32ByteStride = psBuffer->ui32ByteStride;
	*phBuffer		 = psBuffer;

err_out:
	return eError;
}

static
PVRSRV_ERROR DC_INTEL_BufferImport(IMG_HANDLE hDisplayContext,
								   IMG_UINT32 ui32NumPlanes,
								   IMG_HANDLE **paphImport,
								   DC_BUFFER_IMPORT_INFO *psSurfInfo,
								   IMG_HANDLE *phBuffer)
{
	DC_INTEL_CONTEXT *psDeviceContext = hDisplayContext;
	PVRSRV_ERROR eError = PVRSRV_OK;
	DC_INTEL_BUFFER *psBuffer;

	if(ui32NumPlanes > 1)
	{
		eError = PVRSRV_ERROR_DC_INVALID_CONFIG;
		goto err_out;
	}

	psBuffer = CreateBuffer(psDeviceContext,
							psSurfInfo->ui32Width[0],
							psSurfInfo->ui32Height[0],
							psSurfInfo->ePixFormat,
							psSurfInfo->ui32BPP,
							psSurfInfo->ui32ByteStride[0]);
	if(!psBuffer)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	psBuffer->hImport = paphImport[0];
	*phBuffer = psBuffer;

err_out:
	return eError;
}

static
PVRSRV_ERROR DC_INTEL_BufferAcquire(IMG_HANDLE hBuffer,
									IMG_DEV_PHYADDR *pasDevPAddr,
									IMG_PVOID *ppvLinAddr)
{
	DC_INTEL_BUFFER *psBuffer = hBuffer;
	DC_INTEL_DEVICE *psDeviceData = psBuffer->psDeviceContext->psDeviceData;
	DC_INTEL_GTT_CONTEXT *psGtt = &psDeviceData->sGtt;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 i, ui32NumPages;
	IMG_UINT64 ui64BaseAddr;

	ui32NumPages = psBuffer->ui32ByteSize >> PAGE_SHIFT;

	/* Map more of the stolen region into the Gtt's stolen zone. If this
	 * mapping fails, it means we don't have any address space left.
	 *
	 * We can figure out where to start mapping the stolen region using
	 * the stolen zone offset.
	 */

	ui64BaseAddr = psGtt->sStolenBase.uiAddr +
				   ((IMG_UINT64)psGtt->sStolenZone.ui32Offset << PAGE_SHIFT) +
				   ((IMG_UINT64)psGtt->sStolenZone.ui32WriteOffset << PAGE_SHIFT);
	for(i = 0; i < ui32NumPages; i++)
		pasDevPAddr[i].uiAddr = ui64BaseAddr + ((IMG_UINT64)i * PAGE_SIZE);

	psBuffer->ui32GttPTEOffset =
		GttMapDevicePagePhyAddrs(psGtt, &psGtt->sStolenZone,
								 pasDevPAddr, ui32NumPages);
	if(!psBuffer->ui32GttPTEOffset)
	{
		eError = PVRSRV_ERROR_OUT_OF_MEMORY;
		goto err_out;
	}

	/* We had to use the real stolen region physical address to program
	 * the GTT, but services uses these addresses to establish a user
	 * mapping (for CPU access, etc.) and the PCH blocks writes from
	 * the CPU to the stolen region.
	 *
	 * Uniquely, stolen RAM is contiguous in physical, and as the GTT
	 * mapping is also contiguous in "virtual", we can use the GTT
	 * hostport to work around the PCH block.
	 */
	ui64BaseAddr -= psGtt->sStolenBase.uiAddr;
	ui64BaseAddr += psDeviceData->psLINFBInfo->fix.smem_start;
	for(i = 0; i < ui32NumPages; i++)
		pasDevPAddr[i].uiAddr = ui64BaseAddr + ((IMG_UINT64)i * PAGE_SIZE);

	/* Stolen mem is special and is contiguous in physical and virtual
	 * address space. We can use the Linux framebuffer mapping here.
	 */
	*ppvLinAddr = psDeviceData->psLINFBInfo->screen_base + 
				  (psBuffer->ui32GttPTEOffset << PAGE_SHIFT);

err_out:
	return eError;
}

static IMG_VOID DC_INTEL_BufferRelease(IMG_HANDLE hBuffer)
{
	DC_INTEL_BUFFER *psBuffer = hBuffer;
	DC_INTEL_DEVICE *psDeviceData = psBuffer->psDeviceContext->psDeviceData;
	DC_INTEL_GTT_CONTEXT *psGtt = &psDeviceData->sGtt;

	GttUnmapDevicePagePhyAddrs(psGtt, &psGtt->sStolenZone,
							   psBuffer->ui32GttPTEOffset,
							   psBuffer->ui32ByteSize >> PAGE_SHIFT);
	psBuffer->ui32GttPTEOffset = 0;
}

static PVRSRV_ERROR DC_INTEL_BufferMap(IMG_HANDLE hBuffer)
{
	DC_INTEL_BUFFER *psBuffer = hBuffer;
	DC_INTEL_DEVICE *psDeviceData = psBuffer->psDeviceContext->psDeviceData;
	DC_INTEL_GTT_CONTEXT *psGtt = &psDeviceData->sGtt;

	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 ui32NumPages;

	if(psBuffer->hImport)
	{
		eError = DCImportBufferAcquire(psBuffer->hImport, PAGE_SHIFT,
									   &ui32NumPages,
									   &psBuffer->asPhyAddrs);

		/* DCImportBufferAcquire gives us pages, not bytes */
		psBuffer->ui32ByteSize = ui32NumPages << PAGE_SHIFT;

		psBuffer->ui32GttPTEOffset =
			GttMapDevicePagePhyAddrs(psGtt, &psGtt->sDynamicZone,
									 psBuffer->asPhyAddrs, ui32NumPages);

		if(!psBuffer->ui32GttPTEOffset)
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	return eError;
}

static IMG_VOID DC_INTEL_BufferUnmap(IMG_HANDLE hBuffer)
{
	DC_INTEL_BUFFER *psBuffer = hBuffer;
	DC_INTEL_DEVICE *psDeviceData = psBuffer->psDeviceContext->psDeviceData;
	DC_INTEL_GTT_CONTEXT *psGtt = &psDeviceData->sGtt;

	if(psBuffer->hImport)
	{
		GttUnmapDevicePagePhyAddrs(psGtt, &psGtt->sDynamicZone,
								   psBuffer->ui32GttPTEOffset,
								   psBuffer->ui32ByteSize >> PAGE_SHIFT);
		psBuffer->ui32GttPTEOffset = 0;

		DCImportBufferRelease(psBuffer->hImport, psBuffer->asPhyAddrs);
		psBuffer->ui32ByteSize = 0;
		psBuffer->asPhyAddrs = 0;
	}
}

static IMG_VOID DC_INTEL_BufferFree(IMG_HANDLE hBuffer)
{
	DC_INTEL_BUFFER *psBuffer = hBuffer;
	BUG_ON(psBuffer->ui32GttPTEOffset != 0);
	kfree(psBuffer);
}

static int __init DC_INTEL_Init(void)
{
	IMG_UINT32 ui32NumLanes = 0, ui32StolenSize, ui32PageTableSize;
	DC_INTEL_FLIP_CONFIG sConfig = {};
	IMG_UINT32 ui32Width, ui32Height;
	struct fb_info *psLINFBInfo;
	DC_INTEL_GTT_CONTEXT sGtt;
	DC_INTEL_TYPE eDeviceType;
	struct pci_dev *psPciDev;
	IMG_UINT16 ui16RegVal;
	IMG_PIXFMT ePixFormat;
	DC_INTEL_DTD sDTD;
	void *pvRegisters;
	int err;

	static DC_DEVICE_FUNCTIONS sDCFunctions =
	{
		.pfnGetInfo					= DC_INTEL_GetInfo,
		.pfnPanelQueryCount			= DC_INTEL_PanelQueryCount,
		.pfnPanelQuery				= DC_INTEL_PanelQuery,
		.pfnFormatQuery				= DC_INTEL_FormatQuery,
		.pfnDimQuery				= DC_INTEL_DimQuery,
		.pfnSetBlank				= IMG_NULL,
		.pfnSetVSyncReporting		= IMG_NULL,
		.pfnLastVSyncQuery			= IMG_NULL,
		.pfnContextCreate			= DC_INTEL_ContextCreate,
		.pfnContextDestroy			= DC_INTEL_ContextDestroy,
		.pfnContextConfigure		= DC_INTEL_ContextConfigure,
		.pfnContextConfigureCheck	= DC_INTEL_ContextConfigureCheck,
		.pfnBufferAlloc				= DC_INTEL_BufferAlloc,
		.pfnBufferAcquire			= DC_INTEL_BufferAcquire,
		.pfnBufferRelease			= DC_INTEL_BufferRelease,
		.pfnBufferFree				= DC_INTEL_BufferFree,
		.pfnBufferImport			= DC_INTEL_BufferImport,
		.pfnBufferMap				= DC_INTEL_BufferMap,
		.pfnBufferUnmap				= DC_INTEL_BufferUnmap,
	};

	psPciDev = pci_get_bus_and_slot(DC_INTEL_BUS_ID,
									PCI_DEVFN(DC_INTEL_DEV_ID, DC_INTEL_FUNC));
	if(!psPciDev)
	{
		pr_err("No Intel display block found.");
		err = -ENODEV;
		goto err_out;
	}

	err = pci_enable_device(psPciDev);
	if(err)
	{
		pr_err("Failed to enable PCI device.");
		goto err_out;
	}

	eDeviceType = GetDevice(psPciDev->device);
	if(eDeviceType == DC_INTEL_TYPE_UNKNOWN)
	{
		pr_err("Failed to determine device type.");
		err = -ENODEV;
		goto err_pci_disable_device;
	}

	pvRegisters = pci_iomap(psPciDev, 0, 0);
	if(!pvRegisters)
	{
		pr_err("Failed to map display register bank to CPU.");
		err = -ENODEV;
		goto err_pci_disable_device;
	}

	if(!IsHDMIDDetectedAndEnabled(pvRegisters))
	{
		err = -ENODEV;
		goto err_pci_iounmap;
	}

	/* Figure out a mutually supportable modes */
	err = PopulateDTDModesFromEDID(pvRegisters);
	if(err)
		goto err_pci_iounmap;

	/* Parse the `mode' module parameter and pick a mode */
	DTDAndBppFromModeString(mode, &sDTD, &ePixFormat);

	/* GTT registers start 2MB into the primary register bank */
	sGtt.psPTEs = (IMG_SYS_PHYADDR *)(pvRegisters + 2 * 1024 * 1024);

	/* Get the base address of the stolen memory */
	err = pci_read_config_dword(psPciDev, 0x5C,
								(u32 *)&sGtt.sStolenBase.uiAddr);
	if(err)
	{
		pr_err("Failed to read GMCH_BSM PCI config register.");
		err = -ENODEV;
		goto err_pci_iounmap;
	}

	/* Sometimes we see the low bit set */
	sGtt.sStolenBase.uiAddr &= PAGE_MASK;

	/* Get the size of the GTT page tables and the stolen region */
	if(eDeviceType == DC_INTEL_TYPE_IRONLAKE)
	{
		err = pci_read_config_word(psPciDev, 0x52, &ui16RegVal);
		if(err)
		{
			pr_err("Failed to read GMCH_CTRL PCI config register.");
			err = -ENODEV;
			goto err_pci_iounmap;
		}

		GetStolenAndGttPageTableSizeILK(ui16RegVal,
										&ui32StolenSize, &ui32PageTableSize);

		if(ui32PageTableSize != 2 * 1024 * 1024)
		{
			pr_err("Saw something other than 2MB page table, aborting.");
			err = -ENODEV;
			goto err_pci_iounmap;
		}

		/* Make sure the HW page table setup is consistent */
		if(!CheckGttSetupILK(pvRegisters))
		{
			err = -ENODEV;
			goto err_pci_iounmap;
		}
	}
	else /* Sandy Bridge */
	{
		err = pci_read_config_word(psPciDev, 0x50, &ui16RegVal);
		if(err)
		{
			pr_err("Failed to read GMCH_CTRL PCI config register.");
			err = -ENODEV;
			goto err_pci_iounmap;
		}

		GetStolenAndGttPageTableSizeSNB(ui16RegVal,
										&ui32StolenSize, &ui32PageTableSize);
	}

	/* Read size of memory bar to determine GTT address space size */
	sGtt.ui32NumMappableGttEntries = pci_resource_len(psPciDev, 2) >> PAGE_SHIFT;

	/* Disable the VGA plane and make any VGA programming inoperable.
	 * On Ironlake, not doing this will break mode setting.
	 */
	DisableVGAPlane(psPciDev, pvRegisters);

	/* Planes/Pipes disabled and clocks off */
	if(!DisableDisplayEngine(pvRegisters, eDeviceType, IMG_FALSE))
	{
		pr_err("Failed to reset display block.");
		err = -ENODEV;
		goto err_pci_iounmap;
	}

	/* We can set the mode before setting up the GTT or frame buffer
	 * address because clocks aren't enabled yet. By the time the screen
	 * has clocked the new mode timings, everything will be ready to go.
	 */
	if(!SetModeFromDTD(pvRegisters, &sDTD, &ui32NumLanes))
	{
		pr_err("Failed to set mode from EDID DTD.");
		err = -ENODEV;
		goto err_pci_iounmap;
	}

	GetDTDWidthHeight(&sDTD, &ui32Width, &ui32Height);

	/* There is one zone for permanently allocated memory, used by primary
	 * swap chain buffers (DCBufferAlloc). This is referred to as the
	 * "stolen zone". Note that the Linux framebuffer allocation is not
	 * included in this zone (it is not zoned).
	 *
	 * There is an additional zone for dynamic mappings (DCBufferMap etc.).
	 * This zone is used for temporary mappings.
	 */
	sGtt.sStolenZone.ui32Offset =
		PAGE_ALIGN(ui32Width * ui32Height *
				   IMGPixFmtGetBPP(ePixFormat) / 8) >> PAGE_SHIFT;
	sGtt.sStolenZone.ui32Total =
		(ui32StolenSize >> PAGE_SHIFT) - sGtt.sStolenZone.ui32Offset;
	sGtt.sStolenZone.ui32WriteOffset = 0;

	sGtt.sDynamicZone.ui32Offset =
		sGtt.sStolenZone.ui32Offset + sGtt.sStolenZone.ui32Total;
	sGtt.sDynamicZone.ui32Total =
		(sGtt.ui32NumMappableGttEntries - sGtt.sDynamicZone.ui32Offset) / 2;
	sGtt.sDynamicZone.ui32WriteOffset = 0;

	GttInitPageTables(&sGtt);

	gpsDeviceData = kmalloc(GFP_KERNEL, sizeof(DC_INTEL_DEVICE));
	if(!gpsDeviceData)
	{
		pr_err("Failed to allocate device data.");
		err = -ENOMEM;
		goto err_pci_iounmap;
	}

	if(RegisterFramebuffer(psPciDev, &sDTD, ePixFormat,
						   &psLINFBInfo, ui32StolenSize))
		goto err_free_device_data;

	gpsDeviceData->psPciDev		= psPciDev;
	gpsDeviceData->pvRegisters	= pvRegisters;
	gpsDeviceData->eDeviceType	= eDeviceType;
	gpsDeviceData->sGtt			= sGtt;

	gpsDeviceData->psLINFBInfo	= psLINFBInfo;
	gpsDeviceData->ePixFormat	= ePixFormat;

	/* Create a workqueue the interrupt handler can use to schedule
	 * non-ISR-context flip processing to happen on. Though on Intel
	 * the flipping is just a few register writes, it's useful to
	 * demonstrate this use of workqueues, which are commonly required
	 * on other platforms.
	 */
	INIT_WORK(&gpsDeviceData->sFlipWorker, FlipWorker);
	gpsDeviceData->psFlipWQ = create_singlethread_workqueue(DRVNAME);
	if(!gpsDeviceData->psFlipWQ)
	{
		pr_err("Failed to create flip workqueue.");
		err = -ENODEV;
		goto err_unregister_framebuffer;
	}

	/* Register our interrupt handler */
	err = request_irq(psPciDev->irq, IRQHandler,
					  IRQF_SHARED, DRVNAME, gpsDeviceData);
	if(err)
	{
		pr_err("Failed to register interrupt handler (err=%d).", err);
		goto err_free_wq;
	}

	sConfig.sPrimary =
		GetPrimaryConfigFromFBVar(&psLINFBInfo->var, ePixFormat);

	if(!EnableDisplayEngine(pvRegisters, eDeviceType, ui32NumLanes, sConfig))
	{
		pr_err("Failed to enable display block.");
		err = -ENODEV;
		goto err_free_irq;
	}

	if(DCRegisterDevice(&sDCFunctions,
						MAX_COMMANDS_IN_FLIGHT,
						gpsDeviceData,
						&gpsDeviceData->hSrvHandle) != PVRSRV_OK)
	{
		err = -ENODEV;
		goto err_free_irq;
	}

err_out:
	return err;

err_free_irq:
	free_irq(psPciDev->irq, gpsDeviceData);
err_free_wq:
	destroy_workqueue(gpsDeviceData->psFlipWQ);
err_unregister_framebuffer:
	unregister_framebuffer(psLINFBInfo);
err_free_device_data:
	kfree(gpsDeviceData);
	gpsDeviceData = NULL;
err_pci_iounmap:
	pci_iounmap(psPciDev, pvRegisters);
err_pci_disable_device:
	pci_disable_device(psPciDev);
	goto err_out;
}

static void __exit DC_INTEL_Exit(void)
{
	DC_INTEL_DEVICE *psDeviceData = gpsDeviceData;

	DCUnregisterDevice(psDeviceData->hSrvHandle);
	DisableDisplayEngine(psDeviceData->pvRegisters,
						 psDeviceData->eDeviceType, IMG_TRUE);
	free_irq(psDeviceData->psPciDev->irq, psDeviceData);
	destroy_workqueue(gpsDeviceData->psFlipWQ);
	kfree(gpsConfigQueue);
	unregister_framebuffer(psDeviceData->psLINFBInfo);
	pci_iounmap(psDeviceData->psPciDev, psDeviceData->pvRegisters);
	pci_disable_device(psDeviceData->psPciDev);
	kfree(psDeviceData);
}

module_init(DC_INTEL_Init);
module_exit(DC_INTEL_Exit);

module_param(mode, charp, S_IRUGO);
MODULE_PARM_DESC(mode,
	"Initial video mode \"<xres>x<yres>[-<depth>][@<refresh>]\"");
