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

	RAX = 0
	RCX = 1
	RDX = 2
	RBX = 3
	CR2 = 4
	RBP = 5
	RSI = 6
	RDI = 7
	R8  = 8
	R9  = 9
	R10 = 10
	R11 = 11
	R12 = 12
	R13 = 13
	R14 = 14
	R15 = 15
	VMCS_HOST_RSP = 0x6C14
	VMCS_HOST_RIP = 0x6C16

	.code32
	.globl	asm_vmlaunch_regs_32
	.align	16
asm_vmlaunch_regs_32:
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	mov	20(%esp),%edi	# arg1
	push	%edi
	sub	$4,%esp
	mov	$VMCS_HOST_RSP,%eax
	mov	%esp,%edx
	vmwrite	%edx,%eax
	mov	$VMCS_HOST_RIP,%eax
	mov	$1f,%edx
	vmwrite	%edx,%eax
	mov	4*CR2(%edi),%eax
	mov	%eax,%cr2
	mov	4*RAX(%edi),%eax
	mov	4*RCX(%edi),%ecx
	mov	4*RDX(%edi),%edx
	mov	4*RBX(%edi),%ebx
	mov	4*RBP(%edi),%ebp
	mov	4*RSI(%edi),%esi
	mov	4*RDI(%edi),%edi
	vmlaunch
	xor	%eax,%eax
	dec	%eax
	add	$4,%esp
	pop	%edi
	jmp	2f
	.align	16
1:
	mov	%edi,(%esp)
	mov	4(%esp),%edi
	mov	%eax,4*RAX(%edi)
	mov	%ecx,4*RCX(%edi)
	mov	%edx,4*RDX(%edi)
	mov	%ebx,4*RBX(%edi)
	mov	%ebp,4*RBP(%edi)
	mov	%esi,4*RSI(%edi)
	popl	4*RDI(%edi)
	add	$4,%esp
	mov	%cr2,%eax
	mov	%eax,4*CR2(%edi)
	xor	%eax,%eax
2:
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
	.globl	asm_vmresume_regs_32
	.align	16
asm_vmresume_regs_32:
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	mov	20(%esp),%edi	# arg1
	push	%edi
	sub	$4,%esp
	mov	$VMCS_HOST_RSP,%eax
	mov	%esp,%edx
	vmwrite	%edx,%eax
	mov	$VMCS_HOST_RIP,%eax
	mov	$1f,%edx
	vmwrite	%edx,%eax
	mov	4*CR2(%edi),%eax
	mov	%eax,%cr2
	mov	4*RAX(%edi),%eax
	mov	4*RCX(%edi),%ecx
	mov	4*RDX(%edi),%edx
	mov	4*RBX(%edi),%ebx
	mov	4*RBP(%edi),%ebp
	mov	4*RSI(%edi),%esi
	mov	4*RDI(%edi),%edi
	vmresume
	xor	%eax,%eax
	dec	%eax
	add	$4,%esp
	pop	%edi
	jmp	2f
	.align	16
1:
	mov	%edi,(%esp)
	mov	4(%esp),%edi
	mov	%eax,4*RAX(%edi)
	mov	%ecx,4*RCX(%edi)
	mov	%edx,4*RDX(%edi)
	mov	%ebx,4*RBX(%edi)
	mov	%ebp,4*RBP(%edi)
	mov	%esi,4*RSI(%edi)
	popl	4*RDI(%edi)
	add	$4,%esp
	mov	%cr2,%eax
	mov	%eax,4*CR2(%edi)
	xor	%eax,%eax
2:
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
	.globl	asm_vmrun_regs_32
	.align	16
asm_vmrun_regs_32:
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	mov	28(%esp),%eax	# arg3
	vmsave
	mov	20(%esp),%eax	# arg1
	mov	4*RCX(%eax),%ecx
	mov	4*RDX(%eax),%edx
	mov	4*RBX(%eax),%ebx
	mov	4*RBP(%eax),%ebp
	mov	4*RSI(%eax),%esi
	mov	4*RDI(%eax),%edi
	mov	24(%esp),%eax	# arg2
	clgi
	vmload
	vmrun
	vmsave
	mov	20(%esp),%eax	# arg1
	mov	%ecx,4*RCX(%eax)
	mov	%edx,4*RDX(%eax)
	mov	%ebx,4*RBX(%eax)
	mov	%ebp,4*RBP(%eax)
	mov	%esi,4*RSI(%eax)
	mov	%edi,4*RDI(%eax)
	mov	28(%esp),%eax	# arg3
	vmload
	stgi
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
	.globl	asm_vmrun_regs_nested_32
	.align	16
asm_vmrun_regs_nested_32:
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	mov	28(%esp),%eax	# arg3
	vmsave
	mov	20(%esp),%eax	# arg1
	mov	4*RCX(%eax),%ecx
	mov	4*RDX(%eax),%edx
	mov	4*RBX(%eax),%ebx
	mov	4*RBP(%eax),%ebp
	mov	4*RSI(%eax),%esi
	mov	4*RDI(%eax),%edi
	mov	24(%esp),%eax	# arg2
	clgi
	vmload
	mov	32(%esp),%eax	# arg4
	testl	$-1,36(%esp)	# arg5
	je	1f
	sti
1:
	vmrun
	cli
	mov	24(%esp),%eax	# arg2
	vmsave
	mov	20(%esp),%eax	# arg1
	mov	%ecx,4*RCX(%eax)
	mov	%edx,4*RDX(%eax)
	mov	%ebx,4*RBX(%eax)
	mov	%ebp,4*RBP(%eax)
	mov	%esi,4*RSI(%eax)
	mov	%edi,4*RDI(%eax)
	mov	28(%esp),%eax	# arg3
	vmload
	stgi
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp
	ret
	.code64
	.globl	asm_vmlaunch_regs_64
	.align	16
asm_vmlaunch_regs_64:
	push	%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rdi		# arg1
	sub	$8,%rsp
	mov	$VMCS_HOST_RSP,%rax
	mov	%rsp,%rdx
	vmwrite	%rdx,%rax
	mov	$VMCS_HOST_RIP,%rax
	mov	$1f,%edx
	vmwrite	%rdx,%rax
	mov	8*CR2(%rdi),%rax
	mov	%rax,%cr2
	mov	8*R8(%rdi),%r8
	mov	8*R9(%rdi),%r9
	mov	8*R10(%rdi),%r10
	mov	8*R11(%rdi),%r11
	mov	8*R12(%rdi),%r12
	mov	8*R13(%rdi),%r13
	mov	8*R14(%rdi),%r14
	mov	8*R15(%rdi),%r15
	mov	8*RAX(%rdi),%rax
	mov	8*RCX(%rdi),%rcx
	mov	8*RDX(%rdi),%rdx
	mov	8*RBX(%rdi),%rbx
	mov	8*RBP(%rdi),%rbp
	mov	8*RSI(%rdi),%rsi
	mov	8*RDI(%rdi),%rdi
	vmlaunch
	xor	%rax,%rax
	dec	%rax
	add	$8,%rsp
	pop	%rdi
	jmp	2f
	.align	16
1:
	mov	%rdi,(%rsp)
	mov	8(%rsp),%rdi
	mov	%rax,8*RAX(%rdi)
	mov	%rcx,8*RCX(%rdi)
	mov	%rdx,8*RDX(%rdi)
	mov	%rbx,8*RBX(%rdi)
	mov	%rbp,8*RBP(%rdi)
	mov	%rsi,8*RSI(%rdi)
	popq	8*RDI(%rdi)
	add	$8,%rsp
	mov	%cr2,%rax
	mov	%rax,8*CR2(%rdi)
	mov	%r8,8*R8(%rdi)
	mov	%r9,8*R9(%rdi)
	mov	%r10,8*R10(%rdi)
	mov	%r11,8*R11(%rdi)
	mov	%r12,8*R12(%rdi)
	mov	%r13,8*R13(%rdi)
	mov	%r14,8*R14(%rdi)
	mov	%r15,8*R15(%rdi)
	xor	%rax,%rax
2:
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	pop	%rbp
	ret
	.globl	asm_vmresume_regs_64
	.align	16
asm_vmresume_regs_64:
	push	%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rdi		# arg1
	sub	$8,%rsp
	mov	$VMCS_HOST_RSP,%rax
	mov	%rsp,%rdx
	vmwrite	%rdx,%rax
	mov	$VMCS_HOST_RIP,%rax
	mov	$1f,%edx
	vmwrite	%rdx,%rax
	mov	8*CR2(%rdi),%rax
	mov	%rax,%cr2
	mov	8*R8(%rdi),%r8
	mov	8*R9(%rdi),%r9
	mov	8*R10(%rdi),%r10
	mov	8*R11(%rdi),%r11
	mov	8*R12(%rdi),%r12
	mov	8*R13(%rdi),%r13
	mov	8*R14(%rdi),%r14
	mov	8*R15(%rdi),%r15
	mov	8*RAX(%rdi),%rax
	mov	8*RCX(%rdi),%rcx
	mov	8*RDX(%rdi),%rdx
	mov	8*RBX(%rdi),%rbx
	mov	8*RBP(%rdi),%rbp
	mov	8*RSI(%rdi),%rsi
	mov	8*RDI(%rdi),%rdi
	vmresume
	xor	%rax,%rax
	dec	%rax
	add	$8,%rsp
	pop	%rdi
	jmp	2f
	.align	16
1:
	mov	%rdi,(%rsp)
	mov	8(%rsp),%rdi
	mov	%rax,8*RAX(%rdi)
	mov	%rcx,8*RCX(%rdi)
	mov	%rdx,8*RDX(%rdi)
	mov	%rbx,8*RBX(%rdi)
	mov	%rbp,8*RBP(%rdi)
	mov	%rsi,8*RSI(%rdi)
	popq	8*RDI(%rdi)
	add	$8,%rsp
	mov	%cr2,%rax
	mov	%rax,8*CR2(%rdi)
	mov	%r8,8*R8(%rdi)
	mov	%r9,8*R9(%rdi)
	mov	%r10,8*R10(%rdi)
	mov	%r11,8*R11(%rdi)
	mov	%r12,8*R12(%rdi)
	mov	%r13,8*R13(%rdi)
	mov	%r14,8*R14(%rdi)
	mov	%r15,8*R15(%rdi)
	xor	%rax,%rax
2:
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	pop	%rbp
	ret
	.code64
	.globl	asm_vmrun_regs_64
	.align	16
asm_vmrun_regs_64:
	push	%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	mov	%rdx,%rax	# arg3
	push	%rdx		# arg3
	vmsave
	push	%rdi		# arg1
	mov	%rsi,%rax	# arg2
	mov	8*R8(%rdi),%r8
	mov	8*R9(%rdi),%r9
	mov	8*R10(%rdi),%r10
	mov	8*R11(%rdi),%r11
	mov	8*R12(%rdi),%r12
	mov	8*R13(%rdi),%r13
	mov	8*R14(%rdi),%r14
	mov	8*R15(%rdi),%r15
	mov	8*RCX(%rdi),%rcx
	mov	8*RDX(%rdi),%rdx
	mov	8*RBX(%rdi),%rbx
	mov	8*RBP(%rdi),%rbp
	mov	8*RSI(%rdi),%rsi
	mov	8*RDI(%rdi),%rdi
	clgi
	vmload
	vmrun
	vmsave
	pop	%rax		# arg1
	mov	%rcx,8*RCX(%rax)
	mov	%rdx,8*RDX(%rax)
	mov	%rbx,8*RBX(%rax)
	mov	%rbp,8*RBP(%rax)
	mov	%rsi,8*RSI(%rax)
	mov	%rdi,8*RDI(%rax)
	mov	%r8,8*R8(%rax)
	mov	%r9,8*R9(%rax)
	mov	%r10,8*R10(%rax)
	mov	%r11,8*R11(%rax)
	mov	%r12,8*R12(%rax)
	mov	%r13,8*R13(%rax)
	mov	%r14,8*R14(%rax)
	mov	%r15,8*R15(%rax)
	pop	%rax		# arg3
	vmload
	stgi
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	pop	%rbp
	ret
	.globl	asm_vmrun_regs_nested_64
	.align	16
asm_vmrun_regs_nested_64:
	push	%rbp
	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	mov	%rdx,%rax	# arg3
	push	%rdx		# arg3
	vmsave
	push	%rdi		# arg1
	push	%rsi		# arg2
	push	%rcx		# arg4
	push	%r8		# arg5
	mov	%rsi,%rax	# arg2
	mov	8*R8(%rdi),%r8
	mov	8*R9(%rdi),%r9
	mov	8*R10(%rdi),%r10
	mov	8*R11(%rdi),%r11
	mov	8*R12(%rdi),%r12
	mov	8*R13(%rdi),%r13
	mov	8*R14(%rdi),%r14
	mov	8*R15(%rdi),%r15
	mov	8*RCX(%rdi),%rcx
	mov	8*RDX(%rdi),%rdx
	mov	8*RBX(%rdi),%rbx
	mov	8*RBP(%rdi),%rbp
	mov	8*RSI(%rdi),%rsi
	mov	8*RDI(%rdi),%rdi
	clgi
	vmload
	pop	%rax		# arg5
	test	%rax,%rax
	je	1f
	sti
1:
	pop	%rax		# arg4
	vmrun
	cli
	pop	%rax		# arg2
	vmsave
	pop	%rax		# arg1
	mov	%rcx,8*RCX(%rax)
	mov	%rdx,8*RDX(%rax)
	mov	%rbx,8*RBX(%rax)
	mov	%rbp,8*RBP(%rax)
	mov	%rsi,8*RSI(%rax)
	mov	%rdi,8*RDI(%rax)
	mov	%r8,8*R8(%rax)
	mov	%r9,8*R9(%rax)
	mov	%r10,8*R10(%rax)
	mov	%r11,8*R11(%rax)
	mov	%r12,8*R12(%rax)
	mov	%r13,8*R13(%rax)
	mov	%r14,8*R14(%rax)
	mov	%r15,8*R15(%rax)
	pop	%rax		# arg3
	vmload
	stgi
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx
	pop	%rbp
	ret
