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

	PAGESIZE = 4096
	SYS_MSGRET = 8
	SEG_SEL_CODE32U	= (3 * 8 + 3)
	SEG_SEL_DATA32U	= (4 * 8 + 3)
	SEG_SEL_PCPU32	= (8 * 8)
	SEG_SEL_CALLGATE = (9 * 8 + 3)
	SEG_SEL_DATA64	= (11 * 8)
	SEG_SEL_PCPU64	= (15 * 8)
	gs_syscallstack	= 16

	.text
	.globl	syscall_entry_sysexit
	.globl	syscall_entry_lret
	.globl	ret_to_user32
	.globl	ret_to_user64
	.globl	syscall_entry_sysret64
	.globl	processuser_sysenter
	.globl	processuser_no_sysenter
	.globl	processuser_syscall

	.code32
	.align	4
syscall_entry_sysexit:
	mov	%ss,%eax
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%fs
	mov	$SEG_SEL_PCPU32,%eax
	mov	%eax,%gs
	mov	%gs:gs_syscallstack,%esp
	pusha
	mov	%esp,%eax
	call	process_syscall
	mov	$SEG_SEL_DATA32U,%eax
	mov	%eax,%ds
	mov	%eax,%es
	popa
	sysexit

	.align	4
syscall_entry_lret:
	mov	%ss,%eax
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%fs
	mov	$SEG_SEL_PCPU32,%eax
	mov	%eax,%gs
	mov	%gs:gs_syscallstack,%esp
	pusha
	mov	%esp,%eax
	call	process_syscall
	mov	$SEG_SEL_DATA32U,%eax
	mov	%eax,%ds
	mov	%eax,%es
	popa
ret_to_user:
	pushl	$SEG_SEL_DATA32U
	push	%ecx
	pushl	$SEG_SEL_CODE32U
	push	%edx
	lret

	.align	4
ret_to_user32:
	mov	$SEG_SEL_DATA32U,%eax
	mov	%eax,%ds
	mov	%eax,%es
	jmp	ret_to_user

	.code64
	.align	8
syscall_entry_sysret64:
	mov	%rsp,%rdx
	mov	$SEG_SEL_DATA64,%eax
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%fs
	mov	%eax,%ss
	mov	$SEG_SEL_PCPU64,%eax
	mov	%eax,%gs
	mov	%gs:gs_syscallstack,%rsp
	push	%rax
	push	%rdx		# RSP
	push	%rcx		# RIP
	push	%rbx
	push	%rsp
	push	%rbp
	push	%rsi
	push	%rdi
	mov	%rsp,%rdi
	call	process_syscall
	pop	%rdi
	pop	%rsi
	pop	%rbp
	pop	%rsp
	pop	%rbx
	pop	%rcx		# RIP
	pop	%rdx		# RSP
	pop	%rax
	pushf
	pop	%r11
	mov	%rdx,%rsp
	sysretq

	.align	8
ret_to_user64:
	pushf
	pop	%r11
	mov	%rcx,%rsp
	mov	$0x3FFFF200,%rcx
	sysretq

	.code32
	.align	PAGESIZE
processuser_sysenter:				# at 0x3FFFF000
	pop	%edx
	mov	%esp,%ecx
	sysenter
	.org	processuser_sysenter + 0x100	# at 0x3FFFF100
	mov	$SYS_MSGRET,%ebx
	mov	%eax,%esi
	sysenter
	.align	PAGESIZE
processuser_no_sysenter:			# at 0x3FFFF000
	pop	%edx
	mov	%esp,%ecx
	lcall	$SEG_SEL_CALLGATE,$0
	.org	processuser_no_sysenter + 0x100	# at 0x3FFFF100
	mov	$SYS_MSGRET,%ebx
	mov	%eax,%esi
	lcall	$SEG_SEL_CALLGATE,$0
	.align	PAGESIZE

	.code64
	.align	PAGESIZE
processuser_syscall:			# at 0x3FFFF000
	syscall				# unused
	ret				#
	.org	processuser_syscall + 0x100	# at 0x3FFFF100
	mov	$SYS_MSGRET,%ebx
	mov	%rax,%rsi
	syscall
	.org processuser_syscall+0x200	# at 0x3FFFF200 (used by ret_to_user64)
	mov	%rdx,%rax
	pop	%rbx
	pop	%rdi
	pop	%rsi
	pop	%rdx
	pop	%rcx
	pop	%r8
	pop	%r9
	push	%rbx
	jmp	*%rax
	.align	PAGESIZE
