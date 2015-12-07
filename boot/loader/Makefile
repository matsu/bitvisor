CC=gcc

.PHONY : all

all : bootloader bootloaderusb

bootloader : bootloader.s
	$(CC) -nostdlib -Wl,--oformat,binary -o bootloader bootloader.s

bootloaderusb : bootloaderusb.s
	$(CC) -nostdlib -Wl,--oformat,binary -o bootloaderusb bootloaderusb.s
