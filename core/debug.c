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

#include "debug.h"
#include "gmm_access.h"
#include "i386-stub.h"
#include "printf.h"
#include "process.h"
#include "serial.h"
#include "types.h"

static int memdump;

static int
memdump_msghandler (int m, int c, void *recvbuf, int recvlen, void *sendbuf,
		    int sendlen)
{
	u64 physaddr;
	u8 *q;
	int i;

	if (m != 1)
		return -1;
	if (recvlen < sizeof (u64))
		return -1;
	q = sendbuf;
	switch (c) {
	case 0:			/* guest-physical */
		physaddr = *(u64 *)recvbuf;
		for (i = 0; i < sendlen; i++)
			read_phys_b (physaddr + i, &q[i]);
		break;
	case 1:			/* hypervisor-virtual */
		for (i = 0; i < sendlen; i++)
			q[i] = (*(u8 **)recvbuf)[i];
		break;
	}
	return 0;
}

void
debug_gdb (void)
{
#ifdef DEBUG_GDB
	static bool f = false;

	if (!f) {
		f = true;
		printf ("gdb set_debug_traps\n");
		set_debug_traps ();
		printf ("gdb breakpoint\n");
		breakpoint ();
	}
#endif
}

void
debug_iohook (void)
{
#ifdef DEBUG_GDB
	serial_init_iohook ();
#endif
}

void
debug_msgregister (void)
{
	memdump = msgregister ("memdump", memdump_msghandler);
}

void
debug_msgunregister (void)
{
	msgunregister (memdump);
}
