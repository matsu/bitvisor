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

# multiple precision arithmetic
	.include "longmode.h"

	.text
	.globl	mpumul_64_64
	.globl	mpudiv_128_32
	.globl	ipchecksum

# void mpumul_64_64 (u64 m1, u64 m2, u64 ans[2]);
# u32 mpudiv_128_32 (u64 d1[2], u32 d2, u64 quotient[2]);
# u16 ipchecksum (void *buf, u32 len);

.if longmode
	.code64
	.align	16
mpumul_64_64:
	mov	%rdi,%rax	# m1 -> rax
	mov	%rdx,%rdi	# ans -> rdi
	mul	%rsi		# rax * m2 -> rdx:rax
	mov	%rax,0(%rdi)	# rax -> ans[0]
	mov	%rdx,8(%rdi)	# rdx -> ans[1]
	ret
	.align	16
mpudiv_128_32:
	mov	%rdx,%rcx	# quotient -> rcx
	xor	%rdx,%rdx	# 0 -> rdx
	mov	%esi,%esi	# d2 (32bit) -> rsi
	mov	8(%rdi),%rax	# d1[1] -> rax
	div	%rsi		# rdx:rax % rsi -> rdx, rdx:rax / rsi -> rax
	mov	%rax,8(%rcx)	# rax -> quotient[1]
	mov	0(%rdi),%rax	# d1[0] -> rax
	div	%rsi		# rdx:rax % rsi -> rdx, rdx:rax / rsi -> rax
	mov	%rax,0(%rcx)	# rax -> quotient[0]
	mov	%rdx,%rax	# return rdx
	ret
	.align	16
ipchecksum:
	mov	%esi,%ecx	# len (32bit) -> rcx
	mov	%rdi,%rsi	# buf -> rsi
	mov	$-1,%rdi
	xor	%rdx,%rdx
	cld
1:
	shr	%ecx		# len bit0 test
	jnc	1f
	shl	$8,%rdi
1:
	test	$6,%esi		# rsi bit1 and bit2 test
	je	1f
	test	%ecx,%ecx
	je	1f
	xor	%eax,%eax
2:
	lodsw
	add	%eax,%edx
	sub	$1,%ecx
	je	1f
	test	$6,%esi
	jne	2b
1:
	shr	%ecx		# len bit1 test
	jnc	1f
	shl	$16,%rdi
1:
	shr	%ecx		# len bit2 test
	jnc	1f
	shl	$32,%rdi
1:
	not	%rdi
	test	%ecx,%ecx
	je	1f
2:
	lodsq
	add	%rax,%rdx
	adc	$0,%rdx
	sub	$1,%ecx
	jne	2b
1:
	lodsq
	and	%rdi,%rax
	add	%rdx,%rax
	adc	$0,%rax
	mov	%eax,%edx
	shr	$32,%rax
	add	%edx,%eax
	adc	$0,%eax
	mov	%eax,%edx
	shr	$16,%eax
	add	%dx,%ax
	adc	$0,%ax
1:
	xor	$~0,%ax
	je	1b
	ret
.else
	.code32
	# 0=ret 4=m1l 8=m1h 12=m2l 16=m2h 20=ans[]
	.align	16
mpumul_64_64:
	mov	20(%esp),%ecx	# ans
	mov	4(%esp),%eax	# m1l
	mov	12(%esp),%edx	# m2l
	mul	%edx		# m1l * m2l
	mov	%eax,0(%ecx)
	mov	%edx,4(%ecx)
	mov	8(%esp),%eax	# m1h
	mov	16(%esp),%edx	# m2h
	mul	%edx		# m1h * m2h
	mov	%eax,8(%ecx)
	mov	%edx,12(%ecx)
	mov	4(%esp),%eax	# m1l
	mov	16(%esp),%edx	# m2h
	mul	%edx		# m1l * m2h
	add	%eax,4(%ecx)
	adc	%edx,8(%ecx)
	adcl	$0,12(%ecx)
	mov	8(%esp),%eax	# m1h
	mov	12(%esp),%edx	# m2l
	mul	%edx		# m1h * m2l
	add	%eax,4(%ecx)
	adc	%edx,8(%ecx)
	adcl	$0,12(%ecx)
	ret
	.align	16
mpudiv_128_32:
	push	%edi
	push	%esi
	# 8=ret 12=d1[] 16=d2 20=quotient[]
	mov	12(%esp),%edi	# d1 -> edi
	mov	16(%esp),%esi	# d2 -> esi
	mov	20(%esp),%ecx	# quotient -> ecx
	xor	%edx,%edx	# 0 -> edx
	mov	12(%edi),%eax	# ((u32*)d1)[3] -> eax
	div	%esi		# edx:eax % esi -> edx, edx:eax / esi -> eax
	mov	%eax,12(%ecx)	# eax -> ((u32*)quotient)[3]
	mov	8(%edi),%eax	# ((u32*)d1)[2] -> eax
	div	%esi		# edx:eax % esi -> edx, edx:eax / esi -> eax
	mov	%eax,8(%ecx)	# eax -> ((u32*)quotient)[2]
	mov	4(%edi),%eax	# ((u32*)d1)[1] -> eax
	div	%esi		# edx:eax % esi -> edx, edx:eax / esi -> eax
	mov	%eax,4(%ecx)	# eax -> ((u32*)quotient)[1]
	mov	0(%edi),%eax	# ((u32*)d1)[0] -> eax
	div	%esi		# edx:eax % esi -> edx, edx:eax / esi -> eax
	mov	%eax,0(%ecx)	# eax -> ((u32*)quotient)[0]
	mov	%edx,%eax	# return edx
	pop	%esi
	pop	%edi
	ret
	.align	16
ipchecksum:
	push	%edi
	push	%esi
	mov	16(%esp),%ecx	# len -> ecx
	mov	12(%esp),%esi	# buf -> esi
	mov	$-1,%edi
	xor	%edx,%edx
	cld
1:
	shr	%ecx		# len bit0 test
	jnc	1f
	shl	$8,%edi
1:
	test	$2,%esi		# esi bit1 test
	je	1f
	test	%ecx,%ecx
	je	1f
	xor	%eax,%eax
2:
	lodsw
	add	%eax,%edx
	sub	$1,%ecx
	je	1f
	test	$2,%esi
	jne	2b
1:
	shr	%ecx		# len bit1 test
	jnc	1f
	shl	$16,%edi
1:
	not	%edi
	test	%ecx,%ecx
	je	1f
2:
	lodsl
	add	%eax,%edx
	adc	$0,%edx
	sub	$1,%ecx
	jne	2b
1:
	lodsl
	and	%edi,%eax
	add	%edx,%eax
	adc	$0,%eax
	mov	%eax,%edx
	shr	$16,%eax
	add	%dx,%ax
	adc	$0,%ax
1:
	xor	$~0,%ax
	je	1b
	pop	%esi
	pop	%edi
	ret
.endif
