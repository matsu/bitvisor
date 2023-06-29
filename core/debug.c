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

#include <arch/debug.h>
#include "debug.h"
#include "linkage.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "process.h"
#include "spinlock.h"
#include "string.h"
#include "types.h"

static int memdump, memfree;
static spinlock_t debug_msglock = SPINLOCK_INITIALIZER;
static int debug_msglock_count;

enum memdump_type {
	MEMDUMP_GPHYS,
	MEMDUMP_HVIRT,
	MEMDUMP_GVIRT,
	MEMDUMP_HPHYS,
};

struct memdump_gphys_data {
	u64 physaddr;
	u8 *q;
	int sendlen;
};

struct memdump_hvirt_data {
	u8 *p, *q;
	int sendlen;
};

struct memdump_gvirt_data {
	struct debug_arch_memdump_data *d;
	u8 *q;
	int sendlen, errlen;
	char *errbuf;
};

static asmlinkage void
memdump_gphys (void *data)
{
	struct memdump_gphys_data *d;
	int i;

	d = data;
	for (i = 0; i < d->sendlen; i++)
		debug_arch_read_gphys_mem (d->physaddr + i, &d->q[i]);
}

static asmlinkage void
memdump_hvirt (void *data)
{
	struct memdump_hvirt_data *d;

	d = data;
	memcpy (d->q, d->p, d->sendlen);
}

static asmlinkage void
memdump_gvirt (void *data)
{
	struct memdump_gvirt_data *dd;

	dd = data;
	debug_arch_read_gvirt_mem (dd->d, dd->q, dd->sendlen, dd->errbuf,
				   dd->errlen);
}

static int
memdump_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	u8 *q, *tmp;
	struct debug_arch_memdump_data *d;
	void *recvbuf, *sendbuf;
	int recvlen, sendlen, errlen;
	char *errbuf;
	int num = -1;
	struct memdump_gphys_data gphys_data;
	struct memdump_gvirt_data gvirt_data;
	struct memdump_hvirt_data hvirt_data;
	u64 physaddr;

	if (m != 1)
		return -1;
	if (bufcnt < 3)
		return -1;
	recvbuf = buf[0].base;
	recvlen = buf[0].len;
	sendbuf = buf[1].base;
	sendlen = buf[1].len;
	errbuf = buf[2].base;
	errlen = buf[2].len;
	if (recvlen < debug_arch_memdump_data_get_struct_size ())
		return -1;
	if (errlen > 0)
		errbuf[0] = '\0';
	d = (struct debug_arch_memdump_data *)recvbuf;
	q = sendbuf;
	switch ((enum memdump_type)c) {
	case MEMDUMP_GPHYS:
		physaddr = debug_arch_memdump_data_get_physaddr (d);
		gphys_data.physaddr = physaddr;
		gphys_data.q = q;
		gphys_data.sendlen = sendlen;
		num = debug_arch_callfunc_with_possible_int (memdump_gphys,
							     &gphys_data);
		break;
	case MEMDUMP_HVIRT:
		hvirt_data.p = (u8 *)debug_arch_memdump_data_get_virtaddr (d);
		hvirt_data.q = q;
		hvirt_data.sendlen = sendlen;
		num = debug_arch_callfunc_with_possible_int (memdump_hvirt,
							     &hvirt_data);
		break;
	case MEMDUMP_HPHYS:
		physaddr = debug_arch_memdump_data_get_physaddr (d);
		tmp = mapmem_hphys (physaddr, sendlen, MAPMEM_CANFAIL);
		if (tmp) {
			hvirt_data.p = tmp;
			hvirt_data.q = q;
			hvirt_data.sendlen = sendlen;
			num = debug_arch_callfunc_with_possible_int (
						   memdump_hvirt, &hvirt_data);
			unmapmem (tmp, sendlen);
		} else {
			snprintf (errbuf, errlen,
				  "mapmem_hphys failed (phys=0x%llX)",
				  physaddr);
		}
		break;
	case MEMDUMP_GVIRT:
		gvirt_data.d = d;
		gvirt_data.q = q;
		gvirt_data.sendlen = sendlen;
		gvirt_data.errbuf = errbuf;
		gvirt_data.errlen = errlen;
		num = debug_arch_callfunc_with_possible_int (memdump_gvirt,
							     &gvirt_data);
		break;
	default:
		return -1;
	}
	if (num != -1)
		snprintf (errbuf, errlen, "exception %d", num);
	return 0;
}

static int
memfree_msghandler (int m, int c)
{
	int n;

	if (m == 0) {
		n = num_of_available_pages ();
		printf ("%d pages (%d KiB) free\n", n, n * 4);
	}
	return 0;
}

void
debug_msgregister (void)
{
	int reg = 0;

	/* No need to hurry msgregister because these are used by
	 * debug process.  Call msgregister after unlock to avoid
	 * panic loop in the msgregister function. */

	spinlock_lock (&debug_msglock);
	if (!debug_msglock_count++)
		reg = 1;
	spinlock_unlock (&debug_msglock);
	if (reg) {
		memdump = msgregister ("memdump", memdump_msghandler);
		memfree = msgregister ("free", memfree_msghandler);
	}
}

void
debug_msgunregister (void)
{
	spinlock_lock (&debug_msglock);
	if (!--debug_msglock_count) {
		msgunregister (memdump);
		msgunregister (memfree);
	}
	spinlock_unlock (&debug_msglock);
}

void
debug_shell (int ttyin, int ttyout)
{
	int shell;

	debug_msgregister ();
	shell = newprocess ("shell");
	if (ttyin < 0 || ttyout < 0 || shell < 0)
		panic ("debug_shell start failed (%d,%d,%d)",
		       ttyin, ttyout, shell);
	msgsenddesc (shell, ttyin);
	msgsenddesc (shell, ttyout);
	msgsendint (shell, 0);
	msgclose (shell);
	debug_msgunregister ();
}
