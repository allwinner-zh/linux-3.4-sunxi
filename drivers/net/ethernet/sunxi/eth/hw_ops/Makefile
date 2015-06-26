#
# Makefile for libgeth
#
#

# Set the targets
targets := sunxi_libgeth $(libgeth-y)
always	:= sunxi_libgeth $(libgeth-y)

# Depend files
libgeth-y := sunxi_geth_reg.o

libgeth-y := $(addprefix $(obj)/,$(libgeth-y))

# Compile's flags
CFLAGS_REMOVE_sunxi_geth_reg.o = -funwind-tables -pg -g -gdwarf-2
EXTRA_CFLAGS	:= -finline-functions -fno-verbose-asm -Wa,--strip-local-absolute,-L
#EXTRA_AFLAGS  	:= -L
asflags-y  := --keep-loacls

quiet_cmd_libgeth = AR      $@
      cmd_libgeth = $(STRIP) -x --strip-debug --strip-unneeded $(filter-out FORCE,$^); \
		    $(AR) rcs$(KBUILD_ARFLAGS) $@ $(filter-out FORCE,$^)

quiet_cmd_copylib = IN      $@
      cmd_copylib = cp -fr $< $(dir $(obj)/../);rm -fr $<

$(obj)/sunxi_libgeth: $(libgeth-y) FORCE
	$(call cmd,libgeth)

libgeth_install: $(obj)/sunxi_libgeth
	$(call cmd,copylib)

clean-files = *.o sunxi_libgeth
