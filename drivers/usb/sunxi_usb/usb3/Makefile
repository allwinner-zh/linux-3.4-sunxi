ccflags-$(CONFIG_SUNXI_USB300_DEBUG)	:= -DDEBUG
ccflags-$(CONFIG_SUNXI_USB300_VERBOSE)	+= -DVERBOSE_DEBUG

obj-$(CONFIG_SUNXI_USB300)			+= sunxi.o

sunxi-y					:= core.o
sunxi-y					+= gadget.o
sunxi-y					+= ep0.o
sunxi-y					+= host.o
sunxi-y					+= libusb300.o
sunxi-y					+= usb300_table.o

ifneq ($(CONFIG_DEBUG_FS),)
	sunxi-y				+= debugfs.o
endif
