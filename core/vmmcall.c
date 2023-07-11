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

/* process VMM calls (hypervisor calls) */

#include <arch/gmm.h>
#include <arch/vmmcall.h>
#include <core/initfunc.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include "vmmcall.h"

#define VMMCALL_MAX 128
#define VMMCALL_NAME_MAXLEN 256

static int n_vmmcall;
static struct {
	char *name;
	vmmcall_func_t func;
} vmmcall_data[VMMCALL_MAX];

/* get a number for VMM call */
/* INPUT: arg1=virtual address of a name of a VMM call (in 256 bytes) */
/* OUTPUT: ret=a number for the VMM call (0 on error) */
static void
get_vmmcall_number (void)
{
	u32 i;
	char buf[VMMCALL_NAME_MAXLEN];
	ulong nameaddr;

	vmmcall_arch_read_arg (1, &nameaddr);
	for (i = 0; i < VMMCALL_NAME_MAXLEN; i++) {
		if (gmm_arch_readlinear_b (nameaddr + i, &buf[i])
		    != GMM_ACCESS_OK)
			break;
		if (buf[i] == '\0')
			goto copy_ok;
	}
	vmmcall_arch_write_ret (0);
	return;
copy_ok:
	for (i = 0; i < n_vmmcall; i++) {
		if (strcmp (vmmcall_data[i].name, buf) == 0)
			goto found;
	}
	vmmcall_arch_write_ret (0);
	return;
found:
	vmmcall_arch_write_ret (i);
	return;
}

/* handling a VMM call instruction */
void
vmmcall (void)
{
	ulong cmd;

	vmmcall_arch_read_arg (0, &cmd);
	if (cmd < VMMCALL_MAX && vmmcall_data[cmd].func)
		vmmcall_data[cmd].func ();
	else
		vmmcall_arch_write_ret (0);
}

/* register a VMM call */
void
vmmcall_register (char *name, vmmcall_func_t func)
{
	if (n_vmmcall >= VMMCALL_MAX)
		panic ("Too many vmmcall_register.");
	vmmcall_data[n_vmmcall].name = name;
	vmmcall_data[n_vmmcall].func = func;
	n_vmmcall++;
}

void
vmmcall_init (void)
{
	n_vmmcall = 0;
	vmmcall_register ("get_vmmcall_number", get_vmmcall_number);
	call_initfunc ("vmmcal");
}

INITFUNC ("paral10", vmmcall_init);
