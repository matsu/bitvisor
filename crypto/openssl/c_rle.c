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
#include <chelp.h>

#include <openssl/objects.h>
#include <openssl/comp.h>

static int rle_compress_block(COMP_CTX *ctx, unsigned char *out,
	unsigned int olen, unsigned char *in, unsigned int ilen);
static int rle_expand_block(COMP_CTX *ctx, unsigned char *out,
	unsigned int olen, unsigned char *in, unsigned int ilen);

static COMP_METHOD rle_method={
	NID_rle_compression,
	LN_rle_compression,
	NULL,
	NULL,
	rle_compress_block,
	rle_expand_block,
	NULL,
	NULL,
	};

COMP_METHOD *COMP_rle(void)
	{
	return(&rle_method);
	}

static int rle_compress_block(COMP_CTX *ctx, unsigned char *out,
	     unsigned int olen, unsigned char *in, unsigned int ilen)
	{
	/* int i; */

	if (olen < (ilen+1))
		{
		/* ZZZZZZZZZZZZZZZZZZZZZZ */
		return(-1);
		}

	*(out++)=0;
	memcpy(out,in,ilen);
	return(ilen+1);
	}

static int rle_expand_block(COMP_CTX *ctx, unsigned char *out,
	     unsigned int olen, unsigned char *in, unsigned int ilen)
	{
	int i;

	if (olen < (ilen-1))
		{
		/* ZZZZZZZZZZZZZZZZZZZZZZ */
		return(-1);
		}

	i= *(in++);
	if (i == 0)
		{
		memcpy(out,in,ilen-1);
		}
	return(ilen-1);
	}

