#
# Makefile for the linux kernel.
#

obj-y=arisc.o arisc_dram.o arisc-notifier.o arisc-sysfs.o
ifdef CONFIG_ARCH_SUN8IW6P1
obj-y += arisc_bsp_sun8iw6p1_table.o
endif

obj-y += hwmsgbox/
obj-y += hwspinlock/
obj-y += interfaces/
obj-y += message_manager/
