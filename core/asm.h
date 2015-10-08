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

#ifndef _CORE_ASM_H
#define _CORE_ASM_H

#include "desc.h"
#include "linkage.h"
#include "types.h"

enum invvpid_type {
	INVVPID_TYPE_INDIVIDUAL_ADDRESS = 0,
	INVVPID_TYPE_SINGLE_CONTEXT = 1,
	INVVPID_TYPE_ALL_CONTEXTS = 2,
	INVVPID_TYPE_SINGLE_CONTEXT_EXCEPT_GLOBAL = 3,
};

enum invept_type {
	INVEPT_TYPE_SINGLE_CONTEXT = 1,
	INVEPT_TYPE_ALL_CONTEXTS = 2,
};

struct vt_vmentry_regs {
	ulong rax, rcx, rdx, rbx, cr2, rbp, rsi, rdi;
	ulong r8, r9, r10, r11, r12, r13, r14, r15;
	ulong cr3;
	int re, pg;
	struct {
		int enable, num;
		u16 es, cs, ss, ds, fs, gs;
	} sw;
};

struct svm_vmrun_regs {
	ulong padding1, rcx, rdx, rbx, padding2, rbp, rsi, rdi;
	ulong r8, r9, r10, r11, r12, r13, r14, r15;
};

struct invvpid_desc {
	u64 vpid;
	u64 linearaddr;
} __attribute__ ((packed));

struct invept_desc {
	u64 eptp;
	u64 reserved;
} __attribute__ ((packed));

#define SW_SREG_ES_BIT (1 << 0)
#define SW_SREG_CS_BIT (1 << 1)
#define SW_SREG_SS_BIT (1 << 2)
#define SW_SREG_DS_BIT (1 << 3)
#define SW_SREG_FS_BIT (1 << 4)
#define SW_SREG_GS_BIT (1 << 5)

/* 0f 01 c1                vmcall */
#ifdef AS_DOESNT_SUPPORT_VMX
#	define ASM_VMCALL ".byte 0x0F, 0x01, 0xC1"
#else
#	define ASM_VMCALL "vmcall"
#endif

asmlinkage int asm_vmlaunch_regs_32 (struct vt_vmentry_regs *p);
asmlinkage int asm_vmresume_regs_32 (struct vt_vmentry_regs *p);
asmlinkage int asm_vmlaunch_regs_64 (struct vt_vmentry_regs *p);
asmlinkage int asm_vmresume_regs_64 (struct vt_vmentry_regs *p);
asmlinkage void asm_vmrun_regs_32 (struct svm_vmrun_regs *p, ulong vmcb_phys,
				   ulong vmcbhost_phys);
asmlinkage void asm_vmrun_regs_64 (struct svm_vmrun_regs *p, ulong vmcb_phys,
				   ulong vmcbhost_phys);
asmlinkage void asm_vmrun_regs_nested_32 (struct svm_vmrun_regs *p,
					  ulong vmcb_phys,
					  ulong vmcbhost_phys,
					  ulong vmcbnested,
					  ulong rflags_if_bit);
asmlinkage void asm_vmrun_regs_nested_64 (struct svm_vmrun_regs *p,
					  ulong vmcb_phys,
					  ulong vmcbhost_phys,
					  ulong vmcbnested,
					  ulong rflags_if_bit);

static inline void
asm_cpuid (u32 num, u32 numc, u32 *a, u32 *b, u32 *c, u32 *d)
{
	asm volatile ("cpuid"
		      : "=a" (*a), "=b" (*b), "=c" (*c), "=d" (*d)
		      : "a" (num), "c" (numc));
}

static inline void
asm_rdmsr32 (ulong num, u32 *a, u32 *d)
{
	asm volatile ("rdmsr"
		      : "=a" (*a), "=d" (*d)
		      : "c" (num));
}

static inline void
asm_wrmsr32 (ulong num, u32 a, u32 d)
{
	asm volatile ("wrmsr"
		      :
		      : "c" (num), "a" (a), "d" (d));
}

static inline void
asm_rdmsr64 (ulong num, u64 *value)
{
	u32 a, d;

	asm_rdmsr32 (num, &a, &d);
	*value = (u64)a | ((u64)d << 32);
}

static inline void
asm_wrmsr64 (ulong num, u64 value)
{
	u32 a, d;

	a = (u32)value;
	d = (u32)(value >> 32);
	asm_wrmsr32 (num, a, d);
}

static inline void
asm_rdmsr (ulong num, ulong *value)
{
#ifdef __x86_64__
	asm_rdmsr64 (num, (u64 *)value);
#else
	u32 dummy;

	asm_rdmsr32 (num, (u32 *)value, &dummy);
#endif
}

static inline void
asm_wrmsr (ulong num, ulong value)
{
#ifdef __x86_64__
	asm_wrmsr64 (num, (u64)value);
#else
	asm_wrmsr32 (num, (u32)value, 0);
#endif
}

static inline void
asm_rdcr0 (ulong *cr0)
{
	asm volatile ("mov %%cr0,%0"
		      : "=r" (*cr0));
}

static inline void
asm_wrcr0 (ulong cr0)
{
	asm volatile ("mov %0,%%cr0"
		      :
		      : "r" (cr0));
}

static inline void
asm_rdcr2 (ulong *cr2)
{
	asm volatile ("mov %%cr2,%0"
		      : "=r" (*cr2));
}

static inline void
asm_wrcr2 (ulong cr2)
{
	asm volatile ("mov %0,%%cr2"
		      :
		      : "r" (cr2));
}

static inline void
asm_rdcr3 (ulong *cr3)
{
	asm volatile ("mov %%cr3,%0"
		      : "=r" (*cr3));
}

static inline void
asm_wrcr3 (ulong cr3)
{
	asm volatile ("mov %0,%%cr3"
		      :
		      : "r" (cr3));
}

static inline void
asm_rdcr4 (ulong *cr4)
{
	asm volatile ("mov %%cr4,%0"
		      : "=r" (*cr4));
}

static inline void
asm_wrcr4 (ulong cr4)
{
	asm volatile ("mov %0,%%cr4"
		      :
		      : "r" (cr4));
}

#ifdef __x86_64__
static inline void
asm_rdcr8 (u64 *cr8)
{
	asm volatile ("mov %%cr8,%0"
		      : "=r" (*cr8));
}

static inline void
asm_wrcr8 (ulong cr8)
{
	asm volatile ("mov %0,%%cr8"
		      :
		      : "r" (cr8));
}
#endif

/* f3 0f c7 33             vmxon  (%ebx) */
static inline void
asm_vmxon (void *vmxon_region)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0xf3, 0x0f, 0xc7, 0x33"
		      :
		      : "b" (vmxon_region)
		      : "cc", "memory");
#else
	asm volatile ("vmxon %0"
		      :
		      : "m" (*(ulong *)vmxon_region)
		      : "cc", "memory");
#endif
}

/* 0f 01 c4                vmxoff  */
static inline void
asm_vmxoff (void)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x0f, 0x01, 0xc4"
		      :
		      :
		      : "cc");
#else
	asm volatile ("vmxoff"
		      :
		      :
		      : "cc");
#endif
}

/* 66 0f c7 33             vmclear (%ebx) */
static inline void
asm_vmclear (void *p)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x66, 0x0f, 0xc7, 0x33"
		      :
		      : "b" (p)
		      : "cc", "memory");
#else
	asm volatile ("vmclear %0"
		      :
		      : "m" (*(ulong *)p)
		      : "cc", "memory");
#endif
}

/* 0f c7 33                vmptrld (%ebx) */
static inline void
asm_vmptrld (void *p)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x0f, 0xc7, 0x33"
		      :
		      : "b" (p)
		      : "cc", "memory");
#else
	asm volatile ("vmptrld %0"
		      :
		      : "m" (*(ulong *)p)
		      : "cc", "memory");
#endif
}

/* 0f c7 3b                vmptrst (%ebx) */
static inline void
asm_vmptrst (void *p)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x0f, 0xc7, 0x3b"
		      :
		      : "b" (p)
		      : "cc", "memory");
#else
	asm volatile ("vmptrst %0"
		      :
		      : "m" (*(ulong *)p)
		      : "cc", "memory");
#endif
}

/* 0f 79 c2                vmwrite %edx,%eax */
static inline void
asm_vmwrite (ulong index, ulong val)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x0f, 0x79, 0xc2"
		      :
		      : "a" (index), "d" (val)
		      : "cc");
#else
	asm volatile ("vmwrite %1,%0"
		      :
		      : "r" (index), "rm" (val)
		      : "cc");
#endif
}

static inline void
asm_vmwrite64 (ulong index, u64 val)
{
	ulong tmp, tmp_high;

	tmp = val;
	asm_vmwrite (index, tmp);
	if (sizeof val != sizeof (ulong)) {
		tmp_high = val >> 32;
		asm_vmwrite (index + 1, tmp_high);
	}
}

/* 0f 01 c2                vmlaunch  */
static inline void
asm_vmlaunch (void)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x0f, 0x01, 0xc2"
		      :
		      :
		      : "cc", "memory");
#else
	asm volatile ("vmlaunch"
		      :
		      :
		      : "cc", "memory");
#endif
}

/* 0f 01 c2                vmlaunch  */
static inline int
asm_vmlaunch_regs (struct vt_vmentry_regs *p)
{
#ifdef __x86_64__
	return asm_vmlaunch_regs_64 (p);
#else
	return asm_vmlaunch_regs_32 (p);
#endif
}

/* 0f 01 c3                vmresume  */
static inline void
asm_vmresume (void)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x0f, 0x01, 0xc3"
		      :
		      :
		      : "cc", "memory");
#else
	asm volatile ("vmresume"
		      :
		      :
		      : "cc", "memory");
#endif
}

/* 0f 01 c3                vmresume  */
static inline int
asm_vmresume_regs (struct vt_vmentry_regs *p)
{
#ifdef __x86_64__
	return asm_vmresume_regs_64 (p);
#else
	return asm_vmresume_regs_32 (p);
#endif
}

/* 0f 78 c2                vmread %eax,%edx */
static inline void
asm_vmread (ulong index, ulong *val)
{
#ifdef AS_DOESNT_SUPPORT_VMX
	asm volatile (".byte 0x0f, 0x78, 0xc2"
		      : "=d" (*val)
		      : "a" (index)
		      : "cc");
#else
	asm volatile ("vmread %1,%0"
		      : "=rm" (*val)
		      : "r" (index)
		      : "cc");
#endif
}

static inline void
asm_vmread64 (ulong index, u64 *val)
{
	ulong tmp, tmp_high;

	asm_vmread (index, &tmp);
	if (sizeof *val != sizeof (ulong)) {
		asm_vmread (index + 1, &tmp_high);
		*val = tmp | (u64)tmp_high << 32;
	} else {
		*val = tmp;
	}
}

static inline void
asm_rdes (u16 *es)
{
	asm volatile ("mov %%es,%0"
		      : "=rm" (*es));
}

static inline void
asm_rdcs (u16 *cs)
{
	asm volatile ("mov %%cs,%0"
		      : "=rm" (*cs));
}

static inline void
asm_rdss (u16 *ss)
{
	asm volatile ("mov %%ss,%0"
		      : "=rm" (*ss));
}

static inline void
asm_rdds (u16 *ds)
{
	asm volatile ("mov %%ds,%0"
		      : "=rm" (*ds));
}

static inline void
asm_rdfs (u16 *fs)
{
	asm volatile ("mov %%fs,%0"
		      : "=rm" (*fs));
}

static inline void
asm_rdgs (u16 *gs)
{
	asm volatile ("mov %%gs,%0"
		      : "=rm" (*gs));
}

static inline void
asm_wres (u16 es)
{
	asm volatile ("mov %0, %%es"
		      :
		      : "rm" ((ulong)es));
}

static inline void
asm_wrcs (u16 cs)
{
#ifdef __x86_64__
	asm volatile ("pushq %0; push $1f; lretq; 1:"
		      :
		      : "g" ((ulong)cs));
#else
	asm volatile ("pushl %0; push $1f; lret; 1:"
		      :
		      : "g" ((ulong)cs));
#endif
}

static inline void
asm_wrss (u16 ss)
{
	asm volatile ("mov %0, %%ss"
		      :
		      : "rm" ((ulong)ss));
}

static inline void
asm_wrds (u16 ds)
{
	asm volatile ("mov %0, %%ds"
		      :
		      : "rm" ((ulong)ds));
}

static inline void
asm_wrfs (u16 fs)
{
	asm volatile ("mov %0, %%fs"
		      :
		      : "rm" ((ulong)fs));
}

static inline void
asm_wrgs (u16 gs)
{
	asm volatile ("mov %0, %%gs"
		      :
		      : "rm" ((ulong)gs));
}

static inline void
asm_rdldtr (u16 *ldtr)
{
	asm volatile ("sldt %0"
		      : "=rm" (*ldtr));
}

static inline void
asm_wrldtr (u16 ldtr)
{
	asm volatile ("lldt %0"
		      :
		      : "rm" (ldtr));
}

static inline void
asm_rdtr (u16 *tr)
{
	asm volatile ("str %0"
		      : "=rm" (*tr));
}

static inline void
asm_wrtr (u16 tr)
{
	asm volatile ("ltr %0"
		      :
		      : "rm" (tr));
}

static inline void
asm_lsl (u32 sel, ulong *limit)
{
	asm volatile ("lsl %1,%0"
		      : "=r" (*limit)
		      : "rm" ((ulong)sel)); /* avoid assembler bug */
}

static inline void
asm_lar (u32 sel, ulong *ar)
{
	asm volatile ("lar %1,%0"
		      : "=r" (*ar)
		      : "rm" ((ulong)sel)); /* avoid assembler bug */
}

static inline void
asm_rdgdtr (ulong *gdtbase, ulong *gdtlimit)
{
	struct descreg gdtr;

	asm volatile ("sgdt %0"
		      : "=m" (gdtr));
	*gdtbase = gdtr.base;
	*gdtlimit = gdtr.limit;
}

static inline void
asm_rdidtr (ulong *idtbase, ulong *idtlimit)
{
	struct descreg idtr;

	asm volatile ("sidt %0"
		      : "=m" (idtr));
	*idtbase = idtr.base;
	*idtlimit = idtr.limit;
}

static inline void
asm_wrgdtr (ulong gdtbase, ulong gdtlimit)
{
	struct descreg gdtr;

	gdtr.base = gdtbase;
	gdtr.limit = gdtlimit;
	asm volatile ("lgdt %0"
		      :
		      : "m" (gdtr));
}

static inline void
asm_wridtr (ulong idtbase, ulong idtlimit)
{
	struct descreg idtr;

	idtr.base = idtbase;
	idtr.limit = idtlimit;
	asm volatile ("lidt %0"
		      :
		      : "m" (idtr));
}

static inline void
asm_rddr7 (ulong *dr7)
{
	asm volatile ("mov %%dr7,%0"
		      : "=r" (*dr7));
}

static inline void
asm_rdrflags (ulong *rflags)
{
	asm volatile (
#ifdef __x86_64__
		"pushfq ; popq %0"
#else
		"pushfl ; popl %0"
#endif
		: "=rm" (*rflags));
}

static inline void
asm_inb (u32 port, u8 *data)
{
	asm volatile ("inb %%dx,%%al"
		      : "=a" (*data)
		      : "d" (port));
}

static inline void
asm_inw (u32 port, u16 *data)
{
	asm volatile ("inw %%dx,%%ax"
		      : "=a" (*data)
		      : "d" (port));
}

static inline void
asm_inl (u32 port, u32 *data)
{
	asm volatile ("inl %%dx,%%eax"
		      : "=a" (*data)
		      : "d" (port));
}

static inline void
asm_outb (u32 port, u8 data)
{
	asm volatile ("outb %%al,%%dx"
		      :
		      : "a" (data)
		      , "d" (port));
}

static inline void
asm_outw (u32 port, u16 data)
{
	asm volatile ("outw %%ax,%%dx"
		      :
		      : "a" (data)
		      , "d" (port));
}

static inline void
asm_outl (u32 port, u32 data)
{
	asm volatile ("outl %%eax,%%dx"
		      :
		      : "a" (data)
		      , "d" (port));
}

static inline void
asm_cli (void)
{
	asm volatile ("cli");
}

static inline void
asm_sti (void)
{
	asm volatile ("sti");
}

static inline void
asm_wrrsp_and_jmp (ulong rsp, void *jmpto)
{
	asm volatile (
		"xor %%ebp,%%ebp; "
#ifdef __x86_64__
		"mov %0,%%rsp; jmp *%1"
#else
		"mov %0,%%esp; jmp *%1"
#endif
		:
		: "g" (rsp)
		, "r" (jmpto));
}

static inline void
asm_wrrsp_and_ret (ulong rsp, ulong rax)
{
	asm volatile (
#ifdef __x86_64__
		"mov %0,%%rsp; ret"
#else
		"mov %0,%%esp; ret"
#endif
		:
		: "g" (rsp)
		, "a" (rax));
}

static inline void
asm_wbinvd (void)
{
	asm volatile ("wbinvd");
}

static inline void
asm_invlpg (void *p)
{
	asm volatile ("invlpg %0"
		      :
		      : "m" (*(u8 *)p));
}

static inline void
asm_invlpga (ulong addr, u32 asid)
{
	asm volatile ("invlpga" : : "a" (addr), "c" (asid));
}

static inline void
asm_cli_and_hlt (void)
{
	asm volatile ("cli; hlt");
}

static inline void
asm_sti_and_nop_and_cli (void)
{
	asm volatile ("sti ; nop ; cli");
}

static inline void
asm_pause (void)
{
	/* This function is used by in loops of spinlock-like
	 * functions.  So memory may be modified by other CPUs during
	 * this function.  "memory" avoids some optimization for
	 * convenient. */
	asm volatile ("pause" : : : "memory");
}

static inline void
asm_rdrsp (ulong *rsp)
{
#ifdef __x86_64__
	asm volatile ("mov %%rsp,%0" : "=rm" (*rsp));
#else
	asm volatile ("mov %%esp,%0" : "=rm" (*rsp));
#endif
}

static inline void
asm_rdtsc (u32 *a, u32 *d)
{
	asm volatile ("rdtsc" : "=a" (*a), "=d" (*d));
}

static inline void
asm_mul_and_div (u32 mul1, u32 mul2, u32 div1, u32 *quotient, u32 *remainder)
{
	asm ("mull %4 ; divl %5"
	     : "=&a" (*quotient)
	     , "=&d" (*remainder)
	     : "0" (mul1)
	     , "1" (0)
	     , "rm" (mul2)
	     , "rm" (div1)
	     : "cc");
}

static inline void
asm_lock_incl (u32 *d)
{
	asm volatile ("lock incl %0" : "+m" (*d));
}

/*
  if (*dest == *cmp) {
      *dest = eq;
      return false;
  } else {
      *cmp = *dest;
      return true;
  }
*/
static inline bool
asm_lock_cmpxchgl (u32 *dest, u32 *cmp, u32 eq)
{
	u8 tmp;

	asm volatile ("lock cmpxchgl %4,%1 ; setne %2"
		      : "=&a" (*cmp)
		      , "+m" (*dest)
#ifdef __x86_64__
		      , "=r" (tmp)
#else
		      , "=dcb" (tmp)
#endif
		      : "0" (*cmp)
		      , "r" (eq)
		      : "memory", "cc");
	return (bool)tmp;
}

static inline bool
asm_lock_cmpxchgq (u64 *dest, u64 *cmp, u64 eq)
{
	u8 tmp;

#ifdef __x86_64__
	asm volatile ("lock cmpxchgq %4,%1 ; setne %2"
		      : "=&a" (*cmp)
		      , "+m" (*dest)
		      , "=r" (tmp)
		      : "0" (*cmp)
		      , "r" (eq)
		      : "memory", "cc");
#else
	asm volatile ("lock cmpxchg8b %1 ; setne %2"
		      : "=&A" (*cmp)
		      , "+m" (*dest)
		      , "=bc" (tmp)
		      : "0" (*cmp)
		      , "b" ((u32)eq)
		      , "c" ((u32)(eq >> 32))
		      : "memory", "cc");
#endif
	return (bool)tmp;
}

/* old = *mem; *mem = newval; return old; */
static inline u32
asm_lock_xchgl (u32 *mem, u32 newval)
{
	u32 oldval;

	asm volatile ("xchg %0,%1"
		      : "=r" (oldval)
		      , "+m" (*mem)
		      : "0" (newval));
	return oldval;
}

/* old = *mem; *mem = newval; return old; */
static inline ulong
asm_lock_ulong_swap (ulong *mem, ulong newval)
{
	ulong oldval;

	asm volatile ("xchg %0,%1"
		      : "=r" (oldval)
		      , "+m" (*mem)
		      : "0" (newval));
	return oldval;
}

/* 0f 01 d8                vmrun */
static inline void
asm_vmrun_regs (struct svm_vmrun_regs *p, ulong vmcb_phys, ulong vmcbhost_phys)
{
#ifdef __x86_64__
	asm_vmrun_regs_64 (p, vmcb_phys, vmcbhost_phys);
#else
	asm_vmrun_regs_32 (p, vmcb_phys, vmcbhost_phys);
#endif
}

static inline void
asm_vmrun_regs_nested (struct svm_vmrun_regs *p, ulong vmcb_phys,
		       ulong vmcbhost_phys, ulong vmcbnested,
		       ulong rflags_if_bit)
{
#ifdef __x86_64__
	asm_vmrun_regs_nested_64 (p, vmcb_phys, vmcbhost_phys, vmcbnested,
				  rflags_if_bit);
#else
	asm_vmrun_regs_nested_32 (p, vmcb_phys, vmcbhost_phys, vmcbnested,
				  rflags_if_bit);
#endif
}

/* 0f 01 d1                xsetbv */
static inline void
asm_xsetbv (u32 c, u32 a, u32 d)
{
	asm volatile (".byte 0x0F, 0x01, 0xD1" /* xsetbv */
		      :
		      : "c" (c), "a" (a), "d" (d));
}

/* 66 0f 38 81 3e          invvpid (%esi),%edi*/
static inline void
asm_invvpid (enum invvpid_type type, struct invvpid_desc *desc)
{
	asm volatile (".byte 0x66, 0x0F, 0x38, 0x81, 0x3E" /* invvpid */
		      :
		      : "D" (type), "S" (desc)
		      : "memory", "cc");
}

/* 66 0f 38 80 3e          invept (%esi),%edi*/
static inline void
asm_invept (enum invept_type type, struct invept_desc *desc)
{
	asm volatile (".byte 0x66, 0x0F, 0x38, 0x80, 0x3E" /* invept */
		      :
		      : "D" (type), "S" (desc)
		      : "memory", "cc");
}

#endif
