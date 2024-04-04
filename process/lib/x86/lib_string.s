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
	.globl	strncmp
.if longmode
	.set	memset, memset64
	.set	memcpy, memcpy64
	.set	strcmp, strcmp64
	.set	memcmp, memcmp64
	.set	strlen, strlen64
	.set	strncmp, strncmp64
.else
	.set	memset, memset32
	.set	memcpy, memcpy32
	.set	strcmp, strcmp32
	.set	memcmp, memcmp32
	.set	strlen, strlen32
	.set	strncmp, strncmp32
.endif

.if !longmode
	.code32
	.align	4
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

	.align	4
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

	.align	4
strcmp32:
	mov	4(%esp),%edx
	mov	8(%esp),%ecx
	.align	4
1:
	mov	(%ecx),%al
	cmp	(%edx),%al
	jne	1f
	inc	%ecx
	inc	%edx
	test	%al,%al
	jne	1b
	xor	%eax,%eax
	ret
1:
	sbb	%eax,%eax
	xor	$-2,%eax
	ret

	.align	4
memcmp32:
	push	%edi
	mov	8(%esp),%edi
	mov	12(%esp),%ecx
	mov	16(%esp),%edx
	test	%edx,%edx
	je	3f
	test	$3,%edx
	jne	2f
	.align	4
1:
	mov	(%edi),%eax
	cmp	(%ecx),%eax
	jne	2f
	add	$4,%edi
	add	$4,%ecx
	sub	$4,%edx
	jne	1b
	xor	%eax,%eax
	pop	%edi
	ret
	.align	4
2:
	mov	(%edi),%al
	sub	(%ecx),%al
	jne	1f
	inc	%edi
	inc	%ecx
	dec	%edx
	jne	2b
3:
	xor	%eax,%eax
	pop	%edi
	ret
1:
	movsbl	%al,%eax
	pop	%edi
	ret

	.align	4
strlen32:
	xor	%eax,%eax
	mov	4(%esp),%edx
	lea	8(%edx),%ecx
	cmp	0(%edx),%al
	je	0f
	cmp	1(%edx),%al
	je	1f
	cmp	2(%edx),%al
	je	2f
	cmp	3(%edx),%al
	je	3f
	cmp	4(%edx),%al
	je	4f
	cmp	5(%edx),%al
	je	5f
	cmp	6(%edx),%al
	je	6f
	cmp	7(%edx),%al
	je	7f
	.align	4
8:
	mov	%ecx,%edx
	lea	8(%ecx),%ecx
	cmp	0(%edx),%al
	je	0f
	cmp	1(%edx),%al
	je	1f
	cmp	2(%edx),%al
	je	2f
	cmp	3(%edx),%al
	je	3f
	cmp	4(%edx),%al
	je	4f
	cmp	5(%edx),%al
	je	5f
	cmp	6(%edx),%al
	je	6f
	cmp	7(%edx),%al
	jne	8b
7:
	inc	%eax
6:
	inc	%eax
5:
	inc	%eax
4:
	inc	%eax
3:
	inc	%eax
2:
	inc	%eax
1:
	inc	%eax
0:
	sub	4(%esp),%edx
	add	%edx,%eax
	ret

	.align	4
strncmp32:
	push	%edi
	mov	8(%esp),%edi
	mov	12(%esp),%ecx
	mov	16(%esp),%edx
	.align	4
1:
	sub	$1,%edx
	jb	2f
	mov	(%ecx),%al
	cmp	(%edi),%al
	jne	1f
	inc	%ecx
	inc	%edi
	test	%al,%al
	jne	1b
2:
	xor	%eax,%eax
	pop	%edi
	ret
1:
	sbb	%eax,%eax
	xor	$-2,%eax
	pop	%edi
	ret

.else
	.code64
	.align	4
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

	.align	4
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

	.align	4
strcmp64:
	mov	(%rsi),%al
	cmp	(%rdi),%al
	jne	1f
	inc	%rsi
	inc	%rdi
	test	%al,%al
	jne	strcmp64
	xor	%eax,%eax
	ret
1:
	sbb	%eax,%eax
	xor	$-2,%eax
	ret

	.align	4
memcmp64:
	test	%rdx,%rdx
	je	3f
	test	$3,%rdx
	jne	2f
	.align	4
1:
	mov	(%rdi),%eax
	cmp	(%rsi),%eax
	jne	2f
	add	$4,%rdi
	add	$4,%rsi
	sub	$4,%rdx
	jne	1b
	xor	%eax,%eax
	ret
	.align	4
2:
	mov	(%rdi),%al
	sub	(%rsi),%al
	jne	1f
	inc	%rdi
	inc	%rsi
	dec	%rdx
	jne	2b
3:
	xor	%eax,%eax
	ret
1:
	movsbl	%al,%eax
	ret

	.align	4
strlen64:
	xor	%eax,%eax
	mov	%rdi,%rdx
	lea	8(%rdi),%rsi
	cmp	0(%rdi),%al
	je	0f
	cmp	1(%rdi),%al
	je	1f
	cmp	2(%rdi),%al
	je	2f
	cmp	3(%rdi),%al
	je	3f
	cmp	4(%rdi),%al
	je	4f
	cmp	5(%rdi),%al
	je	5f
	cmp	6(%rdi),%al
	je	6f
	cmp	7(%rdi),%al
	je	7f
	.align	4
8:
	mov	%rsi,%rdi
	lea	8(%rsi),%rsi
	cmp	0(%rdi),%al
	je	0f
	cmp	1(%rdi),%al
	je	1f
	cmp	2(%rdi),%al
	je	2f
	cmp	3(%rdi),%al
	je	3f
	cmp	4(%rdi),%al
	je	4f
	cmp	5(%rdi),%al
	je	5f
	cmp	6(%rdi),%al
	je	6f
	cmp	7(%rdi),%al
	jne	8b
7:
	inc	%eax
6:
	inc	%eax
5:
	inc	%eax
4:
	inc	%eax
3:
	inc	%eax
2:
	inc	%eax
1:
	inc	%eax
0:
	sub	%rdx,%rdi
	add	%rdi,%rax
	ret

	.align	4
strncmp64:
	sub	$1,%rdx
	jb	2f
	mov	(%rsi),%al
	cmp	(%rdi),%al
	jne	1f
	inc	%rsi
	inc	%rdi
	test	%al,%al
	jne	strncmp64
2:
	xor	%eax,%eax
	ret
1:
	sbb	%eax,%eax
	xor	$-2,%eax
	ret
.endif
