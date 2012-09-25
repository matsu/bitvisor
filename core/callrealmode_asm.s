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

	CALLREALMODE_OFFSET = 0x5000
	CALLREALMODE_FUNC_PRINTMSG = 0x0
	CALLREALMODE_FUNC_GETSYSMEMMAP = 0x1
	CALLREALMODE_FUNC_GETSHIFTFLAGS = 0x2
	CALLREALMODE_FUNC_SETVIDEOMODE = 0x3
	CALLREALMODE_FUNC_REBOOT = 0x4
	CALLREALMODE_FUNC_DISK_READMBR = 0x5
	CALLREALMODE_FUNC_DISK_READLBA = 0x6
	CALLREALMODE_FUNC_BOOTCD_GETSTATUS = 0x7
	CALLREALMODE_FUNC_SETCURSORPOS = 0x8
	CALLREALMODE_FUNC_STARTKERNEL32 = 0x9
	CALLREALMODE_FUNC_TCGBIOS = 0xA
	CALLREALMODE_FUNC_GETFONTINFO = 0xB

	SEG_SEL_CODE_REAL = 0x0000
	SEG_SEL_DATA_REAL = 0x0000
	SEG_SEL_DATA32    = (2 * 8)
	SEG_SEL_CODE16    = (6 * 8)
	SEG_SEL_DATA16    = (7 * 8)

	CPUID_0x80000001_EDX_64_BIT = 0x20000000
	MSR_IA32_EFER = 0xC0000080
	MSR_IA32_EFER_LMA_BIT = 0x400

	.globl	callrealmode_start
	.globl	callrealmode_start2
	.globl	callrealmode_end

	.text
	.code16

callrealmode_start:
	push	%ebp
	mov	%sp,%bp

	OFF_SAVED_CR0 = -0x04
	OFF_SAVED_CR3 = -0x08
	OFF_SAVED_CR4 = -0x0C
	OFF_SAVED_EFER = -0x14
	OFF_SAVED_TR = -0x16
	OFF_SAVED_GDTR = -0x1C
	sub	$0x1C,%sp

	call	paging_and_protection_off
	call	callrealmode_switch
	call	protection_and_paging_on

	mov	%bp,%sp
	pop	%ebp
	lretl			# Return to 32bit segment

callrealmode_start2:
	sub	$0x2C,%sp
	mov	%sp,%bp
	push	%eax
	call	callrealmode_switch
	cli
	mov	%cr0,%eax
	lgdtw	%cs:(start2_gdtr-callrealmode_start+CALLREALMODE_OFFSET)
	or	$1,%eax
	mov	%eax,%cr0
	ljmp	$0x8,$(1f-callrealmode_start+CALLREALMODE_OFFSET)
1:
	.code32
	mov	$0x10,%ax
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%fs
	mov	%eax,%gs
	mov	%eax,%ss
	mov	$(3f-callrealmode_start+CALLREALMODE_OFFSET),%ebx
	pop	%eax
	test	%eax,%eax
	je	1f
	xor	%eax,%eax
2:
	vmcall
	xor	%ebx,%ebx
	jmp	2b
1:
	vmmcall
	xor	%ebx,%ebx
	jmp	1b
3:
	.code16
	.string	"boot"
start2_gdtr:
	.short	0x17
	.long	start2_gdt-callrealmode_start+CALLREALMODE_OFFSET
start2_gdt:
	.quad	0
	.quad   0x00CF9B000000FFFF      # 0x08  CODE32, DPL=0
	.quad   0x00CF93000000FFFF      # 0x10  DATA32, DPL=0

callrealmode_switch:
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
	OFF_DISK_READMBR_BUFFER_ADDR = 0x30
	OFF_DISK_READMBR_DRIVE = 0x34
	OFF_DISK_READMBR_STATUS = 0x35
	OFF_DISK_READLBA_BUFFER_ADDR = 0x30
	OFF_DISK_READLBA_LBA = 0x34
	OFF_DISK_READLBA_NUM_OF_BLOCKS = 0x3C
	OFF_DISK_READLBA_DRIVE = 0x3E
	OFF_DISK_READLBA_STATUS = 0x3F
	OFF_BOOTCD_GETSTATUS_DRIVE = 0x30
	OFF_BOOTCD_GETSTATUS_ERROR = 0x31
	OFF_BOOTCD_GETSTATUS_RETCODE = 0x32
	OFF_BOOTCD_GETSTATUS_DATA = 0x34
	OFF_SETCURSORPOS_PAGE_NUM = 0x30
	OFF_SETCURSORPOS_ROW = 0x31
	OFF_SETCURSORPOS_COLUMN = 0x32
	OFF_STARTKERNEL32_PARAMSADDR = 0x30
	OFF_STARTKERNEL32_STARTADDR = 0x34
	OFF_TCGBIOS_OUT_EAX = 0x30
	OFF_TCGBIOS_OUT_EBX = 0x34
	OFF_TCGBIOS_OUT_ECX = 0x38
	OFF_TCGBIOS_OUT_EDX = 0x3C
	OFF_TCGBIOS_OUT_ESI = 0x40
	OFF_TCGBIOS_OUT_EDI = 0x44
	OFF_TCGBIOS_OUT_DS = 0x48
	OFF_TCGBIOS_IN_EBX = 0x4C
	OFF_TCGBIOS_IN_ECX = 0x50
	OFF_TCGBIOS_IN_EDX = 0x54
	OFF_TCGBIOS_IN_ESI = 0x58
	OFF_TCGBIOS_IN_EDI = 0x5C
	OFF_TCGBIOS_IN_ES = 0x60
	OFF_TCGBIOS_IN_DS = 0x64
	OFF_TCGBIOS_AL = 0x68
	OFF_GETFONTINFO_BP_RET = 0x30
	OFF_GETFONTINFO_ES_RET = 0x32
	OFF_GETFONTINFO_CX_RET = 0x34
	OFF_GETFONTINFO_DL_RET = 0x36
	OFF_GETFONTINFO_BH = 0x37

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
	cmp	$CALLREALMODE_FUNC_DISK_READMBR,%ax
	je	disk_readmbr
	cmp	$CALLREALMODE_FUNC_DISK_READLBA,%ax
	je	disk_readlba
	cmp	$CALLREALMODE_FUNC_BOOTCD_GETSTATUS,%ax
	je	bootcd_getstatus
	cmp	$CALLREALMODE_FUNC_SETCURSORPOS,%ax
	je	setcursorpos
	cmp	$CALLREALMODE_FUNC_STARTKERNEL32,%ax
	je	startkernel32
	cmp	$CALLREALMODE_FUNC_TCGBIOS,%ax
	je	tcgbios
	cmp	$CALLREALMODE_FUNC_GETFONTINFO,%ax
	je	getfontinfo
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

# printmsg
#
printmsg:
	mov	OFF_PRINTMSG_MESSAGE(%bp),%si
printmsg_cont:
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
	ret

# getshiftflags
#
getshiftflags:
	# Enable interrupts
	sti
	# Call int $0x16
	mov	$0x02,%ah
	int	$0x16
	mov	%al,OFF_GETSHIFTFLAGS_AL_RET(%bp)
	ret

# setvideomode
#
setvideomode:
	mov	OFF_SETVIDEOMODE_AL(%bp),%si
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
	ret

# reboot
#
reboot:
	ljmp	$0xFFFF,$0x0

# disk_readmbr
#
disk_readmbr:
	sti
	mov	OFF_DISK_READMBR_DRIVE(%bp),%dl
	mov	$0x00,%ah	# DRIVE RESET
	int	$0x13		# CALL DISK BIOS
	jc	1f
	mov	$0x0201,%ax	# READ, 1 SECTOR
	mov	$0x0001,%cx	# CYLINDER 0, SECTOR 1
	mov	$0x00,%dh	# HEAD 0
	les	OFF_DISK_READMBR_BUFFER_ADDR(%bp),%bx
	int	$0x13		# CALL DISK BIOS
	jc	1f
	mov	$0,%ah
1:
	mov	%ah,OFF_DISK_READMBR_STATUS(%bp)
	ret

# disk_readlba
#
disk_readlba:
	sti
	pushl	OFF_DISK_READLBA_LBA+4(%bp)
	pushl	OFF_DISK_READLBA_LBA+0(%bp)
	pushl	OFF_DISK_READLBA_BUFFER_ADDR(%bp)
	pushw	OFF_DISK_READLBA_NUM_OF_BLOCKS(%bp)
	pushw	$0x10
	mov	%sp,%si		# %ss and %ds are the same
	mov	OFF_DISK_READLBA_DRIVE(%bp),%dl
	mov     $0x42,%ah	# EXTENDED READ
	int	$0x13		# CALL DISK BIOS
	mov	%ah,OFF_DISK_READLBA_STATUS(%bp)
	add	$0x10,%sp
	ret

# bootcd_getstatus
#
bootcd_getstatus:
	sti
	# %ss and %ds are the same
	lea	OFF_BOOTCD_GETSTATUS_DATA(%bp),%si
	mov	OFF_BOOTCD_GETSTATUS_DRIVE(%bp),%dl
	mov	$0x4B01,%ax	# GET STATUS
	int	$0x13		# CALL DISK BIOS
	mov	%ax,OFF_BOOTCD_GETSTATUS_RETCODE(%bp)
	sbb	%ax,%ax
	mov	%al,OFF_BOOTCD_GETSTATUS_ERROR(%bp)
	ret

# setcursorpos
#
setcursorpos:
	mov	OFF_SETCURSORPOS_PAGE_NUM(%bp),%bh
	mov	OFF_SETCURSORPOS_ROW(%bp),%dh
	mov	OFF_SETCURSORPOS_COLUMN(%bp),%dl
	mov	$0x02,%ah
	int	$0x10
	ret

# startkernel32
#
startkernel32:
	mov	OFF_STARTKERNEL32_PARAMSADDR(%bp),%esi
	mov	OFF_STARTKERNEL32_STARTADDR(%bp),%edi
	cli
	mov	%cr0,%eax
	lgdtw	%cs:(start2_gdtr-callrealmode_start+CALLREALMODE_OFFSET)
	or	$1,%eax
	mov	%eax,%cr0
	ljmp	$0x8,$(1f-callrealmode_start+CALLREALMODE_OFFSET)
1:
	.code32
	mov	$0x10,%ax
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%fs
	mov	%eax,%gs
	mov	%eax,%ss
	jmp	*%edi
	.code16

# tcgbios
#
tcgbios:
	# Enable interrupts
	sti
	# Call int $0x1A
	mov	OFF_TCGBIOS_IN_EBX(%bp),%ebx
	mov	OFF_TCGBIOS_IN_ECX(%bp),%ecx
	mov	OFF_TCGBIOS_IN_EDX(%bp),%edx
	mov	OFF_TCGBIOS_IN_ESI(%bp),%esi
	mov	OFF_TCGBIOS_IN_EDI(%bp),%edi
	mov	OFF_TCGBIOS_IN_ES(%bp),%es
	mov	OFF_TCGBIOS_IN_DS(%bp),%ds
	mov	$0xBB00,%eax
	mov	OFF_TCGBIOS_AL(%bp),%al
	int	$0x1A
	mov	%eax,OFF_TCGBIOS_OUT_EAX(%bp)
	mov	%ebx,OFF_TCGBIOS_OUT_EBX(%bp)
	mov	%ecx,OFF_TCGBIOS_OUT_ECX(%bp)
	mov	%edx,OFF_TCGBIOS_OUT_EDX(%bp)
	mov	%esi,OFF_TCGBIOS_OUT_ESI(%bp)
	mov	%edi,OFF_TCGBIOS_OUT_EDI(%bp)
	mov	%ds,OFF_TCGBIOS_OUT_DS(%bp)
	ret

# getfontinfo
#
getfontinfo:
	mov	OFF_GETFONTINFO_BH(%bp),%bh
	mov	$0x1130,%ax
	push	%bp
	int	$0x10
	mov	%bp,%bx
	pop	%bp
	mov	%bx,OFF_GETFONTINFO_BP_RET(%bp)
	mov	%es,OFF_GETFONTINFO_ES_RET(%bp)
	mov	%cx,OFF_GETFONTINFO_CX_RET(%bp)
	mov	%dl,OFF_GETFONTINFO_DL_RET(%bp)
	ret

# Subroutines
#
paging_and_protection_off:
	cli
	mov	%cr0,%eax
	mov	%eax,OFF_SAVED_CR0(%bp)
	mov	%cr3,%eax
	mov	%eax,OFF_SAVED_CR3(%bp)
	mov	%cr4,%eax
	mov	%eax,OFF_SAVED_CR4(%bp)
	mov	$0x80000000,%eax
	cpuid
	cmp	$0x80000000,%eax
	jbe	1f
	mov	$0x80000001,%eax
	cpuid
	test	$CPUID_0x80000001_EDX_64_BIT,%edx
	je	1f
	mov	$MSR_IA32_EFER,%ecx
	rdmsr
	and	$~MSR_IA32_EFER_LMA_BIT,%eax
	mov	%eax,OFF_SAVED_EFER+0(%bp)
	mov	%edx,OFF_SAVED_EFER+4(%bp)
1:
	strw	OFF_SAVED_TR(%bp)
	sgdtl	OFF_SAVED_GDTR(%bp)
	# PG BIT (PAGING) AND PE BIT OFF
	mov	%cr0,%eax
	and	$0x7FFFFFFE,%eax
	mov	%eax,%cr0
	mov	%cr3,%eax
	mov	%eax,%cr3
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
	lgdtl	OFF_SAVED_GDTR(%bp)
	mov	$0x80000000,%eax
	cpuid
	cmp	$0x80000000,%eax
	jbe	1f
	mov	$0x80000001,%eax
	cpuid
	test	$CPUID_0x80000001_EDX_64_BIT,%edx
	je	1f
	mov	$MSR_IA32_EFER,%ecx
	# Do not change EFER.LMA, or
	# an exception (#GP) occurs on AMD processors.
	rdmsr
	and	$MSR_IA32_EFER_LMA_BIT,%eax
	or	OFF_SAVED_EFER+0(%bp),%eax
	mov	OFF_SAVED_EFER+4(%bp),%edx
	wrmsr
1:
	mov	OFF_SAVED_CR4(%bp),%eax
	mov	%eax,%cr4
	mov	OFF_SAVED_CR3(%bp),%eax
	mov	%eax,%cr3
	# PE BIT AND PG BIT ON
	mov	OFF_SAVED_CR0(%bp),%eax
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
	push	%ds
	mov	$SEG_SEL_DATA32,%ax
	mov	%ax,%ds
	movzwl	OFF_SAVED_TR(%bp),%eax
	mov	OFF_SAVED_GDTR+2(%bp),%edx
	andb	$~2,5(%edx,%eax)
	pop	%ds
	ltrw	%ax
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
