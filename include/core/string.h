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

#ifndef __CORE_STRING_H
#define __CORE_STRING_H

#include <core/types.h>

#define memset(addr, val, len)	memset_builtin (addr, val, len)
#define memcpy(dest, src, len)	memcpy_builtin (dest, src, len)
#define strcmp(s1, s2)		strcmp_builtin (s1, s2)
#define memcmp(p1, p2, len)	memcmp_builtin (p1, p2, len)
#define strlen(p)		strlen_builtin (p)
#define strncmp(s1, s2, len)	strncmp_builtin (s1, s2, len)

static inline void *
memset_builtin (void *addr, int val, size_t len)
{
	return __builtin_memset (addr, val, len);
}

static inline void *
memcpy_builtin (void *dest, const void *src, size_t len)
{
	return __builtin_memcpy (dest, src, len);
}

static inline int
strcmp_builtin (const char *s1, const char *s2)
{
	return __builtin_strcmp (s1, s2);
}

static inline int
memcmp_builtin (const void *p1, const void *p2, size_t len)
{
	if (__builtin_constant_p (len) && len == 2) {
		const u16 *q1 = p1, *q2 = p2;
		return *q1 == *q2 ? 0 : __builtin_memcmp (p1, p2, len) | 1;
	}
	if (__builtin_constant_p (len) && len == 4) {
		const u32 *q1 = p1, *q2 = p2;
		return *q1 == *q2 ? 0 : __builtin_memcmp (p1, p2, len) | 1;
	}
	return __builtin_memcmp (p1, p2, len);
}

static inline int
strlen_builtin (const char *p)
{
	return __builtin_strlen (p);
}

static inline int
strncmp_builtin (const char *s1, const char *s2, size_t len)
{
	return __builtin_strncmp (s1, s2, len);
}

#endif
