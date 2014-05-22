PKGCONFIG = pkg-config
CFLAGS = $(shell $(PKGCONFIG) --cflags libraw1394) -Wall
LDLIBS = $(shell $(PKGCONFIG) --libs libraw1394)

.PHONY : all
all : ieee1394log

.PHONY : clean
clean :
	rm -f ieee1394log.o ieee1394log
