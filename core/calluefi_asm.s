/*
 * Copyright (c) 2013 Igel Co., Ltd.
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

	PAGESIZE = 4096
	CR4_PGE_BIT = 0x80

	.include "longmode.h"

	.data
	.align	PAGESIZE
calluefi_pt:
	.space	PAGESIZE*4
	.globl	calluefi
calluefi:
	# NOTE: This code and additional data must be stored in the same page
.if longmode
	mov	%rsp,%rax
	mov	$calluefi_stack,%rsp
	sub	$4,%rsi
	jbe	2f
	# 16-byte stack alignment seems to be undocumented but
	# sometimes required for calling UEFI services that probably
	# use SSE2.  If the number of arguments is odd, push 8-byte
	# something to keep 16-byte stack alignment.
	test	$1,%rsi
	je	1f
	push	%rsi
1:
	pushq	(%rax,%rsi,8)
	sub	$1,%rsi
	ja	1b
2:
	push	%r9
	push	%r8
	push	%rcx
	push	%rdx
	push	%rax
	push	%rdi
	mov	$calluefi_pt,%rdi
	mov	calluefi_pt_phys,%rsi
	mov	$calluefi,%rdx
	call	fill_pagetable
	mov	%rax,calluefi_pt_phys
	sgdtq	calluefi_vmm_gdtr
	sidtq	calluefi_vmm_idtr
	sldt	calluefi_vmm_ldtr
	mov	%es,calluefi_vmm_sregs+0
	mov	%cs,calluefi_vmm_sregs+2
	mov	%ss,calluefi_vmm_sregs+4
	mov	%ds,calluefi_vmm_sregs+6
	mov	%fs,calluefi_vmm_sregs+8
	mov	%gs,calluefi_vmm_sregs+10
	mov	%cr0,%rsi
	mov	%rsi,calluefi_vmm_cr0
	mov	%cr3,%rsi
	mov	%rsi,calluefi_vmm_cr3
	mov	%cr4,%rsi
	mov	%rsi,calluefi_vmm_cr4
	and	$~CR4_PGE_BIT,%rsi
	mov	%rsi,%cr4		# CR4.PGE=0 to flush all TLB entries
	mov	calluefi_uefi_cr3,%rsi
	mov	%rax,%cr3
	# using a special page table
	sub	$calluefi_pt,%rax
	add	%rax,%rsp
	add	$1f,%rax
	jmp	*%rax
1:
	mov	%rsi,%cr3
	# using the UEFI page table
	lgdtq	calluefi_uefi_gdtr(%rip)
	lidtq	calluefi_uefi_idtr(%rip)
	lldt	calluefi_uefi_ldtr(%rip)
	mov	calluefi_uefi_sregs+0(%rip),%es
	mov	calluefi_uefi_sregs+4(%rip),%ss
	mov	calluefi_uefi_sregs+6(%rip),%ds
	mov	calluefi_uefi_sregs+8(%rip),%fs
	mov	calluefi_uefi_sregs+10(%rip),%gs
	pushq	calluefi_uefi_sregs+2(%rip)
	call	calluefi_loadcs
	clts			# for VirtualBox's UEFI
	cld
	sti
	pop	%rdi
	pop	%rsi
	mov	0x0(%rsp),%rcx
	mov	0x8(%rsp),%rdx
	mov	0x10(%rsp),%r8
	mov	0x18(%rsp),%r9
	call	*%rdi
	push	%rsi
	push	%rax
	mov	calluefi_pt_phys(%rip),%rax
	mov	calluefi_vmm_cr3(%rip),%rsi
	cli
	mov	%rax,%cr3
	# using a special page table
	sub	$calluefi_pt,%rax
	sub	%rax,%rsp
	mov	$1f,%rax
	jmp	*%rax
1:
	mov	%rsi,%cr3
	# using the VMM page table
	lgdtq	calluefi_vmm_gdtr
	lidtq	calluefi_vmm_idtr
	lldt	calluefi_vmm_ldtr
	mov	calluefi_vmm_sregs+0,%es
	mov	calluefi_vmm_sregs+4,%ss
	mov	calluefi_vmm_sregs+6,%ds
	mov	calluefi_vmm_sregs+8,%fs
	mov	calluefi_vmm_sregs+10,%gs
	pushq	calluefi_vmm_sregs+2
	call	calluefi_loadcs
	mov	calluefi_vmm_cr0,%rsi
	mov	%rsi,%cr0
	mov	calluefi_vmm_cr4,%rsi
	mov	%rsi,%cr4
	pop	%rax
	pop	%rsp
	ret
calluefi_loadcs:
	lretq
.else
.endif
calluefi_pt_phys:
	.quad	1
calluefi_vmm_gdtr:
	.space	16
calluefi_vmm_idtr:
	.space	16
calluefi_vmm_ldtr:
	.space	8
calluefi_vmm_sregs:
	.space	16
calluefi_vmm_cr0:
	.space	8
calluefi_vmm_cr3:
	.space	8
calluefi_vmm_cr4:
	.space	8
	.globl	calluefi_uefi_gdtr
calluefi_uefi_gdtr:
	.space	16
	.globl	calluefi_uefi_idtr
calluefi_uefi_idtr:
	.space	16
	.globl	calluefi_uefi_ldtr
calluefi_uefi_ldtr:
	.space	8
	.globl	calluefi_uefi_sregs
calluefi_uefi_sregs:
	.space	16
	.globl	calluefi_uefi_cr3
calluefi_uefi_cr3:
	.space	8

	.bss
	# UEFI calls are allowed on processor 0 only
	.space	128*1024	# 128KB stack space for UEFI
	.space	PAGESIZE	# some space for arguments
	.align	16		# Force 16-byte alignment; see above
calluefi_stack:
