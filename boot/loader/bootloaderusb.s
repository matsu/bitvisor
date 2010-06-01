/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2009 Igel Co., Ltd
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
/* booting BitVisor from a USB storage and */
/* booting a guest OS from an internal HDD */

	CR0_PE_BIT = 0x1
	.code16
entry:					# 0x7C00
	jmp	start
	.org	entry+4
	.space	4			# 0x7C04 length
	.space	8			# 0x7C08 LBA
	.space	4			# 0x7C10 1st module start offset
	.space	4			# 0x7C14 2nd module start offset
	#.short	0x27			# 0x7C10 GDTR
	#.long	0x7C10			# 0x7C12
	#.short	0			# 0x7C16
	.quad	0x00CF9B000000FFFF	# 0x7C18 0x08 CODE32
	.quad	0x008F93000000FFFF	# 0x7C20 0x10 DATA32 not big
	.quad	0x00009B000000FFFF	# 0x7C28 0x18 CODE16
start:					# 0x7C30
	cli
	push	%cs
	pop	%ds
	push	%cs
	pop	%ss
	mov	$0x7FFC,%sp	# B flag of the stack segment is off
	mov	%sp,%bp

	# Enable A20
	in	$0x92,%al
	or	$0x2,%al
	out	%al,$0x92

	# Read sectors
	lea	0x7C00,%esi
	mov	4(%si),%ecx	# length
	pushl	%ecx
	lea	16(%si),%ebx
	pushl	4(%bx)
	pushl	(%bx)
	mov	%bx,(%bx)
	mov	%ebx,2(%bx)
	mov	$511,%bx
	add	%ebx,%ecx
	mov	$0x100000,%edi	# destination address
	push	%edi
	mov	$'L'|('o'<<8)|('a'<<16)|('d'<<24),%eax
	call	print
	shr	$9,%ecx
	je	error
	mov	%ecx,%eax
	shr	$6,%eax
	mov	%ax,(%bp)
	mov	%ax,2(%bp)
	mov	$'i'|('n'<<8)|('g'<<16)|(' '<<24),%eax
	call	print
readloop:
	sti
	pushl	%ecx
	movw	$0x10,(%si)
	mov	$0x40,%al
	cmpl	%eax,%ecx
	jb	readsmall
	mov	%eax,%ecx
readsmall:
	mov	%cx,2(%si)
	mov	$0x8000,%ax
	movl	%eax,4(%si)
	mov	$0x42,%ah
	int	$0x13
	jnc	copy
error:
	mov	$'E'|('r'<<8)|('r'<<16)|('o'<<24),%eax
	call	print
	mov	$'r'|('!'<<8)|('\r'<<16)|('\n'<<24),%eax
	call	print
	int	$0x16
	int	$0x19
toreal:
	mov	%cx,%ds
	mov	%cx,%es
	mov	%cx,%ss
	jc	error
	pop	%ecx
progressloop:
	decw	2(%si)
	js	readloop
	lea	'.'|('\b'<<8),%eax
	decw	(%bp)
	jne	progressprint
	pushw	2(%bp)
	popw	(%bp)
	cbtw
progressprint:
	call	print
	dec	%ecx
	jne	progressloop
	mov	$'\r'|('\n'<<8),%ax
	call	print
copy:	
	mov	$0x10,%bx
	lgdt	(%bx,%si)		# GDTR
	cli
	mov	%cr0,%eax
	inc	%ax
	mov	%eax,%cr0
	ljmp	$0x8,$to32-entry+0x7C00
to16:
	dec	%ax			# CF is not affected
	mov	%eax,%cr0
	ljmp	$0x0,$toreal-entry+0x7C00
print:
	pushal
	mov	$7,%bx
	mov	$0xE,%ah
	int	$0x10
	popal
	shr	$8,%eax
	jne	print
	ret
hookcode:
	inc	%dl		# 025F
	jge	hookdecjmp	# 0261
	int	$0x9B		# 0263 (pushf; lcall *0x26C)
	dec	%dx		# 0265
	lret	$2		# 0266
hookdecjmp:
	dec	%dl		# 0269
hookjmp:
	ljmp	$0,$0		# 026B
hookend:
	hookorig = hookjmp + 1
	.code32
to32:
	mov	%ebx,%ds
	mov	%ebx,%es
	mov	%ebx,%ss
	cld
	jecxz	start32
	addl	%ecx,8(%esi)
	adcl	$0,12(%esi)
	push	%esi
	mov	4(%esi),%esi
	shl	$7,%ecx			# CF should be zero after this
	rep	movsl
	pop	%esi
error2:
	ljmpw	$0x18,$to16-entry+0x7C00
	# Start
start32:
	xor	%eax,%eax
	addr16 lea hookcode-entry+0x7C00,%esi
	addr16 lea 0x25F,%edi
	lea	hookend-hookcode(%eax),%ecx
	pushl	0x13*4(%eax)
	popl	hookorig-hookcode(%esi)
	mov	%edi,0x13*4(%eax)
	rep	movsb
	pop	%esi			# 0x100000
	# ELF header check
	cmpl	$0x464C457F,(%esi)	# ELF header correct?
	stc				# Set CF
	jne	error2			# No-
	# Make a multiboot info
	lea	4(%bp),%edi
	push	%edi			# Clear multiboot info
	lea	88/4(%eax),%ecx
	rep	stosl
	pop	%ebx
	mov	%dl,15(%ebx)		# Write a drive number
	orb	$2,(%ebx)		# Set a boot device flag
	# Modules
	pop	%eax			# 1st module offset
	test	%eax,%eax
	je	dojmp
	mov	%edi,24(%ebx)		# Write a modules structure address
	orb	$8,(%ebx)		# Set a mods flag
	add	%esi,%eax		# Calculate the address of the module
	stosl				# Store the 1st module start address
	pop	%eax			# 2nd module offset
	test	%eax,%eax
	je	modend
	add	%esi,%eax		# Calculate the address of the module
	stosl				# Store the 1st module end address
	stosl				# unused
	stosl				# unused
	incl	20(%ebx)		# Increment modules count
	stosl				# Store the 2nd module start address
modend:
	pop	%eax			# end of the data
	add	%esi,%eax		# Calculate the address
	stosl				# Store the module end address
	stosl				# unused
	stosl				# unused
	incl	20(%ebx)		# Increment modules count
dojmp:
	mov	0x18(%esi),%si		# Get an entry point address < 64KiB
	jmp	*%esi

	.org	entry+0x1FE
	.byte	0x55
	.byte	0xAA
