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

	.include "longmode.h"

	.text
	.globl	nmi_handler

.if longmode
	# 64bit
	.align	8
nmi_handler:
	push	%gs
	push	$SEG_SEL_PCPU64
	pop	%gs
	incl	%gs:gs_nmi_count
	cmpq	$0,%gs:gs_nmi_critical
	je	1f
	push	%rax
	push	%rbx
	mov	%gs:gs_nmi_critical,%rbx
	mov	8*3(%rsp),%rax
	cmp	%rax,(%rbx)
	ja	2f
	cmp	%rax,8(%rbx)
	jbe	2f
	mov	16(%rbx),%rax
	mov	%rax,8*3(%rsp)
2:
	pop	%rbx
	pop	%rax
1:
	pop	%gs
	iretq
.else
	# 32bit
	.align	8
nmi_handler:
	push	%gs
	push	$SEG_SEL_PCPU32
	pop	%gs
	incl	%gs:gs_nmi_count
	cmpl	$0,%gs:gs_nmi_critical
	je	1f
	push	%eax
	push	%ebx
	mov	%gs:gs_nmi_critical,%ebx
	mov	4*3(%esp),%eax
	cmp	%eax,(%ebx)
	ja	2f
	cmp	%eax,4(%ebx)
	jbe	2f
	mov	8(%ebx),%eax
	mov	%eax,4*3(%esp)
2:
	pop	%ebx
	pop	%eax
1:
	pop	%gs
	iretl
.endif
