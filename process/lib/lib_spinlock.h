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

typedef char spinlock_t;

static inline void
spinlock_lock (spinlock_t *l)
{
	char dummy;

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
		      , "+m" (*l)
		      : "0" ((char)1)
		      : "cc");
}

static inline void
spinlock_unlock (spinlock_t *l)
{
	char dummy;

	asm volatile ("xchg %1, %0 \n"
#ifdef __x86_64__
		      : "=r" (dummy)
#else
		      : "=abcd" (dummy)
#endif
		      , "+m" (*l)
		      : "0" ((char)0));
}

static inline void
spinlock_init (spinlock_t *l)
{
	spinlock_unlock (l);
}
