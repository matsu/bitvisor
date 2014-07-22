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

#ifndef _CORE_PCPU_H
#define _CORE_PCPU_H

#include "asm.h"
#include "cache.h"
#include "desc.h"
#include "panic.h"
#include "seg.h"
#include "spinlock.h"
#include "svm.h"
#include "thread.h"
#include "types.h"
#include "vt.h"

#define NUM_OF_SEGDESCTBL 32
#define PCPU_GS_ALIGN  __attribute__ ((aligned (8)))

enum fullvirtualize_type {
	FULLVIRTUALIZE_NONE,
	FULLVIRTUALIZE_VT,
	FULLVIRTUALIZE_SVM,
};

struct pcpu_func {
};

struct pcpu {
	struct pcpu *next;
	struct pcpu_func func;
	struct segdesc segdesctbl[NUM_OF_SEGDESCTBL];
	struct tss32 tss32;
	struct tss64 tss64;
	struct vt_pcpu_data vt;
	struct svm_pcpu_data svm;
	struct cache_pcpu_data cache;
	struct panic_pcpu_data panic;
	struct thread_pcpu_data thread;
	enum fullvirtualize_type fullvirtualize;
	int cpunum;
	int pid;
	void *stackaddr;
	u64 tsc, hz, timediff;
	spinlock_t suspend_lock;
	phys_t cr3;
	bool pass_vm_created;
	bool use_invariant_tsc;
	void (*release_process64_msrs) (void *release_process64_msrs_data);
	void *release_process64_msrs_data;
};

struct pcpu_gs {
	u64 inthandling PCPU_GS_ALIGN;	/* %gs:0  (int.c, int_handler.s) */
	void *currentcpu PCPU_GS_ALIGN;	/* %gs:8  (pcpu.h) */
	void *syscallstack PCPU_GS_ALIGN;
				   /* %gs:16 (process.c, process_sysenter.s) */
	void *current PCPU_GS_ALIGN;	/* %gs:24 (current.h) */
	u64 nmi;		/* %gs:32 (nmi_pass.c) */
	u64 init_count;		/* %gs:40 (sx_init_pass.c, sx_handler.s) */
};

extern struct pcpu pcpu_default;
extern struct pcpu *currentcpu asm ("%gs:gs_currentcpu");

static inline bool
currentcpu_available (void)
{
	u16 gs;

	/* FIXME: SWAPGS in 64bit long mode can't be used. */
	asm_rdgs (&gs);
	if (gs == SEG_SEL_PCPU32 || gs == SEG_SEL_PCPU64)
		return true;
	else
		return false;
}

void pcpu_list_foreach (bool (*func) (struct pcpu *p, void *q), void *q);
void pcpu_list_add (struct pcpu *d);
void pcpu_init (void);

#endif
