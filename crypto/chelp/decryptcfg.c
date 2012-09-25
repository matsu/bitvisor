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

#define	CHELP_OPENSSL_SOURCE
#include "chelp.h"
#include <openssl/aes.h>
#include <openssl/evp.h>
#include "decryptcfg.h"

#define SALTLEN 8		/* salt size */
#define ITER 1024		/* iteration */
#define KEYLEN 32		/* 256 bit */
#define IVLEN 16		/* AES block size */

void
decryptcfg (unsigned char *pass, unsigned int passlen, unsigned char *data,
	    unsigned int datalen, void *out)
{
	unsigned char keyiv[KEYLEN + IVLEN];
	AES_KEY key;

	PKCS5_PBKDF2_HMAC_SHA1 ((char *)pass, passlen, data, SALTLEN, ITER,
				KEYLEN + IVLEN, keyiv);
	AES_set_decrypt_key (keyiv, KEYLEN * 8, &key);
	AES_cbc_encrypt (data + SALTLEN, out, datalen - SALTLEN,
			 &key, keyiv + KEYLEN, 0);
}
