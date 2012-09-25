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

#ifndef	CHELP_H
#define	CHELP_H

#define	CHELP

// OpenSSL のためのコンパイルフラグ
#ifndef	GETPID_IS_MEANINGLESS
#define	GETPID_IS_MEANINGLESS
#endif	// GETPID_IS_MEANINGLESS

#ifndef	L_ENDIAN
#define	L_ENDIAN
#endif	// L_ENDIAN

#ifndef	_CRT_SECURE_NO_DEPRECATE
#define	_CRT_SECURE_NO_DEPRECATE
#endif	// _CRT_SECURE_NO_DEPRECATE

#ifndef	_CRT_NONSTDC_NO_DEPRECATE
#define	_CRT_NONSTDC_NO_DEPRECATE
#endif	// _CRT_NONSTDC_NO_DEPRECATE

#ifndef	OPENSSL_NO_ENGINE
#define	OPENSSL_NO_ENGINE
#endif	// OPENSSL_NO_ENGINE

#ifndef	OPENSSL_NO_DYNAMIC_ENGINE
#define	OPENSSL_NO_DYNAMIC_ENGINE
#endif	// OPENSSL_NO_DYNAMIC_ENGINE

#ifndef	OPENSSL_NO_CAMELLIA
#define	OPENSSL_NO_CAMELLIA
#endif	// OPENSSL_NO_CAMELLIA

#ifndef	OPENSSL_NO_SEED
#define	OPENSSL_NO_SEED
#endif	// OPENSSL_NO_SEED

#ifndef	OPENSSL_NO_RC5
#define	OPENSSL_NO_RC5
#endif	// OPENSSL_NO_RC5

#ifndef	OPENSSL_NO_MDC2
#define	OPENSSL_NO_MDC2
#endif	// OPENSSL_NO_MDC2

#ifndef	OPENSSL_NO_TLSEXT
#define	OPENSSL_NO_TLSEXT
#endif	// OPENSSL_NO_TLSEXT

#ifndef	OPENSSL_NO_KRB5
#define	OPENSSL_NO_KRB5
#endif	// OPENSSL_NO_KRB5

#ifndef	OPENSSL_NO_SOCK
#define	OPENSSL_NO_SOCK
#endif	// OPENSSL_NO_SOCK

#ifndef	OPENSSL_NO_SSL2
#define	OPENSSL_NO_SSL2
#endif	// OPENSSL_NO_SSL2

#ifndef	OPENSSL_NO_SSL3
#define	OPENSSL_NO_SSL3
#endif	// OPENSSL_NO_SSL3

#ifndef	OPENSSL_NO_HW
#define	OPENSSL_NO_HW
#endif	// OPENSSL_NO_HW

#ifndef	OPENSSL_NO_DGRAM
#define	OPENSSL_NO_DGRAM
#endif	// OPENSSL_NO_DGRAM

#ifndef	OPENSSL_NO_FP_API
#define	OPENSSL_NO_FP_API
#endif	// OPENSSL_NO_FP_API

#ifndef	__x86_64__
#define	CRYPT_32BIT
#endif	// __x86_64__

#ifndef	NULL
#define NULL				((void *)0)
#endif

#define	CHELP_WHERE			chelp_printf("[CHELP_WHERE] %s: %u\n", __FILE__, __LINE__);
#define	CHELP_WHERE_P		printf("[CHELP_WHERE_P] %s: %u\n", __FILE__, __LINE__);

#ifdef	CHELP_OPENSSL_SOURCE
// OpenSSL ソースコードのコンパイルのために必要な定義
#ifndef	_WIN64
#ifndef	__CORE_TYPES_H
typedef	unsigned long int	size_t;
#endif	// __CORE_TYPES_H
typedef	long int ssize_t;
#endif	// _WIN64
typedef void * FILE;

#ifndef	_MSC_VER
#ifndef	va_list
#define va_list			__builtin_va_list
#endif	// va_list
#else	// _MSC_VER
typedef char *  va_list;
#endif	// _MSC_VER

typedef unsigned int time_t;
struct tm
{
	int tm_sec, tm_min, tm_hour, tm_mday;
	int tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
};
#define MB_LEN_MAX		5
#define SHRT_MIN		(-32768)
#define SHRT_MAX		32767
/*#define USHRT_MAX		0xffff*/
#define INT_MIN			(-2147483647 - 1)
#define INT_MAX			2147483647
/*#define UINT_MAX		0xffffffff*/
#define LONG_MIN		(-2147483647L - 1)
#define LONG_MAX		2147483647L
#define ULONG_MAX		0xffffffffUL
#define offsetof(s,m)	(size_t)&(((s *)0)->m)

#define	assert(a)

#undef	memcpy
#undef	memset
#undef	memcmp
#undef	memmove
#undef	memchr
#undef	qsort
#define	memcpy(a, b, c)		chelp_memcpy(a, b, (unsigned int)(c))
#define	memset(a, b, c)		chelp_memset(a, b, (unsigned int)(c))
#define	memcmp(a, b, c)		chelp_memcmp(a, b, (unsigned int)(c))
#define	memmove(a, b, c)	chelp_memmove(a, b, (unsigned int)(c))
#define	memchr(a, b, c)		chelp_memchr(a, b, (unsigned int)(c))
#define	qsort				chelp_qsort

#ifdef	_MSC_VER

#ifndef	_WIN64
#define CHELP_INTSIZEOF(n)   ((sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1))
#else	// _WIN64
#define CHELP_INTSIZEOF(n)   ((sizeof(n) + 8 - 1) & ~(8 - 1))
#endif	// _WIN64

#define va_start(ap, v)		(ap = (va_list)(&(v)) + CHELP_INTSIZEOF(v))

#ifndef	_WIN64
#define va_arg(ap,t)		(*(t *)((ap += CHELP_INTSIZEOF(t)) - CHELP_INTSIZEOF(t)))
#else	// _WIN64
#define va_arg(ap,t)		((sizeof(t) > 8 || ( sizeof(t) & (sizeof(t) - 1)) != 0) ? **(t **)((ap += 8) - 8) : *(t *)((ap += 8) - 8))
#endif	// _WIN64

#define va_end(ap)			(ap = (va_list)NULL)


#else	// _MSC_VER

#ifndef	va_start
#define va_start(PTR, LASTARG)	__builtin_va_start (PTR, LASTARG)
#endif	// va_start

#ifndef	va_end
#define va_end(PTR)		__builtin_va_end (PTR)
#endif	// va_end

#ifndef	va_arg
#define va_arg(PTR, TYPE)	__builtin_va_arg (PTR, TYPE)
#endif	// va_arg

#endif	// _MSC_VER


#undef	strlen
#undef	strchr
#undef	strncmp
#undef	strcmp
#undef	strcat
#undef	strcpy
#undef	strncpy
#undef	strrchr
#define	strlen				chelp_strlen
#define	strchr				chelp_strchr
#define	strncmp				chelp_strncmp
#define	strcmp				chelp_strcmp
#define	strcat				chelp_strcat
#define	strcpy				chelp_strcpy
#define	strncpy				chelp_strncpy
#define	strrchr				chelp_strrchr

#define	strtoul				chelp_strtoul
#define	strcasecmp			chelp_stricmp
#define	stricmp				chelp_stricmp
#define	strcmpi				chelp_stricmp
#define	strncasecmp			chelp_strncasecmp

#define	toupper				chelp_toupper
#define	tolower				chelp_tolower
#define	isspace				chelp_isspace
#define	isdigit				chelp_isdigit
#define	isalnum				chelp_isalnum
#define	isalpha				chelp_isalpha
#define	isxdigit			chelp_isxdigit
#define	isupper				chelp_isupper
#define	islower				chelp_islower



#define OPENSSL_NO_STDIO
#define OPENSSL_DISABLE_OLD_DES_SUPPORT
#define TERMIO
#define DECLARE_PEM_write_fp_const DECLARE_PEM_write_fp
#define BUFSIZ 512
#define malloc chelp_malloc
#define realloc chelp_realloc
#define free chelp_free
#define stderr NULL
#define vfprintf BIO_printf
#define abort chelp_abort
#define double long
#define printf chelp_printf

void chelp_abort (void);

static inline char *
getenv (const char *name)
{
	return NULL;
}

static inline void *
BIO_new_file (const char *filename, const char *mode)
{
	return NULL;
}

static inline int
atoi (const char *nptr)
{
	int s, i = 0;
	unsigned int r = 0, m;
	
	switch (nptr[0]) {
	case '+':
		i++;
	default:
		s = 0;
		m = INT_MAX;
		break;
	case '-':
		i++;
		s = 1;
		m = (unsigned int)INT_MAX + 1U;
		break;
	}
	while (nptr[i] >= '0' && nptr[i] <= '9') {
		if (r > m / 10) {
			r = m;
			break;
		}
		r = r * 10 + (nptr[i] - '0');
		if (r > m) {
			r = m;
			break;
		}
		i++;
	}
	if (s)
		return -r;
	else
		return r;
}
#endif	// CHELP_OPENSSL_SOURCE

// chelp.c
void InitCryptoLibrary(unsigned char *initial_seed, int initial_seed_size);
void chelp_print(char *str);
void chelp_printf(char *format, ...);
void chelp_snprintf(char *dst, unsigned int size, char *format, ...);
void chelp_sprintf(char *dst, char *format, ...);
unsigned long long chelp_mul_64_64_64(unsigned long long a, unsigned long long b);
unsigned long long chelp_div_64_32_64(unsigned long long a, unsigned int b);
unsigned int chelp_div_64_32_32(unsigned long long a, unsigned int b);
unsigned int chelp_mod_64_32_32(unsigned long long a, unsigned int b);

// chelp_mem.c
void *chelp_malloc(unsigned long size);
void *chelp_realloc(void *memblock, unsigned long size);
void chelp_free(void *memblock);
void *chelp_memcpy(void *dst, const void *src, unsigned int size);
void *chelp_memset(void *dst, int c, unsigned int count);
int chelp_memcmp(const void *addr1, const void *addr2, unsigned int size);
void *chelp_memmove(void *dst, const void *src, unsigned int count);
void *chelp_memchr(const void *buf, int chr, unsigned int count);
void chelp_qsort(void *base, unsigned int num, unsigned int width, int (*compare_function)(const void *, const void *));
void *chelp_bsearch(void *key, void *base, unsigned int num, unsigned int width, int (*compare_function)(const void *, const void *));
void chelp_swap(unsigned char *a, unsigned char *b, unsigned int width);

// chelp_str.c
unsigned int chelp_strlen(const char *str);
char *chelp_strrchr(const char *string, int ch);
char *chelp_strchr(const char *str, int c);
int chelp_strncmp(const char *first, const char *last, unsigned int count);
int chelp_strcmp(const char *src, const char *dst);
int chelp_stricmp(const char *dst, const char *src);
char *chelp_strcat(char *dst, const char *src);
char *chelp_strcpy(char *dst, const char *src);
char *chelp_strncpy(char *dst, const char *src, unsigned int count);
int chelp_strtol(const char *nptr, char **endptr, int ibase);
unsigned int chelp_strtoul(const char *nptr, char **endptr, int ibase);
unsigned int chelp_strtoul_ex(const char *nptr, char **endptr, int ibase, int flags);
int chelp_isupper(int ch);
int chelp_islower(int ch);
int chelp_toupper(int ch);
int chelp_tolower(int ch);
int chelp_isspace(int ch);
int chelp_isdigit(int ch);
int chelp_isxdigit(int ch);
int chelp_isalpha(int ch);
int chelp_isalnum(int ch);
int chelp_strncasecmp(const char *dst, const char *src, unsigned int n);




#endif	// CHELP_H

