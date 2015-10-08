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

#ifndef _CORE_SVM_H
#define _CORE_SVM_H

#include "svm_io.h"
#include "svm_msr.h"
#include "svm_vmcb.h"

struct svm_intr_data {
	union {
		struct vmcb_eventinj s;
		u64 v;
	} vmcb_intr_info;
};

struct svm_vmcb_info {
	struct vmcb *vmcb;
	u64 vmcb_phys;
};

struct svm_np;

struct svm {
	struct svm_vmrun_regs vr;
	struct svm_vmcb_info vi;
	struct svm_intr_data intr;
	struct svm_io *io;
	struct svm_msrbmp *msrbmp;
	struct svm_np *np;
	bool lme, lma, svme;
	struct vmcb *saved_vmcb;
	u64 *cr0, *cr3, *cr4;
	u64 gcr0, gcr3, gcr4;
	u64 vm_cr, hsave_pa;
};

struct svm_pcpu_data {
	void *hsave;
	u64 hsave_phys;
	struct vmcb *vmcbhost;
	u64 vmcbhost_phys;
	bool flush_by_asid;
	bool nrip_save;
	u32 nasid;
};

void vmctl_svm_init (void);

#endif
