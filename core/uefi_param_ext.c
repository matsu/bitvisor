/*
 * Copyright (c) 2017 Igel Co., Ltd.
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
 * 3. Neither the name of the copyright holder nor the names of its
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

#include "initfunc.h"
#include "mm.h"
#include "panic.h"
#include "string.h"
#include "types.h"
#include "uefi.h"
#include "uefi_param_ext.h"
#include <share/uefi_boot.h>

static phys_t boot_param_ext_addrs[MAX_N_PARAM_EXTS];
static u64 n_param_exts;

static u8
compare_uuid (struct uuid *uuid1, struct uuid *uuid2)
{
	u64 *val1 = (u64 *)uuid1;
	u64 *val2 = (u64 *)uuid2;

	return (val1[0] == val2[0] &&
		val1[1] == val2[1]);
}

phys_t
uefi_param_ext_get_phys (struct uuid *ext_uuid)
{
	if (n_param_exts == 0 || !ext_uuid)
		return 0x0;

	phys_t addr = 0x0;
	struct param_ext *base;
	uint i;

	for (i = 0; i < n_param_exts && addr == 0x0; i++) {
		base = mapmem_hphys (boot_param_ext_addrs[i], sizeof *base, 0);
		if (compare_uuid (&base->uuid, ext_uuid))
			addr = boot_param_ext_addrs[i];
		unmapmem (base, sizeof *base);
	}

	return addr;
}

static void
uefi_param_ext_init (void)
{
	if (uefi_boot_param_ext_addr == 0x0)
		return;

	phys_t *param_ext_map;
	param_ext_map = mapmem_hphys (uefi_boot_param_ext_addr,
				      sizeof (phys_t) * MAX_N_PARAM_EXTS, 0);

	uint i;
	for (i = 0; i < MAX_N_PARAM_EXTS && param_ext_map[i]; i++) {
		boot_param_ext_addrs[i] = param_ext_map[i];
		n_param_exts++;
	}
	if (param_ext_map[i])
		panic ("Number of parameter extension beyond the limit");

	unmapmem (param_ext_map, sizeof (phys_t) * MAX_N_PARAM_EXTS);
}

INITFUNC ("global5", uefi_param_ext_init);
