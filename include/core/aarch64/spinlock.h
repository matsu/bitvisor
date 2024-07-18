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

#ifndef _INC_CORE_AARCH64_SPINLOCK_H
#define _INC_CORE_AARCH64_SPINLOCK_H

/*
 * === Note on WFE ===
 *
 * Upon store-release operation, writing causes the Exclusive Monitor mode to
 * be cleared. This implicitly generated a WFE wake-up event. There is no need
 * to call SEV explicitly. Note that threads that execute WFE instruction are
 * in Exclusive Minotor mode due to load-acquire operation. See Figure B2-5,
 * Section B2.10.6 and Section D1.17.1 in ARM DDI 0487A.f. for references.
 *
 */
static inline void
spinlock_arch_wait (void)
{
	asm volatile ("wfe" : : : "memory");
}

static inline void
spinlock_lock (spinlock_t *l)
{
	/* __atomic_test_and_set() returns true if *l were set */
	while (__atomic_test_and_set (l, __ATOMIC_ACQUIRE))
		spinlock_arch_wait ();
}

static inline void
spinlock_unlock (spinlock_t *l)
{
	__atomic_clear (l, __ATOMIC_RELEASE);
}

static inline void
rw_spinlock_lock_sh (rw_spinlock_t *l)
{
	while (__atomic_add_fetch (l, 1, __ATOMIC_ACQUIRE) & (1 << 31))
		spinlock_arch_wait ();
}

static inline void
rw_spinlock_unlock_sh (rw_spinlock_t *l)
{
	__atomic_sub_fetch (l, 1, __ATOMIC_RELEASE);
}

/* return  0 if lock is successful */
static inline rw_spinlock_t
rw_spinlock_trylock_ex (rw_spinlock_t *l)
{
	rw_spinlock_t expect = 0, desire = 1 << 31, strong = 0;
	__atomic_compare_exchange_n (l, &expect, desire, strong,
				     __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
	return expect; /* expect = *l if *l != 0 */
}

static inline void
rw_spinlock_lock_ex (rw_spinlock_t *l)
{
	while (rw_spinlock_trylock_ex (l))
		spinlock_arch_wait ();
}

static inline void
rw_spinlock_unlock_ex (rw_spinlock_t *l)
{
	__atomic_and_fetch (l, 0x7FFFFFFF, __ATOMIC_RELEASE);
}

static inline void
ticketlock_lock (ticketlock_t *l)
{
	u32 ticket = __atomic_fetch_add (&l->next_ticket, 1, __ATOMIC_RELAXED);
	while (__atomic_load_n (&l->now_serving, __ATOMIC_ACQUIRE) != ticket)
		spinlock_arch_wait ();
}

static inline void
ticketlock_unlock (ticketlock_t *l)
{
	u32 next = __atomic_load_n (&l->now_serving, __ATOMIC_RELAXED) + 1;
	__atomic_store_n (&l->now_serving, next, __ATOMIC_RELEASE);
}

#endif
