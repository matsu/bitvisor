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
#include <core/currentcpu.h>
#include <core/initfunc.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include <core/thread.h>
#include "../sym.h"
#include "arm_std_regs.h"
#include "cnt.h"
#include "entry.h"
#include "exception.h"
#include "gic.h"
#include "psci.h"
#include "smc.h"
#include "smc_asm.h"
#include "vm.h"

#define SMCC_VERSION	0x80000000
#define SMCC_MAJOR(val) (((val) >> 16) & 0x7FFF)
#define SMCC_MINOR(val) ((val) & 0xFFFF)

static bool psci_cpu_suspend_support;
static u64 psci_cpu_suspend_statetype_bit;

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
cpu_suspend_reset_ctx_after_resume (union exception_saved_regs *r,
				    u64 entry_point, u64 context_id)
{
	r->reg.elr_el2 = entry_point;
	/* Ensure that when entering EL1, all interrupts are masked. */
	r->reg.spsr_el2 = 0x5 | SPSR_D_BIT | SPSR_A_BIT | SPSR_I_BIT |
		SPSR_F_BIT;
	r->reg.far_el2 = 0x0;
	r->reg.esr_el2 = 0x0;
	r->reg.sp_el0 = 0x0;
	r->reg.tpidr_el0 = 0x0;
	memset (r->regs, 0, sizeof r->regs);
	r->reg.x0 = context_id;
}

static void
handle_psci_powerdown_cpu_suspend (union exception_saved_regs *r,
				   bool *skip_inst)
{
	struct cnt_context cnt_c;
	struct gic_context gic_c;
	bool imm_ret = false;
	int error;

	/*
	 * x0: CPU_SUSPEND function ID
	 * x1: Power state
	 * x2: Entry point
	 * x3: Context ID
	 */
	if (gic_can_suspend ()) {
		cnt_before_suspend (&cnt_c);
		gic_before_suspend (&gic_c);

		/* Check for error from SMC call */
		error = smc_asm_psci_suspend_call (r->reg.x0, r->reg.x1,
						   sym_to_phys (entry_resume),
						   vm_get_current_ctx (),
						   &imm_ret,
						   vmm_mem_start_phys (),
						   vmm_mem_start_virt ());
	} else {
		/*
		 * If it is not possible to suspend, we return immediately.
		 * This emulates the allowed behavior written in the PSCI
		 * document on CPU_SUSPEND caller responsibilities:
		 * 	The powerdown request might not complete due, for
		 * 	example, to pending interrupts.
		 */
		imm_ret = true;
		error = 0;
	}

	if (imm_ret) {
		/* In case of returning from the SMC call immediately */
		if (error)
			printf ("%s(): core %X fails to power-down"
				" error %d\n",
				__func__, currentcpu_get_id (), error);
		r->reg.x0 = error;
	} else {
		if (0)
			printf ("%s(): core %X resumes from power-down"
				" with context 0x%llX to 0x%llX\n",
				__func__, currentcpu_get_id (), r->reg.x3,
				r->reg.x2);
		gic_after_suspend (&gic_c);
		cnt_after_suspend (&cnt_c);
		cpu_suspend_reset_ctx_after_resume (r, r->reg.x2, r->reg.x3);
		*skip_inst = false;
	}
}

static void
handle_psci_cpu_suspend (union exception_saved_regs *r, bool *skip_inst)
{
	bool powerdown;

	if (!psci_cpu_suspend_support) {
		r->reg.x0 = PSCI_ERR_NOT_SUPPORTED;
		return;
	}

	powerdown = !!(r->reg.x1 & psci_cpu_suspend_statetype_bit);
	if (powerdown) {
		handle_psci_powerdown_cpu_suspend (r, skip_inst);
	} else {
		/*
		 * Standby/retention case
		 * x0: CPU_SUSPEND function ID
		 * x1: Power state
		 */
		int error = smc_asm_psci_call (r->reg.x0, r->reg.x1, 0, 0);
		if (error)
			printf ("%s(): core %X fails to standby error %d\n",
				__func__, currentcpu_get_id (), error);
		r->reg.x0 = error;
	}
}

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
				   sym_to_phys (entry_cpu_on),
				   (u64)(stack + VMM_STACKSIZE));

	if (error) {
		printf ("Fail to start core 0x%llX, error %d\n", r->reg.x1,
			error);
		free (stack);
	}

	/* Return error to the guest */
	r->reg.x0 = error;
}

static void
smc_std_handle (union exception_saved_regs *r, bool *skip_inst)
{
	u32 func_id = r->reg.x0 & 0xFFFFFFFF;

	/* Default behavior */
	*skip_inst = true;

	switch (func_id) {
	case PSCI_CPU_SUSPEND_32BIT:
	case PSCI_CPU_SUSPEND_64BIT:
		handle_psci_cpu_suspend (r, skip_inst);
		break;
	case PSCI_CPU_ON_32BIT:
	case PSCI_CPU_ON_64BIT:
		handle_psci_cpu_on (r);
		break;
	default:
		smc_asm_passthrough_call (r);
	}
}

int
smc_call_hook (union exception_saved_regs *r, uint smc_num,
	       bool *skip_inst)
{
	switch (smc_num) {
	case 0:
		smc_std_handle (r, skip_inst);
		break;
	default:
		/*
		 * According to SMC call convention document, non-zero values
		 * are reserved. For now, we do nothing by returning
		 * PSCI_ERR_NOT_SUPPORTED as an error to the caller.
		 */
		*skip_inst = true;
		r->reg.x0 = PSCI_ERR_NOT_SUPPORTED;
		printf ("%s(): ignore SMC call %u", __func__, smc_num);
		break;
	}

	return 0;
}

static void
smc_psci_init (void)
{
	u64 ret;
	u64 ver_major, ver_minor;

	ret = smc_asm_psci_call (PSCI_FEATURES_32BIT, SMCC_VERSION, 0, 0);
	if (ret == PSCI_ERR_NOT_SUPPORTED)
		printf ("SMCC_VERSION not implemented\n");

	ret = smc_asm_psci_call (SMCC_VERSION, 0, 0, 0);
	if (ret == PSCI_ERR_NOT_SUPPORTED)
		printf ("SMCC_VERSION treat as 1.0\n");
	else
		printf ("SMCC_VERSION %llu.%llu\n", SMCC_MAJOR (ret),
			SMCC_MINOR (ret));

	ret = smc_asm_psci_call (PSCI_VERSION_32BIT, 0, 0, 0);
	ver_major = PSCI_VERSION_MAJOR (ret);
	ver_minor = PSCI_VERSION_MINOR (ret);
	printf ("psci: version %llu.%llu\n", ver_major, ver_minor);
	if (ver_major == 0 && ver_minor < 2)
		panic ("%s(): currently expect PSCI VERSION 0.2 or later",
		       __func__);

	psci_cpu_suspend_statetype_bit = BIT (16); /* Default */
	ret = smc_asm_psci_call (PSCI_FEATURES_32BIT, PSCI_CPU_SUSPEND_64BIT,
				 0, 0);
	if (ret == PSCI_ERR_NOT_SUPPORTED) {
		printf ("psci: CPU_SUSPEND not supported\n");
		psci_cpu_suspend_support = false;
	} else {
		printf ("psci: CPU_SUSPEND OS-initiated support %u"
			" ext format %u\n",
			PSCI_CPU_SUSPEND_OS_INITIATED_MODE_SUPPORT (ret),
			PSCI_CPU_SUSPEND_EXT_FORMAT (ret));
		psci_cpu_suspend_support = true;
		if (PSCI_CPU_SUSPEND_EXT_FORMAT (ret))
			psci_cpu_suspend_statetype_bit = BIT (30);
	}
}

INITFUNC ("global5", smc_psci_init);
