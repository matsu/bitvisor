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

#ifndef _CORE_VT_H
#define _CORE_VT_H

#include "asm.h"
#include "vt_io.h"
#include "vt_msr.h"
#include "vt_vmcs.h"

enum vt_event_type {
	VT_EVENT_TYPE_PHYSICAL,
	VT_EVENT_TYPE_VIRTUAL,
	VT_EVENT_TYPE_DELIVERY,
};

struct vt_realmode_data {
	struct descreg idtr;
	ulong tr_limit, tr_acr, tr_base;
};

struct vt_intr_data {
	union {
		struct intr_info s;
		u32 v;
	} vmcs_intr_info;
	u32 vmcs_exception_errcode;
	u32 vmcs_instruction_len;
};

struct vt_vmcs_info {
	void *vmcs_region_virt;
	u64 vmcs_region_phys;
};

struct vt {
	struct vt_vmentry_regs vr;
	struct vt_vmcs_info vi;
	struct vt_realmode_data realmode;
	struct vt_intr_data intr;
	enum vt_event_type event;
	struct vt_io_data io;
	struct vt_msr msr;
	bool lme, lma;
	bool first;
	void *saved_vmcs;
};

struct vt_pcpu_data {
	u32 vmcs_revision_identifier;
	void *vmxon_region_virt;
	u64 vmxon_region_phys;
	u64 vmcs_region_phys;
};

void vt_generate_pagefault (ulong err, ulong cr2);
void vt_generate_external_int (uint num);
void vt_event_virtual (void);
void vmctl_vt_init (void);
void vt_vmptrld (u64 ptr);

#endif
