PWD := $(shell pwd)
include $(CONFIG)

TARGET	 = driver.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS += $(wildcard *.h)
CFLAGS  += -I$(PWD)

OBJS-$(CONFIG_DEBUG_ATA) += ata_debug.o
OBJS-1	 = $(filter-out $(OBJS-0), $(OBJS))

$(TARGET): $(OBJS-1) $(CONFIG)
	$(LD) $(LDFLAGS) -o $(TARGET) $(OBJS-1)

$(OBJS-1): $(HEADERS) $(CONFIG)
