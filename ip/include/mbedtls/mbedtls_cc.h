/*
 * Copyright (c) 2023 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

#ifndef __IP_INCLUDE_MBEDTLS_MBEDTLS_CC_H
#define __IP_INCLUDE_MBEDTLS_MBEDTLS_CC_H

#include <core/printf.h> /* for snprintf */
#include <core/random.h>
#include <limits.h>

#define CHAR_BIT	8
#define UINT32_MAX	0xFFFFFFFFU
#define SIZE_MAX	0xFFFFFFFFU
#define INT_MAX		0x7FFFFFFF
#define NULL		((void *)0)
#define RAND_RETRY	20

typedef unsigned long int uintptr_t;

char *strstr (const char *str, const char *sub);

static inline unsigned int
rand (void)
{
	unsigned int num, success;

	success = random_num_hw (RAND_RETRY, &num);
	if (!success)
		num = random_num_sw ();

	return num;
}

#endif /* __IP_INCLUDE_MBEDTLS_MBEDTLS_CC_H */
