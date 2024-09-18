/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2022 Igel Co., Ltd
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

#include <core/initfunc.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/process.h>
#include "../dt.h"
#include "../initfunc.h"
#include "../uefi.h"
#include "asm.h"
#include "exception.h"
#include "pcpu.h"
#include "vm.h"

enum vmm_boot_mode {
	VMM_BOOT_UEFI,
};

u64 vmm_mair_host;
u64 vmm_tcr_host;
u64 vmm_sctlr_host;

static void
print_boot_msg (void)
{
	printf ("Starting BitVisor...\n");
	printf ("Copyright (c) 2007, 2008 University of Tsukuba\n");
	printf ("Copyright (c) 2022 IGEL Co.,Ltd.\n");
	printf ("All rights reserved.\n");
}

static void
call_parallel (void)
{
	static struct {
		char *name;
		ulong not_called;
	} paral[] = { { "paral0", 1 },
		      { "paral1", 1 },
		      { "paral2", 1 },
		      { "paral3", 1 },
		      { NULL, 0 } };
	int i;

	for (i = 0; paral[i].name; i++) {
		ulong *nc = &paral[i].not_called;
		if (__atomic_exchange_n (nc, 0, __ATOMIC_ACQ_REL))
			call_initfunc (paral[i].name);
	}
}

static void
vmm_main (void)
{
	int d;

	initfunc_init ();
	call_initfunc ("global");
#ifdef DEVICETREE
	dt_init ();
#endif
	call_initfunc ("bsp");
	call_parallel ();
	call_initfunc ("pcpu");

	call_initfunc ("config0");
	printf ("Loading drivers...\n");
	call_initfunc ("driver");
	call_initfunc ("config1");

	printf ("Initialization done\n");

	d = newprocess ("init");
	msgsendint (d, 0);
	msgclose (d);

	printf ("Starting a virtual machine...\n");
	vm_start ();
	panic ("vm_start() returns\n");
}

void
vmm_entry (enum vmm_boot_mode boot_mode)
{
	switch (boot_mode) {
	case VMM_BOOT_UEFI:
		uefi_booted = true;
		break;
	default:
		break;
	}

	/* Save this for secondary cores start */
	vmm_mair_host = mrs (MAIR_EL2);
	vmm_tcr_host = mrs (TCR_EL2);
	vmm_sctlr_host = mrs (SCTLR_EL2);

	exception_init ();
	pcpu_early_init ();

	vmm_main ();
}

void
vmm_entry_cpu_on (struct vm_ctx *vm, u64 g_mpidr, u64 g_entry, u64 g_ctx_id)
{
	exception_secondary_init ();
	pcpu_secondary_init ();

	call_initfunc ("ap");
	call_parallel ();
	call_initfunc ("pcpu");

	vm_start_at (vm, g_mpidr, g_entry, g_ctx_id);
	panic ("vm_start_at() returns");
}

INITFUNC ("global1", print_boot_msg);
