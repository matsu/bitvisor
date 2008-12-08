CONFIG        = ../.config
include $(CONFIG)

CFLAGS        = -fsigned-char -g -O

.PHONY : all clean

all : maketestfs

clean : 
	rm -f maketestfs

maketestfs : maketestfs.c $(CONFIG)
	$(CC) $(CFLAGS) maketestfs.c -o maketestfs


