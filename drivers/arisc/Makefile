#
# Makefile for the linux kernel.
#

obj-y=arisc.o arisc_dram.o arisc-notifier.o arisc-sysfs.o
ifdef CONFIG_ARCH_SUN8IW1P1
obj-y += arisc_bsp_sun8iw1p1_table.o
else ifdef CONFIG_ARCH_SUN8IW3P1
obj-y += arisc_bsp_sun8iw3p1_table.o
else ifdef CONFIG_ARCH_SUN8IW5P1
obj-y += arisc_bsp_sun8iw5p1_table.o
else ifdef CONFIG_ARCH_SUN8IW6P1
obj-y += arisc_bsp_sun8iw6p1_table.o
else ifdef CONFIG_ARCH_SUN8IW7P1
obj-y += arisc_bsp_sun8iw7p1_table.o
else ifdef CONFIG_ARCH_SUN8IW9P1
obj-y += arisc_bsp_sun8iw9p1_table.o
else ifdef CONFIG_ARCH_SUN9IW1P1
obj-y += arisc_bsp_sun9iw1p1_table.o
endif

obj-y += hwmsgbox/
obj-y += hwspinlock/
obj-y += interfaces/
obj-y += message_manager/
