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

COMP_CTX *COMP_CTX_new(COMP_METHOD *meth)
	{
	COMP_CTX *ret;

	if ((ret=(COMP_CTX *)OPENSSL_malloc(sizeof(COMP_CTX))) == NULL)
		{
		/* ZZZZZZZZZZZZZZZZ */
		return(NULL);
		}
	memset(ret,0,sizeof(COMP_CTX));
	ret->meth=meth;
	if ((ret->meth->init != NULL) && !ret->meth->init(ret))
		{
		OPENSSL_free(ret);
		ret=NULL;
		}
	return(ret);
	}

void COMP_CTX_free(COMP_CTX *ctx)
	{
	if(ctx == NULL)
	    return;

	if (ctx->meth->finish != NULL)
		ctx->meth->finish(ctx);

	OPENSSL_free(ctx);
	}

int COMP_compress_block(COMP_CTX *ctx, unsigned char *out, int olen,
	     unsigned char *in, int ilen)
	{
	int ret;
	if (ctx->meth->compress == NULL)
		{
		/* ZZZZZZZZZZZZZZZZZ */
		return(-1);
		}
	ret=ctx->meth->compress(ctx,out,olen,in,ilen);
	if (ret > 0)
		{
		ctx->compress_in+=ilen;
		ctx->compress_out+=ret;
		}
	return(ret);
	}

int COMP_expand_block(COMP_CTX *ctx, unsigned char *out, int olen,
	     unsigned char *in, int ilen)
	{
	int ret;

	if (ctx->meth->expand == NULL)
		{
		/* ZZZZZZZZZZZZZZZZZ */
		return(-1);
		}
	ret=ctx->meth->expand(ctx,out,olen,in,ilen);
	if (ret > 0)
		{
		ctx->expand_in+=ilen;
		ctx->expand_out+=ret;
		}
	return(ret);
	}
