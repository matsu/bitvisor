#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef TESTLIBC
#include <string.h>
#else
#ifdef __x86_64__
asm (".include \"string.s\"; .code64");
#define memset memset64
#define memcpy memcpy64
#define strcmp strcmp64
#define memcmp memcmp64
#define strlen strlen64
#define strncmp strncmp64
#else
asm (".include \"string.s\"; .code32");
#define memset memset32
#define memcpy memcpy32
#define strcmp strcmp32
#define memcmp memcmp32
#define strlen strlen32
#define strncmp strncmp32
#endif

void *memset (void *addr, int val, int len);
void *memcpy (void *dest, void *src, int len);
int strcmp (char *s1, char *s2);
int memcmp (void *p1, void *p2, int len);
int strlen (char *p);
int strncmp (char *s1, char *s2, int len);
#endif

#define TESTSIZE 1048576
#define BENCH(CODE)							\
	do {								\
		uint64_t t, n = 0;					\
		printf ("%s:\n\t", #CODE);				\
		serialize ();						\
		for (t = gett (); gett () - t < 16777216ULL; n++) {	\
			CODE;						\
			serialize ();					\
		}							\
		n = 0;							\
		for (t = gett (); gett () - t < 16777216ULL; n++) {	\
			CODE;						\
			serialize ();					\
		}							\
		printf (" %" PRIu64 ",", n);				\
		for (int i = 0; i < 5; i++) {				\
			serialize ();					\
			t = gett ();					\
			CODE;						\
			t = gett () - t;				\
			printf (" %" PRIu64, t);			\
		}							\
		printf ("\n");						\
	} while (0)

static inline void
serialize (void)
{
	uint32_t a = 0, b = 0, c = 0, d = 0;
	asm volatile ("cpuid"
		      : "+a" (a), "+b" (b), "+c" (c), "+d" (d)
		      :
		      : "memory");
}

static inline uint64_t
gett (void)
{
	uint32_t a, d;
	asm volatile ("rdtsc" : "=a" (a), "=d" (d));
	return a | (uint64_t)d << 32;
}

static void
warmup (void *buf, size_t size)
{
	uint64_t t = gett ();
	while (gett () - t < 4294967296ULL) {
		serialize ();
		memset (buf, 0, size);
	}
}

int
main (int argc, char **argv)
{
	char *buf = malloc (TESTSIZE);
	/* Quick test basic functionality */
#define T(E) do { if (!(E)) abort (); else printf ("%s: OK\n", #E); } while (0)
	memset (buf, 0, 8);
	memset (buf, '7', 7);
	memset (buf, '6', 6);
	memset (buf, '5', 5);
	memset (buf, '4', 4);
	memset (buf, '3', 3);
	memset (buf, '2', 2);
	memset (buf, '1', 1);
	T (strcmp (buf, "1234") > 0);
	T (strcmp (buf, "1234566") > 0);
	T (strcmp (buf, "1234567") == 0);
	T (strcmp (buf, "1234568") < 0);
	T (strcmp (buf, "1235") < 0);
	T (strlen (buf) == 7);
	memcpy (buf + 4, buf, 3);
	T (strcmp (buf, "1234") > 0);
	T (strcmp (buf, "1234122") > 0);
	T (strcmp (buf, "1234123") == 0);
	T (strcmp (buf, "1234124") < 0);
	T (strcmp (buf, "1235") < 0);
	T (strlen (buf) == 7);
	memcpy (buf, "abcdefg", 8);
	T (strncmp (buf, "abcdef", 7) > 0);
	T (strncmp (buf, "abcdefg", 7) == 0);
	T (strncmp (buf, "abcdefgh", 7) == 0);
	T (strncmp (buf, "abcdefgh", 8) < 0);
	T (strlen (buf) == 7);
	T (memcmp (buf, "abcdef", 6) == 0);
	T (memcmp (buf, "abcdef", 7) > 0);
	T (memcmp (buf, "abcdefgh", 8) < 0);
	memset (buf, 'a', TESTSIZE);
	buf[TESTSIZE - 1] = '\0';
	T (strlen (buf) == TESTSIZE - 1);
	warmup (buf, TESTSIZE);

	volatile int r;
	BENCH (memset (buf, 0, 4));
	BENCH (memset (buf, 0, TESTSIZE));
	BENCH (memcpy (buf, buf + TESTSIZE / 2, 4));
	BENCH (memcpy (buf, buf + TESTSIZE / 2, TESTSIZE / 2));
	memset (buf, 'a', TESTSIZE);
	buf[TESTSIZE - 1] = '\0';
	BENCH (r = strcmp ("0", buf));
	BENCH (r = strcmp ("aaaaaaa", buf + TESTSIZE - 8));
	BENCH (r = strcmp (buf, buf + TESTSIZE / 2));
	BENCH (r = memcmp ("aaaaaaa", buf + TESTSIZE - 8, 7));
	BENCH (r = memcmp (buf, buf + TESTSIZE / 2, TESTSIZE / 2));
	BENCH (r = memcmp (buf, buf + TESTSIZE / 2 - 1, TESTSIZE / 2));
	BENCH (r = memcmp (buf, buf + TESTSIZE / 2 - 1, TESTSIZE / 2 - 1));
	BENCH (r = strlen (buf + TESTSIZE - 8));
	BENCH (r = strlen (buf));
	BENCH (r = strncmp ("0", buf, 7));
	BENCH (r = strncmp ("aaaaaaa", buf + TESTSIZE - 8, 7));
	BENCH (r = strncmp (buf, buf + TESTSIZE / 2 - 1, TESTSIZE / 2));
	return 0;
}
