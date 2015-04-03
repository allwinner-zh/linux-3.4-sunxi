
################################################################################
#
# Common Variables that already set:
#     LICHEE_KDIR
#     LICHEE_MOD_DIR
#     CROSS_COMPILE
#     ARCH
#
#################################################################################
CC=arm-linux-gnueabi-gcc
AR=arm-linux-gnueabi-ar
LIBPATH=$(shell pwd)
CFLAGS = -g -c -nostdlib -march=armv7-a -marm -fno-unwind-tables -fno-asynchronous-unwind-tables -mlittle-endian -O2 --min_array_alignment=4 --no_unaligned_access
#CFLAGS = -g -c -nostdlib -march=armv7-a -marm -fno-unwind-tables -fno-asynchronous-unwind-tables -mlittle-endian -O2 --min_array_alignment=4 --no_unaligned_access -fPIC
LIB=$(LIBPATH)/libnand.a
OBJ=src/format/nand_format.o src/logic/bad_manage.o \
         	src/logic/logic_ctl.o src/logic/mapping.o \
			src/logic/mbr.o src/logic/mapping_base.o src/logic/merge.o \
         	src/logic/read_reclaim.o src/logic/wear_levelling.o src/logic/logic_cache.o \
         	src/scan/nand_scan.o src/scan/nand_id.o\
         	src/physic/nand_phy.o src/physic/nand_simple_r.o src/physic/nand_simple_w.o \
         	src/nfc/nfc_r.o src/nfc/nfc_w.o

install:lib
	cp libnand.a ../libnand

lib : $(LIB)
$(OBJ) : $(LIBPATH)/../include/nand_include.h $(LIBPATH)/osal/nand_osal.h

$(LIB) : $(OBJ)
	$(AR) -rcs $@ $^
clean:
	-rm $(OBJ) $(LIB)
