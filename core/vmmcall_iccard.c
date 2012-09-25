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

#ifdef CARDSTATUS
#include <IDMan.h>
#include "assert.h"
#include "config.h"
#include "current.h"
#include "iccard.h"
#include "initfunc.h"
#include "panic.h"
#include "printf.h"
#include "spinlock.h"
#include "timer.h"
#include "vmmcall.h"

static enum {
	IS_NOT_READY,
	IS_OK,
	IS_NOCARD,
} iccard_status, guest_status;
static int shutdowntime;
static rw_spinlock_t cardtest;
int ps2_locked = 0;

static void
iccard (void)
{
	if (iccard_status == IS_NOCARD && shutdowntime >= 3) {
		guest_status = IS_NOCARD;
		current->vmctl.write_general_reg (GENERAL_REG_RAX, 1);
	} else {
		guest_status = IS_OK;
		current->vmctl.write_general_reg (GENERAL_REG_RAX, 0);
	}
}

static void
iccard_timer (void *handle, void *data)
{
	int r;
	unsigned long idman_session;

	if (!config.vmm.iccard.status)
		return;
	if (!get_idman_session (&idman_session)) {
		timer_set (handle, 1000000);
		return;
	}
	if (iccard_status == IS_NOT_READY) {
		iccard_status = IS_OK;
		printf ("Starting IC card status monitor\n");
	}
	if (iccard_status == IS_OK) {
		r = IDMan_CheckCardStatus (idman_session);
		if (r) {
			shutdowntime = 0;
			iccard_status = IS_NOCARD;
			printf ("No card\n");
			/* printf ("Stopping IC card status monitor\n"); */
		}
	}
	if (iccard_status == IS_NOCARD) {
		shutdowntime++;
		idman_reinit2 ();
		get_idman_session (&idman_session);
		r = IDMan_CheckCardStatus (idman_session);
		if (r) {
			if (shutdowntime == 6) {
				printf ("Wait for card ready\n");
				if (guest_status == IS_OK)
					panic ("helper program"
					       " not responding");
				else
					ps2_locked = 1;
			}
		}
		if (!r) {
			ps2_locked = 0;
			iccard_status = IS_OK;
			printf ("Card is OK now\n");
		}
#if 0
		if (shutdowntime > 10)
			panic ("no response from guest helper program");
		if (shutdowntime > 60)
			panic ("shutdown time out");
#endif
	}
	timer_set (handle, 1000000);
}

static void
vmmcall_iccard_init (void)
{
	void *handle;

	if (!config.vmm.iccard.status)
		return;
	rw_spinlock_init (&cardtest);
	iccard_status = IS_NOT_READY;
	vmmcall_register ("iccard", iccard);
	handle = timer_new (iccard_timer, NULL);
	ASSERT (handle);
	timer_set (handle, 1000000);
}

INITFUNC ("driver5", vmmcall_iccard_init);
#endif
