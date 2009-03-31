PWD := $(shell pwd)
include $(CONFIG)

CONSTANTS-$(CONFIG_STORAGE_ENC) += -DSTORAGE_ENC
CONSTANTS-$(CONFIG_IEEE1394_CONCEALER) += -DIEEE1394_CONCEALER
CONSTANTS-$(CONFIG_FWDBG) += -DFWDBG
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS

DRIVERS-$(CONFIG_ATA_DRIVER) += ata/driver.o
DRIVERS-$(CONFIG_USB_DRIVER) += usb/driver.o
DRIVERS-$(CONFIG_STORAGE_ENC) += crypto/driver.o
DRIVERS-$(CONFIG_CRYPTO_VPN) += vpn/driver.o

TARGET   = drivers.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(foreach header,$(wildcard *.h),$(PWD)/$(header))
CFLAGS  += $(CONSTANTS-1) -I$(PWD) -I$(PWD)/../vpn/
LDFLAGS += -r
define callmake
$(MAKE) -C $(dir)

endef

.PHONY : all clean $(DRIVERS-1) sloc count

all: $(TARGET)

clean: 
	find . -name '*.o' -type f -exec rm -f {} \;

$(TARGET): $(OBJS) $(DRIVERS-1) $(CONFIG)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(DRIVERS-1)

$(OBJS): $(HEADERS) $(CONFIG)

$(DRIVERS-1):
	$(foreach dir, $(dir $(DRIVERS-1)), $(callmake))

storage.o:

sloc:
	sloccount .

count:
	wc *.[ch]
