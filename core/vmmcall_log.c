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

#ifdef LOG_TO_GUEST
#include <builtin.h>
#include <arch/gmm.h>
#include <arch/vmmcall.h>
#include <core/initfunc.h>
#include <core/mm.h>
#include "putchar.h"
#include "vmmcall.h"

static putchar_func_t old;
static u8 *buf;
static ulong bufsize, offset;

static void
log_putchar (unsigned char c)
{
	if (buf) {
		buf[offset++] = c;
		if (offset >= bufsize)
			offset = 4;
		atomic_fetch_and32 ((u32 *)(void *)buf, 1);
	}
	old (c);
}

static void
log_set_buf (void)
{
	ulong physaddr;

	if (vmmcall_arch_caller_user ())
		return;
	if (buf != NULL) {
		unmapmem (buf, bufsize);
		buf = NULL;
	}
	vmmcall_arch_read_arg (1, &physaddr);
	vmmcall_arch_read_arg (2, &bufsize);
	if (physaddr == 0 || bufsize == 0)
		return;
	offset = 4;
	buf = mapmem_as (gmm_arch_current_as (), physaddr, bufsize,
			 MAPMEM_WRITE);
	if (old == NULL)
		putchar_set_func (log_putchar, &old);
}

static void
vmmcall_log_init (void)
{
	old = NULL;
	buf = NULL;
	vmmcall_register ("log_set_buf", log_set_buf);
}

INITFUNC ("vmmcal0", vmmcall_log_init);
#endif
