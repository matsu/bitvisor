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

#include "current.h"
#include "gmm_access.h"
#include "initfunc.h"
#include "putchar.h"
#include "vmmcall.h"
#include "vmmcall_log.h"

static ulong offset, pagenum;

static void
log_putchar (unsigned char c)
{
	u32 n;

	write_phys_b ((pagenum << 12) + offset, c);
	offset++;
	if (offset >= 4096)
		offset = 4;
	read_phys_l ((pagenum << 12) + 0, &n);
	n++;
	write_phys_l ((pagenum << 12) + 0, n);
}

static void
log_set_page (void)
{
	putchar_exit_global ();
	current->vmctl.read_general_reg (GENERAL_REG_RBX, &pagenum);
	if (pagenum == 0xFFFFFFFF)
		return;
	offset = 4;
	putchar_init_global (log_putchar);
}

static void
vmmcall_log_init (void)
{
	vmmcall_register ("log_set_page", log_set_page);
}

INITFUNC ("vmmcal0", vmmcall_log_init);
