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

#include <core/spinlock.h>
#include "current.h"
#include "initfunc.h"
#include "list.h"
#include "mm.h"

struct mmioclr {
	LIST1_DEFINE (struct mmioclr);
	void *data;
	bool (*callback) (void *data, phys_t s, phys_t e);
};

static LIST1_DEFINE_HEAD (struct mmioclr, mmioclr_list);
static rw_spinlock_t lock;

struct mmioclr *
mmioclr_register (void *data,
		  bool (*callback) (void *data, phys_t s, phys_t e))
{
	struct mmioclr *p = alloc (sizeof *p);
	p->data = data;
	p->callback = callback;
	rw_spinlock_lock_ex (&lock);
	LIST1_ADD (mmioclr_list, p);
	rw_spinlock_unlock_ex (&lock);
	return p;
}

void
mmioclr_unregister (struct mmioclr *p)
{
	rw_spinlock_lock_ex (&lock);
	LIST1_DEL (mmioclr_list, p);
	rw_spinlock_unlock_ex (&lock);
}

bool
mmioclr_clear_hmap (phys_t hpst, phys_t hpend)
{
	struct mmioclr *p;
	bool ret = false;
	rw_spinlock_lock_sh (&lock);
	LIST1_FOREACH (mmioclr_list, p) {
		if (p->callback (p->data, hpst, hpend)) {
			ret = true;
			break;
		}
	}
	rw_spinlock_unlock_sh (&lock);
	return ret;
}

bool
mmioclr_clear_gmap (phys_t gpst, phys_t gpend)
{
	phys_t hp0, hp1, hp2, gp2;

	hp0 = current->gmm.gp2hp (gpst, NULL);
	gp2 = (gpst | PAGESIZE_MASK) + 1;
	hp1 = hp0 | PAGESIZE_MASK;
	while (gp2 <= gpend) {
		hp2 = current->gmm.gp2hp (gp2, NULL);
		if (hp1 + 1 != hp2) {
			if (mmioclr_clear_hmap (hp0, hp1))
				return true;
			hp0 = hp2;
		}
		hp1 = hp2 | PAGESIZE_MASK;
		gp2 = (gp2 | PAGESIZE_MASK) + 1;
	}
	if (mmioclr_clear_hmap (hp0, hp1))
		return true;
	return false;
}

static void
mmioclr_init (void)
{
	rw_spinlock_init (&lock);
	LIST1_HEAD_INIT (mmioclr_list);
}

INITFUNC ("paral00", mmioclr_init);
