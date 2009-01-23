CONFIG        = ../.config
include $(CONFIG)

BITS-0        = 32
BITS-1        = 64
OBJS          = $(patsubst %.c,%.o,$(wildcard ./Se/*.c))
HEADERS       = $(wildcard *.h) $(wildcard ../include/core/*.h) $(wildcard ./Se/*.h) $(wildcard ../crypto/*.h) $(wildcard ../crypto/openssl/*.h)
CFLAGS        = -m$(BITS-$(CONFIG_64)) -fsigned-char -mno-red-zone -g -O \
		-nostdinc -fno-builtin $(CONSTANTS-1) -I../include/ -I./ \
		-I./openssl/ -I../core/ -I../crypto/ -fno-stack-protector
ASFLAGS       = --$(BITS-$(CONFIG_64))
LDFLAGS-0     = -Wl,-melf_i386
LDFLAGS-1     = -Wl,-melf_x86_64
LDFLAGS       = $(LDFLAGS-$(CONFIG_64)) -g -nostdlib -Wl,-r
OUT_OBJ       = vpn.o

.PHONY : all clean $(OBJ_OPENSSL)

all : $(OUT_OBJ)

clean : 
	rm -f $(OUT_OBJ) *.o Se/*.o *~

$(OUT_OBJ) : $(OBJS) $(OBJ_OPENSSL) $(CONFIG)
	$(CC) $(LDFLAGS) -o $(OUT_OBJ) $(OBJS) $(OBJ_OPENSSL)

$(OBJS) : $(HEADERS) $(CONFIG)
