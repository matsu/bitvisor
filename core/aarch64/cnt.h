/*
 * Copyright (c) 2024 Igel Co., Ltd
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

#ifndef _CORE_AARCH64_CT_H
#define _CORE_AARCH64_CT_H

#include <core/types.h>
#include "asm.h"
#include "arm_std_regs.h"

/* We don't currently trap counter-timer access */
#define CNTHCTL_DEFAULT_FLAGS \
	(CNTHCTL_EL0PCTEN | CNTHCTL_EL0VCTEN | CNTHCTL_EL0VTEN | \
	 CNTHCTL_EL0PTEN | CNTHCTL_EL1PCTEN | CNTHCTL_EL1PTEN)

struct cnt_context {
	u64 cntv_cval_el02;
	u64 cntv_ctl_el02;
};

static inline void
cnt_set_default_after_e2h_en (void)
{
	msr (CNTHCTL_EL2, CNTHCTL_DEFAULT_FLAGS);
	msr (CNTVOFF_EL2, 0);
	isb ();
}

static inline u64
cnt_get_cntfrq_el0 (void)
{
	return mrs (CNTFRQ_EL0);
}

static inline u64
cnt_get_cntpct_el0 (void)
{
	isb (); /* Avoid out-of-order reading */
	return mrs (CNTPCT_EL0);
}

void cnt_before_suspend (struct cnt_context *c);
void cnt_after_suspend (struct cnt_context *c);

#endif
