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

#ifndef _CORE_X86_VCPU_H
#define _CORE_X86_VCPU_H

#include <core/types.h>
#include "asm.h"
#include "acpi.h"
#include "cache.h"
#include "cpu_mmu_spt.h"
#include "cpuid.h"
#include "exint.h"
#include "gmm.h"
#include "initipi.h"
#include "io_io.h"
#include "localapic.h"
#include "mmio.h"
#include "msr.h"
#include "nmi.h"
#include "svm.h"
#include "vmctl.h"
#include "vt.h"
#include "xsetbv.h"

struct vcpu {
	struct vcpu *next;
	union {
		struct vt vt;
		struct svm svm;
	} u;
	bool halt;
	bool initialized;
	u64 tsc_offset;
	bool updateip;
	bool pass_vm;
	u64 pte_addr_mask;
	struct cpu_mmu_spt_data spt;
	struct cpuid_data cpuid;
	struct exint_func exint;
	struct gmm_func gmm;
	struct io_io_data io;
	struct msr_data msr;
	struct vmctl_func vmctl;
	const struct mm_as *as;
	/* vcpu0: data per VM */
	struct vcpu *vcpu0;
	struct mmio_data mmio;
	struct nmi_func nmi;
	struct xsetbv_data xsetbv;
	struct acpi_data acpi;
	struct localapic_data localapic;
	struct initipi_func initipi;
	struct cache_data cache;
};

void vcpu_list_foreach (bool (*func) (struct vcpu *p, void *q), void *q);
void load_new_vcpu (struct vcpu *vcpu0);

#endif
