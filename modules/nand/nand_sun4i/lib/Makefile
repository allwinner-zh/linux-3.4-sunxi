
################################################################################
#
# Common Variables that already set:
#     LICHEE_KDIR
#     LICHEE_MOD_DIR
#     CROSS_COMPILE
#     ARCH
#
#################################################################################
PWD=$(shell pwd)

obj-m+=nand.o
nand-objs := /src/format/nand_format.o /src/logic/bad_manage.o \
         	/src/logic/logic_ctl.o /src/logic/mapping.o \
         	/src/logic/mbr.o /src/logic/mapping_base.o /src/logic/merge.o \
         	/src/logic/read_reclaim.o /src/logic/wear_levelling.o /src/logic/logic_cache.o \
         	/src/scan/nand_scan.o /src/scan/nand_id.o\
         	/src/physic/nand_phy.o /src/physic/nand_simple_r.o /src/physic/nand_simple_w.o \
         	/src/nfc/nfc_r.o /src/nfc/nfc_w.o

install: build
	arm-none-linux-gnueabi-ar crv libnand nand.o
	cp libnand ../
	rm *.ko
build:
	@echo $(LICHEE_KDIR)
	$(MAKE) -C $(LICHEE_KDIR) M=$(PWD)

clean:
	@rm -rf *.o *.ko .*.cmd *.mod.c *.order *.symvers .tmp_versions *~
