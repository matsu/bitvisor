PWD := $(shell pwd)
include $(CONFIG)

ifeq ($(CONFIG_64),1)
OBJS	= aes_amd64.o
CFLAGS	+= -DASM_AMD64_C -D_M_X64 -DUNIT_BITS=64
YFLAGS	= -f elf64 -D ASM_AMD64_C -D __GNUC__
else
OBJS	= aes_x86_v2.o
CFLAGS	+= -DASM_X86_V2C -D_M_IX86 -DUNIT_BITS=32
YFLAGS	= -f elf32 -D ASM_X86_V2C -D _aes_encrypt=aes_encrypt -D _aes_decrypt=aes_decrypt -D _ls_sub=ls_sub -D _im_sub=im_sub 
endif

TARGET	 = driver.o
OBJS	+= $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(wildcard *.h)
CFLAGS  += -I$(PWD)

$(TARGET): $(OBJS) $(CONFIG)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

$(OBJS): $(HEADERS) $(CONFIG)

.SUFFIXES: .asm .o

.asm.o:
	yasm $(YFLAGS) $<

aes.h aes_amd64.asm aes_x86_v2.asm aeskey.c aesopt.h aestab.c aestab.h brg_endian.h brg_types.h :
	@echo Download AES code from http://fp.gladman.plus.com/AES/ and extract it to drivers/crypto/aes-gladman/. >&2
	@echo Needed files with CONFIG_SELECT_AES_GLADMAN=1: aes.h aes_amd64.asm aes_x86_v2.asm aeskey.c aesopt.h aestab.c aestab.h brg_endian.h brg_types.h >&2
	@false
