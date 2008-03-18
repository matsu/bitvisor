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
		      " xchg  %1, %0 \n" /* %0 と *l を交換する */
		      " test  %0, %0 \n" /* %0 が 0 かどうか調べる */
		      " je    1f \n" /* 0 だったらロック獲得成功 */
		      "2: \n"
		      " pause \n" /* spin loop hint */
		      " cmp   %1, %0 \n" /* %0 と *l を比較する */
		      " je    2b \n" /* 0 でなかったら spin loop */
		      " jmp   3b \n" /* 0 だったらもう一度ロック獲得を試みる */
		      "1: \n"
		      : "=r" (dummy)
		      , "=m" (*l)
		      : "0" (1)
		      : "cc");
}

static inline void
spinlock_unlock (spinlock_t *l)
{
	char dummy;

	asm volatile ("xchg %1, %0 \n"
		      : "=r" (dummy)
		      , "=m" (*l)
		      : "0" (0));
}

static inline void
spinlock_init (spinlock_t *l)
{
	spinlock_unlock (l);
}
