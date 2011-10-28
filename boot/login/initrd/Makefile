MINIOS_INIT = ../minios_init/minios_init
MINIOS_FILES = ../minios_init/passscreen-1 ../minios_init/passscreen-2 ../minios_init/passscreen-3
MINIOS_LIBS = /lib/libc.so.6 /lib/libusb-0.1.so.4 /lib/ld-linux.so.2 /usr/lib/libssl.so.0.9.8 /usr/lib/libcrypto.so.0.9.8 /usr/lib/libz.so.1 /lib/libdl.so.2
MINIOS_CONF = ../conf

.PHONY : all clean initrd.gz

all : initrd.gz

clean :
	rm -f initrd.gz

initrd.gz : $(MINIOS_INIT) $(MINIOS_CONF)/bitvisor.conf
	rm -rf work
	mkdir work
	cat dev.tar.gz | gzip -dc | (cd work/ && tar xf -)
	mkdir work/lib
	#cp $(MINIOS_LIBS) work/lib/
	cp $(MINIOS_INIT) work/init
	strip work/init
	cp $(MINIOS_FILES) work/
	cp -r $(MINIOS_CONF) work/
	(cd work/ && find . | cpio -H newc -o) | gzip -9c > initrd.gz
	rm -rf work
