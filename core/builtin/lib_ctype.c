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

#include <lib_ctype.h>

int
isalnum (int c)
{
	return isalpha(c) || isdigit(c);
}

int
isalpha (int c)
{
	return isupper(c) || islower(c);
}

int
isascii (int c)
{
	return c >= '\0' && c <= '\177';
}

int
isblank (int c)
{
	return c == ' ' || c == '\t';
}

int
iscntrl (int c)
{
	return isascii (c) && !isprint (c);
}

int
isdigit (int c)
{
	return c >= '0' && c <= '9';
}

int
isgraph (int c)
{
	return isprint (c) && !isblank (c);
}

int
islower (int c)
{
	return c >= 'a' && c <= 'z';
}

int
isprint (int c)
{
	return c >= ' ' && c <= '~';
}

int
ispunct (int c)
{
	return isgraph (c) && !isalnum (c);
}

int
isspace (int c)
{
	return isblank (c) || c == '\f' || c == '\n' || c == '\r' || c == '\v';
}

int
isupper (int c)
{
	return c >= 'A' && c <= 'Z';
}

int
isxdigit (int c)
{
	return isdigit (c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}
