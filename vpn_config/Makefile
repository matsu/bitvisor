CONFIG = ../.config
include $(CONFIG)

all : testfs.c

testfs.c :
	../maketestfs/maketestfs $(wildcard *.txt) $(wildcard *.key) $(wildcard *.crt) $(wildcard *.cer) $(wildcard *.pem)

clean :
	rm -f testfs.c testfs.h

