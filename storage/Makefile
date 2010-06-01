PWD := $(shell pwd)
include $(CONFIG)

CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_STORAGE_PD) += -DSTORAGE_PD

DRIVERS-1 += lib/libstorage.a

TARGET   = storage.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(foreach header,$(wildcard *.h),$(PWD)/$(header))
CFLAGS  += $(CONSTANTS-1) -I$(PWD)
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

sloc:
	sloccount .

count:
	wc *.[ch]
