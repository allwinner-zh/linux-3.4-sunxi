CC := arm-none-linux-gnueabi-gcc
LINUX_DIR = ~/workspace/exdroid3.4/lichee/linux-3.4/include
#ASM_DIR = ~/workspace/exdroid_3_8/lichee/linux-3.8/arch/arm/include/asm
CFLAGS := -I $(LINUX_DIR)
TARGET := app_basic 

.PHONY: all clean

all: $(TARGET) 

app_basic:app_basic.c 
	$(CC) $(CFLAGS) -static  $^  -o  $@

clean:
	rm -rf $(TARGET)  
