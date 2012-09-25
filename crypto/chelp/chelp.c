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

#define	CHELP_OPENSSL_SOURCE

#include <core/arith.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/types.h>
#include <chelp.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/pkcs7.h>
#include <openssl/pkcs12.h>
#include <openssl/rc4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/des.h>
#include <openssl/dh.h>
#include <openssl/pem.h>

#define UINT	unsigned int
#define UINT64	unsigned long long int

bool chelp_inited = false;

static const char rnd_seed[] = "string to make the random number generator think it has entropy";

// 暗号化ライブラリの初期化
void InitCryptoLibrary(unsigned char *initial_seed, int initial_seed_size)
{
	unsigned char tmp[16];
	if (chelp_inited)
	{
		return;
	}
	// 引数チェック
	if (initial_seed_size != 0 && initial_seed == NULL)
	{
		return;
	}

	OpenSSL_add_all_ciphers();
	SSLeay_add_all_digests();
	ERR_load_crypto_strings();
	RAND_poll();
	RAND_seed(rnd_seed, sizeof(rnd_seed));
	RAND_seed(initial_seed, initial_seed_size);

	RAND_bytes(tmp, sizeof(tmp));

	chelp_inited = true;
}

// printf 関数 (デバッグ用)
void chelp_printf(char *format, ...)
{
	va_list args;
	char tmp[1024];
	// 引数チェック
	if (format == NULL)
	{
		return;
	}

	tmp[0] = 0;

	va_start(args, format);
	vsnprintf(tmp, sizeof(tmp), format, args);
	va_end(args);

	chelp_print(tmp);
}
void chelp_snprintf(char *dst, UINT size, char *format, ...)
{
	va_list args;
	// 引数チェック
	if (format == NULL)
	{
		return;
	}

	va_start(args, format);
	vsnprintf(dst, size, format, args);
	va_end(args);
}
void chelp_sprintf(char *dst, char *format, ...)
{
	va_list args;
	// 引数チェック
	if (format == NULL)
	{
		return;
	}

	va_start(args, format);
	vsnprintf(dst, 4096, format, args);
	va_end(args);
}

// 文字列を画面に表示する
void chelp_print(char *str)
{
	printf ("%s", str);
}

// 64 bit を 32 bit で割って mod をとる (結果 32 bit)
// (a % b) = (a - (a / b) * b)
UINT chelp_mod_64_32_32(UINT64 a, UINT b)
{
#ifndef	CRYPT_32BIT
	return a % (UINT64)b;
#else	// CRYPT_32BIT
	UINT t = chelp_div_64_32_32(a, b);
	UINT64 x = chelp_mul_64_64_64((UINT64)t, (UINT64)b);
	return a - x;
#endif	// CRYPT_32BIT
}

// 64 bit を 32 bit で割る (結果 32 bit)
UINT chelp_div_64_32_32(UINT64 a, UINT b)
{
	return (UINT)(chelp_div_64_32_64(a, b));
}

// 64 bit を 32 bit で割る (結果 64 bit)
UINT64 chelp_div_64_32_64(UINT64 a, UINT b)
{
#ifndef	CRYPT_32BIT
	return a / (UINT64)b;
#else	// CRYPT_32BIT
	UINT64 ret[2];

	ret[0] = a;
	ret[1] = 0;

	mpudiv_128_32(ret, b, ret);

	return (UINT64)(ret[0]);
#endif	// CRYPT_32BIT
}

// 64 bit 同士の掛け算 (結果 64 bit)
UINT64 chelp_mul_64_64_64(UINT64 a, UINT64 b)
{
#ifndef	CRYPT_32BIT
	return a * b;
#else	// CRYPT_32BIT
	UINT64 ret[2];

	mpumul_64_64(a, b, ret);

	return ret[0];
#endif	// CRYPT_32BIT
}


int
BIO_printf (BIO *bio, const char *format, ...)
{
	va_list ap;
	int r;

	va_start (ap, format);
	r = vprintf (format, ap);
	va_end (ap);
	return r;
}

int
BIO_snprintf (char *str, size_t size, const char *format, ...)
{
	va_list ap;
	int r;

	va_start (ap, format);
	r = vsnprintf (str, size, format, ap);
	va_end (ap);
	return r;
}

void
chelp_abort (void)
{
	panic ("chelp abort!");
}

int
RAND_poll (void)
{
	RAND_add ("TEST", 4, 4);
	return 0;
}
