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

#ifndef _UEFI_BOOT_H
#define _UEFI_BOOT_H

#define MAX_N_PARAM_EXTS (256)

#define UEFI_BITVISOR_BOOT_UUID \
	{0x4CF80319, 0xA870, 0x44C5, \
	{0x9A, 0x87, 0x60, 0xE5, 0x86, 0xE7, 0x9D, 0x0F}}

#define UEFI_BITVISOR_PASS_AUTH_UUID \
	{0xE0970CB4, 0xDF2E, 0x44D1, \
	{0xB1, 0xA9, 0x63, 0x3C, 0xCD, 0xE3, 0xA2, 0xC6}}

#define UEFI_BITVISOR_CPU_TYPE_UUID \
	{0x0992D209, 0x72B0, 0x491F, \
	{0xA2, 0xBC, 0xC2, 0x3D, 0x39, 0xF9, 0x78, 0x76}}

struct uuid {
	unsigned int   field1;
	unsigned short field2;
	unsigned short field3;
	unsigned char  field4[8];
};

struct param_ext {
	struct uuid uuid;
};

struct bitvisor_boot {
	struct param_ext base;
	/* IN */
	unsigned long long int loadaddr;
	unsigned long long int loadsize;
	void *file;
};

#define INITIAL_BOOT_EXT {{UEFI_BITVISOR_BOOT_UUID}, 0, 0, 0}

/* ------------------------------------------------------------------------- */

struct pass_auth_param_ext {
	struct param_ext base;
};

#define INITIAL_PASS_AUTH_EXT {{UEFI_BITVISOR_PASS_AUTH_UUID}}

/* ------------------------------------------------------------------------- */

#define CPU_TYPE_INTEL (1)
#define CPU_TYPE_AMD   (1 << 1)

struct cpu_type_param_ext {
	struct param_ext base;
	/* OUT */
	unsigned long long int type;
};

#define INITIAL_CPU_TYPE_EXT {{UEFI_BITVISOR_CPU_TYPE_UUID}, 0}

/* ------------------------------------------------------------------------- */

#endif
