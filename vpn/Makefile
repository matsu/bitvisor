CONFIG        = ../.config
include $(CONFIG)

# config macros
CONSTANTS-$(CONFIG_CRYPTO_VPN) += -DCRYPTO_VPN
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN) += -DIDMAN
CONSTANTS-$(CONFIG_VPN_PD) += -DVPN_PD

BITS-0        = 32
BITS-1        = 64
OBJS          = user.o crypt.o $(patsubst %.c,%.o,$(wildcard ./Se/*.c))
HEADERS       = $(wildcard *.h) $(wildcard ../include/core/*.h) \
		$(wildcard ./Se/*.h) $(wildcard ../crypto/*.h) \
		$(wildcard ../crypto/openssl/*.h)
CFLAGS        = -m$(BITS-$(CONFIG_64)) -fsigned-char -mno-red-zone -g -O \
		-nostdinc -fno-builtin $(CONSTANTS-1) -I../include/ -I./ \
		-I./openssl/ -I../core/ -I../crypto/ -fno-stack-protector \
		-mno-sse
ASFLAGS       = --$(BITS-$(CONFIG_64))
LDFLAGS-0     = -Wl,-melf_i386
LDFLAGS-1     = -Wl,-melf_x86_64
LDFLAGS       = $(LDFLAGS-$(CONFIG_64)) -g -nostdlib -Wl,-r
OUT_LIB       = libvpn.a
OUT_OBJ       = vpn.o

.PHONY : all clean

all : $(OUT_OBJ)

clean : 
	rm -f $(OUT_OBJ) $(OUT_LIB) *.o Se/*.o *~

$(OUT_LIB) : $(OBJS) $(CONFIG)
	rm -f $(OUT_LIB)
	ar cr $(OUT_LIB) $(OBJS)

$(OUT_OBJ) : kernel.o $(OUT_LIB) $(CONFIG)
	$(CC) $(LDFLAGS) -o $(OUT_OBJ) kernel.o $(OUT_LIB)

$(OBJS) kernel.o : $(HEADERS) $(CONFIG)
