CONFIG        = ../.config
include $(CONFIG)

BITS-0        = 32
BITS-1        = 64
OBJS-$(CONFIG_CRYPTO_VPN) = $(patsubst %.c,%.o,$(wildcard *.c))
OBJS          = $(OBJS-1)
HEADERS       = $(wildcard *.h) $(wildcard ../include/core/*.h) $(wildcard ./openssl/*.h) $(wildcard ../vpn/Se/*.h)
LIB_OPENSSL   = ./openssl/openssl.a
CFLAGS        = -m$(BITS-$(CONFIG_64)) -fsigned-char -mno-red-zone -g -O \
		-nostdinc -fno-builtin $(CONSTANTS-1) -I../include/ -I./ \
		-I./openssl/ -I../vpn/ -I../core/ -fno-stack-protector \
		-mno-sse
ASFLAGS       = --$(BITS-$(CONFIG_64))
LDFLAGS-0     = -Wl,-melf_i386
LDFLAGS-1     = -Wl,-melf_x86_64
LDFLAGS       = $(LDFLAGS-$(CONFIG_64)) -g -nostdlib -Wl,-r
OUT_LIB       = crypto.a

.PHONY : all clean $(LIB_OPENSSL)

all : $(OUT_LIB)

clean : 
	$(MAKE) -C ./openssl clean
	rm -f $(OUT_LIB) *.o *~

$(LIB_OPENSSL) :
	$(MAKE) -C ./openssl

$(OUT_LIB) : $(OBJS) $(LIB_OPENSSL) $(CONFIG)
	cp $(LIB_OPENSSL) $(OUT_LIB)
	ar cr $(OUT_LIB) $(OBJS)

$(OBJS) : $(HEADERS) $(CONFIG)
