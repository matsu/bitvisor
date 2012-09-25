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
	.globl	memset
	.globl	memcpy
	.globl	strcmp
	.globl	memcmp
	.globl	strlen
.if longmode
	.set	memset, memset64
	.set	memcpy, memcpy64
	.set	strcmp, strcmp64
	.set	memcmp, memcmp64
	.set	strlen, strlen64
.else
	.set	memset, memset32
	.set	memcpy, memcpy32
	.set	strcmp, strcmp32
	.set	memcmp, memcmp32
	.set	strlen, strlen32
.endif

	.code32
	.align	64
memset32:
	push	%edi
	mov	8(%esp),%edi
	mov	12(%esp),%al
	mov	16(%esp),%ecx
	cld
	mov	%al,%ah
	shr	%ecx
	jc	1f
	mov	%eax,%edx
	shl	$16,%eax
	mov	%dx,%ax
	shr	%ecx
	jc	2f
	rep	stosl
	mov	8(%esp),%eax
	pop	%edi
	ret
1:
	rep	stosw
	stosb
	mov	8(%esp),%eax
	pop	%edi
	ret
2:
	rep	stosl
	stosw
	mov	8(%esp),%eax
	pop	%edi
	ret

	.align	64
memcpy32:
	push	%esi
	push	%edi
	mov	12(%esp),%edi
	mov	16(%esp),%esi
	mov	20(%esp),%ecx
	cld
	mov	%edi,%eax
	shr	%ecx
	jc	1f
	shr	%ecx
	jc	2f
	rep	movsl
	pop	%edi
	pop	%esi
	ret
1:
	rep	movsw
	movsb
	pop	%edi
	pop	%esi
	ret
2:
	rep	movsl
	movsw
	pop	%edi
	pop	%esi
	ret

	.align	64
strcmp32:
	push	%esi
	push	%edi
	mov	12(%esp),%edi
	mov	16(%esp),%esi
	xor	%eax,%eax
	cld
1:
	lodsb
	scasb
	jne	1f
	test	%al,%al
	jne	1b
	pop	%edi
	pop	%esi
	ret
1:
	setnc	%al
	setc	%ah
	ror	%eax
	pop	%edi
	pop	%esi
	ret

	.align	64
memcmp32:
	push	%esi
	push	%edi
	mov	12(%esp),%edi
	mov	16(%esp),%esi
	mov	20(%esp),%ecx
	xor	%eax,%eax
	cld
	repe	cmpsb
	je	1f
	setnc	%al
	setc	%ah
	ror	%eax
1:
	pop	%edi
	pop	%esi
	ret

	.align	64
strlen32:
	mov	%edi,%edx
	mov	4(%esp),%edi
	cld
	mov	$-1,%ecx
	mov	$0,%al
	repne	scasb
	lea	1(%ecx),%eax
	not	%eax
	mov	%edx,%edi
	ret

	.code64
	.align	64
memset64:
	mov	%esi,%eax
	mov	%rdx,%rcx
	cld
	mov	%al,%ah
	shr	%rcx
	jc	1f
	mov	%eax,%edx
	shl	$16,%eax
	mov	%dx,%ax
	shr	%rcx
	jc	2f
	mov	%eax,%edx
	shl	$32,%rax
	or	%rdx,%rax
	shr	%rcx
	jc	3f
	mov	%rdi,%rdx
	rep	stosq
	mov	%rdx,%rax
	ret
1:
	mov	%rdi,%rdx
	rep	stosw
	stosb
	mov	%rdx,%rax
	ret
2:
	mov	%rdi,%rdx
	rep	stosl
	stosw
	mov	%rdx,%rax
	ret
3:
	mov	%rdi,%rdx
	rep	stosq
	stosl
	mov	%rdx,%rax
	ret

	.align	64
memcpy64:
	mov	%rdx,%rcx
	cld
	mov	%rdi,%rax
	shr	%rcx
	jc	1f
	shr	%rcx
	jc	2f
	shr	%rcx
	jc	3f
	rep	movsq
	ret
1:
	rep	movsw
	movsb
	ret
2:
	rep	movsl
	movsw
	ret
3:
	rep	movsq
	movsl
	ret

	.align	64
strcmp64:
	xor	%eax,%eax
	cld
1:
	lodsb
	scasb
	jne	1f
	test	%al,%al
	jne	1b
	ret
1:
	setnc	%al
	setc	%ah
	ror	%eax
	ret

	.align	64
memcmp64:
	mov	%rdx,%rcx
	xor	%eax,%eax
	cld
	repe	cmpsb
	je	1f
	setnc	%al
	setc	%ah
	ror	%eax
1:
	ret

	.align	64
strlen64:
	cld
	mov	$-1,%rcx
	mov	$0,%al
	repne	scasb
	lea	1(%rcx),%rax
	not	%rax
	ret
