/*
 * Copyright (c) 2022 Igel Co., Ltd
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

#ifndef _CORE_AARCH64_EXCEPTION_H
#define _CORE_AARCH64_EXCEPTION_H

#include <core/types.h>

enum exception_handle_return {
	EXCEPTION_HANDLE_RETURN_OK,
	EXCEPTION_HANDLE_RETURN_NOT_HANDLED,
};

/*
 * We can access general registers by using regs[idx] upto index 30 (x0-x30).
 * Otherwise, we have to use struct to access other saved registers instead.
 */
union exception_saved_regs {
	struct {
		u64 x0, x1, x2, x3, x4, x5, x6;
		u64 x7, x8, x9, x10, x11, x12;
		u64 x13, x14, x15, x16, x17, x18;
		u64 x19, x20, x21, x22, x23, x24;
		u64 x25, x26, x27, x28, x29, x30;
		u64 xzr; /* padding for general registers load/store pair */
		u64 elr_el2, spsr_el2, far_el2, esr_el2, hcr_el2, sp_el0;
		u64 tpidr_el0;
		u64 padding;
	} reg;
	u64 regs[31];
};

struct exception_pcpu_data {
	union exception_saved_regs *saved_regs;
	bool el2_try_recovery;
	bool el2_error_occur_on_recovery;
};

void exception_init (void);
void exception_secondary_init (void);
u64 exception_sp_on_entry (void);
void exception_set_handler (enum exception_handle_return (*handle_irq) (
				    union exception_saved_regs *r),
			    enum exception_handle_return (*handle_fiq) (
				    union exception_saved_regs *r));
void exception_el2_enable_try_recovery (void);
bool exception_el2_recover_from_error (void);
void exception_el2_disable_try_recovery (void);

#endif
