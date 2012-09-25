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

	SEG_SEL_PCPU32	= (8 * 8)
	SEG_SEL_PCPU64	= (15 * 8)
	gs_inthandling	= 0

	.include "longmode.h"

	.text
	.globl	int_handler
	.globl	int_callfunc

.if longmode
	# 64bit
	.align	8
int_handler:
	push	%gs
	push	$SEG_SEL_PCPU64
	pop	%gs
	cmpq	$0,%gs:gs_inthandling
	je	1f
	pop	%rax
	pop	%rax		# interrupt number
	mov	%gs:gs_inthandling,%rsp
	ret			# return
1:
	push	$0
	push	$0
	push	$0
	push	$0
	push	%fs
	push	%rax
	mov	%ds,%rax
	mov	%rax,8*2(%rsp)
	mov	%ss,%rax
	mov	%rax,8*3(%rsp)
	mov	%cs,%rax
	mov	%rax,8*4(%rsp)
	mov	%es,%rax
	mov	%rax,8*5(%rsp)
	push	%rcx
	push	%rdx
	push	%rbx
	push	%rsp
	push	%rbp
	push	%rsi
	push	%rdi
	push	%r8
	push	%r9
	push	%r10
	push	%r11
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	mov	%ss,%rax
	mov	%rax,%ds
	mov	%rax,%es
	mov	%rax,%fs
	mov	%rsp,%rdi
	call	int_fatal
	cli
	hlt

	.align	8
int_callfunc:
	push	%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	$1f
	mov	%rsp,%gs:gs_inthandling
	call	*%rsi
	add	$56,%rsp
	mov	$-1,%eax
	movq	$0,%gs:gs_inthandling
	ret
	.align	8
1:
	movq	$0,%gs:gs_inthandling
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	pop	%rbp
	ret
.else
	# 32bit
	.align	8
int_handler:
	push	%gs
	push	$SEG_SEL_PCPU32
	pop	%gs
	cmpl	$0,%gs:gs_inthandling
	je	1f
	pop	%eax
	pop	%eax		# interrupt number
	mov	%gs:gs_inthandling,%esp
	ret			# return
1:
	push	%es
	push	%cs
	push	%ss
	push	%ds
	push	%fs
	pusha
	push	$0
	push	$0
	push	$0
	push	$0
	push	$0
	push	$0
	push	$0
	push	$0
	mov	%ss,%eax
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%fs
	push	%esp
	call	int_fatal
	cli
	hlt

	.align	8
int_callfunc:
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	push	$1f
	mov	%esp,%gs:gs_inthandling
	mov	24(%esp),%eax
	mov	28(%esp),%edx
	push	%eax
	call	*%edx
	add	$24,%esp
	mov	$-1,%eax
	movl	$0,%gs:gs_inthandling
	ret
	.align	8
1:
	movl	$0,%gs:gs_inthandling
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
.endif
