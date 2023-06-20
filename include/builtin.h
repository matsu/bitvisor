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

#ifndef _BUILTIN_H
#define _BUILTIN_H

#include <common/types.h>

static inline u32
atomic_fetch_add32 (u32 *ptr, u32 val)
{
	return __atomic_fetch_add (ptr, val, __ATOMIC_ACQ_REL);
}

static inline u32
atomic_xchg32 (u32 *ptr, u32 val)
{
	return __atomic_exchange_n (ptr, val, __ATOMIC_ACQ_REL);
}

/*
 * We currently have no use case of weak cmpxchg. If we have the weak use case
 * in the future, we need to add 'weak' parameter in the future.
 *
 * atomic_cmpxchg*() family operations:
 *
 * if (*ptr == *expected) {
 * 	*ptr = desired;
 * 	return true;
 * } else {
 * 	*expected = *ptr;
 * 	return false;
 * }
 *
 */
static inline bool
atomic_cmpxchg32 (u32 *ptr, u32 *expected, u32 desired)
{
	return __atomic_compare_exchange_n (ptr, expected, desired, false,
					    __ATOMIC_ACQ_REL,
					    __ATOMIC_RELAXED);
}

static inline bool
atomic_cmpxchg64 (u64 *ptr, u64 *expected, u64 desired)
{
	return __atomic_compare_exchange_n (ptr, expected, desired, false,
					    __ATOMIC_ACQ_REL,
					    __ATOMIC_RELAXED);
}

#endif
