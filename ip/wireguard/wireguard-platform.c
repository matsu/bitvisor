#include <core/random.h>
#include <ip_sys.h>
#include "wireguard-lwip/src/crypto.h"
#include "wireguard-lwip/src/wireguard-platform.h"

#define RAND_RETRY 20

static unsigned long next = 1;

/* This file is derived from wireguard-lwip/example/wireguard-platform.c */

static int
myrand (void)
{
	unsigned int num;
	if (random_num_hw (RAND_RETRY, &num))
		return num % 32768;
	next = next * 1103515245 + 12345;
	/* RAND_MAX assumed to be 32767 */
	return ((unsigned)(next / 65536) % 32768);
}

/* TODO: make a better random byte generator */
void
wireguard_random_bytes (void *bytes, size_t size)
{
	int x;
	uint8_t *out = (uint8_t *)bytes;

	if (next == 1) {
		long long second;
		int microsecond;
		epoch_now (&second, &microsecond);
		next = (long) second;
	}
	for (x = 0; x < size; x++)
		out[x] = myrand () % 0xFF;
}

uint32_t
wireguard_sys_now ()
{
	return ip_sys_now ();
}

/* See https://cr.yp.to/libtai/tai64.html */
/* 64 bit seconds from 1970 = 8 bytes */
/* 32 bit nano seconds from current second */
void
wireguard_tai64n_now (uint8_t *output)
{
	long long second;
	int microsecond;

	epoch_now (&second, &microsecond);
	/*  Split into seconds offset + nanos */
	uint64_t seconds = 0x400000000000000aULL + second;
	uint32_t nanos = microsecond * 1000;
	U64TO8_BIG (output + 0, seconds);
	U32TO8_BIG (output + 8, nanos);
}

bool
wireguard_is_under_load ()
{
	return false;
}
