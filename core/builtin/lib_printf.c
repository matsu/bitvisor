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

#include <lib_printf.h>
#include <lib_putchar.h>
#include <lib_string.h>

#define NULL			((void *)0)
#define va_start(PTR, LASTARG)	__builtin_va_start (PTR, LASTARG)
#define va_end(PTR)		__builtin_va_end (PTR)
#define va_arg(PTR, TYPE)	__builtin_va_arg (PTR, TYPE)
#define va_list			__builtin_va_list
typedef unsigned long int	size_t;
#define spinlock_t int
#define spinlock_init(a) (*a) = 0
#define spinlock_lock(a)
#define spinlock_unlock(a)

int vprintf (const char *format, va_list ap);
int vsnprintf (char *str, size_t size, const char *format, va_list ap);

/* printf 20071010

   %[flags][width][.[precision]][length modifier]<conversion>

   flags:
   #	alternate form
   0	zero padded
   -	left adjusted
   ' '	a blank before a positive number
   +	a sign before a number

   width, precision:
   decimal number only.

   length modifier:
   h	short
   hh	char
   l	long
   ll	long long
   j	intmax_t
   z	size_t
   t	ptrdiff_t

   conversion:
   d, i	integer as decimal
   o	unsigned integer as octal number
   u	unsigned integer as decimal number
   X	unsigned integer as capital hexadecimal number
   x	unsigned integer as hexadecimal number
   c	int as character
   s	char* as string
   p	void* as hexadecimal number
   %	%
*/

struct snputchar_data {
	char *buf;
	size_t len;
};

struct parse_data {
	int width;
	int precision;
};
	
#define FLAG_ALTERNATE_FORM	0x1
#define FLAG_ZERO_PADDED	0x2
#define FLAG_LEFT_ADJUSTED	0x4
#define FLAG_POSITIVE_BLANK	0x8
#define FLAG_POSITIVE_SIGN	0x10
#define HAS_WIDTH		0x20
#define HAS_PRECISION		0x40
#define LENGTH_SHORT		0x80
#define LENGTH_CHAR		0x100
#define LENGTH_LONG		0x200
#define LENGTH_LONGLONG		0x400
#define LENGTH_INTMAX		0x800
#define LENGTH_SIZE		0x1000
#define LENGTH_PTRDIFF		0x2000
#define CONVERSION_INT		0x4000
#define CONVERSION_DECIMAL	0x8000
#define CONVERSION_UINT		0x10000
#define CONVERSION_OCTAL	0x20000
#define CONVERSION_CAPITAL	0x40000
#define CONVERSION_HEXADECIMAL	0x80000
#define CONVERSION_CHARACTER	0x100000
#define CONVERSION_CHARP	0x200000
#define CONVERSION_VOIDP	0x400000
#define CONVERSION_NONE		0x800000
#define END_STRING		0x1000000
#define FLAG_MINUS		0x2000000
#define FLAG_PLUS		0x4000000

#define PUTCHAR(c) do { if (func (c, data) < 0) goto error; n++; } while (0)

static unsigned long long sub10[] = {
	10000000000000000000ULL,
	1000000000000000000ULL,
	100000000000000000ULL,
	10000000000000000ULL,
	1000000000000000ULL,
	100000000000000ULL,
	10000000000000ULL,
	1000000000000ULL,
	100000000000ULL,
	10000000000ULL,
	1000000000ULL,
	100000000ULL,
	10000000ULL,
	1000000ULL,
	100000ULL,
	10000ULL,
	1000ULL,
	100ULL,
	10ULL,
	1ULL,
	0ULL,
};

static unsigned long long sub8[] = {
	01000000000000000000000ULL,
	0100000000000000000000ULL,
	010000000000000000000ULL,
	01000000000000000000ULL,
	0100000000000000000ULL,
	010000000000000000ULL,
	01000000000000000ULL,
	0100000000000000ULL,
	010000000000000ULL,
	01000000000000ULL,
	0100000000000ULL,
	010000000000ULL,
	01000000000ULL,
	0100000000ULL,
	010000000ULL,
	01000000ULL,
	0100000ULL,
	010000ULL,
	01000ULL,
	0100ULL,
	010ULL,
	01ULL,
	00ULL,
};

static unsigned long long sub16[] = {
	0x1000000000000000ULL,
	0x100000000000000ULL,
	0x10000000000000ULL,
	0x1000000000000ULL,
	0x100000000000ULL,
	0x10000000000ULL,
	0x1000000000ULL,
	0x100000000ULL,
	0x10000000ULL,
	0x1000000ULL,
	0x100000ULL,
	0x10000ULL,
	0x1000ULL,
	0x100ULL,
	0x10ULL,
	0x1ULL,
	0x0ULL,
};

static spinlock_t printf_lock;

static int
parse_format (const char **format, int *width, int *precision)
{
	char c;
	int r = 0;

	*width = 0;
	*precision = 0;
	while ((c = *(*format)++) != '\0') {
		switch (c) {
		case '#':
			r |= FLAG_ALTERNATE_FORM;
			break;
		case '0':
			r |= FLAG_ZERO_PADDED;
			break;
		case '-':
			r |= FLAG_LEFT_ADJUSTED;
			break;
		case ' ':
			r |= FLAG_POSITIVE_BLANK;
			break;
		case '+':
			r |= FLAG_POSITIVE_SIGN;
			break;
		default:
			goto parse_width;
		}
	}
	return END_STRING;
	while ((c = *(*format)++) != '\0') {
parse_width:
		if (c >= '0' && c <= '9') {
			r |= HAS_WIDTH;
			*width *= 10;
			*width += (c - '0');
		} else {
			goto parse_precision;
		}
	}
	return END_STRING;
parse_precision:
	if (c != '.')
		goto parse_length_modifier;
	r |= HAS_PRECISION;
	while ((c = *(*format)++) != '\0') {
		if (c >= '0' && c <= '9') {
			*precision *= 10;
			*precision += (c - '0');
		} else {
			goto parse_length_modifier;
		}
	}
	return END_STRING;
	while ((c = *(*format)++) != '\0') {
parse_length_modifier:
		switch (c) {
		case 'h':
			if (r & LENGTH_SHORT)
				r |= LENGTH_CHAR;
			else
				r |= LENGTH_SHORT;
			break;
		case 'l':
			if (r & LENGTH_LONG)
				r |= LENGTH_LONGLONG;
			else
				r |= LENGTH_LONG;
			break;
		case 'j':
			r |= LENGTH_INTMAX;
			break;
		case 'z':
			r |= LENGTH_SIZE;
			break;
		case 't':
			r |= LENGTH_PTRDIFF;
			break;
		default:
			goto parse_conversion;
		}
	}
	return END_STRING;
parse_conversion:
	switch (c) {
	case 'd':
	case 'i':
		r |= CONVERSION_INT | CONVERSION_DECIMAL;
		break;
	case 'o':
		r |= CONVERSION_UINT | CONVERSION_OCTAL;
		break;
	case 'u':
		r |= CONVERSION_UINT | CONVERSION_DECIMAL;
		break;
	case 'X':
		r |= CONVERSION_CAPITAL;
		/* Fall through */
	case 'x':
		r |= CONVERSION_UINT | CONVERSION_HEXADECIMAL;
		break;
	case 'c':
		r |= CONVERSION_INT | CONVERSION_CHARACTER;
		break;
	case 's':
		r |= CONVERSION_CHARP;
		break;
	case 'p':
		r |= CONVERSION_VOIDP | CONVERSION_HEXADECIMAL |
			FLAG_ALTERNATE_FORM;
		break;
	case '%':
		r |= CONVERSION_NONE;
		break;
	}
	return r;
}

static int
valconv (unsigned long long val, char *buf, unsigned long long *sub, char *str)
{
	int n = 0;
	int digit;

	if (val == 0) {
		*buf++ = '0';
		return 1;
	}
	while (val < *sub)
		sub++;
	while (*sub) {
		digit = 0;
		while (val >= *sub) {
			digit++;
			val -= *sub;
		}
		*buf++ = str[digit];
		n++;
		sub++;
	}
	return n;
}

static int
do_conversion_int (unsigned long long val, int f, int width, int precision,
		   int (*func)(int c, void *data), void *data)
{
	int len = 0;
	int n = 0;
	int leftsp = 0, leftlen = 0, leftzero = 0, rightsp = 0;
	char buf[32], *leftbuf = "", *bufp;

	if (f & CONVERSION_CHARACTER) {
		PUTCHAR ((int)val);
		return n;
	}
	if ((f & HAS_PRECISION) && precision == 0 && val == 0)
		return 0;
	if (f & CONVERSION_DECIMAL)
		len = valconv (val, buf, sub10, "0123456789");
	else if (f & CONVERSION_OCTAL)
		len = valconv (val, buf, sub8, "01234567");
	else if ((f & CONVERSION_HEXADECIMAL) && (f & CONVERSION_CAPITAL))
		len = valconv (val, buf, sub16, "0123456789ABCDEF");
	else if (f & CONVERSION_HEXADECIMAL)
		len = valconv (val, buf, sub16, "0123456789abcdef");
	if ((f & HAS_PRECISION) && precision > len)
		leftzero = precision - len;
	if (!(f & CONVERSION_DECIMAL) &&
	    (f & FLAG_ALTERNATE_FORM) && val != 0) {
		if ((f & CONVERSION_OCTAL) && leftzero == 0) {
			leftbuf = "0";
			leftlen = 1;
		} else if (f & CONVERSION_HEXADECIMAL) {
			if (f & CONVERSION_CAPITAL)
				leftbuf = "0X";
			else
				leftbuf = "0x";
			leftlen = 2;
		}
	}
	if (f & CONVERSION_DECIMAL) {
		if (f & FLAG_PLUS) {
			if (f & FLAG_POSITIVE_SIGN) {
				leftbuf = "+";
				leftlen = 1;
			} else if (f & FLAG_POSITIVE_BLANK) {
				leftbuf = " ";
				leftlen = 1;
			}
		} else if (f & FLAG_MINUS) {
			leftbuf = "-";
			leftlen = 1;
		}
	}
	if ((f & HAS_WIDTH) && width > leftlen + leftzero + len) {
		if (f & FLAG_LEFT_ADJUSTED)
			rightsp = width - (leftlen + leftzero + len);
		else
			leftsp = width - (leftlen + leftzero + len);
	}
	if ((f & FLAG_ZERO_PADDED) && !(f & HAS_PRECISION)) {
		leftzero += leftsp;
		leftsp = 0;
	}
	bufp = buf;
	while (leftsp--)
		PUTCHAR (' ');
	while (leftlen--)
		PUTCHAR (*leftbuf++);
	while (leftzero--)
		PUTCHAR ('0');
	while (len--)
		PUTCHAR (*bufp++);
	while (rightsp--)
		PUTCHAR (' ');
error:
	return n;
}

static int
do_conversion_string (const char *str, int f, int width, int precision,
		      int (*func)(int c, void *data), void *data)
{
	int len;
	int n = 0;
	int leftsp = 0, rightsp = 0;

	len = 0;
	while (!((f & HAS_PRECISION) && len >= precision) &&
	       str[len] != '\0')
		len++;
	if ((f & HAS_WIDTH) && width > len) {
		if (f & FLAG_LEFT_ADJUSTED)
			rightsp = width - len;
		else
			leftsp = width - len;
	}
	while (leftsp--)
		PUTCHAR (' ');
	while (len--)
		PUTCHAR (*str++);
	while (rightsp--)
		PUTCHAR (' ');
error:
	return n;
}

static int
do_printf (const char *format, va_list ap, int (*func)(int c, void *data),
	   void *data)
{
	char c;
	int n, f;
	int width, precision;

	n = 0;
	while ((c = *format++) != '\0') {
		if (c == '%') {
			f = parse_format (&format, &width, &precision);
			if (f & LENGTH_INTMAX)
				f |= LENGTH_LONGLONG;
			else if (f & LENGTH_SIZE)
				f |= LENGTH_LONG;
			else if (f & LENGTH_PTRDIFF)
				f |= LENGTH_LONG;
			if (f == END_STRING) {
				break;
			} else if (f & CONVERSION_NONE) {
				PUTCHAR ('%');
			} else if (f & CONVERSION_INT) {
				long long intval;
				unsigned long long uintval;

				if (f & LENGTH_CHAR)
					intval = (long long)va_arg (ap, int);
				else if (f & LENGTH_SHORT)
					intval = (long long)va_arg (ap, int);
				else if (f & LENGTH_LONGLONG)
					intval = va_arg (ap, long long);
				else if (f & LENGTH_LONG)
					intval = (long long)va_arg (ap, long);
				else
					intval = (long long)va_arg (ap, int);
				if (intval < 0) {
					uintval = (unsigned long long)-intval;
					f |= FLAG_MINUS;
				} else {
					uintval = (unsigned long long)intval;
					f |= FLAG_PLUS;
				}
				n += do_conversion_int (uintval, f, width,
							precision, func, data);
				continue;
			} else if (f & CONVERSION_UINT) {
				unsigned long long uintval;

				if (f & LENGTH_CHAR)
					uintval = (unsigned long long)
						va_arg (ap, unsigned int);
				else if (f & LENGTH_SHORT)
					uintval = (unsigned long long)
						va_arg (ap, unsigned int);
				else if (f & LENGTH_LONGLONG)
					uintval = va_arg (ap,
							  unsigned long long);
				else if (f & LENGTH_LONG)
					uintval = (unsigned long long)
						va_arg (ap, unsigned long);
				else
					uintval = (unsigned long long)
						va_arg (ap, unsigned int);
				n += do_conversion_int (uintval, f, width,
							precision, func, data);
			} else if (f & CONVERSION_VOIDP) {
				void *voidpval;
				unsigned long long uintval;

				voidpval = va_arg (ap, void *);
				uintval = (unsigned long long)
					(unsigned long)voidpval;
				n += do_conversion_int (uintval, f, width,
							precision, func, data);
			} else if (f & CONVERSION_CHARP) {
				char *charpval;

				charpval = va_arg (ap, char *);
				if (charpval == NULL)
					charpval = "(null)";
				n += do_conversion_string (charpval, f, width,
							   precision, func,
							   data);
			} else {
				n += do_conversion_string ("FORMAT ERROR",
							   CONVERSION_CHARP,
							   0, 0, func, data);
			}
		} else {
			PUTCHAR (c);
		}
	}
error:
	return n;
}

static int
do_putchar (int c, void *data)
{
	putchar (c);
	return 0;
}

static int
do_snputchar (int c, void *data)
{
	struct snputchar_data *p;

	p = data;
	if (!p->len)
		return -1;
	if (!--p->len)
		c = '\0';
	*p->buf++ = (char)c;
	return 0;
}

int
printf (const char *format, ...)
{
	va_list ap;
	int r;

	va_start (ap, format);
	r = vprintf (format, ap);
	va_end (ap);
	return r;
}

int
vprintf (const char *format, va_list ap)
{
	int r;

	spinlock_lock (&printf_lock);
	r = do_printf (format, ap, do_putchar, NULL);
	spinlock_unlock (&printf_lock);
	return r;
}

int
snprintf (char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int r;

	va_start (ap, format);
	r = vsnprintf (str, size, format, ap);
	va_end (ap);
	return r;
}

int
vsnprintf (char *str, size_t size, const char *format, va_list ap)
{
	struct snputchar_data data;
	int r;

	data.buf = str;
	data.len = size;
	r = do_printf (format, ap, do_snputchar, &data);
	do_snputchar ('\0', &data);
	return r;
}

void
printf_init_global (void)
{
	spinlock_init (&printf_lock);
}
