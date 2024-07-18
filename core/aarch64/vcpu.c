/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <core/list.h>
#include <core/mm.h>
#include <core/spinlock.h>
#include "vcpu.h"

/*
 * If we are to support multiple VM, this is the place we need to add per VCPU
 * data. The data are those general registers, floating point registers,
 * system registers, etc.
 */
struct vcpu {
	LIST1_DEFINE (struct vcpu);
	struct vm_ctx *vm;
	u64 mpidr;
};

struct vcpu_list {
	LIST1_DEFINE_HEAD (struct vcpu, list);
	spinlock_t lock;
};

struct vcpu *
vcpu_alloc (struct vm_ctx *vm_ctx, u64 mpidr)
{
	struct vcpu *new_vcpu;

	new_vcpu = alloc (sizeof *new_vcpu);
	new_vcpu->vm = vm_ctx;
	new_vcpu->mpidr = mpidr;

	return new_vcpu;
}

struct vcpu_list *
vcpu_list_alloc (void)
{
	struct vcpu_list *new_list;

	new_list = alloc (sizeof *new_list);
	LIST1_HEAD_INIT (new_list->list);
	spinlock_init (&new_list->lock);

	return new_list;
}

void
vcpu_add_to_list (struct vcpu_list *vcpu_list, struct vcpu *vcpu)
{
	spinlock_lock (&vcpu_list->lock);
	LIST1_ADD (vcpu_list->list, vcpu);
	spinlock_unlock (&vcpu_list->lock);
}

struct vm_ctx *
vcpu_get_vm_ctx (struct vcpu *vcpu)
{
	return vcpu->vm;
}
