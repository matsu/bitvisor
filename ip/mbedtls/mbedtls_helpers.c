/*
 * Copyright (c) 2023 Igel Co., Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <mbedtls_cc.h>
#include <mbedtls_helpers.h>
#include "ip_sys.h"
#include "mbedtls/base64.h"
#include "psa/crypto.h"
#include "psa/crypto_platform.h"
#include "time.h"

time_t
time (time_t *timer)
{
	long long second;
	int microsecond;

	epoch_now (&second, &microsecond);
	if (timer != NULL)
		*timer = (time_t)second;
	return second;
}

/* Referenced from process/sqlite/time.h */
static struct tm *
real_gmtime (const time_t *tt, struct tm *tm_buf)
{
	time_t t = *tt;
	time_t s = t % 86400;
	if (s < 0) {
		s += 86400;
		t -= 86400;
	}
	tm_buf->tm_sec = s % 60;
	s /= 60;
	tm_buf->tm_min = s % 60;
	s /= 60;
	tm_buf->tm_hour = s % 24;

	time_t d = t / 86400; /* Number of days since 1970-01-01 */
	d -= 365 * 30 + 7; /* Number of days since 2000-01-01 */
	d -= 60; /* Number of days since 2000-03-01 */
	int d400 = d % (365 * 400 + 97);
	if (d400 < 0) {
		d400 += 365 * 400 + 97;
		d -= 365 * 400 + 97;
	}
	int y400 = d / (365 * 400 + 97);
	int y100 = d400 / (365 * 100 + 24); /* 0-4 */
	if (y100 == 4)
		y100 = 3;
	int d100 = d400 - y100 * (365 * 100 + 24);
	int d4 = d100 % (365 * 4 + 1);
	int y4 = d100 / (365 * 4 + 1); /* 0-24 */
	int d1 = d4 % 365; /* 0-364 */
	int y1 = d4 / 365; /* 0-4 */
	if (y1 == 4) {
		y1 = 3;
		d1 += 365;
	}
	int m = (d1 * 10 + 5) / 306; /* 0-11 (Mar-Feb) */
	int dm = d1 - (m * 306 + 5) / 10;
	tm_buf->tm_mday = dm + 1;
	tm_buf->tm_mon = m > 9 ? m - 10 : m + 2;
	tm_buf->tm_year = y400 * 400 + y100 * 100 + y4 * 4 + y1 + (m > 9) +
		100;
	tm_buf->tm_wday = (3 + d400) % 7;
	tm_buf->tm_yday = m > 9 ? d1 - 306 : d1 + 59 + (!y1 && (y4 || !y100));
	tm_buf->tm_isdst = 0;
	return tm_buf;
}

struct tm *
mbedtls_platform_gmtime_r (const time_t *tt, struct tm *tm_buf)
{
	return real_gmtime (tt, tm_buf);
}

char *
strstr (const char *str, const char *sub)
{
	if (!*sub)
		return (char *)str;
	while (*str) {
		const char *s = str;
		const char *s1 = sub;
		while (*s && *s1 && *s == *s1) {
			s++;
			s1++;
		}
		if (!*s1)
			return (char *)str; /* `sub` is found */
		str++;
	}
	return NULL; /* `sub` is not found */
}

/* Referenced from mbedtls-2.28.6/programs/util/pem2der.c */
int
convert_pem_to_der (const unsigned char *input, size_t ilen,
		    unsigned char *output, size_t *olen)
{
	int ret;
	const unsigned char *s1, *s2, *end = input + ilen;
	size_t len = 0;

	s1 = (unsigned char *)strstr ((const char *)input, "-----BEGIN");
	if (s1 == NULL) {
		return PEM_MISSING_BEGIN;
	}
	s2 = (unsigned char *)strstr ((const char *)input, "-----END");
	if (s2 == NULL) {
		return PEM_MISSING_END;
	}
	s1 += 10;
	while (s1 < end && *s1 != '-') {
		s1++;
	}
	while (s1 < end && *s1 == '-') {
		s1++;
	}
	if (*s1 == '\r') {
		s1++;
	}
	if (*s1 == '\n') {
		s1++;
	}

	if (s2 <= s1 || s2 > end) {
		return PEM_INVALID_STRUCTURE;
	}
	ret = mbedtls_base64_decode (NULL, 0, &len, (const unsigned char *)s1,
				     s2 - s1);
	if (ret == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
		/* This is an error code from mbedtls_base64_decode */
		return ret;
	}
	if (len > *olen) {
		return PEM_OUTPUT_BUFFER_TOO_SMALL;
	}
	if ((ret = mbedtls_base64_decode (output, len, &len,
					  (const unsigned char *)s1,
					  s2 - s1)) != 0) {
		return ret;
	}
	*olen = len;
	return 0;
}

int
mbedtls_rand (void *rng_state, unsigned char *output, size_t len)
{
	int i;

	for (i = 0; i < len; i++)
		output[i] = rand ();
	return 0;
}

int
mbedtls_f_source (void *data, unsigned char *output, size_t len, size_t *olen)
{
	*olen = len;
	return mbedtls_rand (data, output, len);
}

psa_status_t
mbedtls_psa_external_get_random (
	mbedtls_psa_external_random_context_t *context, unsigned char *output,
	size_t output_size, size_t *output_length)
{
	return mbedtls_f_source (context, output, output_size, output_length);
}

void
debug_print_func (void *context, int level, const char *fname,
		  int line_number, const char *msg)
{
	if (level < 3)
		printf ("[%d] %s(%s:%d)\n", level, msg, fname, line_number);
	else
		printf ("[%d] %s", level, msg);
}
