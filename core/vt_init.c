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
#include "assert.h"
#include "constants.h"
#include "current.h"
#include "initfunc.h"
#include "panic.h"
#include "mm.h"
#include "pcpu.h"
#include "printf.h"
#include "string.h"
#include "vt_exitreason.h"
#include "vt_init.h"
#include "vt_paging.h"
#include "vt_panic.h"
#include "vt_regs.h"

/* Check whether VMX is usable
   Return value:
   0:usable
   -1:unusable (disabled by BIOS Setup, or not supported) */
static int
check_vmx (void)
{
	u32 a, b, c, d;
	u64 tmp;

	/* 19.6 DISCOVERING SUPPORT FOR VMX */
	asm_cpuid (CPUID_1, 0, &a, &b, &c, &d);
	if (c & CPUID_1_ECX_VMX_BIT) {
		/* VMX operation is supported. */
	} else {
		/* printf ("VMX operation is not supported.\n"); */
		return -1;
	}

	/* 19.7 ENABLING AND ENTERING VMX OPERATION */
msr_enable_loop:
	asm_rdmsr64 (MSR_IA32_FEATURE_CONTROL, &tmp);
	if (tmp & MSR_IA32_FEATURE_CONTROL_LOCK_BIT) {
		if (tmp & MSR_IA32_FEATURE_CONTROL_VMXON_BIT) {
			/* VMXON is enabled. */
		} else {
			printf ("VMXON is disabled.\n");
			return -1;
		}
	} else {
		printf ("Trying to enable VMXON.\n");
		tmp |= MSR_IA32_FEATURE_CONTROL_VMXON_BIT;
		tmp |= MSR_IA32_FEATURE_CONTROL_LOCK_BIT;
		asm_wrmsr64 (MSR_IA32_FEATURE_CONTROL, tmp);
		goto msr_enable_loop;
	}

	return 0;
}

int
vt_available (void)
{
	return !check_vmx ();
}

void
vt__vmx_init (void)
{
	void *v;
	u64 p;

	if (alloc_page (&v, &p) < 0)
		panic ("Fatal error: vt__vmx_init: alloc_page failed.");
	currentcpu->vt.vmxon_region_virt = v;
	currentcpu->vt.vmxon_region_phys = p;
}

/* Enable VMX and do VMXON
   INPUT:
   vmxon_region_phys: physical address of VMXON region
   vmxon_region_virt: virtual address of VMXON region
   OUTPUT:
   vmcs_revision_identifier: VMCS revision identifier
*/
void
vt__vmxon (void)
{
	ulong cr0_0, cr0_1, cr4_0, cr4_1;
	ulong cr0, cr4;
	u32 *p;
	u32 dummy;

	/* apply FIXED bits */
	asm_rdmsr (MSR_IA32_VMX_CR0_FIXED0, &cr0_0);
	asm_rdmsr (MSR_IA32_VMX_CR0_FIXED1, &cr0_1);
	asm_rdmsr (MSR_IA32_VMX_CR4_FIXED0, &cr4_0);
	asm_rdmsr (MSR_IA32_VMX_CR4_FIXED1, &cr4_1);
	asm_rdcr0 (&cr0);
	cr0 &= cr0_1;
	cr0 |= cr0_0;
	asm_wrcr0 (cr0);
	asm_rdcr4 (&cr4);
	cr4 &= cr4_1;
	cr4 |= cr4_0;
	asm_wrcr4 (cr4);

	/* set VMXE bit to enable VMX */
	asm_rdcr4 (&cr4);
	cr4 |= CR4_VMXE_BIT;
	asm_wrcr4 (cr4);

	/* write a VMCS revision identifier */
	asm_rdmsr32 (MSR_IA32_VMX_BASIC,
		     &currentcpu->vt.vmcs_revision_identifier, &dummy);
	p = currentcpu->vt.vmxon_region_virt;
	*p = currentcpu->vt.vmcs_revision_identifier;

	/* VMXON */
	asm_vmxon (&currentcpu->vt.vmxon_region_phys);
}

static void
vpid_init (void)
{
	u64 ept_vpid_cap;

	asm_rdmsr64 (MSR_IA32_VMX_EPT_VPID_CAP, &ept_vpid_cap);
	if (!(ept_vpid_cap & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_BIT))
		return;
	if (!(ept_vpid_cap &
	      MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_BIT))
		return;
	current->u.vt.vpid = 1; /* FIXME: VPID 1 only */
}

static void
ept_init (void)
{
	u64 ept_vpid_cap;

	asm_rdmsr64 (MSR_IA32_VMX_EPT_VPID_CAP, &ept_vpid_cap);
	if (!(ept_vpid_cap & MSR_IA32_VMX_EPT_VPID_CAP_PAGEWALK_LENGTH_4_BIT))
		return;
	if (!(ept_vpid_cap & MSR_IA32_VMX_EPT_VPID_CAP_EPTSTRUCT_WB_BIT))
		return;
	current->u.vt.ept_available = true;
	if (!(ept_vpid_cap & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_BIT))
		return;
	if (!(ept_vpid_cap & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXT_BIT))
		return;
	current->u.vt.invept_available = true;
}

static bool
vt_cr3exit_controllable (void)
{
	u64 vmx_basic;
	u32 true_procbased_ctls_or, true_procbased_ctls_and;

	asm_rdmsr64 (MSR_IA32_VMX_BASIC, &vmx_basic);
	if (!(vmx_basic & MSR_IA32_VMX_BASIC_TRUE_CTLS_BIT))
		return false;
	asm_rdmsr32 (MSR_IA32_VMX_TRUE_PROCBASED_CTLS,
		     &true_procbased_ctls_or, &true_procbased_ctls_and);
	if (true_procbased_ctls_or &
	    VMCS_PROC_BASED_VMEXEC_CTL_CR3LOADEXIT_BIT)
		return false;
	if (true_procbased_ctls_or &
	    VMCS_PROC_BASED_VMEXEC_CTL_CR3STOREEXIT_BIT)
		return false;
	return true;
}

/* Initialize VMCS region
   INPUT:
   vmcs_revision_identifier: VMCS revision identifier
 */
static void
vt__vmcs_init (void)
{
	u32 *p;
	struct regs_in_vmcs host_riv, guest_riv;
	u32 pinbased_ctls_or, pinbased_ctls_and;
	u32 procbased_ctls_or, procbased_ctls_and;
	u32 exit_ctls_or, exit_ctls_and;
	u32 entry_ctls_or, entry_ctls_and;
	ulong sysenter_cs, sysenter_esp, sysenter_eip;
	ulong exitctl64;
	ulong exitctl_efer = 0, entryctl_efer = 0;
	u64 host_efer;
	u32 procbased_ctls2_or, procbased_ctls2_and = 0;
	ulong procbased_ctls2 = 0;
	struct vt_io_data *io;
	struct vt_msrbmp *msrbmp;

	current->u.vt.first = true;
	/* The iobmp initialization must be executed during
	 * current->vcpu0 == current, because the iobmp is accessed by
	 * vmctl.iopass() that is called from set_iofunc() during
	 * current->vcpu0 == current. */
	if (current->vcpu0 == current) {
		io = alloc (sizeof *io);
		alloc_page (&io->iobmp[0], &io->iobmpphys[0]);
		alloc_page (&io->iobmp[1], &io->iobmpphys[1]);
		memset (io->iobmp[0], 0xFF, PAGESIZE);
		memset (io->iobmp[1], 0xFF, PAGESIZE);
	} else {
		while (!(io = current->vcpu0->u.vt.io))
			asm_pause ();
	}
	current->u.vt.io = io;
	if (current->vcpu0 == current) {
		msrbmp = alloc (sizeof *msrbmp);
		alloc_page (&msrbmp->msrbmp, &msrbmp->msrbmp_phys);
		memset (msrbmp->msrbmp, 0xFF, PAGESIZE);
	} else {
		while (!(msrbmp = current->vcpu0->u.vt.msrbmp))
			asm_pause ();
	}
	current->u.vt.msrbmp = msrbmp;
	current->u.vt.saved_vmcs = NULL;
	current->u.vt.vpid = 0;
	current->u.vt.ept = NULL;
	current->u.vt.ept_available = false;
	current->u.vt.invept_available = false;
	current->u.vt.unrestricted_guest_available = false;
	current->u.vt.unrestricted_guest = false;
	current->u.vt.save_load_efer_enable = false;
	current->u.vt.exint_pass = true;
	current->u.vt.exint_pending = false;
	current->u.vt.cr3exit_controllable = vt_cr3exit_controllable ();
	current->u.vt.cr3exit_off = false;
	alloc_page (&current->u.vt.vi.vmcs_region_virt,
		    &current->u.vt.vi.vmcs_region_phys);
	current->u.vt.intr.vmcs_intr_info.s.valid = INTR_INFO_VALID_INVALID;

	p = current->u.vt.vi.vmcs_region_virt;
	*p = currentcpu->vt.vmcs_revision_identifier; /* write VMCS revision
							 identifier */
	asm_vmclear (&current->u.vt.vi.vmcs_region_phys); /* clear */
	vt_vmptrld (current->u.vt.vi.vmcs_region_phys); /* load */

	/* get information from MSR */
	asm_rdmsr32 (MSR_IA32_VMX_PINBASED_CTLS,
		     &pinbased_ctls_or, &pinbased_ctls_and);
	asm_rdmsr32 (MSR_IA32_VMX_PROCBASED_CTLS,
		     &procbased_ctls_or, &procbased_ctls_and);
	asm_rdmsr32 (MSR_IA32_VMX_EXIT_CTLS,
		     &exit_ctls_or, &exit_ctls_and);
	asm_rdmsr32 (MSR_IA32_VMX_ENTRY_CTLS,
		     &entry_ctls_or, &entry_ctls_and);
	asm_rdmsr (MSR_IA32_SYSENTER_CS, &sysenter_cs);
	asm_rdmsr (MSR_IA32_SYSENTER_ESP, &sysenter_esp);
	asm_rdmsr (MSR_IA32_SYSENTER_EIP, &sysenter_eip);
	if (procbased_ctls_and &
	    VMCS_PROC_BASED_VMEXEC_CTL_ACTIVATECTLS2_BIT) {
		asm_rdmsr32 (MSR_IA32_VMX_PROCBASED_CTLS2,
			     &procbased_ctls2_or, &procbased_ctls2_and);
		/* procbased_ctls2_or is always zero */
		procbased_ctls2 = 0;
		if (procbased_ctls2_and &
		    VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VPID_BIT)
			vpid_init ();
		if (current->u.vt.vpid)
			procbased_ctls2 |=
				VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_VPID_BIT;
		if ((procbased_ctls2_and &
		     VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_EPT_BIT) &&
		    (exit_ctls_and & VMCS_VMEXIT_CTL_SAVE_IA32_PAT_BIT) &&
		    (exit_ctls_and & VMCS_VMEXIT_CTL_LOAD_IA32_PAT_BIT) &&
		    (entry_ctls_and & VMCS_VMENTRY_CTL_LOAD_IA32_PAT_BIT))
			ept_init ();
		if (procbased_ctls2_and &
		    VMCS_PROC_BASED_VMEXEC_CTL2_UNRESTRICTED_GUEST_BIT)
			current->u.vt.unrestricted_guest_available = true;
		if (procbased_ctls2_and &
		    VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_RDTSCP_BIT)
			procbased_ctls2 |=
				VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_RDTSCP_BIT;
		if (procbased_ctls2_and &
		    VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_XSAVES_BIT)
			procbased_ctls2 |=
				VMCS_PROC_BASED_VMEXEC_CTL2_ENABLE_XSAVES_BIT;
	}
	if ((exit_ctls_and & VMCS_VMEXIT_CTL_SAVE_IA32_EFER_BIT) &&
	    (exit_ctls_and & VMCS_VMEXIT_CTL_LOAD_IA32_EFER_BIT) &&
	    (entry_ctls_and & VMCS_VMENTRY_CTL_LOAD_IA32_EFER_BIT)) {
		current->u.vt.save_load_efer_enable = true;
		exitctl_efer |= VMCS_VMEXIT_CTL_SAVE_IA32_EFER_BIT;
		exitctl_efer |= VMCS_VMEXIT_CTL_LOAD_IA32_EFER_BIT;
		entryctl_efer |= VMCS_VMENTRY_CTL_LOAD_IA32_EFER_BIT;
	}

	/* get current information */
	vt_get_current_regs_in_vmcs (&host_riv);
	guest_riv = host_riv;
	exitctl64 = sizeof (ulong) == 4 ? 0 :
		VMCS_VMEXIT_CTL_HOST_ADDRESS_SPACE_SIZE_BIT;

	/* initialize VMCS fields */
	/* 16-Bit Control Field */
	if (current->u.vt.vpid)
		asm_vmwrite (VMCS_VPID, current->u.vt.vpid);
	/* 16-Bit Guest-State Fields */
	asm_vmwrite (VMCS_GUEST_ES_SEL, guest_riv.es.sel);
	asm_vmwrite (VMCS_GUEST_CS_SEL, guest_riv.cs.sel);
	asm_vmwrite (VMCS_GUEST_SS_SEL, guest_riv.ss.sel);
	asm_vmwrite (VMCS_GUEST_DS_SEL, guest_riv.ds.sel);
	asm_vmwrite (VMCS_GUEST_FS_SEL, guest_riv.fs.sel);
	asm_vmwrite (VMCS_GUEST_GS_SEL, guest_riv.gs.sel);
	asm_vmwrite (VMCS_GUEST_LDTR_SEL, guest_riv.ldtr.sel);
	asm_vmwrite (VMCS_GUEST_TR_SEL, guest_riv.tr.sel);
	/* 16-Bit Host-State Fields */
	asm_vmwrite (VMCS_HOST_ES_SEL, host_riv.es.sel);
	asm_vmwrite (VMCS_HOST_CS_SEL, host_riv.cs.sel);
	asm_vmwrite (VMCS_HOST_SS_SEL, host_riv.ss.sel);
	asm_vmwrite (VMCS_HOST_DS_SEL, host_riv.ds.sel);
	asm_vmwrite (VMCS_HOST_FS_SEL, host_riv.fs.sel);
	asm_vmwrite (VMCS_HOST_GS_SEL, host_riv.gs.sel);
	asm_vmwrite (VMCS_HOST_TR_SEL, host_riv.tr.sel);
	/* 64-Bit Control Fields */
	asm_vmwrite64 (VMCS_ADDR_IOBMP_A, io->iobmpphys[0]);
	asm_vmwrite64 (VMCS_ADDR_IOBMP_B, io->iobmpphys[1]);
	asm_vmwrite64 (VMCS_ADDR_MSRBMP, msrbmp->msrbmp_phys);
	asm_vmwrite64 (VMCS_VMEXIT_MSRSTORE_ADDR, 0);
	asm_vmwrite64 (VMCS_VMEXIT_MSRLOAD_ADDR, 0);
	asm_vmwrite64 (VMCS_VMENTRY_MSRLOAD_ADDR, 0);
	asm_vmwrite64 (VMCS_EXEC_VMCS_POINTER, 0);
	asm_vmwrite64 (VMCS_TSC_OFFSET, 0);
	/* 64-Bit Guest-State Fields */
	asm_vmwrite64 (VMCS_VMCS_LINK_POINTER, 0xFFFFFFFFFFFFFFFFULL);
	asm_vmwrite64 (VMCS_GUEST_IA32_DEBUGCTL, 0);
	if (current->u.vt.save_load_efer_enable)
		asm_vmwrite64 (VMCS_GUEST_IA32_EFER, 0);
	/* 32-Bit Control Fields */
	asm_vmwrite (VMCS_PIN_BASED_VMEXEC_CTL,
		     (/* VMCS_PIN_BASED_VMEXEC_CTL_EXINTEXIT_BIT */0 |
		      VMCS_PIN_BASED_VMEXEC_CTL_NMIEXIT_BIT |
		      VMCS_PIN_BASED_VMEXEC_CTL_VIRTNMIS_BIT |
		      pinbased_ctls_or) & pinbased_ctls_and);
	asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL,
		     (/* XXX: VMCS_PROC_BASED_VMEXEC_CTL_HLTEXIT_BIT */0 |
		      VMCS_PROC_BASED_VMEXEC_CTL_INVLPGEXIT_BIT |
		      VMCS_PROC_BASED_VMEXEC_CTL_UNCONDIOEXIT_BIT |
		      VMCS_PROC_BASED_VMEXEC_CTL_USETSCOFF_BIT |
		      VMCS_PROC_BASED_VMEXEC_CTL_USEIOBMP_BIT |
		      VMCS_PROC_BASED_VMEXEC_CTL_USEMSRBMP_BIT |
		      (procbased_ctls2_and ?
		       VMCS_PROC_BASED_VMEXEC_CTL_ACTIVATECTLS2_BIT : 0) |
		      procbased_ctls_or) & procbased_ctls_and);
	asm_vmwrite (VMCS_EXCEPTION_BMP, 0xFFFFFFFF);
	asm_vmwrite (VMCS_PAGEFAULT_ERRCODE_MASK, 0);
	asm_vmwrite (VMCS_PAGEFAULT_ERRCODE_MATCH, 0);
	asm_vmwrite (VMCS_CR3_TARGET_COUNT, 0);
	asm_vmwrite (VMCS_VMEXIT_CTL, (exit_ctls_or & exit_ctls_and) |
		     exitctl64 | exitctl_efer);
	asm_vmwrite (VMCS_VMEXIT_MSR_STORE_COUNT, 0);
	asm_vmwrite (VMCS_VMEXIT_MSR_LOAD_COUNT, 0);
	asm_vmwrite (VMCS_VMENTRY_CTL, (entry_ctls_or & entry_ctls_and) |
		     entryctl_efer);
	asm_vmwrite (VMCS_VMENTRY_MSR_LOAD_COUNT, 0);
	asm_vmwrite (VMCS_VMENTRY_INTR_INFO_FIELD, 0);
	asm_vmwrite (VMCS_VMENTRY_EXCEPTION_ERRCODE, 0);
	asm_vmwrite (VMCS_VMENTRY_INSTRUCTION_LEN, 0);
	asm_vmwrite (VMCS_TPR_THRESHOLD, 0);
	if (procbased_ctls2_and)
		asm_vmwrite (VMCS_PROC_BASED_VMEXEC_CTL2, procbased_ctls2);
	/* 32-Bit Guest-State Fields */
	asm_vmwrite (VMCS_GUEST_ES_LIMIT, guest_riv.es.limit);
	asm_vmwrite (VMCS_GUEST_CS_LIMIT, guest_riv.cs.limit);
	asm_vmwrite (VMCS_GUEST_SS_LIMIT, guest_riv.ss.limit);
	asm_vmwrite (VMCS_GUEST_DS_LIMIT, guest_riv.ds.limit);
	asm_vmwrite (VMCS_GUEST_FS_LIMIT, guest_riv.fs.limit);
	asm_vmwrite (VMCS_GUEST_GS_LIMIT, guest_riv.gs.limit);
	asm_vmwrite (VMCS_GUEST_LDTR_LIMIT, guest_riv.ldtr.limit);
	asm_vmwrite (VMCS_GUEST_TR_LIMIT, guest_riv.tr.limit);
	asm_vmwrite (VMCS_GUEST_GDTR_LIMIT, guest_riv.gdtr.limit);
	asm_vmwrite (VMCS_GUEST_IDTR_LIMIT, guest_riv.idtr.limit);
	asm_vmwrite (VMCS_GUEST_ES_ACCESS_RIGHTS, guest_riv.es.acr);
	asm_vmwrite (VMCS_GUEST_CS_ACCESS_RIGHTS, guest_riv.cs.acr);
	asm_vmwrite (VMCS_GUEST_SS_ACCESS_RIGHTS, guest_riv.ss.acr);
	asm_vmwrite (VMCS_GUEST_DS_ACCESS_RIGHTS, guest_riv.ds.acr);
	asm_vmwrite (VMCS_GUEST_FS_ACCESS_RIGHTS, guest_riv.fs.acr);
	asm_vmwrite (VMCS_GUEST_GS_ACCESS_RIGHTS, guest_riv.gs.acr);
	asm_vmwrite (VMCS_GUEST_LDTR_ACCESS_RIGHTS, guest_riv.ldtr.acr);
	asm_vmwrite (VMCS_GUEST_TR_ACCESS_RIGHTS, guest_riv.tr.acr);
	asm_vmwrite (VMCS_GUEST_INTERRUPTIBILITY_STATE, 0);
	asm_vmwrite (VMCS_GUEST_ACTIVITY_STATE, 0);
	asm_vmwrite (VMCS_GUEST_IA32_SYSENTER_CS, sysenter_cs);
	/* 32-Bit Host-State Field */
	asm_vmwrite (VMCS_HOST_IA32_SYSENTER_CS, sysenter_cs);
	/* Natural-Width Control Fields */
	asm_vmwrite (VMCS_CR0_GUESTHOST_MASK, VT_CR0_GUESTHOST_MASK);
	asm_vmwrite (VMCS_CR4_GUESTHOST_MASK, VT_CR4_GUESTHOST_MASK);
	asm_vmwrite (VMCS_CR0_READ_SHADOW, guest_riv.cr0);
	asm_vmwrite (VMCS_CR4_READ_SHADOW, guest_riv.cr4);
	asm_vmwrite (VMCS_CR3_TARGET_VALUE_0, 0);
	asm_vmwrite (VMCS_CR3_TARGET_VALUE_1, 0);
	asm_vmwrite (VMCS_CR3_TARGET_VALUE_2, 0);
	asm_vmwrite (VMCS_CR3_TARGET_VALUE_3, 0);
	/* Natural-Width Guest-State Fields */
	asm_vmwrite (VMCS_GUEST_CR0, guest_riv.cr0);
	asm_vmwrite (VMCS_GUEST_CR3, guest_riv.cr3);
	asm_vmwrite (VMCS_GUEST_CR4, guest_riv.cr4);
	asm_vmwrite (VMCS_GUEST_ES_BASE, guest_riv.es.base);
	asm_vmwrite (VMCS_GUEST_CS_BASE, guest_riv.cs.base);
	asm_vmwrite (VMCS_GUEST_SS_BASE, guest_riv.ss.base);
	asm_vmwrite (VMCS_GUEST_DS_BASE, guest_riv.ds.base);
	asm_vmwrite (VMCS_GUEST_FS_BASE, guest_riv.fs.base);
	asm_vmwrite (VMCS_GUEST_GS_BASE, guest_riv.gs.base);
	asm_vmwrite (VMCS_GUEST_LDTR_BASE, guest_riv.ldtr.base);
	asm_vmwrite (VMCS_GUEST_TR_BASE, guest_riv.tr.base);
	asm_vmwrite (VMCS_GUEST_GDTR_BASE, guest_riv.gdtr.base);
	asm_vmwrite (VMCS_GUEST_IDTR_BASE, guest_riv.idtr.base);
	asm_vmwrite (VMCS_GUEST_DR7, guest_riv.dr7);
	asm_vmwrite (VMCS_GUEST_RSP, 0xDEADBEEF);
	asm_vmwrite (VMCS_GUEST_RIP, 0xDEADBEEF);
	asm_vmwrite (VMCS_GUEST_RFLAGS, guest_riv.rflags);
	asm_vmwrite (VMCS_GUEST_PENDING_DEBUG_EXCEPTIONS, 0);
	asm_vmwrite (VMCS_GUEST_IA32_SYSENTER_ESP, sysenter_esp);
	asm_vmwrite (VMCS_GUEST_IA32_SYSENTER_EIP, sysenter_eip);
	/* Natural-Width Host-State Fields */
	asm_vmwrite (VMCS_HOST_CR0, host_riv.cr0);
	asm_vmwrite (VMCS_HOST_CR3, host_riv.cr3);
	asm_vmwrite (VMCS_HOST_CR4, host_riv.cr4);
	asm_vmwrite (VMCS_HOST_FS_BASE, host_riv.fs.base);
	asm_vmwrite (VMCS_HOST_GS_BASE, host_riv.gs.base);
	asm_vmwrite (VMCS_HOST_TR_BASE, host_riv.tr.base);
	asm_vmwrite (VMCS_HOST_GDTR_BASE, host_riv.gdtr.base);
	asm_vmwrite (VMCS_HOST_IDTR_BASE, host_riv.idtr.base);
	asm_vmwrite (VMCS_HOST_IA32_SYSENTER_ESP, sysenter_esp);
	asm_vmwrite (VMCS_HOST_IA32_SYSENTER_EIP, sysenter_eip);
	asm_vmwrite (VMCS_HOST_RSP, 0xDEADBEEF);
	asm_vmwrite (VMCS_HOST_RIP, 0xDEADBEEF);
	if (current->u.vt.save_load_efer_enable) {
		asm_rdmsr64 (MSR_IA32_EFER, &host_efer);
		asm_vmwrite64 (VMCS_HOST_IA32_EFER, host_efer);
	}
}

static void
vt__realmode_data_init (void)
{
	current->u.vt.realmode.idtr.base = 0;
	current->u.vt.realmode.idtr.limit = 0x3FF;
	current->u.vt.realmode.tr_limit = 0;
	current->u.vt.realmode.tr_acr = 0x8B; /* 32bit busy TSS */
	current->u.vt.realmode.tr_base = 0;
}

static void
vt__vmcs_exit (void)
{
	free_page (current->u.vt.vi.vmcs_region_virt);
}

void
vt_vminit (void)
{
	vt__vmcs_init ();
	vt__realmode_data_init ();
	vt_msr_init ();
	vt_paging_init ();
	call_initfunc ("vcpu");
}

void
vt_vmexit (void)
{
	vt__vmcs_exit ();
}

void
vt_init (void)
{
	vt__vmx_init ();
	vt__vmxon ();
}

void
vt_enable_resume (void)
{
	ASSERT (!current->u.vt.saved_vmcs);
	alloc_page (&current->u.vt.saved_vmcs, NULL);
	asm_vmclear (&current->u.vt.vi.vmcs_region_phys);
	memcpy (current->u.vt.saved_vmcs, current->u.vt.vi.vmcs_region_virt,
		PAGESIZE);
	asm_vmptrld (&current->u.vt.vi.vmcs_region_phys);
	current->u.vt.first = true;
	spinlock_lock (&currentcpu->suspend_lock);
}

void
vt_resume (void)
{
	ASSERT (current->u.vt.saved_vmcs);
	vt__vmxon ();
	memcpy (current->u.vt.vi.vmcs_region_virt, current->u.vt.saved_vmcs,
		PAGESIZE);
	asm_vmclear (&current->u.vt.vi.vmcs_region_phys);
	asm_vmptrld (&current->u.vt.vi.vmcs_region_phys);
	current->u.vt.first = true;
	spinlock_init (&currentcpu->suspend_lock);
	spinlock_lock (&currentcpu->suspend_lock);
}
