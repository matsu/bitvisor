/*
 * Copyright (c) 2013 Igel Co., Ltd.
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

#ifndef _CORE_CALLUEFI_H
#define _CORE_CALLUEFI_H

#include "types.h"

extern u8 uefi_memory_map_data[16384];
extern ulong uefi_memory_map_size;
extern ulong uefi_memory_map_descsize;

void call_uefi_get_memory_map (void);
int call_uefi_allocate_pages (int type, int memtype, u64 npages, u64 *phys);
int call_uefi_free_pages (u64 phys, u64 npages);
int call_uefi_create_event_exit_boot_services (u64 phys, u64 context,
					       void **event_ret);
int call_uefi_boot_acpi_table_mod (char *signature, u64 table_addr);
u32 call_uefi_getkey (void);
void call_uefi_putchar (unsigned char c);
void call_uefi_disconnect_pcidev_driver (ulong seg, ulong bus, ulong dev,
					 ulong func);
void call_uefi_netdev_get_mac_addr (ulong seg, ulong bus, ulong dev,
				    ulong func, void *mac, uint len);
int call_uefi_get_graphics_info (u32 *hres, u32 *vres, u32 *rmask, u32 *gmask,
				 u32 *bmask, u32 *pxlin, u64 *addr, u64 *size);
void copy_uefi_bootcode (void);

#endif
