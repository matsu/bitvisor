/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2023-2024 The University of Tokyo
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

#include <core/config.h>
#include <core/currentcpu.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include "../exint_pass.h"
#include "asm.h"
#include "cpu_mmu.h"
#include "current.h"
#include "pcpu.h"
#include "vt_addip.h"
#include "vt_ept.h"
#include "vt_exitreason.h"
#include "vt_paging.h"
#include "vt_regs.h"
#include "vt_shadow_vt.h"

#ifdef __x86_64__
#define PUSHF_POP "pushfq; popq"
#else
#define PUSHF_POP "pushfl; popl"
#endif

#define INDICATOR_SHADOWING_BIT 0x80000000

struct vmread_vmwrite_bmp {
	phys_t bmp_pass_phys;
	phys_t bmp_old_phys;
	phys_t bmp_new_phys;
};

union instruction_info {
	ulong v;
	struct {
		/*
		 * Table 27-14 on Intel SDM
		 * (Order Number: 326019-048US, September 2013)
		 */
		unsigned int scalling : 2;
		unsigned int undefined1 : 1;
		unsigned int reg1 : 4;	/* For only VMREAD/WRITE */
		unsigned int addr_size : 3;
		unsigned int mem_or_reg : 1; /* For only VMREAD/WRITE */
		unsigned int undefined2 : 4;
		unsigned int segment_reg : 3;
		unsigned int indexreg : 4;
		unsigned int indexreg_invalid : 1;
		unsigned int basereg : 4;
		unsigned int basereg_invalid : 1;
		unsigned int reg2 : 4;	/* Not used for
					 * VMXON/VMCLEAR/VMPTRLD/VMPTRST */
	} s;
};

#define NUM_OF_SHADOW_EPT 2
#define NUM_OF_SHADOW_VPID 16

struct shadow_ept_ept_info {
	struct vt_ept *shadow_ept;
	u64 ep4ta;
	u8 active;
	LIST3_DEFINE (struct shadow_ept_ept_info, list, i8);
};

struct shadow_ept_vpid_info {
	u64 ep4ta;
	u16 vpid;
	LIST3_DEFINE (struct shadow_ept_vpid_info, list, i8);
};

struct shadow_ept_data {
	struct shadow_ept_ept_info ept[NUM_OF_SHADOW_EPT];
	struct shadow_ept_vpid_info vpid[NUM_OF_SHADOW_VPID];
	LIST3_DEFINE_HEAD (list_ept, struct shadow_ept_ept_info, list);
	LIST3_DEFINE_HEAD (list_vpid, struct shadow_ept_vpid_info, list);
};

#define VMCS_DEFARRAY(name, array) ulong name[sizeof array / sizeof array[0]]
#define VMCS_SAVEARRAY(name, array) \
	save_vmcs (name, array, sizeof array / sizeof array[0])
#define VMCS_RESTOREARRAY(name, array) \
	restore_vmcs (name, array, sizeof array / sizeof array[0])
#define VMCS_CLEARBITSINBMP(bmp_addr, array) \
	clear_shadow_vmcs_bmp (bmp_addr, array, sizeof array / sizeof array[0])

static const u16 vmcs_indexes_shadow_hs[] = {
	VMCS_HOST_ES_SEL,
	VMCS_HOST_CS_SEL,
	VMCS_HOST_SS_SEL,
	VMCS_HOST_DS_SEL,
	VMCS_HOST_FS_SEL,
	VMCS_HOST_GS_SEL,
	VMCS_HOST_TR_SEL,
	VMCS_HOST_IA32_PAT,
	VMCS_HOST_IA32_PAT_HIGH,
	VMCS_HOST_IA32_SYSENTER_CS,
	VMCS_HOST_CR0,
	VMCS_HOST_CR3,
	VMCS_HOST_CR4,
	VMCS_HOST_FS_BASE,
	VMCS_HOST_GS_BASE,
	VMCS_HOST_TR_BASE,
	VMCS_HOST_GDTR_BASE,
	VMCS_HOST_IDTR_BASE,
	VMCS_HOST_IA32_SYSENTER_ESP,
	VMCS_HOST_IA32_SYSENTER_EIP,
	VMCS_HOST_RSP,
	VMCS_HOST_RIP,
};

static const u16 vmcs_indexes_shadow_shadowing[] = {
	VMCS_GUEST_ES_SEL,
	VMCS_GUEST_CS_SEL,
	VMCS_GUEST_SS_SEL,
	VMCS_GUEST_DS_SEL,
	VMCS_GUEST_FS_SEL,
	VMCS_GUEST_GS_SEL,
	VMCS_GUEST_LDTR_SEL,
	VMCS_GUEST_TR_SEL,
	VMCS_TSC_OFFSET,
	VMCS_TSC_OFFSET_HIGH,
	VMCS_PROC_BASED_VMEXEC_CTL,
	VMCS_VMENTRY_INTR_INFO_FIELD,
	VMCS_TPR_THRESHOLD,
	VMCS_GUEST_ES_LIMIT,
	VMCS_GUEST_CS_LIMIT,
	VMCS_GUEST_SS_LIMIT,
	VMCS_GUEST_DS_LIMIT,
	VMCS_GUEST_FS_LIMIT,
	VMCS_GUEST_GS_LIMIT,
	VMCS_GUEST_LDTR_LIMIT,
	VMCS_GUEST_TR_LIMIT,
	VMCS_GUEST_GDTR_LIMIT,
	VMCS_GUEST_IDTR_LIMIT,
	VMCS_GUEST_ES_ACCESS_RIGHTS,
	VMCS_GUEST_CS_ACCESS_RIGHTS,
	VMCS_GUEST_SS_ACCESS_RIGHTS,
	VMCS_GUEST_DS_ACCESS_RIGHTS,
	VMCS_GUEST_FS_ACCESS_RIGHTS,
	VMCS_GUEST_GS_ACCESS_RIGHTS,
	VMCS_GUEST_LDTR_ACCESS_RIGHTS,
	VMCS_GUEST_TR_ACCESS_RIGHTS,
	VMCS_GUEST_INTERRUPTIBILITY_STATE,
	VMCS_CR0_READ_SHADOW,
	VMCS_CR4_READ_SHADOW,
	VMCS_GUEST_CR3,
	VMCS_GUEST_ES_BASE,
	VMCS_GUEST_CS_BASE,
	VMCS_GUEST_SS_BASE,
	VMCS_GUEST_DS_BASE,
	VMCS_GUEST_FS_BASE,
	VMCS_GUEST_GS_BASE,
	VMCS_GUEST_LDTR_BASE,
	VMCS_GUEST_TR_BASE,
	VMCS_GUEST_GDTR_BASE,
	VMCS_GUEST_IDTR_BASE,
	VMCS_GUEST_DR7,
	VMCS_GUEST_RSP,
	VMCS_GUEST_RIP,
	VMCS_GUEST_RFLAGS,
};

static const u16 vmcs_indexes_shadow_readonly[] = {
	VMCS_GUEST_PHYSICAL_ADDRESS,
	VMCS_GUEST_PHYSICAL_ADDRESS_HIGH,
	VMCS_EXIT_REASON,
	VMCS_VMEXIT_INTR_INFO,
	VMCS_VMEXIT_INTR_ERRCODE,
	VMCS_IDT_VECTORING_INFO_FIELD,
	VMCS_IDT_VECTORING_ERRCODE,
	VMCS_VMEXIT_INSTRUCTION_LEN,
	VMCS_VMEXIT_INSTRUCTION_INFO,
	VMCS_EXIT_QUALIFICATION,
};

static const ulong flag_mask =
	(RFLAGS_CF_BIT | RFLAGS_PF_BIT | RFLAGS_AF_BIT | RFLAGS_ZF_BIT |
	 RFLAGS_SF_BIT | RFLAGS_OF_BIT);
static const ulong flag_fail_invalid = RFLAGS_CF_BIT;
static const ulong flag_fail_valid = RFLAGS_ZF_BIT;

static inline void
asm_vmxon_and_rdrflags (void *vmxon_region, ulong *rflags)
{
	asm volatile ("vmxon %1; " PUSHF_POP " %0"
		      : "=rm" (*rflags)
		      : "m" (*(ulong *)vmxon_region)
		      : "cc", "memory");
}

static inline void
asm_vmread_and_rdrflags (ulong index, ulong *val, ulong *rflags)
{
	asm volatile ("vmread %2,%0; " PUSHF_POP " %1"
		      : "=rm" (*val), "=rm" (*rflags)
		      : "r" (index)
		      : "cc");
}

static inline void
asm_vmwrite_and_rdrflags (ulong index, ulong val, ulong *rflags)
{
	asm volatile ("vmwrite %2,%1; " PUSHF_POP " %0"
		      : "=rm" (*rflags)
		      : "r" (index), "rm" (val)
		      : "cc");
}

static inline void
asm_vmptrld_and_rdrflags (void *p, ulong *rflags)
{
	asm volatile ("vmptrld %1; " PUSHF_POP " %0"
		      : "=rm" (*rflags)
		      : "m" (*(ulong *)p)
		      : "cc", "memory");
}

static inline void
asm_vmclear_and_rdrflags (void *p, ulong *rflags)
{
	asm volatile ("vmclear %1; " PUSHF_POP " %0"
		      : "=rm" (*rflags)
		      : "m" (*(ulong *)p)
		      : "cc", "memory");
}

/* 66 0f 38 81 3e          invvpid (%esi),%edi*/
static inline void
asm_invvpid_and_rdrflags (enum invvpid_type type, struct invvpid_desc *desc,
			  ulong *rflags)
{
	asm volatile (".byte 0x66, 0x0F, 0x38, 0x81, 0x3E \n" /* invvpid */
		      PUSHF_POP " %0"
		      : "=rm" (*rflags)
		      : "D" (type), "S" (desc)
		      : "memory", "cc");
}

/* 66 0f 38 80 3e          invept (%esi),%edi*/
static inline void
asm_invept_and_rdrflags (enum invept_type type, struct invept_desc *desc,
			 ulong *rflags)
{
	asm volatile (".byte 0x66, 0x0F, 0x38, 0x80, 0x3E \n" /* invept */
		      PUSHF_POP " %0"
		      : "=rm" (*rflags)
		      : "D" (type), "S" (desc)
		      : "memory", "cc");
}

static inline void
asm_movss_and_vmlaunch (void)
{
	ulong tmp;

	asm volatile ("mov %%ss,%0; mov %0,%%ss; vmlaunch"
		      : "=r" (tmp)
		      :
		      : "memory", "cc");
}

static void
memread (ulong offset, unsigned int sreg, void *dst, u8 size)
{
	enum vmmerr err;

	switch (size) {
	case 4:
		err = cpu_seg_read_l (sreg, offset, dst);
		break;
	case 8:
		err = cpu_seg_read_q (sreg, offset, dst);
		break;
	case 16:
		err = cpu_seg_read_q (sreg, offset, dst);
		if (!err)
			err = cpu_seg_read_q (sreg, offset + 8, dst + 8);
		break;
	default:
		panic ("Invalid read size %u", size);
	}
	if (err)
		panic ("%s %s %d, ERRNO = %d", __FILE__, __func__, __LINE__,
		       err);
}

static void
memwrite (ulong offset, unsigned int sreg, void *src, u8 size)
{
	enum vmmerr err;

	switch (size) {
	case 4:
		err = cpu_seg_write_l (sreg, offset, *(u32 *)src);
		break;
	case 8:
		err = cpu_seg_write_q (sreg, offset, *(u64 *)src);
		break;
	default:
		panic ("Invalid write size %u", size);
	}
	if (err)
		panic ("%s %s %d, ERRNO = %d", __FILE__, __func__, __LINE__,
		       err);
}

static ulong
get_op1_offset (union instruction_info inst_info)
{
	ulong base = 0;
	ulong index = 0;
	ulong addr_mask;
	ulong displacement;
	u8 scale;

	asm_vmread (VMCS_EXIT_QUALIFICATION, &displacement);
	addr_mask = (2UL << ((16 << inst_info.s.addr_size) - 1)) - 1;
	scale = 1 << inst_info.s.scalling;
	if (!inst_info.s.basereg_invalid)
		vt_read_general_reg (inst_info.s.basereg, &base);
	if (!inst_info.s.indexreg_invalid)
		vt_read_general_reg (inst_info.s.indexreg, &index);
	return (base + (index * scale) + displacement) & addr_mask;
}

static void
read_operand1 (void *dst, u8 size, bool allow_reg)
{
	union instruction_info inst_info;
	bool is_reg;

	asm_vmread (VMCS_VMEXIT_INSTRUCTION_INFO, &inst_info.v);
	is_reg = !!inst_info.s.mem_or_reg;
	if (is_reg && !allow_reg)
		panic ("The instruction does not allow register operand."
		       " Instruction Information Register = %lX\n",
		       inst_info.v);
	if (is_reg)
		vt_read_general_reg (inst_info.s.reg1, dst);
	else
		memread (get_op1_offset (inst_info), inst_info.s.segment_reg,
			 dst, size);
}

static void
write_operand1_m64 (u64 val)
{
	union instruction_info inst_info;

	asm_vmread (VMCS_VMEXIT_INSTRUCTION_INFO, &inst_info.v);
	if (inst_info.s.mem_or_reg)
		panic ("The instruction does not allow register operand."
		       " Instruction Information Register = %lX\n",
		       inst_info.v);
	else
		memwrite (get_op1_offset (inst_info), inst_info.s.segment_reg,
			  &val, sizeof val);
}

static void
write_operand1_ulong (ulong val)
{
	union instruction_info inst_info;

	asm_vmread (VMCS_VMEXIT_INSTRUCTION_INFO, &inst_info.v);
	if (inst_info.s.mem_or_reg)
		vt_write_general_reg (inst_info.s.reg1, val);
	else
		memwrite (get_op1_offset (inst_info), inst_info.s.segment_reg,
			  &val, sizeof val);
}

static void
read_operand2 (void *dst)
{
	union instruction_info inst_info;

	asm_vmread (VMCS_VMEXIT_INSTRUCTION_INFO, &inst_info.v);
	vt_read_general_reg (inst_info.s.reg2, dst);
}

static void
set_shadow_vmcs_bmp (void *bmp_addr, u64 index, int bit)
{
	u8 *p = bmp_addr;

	index &= 0x7FFF;
	if (bit)
		p[index >> 3] |= 1 << (index & 7);
	else
		p[index >> 3] &= ~(1 << (index & 7));
}

static void
clear_shadow_vmcs_bmp (void *bmp_addr, const u16 *indexes, int n)
{
	int i;

	for (i = 0; i < n; i++)
		set_shadow_vmcs_bmp (bmp_addr, indexes[i], 0);
}

static void
init_vmread_vmwrite_bitmap (struct vmread_vmwrite_bmp *vm_rw_bmp)
{
	phys_t phys;
	void *virt;

	/* Every bit is zero, for supporting use of VMCS link pointer
	 * by the guest hypervisor */
	alloc_page (&virt, &phys);
	memset (virt, 0, PAGESIZE);
	vm_rw_bmp->bmp_pass_phys = phys;

	/* For CPUs that do not support using VMWRITE to write to
	 * read-only fields */
	alloc_page (&virt, &phys);
	memset (virt, 0xFF, PAGESIZE);
	VMCS_CLEARBITSINBMP (virt, vmcs_indexes_shadow_hs);
	VMCS_CLEARBITSINBMP (virt, vmcs_indexes_shadow_shadowing);
	vm_rw_bmp->bmp_old_phys = phys;

	/* For CPUs that support using VMWRITE to write to read-only
	 * fields */
	alloc_page (&virt, &phys);
	memset (virt, 0xFF, PAGESIZE);
	VMCS_CLEARBITSINBMP (virt, vmcs_indexes_shadow_hs);
	VMCS_CLEARBITSINBMP (virt, vmcs_indexes_shadow_shadowing);
	VMCS_CLEARBITSINBMP (virt, vmcs_indexes_shadow_readonly);
	vm_rw_bmp->bmp_new_phys = phys;
}

static struct vmread_vmwrite_bmp *
get_vmread_vmwrite_bitmap (void)
{
	static struct vmread_vmwrite_bmp *vm_rw_bmp;
	while (!vm_rw_bmp) {
		static u32 init;
		if (!asm_lock_xchgl (&init, 1)) {
			struct vmread_vmwrite_bmp *p = alloc (sizeof *p);
			init_vmread_vmwrite_bitmap (p);
			vm_rw_bmp = p;
			break;
		}
		asm_pause ();
	}
	return vm_rw_bmp;
}

static struct shadow_vt *
init_shadow_vt (void)
{
	struct shadow_vt *ret;
	struct vmread_vmwrite_bmp *vm_rw_bmp;
	int cpu;

	ret = alloc (sizeof *ret);
	cpu = currentcpu_get_id ();
	printf ("Initialize Shadow VT at CPU %d\n", cpu);

	if (current->u.vt.vmcs_shadowing_available) {
		/* Initialize VM{READ,WRITE} bitmap */
		printf ("Initialize Shadow VMCS at CPU %d\n", cpu);
		vm_rw_bmp = get_vmread_vmwrite_bitmap ();
		asm_vmwrite (VMCS_VMREAD_BMP_ADDR,  vm_rw_bmp->bmp_pass_phys);
		asm_vmwrite (VMCS_VMWRITE_BMP_ADDR, vm_rw_bmp->bmp_pass_phys);
	}
	ret->shadow_ept = NULL;
	return ret;
}

void
vt_emul_vmxon (void)
{
	ulong guest_rflags;
	ulong tmp;
	u64 op1, phys;
	u32 *p;

	if (!current->u.vt.shadow_vt)
		current->u.vt.shadow_vt = init_shadow_vt ();

	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags = (guest_rflags & ~flag_mask) | flag_fail_invalid;
	current->u.vt.shadow_vt->current_vmcs_gphys = VMCS_POINTER_INVALID;
	current->u.vt.shadow_vt->current_vmcs_hphys = VMCS_POINTER_INVALID;
	current->u.vt.shadow_vt->exint_hack_mode = EXINT_HACK_MODE_CLEARED;
	current->u.vt.shadow_vt->mode = MODE_CLEARED;
	read_operand1 (&op1, sizeof op1, false);
	if (op1 & ~current->pte_addr_mask)
		goto fail1;
	phys = current->gmm.gp2hp (op1, NULL);
	p = mapmem_hphys (phys, sizeof *p, MAPMEM_WRITE);
	if ((*p & ~(1 << 31)) != currentcpu->vt.vmcs_revision_identifier)
		goto fail2;
	if (current->u.vt.vmcs_shadowing_available) {
		*p = currentcpu->vt.vmcs_revision_identifier | (1 << 31);
		asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL2, &tmp);
		tmp |= VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VMCS_SHADOWING_BIT;
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL2, tmp);
		asm_vmwrite64 (VMCS_VMCS_LINK_POINTER, VMCS_POINTER_INVALID);
	}
	asm_vmclear (&phys);
	current->u.vt.shadow_vt->vmxon_region_phys = phys;
	current->u.vt.vmxon = true;
	guest_rflags &= ~flag_fail_invalid;
fail2:
	unmapmem (p, sizeof *p);
fail1:
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);

	add_ip ();
}

void
vt_emul_vmxon_in_vmx_root_mode (void)
{
	ulong guest_rflags, host_rflags;
	u64 op1, phys;
	u64 orig_vmcs_addr_phys;

	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	read_operand1 (&op1, sizeof op1, false);
	if (current->u.vt.shadow_vt->current_vmcs_hphys !=
	    VMCS_POINTER_INVALID) {
		/*
		 * Emulate VMfailValid ("VMXON executed in VMX root
		 * operation") To set the error number to
		 * VMCS_VM_INSTRUCTION_ERR, issues VMXON
		 */
		asm_vmptrst (&orig_vmcs_addr_phys); /* Save original
						     * VMCS addr */
		asm_vmptrld (&current->u.vt.shadow_vt->current_vmcs_hphys);
		phys = current->gmm.gp2hp (op1, NULL);
		asm_vmxon_and_rdrflags (&phys, &host_rflags);
		asm_vmptrld (&orig_vmcs_addr_phys);
		guest_rflags |= (host_rflags & flag_mask);
	} else {
		/* Emulate VMFailInvalid */
		guest_rflags |= flag_fail_invalid;
	}
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);
	add_ip ();
}

void
vt_shadow_vt_reset (void)
{
	u32 *p;
	ulong tmp;

	if (!current->u.vt.vmxon)
		return;
	current->u.vt.shadow_vt->current_vmcs_gphys = VMCS_POINTER_INVALID;
	current->u.vt.shadow_vt->current_vmcs_hphys = VMCS_POINTER_INVALID;
	asm_vmclear (&current->u.vt.shadow_vt->vmxon_region_phys);
	if (current->u.vt.vmcs_shadowing_available) {
		asm_vmwrite64 (VMCS_VMCS_LINK_POINTER, VMCS_POINTER_INVALID);
		asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL2, &tmp);
		tmp &= ~VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VMCS_SHADOWING_BIT;
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL2, tmp);
		p = mapmem_hphys (current->u.vt.shadow_vt->vmxon_region_phys,
				  sizeof *p, MAPMEM_WRITE);
		*p = currentcpu->vt.vmcs_revision_identifier;
		unmapmem (p, sizeof *p);
	}
	current->u.vt.vmxon = false;
}

void
vt_emul_vmxoff (void)
{
	ulong guest_rflags;

	vt_shadow_vt_reset ();
	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);
	add_ip ();
}

/* Return value of RFLAGS after VMCLEAR */
static ulong
do_clear_shadow_vmcs (u64 sv_gphys)
{
	u64 shadow_vmcs_addr_hphys;
	ulong rflags;

	shadow_vmcs_addr_hphys = current->gmm.gp2hp (sv_gphys, NULL);
	asm_vmclear_and_rdrflags (&shadow_vmcs_addr_hphys, &rflags);
	return rflags;
}

static void
set_link_pointer (struct shadow_vt *shadow_vt)
{
	struct vmread_vmwrite_bmp *vm_rw_bmp;

	if (!current->u.vt.vmcs_shadowing_available)
		return;
	switch (shadow_vt->mode) {
	case MODE_NORMAL:
	case MODE_CLEARED:
		asm_vmwrite64 (VMCS_VMCS_LINK_POINTER, VMCS_POINTER_INVALID);
		break;
	case MODE_SHADOWING:
		asm_vmwrite64 (VMCS_VMCS_LINK_POINTER,
			       shadow_vt->vmxon_region_phys);
		vm_rw_bmp = get_vmread_vmwrite_bitmap ();
		if (currentcpu->vt.vmcs_writable_readonly) {
			asm_vmwrite (VMCS_VMREAD_BMP_ADDR,
				     vm_rw_bmp->bmp_new_phys);
			asm_vmwrite (VMCS_VMWRITE_BMP_ADDR,
				     vm_rw_bmp->bmp_new_phys);
		} else {
			asm_vmwrite (VMCS_VMREAD_BMP_ADDR,
				     vm_rw_bmp->bmp_old_phys);
			asm_vmwrite (VMCS_VMWRITE_BMP_ADDR,
				     vm_rw_bmp->bmp_old_phys);
		}
		break;
	case MODE_NESTED_SHADOWING:
		asm_vmwrite64 (VMCS_VMCS_LINK_POINTER,
			       shadow_vt->current_vmcs_hphys);
		vm_rw_bmp = get_vmread_vmwrite_bitmap ();
		asm_vmwrite (VMCS_VMREAD_BMP_ADDR, vm_rw_bmp->bmp_pass_phys);
		asm_vmwrite (VMCS_VMWRITE_BMP_ADDR, vm_rw_bmp->bmp_pass_phys);
		break;
	}
}

static void
save_vmcs (ulong *values, const u16 *indexes, int n)
{
	int i;

	for (i = 0; i < n; i++)
		asm_vmread (indexes[i], &values[i]);
}

static void
restore_vmcs (ulong *values, const u16 *indexes, int n)
{
	int i;

	for (i = 0; i < n; i++)
		asm_vmwrite (indexes[i], values[i]);
}

static void
vmcs_shadowing_copy (u64 dst, u64 src, bool include_hs)
{
	VMCS_DEFARRAY (hs, vmcs_indexes_shadow_hs);
	VMCS_DEFARRAY (shadowing, vmcs_indexes_shadow_shadowing);
	VMCS_DEFARRAY (readonly, vmcs_indexes_shadow_readonly);

	asm_vmptrld (&src);
	if (include_hs)
		VMCS_SAVEARRAY (hs, vmcs_indexes_shadow_hs);
	VMCS_SAVEARRAY (shadowing, vmcs_indexes_shadow_shadowing);
	if (currentcpu->vt.vmcs_writable_readonly)
		VMCS_SAVEARRAY (readonly, vmcs_indexes_shadow_readonly);
	asm_vmptrld (&dst);
	if (include_hs)
		VMCS_RESTOREARRAY (hs, vmcs_indexes_shadow_hs);
	VMCS_RESTOREARRAY (shadowing, vmcs_indexes_shadow_shadowing);
	if (currentcpu->vt.vmcs_writable_readonly)
		VMCS_RESTOREARRAY (readonly, vmcs_indexes_shadow_readonly);
}

static void
clear_exint_hack (struct shadow_vt *shadow_vt, char *reason)
{
	switch (shadow_vt->exint_hack_mode) {
	case EXINT_HACK_MODE_CLEARED:
		break;
	case EXINT_HACK_MODE_SET:
		printf ("CPU%d: %s: cleared at %s, intr info 0x%lX lost\n",
			currentcpu_get_id (), __func__, reason,
			shadow_vt->exint_hack_val);
		/* Fall through */
	case EXINT_HACK_MODE_READ:
		/* The read value is expected to have been
		 * handled... */
		shadow_vt->exint_hack_mode = EXINT_HACK_MODE_CLEARED;
	}
}

static void
set_exint_hack (struct shadow_vt *shadow_vt, ulong val)
{
	shadow_vt->exint_hack_val = val;
	shadow_vt->exint_hack_mode = EXINT_HACK_MODE_SET;
}

static void
do_exint_hack (struct shadow_vt *shadow_vt, ulong *val)
{
	switch (shadow_vt->exint_hack_mode) {
	case EXINT_HACK_MODE_CLEARED:
		break;
	case EXINT_HACK_MODE_SET:
		shadow_vt->exint_hack_mode = EXINT_HACK_MODE_READ;
		/* Fall through */
	case EXINT_HACK_MODE_READ:
		*val = shadow_vt->exint_hack_val;
	}
}

void
vt_emul_vmclear (void)
{
	u64 operand;
	u64 orig_vmcs_addr_phys;
	ulong host_rflags, guest_rflags;
	struct shadow_vt *shadow_vt = current->u.vt.shadow_vt;

	read_operand1 (&operand, sizeof operand, false);
	if (shadow_vt->current_vmcs_gphys == operand) {
		clear_exint_hack (shadow_vt, "vmclear");
		if (shadow_vt->mode == MODE_SHADOWING) {
			asm_vmptrst (&orig_vmcs_addr_phys); /* Save original
							     * VMCS addr */
			vmcs_shadowing_copy (shadow_vt->current_vmcs_hphys,
					     shadow_vt->vmxon_region_phys,
					     true);
			asm_vmptrld (&orig_vmcs_addr_phys); /* Restore original
							     * VMCS addr */
		}
		shadow_vt->current_vmcs_gphys = VMCS_POINTER_INVALID;
		shadow_vt->current_vmcs_hphys = VMCS_POINTER_INVALID;
		shadow_vt->mode = MODE_CLEARED;
		set_link_pointer (shadow_vt);
		host_rflags = do_clear_shadow_vmcs (operand);
		if (host_rflags & flag_mask) /* Should not fail */
			panic ("vmclear current VMCS failed");
	} else {
		host_rflags = do_clear_shadow_vmcs (operand);
	}

	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags = (guest_rflags & ~flag_mask) | (host_rflags & flag_mask);
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);

	add_ip ();
}

static void
get_vmcs_host_states (struct vmcs_host_states *hs)
{
	ulong tmp;

	asm_vmread (VMCS_HOST_CR0, &tmp);
	hs->cr0 = tmp;
	asm_vmread (VMCS_HOST_CR3, &tmp);
	hs->cr3 = tmp;
	asm_vmread (VMCS_HOST_CR4, &tmp);
	hs->cr4 = tmp;
	asm_vmread (VMCS_HOST_RSP, &tmp);
	hs->rsp = tmp;
	asm_vmread (VMCS_HOST_RIP, &tmp);
	hs->rip = tmp;
	asm_vmread (VMCS_HOST_ES_SEL, &tmp);
	hs->es_sel = tmp;
	asm_vmread (VMCS_HOST_CS_SEL, &tmp);
	hs->cs_sel = tmp;
	asm_vmread (VMCS_HOST_SS_SEL, &tmp);
	hs->ss_sel = tmp;
	asm_vmread (VMCS_HOST_DS_SEL, &tmp);
	hs->ds_sel = tmp;
	asm_vmread (VMCS_HOST_FS_SEL, &tmp);
	hs->fs_sel = tmp;
	asm_vmread (VMCS_HOST_GS_SEL, &tmp);
	hs->gs_sel = tmp;
	asm_vmread (VMCS_HOST_TR_SEL, &tmp);
	hs->tr_sel = tmp;
	asm_vmread (VMCS_HOST_FS_BASE, &tmp);
	hs->fs_base = tmp;
	asm_vmread (VMCS_HOST_GS_BASE, &tmp);
	hs->gs_base = tmp;
	asm_vmread (VMCS_HOST_TR_BASE, &tmp);
	hs->tr_base = tmp;
	asm_vmread (VMCS_HOST_GDTR_BASE, &tmp);
	hs->gdtr_base = tmp;
	asm_vmread (VMCS_HOST_IDTR_BASE, &tmp);
	hs->idtr_base = tmp;
	asm_vmread (VMCS_HOST_IA32_SYSENTER_CS, &tmp);
	hs->ia32_sysenter_cs = tmp;
	asm_vmread (VMCS_HOST_IA32_SYSENTER_ESP, &tmp);
	hs->ia32_sysenter_esp = tmp;
	asm_vmread (VMCS_HOST_IA32_SYSENTER_EIP, &tmp);
	hs->ia32_sysenter_eip = tmp;
	asm_vmread (VMCS_HOST_IA32_PAT, &tmp);
	hs->ia32_pat = tmp;
}

static void
set_vmcs_host_states (struct vmcs_host_states *hs)
{
	ulong tmp;

	tmp = hs->cr0;
	asm_vmwrite (VMCS_HOST_CR0, tmp);
	tmp = hs->cr3;
	asm_vmwrite (VMCS_HOST_CR3, tmp);
	tmp = hs->cr4;
	asm_vmwrite (VMCS_HOST_CR4, tmp);
	tmp = hs->rsp;
	asm_vmwrite (VMCS_HOST_RSP, tmp);
	tmp = hs->rip;
	asm_vmwrite (VMCS_HOST_RIP, tmp);
	tmp = hs->es_sel;
	asm_vmwrite (VMCS_HOST_ES_SEL, tmp);
	tmp = hs->cs_sel;
	asm_vmwrite (VMCS_HOST_CS_SEL, tmp);
	tmp = hs->ss_sel;
	asm_vmwrite (VMCS_HOST_SS_SEL, tmp);
	tmp = hs->ds_sel;
	asm_vmwrite (VMCS_HOST_DS_SEL, tmp);
	tmp = hs->fs_sel;
	asm_vmwrite (VMCS_HOST_FS_SEL, tmp);
	tmp = hs->gs_sel;
	asm_vmwrite (VMCS_HOST_GS_SEL, tmp);
	tmp = hs->tr_sel;
	asm_vmwrite (VMCS_HOST_TR_SEL, tmp);
	tmp = hs->fs_base;
	asm_vmwrite (VMCS_HOST_FS_BASE, tmp);
	tmp = hs->gs_base;
	asm_vmwrite (VMCS_HOST_GS_BASE, tmp);
	tmp = hs->tr_base;
	asm_vmwrite (VMCS_HOST_TR_BASE, tmp);
	tmp = hs->gdtr_base;
	asm_vmwrite (VMCS_HOST_GDTR_BASE, tmp);
	tmp = hs->idtr_base;
	asm_vmwrite (VMCS_HOST_IDTR_BASE, tmp);
	tmp = hs->ia32_sysenter_cs;
	asm_vmwrite (VMCS_HOST_IA32_SYSENTER_CS, tmp);
	tmp = hs->ia32_sysenter_esp;
	asm_vmwrite (VMCS_HOST_IA32_SYSENTER_ESP, tmp);
	tmp = hs->ia32_sysenter_eip;
	asm_vmwrite (VMCS_HOST_IA32_SYSENTER_EIP, tmp);
	tmp = hs->ia32_pat;
	asm_vmwrite (VMCS_HOST_IA32_PAT, tmp);
}

static enum vmcs_mode
choose_vmcs_mode (u64 shadow_vmcs_addr_hphys)
{
	u32 indicator, *p;

	if (!current->u.vt.vmcs_shadowing_available)
		return MODE_NORMAL;
	p = mapmem_hphys (shadow_vmcs_addr_hphys, sizeof *p, 0);
	indicator = *p;
	unmapmem (p, sizeof *p);
	if (indicator & INDICATOR_SHADOWING_BIT)
		return MODE_NESTED_SHADOWING;
	return MODE_SHADOWING;
}

static struct shadow_ept_data *
shadow_ept_init (void)
{
	struct shadow_ept_data *d = alloc (sizeof *d);
	LIST3_HEAD_INIT (d->list_ept, list);
	for (int i = 0; i < NUM_OF_SHADOW_EPT; i++) {
		d->ept[i].shadow_ept = NULL;
		d->ept[i].ep4ta = 0;
		d->ept[i].active = 0;
		LIST3_ADD (d->list_ept, list, &d->ept[i]);
	}
	LIST3_HEAD_INIT (d->list_vpid, list);
	for (int i = 0; i < NUM_OF_SHADOW_VPID; i++) {
		d->vpid[i].ep4ta = 0;
		d->vpid[i].vpid = 0;
		LIST3_ADD (d->list_vpid, list, &d->vpid[i]);
	}
	return d;
}

static void
shadow_ept_clear_ept (struct shadow_vt *shadow_vt, u64 guest_eptp)
{
	struct shadow_ept_data *d = shadow_vt->shadow_ept;
	if (!d)
		return;
	if (guest_eptp) {
		guest_eptp &= 0xFFFFFFFFFF000ULL;
		guest_eptp |= 1; /* Internal flag */
	}
	struct shadow_ept_ept_info *p;
	LIST3_FOREACH (d->list_ept, list, p) {
		if (!guest_eptp || p->ep4ta == guest_eptp) {
			if (p->shadow_ept && p->active) {
				vt_ept_clear (p->shadow_ept);
				p->active = 0;
			}
			if (guest_eptp)
				break;
		}
	}
}

static struct shadow_ept_ept_info *
shadow_ept_get_ept_info (struct shadow_vt *shadow_vt, u64 guest_eptp)
{
	struct shadow_ept_data *d = shadow_vt->shadow_ept;
	if (!d)
		shadow_vt->shadow_ept = d = shadow_ept_init ();
	guest_eptp &= 0xFFFFFFFFFF000ULL;
	guest_eptp |= 1;	/* Internal flag */
	struct shadow_ept_ept_info *p;
	struct shadow_ept_ept_info *last = NULL;
	LIST3_FOREACH (d->list_ept, list, p) {
		if (p->ep4ta == guest_eptp)
			goto found;
		last = p;
	}
	p = last;
	if (p->shadow_ept) {
		if (p->active) {
			vt_ept_clear (p->shadow_ept);
			p->active = 0;
		}
	} else {
		p->shadow_ept = vt_ept_new (16);
	}
	p->ep4ta = guest_eptp;
found:
	if (LIST3_HEAD (d->list_ept, list) != p) {
		LIST3_DEL (d->list_ept, list, p);
		LIST3_PUSH (d->list_ept, list, p);
	}
	return p;
}

static u16
shadow_ept_real_vpid (struct shadow_ept_data *d,
		      struct shadow_ept_vpid_info *p)
{
	/* Use a different VPID from one in current->vpid to avoid
	 * unnecessary invalidation. */
	return (p - d->vpid) + 0x10;
}

static void
shadow_ept_invalidate_vpid (struct shadow_ept_data *d,
			    struct shadow_ept_vpid_info *p)
{
	struct invvpid_desc desc = {
		.vpid = shadow_ept_real_vpid (d, p),
	};
	asm_invvpid (INVVPID_TYPE_SINGLE_CONTEXT, &desc);
}

/* The guest might use invvpid for multiple virtual machines that
 * share same VPID with different EP4TA.  The shadow EPT may use same
 * real EP4TA for such different EP4TA, so real VPID is assigned
 * differently from the guest VPID.  This function searches a VPID
 * list for the invvpid descriptor VPID.  If one entry found, replaces
 * desc->vpid to a real one.  If multiple entries found, replaces
 * desc->vpid to one of the real VPIDs, and does invvpid for others.
 * When this function does invvpid, no errors are checked. */
static void
shadow_ept_invvpid (struct shadow_vt *shadow_vt, enum invvpid_type type,
		    struct invvpid_desc *desc)
{
	struct shadow_ept_data *d = shadow_vt->shadow_ept;
	if (!d)
		return;
	u16 guest_vpid = desc->vpid;
	int first = 1;
	struct shadow_ept_vpid_info *p;
	LIST3_FOREACH (d->list_vpid, list, p) {
		if (p->vpid == guest_vpid) {
			if (!first)
				asm_invvpid (type, desc);
			first = 0;
			desc->vpid = shadow_ept_real_vpid (d, p);
		}
	}
}

static struct shadow_ept_vpid_info *
shadow_ept_get_vpid_info (struct shadow_vt *shadow_vt, u64 guest_eptp,
			  u16 guest_vpid)
{
	struct shadow_ept_data *d = shadow_vt->shadow_ept;
	if (!d)
		shadow_vt->shadow_ept = d = shadow_ept_init ();
	guest_eptp &= 0xFFFFFFFFFF000ULL;
	guest_eptp |= 1;	/* Internal flag */
	struct shadow_ept_vpid_info *p;
	struct shadow_ept_vpid_info *last = NULL;
	LIST3_FOREACH (d->list_vpid, list, p) {
		if (p->ep4ta == guest_eptp && p->vpid == guest_vpid)
			goto found;
		last = p;
	}
	p = last;
	shadow_ept_invalidate_vpid (d, p);
	p->ep4ta = guest_eptp;
	p->vpid = guest_vpid;
found:
	if (LIST3_HEAD (d->list_vpid, list) != p) {
		LIST3_DEL (d->list_vpid, list, p);
		LIST3_PUSH (d->list_vpid, list, p);
	}
	return p;
}

static void
shadow_ept_activate_and_vmwrite (struct shadow_vt *shadow_vt,
				 struct shadow_ept_ept_info *ept,
				 struct shadow_ept_vpid_info *vpid)
{
	struct shadow_ept_data *d = shadow_vt->shadow_ept;
	ept->active = 1;
	asm_vmwrite64 (VMCS_EPT_POINTER, vt_ept_get_eptp (ept->shadow_ept));
	if (vpid)
		asm_vmwrite (VMCS_VPID, shadow_ept_real_vpid (d, vpid));
}

static struct vt_ept *
shadow_ept_get_sept (struct shadow_vt *shadow_vt,
		     struct shadow_ept_ept_info *ept)
{
	return ept->shadow_ept;
}

/* load_shadow_vmcs_ptr (void) */
void
vt_emul_vmptrld (void)
{
	enum vmcs_mode mode;
	u64 operand;
	u64 shadow_vmcs_addr_hphys;
	u64 orig_vmcs_addr_phys;
	ulong host_rflags, guest_rflags;
	struct vmcs_host_states hsl01;
	struct shadow_vt *shadow_vt = current->u.vt.shadow_vt;

	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	read_operand1 (&operand, sizeof operand, false);
	shadow_vmcs_addr_hphys = current->gmm.gp2hp (operand, NULL);

	const u64 cvg = shadow_vt->current_vmcs_gphys;
	if (cvg == operand)	/* Fast path */
		goto end;

	asm_vmptrst (&orig_vmcs_addr_phys); /* Save original VMCS addr */

	/* Try VMPTRLD for shadow VMCS addr */
	if (cvg != VMCS_POINTER_INVALID) {
		/* Errors should be saved to the current VMCS */
		asm_vmptrld (&shadow_vt->current_vmcs_hphys);
	}
	asm_vmptrld_and_rdrflags (&shadow_vmcs_addr_hphys, &host_rflags);
	if (host_rflags & flag_mask) {
		asm_vmptrld (&orig_vmcs_addr_phys); /* Restore
						     * original VMCS
						     * addr */
		if (cvg == VMCS_POINTER_INVALID) {
			guest_rflags |= flag_fail_invalid;
			goto end;
		}
		guest_rflags |= host_rflags & flag_mask;
		goto end;
	}
	clear_exint_hack (shadow_vt, "vmptrld");
	if (cvg != VMCS_POINTER_INVALID && shadow_vt->mode == MODE_SHADOWING)
		vmcs_shadowing_copy (shadow_vt->current_vmcs_hphys,
				     shadow_vt->vmxon_region_phys, true);
	mode = choose_vmcs_mode (shadow_vmcs_addr_hphys);
	if (mode == MODE_SHADOWING) {
		vmcs_shadowing_copy (shadow_vt->vmxon_region_phys,
				     shadow_vmcs_addr_hphys, true);
		asm_vmptrld (&orig_vmcs_addr_phys);
		get_vmcs_host_states (&hsl01);
		asm_vmptrld (&shadow_vmcs_addr_hphys);
		set_vmcs_host_states (&hsl01);
	}
	asm_vmptrld (&orig_vmcs_addr_phys); /* Restore original VMCS addr */
	shadow_vt->current_vmcs_gphys = operand;
	shadow_vt->current_vmcs_hphys = shadow_vmcs_addr_hphys;
	shadow_vt->mode = mode;
	set_link_pointer (shadow_vt);
end:
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);

	add_ip ();
}

void
vt_emul_vmptrst (void)
{
	ulong guest_rflags;

	write_operand1_m64 (current->u.vt.shadow_vt->current_vmcs_gphys);
	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);
	add_ip ();
}

void
vt_emul_invept (void)
{
	u64 orig_vmcs_addr_phys;
	ulong guest_rflags, host_rflags;
	struct invept_desc desc;
	ulong type;
	struct shadow_vt *shadow_vt = current->u.vt.shadow_vt;

	read_operand1 (&desc, sizeof desc, false);
	read_operand2 (&type);
	if (type != INVEPT_TYPE_SINGLE_CONTEXT)
		shadow_ept_clear_ept (shadow_vt, 0);
	else
		shadow_ept_clear_ept (shadow_vt, desc.eptp);

	asm_vmptrst (&orig_vmcs_addr_phys); /* Save original VMCS addr */
	asm_vmptrld (&shadow_vt->current_vmcs_hphys);
	asm_invept_and_rdrflags (type, &desc, &host_rflags);
	asm_vmptrld (&orig_vmcs_addr_phys);
	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	guest_rflags |= (host_rflags & flag_mask);
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);

	add_ip ();
}

void
vt_emul_invvpid (void)
{
	u64 orig_vmcs_addr_phys;
	ulong guest_rflags, host_rflags;
	struct invvpid_desc desc;
	ulong type;
	struct shadow_vt *shadow_vt = current->u.vt.shadow_vt;

	read_operand1 (&desc, sizeof desc, false);
	read_operand2 (&type);
	if (type != INVVPID_TYPE_ALL_CONTEXTS)
		shadow_ept_invvpid (shadow_vt, type, &desc);

	asm_vmptrst (&orig_vmcs_addr_phys); /* Save original VMCS addr */
	asm_vmptrld (&shadow_vt->current_vmcs_hphys);
	asm_invvpid_and_rdrflags (type, &desc, &host_rflags);
	asm_vmptrld (&orig_vmcs_addr_phys);
	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	guest_rflags |= (host_rflags & flag_mask);
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);

	add_ip ();
}

void
vt_emul_vmwrite (void)
{
	ulong index, val;
	u64 orig_vmcs_addr_phys;
	ulong host_rflags, guest_rflags;

	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	if (current->u.vt.shadow_vt->current_vmcs_gphys ==
	    VMCS_POINTER_INVALID) {
		guest_rflags |= flag_fail_invalid;
		goto fail;
	}
	read_operand1 (&val, sizeof val, true);
	read_operand2 (&index);
	asm_vmptrst (&orig_vmcs_addr_phys); /* Save original VMCS
					     * addr */
	asm_vmptrld (&current->u.vt.shadow_vt->current_vmcs_hphys);
	asm_vmwrite_and_rdrflags (index, val, &host_rflags);
	asm_vmptrld (&orig_vmcs_addr_phys); /* Restore original VMCS
					     * addr */
	guest_rflags |= host_rflags & flag_mask;
fail:
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);

	add_ip ();
}

void
vt_emul_vmread (void)
{
	ulong index;
	ulong val;
	u64 orig_vmcs_addr_phys;
	ulong host_rflags, guest_rflags;
	struct shadow_vt *shadow_vt = current->u.vt.shadow_vt;

	asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
	guest_rflags &= ~flag_mask;
	if (shadow_vt->current_vmcs_gphys == VMCS_POINTER_INVALID) {
		guest_rflags |= flag_fail_invalid;
		goto fail;
	}
	read_operand2 (&index);
	asm_vmptrst (&orig_vmcs_addr_phys); /* Save original VMCS
					     * addr */
	asm_vmptrld (&shadow_vt->current_vmcs_hphys);
	asm_vmread_and_rdrflags (index, &val, &host_rflags);
	guest_rflags |= (host_rflags & flag_mask);
	asm_vmptrld (&orig_vmcs_addr_phys); /* Restore original VMCS
					     * addr */
	if (!(guest_rflags & flag_mask)) {
		if (index == VMCS_VMEXIT_INTR_INFO)
			do_exint_hack (shadow_vt, &val);
		write_operand1_ulong (val);
	}
fail:
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);

	add_ip ();
}

static ulong
get_code_acr (bool to_64bit_mode, u16 sel)
{
	ulong l_or_db = to_64bit_mode ?
		ACCESS_RIGHTS_L_BIT : ACCESS_RIGHTS_D_B_BIT;
	return SEGDESC_TYPE_EXECREAD_CODE_A | ACCESS_RIGHTS_S_BIT |
		ACCESS_RIGHTS_P_BIT | l_or_db |
		ACCESS_RIGHTS_G_BIT;
}

static ulong
get_data_acr (u16 sel)
{
	if (!sel)
		return ACCESS_RIGHTS_UNUSABLE_BIT;
	return SEGDESC_TYPE_RDWR_DATA_A | ACCESS_RIGHTS_S_BIT |
		ACCESS_RIGHTS_P_BIT | ACCESS_RIGHTS_D_B_BIT |
		ACCESS_RIGHTS_G_BIT;
}

static void
switch_vmcs_and_load_l1_host_state (u64 orig_vmcs_addr_phys, ulong efer_l2,
				    ulong exit_ctl,
				    struct vmcs_host_states *hs)
{
	/*
	 * Restore host state from shadow VMCS to guest state of L1-L0 VMCS.
	 * According to
	 * Section 27.5.1 on Intel SDM
	 * (Order Number: 326019-048US, September 2013)
	 */
	bool to_64bit_mode;
	ulong cr0, cr3, cr4;
	ulong ccr0, ccr3, ccr4;
	const ulong cr0_not_reserved = CR0_PE_BIT | CR0_MP_BIT | CR0_EM_BIT |
		CR0_TS_BIT | CR0_ET_BIT | CR0_NE_BIT | CR0_WP_BIT |
		CR0_AM_BIT | CR0_NW_BIT | CR0_CD_BIT | CR0_PG_BIT;
	ulong cr0_mask = CR0_ET_BIT | CR0_CD_BIT | CR0_NW_BIT | CR0_NE_BIT |
		~cr0_not_reserved;
	ulong cr3_mask = ~(current->pte_addr_mask | PAGESIZE_MASK);
	ulong cr4_mask = CR4_VMXE_BIT;
	u64 guest_efer;
	u64 cguest_efer;
	u64 cguest_pat;
	const u64 efer_mask = MSR_IA32_EFER_LME_BIT | MSR_IA32_EFER_LMA_BIT;
	ulong exec_ctl;

	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL2, &exec_ctl);

	/* Switch VMCS */
	asm_vmptrld (&orig_vmcs_addr_phys);

	guest_efer = efer_l2;

	if (!(exec_ctl & VMCS_PROC_BASED_VMEXEC_CTL2_UNRESTRICTED_GUEST_BIT)) {
		/* L2 guest is restricted */
		cr0_mask |= (CR0_PE_BIT | CR0_PG_BIT);
	}

	vt_read_control_reg (CONTROL_REG_CR0, &cr0);
	vt_read_control_reg (CONTROL_REG_CR3, &cr3);
	vt_read_control_reg (CONTROL_REG_CR4, &cr4);
	ccr0 = cr0;
	ccr3 = cr3;
	ccr4 = cr4;
	cr0 = (cr0 & cr0_mask) | (hs->cr0 & ~cr0_mask);
	cr3 = (cr3 & cr3_mask) | (hs->cr3 & ~cr3_mask);
	cr4 = (cr4 & cr4_mask) | (hs->cr4 & ~cr4_mask);

	to_64bit_mode = !!(exit_ctl &
			   VMCS_VMEXIT_CTL_HOST_ADDRESS_SPACE_SIZE_BIT);
#ifndef __x86_64__
	if (to_64bit_mode)
		panic ("Switching to long mode is not allowed");
#endif
	if (to_64bit_mode) {
		cr4 = hs->cr4 | CR4_PAE_BIT;
		guest_efer |= efer_mask;
	} else {
		cr4 = hs->cr4 & ~CR4_PCIDE_BIT;
		guest_efer &= ~efer_mask;
	}

	if (ccr0 != cr0)
		vt_write_control_reg (CONTROL_REG_CR0, cr0);
	if (ccr4 != cr4)
		vt_write_control_reg (CONTROL_REG_CR4, cr4);
	asm_vmwrite (VMCS_GUEST_DR7, 0x400);
	vt_write_msr (MSR_IA32_DEBUGCTL, 0x0);
	vt_write_msr (MSR_IA32_SYSENTER_CS, hs->ia32_sysenter_cs);
	vt_write_msr (MSR_IA32_SYSENTER_ESP, hs->ia32_sysenter_esp);
	vt_write_msr (MSR_IA32_SYSENTER_EIP, hs->ia32_sysenter_eip);
	if (exit_ctl & VMCS_VMEXIT_CTL_LOAD_IA32_PERF_GLOBAL_CTRL_BIT)
		vt_write_msr (MSR_IA32_PERF_GLOBAL_CTRL,
			      hs->ia32_perf_global_ctrl);
	if (exit_ctl & VMCS_VMEXIT_CTL_LOAD_IA32_PAT_BIT) {
		vt_read_msr (MSR_IA32_PAT, &cguest_pat);
		if (cguest_pat != hs->ia32_pat)
			vt_write_msr (MSR_IA32_PAT, hs->ia32_pat);
	}
	vt_read_msr (MSR_IA32_EFER, &cguest_efer);
	if (cguest_efer != guest_efer)
		vt_write_msr (MSR_IA32_EFER, guest_efer);

	/* CR3 depends on CR0, CR4 and EFER, so load after them */
	/* Loading CR3 invalidates TLBs.  If CR3 is not changed, avoid
	 * invalidation for performance, in case of enable VPID=1.  If
	 * enable VPID=0, VM exit invalidates TLBs. */
	if (ccr3 != cr3)
		vt_write_control_reg (CONTROL_REG_CR3, cr3);

	/* Selector */
	asm_vmwrite (VMCS_GUEST_ES_SEL, hs->es_sel);
	asm_vmwrite (VMCS_GUEST_CS_SEL, hs->cs_sel);
	asm_vmwrite (VMCS_GUEST_SS_SEL, hs->ss_sel);
	asm_vmwrite (VMCS_GUEST_DS_SEL, hs->ds_sel);
	asm_vmwrite (VMCS_GUEST_FS_SEL, hs->fs_sel);
	asm_vmwrite (VMCS_GUEST_GS_SEL, hs->gs_sel);
	asm_vmwrite (VMCS_GUEST_TR_SEL, hs->tr_sel);

	/* Base address */
	asm_vmwrite (VMCS_GUEST_ES_BASE, 0x0);
	asm_vmwrite (VMCS_GUEST_CS_BASE, 0x0);
	asm_vmwrite (VMCS_GUEST_SS_BASE, 0x0);
	asm_vmwrite (VMCS_GUEST_DS_BASE, 0x0);
	asm_vmwrite (VMCS_GUEST_FS_BASE, hs->fs_base);
	asm_vmwrite (VMCS_GUEST_GS_BASE, hs->gs_base);
	asm_vmwrite (VMCS_GUEST_TR_BASE, hs->tr_base);

	/* Segment limit */
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, 0xFFFFFFFF);
	asm_vmwrite (VMCS_GUEST_TR_LIMIT, 0x67);

	/* Access rights */
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, get_data_acr (hs->es_sel));
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, get_code_acr (to_64bit_mode,
								hs->cs_sel));
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, get_data_acr (hs->ss_sel));
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, get_data_acr (hs->ds_sel));
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, get_data_acr (hs->fs_sel));
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, get_data_acr (hs->gs_sel));
	asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS,
		     SEGDESC_TYPE_32BIT_TSS_BUSY | ACCESS_RIGHTS_P_BIT);

	/* LDTR */
	asm_vmwrite (VMCS_GUEST_LDTR_SEL, 0x0);
	asm_vmwrite (VMCS_GUEST_LDTR_ACCESS_RIGHTS,
		     ACCESS_RIGHTS_UNUSABLE_BIT);

	/* GDTR */
	asm_vmwrite (VMCS_GUEST_GDTR_BASE, hs->gdtr_base);
	asm_vmwrite (VMCS_GUEST_GDTR_LIMIT, 0xFFFF);

	/* IDTR */
	asm_vmwrite (VMCS_GUEST_IDTR_BASE, hs->idtr_base);
	asm_vmwrite (VMCS_GUEST_IDTR_LIMIT, 0xFFFF);

	/* RIP, RSP */
	asm_vmwrite (VMCS_GUEST_RIP, hs->rip);
	asm_vmwrite (VMCS_GUEST_RSP, hs->rsp);

	/* RFLAGS */
	asm_vmwrite (VMCS_GUEST_RFLAGS, 0x2);
}

static void
handle_acked_exint_pass (struct shadow_vt *shadow_vt)
{
	ulong exit_reason;
	union {
		struct intr_info s;
		ulong v;
	} vii;
	int num;

	/* Return if the exit reason is not an external interrupt */
	asm_vmread (VMCS_EXIT_REASON, &exit_reason);
	if ((exit_reason & EXIT_REASON_MASK) != EXIT_REASON_EXTERNAL_INT)
		return;
	if (exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)
		return;

	/* Return if the VM-exit interruption information is not valid
	 * or is not an external interrupt */
	asm_vmread (VMCS_VMEXIT_INTR_INFO, &vii.v);
	if (vii.s.valid != INTR_INFO_VALID_VALID)
		return;
	if (vii.s.type != INTR_INFO_TYPE_EXTERNAL)
		return;

	/* Handle the external interrupt and return if the vector
	 * number is not needed to be changed */
	num = exint_pass_intr_call (vii.s.vector);
	if (num == vii.s.vector)
		return;

	/* Vector number conversion.  Note: changing the interruption
	 * information to invalid might cause troubles... */
	if (num >= 0)
		vii.s.vector = num;
	else
		vii.s.valid = INTR_INFO_VALID_INVALID;

	/* If the interruption information is writable, simply modify
	 * the field. */
	if (currentcpu->vt.vmcs_writable_readonly) {
		asm_vmwrite (VMCS_VMEXIT_INTR_INFO, vii.v);
		return;
	}

	/* exint_hack: Because the interrupt is already acknowledged,
	 * the interruption information field is expected to be read
	 * soon.  Keeping the modified value until next
	 * vmptrld/vmclear/vmlaunch/vmresume should work... */
	set_exint_hack (shadow_vt, vii.v);
}

static bool
handle_ept_shadow (struct vt_ept *sept, u64 guest_eptp)
{
	ulong exit_reason;
	asm_vmread (VMCS_EXIT_REASON, &exit_reason);
	if (exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)
		return false;
	u64 gp;
	switch (exit_reason & EXIT_REASON_MASK) {
	case EXIT_REASON_EPT_MISCONFIG:
		/* Misconfigured entries must not be cached. */
		asm_vmread64 (VMCS_GUEST_PHYSICAL_ADDRESS, &gp);
		vt_ept_shadow_invalidate (sept, gp);
		/* Fall through */
	default:
		return false;
	case EXIT_REASON_EPT_VIOLATION:
		break;
	}

	ulong eqe;
	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqe);
	asm_vmread64 (VMCS_GUEST_PHYSICAL_ADDRESS, &gp);

	/* If the page is already mapped, invalidate the page and
	 * return EPT violation for read access.  Since write access
	 * possibly hits VMM address protection, it will be checked
	 * later. */
	bool write_check = false;
	if (eqe & (EPT_VIOLATION_EXIT_QUAL_E_READ_BIT |
		   EPT_VIOLATION_EXIT_QUAL_E_WRITE_BIT |
		   EPT_VIOLATION_EXIT_QUAL_E_EXEC_BIT)) {
		vt_ept_shadow_invalidate (sept, gp);
		if (!(eqe & EPT_VIOLATION_EXIT_QUAL_WRITE_BIT))
			return false;
		write_check = true;
	}

	/* Read guest EPT. */
	u64 mask = current->pte_addr_mask;
	u64 ge;
	int gl = vt_ept_read_epte (current->as, mask, guest_eptp, gp, &ge);

	if (write_check) {
		/* If the exit qualification matches the entry, return
		 * EFI violation. */
		if (!!(ge & 1) ==
		    !!(eqe & EPT_VIOLATION_EXIT_QUAL_E_READ_BIT) &&
		    !!(ge & 2) ==
		    !!(eqe & EPT_VIOLATION_EXIT_QUAL_E_WRITE_BIT) &&
		    !!(ge & 4) ==
		    !!(eqe & EPT_VIOLATION_EXIT_QUAL_E_EXEC_BIT))
			return false;
		/* If the entry is not present and the exit
		 * qualification is different, the entry has already
		 * been cleared above.  In this case, the next VM exit
		 * may have a proper exit qualification. */
		if (!(ge & 7))
			goto ret_true;
	} else {
		/* If nothing has mapped and cached, simply return EPT
		 * violation. */
		if (!(ge & 7))
			return false;
		/* Fast path for a simple misconfigured case on a
		 * processor that can vmwrite to exit reason field.
		 * Reserved bits are treated by CPU - mapped once,
		 * then EPT misconfiguration VM exit will occur. */
		if ((ge & 3) == 2 && currentcpu->vt.vmcs_writable_readonly) {
			asm_vmwrite (VMCS_EXIT_REASON,
				     EXIT_REASON_EPT_MISCONFIG);
			return false;
		}
	}

	/* Write shadow EPT. */
	u64 hentry = vt_ept_shadow_write (sept, mask, gp, gl, ge);
	if (write_check) {
		if (!(hentry & 2) && (ge & 2))
			panic ("%s: Writing to VMM memory.", __func__);
	}

ret_true:
	/* NMI unblocking workaround before the next VM entry. */
	if (eqe & EPT_VIOLATION_EXIT_QUAL_NMI_UNBLOCKING_DUE_TO_IRET_BIT) {
		ulong is;
		asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
		is |= VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_NMI_BIT;
		asm_vmwrite (VMCS_GUEST_INTERRUPTIBILITY_STATE, is);
	}
	return true;
}

static bool
handle_ept_host (void)
{
	/* While a guest does not activate EPT, EPT violation and EPT
	 * misconfig must not be delivered to the guest as a VM
	 * exit. */
	ulong exit_reason;
	asm_vmread (VMCS_EXIT_REASON, &exit_reason);
	if (exit_reason & EXIT_REASON_VMENTRY_FAILURE_BIT)
		return false;
	switch (exit_reason & EXIT_REASON_MASK) {
	case EXIT_REASON_EPT_MISCONFIG:
		panic ("Unexpected EPT misconfig"
		       " during nested virtualization");
	case EXIT_REASON_EPT_VIOLATION:
		break;
	default:
		return false;
	}

	ulong eqe;
	u64 gp;
	asm_vmread (VMCS_EXIT_QUALIFICATION, &eqe);
	asm_vmread64 (VMCS_GUEST_PHYSICAL_ADDRESS, &gp);
	if (eqe & EPT_VIOLATION_EXIT_QUAL_NMI_UNBLOCKING_DUE_TO_IRET_BIT) {
		ulong is;
		asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
		is |= VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_NMI_BIT;
		asm_vmwrite (VMCS_GUEST_INTERRUPTIBILITY_STATE, is);
	}
	if (vt_paging_npf (!!(eqe & EPT_VIOLATION_EXIT_QUAL_WRITE_BIT), gp,
			   false))
		panic ("Instruction emulation for nested virtualization"
		       " not supported");
	return true;
}

static bool
handle_ept (struct shadow_vt *shadow_vt, struct shadow_ept_ept_info *ept,
	    u64 guest_eptp)
{
	if (!ept) {
		return handle_ept_host ();
	} else {
		struct vt_ept *sept = shadow_ept_get_sept (shadow_vt, ept);
		return handle_ept_shadow (sept, guest_eptp);
	}
}

/* If EPT violation occurs during event delivery, the event will be
 * injected on the next VM entry.  Note that the guest hypervisor will
 * not know about the EPT violation and the event injection in this
 * case. */
static void
handle_ept_violation_event_delivery (void)
{
	union {
		struct intr_info s;
		ulong v;
	} ivif;
	asm_vmread (VMCS_IDT_VECTORING_INFO_FIELD, &ivif.v);
	/* The valid bit in VMCS_VMENTRY_INTR_INFO_FIELD is always
	 * cleared on VM exit.  If the VMCS_IDT_VECTORING_INFO_FIELD
	 * is not valid, do nothing. */
	if (ivif.s.valid != INTR_INFO_VALID_VALID)
		return;
	/* Copy fields for event injection. */
	asm_vmwrite (VMCS_VMENTRY_INTR_INFO_FIELD, ivif.v);
	ulong tmp;
	if (ivif.s.err == INTR_INFO_ERR_VALID) {
		asm_vmread (VMCS_VMEXIT_INTR_ERRCODE, &tmp);
		asm_vmwrite (VMCS_VMENTRY_EXCEPTION_ERRCODE, tmp);
	}
	asm_vmread (VMCS_VMEXIT_INSTRUCTION_LEN, &tmp);
	asm_vmwrite (VMCS_VMENTRY_INSTRUCTION_LEN, tmp);
	/* Unblock NMI if the event is NMI. */
	if (ivif.s.type == INTR_INFO_TYPE_NMI) {
		ulong is;
		asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
		is &= ~VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_NMI_BIT;
		asm_vmwrite (VMCS_GUEST_INTERRUPTIBILITY_STATE, is);
	}
}

static void
run_l2vm (bool vmlaunched)
{
	int entry_err;
	u64 orig_vmcs_addr_phys;
	u64 efer_l1, efer_l2, saved_host_efer;
	ulong is, procbased_ctls, procbased_ctls2;
	ulong orig_procbased_ctls, orig_procbased_ctls2;
	ulong guest_rflags;
	ulong exit_ctl01, exit_ctl02;
	struct vmcs_host_states hsl01, hsl02;
	struct shadow_vt *shadow_vt = current->u.vt.shadow_vt;

	if (shadow_vt->current_vmcs_gphys == VMCS_POINTER_INVALID) {
		entry_err = -2;
		goto vmfail;
	}
	if (vt_read_msr (MSR_IA32_EFER, &efer_l1))
		panic ("Reading EFER failed"); /* Never happen */
	asm_vmread (VMCS_GUEST_INTERRUPTIBILITY_STATE, &is);
	asm_vmread (VMCS_VMEXIT_CTL, &exit_ctl01);
	u64 host_eptp = 0;
	if (config.vmm.unsafe_nested_virtualization == 2) {
		/* Unrestricted guest must be activated to ensure that
		 * EPT is used and VPID bit will not be modified
		 * below. */
		if (!current->u.vt.unrestricted_guest)
			panic ("Unrestricted guest not supported");
		asm_vmread64 (VMCS_EPT_POINTER, &host_eptp);
	}
	asm_vmptrst (&orig_vmcs_addr_phys); /* Save original VMCS addr */
	if (is & VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCKING_BY_MOV_SS_BIT) {
		asm_vmptrld (&shadow_vt->current_vmcs_hphys);
		asm_movss_and_vmlaunch ();
		entry_err = -1;
		asm_vmptrld (&orig_vmcs_addr_phys);
		goto vmfail;
	}
	if (shadow_vt->mode == MODE_SHADOWING) {
		vmcs_shadowing_copy (shadow_vt->current_vmcs_hphys,
				     shadow_vt->vmxon_region_phys, false);
		asm_vmptrld (&shadow_vt->vmxon_region_phys);
		get_vmcs_host_states (&hsl02);
		asm_vmptrld (&shadow_vt->current_vmcs_hphys);
	} else {
		get_vmcs_host_states (&hsl01);
		asm_vmptrld (&shadow_vt->current_vmcs_hphys);
		/* Update host states */
		get_vmcs_host_states (&hsl02);
		set_vmcs_host_states (&hsl01);
	}

	/* Update VMCS_VMEXIT_CTL */
	asm_vmread (VMCS_VMEXIT_CTL, &exit_ctl02);
	asm_vmwrite (VMCS_VMEXIT_CTL,
		     (exit_ctl02 & ~EXIT_CTL_SHADOW_MASK) |
		     (exit_ctl01 & EXIT_CTL_SHADOW_MASK));

	/* Clear enable VPID bit to invalidate cached mappings
	 * associated with VPID 0 at VM entry and VM exit if
	 * unrestricted guest is not used.  If unrestricted guest is
	 * used, EPT is used and shadow paging is not used.  In case
	 * of EPT, cached mappings are associated with EPTP bits
	 * 51:12, so invalidation is not needed. */
	asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL, &procbased_ctls);
	if (procbased_ctls & VMCS_PROC_BASED_VMEXEC_CTL_ACTIVATECTLS2_BIT)
		asm_vmread (VMCS_PROC_BASED_VMEXEC_CTL2, &procbased_ctls2);
	else
		procbased_ctls2 = 0;
	orig_procbased_ctls = procbased_ctls;
	orig_procbased_ctls2 = procbased_ctls2;
	if ((procbased_ctls2 & VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VPID_BIT) &&
	    !current->u.vt.unrestricted_guest)
		procbased_ctls2 &=
			~VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VPID_BIT;

	/* Set enable EPT bit and EPT pointer.  If the EPT is
	 * concealed for the guest, the previous EPT pointer is not
	 * saved. */
	u64 guest_eptp = guest_eptp; /* Avoid compiler warning */
	ulong guest_vpid = guest_vpid;
	struct shadow_ept_ept_info *ept_info = NULL;
	struct shadow_ept_vpid_info *vpid_info = NULL;
	if ((procbased_ctls2 &
	     VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_EPT_BIT) && host_eptp) {
		/* Using shadow EPT. */
		asm_vmread64 (VMCS_EPT_POINTER, &guest_eptp);
		if (!(guest_eptp & VMCS_EPT_POINTER_EPT_WB) ||
		    !(guest_eptp & VMCS_EPT_PAGEWALK_LENGTH_4))
			panic ("Nested invalid EPT pointer 0x%llX",
			       guest_eptp);
		ept_info = shadow_ept_get_ept_info (shadow_vt, guest_eptp);
		if (procbased_ctls2 &
		    VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VPID_BIT)
			asm_vmread (VMCS_VPID, &guest_vpid);
		else
			guest_vpid = 0;
		if (guest_vpid)
			vpid_info = shadow_ept_get_vpid_info (shadow_vt,
							      guest_eptp,
							      guest_vpid);
		shadow_ept_activate_and_vmwrite (shadow_vt, ept_info,
						 vpid_info);
	} else if (host_eptp) {
		/* Apply host EPTP to the guest. */
		if (procbased_ctls2 &
		    VMCS_PROC_BASED_VMEXEC_CTL2_NESTED_OFF_BITS)
			panic ("Nested invalid procbased_ctls2 0x%lX",
			       procbased_ctls2);
		procbased_ctls2 &=
			~VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VPID_BIT;
		procbased_ctls2 |=
			VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_EPT_BIT;
		asm_vmwrite64 (VMCS_EPT_POINTER, host_eptp);
		ulong ctl;
		asm_vmread (VMCS_VMENTRY_CTL, &ctl);
		if (!(ctl & VMCS_VMENTRY_CTL_64_GUEST_BIT)) {
			ulong cr4;
			asm_vmread (VMCS_GUEST_CR4, &cr4);
			if (cr4 & CR4_PAE_BIT) {
				/* Need to read PDPTE here! */
				ulong cr3;
				asm_vmread (VMCS_GUEST_CR3, &cr3);
				cr3 &= 0xFFFFFFE0;
				u64 *p = mapmem_as (current->as, cr3,
						    sizeof *p * 4, 0);
				asm_vmwrite64 (VMCS_GUEST_PDPTE0, p[0]);
				asm_vmwrite64 (VMCS_GUEST_PDPTE1, p[1]);
				asm_vmwrite64 (VMCS_GUEST_PDPTE2, p[2]);
				asm_vmwrite64 (VMCS_GUEST_PDPTE3, p[3]);
				unmapmem (p, sizeof *p * 4);
			}
		}
	}

	/* Apply processor-based VM-execution controls */
	if (procbased_ctls2)
		procbased_ctls |= VMCS_PROC_BASED_VMEXEC_CTL_ACTIVATECTLS2_BIT;
	if (orig_procbased_ctls != procbased_ctls)
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, procbased_ctls);
	if (orig_procbased_ctls2 != procbased_ctls2)
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL2, procbased_ctls2);

	/* Run */
#ifdef __x86_64__
	efer_l1 |= (MSR_IA32_EFER_LMA_BIT | MSR_IA32_EFER_LME_BIT);
#else
	efer_l1 &= ~(MSR_IA32_EFER_LMA_BIT | MSR_IA32_EFER_LME_BIT);
#endif
	asm_rdmsr64 (MSR_IA32_EFER, &saved_host_efer);
	asm_wrmsr64 (MSR_IA32_EFER, efer_l1);
	if (!vmlaunched)
		entry_err = asm_vmlaunch_regs (&current->u.vt.vr);
	else
		entry_err = asm_vmresume_regs (&current->u.vt.vr);
	asm_rdmsr64 (MSR_IA32_EFER, &efer_l2);
	asm_wrmsr64 (MSR_IA32_EFER, saved_host_efer);

	/* Handle EPT violation */
	if (host_eptp && !entry_err && handle_ept (shadow_vt, ept_info,
						   guest_eptp)) {
		do {
			handle_ept_violation_event_delivery ();
			efer_l1 = efer_l2;
			asm_wrmsr64 (MSR_IA32_EFER, efer_l1);
			entry_err = asm_vmresume_regs (&current->u.vt.vr);
			asm_rdmsr64 (MSR_IA32_EFER, &efer_l2);
			asm_wrmsr64 (MSR_IA32_EFER, saved_host_efer);
		} while (!entry_err && handle_ept (shadow_vt, ept_info,
						   guest_eptp));
		/* If NMI occurs after the EPT violation VM exit, the
		 * NMI should be notified as a VM exit.  It is not
		 * implemented yet.  The following handles it like NMI
		 * before a VM entry for simplicity, but in this case,
		 * the VM state might have been changed.  The guest
		 * might detect the unexpected VM state change.*/
		switch (entry_err) {
		case 1:
			if (!vmlaunched) {
				/* VMCLEAR is needed since VMLAUNCH
				 * will be executed again. */
				asm_vmclear (&shadow_vt->current_vmcs_hphys);
				asm_vmptrld (&shadow_vt->current_vmcs_hphys);
			}
			/* Fall through */
		case 0:
			break;
		default:
			/* Unexpected VM entry failure */
			panic ("Nested VM entry failure %d", entry_err);
		}
	}

	/* Restore EPT pointer and VPID if needed. */
	if (ept_info)
		asm_vmwrite64 (VMCS_EPT_POINTER, guest_eptp);
	if (vpid_info)
		asm_vmwrite (VMCS_VPID, guest_vpid);

	/* Restore processor-based VM-execution controls */
	if (orig_procbased_ctls2 != procbased_ctls2)
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL2,
			     orig_procbased_ctls2);
	if (orig_procbased_ctls != procbased_ctls)
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL, orig_procbased_ctls);

	/* Restore VMCS_VMEXIT_CTL */
	asm_vmwrite (VMCS_VMEXIT_CTL, exit_ctl02);

	/* Restore host states */
	if (shadow_vt->mode == MODE_SHADOWING) {
		vmcs_shadowing_copy (shadow_vt->vmxon_region_phys,
				     shadow_vt->current_vmcs_hphys, false);
		asm_vmptrld (&shadow_vt->current_vmcs_hphys);
	} else {
		set_vmcs_host_states (&hsl02);
	}

	if (entry_err) {	/* VM entry has been canceled */
		asm_vmptrld (&orig_vmcs_addr_phys);
		if (entry_err == 1) { /* NMI */
			/* Restart from the same instruction after
			 * handling NMI */
			return;
		}
	vmfail:
		asm_vmread (VMCS_GUEST_RFLAGS, &guest_rflags);
		guest_rflags &= ~flag_mask;
		if (entry_err == -1) /* VMfailValid */
			guest_rflags |= flag_fail_valid;
		else	/* VMfailInvalid */
			guest_rflags |= flag_fail_invalid;
		asm_vmwrite (VMCS_GUEST_RFLAGS, guest_rflags);
		add_ip ();
		return;
	}

	/* Handle an external interrupt if acknowledged */
	clear_exint_hack (shadow_vt, "VM exit");
	if (exit_ctl02 & VMCS_VMEXIT_CTL_ACK_INTR_ON_EXIT_BIT)
		handle_acked_exint_pass (shadow_vt);

	asm_vmread64 (VMCS_HOST_IA32_PERF_GLOBAL_CTRL,
		      &hsl02.ia32_perf_global_ctrl);
	switch_vmcs_and_load_l1_host_state (orig_vmcs_addr_phys, efer_l2,
					    exit_ctl02, &hsl02);
}

void
vt_emul_vmlaunch (void)
{
	run_l2vm (false);
}

void
vt_emul_vmresume (void)
{
	run_l2vm (true);
}
