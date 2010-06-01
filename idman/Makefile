PWD := $(shell pwd)
include $(CONFIG)

CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN_PD) += -DIDMAN_PD

DRIVERS-1 += ccid/ccid.o iccard/iccard.o idman_pkcs11/idman_pkcs11.o \
	pcsc/pcsc.o pkcs11/pkcs11.o standardio/standardio.o

TARGET   = idman.o
OUT_LIB  = libidman.a
OBJS     = kernel.o
HEADERS += $(foreach header,$(wildcard *.h),$(PWD)/$(header))
CFLAGS  += $(CONSTANTS-1) -I$(PWD) -I../drivers/usb/
LDFLAGS += -r
define callmake
$(MAKE) -C $(dir)

endef

.PHONY : all clean $(DRIVERS-1) sloc count

all: $(TARGET)

clean: 
	find . -name '*.o' -type f -exec rm -f {} \;

$(TARGET): $(OBJS) $(OUT_LIB) $(CONFIG)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(OUT_LIB)

$(OUT_LIB) : user.o $(DRIVERS-1) $(CONFIG)
	rm -f $(OUT_LIB)
	ar cr $(OUT_LIB) user.o $(DRIVERS-1)

$(OBJS) user.o : $(HEADERS) $(CONFIG)

$(DRIVERS-1):
	$(foreach dir, $(dir $(DRIVERS-1)), $(callmake))

sloc:
	sloccount .

count:
	wc *.[ch]
