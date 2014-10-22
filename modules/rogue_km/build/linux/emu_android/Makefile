########################################################################### ###
#@Copyright     Copyright (c) Imagination Technologies Ltd. All Rights Reserved
#@License       Dual MIT/GPLv2
# 
# The contents of this file are subject to the MIT license as set out below.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# Alternatively, the contents of this file may be used under the terms of
# the GNU General Public License Version 2 ("GPL") in which case the provisions
# of GPL are applicable instead of those above.
# 
# If you wish to allow use of your version of this file only under the terms of
# GPL, and not to allow others to use your version of this file under the terms
# of the MIT license, indicate your decision by deleting the provisions above
# and replace them with the notice and other provisions required by GPL as set
# out in the file called "GPL-COPYING" included in this distribution. If you do
# not delete the provisions above, a recipient may use your version of this file
# under the terms of either the MIT license or GPL.
# 
# This License is also included in this distribution in the file called
# "MIT-COPYING".
# 
# EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
# PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
# BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
# PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
### ###########################################################################

include ../config/preconfig.mk

# If a TARGET_PRODUCT is specified but not a TARGET_DEVICE, try to
# derive the TARGET_DEVICE from TARGET_PRODUCT.
#
ifeq ($(TARGET_DEVICE),)
override TARGET_DEVICE := \
 $(patsubst mini_%,%,$(patsubst full_%,%,$(TARGET_PRODUCT)))
endif

ifeq ($(TARGET_DEVICE),)
override TARGET_DEVICE := pc
endif

LDM_PCI := 1

HAL_VARIANT := pc

PVR_SYSTEM := emu

include ../common/android/paths.mk
include ../common/android/x86.mk
include ../common/android/features.mk

DISPLAY_CONTROLLER := dc_example
KERNEL_COMPONENTS += dc_example

# We can't use Android's x86 GCC because it doesn't support
# building kernel images.
#
KERNEL_CROSS_COMPILE := undef

CACHEFLUSH_TYPE ?= CACHEFLUSH_X86

LMA ?= 0

include ../config/core.mk
include ../common/android/extra_config.mk

$(eval $(call BothConfigC,EMULATOR,1))
$(eval $(call TunableBothConfigC,LMA,))
$(eval $(call TunableKernelConfigMake,LMA,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_WIDTH,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_HEIGHT,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_DPI,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_BIT_DEPTH,))
$(eval $(call TunableKernelConfigC,DC_EXAMPLE_FBC_MODE,))
