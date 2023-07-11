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

#include <arch/exint_pass.h>
#include <core/mm.h>
#include <core/panic.h>
#include "exint_pass.h"
#include "list.h"

struct exint_pass_intr {
	int (*callback) (void *data, int num);
	void *data;
};

struct exint_pass_intr_list {
	LIST1_DEFINE (struct exint_pass_intr_list);
	struct exint_pass_intr intr;
};

static LIST1_DEFINE_HEAD_INIT (struct exint_pass_intr_list, intr_list);

int
exint_pass_intr_run_callback_list (int num)
{
	struct exint_pass_intr_list *intr;

	LIST1_FOREACH (intr_list, intr)
		num = intr->intr.callback (intr->intr.data, num);
	return num;
}

int
exint_pass_intr_call (int num)
{
	return exint_pass_arch_intr_call (num);
}

int
exint_pass_intr_alloc (int (*callback) (void *data, int num), void *data)
{
	return exint_pass_arch_intr_alloc (callback, data);
}

void
exint_pass_intr_free (int num)
{
	exint_pass_arch_intr_free (num);
}

int
exint_pass_intr_register_callback (int (*callback) (void *data, int num),
				   void *data)
{
	struct exint_pass_intr_list *intr = alloc (sizeof *intr);

	if (!intr)
		return 0;
	intr->intr.callback = callback;
	intr->intr.data     = data;
	LIST1_ADD (intr_list, intr);
	return 1;
}
