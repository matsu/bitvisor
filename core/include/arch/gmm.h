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

#ifndef _CORE_INCLUDE_ARCH_GMM_H
#define _CORE_INCLUDE_ARCH_GMM_H

#include <core/types.h>

/*
 * For guest memory access actual error code, see the actual implementation of
 * access functions. The actual implementation conventionally returns 0 when an
 * access is successful.
 */
#define GMM_ACCESS_OK 0

struct mm_as;

/*
 * NOTE gmm_arch_current_as() needs to be used carefully. This function returns
 * a pointer stored in 'current'. BitVisor currently runs only one virtual
 * machine so this always returns the same pointer (as_passvm). However, if we
 * are to add support for multiple virtual machines, it can return a different
 * pointer depending on the 'current'. It is ok to use in vmmcall callback like
 * core/vmmcall_log.c because vmmcall callback is synchronous. However, for
 * example, the function should not be used in timer callbacks because the
 * 'current' might be different.
 *
 * This function is likely to be removed once we have a better solution for
 * obtaining address speace information.
 */
const struct mm_as *gmm_arch_current_as (void);
int gmm_arch_readlinear_b (ulong linear, void *data);
int gmm_arch_writelinear_b (ulong linear, u8 data);

#endif
