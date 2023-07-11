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

#include <arch/gmm.h>
#include <arch/vmmcall.h>
#include <core/mm.h>
#include <core/spinlock.h>
#include <core/string.h>
#include "config.h"
#include "initfunc.h"
#include "list.h"
#include "vmmcall.h"
#include "vmmcall_status.h"

struct status {
	LIST1_DEFINE (struct status);
	char *(*func) (void);
	char *ret;
};

static LIST1_DEFINE_HEAD (struct status, list1_status);
static spinlock_t status_lock;

void
register_status_callback (char *(*func) (void))
{
#ifdef VMMCALL_STATUS_ENABLE
	struct status *s;

	s = alloc (sizeof *s);
	s->func = func;
	LIST1_ADD (list1_status, s);
#endif
}

/*
  arg1=linear address of a buffer
  arg2=size of the buffer
 */
static void
get_status (void)
{
	struct status *s;
	uint len = 0;
	ulong arg1, arg2;
	char c;

	if (!config.vmm.status)
		return;
	spinlock_lock (&status_lock);
	vmmcall_arch_read_arg (1, &arg1);
	vmmcall_arch_read_arg (2, &arg2);
	LIST1_FOREACH (list1_status, s) {
		s->ret = s->func ();
		len += strlen (s->ret);
	}
	vmmcall_arch_write_arg (2, len);
	if (len <= arg2) {
		LIST1_FOREACH (list1_status, s) {
			while ((c = *s->ret++) != '\0')
				if (gmm_arch_writelinear_b (arg1++, c)
				    != GMM_ACCESS_OK)
					goto err;
		}
		vmmcall_arch_write_ret (0);
	} else {
	err:
		vmmcall_arch_write_ret (1);
	}
	spinlock_unlock (&status_lock);
}

static void
vmmcall_status_init_global (void)
{
	LIST1_HEAD_INIT (list1_status);
}

static void
vmmcall_status_init (void)
{
	spinlock_init (&status_lock);
#ifdef VMMCALL_STATUS_ENABLE
	vmmcall_register ("get_status", get_status);
#else
	if (0)
		get_status ();	/* supress warnings */
#endif
}

INITFUNC ("global3", vmmcall_status_init_global);
INITFUNC ("vmmcal0", vmmcall_status_init);
