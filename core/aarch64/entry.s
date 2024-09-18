/*
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

	PANIC_INITIAL_STATE = 0xFF
	PAGESIZE	    = 4096
	VMM_BOOT_UEFI	    = 0

	.section .entry,"ax"
	.balign 4
	.global entry
entry:
	/* Check whether we are in EL2 or not */
	mrs	x9, CurrentEL
	lsr	x9, x9, #2
	cmp	x9, #2
	bne	.L11 /* Return error if not running on EL2 */

	/*
	 * Save callee-saved registers and other necessary system registers for
	 * returning to the loader later.
	 */
	adrp	x9, _uefi_entry_ctx
	add	x9, x9, :lo12:_uefi_entry_ctx
	stp	x19, x20, [x9], #16
	stp	x21, x22, [x9], #16
	stp	x23, x24, [x9], #16
	stp	x25, x26, [x9], #16
	stp	x27, x28, [x9], #16
	stp	x29, x30, [x9], #16
	mov	x10, sp
	str	x10, [x9], #8
	mrs	x10, ESR_EL2
	str	x10, [x9], #8
	mrs	x10, FAR_EL2
	str	x10, [x9], #8
	mrs	x10, MAIR_EL2
	str	x10, [x9], #8
	mrs	x10, TCR_EL2
	str	x10, [x9], #8
	mrs	x10, TPIDR_EL2
	str	x10, [x9], #8
	mrs	x10, TTBR0_EL2
	str	x10, [x9], #8
	mrs	x10, VBAR_EL2
	str	x10, [x9], #8
	/* Construct partial SPSR_EL2 for ERET later */
	mov	x10, xzr
	mrs	x11, NZCV
	orr	x10, x10, x11
	mrs	x11, DAIF
	orr	x10, x10, x11
	/* Might need them in the future
	mrs	x11, TCO
	orr	x10, x10, x11
	mrs	x11, DIT
	orr	x10, x10, x11
	mrs	x11, UAO
	orr	x10, x10, x11
	mrs	x11, SSBS
	orr	x10, x10, x11
	*/
	mrs	x11, PAN
	orr	x10, x10, x11
	str	x10, [x9], #8

	sub	sp, sp, #(16 * 3)
	stp	x29, x30, [sp, #(16 * 0)]
	stp	x22, x23, [sp, #(16 * 1)]
	stp	x20, x21, [sp, #(16 * 2)]

	mov	x20, x0
	mov	x21, x1
	mov	x22, x2

	/* Need to apply relocation as the binary is relocatable */
	adrp	x0, head
	add	x0, x0, :lo12:head
	adrp	x1, _rela_start
	add	x1, x1, :lo12:_rela_start
	adrp	x2, _rela_end
	add	x2, x2, :lo12:_rela_end
	bl	apply_reloc_on_entry
	cmp	x0, 0
	bne	.L1 /* Return if apply_reloc_on_entry() fails */

	mov	x0, x20
	mov	x1, x21
	mov	x2, x22

	bl	uefi_entry_load
	cmp	x0, 0
	beq	.L1 /* Return if new base is NULL */

	/*
	 * x0 now contains new virtual memory base address. Next, calulate the
	 * position of _vmm_entry() relative x0.
	 */
	adrp	x21, head /* Old head */
	add	x21, x21, :lo12:head
	adrp	x11, _vmm_entry
	add	x11, x11, :lo12:_vmm_entry
	sub	x11, x11, x21
	add	x11, x11, x0
	mov	x1, VMM_BOOT_UEFI /* Boot via UEFI */

	msr	DAIFSet, #3 /* Disable interrupts */
	isb /* Want msr write to be effective immediately */

	/* Jump to newly located _vmm_entry(virt_base, boot_mode) */
	br	x11
.L1:
	ldp	x29, x30, [sp, #(16 * 0)]
	ldp	x22, x23, [sp, #(16 * 1)]
	ldp	x20, x21, [sp, #(16 * 2)]
	add	sp, sp, #(16 * 3)
.L11:
	mov	x0, xzr /* loadvmm expects 0 on error */
	ret

_vmm_entry:
	mov	x20, x1 /* Save boot_mode */

	/* Apply relocation with new virtual memory base */
	adrp	x0, head
	add	x0, x0, :lo12:head
	adrp	x1, _rela_start
	add	x1, x1, :lo12:_rela_start
	adrp	x2, _rela_end
	add	x2, x2, :lo12:_rela_end
	bl	apply_reloc
	cmp	x0, 0
	bne	.L2 /* Should not happen */

	adrp	x0, cpu0_stack
	add	x0, x0, :lo12:cpu0_stack
	mov	sp, x0

	/* Reset TPIDR_EL2 with panic initial state */
	mov	x10, PANIC_INITIAL_STATE
	msr	TPIDR_EL2, x10

	mov	x0, x20 /* Restore boot_mode to x0 */

	mov	x29, xzr
	mov	x30, xzr
	bl	vmm_entry
.L2:
	b	.

	.global uefi_entry_arch_call
uefi_entry_arch_call:
	adr	x10, _uefi_call_ret
	stp	x29, x30, [x10]

	mov	x10, x0 /* func */
	mov	x11, x1 /* n_args */

	mov	x0, x2
	mov	x1, x3
	mov	x2, x4
	mov	x3, x5
	mov	x4, x6
	mov	x5, x7
	cmp	x11, #7
	b.ge	.L3
	blr	x10
	b	.L4
.L3:
	ldp	x6, x7, [sp]
	add	sp, sp, #16
	blr	x10
	sub	sp, sp, #16
.L4:
	adr	x10, _uefi_call_ret
	ldp	x29, x30, [x10]
	ret

	/*
	 * entry_identity code gets identity-mapped for core entry after
	 * BitVisor start.
	 */
	.balign	PAGESIZE
	.global entry_identity
entry_identity:
	/* x0 points to top of the stack (virtual address) */
	.global entry_cpu_on
entry_cpu_on:
	msr	DAIFSet, #3 /* Disable interrupts */
	isb /* Want msr write to be effective immediately */

	adrp	x10, mmu_ttbr0_identity_table_phys
	add	x10, x10, :lo12:mmu_ttbr0_identity_table_phys
	ldr	x10, [x10]
	msr	TTBR0_EL2, x10
	isb

	adrp	x10, mmu_ttbr1_table_phys
	add	x10, x10, :lo12:mmu_ttbr1_table_phys
	ldr	x10, [x10]
	msr	TTBR1_EL2, x10
	isb

	adrp	x10, vmm_mair_host
	add	x10, x10, :lo12:vmm_mair_host
	ldr	x10, [x10]
	msr	MAIR_EL2, x10
	isb

	adrp	x10, vmm_tcr_host
	add	x10, x10, :lo12:vmm_tcr_host
	ldr	x10, [x10]
	msr	TCR_EL2, x10
	isb

	mov	x10, #1
	lsl	x10, x10, #34 /* HCR_E2H */
	msr	HCR_EL2, x10
	isb

	adrp	x10, vmm_sctlr_host
	add	x10, x10, :lo12:vmm_sctlr_host
	ldr	x10, [x10]
	/* Flush TLB */
	msr	SCTLR_EL2, x10
	dsb	ishst
	tlbi	alle2
	dsb	ish
	isb

	/* Reset TPIDR_EL2 with panic initial state */
	mov	x10, PANIC_INITIAL_STATE
	msr	TPIDR_EL2, x10

	/* We can access virtual address now */
	mov	sp, x0

	/*
	 * x0: vm
	 * x1: g_mpidr
	 * x2: g_entry
	 * x3: g_ctx_idx
	 * x4: pa_base
	 * x5: va_base
	 */
	ldp	x0, x1, [sp, #-48]
	ldp	x2, x3, [sp, #-32]
	ldp	x4, x5, [sp, #-16]

	adrp	x10, vmm_entry_cpu_on
	add	x10, x10, :lo12:vmm_entry_cpu_on
	sub	x10, x10, x4
	add	x10, x10, x5

	mov	x29, xzr
	mov	x30, xzr
	br	x10 /* Jump to vmm_entry_cpu_on with virtual address */
	b	. /* The instruction should never return */

	.balign PAGESIZE
	.global entry_identity_end
entry_identity_end:

	.section .entry.data
	.balign 16
_uefi_call_ret:
	.space 16

	.section .bss
	.balign PAGESIZE
	.space PAGESIZE * 8
	.global cpu0_stack
cpu0_stack:
