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

	/* x0: address of _uefi_entry_ctx */
	.global vm_asm_start_guest
	.type vm_asm_start_guest, @function
	.balign 4
vm_asm_start_guest:
	ldp	x19, x20, [x0, #(16 * 0)]
	ldp	x21, x22, [x0, #(16 * 1)]
	ldp	x23, x24, [x0, #(16 * 2)]
	ldp	x25, x26, [x0, #(16 * 3)]
	ldp	x27, x28, [x0, #(16 * 4)]
	ldp	x29, x30, [x0, #(16 * 5)]
	/* Clear all caller-saved register */
	mov	x15, xzr
	mov	x14, xzr
	mov	x13, xzr
	mov	x12, xzr
	mov	x11, xzr
	mov	x10, xzr
	mov	x9, xzr
	mov	x8, xzr
	mov	x7, xzr
	mov	x6, xzr
	mov	x5, xzr
	mov	x4, xzr
	mov	x3, xzr
	mov	x2, xzr
	mov	x1, xzr
	mov	x0, #1 /* Return 1 as success upon entry guest */
	dsb	ish
	isb
	eret
	/* Prevent speculative execution */
	dsb	nsh
	isb

	/* x0: guest context id */
	.global vm_asm_start_guest_with_ctx_id
	.type vm_asm_start_guest_with_ctx_id, @function
	.balign 4
vm_asm_start_guest_with_ctx_id:
	mov	x30, xzr
	mov	x29, xzr
	mov	x28, xzr
	mov	x27, xzr
	mov	x26, xzr
	mov	x25, xzr
	mov	x24, xzr
	mov	x23, xzr
	mov	x22, xzr
	mov	x21, xzr
	mov	x20, xzr
	mov	x19, xzr
	mov	x18, xzr
	mov	x17, xzr
	mov	x16, xzr
	mov	x15, xzr
	mov	x14, xzr
	mov	x13, xzr
	mov	x12, xzr
	mov	x11, xzr
	mov	x10, xzr
	mov	x9, xzr
	mov	x8, xzr
	mov	x7, xzr
	mov	x6, xzr
	mov	x5, xzr
	mov	x4, xzr
	mov	x3, xzr
	mov	x2, xzr
	mov	x1, xzr
	eret
	dsb	nsh
	isb
