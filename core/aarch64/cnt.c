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

#include "asm.h"
#include "cnt.h"

void
cnt_before_suspend (struct cnt_context *c)
{
	/*
	 * It appears that CNTV_CTL_EL0 used by the guest can lose its states
	 * after going power-down suspending. We found this behavior on Jetson
	 * Orin Nano Developer Kit. To avoid the problem, we save and restore
	 * both CNTV_CVAL_EL0 and CNTV_CTL_EL0 to avoid virtual timer interrupt
	 * loss in the guest. Note that while the reference manual says the
	 * system counter must be implemented in an always-on power domain, we
	 * want to be more defensive here by saving and restoring CNTV_CVAL_EL0
	 * too just in case the system implementation does not strictly follow
	 * the reference manual.
	 *
	 * We use isb() after reading the registers to ensure that we get the
	 * value at the read time immediately. It is to avoid out-of-order
	 * execution.
	 */
	c->cntv_cval_el02 = mrs (CNTV_CVAL_EL02);
	isb ();
	c->cntv_ctl_el02 = mrs (CNTV_CTL_EL02);
	isb ();
}

void
cnt_after_suspend (struct cnt_context *c)
{
	/* Avoid out-of-order execution as well on restore */
	msr (CNTV_CVAL_EL02, c->cntv_cval_el02);
	isb ();
	msr (CNTV_CTL_EL02, c->cntv_ctl_el02);
	isb ();
}
