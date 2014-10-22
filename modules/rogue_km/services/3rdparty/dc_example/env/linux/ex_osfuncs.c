/*************************************************************************/ /*!
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

#if defined(LINUX)

#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
/* For MODULE_LICENSE */
#include "pvrmodule.h"

#if defined(SUPPORT_DRM)
#include <drm/drmP.h>
#include "pvr_drm.h"
#endif

#include "dc_osfuncs.h"
#include "dc_example.h"

MODULE_SUPPORTED_DEVICE(DEVNAME);

#ifndef DC_EXAMPLE_WIDTH
#define DC_EXAMPLE_WIDTH 640
#endif

#ifndef DC_EXAMPLE_HEIGHT
#define DC_EXAMPLE_HEIGHT 480
#endif

#ifndef DC_EXAMPLE_BIT_DEPTH
#define DC_EXAMPLE_BIT_DEPTH 32
#endif

#ifndef DC_EXAMPLE_FBC_FORMAT
#define DC_EXAMPLE_FBC_FORMAT 0
#endif

#ifndef DC_EXAMPLE_REFRESH_RATE
#define DC_EXAMPLE_REFRESH_RATE 60
#endif

#ifndef DC_EXAMPLE_DPI
#define DC_EXAMPLE_DPI 160
#endif


DC_EXAMPLE_MODULE_PARAMETERS sModuleParams =
{
	.ui32Width = DC_EXAMPLE_WIDTH,
	.ui32Height = DC_EXAMPLE_HEIGHT,
	.ui32Depth = DC_EXAMPLE_BIT_DEPTH,
	.ui32FBCFormat = DC_EXAMPLE_FBC_FORMAT,
	.ui32RefreshRate = DC_EXAMPLE_REFRESH_RATE,
	.ui32XDpi = DC_EXAMPLE_DPI,
	.ui32YDpi = DC_EXAMPLE_DPI
};

module_param_named(width, 	sModuleParams.ui32Width, uint, S_IRUGO);
module_param_named(height, 	sModuleParams.ui32Height, uint, S_IRUGO);
module_param_named(depth, 	sModuleParams.ui32Depth, uint, S_IRUGO);
module_param_named(fbcformat, 	sModuleParams.ui32FBCFormat, uint, S_IRUGO);
module_param_named(refreshrate, sModuleParams.ui32RefreshRate, uint, S_IRUGO);
module_param_named(xdpi,	sModuleParams.ui32XDpi, uint, S_IRUGO);
module_param_named(ydpi,	sModuleParams.ui32YDpi, uint, S_IRUGO);

const DC_EXAMPLE_MODULE_PARAMETERS *DCExampleGetModuleParameters(IMG_VOID)
{
	return &sModuleParams;
}

IMG_CPU_VIRTADDR DCExampleVirtualAllocUncached(IMG_SIZE_T uiSize)
{
	return __vmalloc(uiSize,
			 GFP_KERNEL | __GFP_HIGHMEM,
			 pgprot_noncached(PAGE_KERNEL));
}

IMG_BOOL DCExampleVirtualFree(IMG_PVOID lpAddress, IMG_SIZE_T uiSize)
{
	PVR_UNREFERENCED_PARAMETER(uiSize);
	vfree(lpAddress);
	
	/* vfree does not return a value, so all we can do is hard code IMG_TRUE */
	return IMG_TRUE;
}

#define	VMALLOC_TO_PAGE_PHYS(vAddr) page_to_phys(vmalloc_to_page(vAddr))

PVRSRV_ERROR DCExampleLinAddrToDevPAddrs(IMG_CPU_VIRTADDR pvLinAddr,
					 IMG_DEV_PHYADDR *pasDevPAddr,
					 IMG_SIZE_T uiSize)
{
	unsigned long ulPages = DC_OS_BYTES_TO_PAGES(uiSize);
	int i;

	for (i = 0; i < ulPages; i++)
	{
		pasDevPAddr[i].uiAddr = VMALLOC_TO_PAGE_PHYS(pvLinAddr);
		pvLinAddr += PAGE_SIZE;
	}

	return PVRSRV_OK;
}

#if defined(SUPPORT_DRM)
int PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Init)(struct drm_device unref__ *dev)
#else
static int __init dc_example_init(void)
#endif
{
	if (DCExampleInit() != PVRSRV_OK)
	{
		return -ENODEV;
	}

	return 0;
}

/*****************************************************************************
 Function Name: DC_NOHW_Cleanup
 Description  : Remove the driver from the kernel.

                __exit places the function in a special memory section that
                the kernel frees once the function has been run.  Refer also
                to module_exit() macro call below.
*****************************************************************************/
#if defined(SUPPORT_DRM)
void PVR_DRM_MAKENAME(DISPLAY_CONTROLLER, _Cleanup)(struct drm_device unref__ *dev)
#else
static void __exit dc_example_deinit(void)
#endif
{
	DCExampleDeinit();
}

#if !defined(SUPPORT_DRM)
module_init(dc_example_init);
module_exit(dc_example_deinit);
#endif

#endif /* defined(LINUX) */

