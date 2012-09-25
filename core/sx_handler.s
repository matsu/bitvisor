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

	SEG_SEL_PCPU32	= (8 * 8)
	SEG_SEL_PCPU64	= (15 * 8)
	EXCEPTION_SX	= 0x1E
	gs_init_count	= 40

	.include "longmode.h"

	.text
	.globl	sx_handler

.if longmode
	# 64bit
	.align	8
sx_handler:
	# If this is an #SX exception, (%esp) is an error code.
	# If this is a hardware interrupt, (%esp) is a program counter (IP).
	# A hardware interrupt must not be occurred when IP is 1.
	cmpq	$1,(%rsp)
	je	sx_init
	push	$EXCEPTION_SX
	jmp	int_handler
sx_init:
	push	%gs
	push	$SEG_SEL_PCPU64
	pop	%gs
	incl	%gs:gs_init_count
	pop	%gs
	add	$8,%rsp
	iretq
.else
	# 32bit
	.align	8
sx_handler:
	# If this is an #SX exception, (%esp) is an error code.
	# If this is a hardware interrupt, (%esp) is a program counter (IP).
	# A hardware interrupt must not be occurred when IP is 1.
	cmpl	$1,(%esp)
	je	sx_init
	push	$EXCEPTION_SX
	jmp	int_handler
sx_init:
	push	%gs
	push	$SEG_SEL_PCPU32
	pop	%gs
	incl	%gs:gs_init_count
	pop	%gs
	add	$4,%esp
	iretl
.endif
