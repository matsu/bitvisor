TOPDIR := $(shell pwd)
CONFIG = $(TOPDIR)/.config
include	$(CONFIG)

# default configurations
CONFIG_64 ?= \
	$(shell gcc --target-help | grep -q x86_64: && echo -n 1 || echo -n 0)
CONFIG_DEBUG_GDB ?= 0
CONFIG_SPINLOCK_DEBUG ?= 0
CONFIG_TTY_SERIAL ?= 0
CONFIG_TTY_PRO1000 ?= 1
CONFIG_CPU_MMU_SPT_1 ?= 0
CONFIG_CPU_MMU_SPT_2 ?= 0
CONFIG_CPU_MMU_SPT_3 ?= 1
CONFIG_CPU_MMU_SPT_USE_PAE ?= 1
CONFIG_PS2KBD_F11PANIC ?= 0
CONFIG_PS2KBD_F12MSG ?= 1
CONFIG_DBGSH ?= 1
CONFIG_STATUS ?= 0
CONFIG_LOG_TO_GUEST ?= 0
CONFIG_ATA_DRIVER ?= 1
CONFIG_STORAGE_ENC ?= 1
CONFIG_CRYPTO_VPN ?= 1
CONFIG_USB_DRIVER ?= 1
CONFIG_SHADOW_UHCI ?= 1
CONFIG_SHADOW_EHCI ?= 1
CONFIG_HANDLE_USBMSC ?= 1
CONFIG_HANDLE_USBHUB ?= 1
CONFIG_CONCEAL_USBCCID ?= 1
CONFIG_IEEE1394_CONCEALER ?= 1
CONFIG_FWDBG ?= 0
CONFIG_ACPI_DSDT ?= 1
CONFIG_DISABLE_SLEEP ?= 1
CONFIG_ENABLE_ASSERT ?= 1
CONFIG_DEBUG_ATA ?= 0
CONFIG_SELECT_AES_GLADMAN ?= 0
CONFIG_CARDSTATUS ?= 1
CONFIG_IDMAN ?= 1
CONFIG_VPN_PRO100 ?= 1
CONFIG_VPN_PRO1000 ?= 1
CONFIG_VPN_RTL8169 ?= 0
CONFIG_VPN_VE ?= 1
CONFIG_VTD_TRANS ?= 0
CONFIG_STORAGE_PD ?= 0
CONFIG_IDMAN_PD ?= 0
CONFIG_VPN_PD ?= 0
CONFIG_DISABLE_TCG_BIOS ?= 1
CONFIG_ACPI_TIME_SOURCE ?= 1

# config list
CONFIGLIST :=
CONFIGLIST += CONFIG_64=$(CONFIG_64)[64bit VMM]
CONFIGLIST += CONFIG_DEBUG_GDB=$(CONFIG_DEBUG_GDB)[gdb remote debug support (32bit only)]
#CONFIGLIST += CONFIG_SPINLOCK_DEBUG=$(CONFIG_SPINLOCK_DEBUG)[spinlock debug (unstable)]
CONFIGLIST += CONFIG_TTY_SERIAL=$(CONFIG_TTY_SERIAL)[VMM uses a serial port (COM1) for output]
CONFIGLIST += CONFIG_TTY_PRO1000=$(CONFIG_TTY_PRO1000)[VMM output to LAN (VPN_PRO1000 must be 1)]
CONFIGLIST += CONFIG_CPU_MMU_SPT_1=$(CONFIG_CPU_MMU_SPT_1)[Shadow type 1 (very slow and stable)]
CONFIGLIST += CONFIG_CPU_MMU_SPT_2=$(CONFIG_CPU_MMU_SPT_2)[Shadow type 2 (faster and unstable)]
CONFIGLIST += CONFIG_CPU_MMU_SPT_3=$(CONFIG_CPU_MMU_SPT_3)[Shadow type 3 (faster and unstable)]
CONFIGLIST += CONFIG_CPU_MMU_SPT_USE_PAE=$(CONFIG_CPU_MMU_SPT_USE_PAE)[Shadow page table uses PAE]
CONFIGLIST += CONFIG_PS2KBD_F11PANIC=$(CONFIG_PS2KBD_F11PANIC)[Panic when F11 is pressed (PS/2 only)]
CONFIGLIST += CONFIG_PS2KBD_F12MSG=$(CONFIG_PS2KBD_F12MSG)[Print when F12 is pressed (PS/2 only)]
CONFIGLIST += CONFIG_DBGSH=$(CONFIG_DBGSH)[Debug shell access from guest]
CONFIGLIST += CONFIG_STATUS=$(CONFIG_STATUS)[Provide VMM status for guest]
CONFIGLIST += CONFIG_LOG_TO_GUEST=$(CONFIG_LOG_TO_GUEST)[Log to guest memory]
CONFIGLIST += CONFIG_ATA_DRIVER=$(CONFIG_ATA_DRIVER)[Enable ATA driver]
CONFIGLIST += CONFIG_STORAGE_ENC=$(CONFIG_STORAGE_ENC)[Enable storage encryption (DEBUG)]
CONFIGLIST += CONFIG_CRYPTO_VPN=$(CONFIG_CRYPTO_VPN)[Enable IPsec VPN Client]
CONFIGLIST += CONFIG_USB_DRIVER=$(CONFIG_USB_DRIVER)[Enable USB driver]
CONFIGLIST += CONFIG_SHADOW_UHCI=$(CONFIG_SHADOW_UHCI)[Shadow UHCI(USB1) transfers]
CONFIGLIST += CONFIG_SHADOW_EHCI=$(CONFIG_SHADOW_EHCI)[Shadow EHCI(USB2) transfers]
CONFIGLIST += CONFIG_HANDLE_USBMSC=$(CONFIG_HANDLE_USBMSC)[Handle USB mass storage class devices]
CONFIGLIST += CONFIG_HANDLE_USBHUB=$(CONFIG_HANDLE_USBHUB)[Handle USB hub class devices]
CONFIGLIST += CONFIG_CONCEAL_USBCCID=$(CONFIG_CONCEAL_USBCCID)[Conceal USB ccid class device]
CONFIGLIST += CONFIG_PS2KBD_F10USB=$(CONFIG_PS2KBD_F10USB)[Run a test for USB ICCD when F10 pressed]
CONFIGLIST += CONFIG_PS2KBD_F12USB=$(CONFIG_PS2KBD_F12USB)[Dump EHCI async. list when F12 pressed]
CONFIGLIST += CONFIG_IEEE1394_CONCEALER=$(CONFIG_IEEE1394_CONCEALER)[Conceal OHCI IEEE 1394 host controllers]
CONFIGLIST += CONFIG_FWDBG=$(CONFIG_FWDBG)[Debug via IEEE 1394]
CONFIGLIST += CONFIG_ACPI_DSDT=$(CONFIG_ACPI_DSDT)[Parse ACPI DSDT]
CONFIGLIST += CONFIG_DISABLE_SLEEP=$(CONFIG_DISABLE_SLEEP)[Disable ACPI S2 and S3]
CONFIGLIST += CONFIG_ENABLE_ASSERT=$(CONFIG_ENABLE_ASSERT)[Enable checking assertion failure]
CONFIGLIST += CONFIG_DEBUG_ATA=$(CONFIG_DEBUG_ATA)[Enable debugging ATA driver]
CONFIGLIST += CONFIG_SELECT_AES_GLADMAN=$(CONFIG_SELECT_AES_GLADMAN)[Select Dr. Gladmans AES assembler code]
CONFIGLIST += CONFIG_CARDSTATUS=$(CONFIG_CARDSTATUS)[Panic if an IC card is ejected (IDMAN)]
CONFIGLIST += CONFIG_IDMAN=$(CONFIG_IDMAN)[IDMAN (CRYPTO_VPN must be enabled)]
CONFIGLIST += CONFIG_VPN_PRO100=$(CONFIG_VPN_PRO100)[Enable VPN for Intel PRO/100]
CONFIGLIST += CONFIG_VPN_PRO1000=$(CONFIG_VPN_PRO1000)[Intel PRO/1000 driver]
CONFIGLIST += CONFIG_VPN_RTL8169=$(CONFIG_VPN_RTL8169)[Realtek RTL8169 driver]
CONFIGLIST += CONFIG_VPN_VE=$(CONFIG_VPN_VE)[Enable ve (Virtual Ethernet) driver]
CONFIGLIST += CONFIG_VTD_TRANS=$(CONFIG_VTD_TRANS)[Enable VT-d translation]
CONFIGLIST += CONFIG_STORAGE_PD=$(CONFIG_STORAGE_PD)[Storage encrypting in protection domain]
CONFIGLIST += CONFIG_IDMAN_PD=$(CONFIG_IDMAN_PD)[IDMan in protection domain]
CONFIGLIST += CONFIG_VPN_PD=$(CONFIG_VPN_PD)[VPN in protection domain]
CONFIGLIST += CONFIG_DISABLE_TCG_BIOS=$(CONFIG_DISABLE_TCG_BIOS)[Disable TCG BIOS (TPM related)]
CONFIGLIST += CONFIG_ACPI_TIME_SOURCE=$(CONFIG_ACPI_TIME_SOURCE)[Use ACPI PM Timer as time source]

NAME	= bitvisor
FORMAT	= elf32-i386
TARGET	= $(NAME).elf
MAP	= $(NAME).map
LDS	= $(NAME).lds
OBJS-1	= core/core.o drivers/drivers.o storage/storage.o
OBJS-$(CONFIG_CRYPTO_VPN) += vpn/vpn.o
OBJS-$(CONFIG_IDMAN) += idman/idman.o
OBJS-1 += crypto/crypto.a
OBJS	= $(OBJS-1)
CORE-1  = defconfig crypto/crypto.a storage/storage.o
CORE-$(CONFIG_IDMAN) += idman/idman.o
CORE-$(CONFIG_CRYPTO_VPN) += vpn/vpn.o

BITS-0	= 32
BITS-1	= 64

MAKE	= make
CC	= gcc
LD	= ld
INCLUDE = $(TOPDIR)/include
HEADERS = $(shell find $(INCLUDE) -name '*.h')
CFLAGS	= -m$(BITS-$(CONFIG_64)) -O2 -g -Wall \
	  -mno-red-zone -ffreestanding -nostdinc -fno-stack-protector \
	  -mno-sse -I$(INCLUDE)
LDFLG-0	= -melf_i386
LDFLG-1	= -melf_x86_64
LDFLAGS	= $(LDFLG-$(CONFIG_64)) -g -nostdlib # --build-id=none
LDFLAGS2 = -T $(LDS) --cref -Map $(MAP)

export CONFIG MAKE CC LD CFLAGS LDFLAGS HEADERS


.PHONY : all clean config $(OBJS) doxygen sloc

all : $(TARGET) 

clean :
	rm -f $(TARGET) $(MAP)
	$(MAKE) -C core clean
	$(MAKE) -C drivers clean
	$(MAKE) -C crypto clean
	$(MAKE) -C vpn clean
	$(MAKE) -C idman clean
	rm -rf documents/*

config :
	./config.sh

$(CONFIG) : Makefile
	echo -n '$(CONFIGLIST)' | tr '[' '#' | tr ']' '\n' | \
		sed 's/^ //' > $(CONFIG)

$(TARGET) $(MAP) : $(LDS) $(OBJS) $(CONFIG)
	$(LD) $(LDFLAGS) $(LDFLAGS2) -o $(TARGET) $(OBJS)
	objcopy --output-format $(FORMAT) $(TARGET)

core/core.o : $(CORE-1)
	$(MAKE) -C core

drivers/drivers.o :
	$(MAKE) -C drivers

crypto/crypto.a :
	$(MAKE) -C crypto

vpn/vpn.o :
	$(MAKE) -C vpn

idman/idman.o : 
	$(MAKE) -C idman
 
storage/storage.o :
	$(MAKE) -C storage

defconfig :
	cp defconfig.tmpl defconfig

# generate doxygen documents
doxygen :
	doxygen

# count LOC (Lines Of Code)
# get SLOCCount from http://www.dwheeler.com/sloccount
sloc :
	sloccount .


