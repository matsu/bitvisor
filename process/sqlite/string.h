/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2009 Igel Co., Ltd
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

#define memset(addr, val, len)	__builtin_memset (addr, val, len)
#define memcpy(dest, src, len)	__builtin_memcpy (dest, src, len)
#define strcmp(s1, s2)		__builtin_strcmp (s1, s2)
#define memcmp(p1, p2, len)	__builtin_memcmp (p1, p2, len)
#define strlen(p)		__builtin_strlen (p)
#define strncmp(s1, s2, len)	__builtin_strncmp (s1, s2, len)

static inline size_t
strcspn (const char *s, const char *reject)
{
	const char *p;
	for (p = s; *p; p++)
		for (const char *q = reject; *q; q++)
			if (*p == *q)
				goto ret;
ret:
	return p - s;
}

static inline void *
memmove (void *dest, const void *src, size_t n)
{
	unsigned long destaddr = (unsigned long)dest;
	unsigned long srcaddr = (unsigned long)src;
	if (destaddr + n <= srcaddr || srcaddr + n <= destaddr)
		return memcpy (dest, src, n);
	u8 *d = dest;
	const u8 *s = src;
	if (destaddr < srcaddr)
		for (size_t i = 0; i < n; i++)
			d[i] = s[i];
	else if (destaddr > srcaddr)
		for (size_t i = n; i-- > 0;)
			d[i] = s[i];
	return dest;
}

static inline char *
strrchr (const char *s, int c)
{
	char *ret = NULL;
	do
		if (*s == (char)c)
			ret = (char *)s;
	while (*s++);
	return ret;
}
