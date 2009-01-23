PWD := $(shell pwd)
include $(CONFIG)

CONSTANTS-$(CONFIG_CRYPTO_VPN) += -DCRYPTO_VPN
CONSTANTS-$(CONFIG_VPN_PRO100) += -DVPN_PRO100
CONSTANTS-$(CONFIG_VPN_PRO1000) += -DVPN_PRO1000

TARGET	 = driver.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(wildcard *.h) $(wildcard $(PWD)/../../vpn/Se/*.h)
CFLAGS  += $(CONSTANTS-1) -I$(PWD)/../vpn/

$(TARGET) : $(OBJS) $(CONFIG)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS)

$(OBJS) : $(HEADERS) $(CONFIG)
