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

#ifndef _STRING_H_
#define _STRING_H_

#include <core/string.h>

#define strncpy(dst, src, n)	__builtin_strncpy(dst, src, n)
#define strncmp(dst, src, n)	__builtin_strncmp(dst, src, n)
#define strcat(dst, src)	__builtin_strcat(dst, src)
#define strncat(dst, src, n)	__builtin_strncat(dst, src, n)
#define strchr(s, c)		__builtin_strchr(s, c)
#define strstr(s1, s2)		__builtin_strstr(s1, s2)

static inline int min (int a, int b) { return a < b ? a : b; }

static inline void *regcpy(void *dst, void *src, int n)
{
	if (n == 8)
		*(u64 *)dst = *(u64 *)src;
	else if (n == 4)
		*(u32 *)dst = *(u32 *)src;
	else if (n == 2)
		*(u16 *)dst = *(u16 *)src;
	else if (n == 1)
		*(u8 *)dst = *(u8 *)src;
	return dst;
}

#endif
