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

	/* x0 -> union exception_saved_regs *r */
	.balign 4
	.global smc_asm_passthrough_call
smc_asm_passthrough_call:
	str	x20, [sp, #-16]!
	mov	x20, x0

	/* Load registers, prepare for SMC call */
	ldp	x0, x1, [x20, #(16 * 0)]
	ldp	x2, x3, [x20, #(16 * 1)]
	ldp	x4, x5, [x20, #(16 * 2)]
	ldp	x6, x7, [x20, #(16 * 3)]
	ldp	x8, x9, [x20, #(16 * 4)]
	ldp	x10, x11, [x20, #(16 * 5)]
	ldp	x12, x13, [x20, #(16 * 6)]
	ldp	x14, x15, [x20, #(16 * 7)]
	ldp	x16, x17, [x20, #(16 * 8)]

	smc #0

	/* Save results */
	stp	x0, x1, [x20, #(16 * 0)]
	stp	x2, x3, [x20, #(16 * 1)]
	stp	x4, x5, [x20, #(16 * 2)]
	stp	x6, x7, [x20, #(16 * 3)]
	stp	x8, x9, [x20, #(16 * 4)]
	stp	x10, x11, [x20, #(16 * 5)]
	stp	x12, x13, [x20, #(16 * 6)]
	stp	x14, x15, [x20, #(16 * 7)]
	stp	x16, x17, [x20, #(16 * 8)]

	ldr	x20, [sp], #16
	ret

	/* int smc_asm_psci_call (u64, u64, u64, u64) */
	.global smc_asm_psci_call
smc_asm_psci_call:
	smc	#0
	ret
