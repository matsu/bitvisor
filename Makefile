CONFIG	= .config
include	$(CONFIG)

# default configurations
CONFIG_64 ?= \
	$(shell gcc --target-help | grep -q x86_64: && echo -n 1 || echo -n 0)
CONFIG_DEBUG_GDB ?= 0
CONFIG_SPINLOCK_DEBUG ?= 0
CONFIG_TTY_SERIAL ?= 0
CONFIG_CPU_MMU_SPT_1 ?= 0
CONFIG_CPU_MMU_SPT_2 ?= 0
CONFIG_CPU_MMU_SPT_3 ?= 1
CONFIG_CPU_MMU_SPT_USE_PAE ?= 1
CONFIG_PS2KBD_F11PANIC ?= 0
CONFIG_PS2KBD_F12MSG ?= 1
CONFIG_AUTO_REBOOT ?= 1
CONFIG_DBGSH ?= 0
CONFIG_ATA_DRIVER ?= 0
CONFIG_STORAGE_ENC ?= 0
CONFIG_IEEE1394_CONCEALER ?= 1
CONFIG_FWDBG ?= 0

# config list
CONFIGLIST :=
CONFIGLIST += CONFIG_64=$(CONFIG_64)[64bit VMM]
CONFIGLIST += CONFIG_DEBUG_GDB=$(CONFIG_DEBUG_GDB)[gdb remote debug support (32bit only)]
#CONFIGLIST += CONFIG_SPINLOCK_DEBUG=$(CONFIG_SPINLOCK_DEBUG)[spinlock debug (unstable)]
CONFIGLIST += CONFIG_TTY_SERIAL=$(CONFIG_TTY_SERIAL)[VMM uses a serial port (COM1) for output]
CONFIGLIST += CONFIG_CPU_MMU_SPT_1=$(CONFIG_CPU_MMU_SPT_1)[Shadow type 1 (very slow and stable)]
CONFIGLIST += CONFIG_CPU_MMU_SPT_2=$(CONFIG_CPU_MMU_SPT_2)[Shadow type 2 (faster and unstable)]
CONFIGLIST += CONFIG_CPU_MMU_SPT_3=$(CONFIG_CPU_MMU_SPT_3)[Shadow type 3 (faster and unstable)]
CONFIGLIST += CONFIG_CPU_MMU_SPT_USE_PAE=$(CONFIG_CPU_MMU_SPT_USE_PAE)[Shadow page table uses PAE]
CONFIGLIST += CONFIG_PS2KBD_F11PANIC=$(CONFIG_PS2KBD_F11PANIC)[Panic when F11 is pressed (PS/2 only)]
CONFIGLIST += CONFIG_PS2KBD_F12MSG=$(CONFIG_PS2KBD_F12MSG)[Print when F12 is pressed (PS/2 only)]
CONFIGLIST += CONFIG_AUTO_REBOOT=$(CONFIG_AUTO_REBOOT)[Automatically reboot on guest reboot]
CONFIGLIST += CONFIG_DBGSH=$(CONFIG_DBGSH)[Debug shell access from guest]
CONFIGLIST += CONFIG_ATA_DRIVER=$(CONFIG_ATA_DRIVER)[Enable ATA driver]
CONFIGLIST += CONFIG_STORAGE_ENC=$(CONFIG_STORAGE_ENC)[Enable storage encryption (DEBUG)]
CONFIGLIST += CONFIG_IEEE1394_CONCEALER=$(CONFIG_IEEE1394_CONCEALER)[Conceal OHCI IEEE 1394 host controllers]
CONFIGLIST += CONFIG_FWDBG=$(CONFIG_FWDBG)[Debug via IEEE 1394]

NAME	= bitvisor
FORMAT	= elf32-i386
TARGET	= $(NAME).elf
MAP	= $(NAME).map
LDS	= $(NAME).lds
OBJS	= core/core.o drivers/drivers.o

BITS-0	= 32
BITS-1	= 64
LDFLG-0	= -Wl,-melf_i386
LDFLG-1	= -Wl,-melf_x86_64
LDFLAGS	= $(LDFLG-$(CONFIG_64)) -g -nostdlib -Wl,-T,$(LDS) \
	  -Wl,--cref -Wl,-Map,$(MAP)

.PHONY : all clean config $(OBJS) doxygen sloc

all : $(TARGET)

clean :
	rm -f $(TARGET) $(MAP)
	$(MAKE) -C core clean
	$(MAKE) -C drivers clean
	rm -rf documents/*

config :
	./config.sh

$(CONFIG) : Makefile
	echo -n '$(CONFIGLIST)' | tr '[' '#' | tr ']' '\n' | \
		sed 's/^ //' > $(CONFIG)

$(TARGET) $(MAP) : $(LDS) $(OBJS) $(CONFIG)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)
	objcopy --output-format $(FORMAT) $(TARGET)

core/core.o :
	$(MAKE) -C core

drivers/drivers.o :
	$(MAKE) -C drivers

# generate doxygen documents
doxygen :
	doxygen

# count LOC (Lines Of Code)
# get SLOCCount from http://www.dwheeler.com/sloccount
sloc :
	sloccount .
