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

#ifndef _CORE_UEFI_H
#define _CORE_UEFI_H

#include "types.h"

extern void *uefi_conin;
extern void *uefi_conout;
extern ulong uefi_conin_read_key_stroke;
extern ulong uefi_conout_output_string;
extern ulong uefi_allocate_pages;
extern ulong uefi_get_memory_map;
extern ulong uefi_free_pages;
extern ulong uefi_wait_for_event;
extern ulong uefi_acpi_20_table;
extern ulong uefi_acpi_table;
extern ulong uefi_locate_handle_buffer;
extern ulong uefi_free_pool;
extern ulong uefi_locate_device_path;
extern ulong uefi_open_protocol;
extern ulong uefi_close_protocol;
extern ulong uefi_image_handle;
extern ulong uefi_disconnect_controller;
extern ulong uefi_boot_param_ext_addr;
extern ulong uefi_protocols_per_handle;
extern ulong uefi_uninstall_protocol_interface;
extern ulong uefi_create_event;
extern ulong uefi_boot_acpi_table_mod;
extern bool uefi_booted;
extern ulong uefi_get_time;

#endif
