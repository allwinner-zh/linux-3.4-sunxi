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

ifeq ($(SUPPORT_ANDROID_PLATFORM),)

define if-component
ifneq ($$(filter $(1),$$(COMPONENTS)),)
M4DEFS += $(2)
endif
endef

define if-kernel-component
ifneq ($$(filter $(1),$$(KERNEL_COMPONENTS)),)
M4DEFS_K += $(2)
endif
endef

# common.m4 lives here
#
M4FLAGS := -I$(MAKE_TOP)/scripts

# These defs are required for both KM and UM install script.
M4DEFS_K := \
 -DPVRVERSION="$(PVRVERSION)" \
 -DBUILD=$(BUILD) \
 -DKERNEL_ID=$(KERNEL_ID) \
 -DPVR_BUILD_DIR=$(PVR_BUILD_DIR) \
 -DPVRSRV_MODNAME=$(PVRSRV_MODNAME)

ifeq ($(SUPPORT_DRM),1)
M4DEFS_K += -DSUPPORT_DRM=1
ifeq ($(SUPPORT_DRM_DC_MODULE),1)
M4DEFS_K += -DSUPPORT_DRM_DC_MODULE=1
endif
endif

ifeq ($(PDUMP),1)
M4DEFS_K += -DPDUMP=1
endif

ifneq ($(DISPLAY_CONTROLLER),)
$(eval $(call if-kernel-component,$(DISPLAY_CONTROLLER),\
 -DDISPLAY_CONTROLLER=$(DISPLAY_CONTROLLER)))
endif

# These defs are not derived from user variables
#
M4DEFS := \
 -DSOLIB_VERSION=$(PVRVERSION_MAJ).$(PVRVERSION_MIN).$(PVRVERSION_BUILD)

# XOrg support options are convoluted, so don't bother with if-component.  
ifneq ($(filter pvr_video,$(COMPONENTS)),) # This is an X build

M4DEFS += -DSUPPORT_XORG=1 
M4DEFS += -DXORG_EXPLICIT_PVR_SERVICES_LOAD=$(XORG_EXPLICIT_PVR_SERVICES_LOAD)

ifneq ($(XORG_WAYLAND),1)
M4DEFS += -DXORG_WAYLAND=1
endif

ifeq ($(LWS_NATIVE),1)
M4DEFS += -DPVR_XORG_DESTDIR=/usr/bin
M4DEFS += -DPVR_DDX_DESTDIR=/usr/lib/xorg/modules/drivers
M4DEFS += -DPVR_DRI_DESTDIR=$(SHLIB_DESTDIR)/dri
M4DEFS += -DPVR_CONF_DESTDIR=/etc/X11
$(eval $(call if-component,opengl_mesa,-DSUPPORT_MESA=1))

else

M4DEFS += -DLWS_INSTALL_TREE=1
M4DEFS += -DPVR_XORG_DESTDIR=$(LWS_PREFIX)/bin
M4DEFS += -DPVR_DDX_DESTDIR=$(LWS_PREFIX)/lib/xorg/modules/drivers
M4DEFS += -DPVR_DRI_DESTDIR=$(LWS_PREFIX)/lib/dri
M4DEFS += -DPVR_CONF_DESTDIR=$(LWS_PREFIX)/etc
$(eval $(call if-component,pvr_input, \
  -DSUPPORT_DDX_INPUT -DPVR_DDX_INPUT_DESTDIR=$(LWS_PREFIX)/lib/xorg/modules/input))
$(eval $(call if-component,opengl_mesa,-DSUPPORT_LIBGL=1 -DSUPPORT_MESA=1))

endif

else # This is a non-X build

ifneq ($(filter pvr_dri,$(COMPONENTS)),) # This is a Wayland build

M4DEFS += -DSUPPORT_WAYLAND=1

ifeq ($(LWS_NATIVE),1)
M4DEFS += -DPVR_DRI_DESTDIR=$(SHLIB_DESTDIR)/dri
else
M4DEFS += -DLWS_INSTALL_TREE=1
M4DEFS += -DPVR_DRI_DESTDIR=$(LWS_PREFIX)/lib/dri
endif

else # This is a non-X and Wayland build

$(eval $(call if-component,opengl,-DSUPPORT_LIBGL=1))

endif

endif

# Map other COMPONENTS on to SUPPORT_ defs
#
$(eval $(call if-component,opengles1,\
 -DSUPPORT_OPENGLES1=1 -DOGLES1_MODULE=$(opengles1_target) \
 -DSUPPORT_OPENGLES1_V1_ONLY=0))
$(eval $(call if-component,opengles3,\
 -DSUPPORT_OPENGLES3=1 -DOGLES3_MODULE=$(opengles3_target)))
$(eval $(call if-component,egl,\
 -DSUPPORT_LIBEGL=1 -DEGL_MODULE=$(egl_target)))
$(eval $(call if-component,glslcompiler,\
 -DSUPPORT_SOURCE_SHADER=1))
$(eval $(call if-component,opencl,\
 -DSUPPORT_OPENCL=1))
$(eval $(call if-component,liboclcompiler,\
 -DSUPPORT_OCL_COMPILER=1))
$(eval $(call if-component,openrl,\
 -DSUPPORT_OPENRL=1))
$(eval $(call if-component,rscompute,\
 -DSUPPORT_RSC=1))
$(eval $(call if-component,librscruntime,\
 -DSUPPORT_RSC_RUNTIME=1))
$(eval $(call if-component,librsccompiler,\
 -DSUPPORT_RSC_COMPILER=1))
$(eval $(call if-component,opengl opengl_mesa,\
 -DSUPPORT_OPENGL=1))
$(eval $(call if-component,null_ws,\
 -DSUPPORT_NULL_WS=1))
$(eval $(call if-component,null_drm_ws,\
 -DSUPPORT_NULL_DRM_WS=1))
$(eval $(call if-component,null_remote,\
 -DSUPPORT_NULL_REMOTE=1))
$(eval $(call if-component,ews_ws,\
 -DSUPPORT_EWS=1))
$(eval $(call if-component,ews_ws_remote,\
 -DSUPPORT_EWS=1))
$(eval $(call if-component,ews_wm,\
 -DSUPPORT_LUA=1))
$(eval $(call if-component,graphicshal,\
 -DSUPPORT_GRAPHICS_HAL=1))
$(eval $(call if-component,xmultiegltest,\
 -DSUPPORT_XUNITTESTS=1))
$(eval $(call if-component,pvrgdb,\
 -DPVRGDB=1))

# These defs are common to all driver builds, and inherited from config.mk
#
M4DEFS += \
 -DSHLIB_DESTDIR=$(SHLIB_DESTDIR) \
 -DDEMO_DESTDIR=$(DEMO_DESTDIR) \
 -DHEADER_DESTDIR=$(HEADER_DESTDIR) \
 -DEGL_DESTDIR=$(EGL_DESTDIR)

# These are common to some builds, and inherited from config.mk
#
ifeq ($(SUPPORT_ANDROID_PLATFORM),1)
M4DEFS_K += -DSUPPORT_ANDROID_PLATFORM=$(SUPPORT_ANDROID_PLATFORM)

M4DEFS += \
 -DGRALLOC_MODULE=gralloc.$(HAL_VARIANT).so \
 -DHWCOMPOSER_MODULE=hwcomposer.$(HAL_VARIANT).so \
 -DHAL_VARIANT=$(HAL_VARIANT)
endif

ifeq ($(PVR_REMOTE),1)
M4DEFS += -DPVR_REMOTE=1
endif

ifneq ($(filter pvr_dri,$(COMPONENTS)),)
M4DEFS += -DPVR_DRI_MODULE=1
endif

# M4 just builds KM/UM specific portions of the script.

$(TARGET_OUT)/install.sh: $(PVRVERSION_H) $(MAKE_TOP)/scripts/install.sh.tpl | $(TARGET_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	@sed 's/\[PVRVERSION\]/$(subst /,\/,$(PVRVERSION))/g;s/\[PVRBUILD\]/$(subst /,\/,$(BUILD))/g;s/\[KERNEL_ID\]/$(subst /,\/,$(KERNEL_ID))/g' $(MAKE_TOP)/scripts/install.sh.tpl > $@
	$(CHMOD) +x $@
install_script: $(TARGET_OUT)/install.sh
install_script_km: $(TARGET_OUT)/install.sh

ifneq ($(wildcard $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_km.sh.m4),)

# Need to install KM files
$(TARGET_OUT)/install_km.sh: $(PVRVERSION_H) $(CONFIG_KERNEL_MK) \
 $(MAKE_TOP)/scripts/common.m4 \
 $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_km.sh.m4 \
 | $(TARGET_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(M4) $(M4FLAGS) $(M4DEFS_K) \
	  $(MAKE_TOP)/scripts/common.m4 \
	  $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_km.sh.m4 > $@

install_script_km: $(TARGET_OUT)/install_km.sh

endif # ifneq ($(wildcard $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_km.sh.m4),)

ifneq ($(wildcard $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_um.sh.m4),)

# Need to install UM files
$(TARGET_OUT)/install_um.sh: $(PVRVERSION_H) $(CONFIG_MK) \
 $(MAKE_TOP)/scripts/common.m4 \
 $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_um.sh.m4 \
 | $(TARGET_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(M4) $(M4FLAGS) $(M4DEFS) $(M4DEFS_K) \
	  $(MAKE_TOP)/scripts/common.m4 \
	  $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_um.sh.m4 > $@

install_script: $(TARGET_OUT)/install_um.sh

endif # ifneq ($(wildcard $(MAKE_TOP)/$(PVR_BUILD_DIR)/install_um.sh.m4),)

$(TARGET_OUT)/rc.pvr: $(PVRVERSION_H) $(CONFIG_MK) $(CONFIG_KERNEL_MK) \
 $(MAKE_TOP)/scripts/rc.pvr.m4 $(MAKE_TOP)/scripts/common.m4 \
 $(MAKE_TOP)/$(PVR_BUILD_DIR)/rc.pvr.m4 \
 | $(TARGET_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(M4) $(M4FLAGS) $(M4DEFS) $(M4DEFS_K) $(MAKE_TOP)/scripts/rc.pvr.m4 \
		$(MAKE_TOP)/$(PVR_BUILD_DIR)/rc.pvr.m4 > $@
	$(CHMOD) +x $@

init_script: $(TARGET_OUT)/rc.pvr

$(TARGET_OUT)/udev.pvr: $(CONFIG_KERNEL_MK) \
 $(MAKE_TOP)/scripts/udev.pvr.m4 \
 | $(TARGET_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(M4) $(M4FLAGS) $(M4DEFS_K) $(MAKE_TOP)/scripts/udev.pvr.m4 > $@

udev_rules: $(TARGET_OUT)/udev.pvr

endif # ifeq ($(SUPPORT_ANDROID_PLATFORM),)

# This code mimics the way Make processes our implicit/explicit goal list.
# It tries to build up a list of components that were actually built, from
# whence an install script is generated.
#
ifneq ($(MAKECMDGOALS),)
BUILT_UM := $(MAKECMDGOALS)
ifneq ($(filter build components,$(MAKECMDGOALS)),)
BUILT_UM += $(COMPONENTS)
endif
BUILT_UM := $(sort $(filter $(ALL_MODULES) xorg wl,$(BUILT_UM)))
else
BUILT_UM := $(sort $(COMPONENTS))
endif

ifneq ($(MAKECMDGOALS),)
BUILT_KM := $(MAKECMDGOALS)
ifneq ($(filter build kbuild,$(MAKECMDGOALS)),)
BUILT_KM += $(KERNEL_COMPONENTS)
endif
BUILT_KM := $(sort $(filter $(ALL_MODULES),$(BUILT_KM)))
else
BUILT_KM := $(sort $(KERNEL_COMPONENTS))
endif

INSTALL_UM_MODULES := \
 $(strip $(foreach _m,$(BUILT_UM),\
  $(if $(filter doc host_% module_group,$($(_m)_type)),,\
   $(if $($(_m)_install_path),$(_m),\
    $(warning WARNING: UM $(_m)_install_path not defined)))))

# Build up a list of installable shared libraries. The shared_library module
# type is specially guaranteed to define $(_m)_target, even if the Linux.mk
# itself didn't. The list is formatted with <module>:<target> pairs e.g.
# "moduleA:libmoduleA.so moduleB:libcustom.so" for later processing.
ALL_SHARED_INSTALLABLE := \
 $(sort $(foreach _a,$(ALL_MODULES),\
  $(if $(filter shared_library,$($(_a)_type)),$(_a):$($(_a)_target),)))

# Handle implicit install dependencies. Executables and shared libraries may
# be linked against other shared libraries. Avoid requiring the user to
# specify the program's binary dependencies explicitly with $(m)_install_extra
INSTALL_UM_MODULES := \
 $(sort $(INSTALL_UM_MODULES) \
  $(foreach _a,$(ALL_SHARED_INSTALLABLE),\
   $(foreach _m,$(INSTALL_UM_MODULES),\
    $(foreach _l,$($(_m)_libs),\
     $(if $(filter lib$(_l).so,$(word 2,$(subst :, ,$(_a)))),\
                               $(word 1,$(subst :, ,$(_a))))))))

# Add explicit "extra" install dependencies
INSTALL_UM_MODULES := \
 $(foreach _m,$(INSTALL_UM_MODULES),$(_m) $($(_m)_install_extra))

INSTALL_UM_FRAGMENTS := \
 $(foreach _m,$(INSTALL_UM_MODULES),\
  $(call target-intermediates-of,$(_m),.install))

INSTALL_KM_FRAGMENTS := \
 $(strip $(foreach _m,$(BUILT_KM),\
  $(if $(filter-out kernel_module,$($(_m)_type)),,\
   $(if $($(_m)_install_path),\
    $(call target-intermediates-of,$(_m),.install),\
     $(warning WARNING: KM $(_m)_install_path not defined)))))

.PHONY: install_um_debug
install_um_debug: $(INSTALL_UM_FRAGMENTS)
	$(CAT) $^

.PHONY: install_km_debug
install_km_debug: $(INSTALL_KM_FRAGMENTS)
	$(CAT) $^

ifneq ($(SUPPORT_ANDROID_PLATFORM),)

# Build using even newer scheme which does not use M4 for anything
# (Only works for Android right now.)

$(TARGET_OUT)/install.sh: $(PVRVERSION_H) | $(TARGET_OUT)
$(TARGET_OUT)/install.sh: $(MAKE_TOP)/common/android/install.sh.tpl
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	@sed 's/\[PVRVERSION\]/$(subst /,\/,$(PVRVERSION))/g;s/\[PVRBUILD\]/$(subst /,\/,$(BUILD))/g' $< > $@
	$(CHMOD) +x $@

install_script_km: $(TARGET_OUT)/install.sh
install_script: $(TARGET_OUT)/install.sh

ifneq ($(INSTALL_KM_FRAGMENTS),)
$(TARGET_OUT)/install_km.sh: $(INSTALL_KM_FRAGMENTS) | $(TARGET_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(CAT) $(INSTALL_KM_FRAGMENTS) > $@
install_script_km: $(TARGET_OUT)/install_km.sh
endif

ifneq ($(INSTALL_UM_FRAGMENTS),)
$(TARGET_OUT)/install_um.sh: $(INSTALL_UM_FRAGMENTS) | $(TARGET_OUT)
	$(if $(V),,@echo "  GEN     " $(call relative-to-top,$@))
	$(CAT) $(INSTALL_UM_FRAGMENTS) > $@
install_script: $(TARGET_OUT)/install_um.sh
endif

endif # ifeq ($(SUPPORT_ANDROID_PLATFORM),)
