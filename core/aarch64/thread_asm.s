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

	.global thread_arch_switch
	.type thread_arch_switch, @function
	.balign 4
thread_arch_switch:
	/* Save the old context */
	sub	sp, sp, #(16 * 7)
	mrs	x10, SP_EL0
	mrs	x11, SP_EL1
	stp	x19, x20, [sp, #(16 * 0)]
	stp	x21, x22, [sp, #(16 * 1)]
	stp	x23, x24, [sp, #(16 * 2)]
	stp	x25, x26, [sp, #(16 * 3)]
	stp	x27, x28, [sp, #(16 * 4)]
	stp	x29, x30, [sp, #(16 * 5)]
	stp	x10, x11, [sp, #(16 * 6)]
	mov	x10, sp
	str	x10, [x0]
	/* Set return arg */
	mov	x0, x2
	/* Switch to the new context */
	mov	sp, x1
	ldp	x19, x20, [sp, #(16 * 0)]
	ldp	x21, x22, [sp, #(16 * 1)]
	ldp	x23, x24, [sp, #(16 * 2)]
	ldp	x25, x26, [sp, #(16 * 3)]
	ldp	x27, x28, [sp, #(16 * 4)]
	ldp	x29, x30, [sp, #(16 * 5)]
	ldp	x10, x11, [sp, #(16 * 6)]
	msr	SP_EL0, x10
	msr	SP_EL1, x11
	add	sp, sp, #(16 * 7)
	ret

/*
 * The thread_arch_switch() pops all thread_context data. The remaining on the
 * stack are 'func' and 'arg'. We can load them into x0 and x1 respectively.
 */
	.global thread_asm_start0
	.type thread_asm_start0, @function
	.balign 4
thread_asm_start0:
	ldp	x0, x1, [sp], #16
	b	thread_start1
