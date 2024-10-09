/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

	PAGESIZE	= 4096
	SYS_MSGRET	= 8

	.text
	.global process_asm_return_from_proc
process_asm_return_from_proc:
	/* Restore process return sp */
	mrs	x10, SP_EL1
	mov	sp, x10

	/* Restore the context after running a process */
	ldp	x19, x20, [sp, #(16 * 0)]
	ldp	x21, x22, [sp, #(16 * 1)]
	ldp	x23, x24, [sp, #(16 * 2)]
	ldp	x25, x26, [sp, #(16 * 3)]
	ldp	x27, x28, [sp, #(16 * 4)]
	ldp	x29, x30, [sp, #(16 * 5)]
	add	sp, sp, #(16 * 6)

	/* Clean up registers */
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
	ret /* Return x0 value from a process */

	.global process_asm_enter_el0
process_asm_enter_el0:
	/* Save the current context before running a process */
	sub	sp, sp, #(16 * 6)
	stp	x19, x20, [sp, #(16 * 0)]
	stp	x21, x22, [sp, #(16 * 1)]
	stp	x23, x24, [sp, #(16 * 2)]
	stp	x25, x26, [sp, #(16 * 3)]
	stp	x27, x28, [sp, #(16 * 4)]
	stp	x29, x30, [sp, #(16 * 5)]

	/* Save process return sp */
	mov	x10, sp
	msr	SP_EL1, x10

	/* After a process returns, it goes to 0x3FFFF100 */
	mov	x30, #0xF100
	movk	x30, #0x3FFF, lsl #16

	/* Clear registers before jumping to EL0 */
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
	eret

	.balign PAGESIZE
	.global process_asm_processuser_syscall
process_asm_processuser_syscall:	/* at 0x3FFFF000 */
	b	. /* We don't expect a process would enter here for aarch64 */
	svc	0
	ret

	/* at 0x3FFFF100 */
	.org process_asm_processuser_syscall + 0x100
	mov	x8, SYS_MSGRET
	svc	0
