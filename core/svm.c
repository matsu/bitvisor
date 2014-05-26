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

#include "asm.h"
#include "cpu_emul.h"
#include "cpuid.h"
#include "current.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "string.h"
#include "svm.h"
#include "svm_init.h"
#include "svm_io.h"
#include "svm_main.h"
#include "svm_msr.h"
#include "svm_paging.h"
#include "svm_panic.h"
#include "svm_regs.h"
#include "vmctl.h"

#define UNIMPLEMENTED() do { \
	panic ("DEBUG : file %s line %d function %s NOT IMPLEMENTED", \
	       __FILE__, __LINE__, __FUNCTION__); \
} while (0)

static void svm_generate_pagefault (ulong err, ulong cr2);
static void svm_exint_pass (bool enable);
static void svm_exint_pending (bool pending);
static void svm_generate_external_int (uint num);
static void svm_tsc_offset_changed (void);
static bool svm_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e);
static void svm_spt_tlbflush (void);
static void svm_spt_setcr3 (ulong cr3);
static void svm_invlpg (ulong addr);

static struct vmctl_func func = {
	svm_vminit,
	svm_vmexit,
	svm_start_vm,
	svm_generate_pagefault,
	svm_generate_external_int,
	svm_read_general_reg,
	svm_write_general_reg,
	svm_read_control_reg,
	svm_write_control_reg,
	svm_read_sreg_sel,
	svm_read_sreg_acr,
	svm_read_sreg_base,
	svm_read_sreg_limit,
	svm_spt_setcr3,
	svm_spt_tlbflush,
	svm_read_ip,
	svm_write_ip,
	svm_read_flags,
	svm_write_flags,
	svm_read_gdtr,
	svm_write_gdtr,
	svm_read_idtr,
	svm_write_idtr,
	svm_write_realmode_seg,
	svm_writing_sreg,
	svm_read_msr,
	svm_write_msr,
	call_cpuid,
	svm_iopass,
	svm_exint_pass,
	svm_exint_pending,
	svm_init_signal,
	svm_tsc_offset_changed,
	svm_panic_dump,
	svm_invlpg,
	svm_reset,
	svm_extern_flush_tlb_entry,
	call_xsetbv,
	svm_enable_resume,
	svm_resume,
	svm_paging_map_1mb,
	svm_msrpass,
};

static void
svm_generate_pagefault (ulong err, ulong cr2)
{
	struct svm_intr_data *sid = &current->u.svm.intr;

	sid->vmcb_intr_info.v = 0;
	sid->vmcb_intr_info.s.vector = EXCEPTION_PF;
	sid->vmcb_intr_info.s.type = VMCB_EVENTINJ_TYPE_EXCEPTION;
	sid->vmcb_intr_info.s.ev = 1;
	sid->vmcb_intr_info.s.v = 1;
	sid->vmcb_intr_info.s.errorcode = (u32)err;
	current->u.svm.vi.vmcb->cr2 = cr2;
}

static void
svm_exint_pass (bool enable)
{
	if (enable)
		current->u.svm.vi.vmcb->intercept_intr = 0;
	else
		current->u.svm.vi.vmcb->intercept_intr = 1;
}

static void
svm_exint_pending (bool pending)
{
	if (pending)
		current->u.svm.vi.vmcb->intercept_vintr = 1;
	else
		current->u.svm.vi.vmcb->intercept_vintr = 0;
}

static void
svm_generate_external_int (uint num)
{
	struct svm_intr_data *sid = &current->u.svm.intr;

	sid->vmcb_intr_info.v = 0;
	sid->vmcb_intr_info.s.vector = num;
	sid->vmcb_intr_info.s.type = VMCB_EVENTINJ_TYPE_INTR;
	sid->vmcb_intr_info.s.ev = 0;
	sid->vmcb_intr_info.s.v = 1;
}

static void
svm_tsc_offset_changed (void)
{
	current->u.svm.vi.vmcb->tsc_offset = current->tsc_offset;
}

static bool
svm_extern_flush_tlb_entry (struct vcpu *p, phys_t s, phys_t e)
{
	return svm_paging_extern_flush_tlb_entry (p, s, e);
}

static void
svm_spt_tlbflush (void)
{
	svm_paging_flush_guest_tlb ();
}

static void
svm_spt_setcr3 (ulong cr3)
{
	svm_paging_spt_setcr3 (cr3);
}

static void
svm_invlpg (ulong addr)
{
	svm_paging_invalidate (addr);
}

void
vmctl_svm_init (void)
{
	memcpy ((void *)&current->vmctl, (void *)&func, sizeof func);
}
