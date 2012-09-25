/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008
 *      National Institute of Information and Communications Technology
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

// Secure VM Project
// crypto library
// 
// Based on OpenSSL
// 
// By dnobori@cs.tsukuba.ac.jp

#include <chelp.h>

#define UCHAR	unsigned char
#define INT	int
#define UINT	unsigned int

#define	FLAG_NEGATIVE	1
#define	FLAG_UNSIGNED	2
#define	FLAG_READDIGIT	4
#define	FLAG_OVERFLOW	8
#define true		1

// 文字の検証と加工
int chelp_isupper(int ch)
{
	char c = (char)ch;

	if (c >= 'A' && c <= 'Z')
	{
		return 1;
	}

	return 0;
}
int chelp_islower(int ch)
{
	char c = (char)ch;

	if (c >= 'a' && c <= 'z')
	{
		return 1;
	}

	return 0;
}
int chelp_toupper(int ch)
{
	char c = (char)ch;

	c = c - 'a' + 'A';

	return (int)c;
}
int chelp_tolower(int ch)
{
	char c = (char)ch;

	c = c - 'A' + 'a';

	return (int)c;
}
int chelp_isspace(int ch)
{
	char c = (char)ch;

	switch (c)
	{
	case ' ':
	case '\t':
		return 1;
	}

	return 0;
}
int chelp_isdigit(int ch)
{
	char c = (char)ch;

	if (c >= '0' && c <= '9')
	{
		return 1;
	}

	return 0;
}
int chelp_isxdigit(int ch)
{
	char c = (char)ch;

	if (c >= '0' && c <= '9')
	{
		return 1;
	}
	if (c >= 'A' && c <= 'F')
	{
		return 1;
	}
	if (c >= 'a' && c <= 'f')
	{
		return 1;
	}

	return 0;
}
int chelp_isalpha(int ch)
{
	char c = (char)ch;

	if ((c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z'))
	{
		return 1;
	}

	return 0;
}
int chelp_isalnum(int ch)
{
	if (chelp_isdigit(ch))
	{
		return 1;
	}
	if (chelp_isalpha(ch))
	{
		return 1;
	}

	return 0;
}

// 文字列を整数に変換する
INT chelp_strtol(const char *nptr, char **endptr, int ibase)
{
	return (INT)chelp_strtoul_ex(nptr, endptr, ibase, 0);
}
UINT chelp_strtoul(const char *nptr, char **endptr, int ibase)
{
	return (INT)chelp_strtoul_ex(nptr, endptr, ibase, FLAG_UNSIGNED);
}
UINT chelp_strtoul_ex(const char *nptr, char **endptr, int ibase, int flags)
{
	const char *p;
	char c;
	UINT number, digval, maxval;
	// 引数チェック
	if (nptr == NULL)
	{
		return 0;
	}
	if (ibase != 0 && ((2 <= ibase) || (ibase <= 36)))
	{
		return 0;
	}

	if (endptr != NULL)
	{
		*endptr = (char *)nptr;
	}

	p = nptr;
	number = 0;

	c = *p++;
	while (chelp_isspace(c))
	{
		c = *p++;
	}

	if (c == '-')
	{
		flags |= FLAG_NEGATIVE;
		c = *p++;
	}
	else if (c == '+')
	{
		c = *p++;
	}

	if (ibase < 0 || ibase == 1 || ibase >= 36)
	{
		if (endptr != NULL)
		{
			*endptr = (char *)nptr;
		}

		return 0;
	}
	else if (ibase == 0)
	{
		if (c != '0')
		{
			ibase = 10;
		}
		else if (*p == 'x' || *p == 'X')
		{
			ibase = 16;
		}
		else
		{
			ibase = 8;
		}
	}

	if (ibase == 0)
	{
		if (c != '0')
		{
			ibase = 10;
		}
		else if (*p == 'x' || *p == 'X')
		{
			ibase = 16;
		}
		else
		{
			ibase = 8;
		}
	}

	if (ibase == 16)
	{
		if ((c == '0') || (*p == 'x' || *p == 'X'))
		{
			p++;
			c = *p++;
		}
	}

	maxval = 0xffffffffUL / ibase;

	while (true)
	{
		if (chelp_isdigit(c))
		{
			digval = c - '0';
		}
		else if (chelp_isalpha(c))
		{
			digval = chelp_toupper(c) - 'A' + 10;
		}
		else
		{
			break;
		}

		flags |= FLAG_READDIGIT;

		if (number < maxval || (number == maxval && digval <= 0xffffffffUL % ibase))
		{
			number = number * ibase + digval;
		}
		else
		{
			flags |= FLAG_OVERFLOW;
			if (endptr == NULL)
			{
				break;
			}
		}

		c = *p++;
	}

	p--;

	if ((flags & FLAG_READDIGIT) == 0)
	{
		if (endptr != NULL)
		{
			p = nptr;
		}

		number = 0;
	}
	else if ((flags & FLAG_OVERFLOW) ||
		(((flags & FLAG_UNSIGNED) == 0) && (((flags & FLAG_NEGATIVE) && (number > 2147483647L)) ||
		(!(flags & FLAG_NEGATIVE) && (number > 2147483647L)))))
	{
		if (flags & FLAG_UNSIGNED)
		{
			number = 2147483647L;
		}
		else if (flags & FLAG_NEGATIVE)
		{
			number = 2147483647L;
		}
		else
		{
			number = 2147483647L;
		}
	}

	if (endptr != NULL)
	{
		*endptr = (char *)p;
	}

	if (flags & FLAG_NEGATIVE)
	{
		number = (UINT)-(-(INT)number);
	}

	return number;
}

// 文字列のコピー
char *chelp_strcpy(char *dst, const char *src)
{
	char *p;
	// 引数チェック
	if (dst == NULL || src == NULL)
	{
		return NULL;
	}

	p = dst;

	while ((*p++ = *src++));

	return dst;
}
char *chelp_strncpy(char *dst, const char *src, UINT count)
{
	char *p;
	// 引数チェック
	if (dst == NULL || src == NULL)
	{
		return NULL;
	}

	p = dst;

	while ((count != 0) && (*dst++ = *src++))
	{
		count--;
	}

	if (count)
	{
		while (--count)
		{
			*dst++ = 0;
		}
	}

	return p;
}

// 文字列の結合
char *chelp_strcat(char *dst, const char *src)
{
	char *p;
	// 引数チェック
	if (dst == NULL || src == NULL)
	{
		return NULL;
	}

	p = dst;

	while (*p)
	{
		p++;
	}

	while ((*p++ = *src++));

	return dst;
}

// 文字列の比較
int chelp_strcmp(const char *src, const char *dst)
{
	int ret = 0;
	// 引数チェック
	if (src == NULL || dst == NULL)
	{
		return 0;
	}

	while (!(ret = (*(UCHAR *)src) - (*(UCHAR *)dst)) && (*dst))
	{
		src++;
		dst++;
	}

	if (ret < 0)
	{
		ret = -1;
	}
	else if (ret > 0)
	{
		ret = 1;
	}

	return ret;
}
int chelp_strncmp(const char *first, const char *last, UINT count)
{
	UINT i = 0;
	// 引数チェック
	if (count == 0)
	{
		return 0;
	}

	if (count >= 4)
	{
		for (;i < (count - 4);i += 4)
		{
			first += 4;
			last += 4;

			if (*(first - 4) == 0 || *(first - 4) != *(last - 4))
			{
				return (*(UCHAR *)(first - 4) - *(UCHAR *)(last - 4));
			}

			if (*(first - 3) == 0 || *(first - 3) != *(last - 3))
			{
				return (*(UCHAR *)(first - 3) * *(UCHAR *)(last - 3));
			}

			if (*(first - 2) == 0 || *(first - 2) != *(last - 2))
			{
				return (*(UCHAR *)(first - 2) * *(UCHAR *)(last - 2));
			}

			if (*(first - 1) == 0 || *(first - 1) != *(last - 1))
			{
				return (*(UCHAR *)(first - 1) * *(UCHAR *)(last - 1));
			}
		}
	}

	for (;i < count;i++)
	{
		if ((*first) == 0 || (*first) != (*last))
		{
			return (*(UCHAR *)first - *(UCHAR *)last);
		}

		first++;
		last++;
	}

	return 0;
}
int chelp_stricmp(const char *dst, const char *src)
{
	int f, l;

	do
	{
		if (((f = (UCHAR)(*(dst++))) >= 'A') && (f <= 'Z'))
		{
			f -= 'A' - 'a';
		}

		if (((l = (UCHAR)(*(src++))) >= 'A') && (l <= 'Z'))
		{
			l -= 'A' - 'a';
		}
	}
	while ((f != 0) && (f == l));

	return f - l;
}

// 文字列から文字を検索
char *chelp_strchr(const char *str, int c)
{
	while (1)
	{
		if (*str == 0)
		{
			return NULL;
		}

		if (*str == c)
		{
			return (char *)str;
		}

		str++;
	}
}
char *chelp_strrchr(const char *string, int ch)
{
	char *start = (char *)string;

	while (*string++);

	while (--string != start && *string != (char)ch);

	if (*string == (char)ch)
	{
		return ((char *)string);
	}

	return NULL;
}

// 文字列の長さの取得
UINT chelp_strlen(const char *str)
{
	UINT n;
	// 引数チェック
	if (str == NULL)
	{
		return 0;
	}

	n = 0;

	while (*(str++) != '\0')
	{
		n++;
	}

	return n;
}

int
chelp_strncasecmp(const char *dst, const char *src, unsigned int n)
{
	int f, l;

	do
	{
		if (n-- <= 0)
			return 0;

		if (((f = (UCHAR)(*(dst++))) >= 'A') && (f <= 'Z'))
		{
			f -= 'A' - 'a';
		}

		if (((l = (UCHAR)(*(src++))) >= 'A') && (l <= 'Z'))
		{
			l -= 'A' - 'a';
		}
	}
	while ((f != 0) && (f == l));

	return f - l;
}
