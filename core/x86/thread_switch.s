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

	.include "longmode.h"
	.text

	.globl	thread_switch
	.globl	thread_start0
.if longmode
	.set	thread_switch, thread_switch64
	.set	thread_start0, thread_start064
.else
	.set	thread_switch, thread_switch32
	.set	thread_start0, thread_start032
.endif

	.code32
	EBP_ARG1 = 8
	EBP_ARG2 = 12
	EBP_ARG3 = 16
	.align	16
thread_switch32:
	push	%ebp
	mov	%esp,%ebp
	push	%ebx
	push	%esi
	push	%edi
	mov	EBP_ARG3(%ebp),%eax
	lea	-8(%esp),%esp
	mov	EBP_ARG1(%ebp),%ebx
	mov	%esp,(%ebx)
	mov	EBP_ARG2(%ebp),%esp
	lea	8(%esp),%esp
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret

	.align	16
thread_start032:
	push	$0
	jmp	thread_start1

	.code64
	.align	16
thread_switch64:
	push	%rbp
	push	%rbx
	push	%r15
	push	%r14
	push	%r13
	push	%r12
	mov	%rdx,%rax
	mov	%rsp,(%rdi)
	mov	%rsi,%rsp
	pop	%r12
	pop	%r13
	pop	%r14
	pop	%r15
	pop	%rbx
	pop	%rbp
	ret

	.align	16
thread_start064:
	pop	%rdi
	pop	%rsi
	jmp	thread_start1
