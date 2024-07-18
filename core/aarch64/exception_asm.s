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

	.balign 4
_restore_regs_and_eret:
	ldp	x0, x1, [sp, #(16 * 16)]
	msr	ELR_EL2, x0
	msr	SPSR_EL2, x1
	ldp	x0, x1, [sp, #(16 * 17)]
	msr	FAR_EL2, x0
	msr	ESR_EL2, x1
	ldp	x0, x1, [sp, #(16 * 18)]
	msr	HCR_EL2, x0
	msr	SP_EL0, x1
	isb /* Want HCR_EL2 an others write to have effect after this */
	ldp	x0, xzr, [sp, #(16 * 19)]
	msr	TPIDR_EL0, x0
	ldp	x0, x1, [sp, #(16 * 0)]
	ldp	x2, x3, [sp, #(16 * 1)]
	ldp	x4, x5, [sp, #(16 * 2)]
	ldp	x6, x7, [sp, #(16 * 3)]
	ldp	x8, x9, [sp, #(16 * 4)]
	ldp	x10, x11, [sp, #(16 * 5)]
	ldp	x12, x13, [sp, #(16 * 6)]
	ldp	x14, x15, [sp, #(16 * 7)]
	ldp	x16, x17, [sp, #(16 * 8)]
	ldp	x18, x19, [sp, #(16 * 9)]
	ldp	x20, x21, [sp, #(16 * 10)]
	ldp	x22, x23, [sp, #(16 * 11)]
	ldp	x24, x25, [sp, #(16 * 12)]
	ldp	x26, x27, [sp, #(16 * 13)]
	ldp	x28, x29, [sp, #(16 * 14)]
	ldp	x30, xzr, [sp, #(16 * 15)]
	add	sp, sp, #(16 * 20)
	eret
	/*
	 * It is possible that the processor can speculatively execute beyond
	 * eret. Need the following to prevent this according to
	 * ARM-Trusted-firmware implementation. Another solution is to use
	 * sb instruction. However, it may or may not be available as it is
	 * optional until Armv8.5.
	 */
	dsb	nsh
	isb

	.macro _handle_exception handler_function
	/*
	 * Need to disable PAN on entry. Otherwise, it can results in stall.
	 * isb following msr as we want the effect immediately.
	 */
	msr	PAN, #0
	isb

	/* Save registers on stack */
	sub	sp, sp, #(16 * 20)
	stp	x0, x1, [sp, #(16 * 0)]
	stp	x2, x3, [sp, #(16 * 1)]
	stp	x4, x5, [sp, #(16 * 2)]
	stp	x6, x7, [sp, #(16 * 3)]
	stp	x8, x9, [sp, #(16 * 4)]
	stp	x10, x11, [sp, #(16 * 5)]
	stp	x12, x13, [sp, #(16 * 6)]
	stp	x14, x15, [sp, #(16 * 7)]
	stp	x16, x17, [sp, #(16 * 8)]
	stp	x18, x19, [sp, #(16 * 9)]
	stp	x20, x21, [sp, #(16 * 10)]
	stp	x22, x23, [sp, #(16 * 11)]
	stp	x24, x25, [sp, #(16 * 12)]
	stp	x26, x27, [sp, #(16 * 13)]
	stp	x28, x29, [sp, #(16 * 14)]
	stp	x30, xzr, [sp, #(16 * 15)]
	mrs	x0, ELR_EL2
	mrs	x1, SPSR_EL2
	stp	x0, x1, [sp, #(16 * 16)]
	mrs	x0, FAR_EL2
	mrs	x1, ESR_EL2
	stp	x0, x1, [sp, #(16 * 17)]
	mrs	x0, HCR_EL2
	mrs	x1, SP_EL0
	stp	x0, x1, [sp, #(16 * 18)]
	mrs	x0, TPIDR_EL0
	stp	x0, xzr, [sp, #(16 * 19)]

	/* Set base frame register to NULL as the bottom of the stack */
	mov	x29, xzr

	mov	x0, sp /* x0 points to exception_save_regs */
	bl	\handler_function

	b	_restore_regs_and_eret
	.endm

	.balign 4
_handle_unsupported_exception:
	_handle_exception exception_unsupported

	.balign 4
_handle_sync_exception:
	_handle_exception exception_sync

	.balign 4
_handle_irq_exception:
	_handle_exception exception_irq

	.balign 4
_handle_fiq_exception:
	_handle_exception exception_fiq

	.balign 4
_handle_serror_exception:
	_handle_exception exception_serror

	.global exception_vector_table
	.balign 0x1000 /* Make sure it is 2048-byte alignment */
exception_vector_table:
	/* === Current EL with SP0 start === */
	/* Synchronous */
	b	_handle_unsupported_exception
	/* IRQ/vIRQ */
	.balign 0x80 /* Each vector entry is 128 byte alignment */
	b	_handle_unsupported_exception
	/* FIQ/vFIQ */
	.balign 0x80
	b	_handle_unsupported_exception
	/* SError/vSError */
	.balign 0x80
	b	_handle_unsupported_exception
	/* === Current EL with SP0 End === */

	/* === Current EL with SPx start === */
	/* Synchronous */
	.balign 0x80
	b	_handle_sync_exception
	/* IRQ/vIRQ */
	.balign 0x80
	b	_handle_irq_exception
	/* FIQ/vFIQ */
	.balign 0x80
	b	_handle_fiq_exception
	/* SError/vSError */
	.balign 0x80
	b	_handle_serror_exception
	/* === Current EL with SPx End === */

	/* === Lower EL AArch64 start === */
	.balign 0x80
	/* Synchronous */
	b	_handle_sync_exception
	.balign 0x80
	/* IRQ/vIRQ */
	b	_handle_irq_exception
	.balign 0x80
	/* FIQ/vFIQ */
	b	_handle_fiq_exception
	.balign 0x80
	/* SError/vSError */
	b	_handle_serror_exception
	/* === Lower EL AArch64 end === */

	/* === Lower EL AArch32 start === */
	.balign 0x80
	/* Synchronous */
	b	_handle_unsupported_exception
	.balign 0x80
	/* IRQ/vIRQ */
	b	_handle_unsupported_exception
	.balign 0x80
	/* FIQ/vFIQ */
	b	_handle_unsupported_exception
	.balign 0x80
	/* SError/vSError */
	b	_handle_unsupported_exception
	/* === Lower EL AArch32 end === */
