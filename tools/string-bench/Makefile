.PHONY : all

all : string-bench

string-bench : string-bench.c ../../core/string.s
	$(CC) -O -Wa,-I,../../core -o string-bench string-bench.c
