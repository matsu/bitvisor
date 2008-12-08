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

	CALLREALMODE_OFFSET = 0xF000
	CALLREALMODE_FUNC_PRINTMSG = 0x0
	CALLREALMODE_FUNC_GETSYSMEMMAP = 0x1
	CALLREALMODE_FUNC_GETSHIFTFLAGS = 0x2
	CALLREALMODE_FUNC_SETVIDEOMODE = 0x3
	CALLREALMODE_FUNC_REBOOT = 0x4

	SEG_SEL_CODE_REAL = 0x0000
	SEG_SEL_DATA_REAL = 0x0000
	SEG_SEL_CODE16    = (6 * 8)
	SEG_SEL_DATA16    = (7 * 8)

	.globl	callrealmode_start
	.globl	callrealmode_end

	.text
	.code16

callrealmode_start:
	push	%ebp
	mov	%sp,%bp

	OFF_EBP = 0x00
	OFF_EIP = 0x04
	OFF_CS  = 0x08
	OFF_EDI = 0x0C
	OFF_ESI = 0x10
	OFF_EBP = 0x14
	OFF_ESP = 0x18
	OFF_EBX = 0x1C
	OFF_EDX = 0x20
	OFF_ECX = 0x24
	OFF_EAX = 0x28
	OFF_FUNC = 0x2C
	OFF_PRINTMSG_MESSAGE = 0x30
	OFF_GETSYSMEMMAP_EBX = 0x30
	OFF_GETSYSMEMMAP_EBX_RET = 0x34
	OFF_GETSYSMEMMAP_FAIL = 0x38
	OFF_GETSYSMEMMAP_DESC = 0x3C
	OFF_GETSHIFTFLAGS_AL_RET = 0x30
	OFF_SETVIDEOMODE_AL = 0x30

	# Which function?
	mov	OFF_FUNC(%bp),%ax
	cmp	$CALLREALMODE_FUNC_PRINTMSG,%ax
	je	printmsg
	cmp	$CALLREALMODE_FUNC_GETSYSMEMMAP,%ax
	je	getsysmemmap
	cmp	$CALLREALMODE_FUNC_GETSHIFTFLAGS,%ax
	je	getshiftflags
	cmp	$CALLREALMODE_FUNC_SETVIDEOMODE,%ax
	je	setvideomode
	cmp	$CALLREALMODE_FUNC_REBOOT,%ax
	je	reboot
	# Error!
	cld
	mov	$(errormsg_data-1-callrealmode_start+CALLREALMODE_OFFSET),%di
	mov	%esp,%eax
	call	hex32
	mov	$',',%al
	stosb
	mov	%cs,%ax
	call	hex16
	mov	$',',%al
	stosb
	mov	%bp,%ax
	call	hex16
	mov	$',',%al
	stosb
	lea	OFF_FUNC(%bp),%si
	call	lodswhex
	call	lodswhex
	lea	OFF_EBP(%bp),%si
	call	lodslhex
	call	lodslhex
	call	lodslhex
	call	lodslhex
	call	lodslhex
	call	lodslhex
	call	lodslhex
	call	lodslhex
	call	lodslhex
	mov	$0,%al
	stosb
	mov	$(errormsg-callrealmode_start+CALLREALMODE_OFFSET),%si
	jmp	printmsg_cont
errormsg:
	.string	"FUNCTION ERROR!\n"
errormsg_data:
	.space	128

callrealmode_ret:
	pop	%ebp
	lretl			# Return to 32bit segment

# printmsg
#
printmsg:
	mov	OFF_PRINTMSG_MESSAGE(%bp),%si
printmsg_cont:
	call	paging_off
	call	protection_off
	call	init_pic
	sti
	# Set mode 80x25 text
	push	%si
	mov	$3,%ax
	int	$0x10
	# Try to set mode 1280x1024 graphics
	mov	$0x4F02,%ax
	mov	$0x0107,%bx
	int	$0x10
1:
	pop	%si
	# Print the message
	call	print_si_by_bios
	# Halt
1:
	cli
	hlt
	jmp	1b

# getsysmemmap
#
getsysmemmap:
	call	paging_off
	call	protection_off
	# Enable interrupts
	sti
	# Call int $0x15
	stc
	mov	$0xE820,%eax
	mov	$0x534D4150,%edx
	mov	OFF_GETSYSMEMMAP_EBX(%bp),%ebx
	mov	$20,%ecx
	lea	OFF_GETSYSMEMMAP_DESC(%bp),%di
	int	$0x15
	mov	$1,%ax
	jc	1f
	xor	%ax,%ax
1:
	mov	%ax,OFF_GETSYSMEMMAP_FAIL(%bp)
	mov	%ebx,OFF_GETSYSMEMMAP_EBX_RET(%bp)
	call	protection_and_paging_on
	jmp	callrealmode_ret

# getshiftflags
#
getshiftflags:
	call	paging_off
	call	protection_off
	# Enable interrupts
	sti
	# Call int $0x16
	mov	$0x02,%ah
	int	$0x16
	mov	%al,OFF_GETSHIFTFLAGS_AL_RET(%bp)
	call	protection_and_paging_on
	jmp	callrealmode_ret

# setvideomode
#
setvideomode:
	mov	OFF_SETVIDEOMODE_AL(%bp),%si
	call	paging_off
	call	protection_off
	call	init_pic
	# Enable interrupts
	sti
	# Set mode 80x25 text
	push	%si
	mov	$3,%ax
	int	$0x10
	# Try to set mode 1280x1024 graphics
	mov	$0x4F02,%ax
	mov	$0x0107,%bx
	int	$0x10
	# Set video mode
	pop	%ax
	mov	$0x00,%ah
	int	$0x10
	call	protection_and_paging_on
	jmp	callrealmode_ret

# reboot
#
reboot:
	call	paging_off
	call	protection_off
	ljmp	$0xFFFF,$0x0

# Subroutines
#
paging_off:
	# PG BIT (PAGING) OFF
	mov	%cr0,%eax
	and	$0x7FFFFFFF,%eax
	mov	%eax,%cr0
	mov	%cr3,%eax
	mov	%eax,%cr3
	ret
protection_off:
	cli
	# PE BIT OFF
	mov	%cr0,%eax
	and	$0xFFFFFFFE,%eax
	mov	%eax,%cr0
	jmp	1f
1:
	ljmp	$SEG_SEL_CODE_REAL,$1f-callrealmode_start+CALLREALMODE_OFFSET
1:
	# Load segment registers
	mov	$SEG_SEL_DATA_REAL,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%fs
	mov	%ax,%gs
	mov	%ax,%ss
	# Clear TS bit
	clts
	ret
protection_and_paging_on:
	cli
	# PE BIT, TS BIT AND PG BIT ON
	mov	%cr0,%eax
	or	$0x80000009,%eax
	mov	%eax,%cr0
	jmp	1f
1:
	ljmp	$SEG_SEL_CODE16,$1f-callrealmode_start+CALLREALMODE_OFFSET
1:
	# Load segment registers
	mov	$SEG_SEL_DATA16,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	xor	%ax,%ax
	mov	%ax,%fs
	mov	%ax,%gs
	ret

init_pic:
	# Initialize PIC master
	mov	$0x11,%al	# ICW1
	out	%al,$0x20
	mov	$8,%al		# ICW2
	out	%al,$0x21
	mov	$4,%al		# ICW3
	out	%al,$0x21
	mov	$1,%al		# ICW4
	out	%al,$0x21
	# Initialize PIC slave
	mov	$0x11,%al	# ICW1
	out	%al,$0xA0
	mov	$0x70,%al	# ICW2
	out	%al,$0xA1
	mov	$2,%al		# ICW3
	out	%al,$0xA1
	mov	$1,%al		# ICW4
	out	%al,$0xA1
	# Wait
	xor	%cx,%cx
2:
	loop	2b
	# Disable interrupts
	mov	$0xFF,%al
	out	%al,$0x21
	mov	$0xFF,%al
	out	%al,$0xA1
	ret

print_by_bios:
	pop	%si
	call	print_si_by_bios
	jmp	*%si
1:
	call	putchar_by_bios
print_si_by_bios:
	cld
	lodsb
	test	%al,%al
	jne	1b
	ret
putchar_by_bios:
	cmp	$'\n',%al
	jne	1f
	mov	$'\r',%al
	call	1f
	mov	$'\n',%al
1:
	push	%si
	mov	$7,%bx
	mov	$0x0E,%ah
	int	$0x10
	pop	%si
	ret
hex32:
	rol	$16,%eax
	call	hex16
	ror	$16,%eax
hex16:
	xchg	%ah,%al
	call	hex8
	xchg	%ah,%al
hex8:
	rol	$4,%al
	call	hex4
	ror	$4,%al
hex4:
	push	%ax
	and	$0xF,%al
	add	$0x90,%al
	daa
	adc	$0x40,%al
	daa
	stosb
	pop	%ax
	ret
lodswhex:
	lodsw
	call	hex16
	mov	$'/',%al
	stosb
	ret
lodslhex:
	lodsl
	call	hex32
	mov	$'/',%al
	stosb
	ret
callrealmode_end:
