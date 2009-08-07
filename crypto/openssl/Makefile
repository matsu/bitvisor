CONFIG        = ../../.config
include $(CONFIG)

BITS-0        = 32
BITS-1        = 64
OBJS-1        = aes_core.o
OBJS-$(CONFIG_CRYPTO_VPN) = $(patsubst %.c,%.o,$(wildcard *.c))
OBJS          = $(OBJS-1)
HEADERS       = $(wildcard *.h)
CFLAGS        = -m$(BITS-$(CONFIG_64)) -fsigned-char -mno-red-zone -g -O \
		-nostdinc -fno-builtin $(CONSTANTS-1) -I../../include/ -I../ \
		-I./ -I../../vpn/ -fno-stack-protector -mno-sse
ASFLAGS       = --$(BITS-$(CONFIG_64))
LDFLAGS-0     = -Wl,-melf_i386
LDFLAGS-1     = -Wl,-melf_x86_64
LDFLAGS       = $(LDFLAGS-$(CONFIG_64)) -g -nostdlib -Wl,-r
OUT_OBJ       = openssl.o

.PHONY : all clean

all : $(OUT_OBJ)

clean : 
	rm -f $(OUT_OBJ) *.o *~

$(OUT_OBJ) : $(OBJS) $(CONFIG)
	$(CC) $(LDFLAGS) -o $(OUT_OBJ) $(OBJS)

$(OBJS) : $(HEADERS) $(CONFIG)
