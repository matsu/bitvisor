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

#include <core.h>
#include "crypto.h"

#define AES_BLK_BYTES   16

#ifdef AES_GLADMAN
#define COPYRIGHT "Copyright (c) 1998-2008, Brian Gladman, Worcester, UK. All rights reserved."
#define AES_VERSION "gladman"
#include "aes-gladman/aes.h"
#define AES_ENC_KEYTYPE		aes_encrypt_ctx
#define AES_ENC_SETKEY		aes_encrypt_key
#define AES_ENC_FUNC		aes_encrypt
#define AES_DEC_KEYTYPE		aes_decrypt_ctx
#define AES_DEC_SETKEY		aes_decrypt_key
#define AES_DEC_FUNC		aes_decrypt
#else
#define COPYRIGHT "Copyright (c) 1998-2002 The OpenSSL Project.  All rights reserved."
#define AES_VERSION "openssl"
#include <openssl/aes.h>
#define AES_ENC_KEYTYPE		AES_KEY
#define AES_ENC_SETKEY		AES_set_encrypt_key
#define AES_ENC_FUNC		AES_encrypt
#define AES_DEC_KEYTYPE		AES_KEY
#define AES_DEC_SETKEY		AES_set_decrypt_key
#define AES_DEC_FUNC		AES_decrypt
#endif

struct aes_xts_encrypt_keys {
	AES_ENC_KEYTYPE		tweak_key;
	AES_ENC_KEYTYPE		encrypt_key;
};
struct aes_xts_decrypt_keys {
	AES_ENC_KEYTYPE		tweak_key;
	AES_DEC_KEYTYPE		decrypt_key;
};
struct aes_xts_keyctx {
	struct aes_xts_encrypt_keys	encrypt;
	struct aes_xts_decrypt_keys	decrypt;
};
typedef void (*aes_crypt_func_t)(const u8 *in, u8 *out, void *key);

static void inline xor128(void *dst, const void *src1, const void *src2)
{
	core_mem_t *s1 = (core_mem_t *)src1, *s2 = (core_mem_t *)src2;
	core_mem_t *d = (core_mem_t *)dst;

	d[0].qword = s1[0].qword ^ s2[0].qword;
	d[1].qword = s1[1].qword ^ s2[1].qword;
}

static void inline movzx128(void *dst, const u64 src)
{
	core_mem_t *d = (core_mem_t *)dst;

	d[0].qword = src;
	d[1].qword = 0;
}

static int shl128(void *dst, const void *src)
{
	int c0, c1;
	core_mem_t *s = (core_mem_t *)src;
	core_mem_t *d = (core_mem_t *)dst;

	c0 = s[0].qword & (1ULL << 63) ? 1 : 0;
	c1 = s[1].qword & (1ULL << 63) ? 1 : 0;
	d[0].qword = s[0].qword << 1;
	d[1].qword = s[1].qword << 1 | c0;
	return c1;
}

static void gf_mul128(u8 *dst, const u8 *src)
{
	int carry;
	static const u8 gf_128_fdbk = 0x87;

	carry = shl128(dst, src);
	if (carry)
		dst[0] ^= gf_128_fdbk;
}

static void aes_xts_crypt(u8 *dst, u8 *src, aes_crypt_func_t crypt, void *tk, void *ck, u64 lba, u32 sector_size)
{
	int i;
	u8 tweak[AES_BLK_BYTES];

	ASSERT(sector_size % AES_BLK_BYTES == 0);
	movzx128(tweak, lba);				// convert sector number to tweak plaintext
	AES_ENC_FUNC(tweak, tweak, tk);			// encrypt the tweak
	for(i = sector_size; i > 0; i -= AES_BLK_BYTES) {
		xor128(dst, src, tweak);		// merge the tweak into the input block
		crypt(dst, dst, ck);			// encrypt one block
		xor128(dst, dst, tweak);
		gf_mul128(tweak, tweak);
		dst += AES_BLK_BYTES; src += AES_BLK_BYTES;
	}
}

static void aes_xts_encrypt(void *dst, void *src, void *keyctx, lba_t lba, int sector_size)
{
	struct aes_xts_keyctx *k = keyctx;

	aes_xts_crypt(dst, src, (aes_crypt_func_t)AES_ENC_FUNC, &k->encrypt.tweak_key, &k->encrypt.encrypt_key, lba, sector_size);
}

static void aes_xts_decrypt(void *dst, void *src, void *keyctx, lba_t lba, int sector_size)
{
	struct aes_xts_keyctx *k = keyctx;

	aes_xts_crypt(dst, src, (aes_crypt_func_t)AES_DEC_FUNC, &k->decrypt.tweak_key, &k->decrypt.decrypt_key, lba, sector_size);
}

static void *aes_xts_setkey(const u8 *key, int bits)
{
	int keybit = bits / 2;
	int keylen = keybit / 8;
	struct aes_xts_keyctx *keyctx = alloc(sizeof(struct aes_xts_keyctx));

	AES_ENC_SETKEY(key + keylen, keybit, &keyctx->encrypt.tweak_key);
	AES_ENC_SETKEY(key         , keybit, &keyctx->encrypt.encrypt_key);
	AES_ENC_SETKEY(key + keylen, keybit, &keyctx->decrypt.tweak_key);
	AES_DEC_SETKEY(key         , keybit, &keyctx->decrypt.decrypt_key);
	return keyctx;
}

static struct crypto aes_xts_crypto = {
	.name = 	"aes-xts",
	.block_size =	AES_BLK_BYTES,
	.keyctx_size =	sizeof(struct aes_xts_keyctx),
	.encrypt =	aes_xts_encrypt,
	.decrypt =	aes_xts_decrypt,
	.setkey =	aes_xts_setkey,
};

void
aes_xts_init (void)
{
	printf("AES/AES-XTS Encryption Engine initialized (AES=%s)\n", AES_VERSION);
	printf(COPYRIGHT "\n");
	crypto_register(&aes_xts_crypto);
}
