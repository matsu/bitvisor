CONFIG        = ../.config
include $(CONFIG)

BITS-0        = 32
BITS-1        = 64
OBJS-$(CONFIG_CRYPTO_VPN) = $(patsubst %.c,%.o,$(wildcard *.c))
OBJS          = $(OBJS-1)
HEADERS       = $(wildcard *.h) $(wildcard ../include/core/*.h) $(wildcard ./openssl/*.h) $(wildcard ../vpn/Se/*.h)
OBJ_OPENSSL   = ./openssl/openssl.o
CFLAGS        = -m$(BITS-$(CONFIG_64)) -fsigned-char -mno-red-zone -g -O \
		-nostdinc -fno-builtin $(CONSTANTS-1) -I../include/ -I./ \
		-I./openssl/ -I../vpn/ -I../core/ -fno-stack-protector
ASFLAGS       = --$(BITS-$(CONFIG_64))
LDFLAGS-0     = -Wl,-melf_i386
LDFLAGS-1     = -Wl,-melf_x86_64
LDFLAGS       = $(LDFLAGS-$(CONFIG_64)) -g -nostdlib -Wl,-r
OUT_OBJ       = crypto.o

.PHONY : all clean $(OBJ_OPENSSL)

all : $(OUT_OBJ)

clean : 
	$(MAKE) -C ./openssl clean
	rm -f $(OUT_OBJ) *.o *~

$(OBJ_OPENSSL) :
	$(MAKE) -C ./openssl

$(OUT_OBJ) : $(OBJS) $(OBJ_OPENSSL) $(CONFIG)
	$(CC) $(LDFLAGS) -o $(OUT_OBJ) $(OBJS) $(OBJ_OPENSSL)

$(OBJS) : $(HEADERS) $(CONFIG)
