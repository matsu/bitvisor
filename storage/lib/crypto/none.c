/*
 * Copyright (c) 2010 Igel Co., Ltd
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

static void
crypto_none_crypt (void *dst, void *src, int sector_size)
{
	if (dst != src)
		memcpy (dst, src, sector_size);
}

static void
crypto_none_encrypt (void *dst, void *src, void *keyctx, lba_t lba,
		     int sector_size)
{
	crypto_none_crypt (dst, src, sector_size);
}

static void
crypto_none_decrypt (void *dst, void *src, void *keyctx, lba_t lba,
		     int sector_size)
{
	crypto_none_crypt (dst, src, sector_size);
}

static void *
crypto_none_setkey (const u8 *key, int bits)
{
	return NULL;
}

static struct crypto crypto_none_crypto = {
	.name = 	"none",
	.block_size =	16,
	.keyctx_size =	0,
	.encrypt =	crypto_none_encrypt,
	.decrypt =	crypto_none_decrypt,
	.setkey =	crypto_none_setkey,
};

void
crypto_none_init (void)
{
	crypto_register (&crypto_none_crypto);
}
