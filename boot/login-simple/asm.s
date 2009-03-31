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

	.text
	.globl	_start
_start:
	mov	$_start,%esp
	mov	8*4,%eax
	mov	%eax,int8orig
	movl	$int8hook,8*4
	mov	9*4,%eax
	mov	%eax,int9orig
	movl	$int9hook,9*4
	push	%esi
	call	os_main
1:
	hlt
	jmp	1b
	.code16
addrand:
	push	%eax
	push	%edx
	pushf
	rdtsc
	movzwl	%cs:seedoff,%edx
	add	%eax,%cs:randseed(%edx)
	add	$4,%dx
	cmp	%cs:randseedsize,%dx
	jb	1f
	xor	%dx,%dx
1:
	mov	%dx,%cs:seedoff
	popf
	pop	%edx
	pop	%eax
	ret
int8hook:
	call	addrand
	.byte	0xEA		# JMP FAR
int8orig:
	.long	0
int9hook:
	call	addrand
	.byte	0xEA		# JMP FAR
int9orig:
	.long	0
	.code32

	.globl	decrypt
decrypt:
	push	%ebx
	mov	$loadcfgstr,%eax
	mov	8(%esp),%ebx
	call	vmmcall_name
	pop	%ebx
	ret
	.globl	boot
boot:
	push	%ebx
	mov	$bootstr,%eax
	mov	8(%esp),%ebx
	call	vmmcall_name
	pop	%ebx
	ret

# %eax: address of vmmcall name
# other registers: vmmcall arguments
vmmcall_name:
	push	%ebx
	mov	%eax,%ebx
	xor	%eax,%eax
	call	vmmcall
	pop	%ebx
	test	%eax,%eax
	je	1f
	call	vmmcall
1:
	ret
vmmcall:
	lidt	idtr2
	vmmcall
	lidt	idtr1
	ret
exception_d:
	lea	16(%esp),%esp
	vmcall
	lidt	idtr1
	ret

	.globl	call0x10
call0x10:
	push	%edi
	push	%esi
	push	%ebx
	mov	16(%esp),%eax
	mov	20(%esp),%ebx
	mov	24(%esp),%ecx
	mov	28(%esp),%edx
	call	int0x10
	pop	%ebx
	pop	%esi
	pop	%edi
	ret
	.globl	getkey
getkey:
	call	toreal
	.code16
	xor	%ax,%ax
	int	$0x16
	call	toprotected
	.code32
	ret
int0x10:
	call	toreal
	.code16
	int	$0x10
	call	toprotected
	.code32
	ret
toreal:
	pushfl
	push	%eax
	lgdtl	gdtr
	ljmpl	$0x18,$1f
1:
	.code16
	mov	$0x20,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	mov	%cr0,%eax
	and	$0xFE,%al
	mov	%eax,%cr0
	ljmp	$0,$1f
1:
	xor	%ax,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	pop	%eax
	popfl
	retl
	.code32
toprotected:
	.code16
	cli
	pushfl
	push	%eax
	lgdt	gdtr
	mov	%cr0,%eax
	or	$1,%al
	mov	%eax,%cr0
	ljmp	$0x8,$1f
1:
	.code32
	mov	$0x10,%eax
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%ss
	pop	%eax
	popfl
	retw

	.data
	.align	8
	.short	0
gdtr:
	.short	0x27
	.long	gdt
gdt:
	.quad   0
	.quad   0x00CF9B000000FFFF      # 0x08  CODE32, DPL=0
	.quad   0x00CF93000000FFFF      # 0x10  DATA32, DPL=0
	.quad   0x00009B000000FFFF      # 0x18  CODE16, DPL=0
	.quad   0x000093000000FFFF      # 0x20  DATA16, DPL=0
idtr1:
	.short	0x3FF
	.long	0
	.short	0
idtr2:
	.short	0x6F
	.long	idt
	.short	0
idt:
	.quad	0, 0, 0, 0, 0, 0, 0, 0
	.quad	0, 0, 0, 0, 0
	.short	exception_d
	.short	8
	.long	0x8E00
loadcfgstr:
	.string	"loadcfg"
bootstr:
	.string	"boot"
seedoff:
	.short	0
