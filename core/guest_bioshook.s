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

	.data
	.code16

	.globl	guest_int0x15_e801_fake_ax
	.globl	guest_int0x15_e801_fake_bx
	.globl	guest_int0x15_e820_data_minus0x18
	.globl	guest_int0x15_e820_end
	.globl	guest_int0x15_orig
	.globl	guest_int0x15_hook
	.globl	guest_int0x15_hook_end

guest_int0x15_hook:
	pushf
	cmp	$0xE820,%ax
	je	hooke820
	cmp	$0xE801,%ax
	je	hooke801
	cmp	$0xE881,%ax
	je	errret
	cmp	$0x88,%ah
	jne	jmporig
errret:
	popf
	stc
	mov	$0x86,%ah
	lret	$2
hooke801:
	mov	$0,%ax
	.set	guest_int0x15_e801_fake_ax, . - 2
	mov	$0,%bx
	.set	guest_int0x15_e801_fake_bx, . - 2
	mov	%ax,%cx
	mov	%bx,%dx
	jmp	okret
jmporig_e820:
	pop	%si
	mov	$0xE820,%eax
jmporig:
	popf
	ljmp	$0,$0
	.set	guest_int0x15_orig, . - 4
hooke820:
	cmp	$0x534D4150,%edx
	jne	errret
	cmp	$0x14,%ecx
	jb	errret
	push	%si
	mov	$0,%si
	.set	guest_int0x15_e820_data_minus0x18, . - 2
	cld
1:
	add	$0x18,%si
	cmp	$0x1111,%si
	.set	guest_int0x15_e820_end, . - 2
	je	jmporig_e820
	lods	%cs:(%si),%eax
	cmp	%eax,%ebx
	jne	1b
	mov	$0xA,%ecx
	push	%di
	rep	movsw	%cs:(%si),%es:(%di)
	mov	%cs:(%si),%ebx
	mov	$0x14,%cl
	pop	%di
	mov	%edx,%eax
	pop	%si
okret:
	popf
	clc
	lret	$2
guest_int0x15_hook_end:
