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

#ifndef __CORE_SPINLOCK_H
#define __CORE_SPINLOCK_H

#ifdef SPINLOCK_DEBUG
#include <core/panic.h>
#endif
#include <core/types.h>

typedef u8 spinlock_t;

#ifdef SPINLOCK_DEBUG
#define spinlock_lock _spinlock_lock
#endif

static inline void
spinlock_lock (spinlock_t *l)
{
	u8 dummy;

	asm volatile ("3: \n"
		      " xchg  %1, %0 \n" /* exchange %0 and *l */
		      " test  %0, %0 \n" /* check whether %0 is 0 */
		      " je    1f \n" /* if 0, succeeded */
		      "2: \n"
		      " pause \n" /* spin loop hint */
		      " cmp   %1, %0 \n" /* compare %0 and *l */
		      " je    2b \n" /* if not 0, do spin loop */
		      " jmp   3b \n" /* if 0, try again */
		      "1: \n"
#ifdef __x86_64__
		      : "=r" (dummy)
#else
		      : "=abcd" (dummy)
#endif
		      , "=m" (*l)
		      : "0" ((u8)1)
		      : "cc");
}

#ifdef SPINLOCK_DEBUG
#undef spinlock_lock
#define spinlock_debug1(a) spinlock_debug2(a)
#define spinlock_debug2(a) #a
#define spinlock_lock(l) spinlock_lock_debug (l, \
	"spinlock_lock failed." \
	" file " __FILE__ \
	" line " spinlock_debug1 (__LINE__))

static inline void
spinlock_lock_debug (spinlock_t *l, char *msg)
{
	u8 dummy;
	u32 c;

	asm volatile ("3: \n"
		      " xchg  %1, %0 \n" /* exchange %0 and *l */
		      " test  %0, %0 \n" /* check whether %0 is 0 */
		      " je    1f \n" /* if 0, succeeded */
		      "2: \n"
		      " dec   %2 \n"
		      " je    1f \n"
		      " pause \n" /* spin loop hint */
		      " cmp   %1, %0 \n" /* compare %0 and *l */
		      " je    2b \n" /* if not 0, do spin loop */
		      " jmp   3b \n" /* if 0, try again */
		      "1: \n"
#ifdef __x86_64__
		      : "=r" (dummy)
#else
		      : "=abd" (dummy)
#endif
		      , "=m" (*l)
		      , "=c" (c)
		      : "0" ((u8)1)
		      , "2" (0xFFFFFFFF)
		      : "cc");
	if (!c)
		panic (msg);
}
#endif

static inline void
spinlock_unlock (spinlock_t *l)
{
	u8 dummy;

	asm volatile ("xchg %1, %0 \n"
#ifdef __x86_64__
		      : "=r" (dummy)
#else
		      : "=abcd" (dummy)
#endif
		      , "=m" (*l)
		      : "0" ((u8)0));
}

static inline void
spinlock_init (spinlock_t *l)
{
	spinlock_unlock (l);
}

#endif
