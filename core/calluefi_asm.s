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
	.globl	calluefi_arch
calluefi_arch:
	# NOTE: This code must be stored in one page
.if longmode
	push	%rbp
	mov	%rsp,%rbp
	push	%r9
	push	%r8
	push	%rcx
	push	%rdx
	push	%rsi
	push	%rdi
	mov	$calluefi_pt,%rdi
	mov	calluefi_pt_phys,%rsi
	mov	$calluefi_arch,%rdx
	call	fill_pagetable
	mov	%rax,calluefi_pt_phys
	sub	$uefi_entry_save_regs_size,%rsp
	mov	%rsp,%rdi
	call	uefi_entry_save_regs
	mov	%cr0,%rsi
	push	%rsi			# VMM %cr0
	mov	%cr4,%rsi
	push	%rsi			# VMM %cr4
	mov	%cr3,%rdi
	push	%rdi			# VMM %cr3
	push	%rax			# Return value of fill_pagetable()
	and	$~CR4_PGE_BIT,%rsi
	mov	%rsi,%cr4		# CR4.PGE=0 to flush all TLB entries
	mov	uefi_entry_cr3,%rsi
	mov	%rax,%cr3
	# using a special page table
	sub	$calluefi_pt,%rax
	add	%rax,%rsp
	add	%rax,%rbp
	add	$1f,%rax
	jmp	*%rax
1:
	mov	%rsi,%cr3
	# using the UEFI page table
	mov	%rsp,%rsi
	mov	uefi_entry_rsp(%rip),%rdi
	mov	%rdi,%rsp
	call	uefi_entry_restore_regs
	clts			# for VirtualBox's UEFI
	cld
	sti
	# 16-byte stack alignment is required.  It was not documented
	# in UEFI Spec 2.1.
	and	$~0xF,%rsp	# Force 16-byte alignment
	mov	-8*1(%rbp),%r9
	mov	-8*2(%rbp),%r8
	mov	-8*3(%rbp),%rdx	# Originally %rcx
	mov	-8*4(%rbp),%rcx	# Originally %rdx
	mov	-8*5(%rbp),%eax	# Originally %rsi, uint num_args
	mov	-8*6(%rbp),%rdi	# ulong func
	sub	$4,%eax
	jbe	2f
	# If the number of arguments is odd, push 8-byte something to
	# keep 16-byte stack alignment.
	test	$1,%al
	je	1f
	push	%rax
1:
	pushq	8(%rbp,%rax,8)
	sub	$1,%eax
	ja	1b
2:
	push	%r9
	push	%r8
	push	%rdx
	push	%rcx
	call	*%rdi
	mov	uefi_entry_rsp(%rip),%rdi
	mov	%rdi,%rsp
	call	uefi_entry_save_regs
	cli
	mov	%rsi,%rsp
	pop	%rdi			# Return value of fill_pagetable()
	pop	%rsi			# VMM %cr3
	mov	%rdi,%cr3
	# using a special page table
	sub	$calluefi_pt,%rdi
	sub	%rdi,%rsp
	sub	%rdi,%rbp
	mov	$1f,%rdi
	jmp	*%rdi
1:
	mov	%rsi,%cr3
	# using the VMM page table
	pop	%rsi			# VMM %cr4
	mov	%rsi,%cr4
	pop	%rsi			# VMM %cr0
	mov	%rsi,%cr0
	mov	%rsp,%rdi
	call	uefi_entry_restore_regs
	mov	%rbp,%rsp
	pop	%rbp
	ret
.else
.endif
calluefi_pt_phys:
	.quad	1
