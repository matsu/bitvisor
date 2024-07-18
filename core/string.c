/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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
 * 3. Neither the name of the University of Tsukuba nor the names of its
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

#include <core/types.h>

#define WEAK __attribute__ ((weak))

#define UL_LEN	sizeof (unsigned long)
#define ADDR_LOW_UL_RESIDUE(addr) ((unsigned long)(addr) & (UL_LEN - 1))

WEAK void *
memset (void *addr, int val, size_t len)
{
	char *p;

	p = addr;
	while (len--)
		*p++ = val;
	return addr;
}

WEAK void *
memcpy (void *dest, const void *src, size_t len)
{
	char *p;
	const char *q;
	unsigned long residue;
	size_t i, j;

	/* In these cases, there is nothing to do */
	if (src == dest || len == 0)
		goto end;

	p = dest;
	q = src;

	/*
	 * If both src and dest are unaligned with the same residue, we
	 * partially copy the residue part first so that subsequent
	 * copying is aligned.
	 */
	residue = ADDR_LOW_UL_RESIDUE (src);
	if (residue && residue == ADDR_LOW_UL_RESIDUE (dest)) {
		j = UL_LEN - residue;
		if (j > len)
			j = len;
		for (i = 0; i < j; i++)
			*p++ = *q++;
		len -= j;
	}

	/*
	 * Try to copy with UL length. The length depends on the running
	 * architecture.
	 */
	j = len / UL_LEN;
	for (i = 0; i < j; i++) {
		*(unsigned long *)p = *(const unsigned long *)q;
		p += UL_LEN;
		q += UL_LEN;
	}
	len -= j * UL_LEN;

	/* Copy the remaining */
	while (len--)
		*p++ = *q++;
end:
	return dest;
}

WEAK int
strcmp (const char *s1, const char *s2)
{
	int r;
	unsigned char c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;
		r = c1 - c2;
	} while (!r && c1);
	return r;
}

WEAK int
memcmp (const void *p1, const void *p2, size_t len)
{
	int r;
	size_t i;
	const unsigned char *q1, *q2;

	q1 = p1;
	q2 = p2;
	for (r = 0, i = 0; !r && i < len; i++)
		r = *q1++ - *q2++;
	return r;
}

WEAK size_t
strlen (const char *p)
{
	size_t len = 0;

	while (*p++)
		len++;
	return len;
}

WEAK int
strncmp (const char *s1, const char *s2, size_t len)
{
	int r;
	unsigned char c1, c2;

	if (len <= 0)
		return 0;
	do {
		c1 = *s1++;
		c2 = *s2++;
		r = c1 - c2;
	} while (!r && c1 && --len > 0);
	return r;
}
