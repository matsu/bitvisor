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

#include "strtol.h"

#define ULONG_MAX (~0UL)
#define LONG_MAX ((long int)(ULONG_MAX >> 1))

static int
isblank (int c)
{
	return c == ' ' || c == '\t';
}

static int
isspace (int c)
{
	return isblank (c) || c == '\f' || c == '\n' || c == '\r' || c == '\v';
}

static unsigned int
digit_to_num (char c, int base)
{
	unsigned int n;

	if (c >= '0' && c <= '9')
		n = c - '0';
	else if (c >= 'A' && c <= 'Z')
		n = c - 'A' + 10;
	else if (c >= 'a' && c <= 'z')
		n = c - 'a' + 10;
	else
		n = base;
	if (n >= base)
		n = base;
	if (base < 2 || base > 36)
		n = base;
	return n;
}

long int
strtol (char *s, char **e, int base)
{
	int b;
	enum { SIGN_NONE, SIGN_PLUS, SIGN_MINUS } sign;
	unsigned long int val = 0, valmax, c;

	while (isspace (*s))
		s++;
	if (*s == '+') {
		sign = SIGN_PLUS;
		s++;
		valmax = LONG_MAX;
	} else if (*s == '-') {
		sign = SIGN_MINUS;
		s++;
		valmax = (unsigned long int)LONG_MAX + 1;
	} else {
		sign = SIGN_NONE;
		valmax = ULONG_MAX;
	}
	if (s[0] == '0') {
		if (s[1] == 'X' || s[1] == 'x')
			b = 16;
		else
			b = 8;
	} else {
		b = 10;
	}
	if (base == 0)
		base = b;
	if (base == 16 && b == 16)
		s += 2;
	for (;;) {
		if (e)
			*e = s;
		c = digit_to_num (*s, base);
		if (c == base)
			break;
		if (val > valmax / base)
			goto overflow_or_underflow;
		val = val * base + c;
		if (val > valmax)
			goto overflow_or_underflow;
		s++;
	}
	if (sign == SIGN_MINUS)
		return -val;
	else
		return val;
overflow_or_underflow:
	if (sign == SIGN_MINUS)
		return -valmax;
	else
		return valmax;
}
