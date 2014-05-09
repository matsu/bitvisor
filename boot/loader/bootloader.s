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

/* simple boot loader for 386 and above PC */

	CR0_PE_BIT = 0x1
	.code16
entry:					# 0x7C00
	jmp	start
	.org	entry+4
	.space	4			# 0x7C04 length
	.space	8			# 0x7C08 LBA
	.space	4			# 0x7C10 1st module start offset
	.space	4			# 0x7C14 2nd module start offset
start:					# 0x7C18
	cli
	push	%cs
	pop	%ds
	push	%cs
	pop	%ss
	mov	$0x8000,%sp
	mov	$0x7C00,%si
	pushl	20(%si)			# 2nd module offset
	pushl	16(%si)			# 1st module offset
	pushl	4(%si)			# length
	movl	$0xF000000,4(%si)	# address (segment 0xF00 offset 0x0)
	pushw	6(%si)			# the segment 0xF00
	mov	$2,%cx			# loop count 2
1:
	pusha
	movl	$0x400010,(%si)		# size of packet (0x10) and 0x40 blocks
	mov	$0x42,%ah		# extended read command
	int	$0x13
	jc	error
	popa
	addw	$0x800,6(%si)		# segment += 0x800
	addl	$0x40,8(%si)		# LBA += 0x40
	adcl	$0,12(%si)		#
	loop	1b
	cmpl	$0x464C457F,0xF000	# ELF header correct?
	jne	error
	pushw	0xF018			# Get an entry point address < 64KiB
	lretw
1:
	call	putchar
	.byte	0x38
error:
	mov	$errormsg-entry+0x7C00,%si
	cld
	lodsb
	test	%al,%al
	jne	1b
	xchg	%ah,%al
	rol	$4,%al
	call	hex4
	ror	$4,%al
	call	hex4
	int	$0x16
	int	$0x19
	jmp	.
hex4:
	pusha
	and	$0xF,%al
	add	$0x90,%al
	daa
	adc	$0x40,%al
	daa
	.byte	0x3C
putchar:
	pusha
	mov	$7,%bx
	mov	$0xE,%ah
	int	$0x10
	popa
	ret
errormsg:
	.string	"Load error "

	.org	entry+0x1FE
	.byte	0x55
	.byte	0xAA
