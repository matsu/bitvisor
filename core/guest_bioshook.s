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

BIOSHOOK_OFFSET = 0x0000
PAGESIZE = 4096
SYSMEMMAP_TYPE_AVAILABLE = 0x1
SYSMEMMAP_TYPE_RESERVED = 0x2
SYSMEMMAP_TYPE_ACPI_RECLAIM = 0x3
SYSMEMMAP_TYPE_ACPI_NVS = 0x4

	.globl	guest_bios_hook
	.globl	guest_bios_hook_end
	.globl	guest_int0x15_orig
	.globl	vmmarea_start64
	.globl	vmmarea_fake_len64
	.globl	bios_e801_ax
	.globl	bios_e801_bx
	.globl	guest_clihlt

	.text
	.code16

	.align	PAGESIZE
guest_bios_hook:
bioshook_start:
	pushf
	cmp	$0xE820,%ax
	je	do_hook
	cmp	$0xE801,%ax
	je	do_hook
	cmp	$0xE881,%ax
	je	do_errret
	cmp	$0x88,%ah
	je	do_errret
	popf
jmporig:
	.byte	0xEA		# JMP FAR
guest_int0x15_orig:
	.space	4
do_errret:
	popf
	stc
	mov	$0x86,%ah
	lret	$2
do_hook:
	popf
	push	%bp
	mov	%sp,%bp
	push	%ax
	stc
	push	%bp
	pushf
	push	%cs
	call	jmporig
	pop	%bp
	jc	errret
	pushf
	push	%ax
	push	%ds
	push	%es
	pop	%ds

	OFF_ORIGAX = -2
	OFF_FLAGS = -4
	OFF_AX = -6
	OFF_DS = -8

	cmpw	$0xE820,OFF_ORIGAX(%bp)
	je	hook_e820
	cmpw	$0xE801,OFF_ORIGAX(%bp)
	je	hook_e801
hook_ret:
	pop	%ds
	pop	%ax
	popf
errret:
	mov	%bp,%sp
	pop	%bp
	lret	$2

	DI_OFF_BASE64 = 0x00
	DI_OFF_LEN64  = 0x08
	DI_OFF_TYPE32 = 0x10
hook_e820:
	# AVAILABLE ?
	cmpw	$SYSMEMMAP_TYPE_AVAILABLE,DI_OFF_TYPE32+0(%di)
	jne	hook_ret
	cmpw	$0,DI_OFF_TYPE32+2(%di)
	jne	hook_ret
	# AVAILABLE .
	# BASE <= vmmarea_start64 ?
	mov	DI_OFF_BASE64+6(%di),%ax
	cmp	%cs:(vmmarea_start64-bioshook_start+BIOSHOOK_OFFSET)+6,%ax
	jb	hook_ret
	jne	1f
	mov	DI_OFF_BASE64+4(%di),%ax
	cmp	%cs:(vmmarea_start64-bioshook_start+BIOSHOOK_OFFSET)+4,%ax
	jb	hook_ret
	jne	1f
	mov	DI_OFF_BASE64+2(%di),%ax
	cmp	%cs:(vmmarea_start64-bioshook_start+BIOSHOOK_OFFSET)+2,%ax
	jb	hook_ret
	jne	1f
	mov	DI_OFF_BASE64+0(%di),%ax
	cmp	%cs:(vmmarea_start64-bioshook_start+BIOSHOOK_OFFSET)+0,%ax
	jb	hook_ret
	jne	1f
	# BASE == vmmarea_start64 .
	mov	%cs:(vmmarea_fake_len64-bioshook_start+BIOSHOOK_OFFSET)+6,%ax
	mov	%ax,DI_OFF_LEN64+6(%di)
	mov	%cs:(vmmarea_fake_len64-bioshook_start+BIOSHOOK_OFFSET)+4,%ax
	mov	%ax,DI_OFF_LEN64+4(%di)
	mov	%cs:(vmmarea_fake_len64-bioshook_start+BIOSHOOK_OFFSET)+2,%ax
	mov	%ax,DI_OFF_LEN64+2(%di)
	mov	%cs:(vmmarea_fake_len64-bioshook_start+BIOSHOOK_OFFSET)+0,%ax
	mov	%ax,DI_OFF_LEN64+0(%di)
	jmp	hook_ret
1:
	# BASE > vmmarea_start64 .
	movw	$SYSMEMMAP_TYPE_RESERVED,DI_OFF_TYPE32+0(%di)
	jmp	hook_ret
hook_e801:
	mov	%cs:(bios_e801_bx-bioshook_start+BIOSHOOK_OFFSET),%dx
	mov	%cs:(bios_e801_ax-bioshook_start+BIOSHOOK_OFFSET),%cx
	mov	%dx,%bx
	mov	%cx,OFF_AX(%bp)
	jmp	hook_ret
guest_clihlt:
	cli
	hlt
	jmp	guest_clihlt
vmmarea_start64:
	.space	8
vmmarea_fake_len64:
	.space	8
bios_e801_ax:
	.space	2
bios_e801_bx:
	.space	2
guest_bios_hook_end:
