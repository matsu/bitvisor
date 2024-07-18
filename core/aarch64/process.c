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

#include <arch/process.h>
#include <arch/vmm_mem.h>
#include <core/assert.h>
#include <core/initfunc.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include "../mm.h"
#include "../sym.h"
#include "../process.h"
#include "../process_builtin.h"
#include "arm_std_regs.h"
#include "exception.h"
#include "mm.h"
#include "pcpu.h"
#include "process.h"
#include "process_asm.h"
#include "tpidr.h"

extern struct process_builtin_data __process_builtin[];
extern struct process_builtin_data __process_builtin_end[];

void
process_arch_setup (void)
{
	/* Currently, there is nothing to do */
}

void
process_arch_callret (int retval)
{
	/*
	 * At the point the stack looks like the following
	 *
	 * ----------High address----------
	 *
	 * Old exception frame on EL2 entry
	 *
	 * --------------------------------
	 *
	 * Saved data on stack before entering EL0
	 *
	 * -------------------------------- < sp_on_entry is here
	 *
	 * Latest excecption frame from EL0 system call for exit
	 *
	 * --------------------------------
	 *
	 * We want to revert sp back to sp_on_entry to continue previous
	 * execution context.
	 *
	 */
	process_asm_return_from_proc (retval, exception_sp_on_entry ());
}

bool
process_arch_syscallstack_ok (void)
{
	return true; /* We currently have nothing to do with syscall stack */
}

int
process_arch_exec (void *func, ulong sp, void *arg, int len, ulong buf,
		   int bufcnt)
{
	struct pcpu *currentcpu;
	union exception_saved_regs *r;
	struct mm_arch_proc_desc *mm_proc_desc;
	u64 orig_elr, orig_spsr;
	u64 orig_sp_el0, orig_tpidr_el0;
	u64 orig_hcr;
	u64 msg[2];
	int ret;

	ASSERT (len == sizeof msg);

	memcpy (msg, arg, sizeof msg);

	currentcpu = tpidr_get_pcpu ();
	r = currentcpu->exception_data.saved_regs;
	mm_proc_desc = currentcpu->cur_mm_proc_desc;

	orig_elr = r->reg.elr_el2;
	orig_spsr = r->reg.spsr_el2;
	orig_hcr = r->reg.hcr_el2;
	orig_sp_el0 = r->reg.sp_el0;
	orig_tpidr_el0 = r->reg.tpidr_el0;

	msr (ELR_EL2, (u64)func);
	msr (SPSR_EL2, SPSR_INTR_MASK); /* SPSR_M is 0 for EL0 */
	mm_process_record_cur_sp (mm_proc_desc, sp);
	msr (SP_EL0, sp);
	msr (TPIDR_EL0, 0);
	msr (HCR_EL2, HCR_E2H | HCR_TGE);
	/* Want HCR_EL2 write to be effective immediately */
	isb ();

	/* Jump to EL0 for execution */
	ret = process_asm_enter_el0 (msg[0], msg[1], buf, bufcnt);

	msr (HCR_EL2, orig_hcr);
	isb ();
	msr (TPIDR_EL0, orig_tpidr_el0);
	mm_process_record_cur_sp (mm_proc_desc, mrs (SP_EL0));
	msr (SP_EL0, orig_sp_el0);
	msr (SPSR_EL2, orig_spsr);
	msr (ELR_EL2, orig_elr);

	return ret;
}

phys_t
processuser_arch_syscall_phys (void)
{
	return sym_to_phys (process_asm_processuser_syscall);
}

int
process_handle_syscall (union exception_saved_regs *r)
{
	syscall_func_t func = process_get_syscall_func (r->reg.x8);

	if (func) {
		r->reg.x0 = func (r->reg.elr_el2, r->reg.sp_el0, r->reg.x8,
				  r->reg.x0, r->reg.x1);
		return 0;
	}

	printf ("Bad system call.\n");
	printf ("x8(syscall_num): %llu, x0(ip): %llX, x1(sp): %llX, "
		"x2(num): %llX, x3(a0): %llX, x4(a1): %llX\n",
		r->reg.x8, r->reg.x0, r->reg.x1, r->reg.x2, r->reg.x3,
		r->reg.x4);
	process_kill (NULL, NULL);
	panic ("process_kill() failed\n");
	return -1; /* Never return for now */
}

static void
update_process_builtin (void)
{
	struct process_builtin_data *p;

	/* Correct virtual address */
	for (p = __process_builtin; p != __process_builtin_end; p++) {
		p->name = (char *)(vmm_mem_start_virt () + (u64)p->name);
		p->bin = (void *)(vmm_mem_start_virt () + (u64)p->bin);
	}
}

INITFUNC ("global0", update_process_builtin);
