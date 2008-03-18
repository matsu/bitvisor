CONFIG        = ../../.config
include $(CONFIG)

BITS-0        = 32
BITS-1        = 64
LIB_OBJS      = $(patsubst %.c,%.o,$(wildcard lib_*.c)) \
		$(patsubst %.s,%.o,$(wildcard lib_*.s))
BIN_OUT       = $(patsubst %.c,%,$(wildcard bin_*.c)) \
		$(patsubst %.s,%,$(wildcard bin_*.s))
BIN_NAME      = $(patsubst bin_%,%,$(patsubst %.c,%,$(wildcard bin_*.c))) \
		$(patsubst bin_%,%,$(patsubst %.s,%,$(wildcard bin_*.s)))
HEADERS       = $(wildcard *.h)
CFLAGS        = -m$(BITS-$(CONFIG_64)) -mno-red-zone -g -O -nostdinc \
		-fno-builtin -Wall -I. -nostdlib
ASFLAGS       = --$(BITS-$(CONFIG_64))
LDFLAGS-0     = -Wl,-melf_i386
LDFLAGS-1     = -Wl,-melf_x86_64
LDFLAGS       = $(LDFLAGS-$(CONFIG_64))
AS_1-0        = .long
AS_2-0        = , 0
AS_1-1        = .quad
AS_2-1        =
AS_1          = $(AS_1-$(CONFIG_64))
AS_2          = $(AS_2-$(CONFIG_64))
OUT_OBJ       = process_builtin.o
OUT_LIB       = lib.a
OUT_ASM       = process_builtin.s

.PHONY : all clean

all : $(OUT_OBJ)

clean :
	rm -f $(OUT_OBJ) $(OUT_LIB) $(OUT_ASM) $(BIN_OUT) *.o *~

$(OUT_OBJ) : $(OUT_ASM)
	as $(ASFLAGS) -o $(OUT_OBJ) $(OUT_ASM)

$(OUT_ASM) : $(BIN_OUT)
	echo '	.data' > $(OUT_ASM)
	echo '	.globl	process_builtin' >> $(OUT_ASM)
	echo '	.align	8' >> $(OUT_ASM)
	echo 'process_builtin:' >> $(OUT_ASM)
	for i in $(BIN_NAME); do \
		echo '	$(AS_1)	prc_'$$i'_name$(AS_2)'; \
		echo '	$(AS_1)	prc_'$$i'_bin$(AS_2)'; \
		echo '	.quad	prc_'$$i'_bin_end - prc_'$$i'_bin'; \
	done >> $(OUT_ASM)
	echo '	.quad	0, 0, 0' >> $(OUT_ASM)
	for i in $(BIN_NAME); do \
		echo 'prc_'$$i'_name:	.string	"'$$i'"'; \
		echo 'prc_'$$i'_bin:	.incbin	"bin_'$$i'"'; \
		echo 'prc_'$$i'_bin_end:'; \
	done >> $(OUT_ASM)

$(BIN_OUT) : $(OUT_LIB)

$(OUT_LIB) : $(LIB_OBJS)
	rm -f $(OUT_LIB)
	ar cr $(OUT_LIB) $(LIB_OBJS)

$(LIB_OBJS) : $(HEADERS) $(CONFIG)
