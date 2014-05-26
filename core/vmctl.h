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

#ifndef _CORE_VMCTL_H
#define _CORE_VMCTL_H

#include "cpu_seg.h"
#include "regs.h"
#include "types.h"

struct vcpu;

struct vmctl_func {
	void (*vminit) (void);
	void (*vmexit) (void);
	void (*start_vm) (void);
	void (*generate_pagefault) (ulong err, ulong cr2);
	void (*generate_external_int) (uint num);
	void (*read_general_reg) (enum general_reg reg, ulong *val);
	void (*write_general_reg) (enum general_reg reg, ulong val);
	void (*read_control_reg) (enum control_reg reg, ulong *val);
	void (*write_control_reg) (enum control_reg reg, ulong val);
	void (*read_sreg_sel) (enum sreg s, u16 *val);
	void (*read_sreg_acr) (enum sreg s, ulong *val);
	void (*read_sreg_base) (enum sreg s, ulong *val);
	void (*read_sreg_limit) (enum sreg s, ulong *val);
	void (*spt_setcr3) (ulong cr3);
	void (*spt_tlbflush) (void);
	void (*read_ip) (ulong *val);
	void (*write_ip) (ulong val);
	void (*read_flags) (ulong *val);
	void (*write_flags) (ulong val);
	void (*read_gdtr) (ulong *base, ulong *limit);
	void (*write_gdtr) (ulong base, ulong limit);
	void (*read_idtr) (ulong *base, ulong *limit);
	void (*write_idtr) (ulong base, ulong limit);
	void (*write_realmode_seg) (enum sreg s, u16 val);
	enum vmmerr (*writing_sreg) (enum sreg s);
	bool (*read_msr) (u32 msrindex, u64 *msrdata);
	bool (*write_msr) (u32 msrindex, u64 msrdata);
	void (*cpuid) (u32 ia, u32 ic, u32 *oa, u32 *ob, u32 *oc, u32 *od);
	void (*iopass) (u32 port, bool pass);
	void (*exint_pass) (bool enable);
	void (*exint_pending) (bool pending);
	void (*init_signal) (void);
	void (*tsc_offset_changed) (void);
	void (*panic_dump) (void);
	void (*invlpg) (ulong addr);
	void (*reset) (void);
	bool (*extern_flush_tlb_entry) (struct vcpu *p, phys_t s, phys_t e);
	bool (*xsetbv) (u32 ic, u32 ia, u32 id);
	void (*enable_resume) (void);
	void (*resume) (void);
	void (*paging_map_1mb) (void);
	void (*msrpass) (u32 msrindex, bool wr, bool pass);
};

#endif
