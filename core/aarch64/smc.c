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

#include <arch/vmm_mem.h>
#include <bits.h>
#include <core/mm.h>
#include <core/printf.h>
#include <core/thread.h>
#include "../sym.h"
#include "entry.h"
#include "exception.h"
#include "psci.h"
#include "smc.h"
#include "smc_asm.h"
#include "vm.h"

/*
 * We want this data to be 16-byte aligned. It is because it is data on the
 * stack. In addition, we can use ldp instruction in assembly easily.
 */
struct entry_data {
	struct vm_ctx *vm;
	u64 g_mpidr;
	phys_t g_entry;
	u64 g_ctx_id;
	u64 pa_base;
	u64 va_base;
};

static void
handle_psci_cpu_on (union exception_saved_regs *r)
{
	struct entry_data *g;
	u8 *stack;
	int error;

	/*
	 * x0: CPU_ON function ID
	 * x1: Affinity fields (MPIDR)
	 * x2: Entry point
	 * x3: Context ID
	 */

	/* Constuct necessary data for starting up core, stack */
	stack = alloc (VMM_STACKSIZE);

	g = (struct entry_data *)(stack + VMM_STACKSIZE - sizeof *g);
	g->vm = vm_get_current_ctx ();
	g->g_mpidr = r->reg.x1;
	g->g_entry = r->reg.x2;
	g->g_ctx_id = r->reg.x3;
	g->pa_base = vmm_mem_start_phys ();
	g->va_base = vmm_mem_start_virt ();

	/* Check for error from SMC call */
	error = smc_asm_psci_call (r->reg.x0, r->reg.x1,
				   sym_to_phys (entry_secondary),
				   (u64)(stack + VMM_STACKSIZE));

	if (error)
		printf ("Fail to start core 0x%llX, error %d\n", r->reg.x1,
			error);

	/* Return error to the guest */
	r->reg.x0 = error;
}

static void
smc_std_handle (union exception_saved_regs *r)
{
	u32 func_id = r->reg.x0 & 0xFFFFFFFF;

	switch (func_id) {
	case PSCI_CPU_ON_32BIT:
	case PSCI_CPU_ON_64BIT:
		handle_psci_cpu_on (r);
		break;
	default:
		smc_asm_passthrough_call (r);
	}
}

int
smc_call_hook (union exception_saved_regs *r, uint smc_num)
{
	switch (smc_num) {
	case 0:
		smc_std_handle (r);
		break;
	default:
		/*
		 * According to SMC call convention document, non-zero values
		 * are reserved. For now, we do nothing by returning
		 * PSCI_ERR_NOT_SUPPORTED as an error to the caller.
		 */
		r->reg.x0 = PSCI_ERR_NOT_SUPPORTED;
		printf ("%s(): ignore SMC call %u", __func__, smc_num);
		break;
	}

	return 0;
}
