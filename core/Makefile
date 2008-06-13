CONFIG        = ../.config
include $(CONFIG)

# config macros
CONSTANTS-$(CONFIG_64) +=
CONSTANTS-$(CONFIG_DEBUG_GDB) += -DDEBUG_GDB
CONSTANTS-$(CONFIG_SPINLOCK_DEBUG) += -DSPINLOCK_DEBUG
CONSTANTS-$(CONFIG_TTY_SERIAL) += -DTTY_SERIAL
CONSTANTS-$(CONFIG_CPU_MMU_SPT_1) += -DCPU_MMU_SPT_1
CONSTANTS-$(CONFIG_CPU_MMU_SPT_2) += -DCPU_MMU_SPT_2
CONSTANTS-$(CONFIG_CPU_MMU_SPT_3) += -DCPU_MMU_SPT_3
CONSTANTS-$(CONFIG_CPU_MMU_SPT_USE_PAE) += -DCPU_MMU_SPT_USE_PAE
CONSTANTS-$(CONFIG_PS2KBD_F11PANIC) += -DF11PANIC
CONSTANTS-$(CONFIG_PS2KBD_F12MSG) += -DF12MSG
CONSTANTS-$(CONFIG_AUTO_REBOOT) += -DAUTO_REBOOT
CONSTANTS-$(CONFIG_DBGSH) += -DDBGSH
CONSTANTS-$(CONFIG_FWDBG) += -DFWDBG

CONSTANTS-1 += -DUSE_PAE

BITS-0        = 32
BITS-1        = 64
OBJS          = $(patsubst %.c,%.o,$(wildcard *.c)) \
		$(patsubst %.s,%.o,$(wildcard *.s))
HEADERS       = $(wildcard *.h) $(wildcard ../include/core/*.h)
OBJ_BUILTIN   = ./builtin/process_builtin.o
CFLAGS        = -m$(BITS-$(CONFIG_64)) -mno-red-zone -g -O -nostdinc \
		-fno-builtin -Wall $(CONSTANTS-1) -I../include/ \
		-fno-stack-protector
ASFLAGS       = --$(BITS-$(CONFIG_64))
LDFLAGS-0     = -Wl,-melf_i386
LDFLAGS-1     = -Wl,-melf_x86_64
LDFLAGS       = $(LDFLAGS-$(CONFIG_64)) -g -nostdlib -Wl,-r
OUT_OBJ       = core.o

.PHONY : all clean $(OBJ_BUILTIN)

all : $(OUT_OBJ)

clean : 
	$(MAKE) -C ./builtin clean
	rm -f $(OUT_OBJ) *.o *~

$(OBJ_BUILTIN) :
	$(MAKE) -C ./builtin

$(OUT_OBJ) : $(OBJS) $(OBJ_BUILTIN) $(CONFIG)
	$(CC) $(LDFLAGS) -o $(OUT_OBJ) $(OBJS) $(OBJ_BUILTIN)

$(OBJS) : $(HEADERS) $(CONFIG)
