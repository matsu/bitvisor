INCLUDE  = ../include
TARGET   = config.o
OBJS     = $(patsubst %.c,%.o,$(wildcard *.c)) \
	   $(patsubst %.s,%.o,$(wildcard *.s))
HEADERS  = $(wildcard *.h) $(wildcard $(INCLUDE)/*.h) 

CC	= gcc
CFLAGS	= -m64 -mno-red-zone -nostdinc -fno-builtin -Wall -I$(INCLUDE) -O -g
LDFLAGS = -g -r -nostdlib 

all: $(TARGET)

clean: 
	rm -f $(TARGET) $(OBJS)

$(OBJS): $(HEADERS)
