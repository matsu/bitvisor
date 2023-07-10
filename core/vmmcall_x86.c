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

#include <arch/vmmcall.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include "current.h"

void
vmmcall_arch_read_arg (uint order, ulong *arg)
{
	enum general_reg reg;

	switch (order) {
	case 0:
		reg = GENERAL_REG_RAX;
		break;
	case 1:
		reg = GENERAL_REG_RBX;
		break;
	case 2:
		reg = GENERAL_REG_RCX;
		break;
	default:
		panic ("%s(): unsupported order %u", __func__, order);
	}

	current->vmctl.read_general_reg (reg, arg);
}

void
vmmcall_arch_write_arg (uint order, ulong val)
{
	enum general_reg reg;

	switch (order) {
	case 0:
		reg = GENERAL_REG_RAX;
		break;
	case 1:
		reg = GENERAL_REG_RBX;
		break;
	case 2:
		reg = GENERAL_REG_RCX;
		break;
	default:
		panic ("%s(): unsupported order %u", __func__, order);
	}

	current->vmctl.write_general_reg (reg, val);
}

void
vmmcall_arch_write_ret (ulong ret)
{
	current->vmctl.write_general_reg (GENERAL_REG_RAX, ret);
}

bool
vmmcall_arch_caller_user (void)
{
	u16 cs;
	current->vmctl.read_sreg_sel (SREG_CS, &cs);
	return !!(cs & 3);
}
