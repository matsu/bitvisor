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
	CPUID_0x80000001_EDX_64_BIT = 0x20000000
	CR0_PE_BIT = 0x1
	CR0_TS_BIT = 0x8
	CR0_PG_BIT = 0x80000000
	CR4_PSE_BIT = 0x10
	CR4_PAE_BIT = 0x20
	EFLAGS_ID_BIT = 0x200000
	MSR_IA32_EFER = 0xC0000080
	MSR_IA32_EFER_LME_BIT = 0x100
	GUEST_APINIT_SEGMENT = 0x0000
	GUEST_APINIT_OFFSET = 0xF000

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
	or	$CR4_PAE_BIT,%ecx
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

	.globl	cpuinit_start
cpuinit_start:
	.code16
	jmp	1f
	.space	30
cpuinit_tmpstack:
1:
	cli
	ljmp $GUEST_APINIT_SEGMENT,$1f-cpuinit_start+GUEST_APINIT_OFFSET
1:
	mov	%cs,%ax			# %ds <- %cs
	mov	%ax,%ds			#
	# Increment the number of processors.
	lock incw apinit_procs-cpuinit_start+GUEST_APINIT_OFFSET
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
.if longmode
	mov	$0x80000000,%eax
	cpuid
	cmp	$0x80000000,%eax
	jbe	1f
	mov	$0x80000001,%eax
	cpuid
	test	$CPUID_0x80000001_EDX_64_BIT,%edx
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
	.short  0x27
	.long   entry_gdt
	.short	0
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
	mov	entry_ebx,%edi
	movl	$0,entry_ebx
	test	%edi,%edi			# AP?
	je	1f				# Yes-
	# BSP
	push	%edi
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
	mov	$entry_ebx,%eax
	mov	(%rax),%edi
	movl	$0,(%rax)
	test	%edi,%edi			# AP?
	je	1f				# Yes-
	# BSP
	call	vmm_main
	cli
	hlt
1:	
	# AP
	call	apinitproc0
	cli
	hlt

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
	.quad	0x000083		#        +0x00000000
	.quad	0x200083		#        +0x00200000
	.quad	0x400083		#        +0x00400000
	.quad	0x600083		#        +0x00600000
	.quad	0x800083		#        +0x00800000
	.quad	0xA00083		#        +0x00A00000
	.quad	0xC00083		#        +0x00C00000
	.quad	0xE00083		#        +0x00E00000
	.space	4096-8*8		#        +0x01000000-
	.globl	entry_pd
entry_pd: # Page directory for PAE OFF	#   7654321|76543210
	.long	0x000083		#        +0x00000000
	.long	0x400083		#        +0x00400000
	.long	0x800083		#        +0x00800000
	.long	0xC00083		#        +0x00C00000
	.space	1024-4*4		#        +0x01000000-
	.long	0x000083		#        +0x40000000
	.long	0x400083		#        +0x40400000
	.long	0x800083		#        +0x40800000
	.long	0xC00083		#        +0x40C00000
	.space	1024-4*4		#        +0x41000000-
	.long	0x000083		#        +0x80000000
	.long	0x400083		#        +0x80400000
	.long	0x800083		#        +0x80800000
	.long	0xC00083		#        +0x80C00000
	.space	1024-4*4		#        +0x81000000-
	.space	1024			#        +0xC0000000-

	# Stack for initialization
	.align	PAGESIZE
	.space	PAGESIZE
start_stack:

	######## ENTRY SECTION END

	.data
	.align	PAGESIZE
	.globl	vmm_pml4
vmm_pml4:
	.space	PAGESIZE
	.globl	vmm_pdp
vmm_pdp:
	.space	PAGESIZE
	.globl	vmm_pd
vmm_pd:
	.space	PAGESIZE
	.globl	vmm_pd1
vmm_pd1:
	.space	PAGESIZE

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
