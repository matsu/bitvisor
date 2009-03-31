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

GUEST_BOOT_OFFSET = 0x800

	.globl	guest_boot_start
	.globl	guest_boot_end

	.text
	.code16
	.align	16

# INPUT:
#   CL = BIOS DRIVE NUMBER
#   DH = CURSOR POSITION (ROW)
#   DL = CURSOR POSITION (COL)
#   BH = CURSOR POSITION (PAGE NUMBER)
#   SI = BOOT AN ACTIVE PARTITION FLAG
#  EDI = 0
#  BOOTING 32BIT KERNEL:
#  ESI = BOOT PARAMS
#  EDI = 32BIT KERNEL START ADDRESS
guest_boot_start:
	cli
	xor	%ax,%ax
	ljmp	$0,$GUEST_BOOT_OFFSET + (1f - guest_boot_start)
1:
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%fs
	mov	%ax,%gs
	mov	%ax,%ss
	mov	$GUEST_BOOT_OFFSET,%sp
	sti
	test	%edi,%edi
	jne	startkernel
	push	%si
	push	%cx
	mov	$0x02,%ah	# SET CURSOR POSITION
	int	$0x10		# CALL VIDEO BIOS
	pop	%dx
	mov	$0x4B01,%ax	# GET STATUS
	mov	$0x7C00,%si
	int	$0x13		# CALL DISK BIOS
	jc	notcd
	cmpw	$0x13,(%si)	# PACKET SIZE = 0x13 AND NO EMULATION
	jne	notcd
	call	print_by_bios
	.string	"Reading CD-ROM.\n"
	mov	$GUEST_BOOT_OFFSET + (exread1 - guest_boot_start),%si
	mov     $0x42,%ah	# EXTENDED READ
	int     $0x13		# CALL DISK BIOS
	jc	notcd
	mov	$GUEST_BOOT_OFFSET + (exread2 - guest_boot_start),%si
	mov	0x7C47,%ax	# READ BOOT CATALOG
	mov	%ax,8(%si)
	mov	0x7C49,%ax
	mov	%ax,10(%si)
	mov     $0x42,%ah	# EXTENDED READ
	int     $0x13		# CALL DISK BIOS
	jc	notcd
	cmpb	$0x88,0x7C20	# BOOTABLE?
	jne	notcd
	mov	$GUEST_BOOT_OFFSET + (exread3 - guest_boot_start),%si
	mov	0x7C22,%ax	# SEGMENT
	test	%ax,%ax
	jne	1f
	mov	$0x7C0,%ax
1:
	cmp	$0x100,%ax	# CHECK SEGMENT
	jb	cderr
	push	%ax
	mov	%ax,6(%si)
	mov	0x7C26,%ax	# SECTOR COUNT
	mov	%ax,2(%si)
	mov	0x7C28,%ax	# LOAD RBA
	mov	%ax,8(%si)
	mov	0x7C2A,%ax
	mov	%ax,10(%si)
	mov     $0x42,%ah	# EXTENDED READ
	int     $0x13		# CALL DISK BIOS
	jc	cderr
	call	print_by_bios
	.string	"Booting from CD.\n"
	xor	%ax,%ax
	push	%ax
	lret
cderr:
	call	print_by_bios
	.string	"CD boot failed.\n"
	jmp	error
exread1:
	.short	0x10		# PACKET SIZE
	.short	1		# NUMBER OF BLOCKS
	.long	0x7C00		# BUFFER ADDRESS
	.quad	0x11		# LBA
exread2:
	.short	0x10		# PACKET SIZE
	.short	1		# NUMBER OF BLOCKS
	.long	0x7C00		# BUFFER ADDRESS
	.quad	0		# LBA
exread3:
	.short	0x10		# PACKET SIZE
	.short	0		# NUMBER OF BLOCKS
	.long	0		# BUFFER ADDRESS
	.quad	0		# LBA
notcd:
	call	print_by_bios
	.string	"Loading MBR.\n"
	mov	$0x00,%ah	# DRIVE RESET
	int	$0x13		# CALL DISK BIOS
	jnc	1f		# JUMP IF NO ERRORS
	call	print_by_bios
	.string	"Drive reset failed.\n"
	jmp	error
1:
	mov	$0x0201,%ax	# READ, 1 SECTOR
	mov	$0x0001,%cx	# CYLINDER 0, SECTOR 1
	mov	$0x00,%dh	# HEAD 0
	mov	$0x7C00,%bx	# BUFFER AT 0x7C00
	int	$0x13		# CALL DISK BIOS
	jnc	1f		# JUMP IF NO ERRORS
4:
	call	print_by_bios
	.string	"Read failed.\n"
	jmp	error
1:
	pop	%si
	test	%si,%si
	je	1f
	mov	$-0x40,%di
3:
	testb	$0x80,0x7DFE(%di)
	je	2f
	mov	$GUEST_BOOT_OFFSET + (exread1 - guest_boot_start),%si
	mov	0x7DFE+8(%di),%eax
	mov	%eax,8(%si)
	mov     $0x42,%ah	# EXTENDED READ
	int     $0x13		# CALL DISK BIOS
	jc	4b
1:
	jmp	*%bx		# JUMP TO MASTER BOOT RECORD
2:
	add	$0x10,%di
	jne	3b
	call	print_by_bios
	.string	"No active partition."
	jmp	error
error:
	xor	%ax,%ax		# GET KEYSTROKE
	int	$0x16		# CALL KEYBOARD BIOS
	int	$0x19		# BOOTSTRAP LOADER
	int	$0x18		# ROM BASIC
	cli
	hlt
2:
	push	%ax
	call	print_by_bios
	.string	"\r"
	pop	%ax
1:
	push	%dx
	mov	$7,%bx		# COLOR (GRAPHICS MODE ONLY) AND PAGE NUMBER
	mov	$0x0E,%ah	# TELETYPE OUTPUT
	int	$0x10		# CALL VIDEO BIOS
	pop	%dx
print_by_bios:
	pop	%si
	cld
	lodsb
	push	%si
	cmp	$'\n',%al
	je	2b
	test	%al,%al
	jne	1b
	ret
startkernel:
	push	%esi
	push	%edi
	call	print_by_bios
	.string	"Starting a kernel.\n"
	pop	%edi
	pop	%esi
	lgdtl	GUEST_BOOT_OFFSET + (startkernel_gdtr - guest_boot_start)
	mov	%cr0,%eax
	or	$1,%eax
	mov	%eax,%cr0
	ljmp	$8,$GUEST_BOOT_OFFSET + (1f - guest_boot_start)
	.code32
1:
	mov	$0x10,%eax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%fs
	mov	%ax,%gs
	mov	%ax,%ss
	mov	$GUEST_BOOT_OFFSET,%esp
	jmp	*%edi
	.align	16
startkernel_gdtr:
	.short	0x17
	.long	GUEST_BOOT_OFFSET + (startkernel_gdt - guest_boot_start)
	.align	16
startkernel_gdt:
	.quad	0
	.quad   0x00CF9B000000FFFF      # 0x08  CODE32, DPL=0
	.quad   0x00CF93000000FFFF      # 0x10  DATA32, DPL=0
guest_boot_end:
