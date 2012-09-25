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

int prohibit_iccard_init;

#ifdef IDMAN
#include <IDMan.h>
#include "assert.h"
#include "config.h"
#include "iccard.h"
#include "initfunc.h"
#include "panic.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "timer.h"

static bool idman_ready;
static spinlock_t idman_lock;
static unsigned long idman_session;

static void
idman_init_session (void)
{
	int r;

	spinlock_lock (&idman_lock);
	if (!idman_ready) {
		if (prohibit_iccard_init)
			goto ret;
		r = IDMan_IPInitializeReader ();
		if (r)
			goto ret; /* card reader or uhci not ready */
		r = IDMan_IPInitialize (config.idman.pin,  &idman_session);
		if (r) {
			/* panic ("IC card error"); */
			printf ("IC card error\n");
			r = IDMan_IPFinalize (idman_session);
			r = IDMan_IPFinalizeReader ();
		} else {
			idman_ready = true;
		}
	}
ret:
	spinlock_unlock (&idman_lock);
}

static void
idman_finish_session (void)
{
	int r;

	spinlock_lock (&idman_lock);
	if (idman_ready) {
		r = IDMan_IPFinalize (idman_session);
		if (r)
			printf ("IDMan_IPFinalize failed. ignored.\n");
		r = IDMan_IPFinalizeReader ();
		if (r)
			printf ("IDMan_IPFinalizeReader failed. ignored.\n");
		idman_ready = false;
	}
	spinlock_unlock (&idman_lock);
}

void
idman_reinit_session (void)
{
	idman_finish_session ();
	idman_init_session ();
}

void
idman_reinit2 (void)
{
	IDMan_IPFinalize (idman_session);
	IDMan_IPInitialize (config.idman.pin,  &idman_session);
}

bool
get_idman_session (unsigned long *session)
{
	if (!idman_ready)
		idman_init_session ();
	if (idman_ready) {
		*session = idman_session;
		return true;
	}
	return false;
}

static void
iccard_init_timer (void *handle, void *data)
{
	if (!config.vmm.iccard.enable)
		return;
	idman_init_session ();
	if (!idman_ready)
		timer_set (handle, 1000000);
}

static void
idman_init (void)
{
	void *handle;

	if (!config.vmm.iccard.enable)
		return;
	handle = timer_new (iccard_init_timer, NULL);
	ASSERT (handle);
	timer_set (handle, 1000000);
}

static void
idman_init_global (void)
{
	spinlock_init (&idman_lock);
	idman_ready = false;
}

INITFUNC ("paral01", idman_init_global);
INITFUNC ("driver4", idman_init);
#endif
