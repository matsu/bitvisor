PWD := $(shell pwd)
include $(CONFIG)

CONSTANTS-$(CONFIG_STORAGE_ENC) += -DSTORAGE_ENC
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT

DRIVERS-$(CONFIG_STORAGE_ENC) += crypto/driver.o

OUT_LIB  = libstorage.a
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(foreach header,$(wildcard *.h),$(PWD)/$(header))
CFLAGS  += $(CONSTANTS-1)
define callmake
$(MAKE) -C $(dir)

endef

.PHONY : all clean $(DRIVERS-1) sloc count

all: $(OUT_LIB)

clean: 
	find . -name '*.o' -type f -exec rm -f {} \;

$(OUT_LIB) : $(OBJS) $(DRIVERS-1) $(CONFIG)
	rm -f $(OUT_LIB)
	ar cr $(OUT_LIB) $(OBJS) $(DRIVERS-1)

$(OBJS): $(HEADERS) $(CONFIG)

$(DRIVERS-1):
	$(foreach dir, $(dir $(DRIVERS-1)), $(callmake))

sloc:
	sloccount .

count:
	wc *.[ch]
