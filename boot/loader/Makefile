.PHONY : all

all : bootloader bootloaderusb

bootloader : bootloader.s
	gcc -nostdlib -Wl,--oformat,binary -o bootloader bootloader.s

bootloaderusb : bootloaderusb.s
	gcc -nostdlib -Wl,--oformat,binary -o bootloaderusb bootloaderusb.s
