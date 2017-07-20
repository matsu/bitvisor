INC    = ../../include/share/config.h

.PHONY : all clean

all : module1.bin module2.bin makedata readdata

clean :
	rm -f module1.bin module2.bin makedata readdata config config.raw

module1.bin : asm.s main.c $(INC)
	gcc -m32 -Wall -s -Wl,-Ttext,0x8000 -nostdlib -nostdinc \
		-idirafter ../../include/share/ -o module1.bin asm.s main.c

makedata : makedata.c
	gcc -Wall `pkg-config --cflags openssl` -o makedata makedata.c \
		`pkg-config --libs openssl || echo -lssl -lcrypto`

readdata : readdata.c
	gcc -Wall `pkg-config --cflags openssl` -o readdata readdata.c \
		`pkg-config --libs openssl || echo -lssl -lcrypto`

module2.bin : config.c configparser.c configparser.h makedata $(INC) \
 bitvisor.conf
	gcc -Wall -idirafter ../../include -o config config.c loaddata.c \
		configparser.c
	./config < bitvisor.conf > config.raw
	./makedata config.raw > module2.bin
