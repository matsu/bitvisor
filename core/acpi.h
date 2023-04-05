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

#ifndef _CORE_ACPI_H
#define _CORE_ACPI_H

#include <core/acpi.h>
#include <core/types.h>
#include <io.h>

#define ACPI_SIGNATURE_LEN	4

struct acpi_description_header {
	u8 signature[4];
	u32 length;
	u8 revision;
	u8 checksum;
	u8 oemid[6];
	u8 oem_table_id[8];
	u8 oem_revision[4];
	u8 creator_id[4];
	u8 creator_revision[4];
} __attribute__ ((packed));

struct acpi_data {
	bool iopass;
	bool smi_hook_disabled;
};

u8 acpi_checksum (void *p, int len);
void *acpi_mapmem (u64 addr, int len);
void *acpi_find_entry (char *signature);
void acpi_itr_rsdt1_entry (void *(*func) (void *data, u64 entry), void *data);
void acpi_itr_rsdt_entry (void *(*func) (void *data, u64 entry), void *data);
void acpi_itr_xsdt_entry (void *(*func) (void *data, u64 entry), void *data);
void acpi_modify_table (char *signature, u64 address);

void acpi_iohook (void);
void acpi_poweroff (void);
bool get_acpi_time_raw (u32 *r);
void acpi_smi_hook (void);
void acpi_reset (void);

#endif
