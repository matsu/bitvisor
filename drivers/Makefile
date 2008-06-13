CONFIG        = ../.config
include $(CONFIG)

CONSTANTS-$(CONFIG_ATA_DRIVER) += -DATA_DRIVER
CONSTANTS-$(CONFIG_STORAGE_ENC) += -DSTORAGE_ENC
CONSTANTS-$(CONFIG_IEEE1394_CONCEALER) += -DIEEE1394_CONCEALER
CONSTANTS-$(CONFIG_FWDBG) += -DFWDBG

INCLUDE  = ../include
TARGET   = drivers.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c)) \
	   $(patsubst %.s,%.o,$(wildcard *.s))
HEADERS  = $(wildcard *.h) $(wildcard $(INCLUDE)/*.h) \
	   $(wildcard $(INCLUDE)/*/*.h)

BITS-0	= 32
BITS-1	= 64

CC	= gcc
CFLAGS	= -m$(BITS-$(CONFIG_64)) -mno-red-zone -ffreestanding -Wall -nostdinc \
	  -I$(INCLUDE) -O2 -g $(CONSTANTS-1) -fno-stack-protector
LDFLG-0	= -Wl,-melf_i386
LDFLG-1	= -Wl,-melf_x86_64
LDFLAGS = $(LDFLG-$(CONFIG_64)) -g -nostdlib -Wl,-r

all: $(TARGET)

clean: 
	rm -f $(TARGET) $(OBJS)

sloc:
	sloccount .

count:
	wc *.[ch]


$(TARGET): $(OBJS) $(CONFIG)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)

$(OBJS): $(HEADERS) $(CONFIG)
