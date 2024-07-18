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
	/* x0: address of fp array */
	.global acc_emu_save_fp_regs
	.type acc_emu_save_fp_regs, @function
	.balign 4
acc_emu_save_fp_regs:
	stp	q0, q1, [x0, #(32 * 0)]
	stp	q2, q3, [x0, #(32 * 1)]
	stp	q4, q5, [x0, #(32 * 2)]
	stp	q6, q7, [x0, #(32 * 3)]
	stp	q8, q9, [x0, #(32 * 4)]
	stp	q10, q11, [x0, #(32 * 5)]
	stp	q12, q13, [x0, #(32 * 6)]
	stp	q14, q15, [x0, #(32 * 7)]
	stp	q16, q17, [x0, #(32 * 8)]
	stp	q18, q19, [x0, #(32 * 9)]
	stp	q20, q21, [x0, #(32 * 10)]
	stp	q22, q23, [x0, #(32 * 11)]
	stp	q24, q25, [x0, #(32 * 12)]
	stp	q26, q27, [x0, #(32 * 13)]
	stp	q28, q29, [x0, #(32 * 14)]
	stp	q30, q31, [x0, #(32 * 15)]
	ret

	.global acc_emu_restore_fp_regs
	.type acc_emu_restore_fp_regs, @function
	.balign 4
acc_emu_restore_fp_regs:
	ldp	q0, q1, [x0, #(32 * 0)]
	ldp	q2, q3, [x0, #(32 * 1)]
	ldp	q4, q5, [x0, #(32 * 2)]
	ldp	q6, q7, [x0, #(32 * 3)]
	ldp	q8, q9, [x0, #(32 * 4)]
	ldp	q10, q11, [x0, #(32 * 5)]
	ldp	q12, q13, [x0, #(32 * 6)]
	ldp	q14, q15, [x0, #(32 * 7)]
	ldp	q16, q17, [x0, #(32 * 8)]
	ldp	q18, q19, [x0, #(32 * 9)]
	ldp	q20, q21, [x0, #(32 * 10)]
	ldp	q22, q23, [x0, #(32 * 11)]
	ldp	q24, q25, [x0, #(32 * 12)]
	ldp	q26, q27, [x0, #(32 * 13)]
	ldp	q28, q29, [x0, #(32 * 14)]
	ldp	q30, q31, [x0, #(32 * 15)]
	ret
