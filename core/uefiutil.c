/*
 * Copyright (c) 2009 Igel Co., Ltd
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

#include "calluefi.h"
#include "cpu.h"
#include "pcpu.h"
#include "uefi.h"
#include "uefiutil.h"

static bool
check_env (void)
{
	if (!uefi_booted)
		return false;
	if (!currentcpu_available () || uefi_no_more_call)
		return false;
	if (get_cpu_id ())
		return false;
	return true;
}

void
uefiutil_disconnect_pcidev_driver (ulong seg, ulong bus, ulong dev, ulong func)
{
	if (!check_env ())
		return;
	call_uefi_disconnect_pcidev_driver (seg, bus, dev, func);
}

void
uefiutil_netdev_get_mac_addr (ulong seg, ulong bus, ulong dev, ulong func,
			      void *mac, uint len)
{
	if (!check_env ())
		return;
	call_uefi_netdev_get_mac_addr (seg, bus, dev, func, mac, len);
}

int
uefiutil_get_graphics_info (u32 *hres, u32 *vres, u32 *rmask, u32 *gmask,
			    u32 *bmask, u32 *pxlin, u64 *addr, u64 *size)
{
	if (!check_env ())
		return -1;
	return call_uefi_get_graphics_info (hres, vres, rmask, gmask, bmask,
					    pxlin, addr, size);
}
