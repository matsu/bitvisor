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

	MSR_IA32_EFER = 0xC0000080
	MSR_IA32_EFER_LME_BIT = 0x100
	SEG_SEL_CODE32 = ( 1 * 8)
	SEG_SEL_DATA32 = ( 2 * 8)
	SEG_SEL_CODE64 = (10 * 8)
	SEG_SEL_DATA64 = (11 * 8)

	.include "longmode.h"

	.text

wakeup_protected:
.if longmode
	mov	$SEG_SEL_DATA64,%ax
.else
	mov	$SEG_SEL_DATA32,%ax
.endif
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%ss
	# get lock
	mov	$1,%al
2:
	pause
	xchg	%al,wakeup_lock
	test	%al,%al
	jne	2b
	# restore segments
	mov	$wakeup_stack,%esp
	call	wakeup_main
	mov	%eax,%esp
	# unlock
	mov	$0,%al
	xchg	%al,wakeup_lock
	jmp	wakeup_cont

	.data

wakeup_lock:
	.long	0
	.space	4096
wakeup_stack:

	.globl	wakeup_entry_start
wakeup_entry_start:
	.code16
	cli
	in	$0x92,%al
	or	$2,%al
	out	%al,$0x92
	mov	$0,%ebx
	.globl	wakeup_entry_addr
	.set	wakeup_entry_addr, . - 4
	ror	$4,%ebx
	mov	%bx,%ds
	shr	$28,%ebx
	lgdtl	wakeup_entry_gdtr-wakeup_entry_start(%bx)
.if longmode
	mov	$MSR_IA32_EFER,%ecx
	rdmsr
	or	$MSR_IA32_EFER_LME_BIT,%eax
	wrmsr
.endif
	mov	$0,%eax
	.globl	wakeup_entry_cr4
	.set	wakeup_entry_cr4, . - 4
	mov	%eax,%cr4
	mov	$0,%eax
	.globl	wakeup_entry_cr3
	.set	wakeup_entry_cr3, . - 4
	mov	%eax,%cr3
	mov	$0,%eax
	.globl	wakeup_entry_cr0
	.set	wakeup_entry_cr0, . - 4
	mov	%eax,%cr0
.if longmode
	ljmpl	$SEG_SEL_CODE64,$wakeup_protected
.else
	ljmpl	$SEG_SEL_CODE32,$wakeup_protected
.endif
	.globl	wakeup_entry_gdtr
wakeup_entry_gdtr:
	.space	10
	.globl	wakeup_entry_end
wakeup_entry_end:
