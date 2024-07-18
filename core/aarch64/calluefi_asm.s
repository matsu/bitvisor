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

	/* Side effect of the macro is explicit synchronization */
	.macro _flush_tlb
	dsb	ishst
	tlbi	alle2
	dsb	ish
	isb
	.endm

	/* Not thread safe */
	.balign 4
	.global calluefi_arch
calluefi_arch:
	/* Save original stack frame and link register */
	adrp	x10, _calluefi_ret
	add	x10, x10, :lo12:_calluefi_ret
	stp	x29, x30, [x10, #(16 * 0)]
	stp	x20, x21, [x10, #(16 * 1)]
	stp	x22, x23, [x10, #(16 * 2)]

	mov	x10, x0 /* func */
	mov	x11, x1 /* n_args */

	/* Switch to UEFI vector table */
	adrp	x12, calluefi_vbar
	add	x12, x12, :lo12:calluefi_vbar
	ldr	x12, [x12]

	/* Save original TTBR0 */
	mrs	x20, TTBR0_EL2

	/* Save original SPSR_EL2, ELR_EL2 and HCR_EL2 */
	mrs	x21, ELR_EL2
	mrs	x22, SPSR_EL2
	mrs	x23, HCR_EL2

	/* Only HCR_E2H is set */
	mov	x13, #0x400000000
	msr	HCR_EL2, x13
	isb /* Want msr write to be effective immediately */

	/* Prepare original UEFI TTBR0 */
	adrp	x13, calluefi_ttbr0
	add	x13, x13, :lo12:calluefi_ttbr0
	ldr	x13, [x13]

	mov	x0, x2
	mov	x1, x3
	mov	x2, x4
	mov	x3, x5
	mov	x4, x6
	mov	x5, x7
	cmp	x11, #7
	b.ge	.L1
	msr	VBAR_EL2, x12
	msr	TTBR0_EL2, x13
	_flush_tlb
	msr	DAIFClr, #3
	isb
	blr	x10
	msr	DAIFSet, #3
	isb
	b	.L2
.L1:
	ldp	x6, x7, [sp]
	add	sp, sp, #16
	msr	VBAR_EL2, x12 /* Restore original UEFI VBAR */
	msr	TTBR0_EL2, x13 /* Restore original UEFI TTBR0_EL2 */
	_flush_tlb
	msr	DAIFClr, #3 /* Enable interrupts */
	isb
	blr	x10
	msr	DAIFSet, #3 /* Disable interrupts */
	isb
	sub	sp, sp, #16
.L2:
	/* Switch back to our vector table */
	adrp	x12, exception_vector_table
	add	x12, x12, :lo12:exception_vector_table
	msr	VBAR_EL2, x12

	/* Switch back to old TTBR0_EL2 */
	msr	TTBR0_EL2, x20
	_flush_tlb

	/* Switch back to old SPSR_EL2, ELR_EL2 and HCR_EL2 */
	msr	ELR_EL2, x21
	msr	SPSR_EL2, x22
	msr	HCR_EL2, x23
	isb /* Want msr write to be effective immediately */

	/* Restore the original values */
	adrp	x10, _calluefi_ret
	add	x10, x10, :lo12:_calluefi_ret
	ldp	x29, x30, [x10, #(16 * 0)]
	ldp	x20, x21, [x10, #(16 * 1)]
	ldp	x22, x23, [x10, #(16 * 2)]
	ret

	.section .bss
	.balign 8
_calluefi_ret:
	.space 8 * 6

	.section .entry.data
	.balign 8
	.global calluefi_vbar
	.global calluefi_ttbr0
calluefi_vbar:
	.space 8
calluefi_ttbr0:
	.space 8
