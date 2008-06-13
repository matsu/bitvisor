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
#include "cpu_mmu.h"
#include "cpu_seg.h"
#include "current.h"

enum vmmerr
cpu_seg_read_b (enum sreg s, ulong offset, u8 *data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (read_linearaddr_b (linear, data));
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_seg_read_w (enum sreg s, ulong offset, u16 *data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (read_linearaddr_w (linear, data));
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_seg_read_l (enum sreg s, ulong offset, u32 *data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (read_linearaddr_l (linear, data));
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_seg_read_q (enum sreg s, ulong offset, u64 *data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (read_linearaddr_q (linear, data));
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_seg_write_b (enum sreg s, ulong offset, u8 data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (write_linearaddr_b (linear, data));
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_seg_write_w (enum sreg s, ulong offset, u16 data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (write_linearaddr_w (linear, data));
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_seg_write_l (enum sreg s, ulong offset, u32 data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (write_linearaddr_l (linear, data));
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_seg_write_q (enum sreg s, ulong offset, u64 data)
{
	ulong linear;
	ulong acr;
	ulong base;

	current->vmctl.read_sreg_acr (s, &acr);
	current->vmctl.read_sreg_base (s, &base);
	if (acr & ACCESS_RIGHTS_UNUSABLE_BIT)
		return VMMERR_INVALID_GUESTSEG;
	if (!(acr & ACCESS_RIGHTS_P_BIT))
		return VMMERR_GUESTSEG_NOT_PRESENT;
	/* FIXME: expand-down */
	/* FIXME: limit check */
	/* FIXME: CPL check */
	/* FIXME: access rights check */
	linear = base + offset;
	RIE (write_linearaddr_q (linear, data));
	return VMMERR_SUCCESS;
}
