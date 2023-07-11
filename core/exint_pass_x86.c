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
#include <core/initfunc.h>
#include <core/string.h>
#include "current.h"
#include "exint_pass.h"
#include "int.h"

/* Interrupts 0x10-0x1F are reserved by CPU for exceptions but can be
 * delivered by APIC.  Operating systems do not use them for external
 * interrupts because they are same number as CPU exceptions and BIOS
 * calls. */
#define EXINT_ALLOC_START 0x10
#define EXINT_ALLOC_NUM 0x10

struct exint_pass_intr {
	int (*callback) (void *data, int num);
	void *data;
};

static struct exint_pass_intr intr[EXINT_ALLOC_NUM];
static int exint_pass_ack (void);

static struct exint_func func = {
	exint_pass_ack,
};

static int
exint_pass_intr_set (int (*callback) (void *data, int num), void *data, int i)
{
	if (callback && intr[i].callback)
		return 0;
	intr[i].callback = callback;
	intr[i].data = data;
	return 1;
}

int
exint_pass_arch_intr_call (int num)
{
	if (num < EXINT_ALLOC_START)
		return num;
	int i = num - EXINT_ALLOC_START;
	if (i >= EXINT_ALLOC_NUM || !intr[i].callback)
		return exint_pass_intr_run_callback_list (num);
	return intr[i].callback (intr[i].data, num);
}

int
exint_pass_arch_intr_alloc (int (*callback) (void *data, int num), void *data)
{
	int i;
	for (i = 0; i < EXINT_ALLOC_NUM; i++)
		if (exint_pass_intr_set (callback, data, i))
			return EXINT_ALLOC_START + i;
	return -1;
}

void
exint_pass_arch_intr_free (int num)
{
	exint_pass_intr_set (NULL, NULL, num);
}

static int
exint_pass_ack (void)
{
	int num;

	num = do_externalint_enable ();
	num = exint_pass_intr_call (num);
	return num;
}

static void
exint_pass_init (void)
{
	memcpy ((void *)&current->exint, (void *)&func, sizeof func);
}

INITFUNC ("pass0", exint_pass_init);
