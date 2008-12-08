PWD := $(shell pwd)
include $(CONFIG)

CFLAGS-$(CONFIG_SELECT_AES_GLADMAN) += -DAES_GLADMAN

TARGET	 = driver.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(foreach header,$(wildcard *.h),$(PWD)/$(header))
CFLAGS  += -I$(PWD) -I../../crypto/ $(CFLAGS-1)

DRIVERS-1 =
DRIVERS-$(CONFIG_SELECT_AES_GLADMAN) = aes-gladman/driver.o


.PHONY : $(DRIVERS-1)

$(TARGET): $(DRIVERS-1) $(OBJS) $(CONFIG)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS) $(DRIVERS-1)

$(OBJS): $(HEADERS) $(CONFIG)

$(DRIVERS-1):
	@drivers="$(DRIVERS-1)";\
	for driver in $$drivers; do \
	make -C `dirname $$driver`; \
	done
