PWD := $(shell pwd)
include $(CONFIG)

CONSTANTS-$(CONFIG_STORAGE_ENC) += -DSTORAGE_ENC
CONSTANTS-$(CONFIG_UHCI_DRIVER) += -DUHCI_DRIVER
CONSTANTS-$(CONFIG_EHCI_CONCEAL) += -DEHCI_CONCEALER
CONSTANTS-$(CONFIG_PS2KBD_F10USB) += -DF10USBTEST
CONSTANTS-$(CONFIG_PS2KBD_F12USB) += -DF12UHCIFRAME
CONSTANTS-$(CONFIG_USBMSC_HANDLE) += -DUSBMSC_HANDLE
CONSTANTS-$(CONFIG_IEEE1394_CONCEALER) += -DIEEE1394_CONCEALER
CONSTANTS-$(CONFIG_FWDBG) += -DFWDBG
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT

DRIVERS-$(CONFIG_ATA_DRIVER) += ata/driver.o
DRIVERS-$(CONFIG_STORAGE_ENC) += crypto/driver.o

TARGET   = drivers.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(foreach header,$(wildcard *.h),$(PWD)/$(header))
CFLAGS  += $(CONSTANTS-1) -I$(PWD)
LDFLAGS += -r


.PHONY : all clean $(DRIVERS-1) sloc count

all: $(TARGET)

clean: 
	find . -name '*.o' -type f -exec rm -f {} \;

$(TARGET): $(OBJS) $(DRIVERS-1) $(CONFIG)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(DRIVERS-1)

$(OBJS): $(HEADERS) $(CONFIG)

$(DRIVERS-1):
	@drivers="$(DRIVERS-1)";\
	for driver in $$drivers; do \
	make -C `dirname $$driver`; \
	done

storage.o: storage_keys.conf

storage_keys.conf: storage_keys.conf.tmpl
	if [ \! -e storage_keys.conf ]; then cp storage_keys.conf.tmpl storage_keys.conf; fi

sloc:
	sloccount .

count:
	wc *.[ch]
