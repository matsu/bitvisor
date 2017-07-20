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

	MULTIBOOT_PAGE_ALIGN = (1 << 0)
	MULTIBOOT_MEMORY_INFO = (1 << 1)

	MULTIBOOT_HEADER_MAGIC = 0x1BADB002
	MULTIBOOT_HEADER_FLAGS = (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO)
	CHECKSUM = -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
	PAGESIZE = 4096
	DIFFPHYS = 0x40000000

	ENTRY_SEL_CODE32 = 0x08
	ENTRY_SEL_DATA32 = 0x10
	ENTRY_SEL_CODE64 = 0x18
	ENTRY_SEL_DATA64 = 0x20
	ENTRY_SEL_CODE16 = 0x28
	ENTRY_SEL_DATA16 = 0x30
	ENTRY_SEL_DATA16_TEXTVRAM = 0x38
	EXCEPTION_UD = 0x6
	EXCEPTION_DF = 0x8
	TEXTVRAM_ADDR = 0xB8000
	TEXTVRAM_ATTR = 0x2
	CPUID_1_EDX_PSE_BIT = 0x8
	CPUID_1_EDX_MSR_BIT = 0x20
	CPUID_1_EDX_PAE_BIT = 0x40
	CPUID_1_EDX_CX8_BIT = 0x100
	CPUID_1_EDX_PGE_BIT = 0x2000
	CPUID_EXT_0 = 0x80000000
	CPUID_EXT_1 = 0x80000001
	CPUID_EXT_1_EDX_64_BIT = 0x20000000
	CR0_PE_BIT = 0x1
	CR0_TS_BIT = 0x8
	CR0_PG_BIT = 0x80000000
	CR4_PSE_BIT = 0x10
	CR4_PAE_BIT = 0x20
	CR4_MCE_BIT = 0x40
	CR4_PGE_BIT = 0x80
	EFLAGS_ID_BIT = 0x200000
	MSR_IA32_EFER = 0xC0000080
	MSR_IA32_EFER_LME_BIT = 0x100
	GUEST_APINIT_OFFSET = 0x0000

	.include "longmode.h"

	######## ENTRY SECTION START
	# This section is placed at the start of the .text section.
	# See the linker script.
	.section .entry

	# GRUB Multiboot Header
	.align	4
	.long	MULTIBOOT_HEADER_MAGIC
	.long	MULTIBOOT_HEADER_FLAGS
	.long	CHECKSUM

	# 32bit code start
	.code32
	.globl	entry
entry:
	# boot/loader jumps to here in real-address mode
	test	$0xF9F9F9F9,%eax	# (test;stc;stc in 16bit mode)
	jc	1f			# must be short jump for 16bit & 32bit
	shl	%al
	inc	%eax			# (rex prefix in 64bit mode)
	rcr	%al			# undo %al
	jnc	uefi64_entry
	jmp	multiboot_entry
1:
	.code16
	# %dl	drive
	# %ds	0
	# %si	0x7C00 (buffer for int $0x13 extended read)
	# %ss	0
	# %cs	0xF00
	# 0(%sp) (uint32_t) length
	# 4(%sp) (uint32_t) 1st module offset
	# 8(%sp) (uint32_t) 2nd module offset
	REALMODE_ENTRY_OFFSET = 0x8000
	push	%ds
	push	%si
	push	%dx
	mov	%sp,%bp
	push	%cs
	pop	%ds
	mov	$realmode_entry,%esi
	sub	$head,%esi
	xor	%ax,%ax
	mov	%ax,%es
	mov	$REALMODE_ENTRY_OFFSET,%di
	mov	$realmode_entry_end-realmode_entry,%cx
	cld
	rep	movsb
	ljmp	$0x0,$REALMODE_ENTRY_OFFSET
realmode_entry:
	# Enable A20
	cli
	in	$0x92,%al
	or	$0x2,%al
	out	%al,$0x92

	# Copy the first 64KiB
	mov	%ds,%ax
	movzwl	%ax,%esi
	shl	$4,%esi
	sub	$8,%sp
	mov	$0x100000,%edi
	mov	$0x10000/2,%cx
	call	1f
	push	%esi
	push	%edi
	movl	$0,-4(%bp)

	mov	$'L'|('o'<<8)|('a'<<16)|('d'<<24),%eax
	call	3f
	mov	6(%bp),%eax		# Length in bytes
	add	$511,%eax
	shr	$9,%eax
	sub	$0x40,%eax
	jbe	2f
	mov	%eax,-8(%bp)		# Length in sectors
	mov	$'i'|('n'<<8)|('g'<<16)|(' '<<24),%eax
	call	3f
5:
	sti
	subl	$0x40,-8(%bp)
	jbe	2f
	lds	2(%bp),%si
	mov	0(%bp),%dl		# Drive number
	movw	$0x10,(%si)		# Size of packet
	mov	-8(%bp),%eax		# Number of remaining sectors
	cmpl	$0x40,%eax
	jb	4f
	mov	$0x40,%ax
4:
	mov	%ax,2(%si)		# Number of sectors to read
	mov	$0x42,%ah		# Extended read command
	int	$0x13
	jc	5f
	movzwl	2(%si),%ecx
	add	%ecx,8(%si)		# LBA += number of sectors
	adcl	$0,12(%si)
	shl	$9+6,%ecx
	sub	%ecx,-4(%bp)
	jnb	6f
4:
	mov	$'.',%eax
	call	3f
	mov	6(%bp),%eax
	add	%eax,-4(%bp)
	jnb	4b
6:
	shr	$9+6-8,%ecx		# Number of words to copy
	pop	%edi
	pop	%esi
	push	%esi
	call	1f
	push	%edi
	jmp	5b
5:
	mov	$'E'|('r'<<8)|('r'<<16)|('o'<<24),%eax
	call	3f
	mov	$'r'|('!'<<8)|('\r'<<16)|('\n'<<24),%eax
	call	3f
	int	$0x16
	int	$0x19
	jmp	5b
1:
	cli
	lgdt	%cs:4f-realmode_entry+REALMODE_ENTRY_OFFSET
	mov	%cr0,%eax
	or	$CR0_PE_BIT,%eax
	mov	%eax,%cr0
	ljmp	$0x08,$1f-realmode_entry+REALMODE_ENTRY_OFFSET
1:
	mov	$0x18,%ax
	mov	%ax,%ds
	mov	%ax,%es
	cld
	rep	movsw %ds:(%esi),%es:(%edi)
	mov	$0x10,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%cr0,%eax
	and	$~CR0_PE_BIT,%eax
	mov	%eax,%cr0
	ljmp	$0x0,$1f-realmode_entry+REALMODE_ENTRY_OFFSET
1:
	push	%cs
	pop	%ds
	push	%cs
	pop	%es
	ret
2:
	mov	$'\r'|('\n'<<8),%eax
	call	3f
	# Make a multiboot info
	push	%cs
	pop	%ds
	push	%cs
	pop	%es
	mov	$realmode_entry_end-realmode_entry+REALMODE_ENTRY_OFFSET,%ebx
	cld
	mov	%bx,%di
	xor	%ax,%ax
	mov	$88/2,%cx
	rep	stosw
	mov	0(%bp),%al		# Drive number
	mov	%al,15(%bx)
	orb	$2,(%bx)		# Set a boot device flag
	# Modules
	mov	10(%bp),%eax		# 1st module offset
	test	%eax,%eax
	je	1f
	mov	%di,24(%bx)		# Write a modules structure address
	orb	$8,(%bx)		# Set a mods flag
	add	$0x100000,%eax		# Calculate the address of the module
	stosl				# Store the 1st module start address
	mov	14(%bp),%eax		# 2nd module offset
	test	%eax,%eax
	je	2f
	add	$0x100000,%eax		# Calculate the address of the module
	stosl				# Store the 1st module end address
	add	$8,%di
	incb	20(%bx)			# Increment modules count
	stosl				# Store the 2nd module start address
2:
	mov	6(%bp),%eax		# End of the data
	add	$0x100000,%eax		# Calculate the address
	stosl				# Store the module end address
	add	$8,%di
	incb	20(%bx)			# Increment modules count
1:
	cli
	lgdt	4f-realmode_entry+REALMODE_ENTRY_OFFSET
	mov	%cr0,%eax
	or	$CR0_PE_BIT,%eax
	mov	%eax,%cr0
	ljmp	$0x08,$1f-realmode_entry+REALMODE_ENTRY_OFFSET
1:
	mov	$0x18,%ax
	mov	%ax,%ds
	addr32 lgdtl	entry_gdtr_phys-DIFFPHYS
	mov	$ENTRY_SEL_DATA32,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	ljmpl	$ENTRY_SEL_CODE32,$multiboot_entry-DIFFPHYS
3:
	pushal
	mov	$7,%bx
	mov	$0xE,%ah
	int	$0x10
	popal
	shr	$8,%eax
	jne	3b
	ret
4:
	.short	0x1F
	.long	4b-2-realmode_entry+REALMODE_ENTRY_OFFSET
	.quad	0x00009B000000FFFF	# 0x08  CODE16, DPL=0
	.quad	0x000093000000FFFF	# 0x10  DATA16, DPL=0
	.quad   0x00CF93000000FFFF      # 0x18  DATA32, DPL=0
realmode_entry_end:

	.code64
uefi64_entry:
.if longmode
	# The entry_pd that will not be used in 64bit mode is used for
	# a page table during this routine
	push	%rbx
	push	%rsi
	push	%rdi
	mov	%rcx,%rdi
	mov	%rdx,%rsi
	mov	%r8,%rdx
	push	%rbp
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rcx
	xor	%ecx,%ecx
	lea	entry_pd(%rip),%ebx
	lea	head-0x100000+0x3(%rip),%eax
1:
	mov	%rax,(%rbx,%rcx,8)
	add	$0x1000,%eax
	add	$1,%ecx
	cmp	$512,%ecx
	jb	1b
	lea	7(%ebx),%eax
	mov	%rax,(%rbx)
	lea	head-0x100000(%rip),%rax
	add	%rax,entry_pml4(%rip)
	add	%rax,entry_pdp+0(%rip)
	add	%rax,entry_pdp+8(%rip)
	addq	$entry_pd-entry_pd0,entry_pdp+8(%rip)
	sub	$head-0x100000,%rax
	mov	%rax,uefi_entry_physoff(%rip)
	pop	%rcx
	mov	%es,%eax
	push	%rax
	mov	%ss,%eax
	push	%rax
	mov	%ds,%eax
	push	%rax
	mov	%fs,%eax
	push	%rax
	mov	%gs,%eax
	push	%rax
	mov	%cr3,%rax
	mov	%rax,uefi_entry_cr3(%rip)
	mov	%rsp,uefi_entry_rsp(%rip)
	lea	uefi_entry_ret(%rip),%rax
	mov	%rax,uefi_entry_ret_addr(%rip)
	lea	entry_pml4(%rip),%rax
	cli
	mov	%rax,%cr3
	mov	$start_stack,%rsp
	mov	$1f,%rax
	jmp	*%rax
1:
	call	uefi_init
	mov	%rax,1b(%rip)
	mov	uefi_entry_physoff(%rip),%rax
	call	uefi_entry_rip_plus_rax
	mov	uefi_entry_rsp(%rip),%rsp
	mov	uefi_entry_cr3(%rip),%rax
	mov	%rax,%cr3
	mov	1b(%rip),%rax
uefi_entry_ret:
	pop	%rbx
	mov	%ebx,%gs
	pop	%rbx
	mov	%ebx,%fs
	pop	%rbx
	mov	%ebx,%ds
	pop	%rbx
	mov	%ebx,%ss
	pop	%rbx
	mov	%ebx,%es
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbp
	pop	%rdi
	pop	%rsi
	pop	%rbx
	ret
.else
	xor	%eax,%eax
	ret
.endif

	.globl	uefi_entry_virttophys
uefi_entry_virttophys:
	mov	uefi_entry_physoff(%rip),%rax
	add	%rdi,%rax
	ret

# uefi_entry_call: number of arguments of UEFI API must be <= 4
	.globl	uefi_entry_call
uefi_entry_call:
	mov	uefi_entry_physoff(%rip),%rax
	call	uefi_entry_rip_plus_rax
	mov	%rsp,%rsi
	mov	uefi_entry_rsp(%rip),%rsp
	and	$~0xF,%rsp	# Force 16-byte alignment; see calluefi
	mov	uefi_entry_cr3(%rip),%rax
	mov	%rax,%cr3
	cld
	sti
	xchg	%rcx,%rdx
	push	%r9
	push	%r8
	push	%rdx
	push	%rcx
	call	*%rdi
	cli
	mov	%rsi,%rsp
	lea	entry_pml4(%rip),%rsi
	mov	%rsi,%cr3
	ret

	.globl	uefi_entry_pcpy
uefi_entry_pcpy:
	mov	uefi_entry_physoff(%rip),%rax
	call	uefi_entry_rip_plus_rax
	mov	uefi_entry_cr3(%rip),%rax
	mov	%rax,%cr3
	lea	entry_pml4(%rip),%rax
	cld
1:
	sub	$8,%rdx
	jb	1f
	movsq
	jne	1b
	mov	%rax,%cr3
	ret
1:
	add	$8,%edx
	.byte	0xA8			# test $imm,%al
1:
	movsb
	sub	$1,%edx
	jnb	1b
	mov	%rax,%cr3
	ret

uefi_entry_rip_plus_rax:
	add	%rax,(%rsp)
	ret

	.globl	uefi_entry_start
uefi_entry_start:
.if longmode
	mov	uefi_entry_physoff(%rip),%rax
	call	uefi_entry_rip_plus_rax
	mov	uefi_entry_cr3(%rip),%rax
	mov	%rax,%cr3
	mov	uefi_entry_physoff(%rip),%rax
	add	$head-0x100000,%rax
	neg	%rax
	add	%rdi,%rax
	add	%rax,entry_pml4-DIFFPHYS(%rdi)
	add	%rax,entry_pdp+0-DIFFPHYS(%rdi)
	add	%rax,entry_pdp+8-DIFFPHYS(%rdi)
	mov	%rdi,%rax
	mov	$0x83,%al
	xor	%ebx,%ebx
1:
	mov	%rax,entry_pd-DIFFPHYS(%rdi,%rbx,8)
	add	$0x200000,%rax
	add	$1,%ebx
	cmp	$512,%ebx
	jb	1b
	lea	entry_pml4-DIFFPHYS(%rdi),%rax
	mov	%rax,%cr3
	mov	%edi,vmm_start_phys
	sgdtq	calluefi_uefi_gdtr
	sidtq	calluefi_uefi_idtr
	sldt	calluefi_uefi_ldtr
	mov	%es,calluefi_uefi_sregs+0
	mov	%cs,calluefi_uefi_sregs+2
	mov	%ss,calluefi_uefi_sregs+4
	mov	%ds,calluefi_uefi_sregs+6
	mov	%fs,calluefi_uefi_sregs+8
	mov	%gs,calluefi_uefi_sregs+10
	mov	uefi_entry_cr3(%rip),%rax
	mov	%rax,calluefi_uefi_cr3
	mov	$bss,%edi		# Clear BSS
	mov	$end+3,%ecx		#
	sub	%edi,%ecx		#
	shr	$2,%ecx			#
	xor	%eax,%eax		#
	cld				#
	rep	stosl			#
	mov	%cr4,%rcx
	or	$(CR4_PAE_BIT|CR4_PGE_BIT),%rcx
	and	$~CR4_MCE_BIT,%rcx
	mov	%cr3,%rax
	mov	%rcx,entry_cr4		# Save CR4
	mov	%rax,vmm_base_cr3	# Save CR3
	mov	%rcx,%cr4
	lgdtq	entry_gdtr		# Load GDTR
	ljmpl	*1f
1:
	.long	callmain64
	.long	ENTRY_SEL_CODE64
.else
	ret
.endif

uefi_entry_cr3:
	.quad	0
	.globl	uefi_entry_rsp
uefi_entry_rsp:
	.quad	0
	.globl	uefi_entry_ret_addr
uefi_entry_ret_addr:
	.quad	0
uefi_entry_physoff:
	.quad	0

	.code32
multiboot_entry:
	# We must use physical addresses while paging is off
	# The VMM is loaded at 0x00100000- (Symbols are placed at 0x40100000-)
	mov	$-DIFFPHYS,%ebp		# Difference of physical & virtual addr
	mov	%ebx,entry_ebx(%ebp)	# Save information from grub
	lea	bss(%ebp),%edi		# Clear BSS
	lea	end+3(%ebp),%ecx	#
	sub	%edi,%ecx		#
	shr	$2,%ecx			#
	xor	%eax,%eax		#
	cld				#
	rep	stosl			#
	mov	%cr4,%ecx
	or	$(CR4_PAE_BIT|CR4_PGE_BIT),%ecx
.if longmode
	lea	entry_pml4(%ebp),%eax	# CR3 for long mode
.else
	lea	entry_pdp(%ebp),%eax	# CR3 for PAE ON
	andw	$0xFE19,8*0(%eax)	# Clear reserved bits
	andw	$0xFE19,8*1(%eax)	#
	andw	$0xFE19,8*2(%eax)	#
	andw	$0xFE19,8*3(%eax)	#
	cmpl	$0,use_pae(%ebp)	# Do we use PAE?
	jne	1f			# Yes-
	lea	entry_pd(%ebp),%eax	# CR3 for PAE OFF
	and	$~CR4_PAE_BIT,%ecx
	or	$CR4_PSE_BIT,%ecx
1:
.endif
	mov	%ecx,entry_cr4(%ebp)	# Save CR4
	mov	%eax,vmm_base_cr3(%ebp)	# Save CR3
	lea	cpuinit_start(%ebp),%eax
	mov	%ax,entry_gdt+0x28+2(%ebp)
	mov	%ax,entry_gdt+0x30+2(%ebp)
	shr	$16,%eax
	mov	%al,entry_gdt+0x28+4(%ebp)
	mov	%al,entry_gdt+0x30+4(%ebp)
	mov	%ah,entry_gdt+0x28+7(%ebp)
	mov	%ah,entry_gdt+0x30+7(%ebp)
	lgdtl	entry_gdtr_phys(%ebp)	# Load GDTR
	mov	$ENTRY_SEL_DATA16,%eax
	mov	%eax,%ds
	mov	%eax,%ss
	mov	$cpuinit_tmpstack-cpuinit_start,%esp
	mov	$ENTRY_SEL_DATA16_TEXTVRAM,%eax
	mov	%eax,%es
	ljmp	$ENTRY_SEL_CODE16,$entry16-cpuinit_start

	# alignment for the data after the code
	# this is necessary for atomic read access of apinit_procs
	.align	16
	.globl	cpuinit_start
cpuinit_start:
	.code16
	jmp	1f
	.space	30
cpuinit_tmpstack:
1:
	cli
	mov	%cs,%ax			# %ds <- %cs
	mov	%ax,%ds			#
	# Increment the number of processors.
	lock incl apinit_procs-cpuinit_start+GUEST_APINIT_OFFSET
	# Setup realmode segments
	mov	%ax,%ss			# %ss <- %cs
	mov	$cpuinit_tmpstack-cpuinit_start+GUEST_APINIT_OFFSET,%sp
	mov	$(TEXTVRAM_ADDR >> 4),%ax # %es <- text vram
	mov	%ax,%es			#
	# Spinlock!
	mov	$1,%al
1:
	pause
	xchg	%al,apinit_lock-cpuinit_start+GUEST_APINIT_OFFSET
	test	%al,%al
	jne	1b
	# Fall through
entry16:
	call	entry16_2
entry16_2:
	pop	%bp
	call	check386
	lea	msg_badcpu16-entry16_2(%bp),%si
	jc	error16
	pushl	$0		# Clear flag register including IOPL
	popfl			#
	call	check_cpuid
	lea	msg_badcpuid-entry16_2(%bp),%si
	jc	error16
	call	check_flags
	lea	msg_badcpu-entry16_2(%bp),%si
	jc	error16
	lgdtl	entry_gdtr-entry16_2(%bp)
	mov	entry_cr4-entry16_2(%bp),%eax
	mov	%eax,%cr4
	mov	vmm_base_cr3-entry16_2(%bp),%eax
	mov	%eax,%cr3
	cli
	je	init32
	mov	$MSR_IA32_EFER,%ecx
	rdmsr
	or	$MSR_IA32_EFER_LME_BIT,%eax
	wrmsr
	mov	%cr0,%eax
	or	$(CR0_PG_BIT|CR0_TS_BIT|CR0_PE_BIT),%eax
	mov	%eax,%cr0
	ljmpl	$ENTRY_SEL_CODE64,$callmain64
init32:
	mov	%cr0,%eax
	or	$(CR0_PG_BIT|CR0_TS_BIT|CR0_PE_BIT),%eax
	mov	%eax,%cr0
	ljmpl	$ENTRY_SEL_CODE32,$callmain32
error16:
	cld
	xor	%di,%di			# %di <- zero
	mov	$TEXTVRAM_ATTR,%ah
1:
	lodsb
	test	%al,%al
	je	1f
	stosw
	jmp	1b
1:
	cli
	hlt
	jmp	1b

	## Processor check (241618J-019)
	# 32bit test
	# OUTPUT
	#  CF   =0: 32bit available
	#  %ax, flags
check386:
	# 8086?
	pushfw
	popw	%ax
	and	$0xFFF,%ax
	pushw	%ax
	popfw
	pushfw
	popw	%ax
	and	$0xF000,%ax
	cmp	$0xF000,%ax
	je	is8086
	# 80286?
	pushfw
	popw	%ax
	or	$0xF000,%ax
	pushw	%ax
	popfw
	test	$0xF000,%ax
	je	is80286
	clc
	ret
is8086:
is80286:
	stc
	ret

	# cpuid test
	# OUTPUT
	#  CF   =0: cpuid available
	#  %eax, %ecx, flags
check_cpuid:
	pushfl
	popl	%eax
	mov	%eax,%ecx
	xor	$EFLAGS_ID_BIT,%eax
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xor	%ecx,%eax
	je	no_cpuid
	clc
	ret
no_cpuid:
	stc
	ret

	# cpuid flags test
	# OUTPUT
	#  CF   =0: ok
	#    ZF =0: 64bit  1: 32bit
	#  %eax, %ebx, %ecx, %edx, flags
check_flags:
	mov	$1,%eax
	cpuid
	test	$CPUID_1_EDX_PSE_BIT,%edx
	je	1f
	test	$CPUID_1_EDX_PAE_BIT,%edx
	je	1f
	test	$CPUID_1_EDX_MSR_BIT,%edx
	je	1f
	test	$CPUID_1_EDX_CX8_BIT,%edx
	je	1f
	test	$CPUID_1_EDX_PGE_BIT,%edx
	je	1f
.if longmode
	mov	$CPUID_EXT_0,%eax
	cpuid
	cmp	$CPUID_EXT_0,%eax
	jbe	1f
	mov	$CPUID_EXT_1,%eax
	cpuid
	test	$CPUID_EXT_1_EDX_64_BIT,%edx
	je	1f
.else
	xor	%eax,%eax
.endif
	clc
	ret
1:
	stc
	ret
msg_badcpu16:
	.string	"16bit processor is not supported."
msg_badcpuid:
	.string	"This processor does not support CPUID. (386 or 486?)"
msg_badcpu:
	.string	"This processor is not supported."
	# Provisional GDT
        .align  4
	.short  0
entry_gdtr:
	.short  0x3F
	.long   entry_gdt
	.short	0
	.long	0
entry_gdtr_phys:
	.short	0x3F
	.long	entry_gdt-DIFFPHYS
	.align  8
entry_gdt:	
	.quad   0
	.quad   0x00CF9B000000FFFF      # 0x08  CODE32, DPL=0
	.quad   0x00CF93000000FFFF      # 0x10  DATA32, DPL=0
	.quad   0x00AF9B000000FFFF      # 0x18  CODE64, DPL=0
	.quad   0x00AF93000000FFFF      # 0x20  DATA64, DPL=0
	.quad	0x00009B000000FFFF	# 0x28  CODE16, DPL=0
	.quad	0x000093000000FFFF	# 0x30  DATA16, DPL=0
	.quad	0x0000930B8000FFFF	# 0x38  DATA16, DPL=0
	.globl	vmm_base_cr3
vmm_base_cr3:
	.quad	0
entry_cr4:
	.quad	0
entry_ebx:
	.quad	0
	.globl	apinit_procs
apinit_procs:
	.quad	0
	.globl	apinit_lock
apinit_lock:
	.quad	0
	.globl	cpuinit_end
cpuinit_end:

	.code32
callmain32:
	mov	$ENTRY_SEL_DATA32,%eax
	xor	%ebx,%ebx
	mov	%eax,%ds
	mov	%eax,%es
	mov	%ebx,%fs
	mov	%ebx,%gs
	mov	%eax,%ss
	mov	$start_stack,%esp
	cmpb	$0,bspinit_done			# BSP?
	jne	1f				# No-
	movb	$1,bspinit_done
	# BSP
	pushl	entry_ebx
	xor	%ebp,%ebp
	call	vmm_main
	cli
	hlt
1:	
	# AP
	call	apinitproc0
	cli
	hlt

	# 64bit code start
	.code64
callmain64:
	mov	$ENTRY_SEL_DATA64,%eax
	xor	%ebx,%ebx
	mov	%eax,%ds
	mov	%eax,%es
	mov	%ebx,%fs
	mov	%ebx,%gs
	mov	%eax,%ss
	mov	$start_stack,%esp
	mov	$bspinit_done,%eax
	cmpb	$0,(%rax)			# BSP?
	jne	1f				# No-
	movb	$1,(%rax)
	# BSP
	mov	$entry_ebx,%eax
	mov	(%rax),%edi
	call	vmm_main
	cli
	hlt
1:	
	# AP
	call	apinitproc0
	cli
	hlt

bspinit_done:
	.byte	0

	# Provisional page tables
	.align	PAGESIZE
entry_pml4:				#   7654321|76543210
	.long	entry_pdp-DIFFPHYS+0x7	# 0x0000000000000000
	.long	0			#
	.space	4096-8*1		# 0x0000008000000000-
	.globl	entry_pdp
entry_pdp:
	.long	entry_pd0-DIFFPHYS+0x7	# 0x0000000000000000
	.long	0			#
	.long	entry_pd0-DIFFPHYS+0x7	# 0x0000000040000000
	.long	0			#
	.long	entry_pd0-DIFFPHYS+0x7	# 0x0000000080000000
	.long	0			#
	.quad	0			# 0x00000000C0000000
	.space	4096-8*4		# 0x0000000100000000-
	.globl	entry_pd0
entry_pd0:
	.rept	512			#        +0x00000000-
	.quad	0x83 + (. - entry_pd0) / 8 * 0x200000
	.endr
	.globl	entry_pd
entry_pd: # Page directory for PAE OFF	#   7654321|76543210
	.rept	3			#        +0x00000000-
1:
	.rept	256
	.long	0x83 + (. - 1b) / 4 * 0x400000
	.endr
	.endr
	.space	1024			#        +0xC0000000-

	# Stack for initialization
	.align	PAGESIZE
	.space	PAGESIZE
start_stack:

	######## ENTRY SECTION END

	.text
	.code32
	.globl	move_vmm_area32
move_vmm_area32:
	push	%esi
	push	%edi
	cld
	mov	$end,%ecx			# size of the VMM
	sub	$head,%ecx			# (end-head)
	mov	$0x40100000,%esi		# source: 0x40100000-
	mov	$0xC0100000,%edi		# destination: 0xC0100000-
	rep	movsb				# copy the VMM code and data
	mov	vmm_base_cr3,%eax		# load new cr3
	mov	%eax,%cr3			#
	pop	%edi
	pop	%esi
	ret

	.code64
	.globl	move_vmm_area64
move_vmm_area64:
	push	%rsi
	push	%rdi
	cld
	mov	$end,%ecx			# size of the VMM
	sub	$head,%ecx			# (end-head)
	mov	$0x40100000,%esi		# source: 0x40100000-
	mov	$0xC0100000,%edi		# destination: 0xC0100000-
	rep	movsb				# copy the VMM code and data
	mov	$vmm_base_cr3,%eax		# load new cr3
	mov	(%rax),%rax			#
	mov	%rax,%cr3			#
	pop	%rdi
	pop	%rsi
	ret
