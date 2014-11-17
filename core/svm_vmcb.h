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

#ifndef _CORE_SVM_VMCB_H
#define _CORE_SVM_VMCB_H

#include "types.h"

enum vmcb_tlb_control {
	VMCB_TLB_CONTROL_DO_NOTHING = 0,
	VMCB_TLB_CONTROL_FLUSH_TLB = 1,
	VMCB_TLB_CONTROL_FLUSH_GUEST_TLB = 3,
	VMCB_TLB_CONTROL_FLUSH_GUEST_NON_GLOBAL_TLB = 7,
};

enum vmcb_eventinj_type {
	VMCB_EVENTINJ_TYPE_INTR = 0,
	VMCB_EVENTINJ_TYPE_NMI = 2,
	VMCB_EVENTINJ_TYPE_EXCEPTION = 3,
	VMCB_EVENTINJ_TYPE_SOFT_INTR = 4,
};

struct vmcb_seg {
	u16 sel;
	u16 attr;
	u32 limit;
	u64 base;
} __attribute__ ((packed));

struct vmcb {
	/* VMCB Control Area */
	/* 0x000 */
	u16 intercept_read_cr;
	u16 intercept_write_cr;
	/* 0x004 */
	u16 intercept_read_dr;
	u16 intercept_write_dr;
	/* 0x008 */
	u32 intercept_exception;
	/* 0x00C */
	unsigned int intercept_intr : 1; /* 0 */
	unsigned int intercept_nmi : 1;
	unsigned int intercept_smi : 1;
	unsigned int intercept_init : 1;
	unsigned int intercept_vintr : 1; /* 4 */
	unsigned int intercept_cr0_ts_or_mp : 1;
	unsigned int intercept_rd_idtr : 1;
	unsigned int intercept_rd_gdtr : 1;
	unsigned int intercept_rd_ldtr : 1; /* 8 */
	unsigned int intercept_rd_tr : 1;
	unsigned int intercept_wr_idtr : 1;
	unsigned int intercept_wr_gdtr : 1;
	unsigned int intercept_wr_ldtr : 1; /* 12 */
	unsigned int intercept_wr_tr : 1;
	unsigned int intercept_rdtsc : 1;
	unsigned int intercept_rdpmc : 1;
	unsigned int intercept_pushf : 1; /* 16 */
	unsigned int intercept_popf : 1;
	unsigned int intercept_cpuid : 1;
	unsigned int intercept_rsm : 1;
	unsigned int intercept_iret : 1; /* 20 */
	unsigned int intercept_int_n : 1;
	unsigned int intercept_invd : 1;
	unsigned int intercept_pause : 1;
	unsigned int intercept_hlt : 1;	/* 24 */
	unsigned int intercept_invlpg : 1;
	unsigned int intercept_invlpga : 1;
	unsigned int intercept_ioio_prot : 1;
	unsigned int intercept_msr_prot : 1; /* 28 */
	unsigned int intercept_task_switches : 1;
	unsigned int intercept_ferr_freeze : 1;
	unsigned int intercept_shutdown : 1;
	/* 0x010 */
	unsigned int intercept_vmrun : 1; /* 0 */
	unsigned int intercept_vmmcall : 1;
	unsigned int intercept_vmload : 1;
	unsigned int intercept_vmsave : 1;
	unsigned int intercept_stgi : 1; /* 4 */
	unsigned int intercept_clgi : 1;
	unsigned int intercept_skinit : 1;
	unsigned int intercept_rdtscp : 1;
	unsigned int intercept_icebp : 1; /* 8 */
	unsigned int intercept_wbinvd : 1;
	unsigned int intercept_monitor : 1;
	unsigned int intercept_mwait_uncond : 1;
	unsigned int intercept_mwait : 1; /* 12 */
	unsigned int reserved010 : 19; /* 31-13 */
	/* 0x014 */
	u32 reserved01[3];	/* 0x014 */
	u32 reserved02[4];	/* 0x020 */
	u32 reserved03[4];	/* 0x030 */
	u64 iopm_base_pa;	/* 0x040 */
	u64 msrpm_base_pa;	/* 0x048 */
	u64 tsc_offset;		/* 0x050 */
	/* 0x058 */
	unsigned int guest_asid : 32; /* 31-0 */
	enum vmcb_tlb_control tlb_control : 8; /* 39-32 */
	unsigned int reserved058 : 24; /* 63-40 */
	/* 0x060 */
	unsigned int v_tpr : 8;	/* 7-0 */
	unsigned int v_irq : 1;	/* 8 */
	unsigned int reserved060_9 : 7;	/* 15-9 */
	unsigned int v_intr_prio : 4; /* 19-16 */
	unsigned int v_ign_tpr : 1; /* 20 */
	unsigned int reserved060_21 : 3; /* 23-21 */
	unsigned int v_intr_masking : 1; /* 24 */
	unsigned int reserved060_25 : 7; /* 31-25 */
	unsigned int v_intr_vector : 8;	/* 39-32 */
	unsigned int reserved060_40 : 24; /* 63-40 */
	/* 0x068 */
	unsigned int interrupt_shadow : 1; /* 0 */
	u64 reserved068 : 63; /* 63-1 */
	/* 0x070 */
	u64 exitcode;		/* 0x070 */
	u64 exitinfo1;		/* 0x078 */
	u64 exitinfo2;		/* 0x080 */
	u64 exitintinfo;	/* 0x088 */
	/* 0x090 */
	unsigned int np_enable : 1; /* 0 */
	u64 reserved090 : 63;	/* 63-1 */
	/* 0x098 */
	u32 reserved09[2];	/* 0x098 */
	u32 reserved0a[2];	/* 0x0A0 */
	u64 eventinj;		/* 0x0A8 */
	u64 n_cr3;		/* 0x0B0 */
	/* 0x0B8 */
	unsigned int lbr_virtualization_enable : 1; /* 0 */
	u64 reserved0b8 : 63;	/* 63-1 */
	/* 0x0C0 */
	u32 reserved0c[2];	/* 0x0C0 */
	u64 nrip;		/* 0x0C8 */
	u32 reserved0d[4];	/* 0x0D0 */
	u32 reserved0e[4];	/* 0x0E0 */
	u32 reserved0f[4];	/* 0x0F0 */
	u32 reserved1[64];	/* 0x100 */
	u32 reserved2[64];	/* 0x200 */
	u32 reserved3[64];	/* 0x300 */
	/* VMCB State Save Area */
	struct vmcb_seg es;	/* 0x400 */
	struct vmcb_seg cs;	/* 0x410 */
	struct vmcb_seg ss;	/* 0x420 */
	struct vmcb_seg ds;	/* 0x430 */
	struct vmcb_seg fs;	/* 0x440 */
	struct vmcb_seg gs;	/* 0x450 */
	struct vmcb_seg gdtr;	/* 0x460 */
	struct vmcb_seg ldtr;	/* 0x470 */
	struct vmcb_seg idtr;	/* 0x480 */
	struct vmcb_seg tr;	/* 0x490 */
	u32 reserved4a[4];	/* 0x4A0 */
	u32 reserved4b[4];	/* 0x4B0 */
	u32 reserved4c_0[2];	/* 0x4C0 */
	u8 reserved4c_8[3];	/* 0x4C8 */
	u8 cpl;			/* 0x4CB */
	u32 reserved4c_c;	/* 0x4CC */
	u64 efer;		/* 0x4D0 */
	u32 reserved4d[2];	/* 0x4D8 */
	u32 reserved4e[4];	/* 0x4E0 */
	u32 reserved4f[4];	/* 0x4F0 */
	u32 reserved50[4];	/* 0x500 */
	u32 reserved51[4];	/* 0x510 */
	u32 reserved52[4];	/* 0x520 */
	u32 reserved53[4];	/* 0x530 */
	u32 reserved54[2];	/* 0x540 */
	u64 cr4;		/* 0x548 */
	u64 cr3;		/* 0x550 */
	u64 cr0;		/* 0x558 */
	u64 dr7;		/* 0x560 */
	u64 dr6;		/* 0x568 */
	u64 rflags;		/* 0x570 */
	u64 rip;		/* 0x578 */
	u32 reserved58[4];	/* 0x580 */
	u32 reserved59[4];	/* 0x590 */
	u32 reserved5a[4];	/* 0x5A0 */
	u32 reserved5b[4];	/* 0x5B0 */
	u32 reserved5c[4];	/* 0x5C0 */
	u32 reserved5d[2];	/* 0x5D0 */
	u64 rsp;		/* 0x5D8 */
	u32 reserved5e[4];	/* 0x5E0 */
	u32 reserved5f[2];	/* 0x5F0 */
	u64 rax;		/* 0x5F8 */
	u64 star;		/* 0x600 */
	u64 lstar;		/* 0x608 */
	u64 cstar;		/* 0x610 */
	u64 sfmask;		/* 0x618 */
	u64 kernel_gs_base;	/* 0x620 */
	u64 sysenter_cs;	/* 0x628 */
	u64 sysenter_esp;	/* 0x630 */
	u64 sysenter_eip;	/* 0x638 */
	u64 cr2;		/* 0x640 */
	u32 reserved64[2];	/* 0x648 */
	u32 reserved65[4];	/* 0x650 */
	u32 reserved66[2];	/* 0x660 */
	u64 g_pat;		/* 0x668 */
	u64 dbgctl;		/* 0x670 */
	u64 br_from;		/* 0x678 */
	u64 br_to;		/* 0x680 */
	u64 lastexcpfrom;	/* 0x688 */
	u64 lastexcpto;		/* 0x690 */
	u32 reserved69[2];	/* 0x698 */
	u32 reserved6a[4];	/* 0x6A0 */
	u32 reserved6b[4];	/* 0x6B0 */
	u32 reserved6c[4];	/* 0x6C0 */
	u32 reserved6d[4];	/* 0x6D0 */
	u32 reserved6e[4];	/* 0x6E0 */
	u32 reserved6f[4];	/* 0x6F0 */
	u32 reserved7[64];	/* 0x700 */
	u32 reserved8[64];	/* 0x800 */
	u32 reserved9[64];	/* 0x900 */
	u32 reserveda[64];	/* 0xA00 */
	u32 reservedb[64];	/* 0xB00 */
	u32 reservedc[64];	/* 0xC00 */
	u32 reservedd[64];	/* 0xD00 */
	u32 reservede[64];	/* 0xE00 */
	u32 reservedf[64];	/* 0xF00 */
} __attribute__ ((packed));

struct exitinfo1_ioio {
	unsigned int type_in : 1;
	unsigned int zero : 1;
	unsigned int str : 1;
	unsigned int rep : 1;
	unsigned int sz8 : 1;
	unsigned int sz16 : 1;
	unsigned int sz32 : 1;
	unsigned int a16 : 1;
	unsigned int a32 : 1;
	unsigned int a64 : 1;
	unsigned int reserved1 : 6;
	unsigned int port : 16;
} __attribute__ ((packed));

struct exitinfo2_task_switch {
	unsigned int err : 32;
	unsigned int reserved1 : 4;
	unsigned int iret : 1;
	unsigned int reserved2 : 1;
	unsigned int jump : 1;
	unsigned int reserved3 : 5;
	unsigned int has_err : 1;
	unsigned int reserved4 : 3;
	unsigned int rf : 1;
	unsigned int reserved5 : 15;
} __attribute__ ((packed));

struct vmcb_eventinj {
	unsigned int vector : 8;
	enum vmcb_eventinj_type type : 3;
	unsigned int ev : 1;
	unsigned int reserved1 : 19;
	unsigned int v : 1;
	unsigned int errorcode : 32;
} __attribute__ ((packed));

#endif
