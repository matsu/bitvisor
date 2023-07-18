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

#include "constants.h"
#include "cpu_stack.h"
#include "current.h"

enum vmmerr
cpu_stack_get (struct cpu_stack *st)
{
	ulong acr;

	current->vmctl.read_sreg_acr (SREG_SS, &acr);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	current->vmctl.read_general_reg (GENERAL_REG_RSP, &st->sp);
	if (!(acr & ACCESS_RIGHTS_D_B_BIT))
		st->sp &= 0xFFFF;
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_stack_set (struct cpu_stack *st)
{
	ulong rsp;
	ulong acr;

	current->vmctl.read_sreg_acr (SREG_SS, &acr);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	if (!(acr & ACCESS_RIGHTS_D_B_BIT)) {
		current->vmctl.read_general_reg (GENERAL_REG_RSP, &rsp);
		rsp &= ~0xFFFF;
		rsp |= st->sp & 0xFFFF;
	} else {
		rsp = st->sp;
	}
	current->vmctl.write_general_reg (GENERAL_REG_RSP, rsp);
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_stack_push (struct cpu_stack *st, void *data, enum optype type)
{
	ulong acr;

	current->vmctl.read_sreg_acr (SREG_SS, &acr);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	if (type == OPTYPE_16BIT) {
		st->sp -= 2;
		if (!(acr & ACCESS_RIGHTS_D_B_BIT))
			st->sp &= 0xFFFF;
		GUESTSEG_WRITE_W (SREG_SS, st->sp, *(u16 *)data);
	} else {
		st->sp -= 4;
		if (!(acr & ACCESS_RIGHTS_D_B_BIT))
			st->sp &= 0xFFFF;
		GUESTSEG_WRITE_L (SREG_SS, st->sp, *(u32 *)data);
	}
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_stack_pop (struct cpu_stack *st, void *data, enum optype type)
{
	ulong acr;

	current->vmctl.read_sreg_acr (SREG_SS, &acr);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	if (type == OPTYPE_16BIT) {
		GUESTSEG_READ_W (SREG_SS, st->sp, (u16 *)data);
		st->sp += 2;
		if (!(acr & ACCESS_RIGHTS_D_B_BIT))
			st->sp &= 0xFFFF;
	} else {
		GUESTSEG_READ_L (SREG_SS, st->sp, (u32 *)data);
		st->sp += 4;
		if (!(acr & ACCESS_RIGHTS_D_B_BIT))
			st->sp &= 0xFFFF;
	}
	return VMMERR_SUCCESS;
}
