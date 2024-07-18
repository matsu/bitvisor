/*
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

#include <constants.h>
#include <core/types.h>
#include <section.h>

#define ENTRY_SIZE (512 * KB) /* Bootstrap code size */

/* From AArch64 ELF format specification */
#define R_AARCH64_NONE	   0
#define R_AARCH64_RELATIVE 1027 /* Delta(S) + Addend */

struct rela_entry {
	u64 r_offset; /* Location to apply relocation */
	u64 r_info; /* Relocation type */
	u64 r_addend;
};

int
apply_reloc (u64 base, struct rela_entry *start, struct rela_entry *end)
{
	struct rela_entry *entries = start;
	u64 *target;
	unsigned int i, n_entries = end - start;

	for (i = 0; i < n_entries; i++) {
		switch (entries[i].r_info) {
		case R_AARCH64_NONE:
			break; /* Do nothing */
		case R_AARCH64_RELATIVE:
			/*
			 * Static head address is 0. That means Delta(S) is
			 * the runtime address.
			 */
			target = (u64 *)(base + entries[i].r_offset);
			*target = base + entries[i].r_addend;
			break;
		default:
			/* Current deal with only R_AARCH64_RELATIVE */
			return -1;
		}
	}

	return 0;
}

int SECTION_ENTRY_TEXT
apply_reloc_on_entry (u64 base, struct rela_entry *start,
		      struct rela_entry *end)
{
	struct rela_entry *entries = start;
	u64 *target;
	unsigned int i, n_entries = end - start;

	for (i = 0; i < n_entries; i++) {
		switch (entries[i].r_info) {
		case R_AARCH64_NONE:
			break; /* Do nothing */
		case R_AARCH64_RELATIVE:
			if (entries[i].r_offset > ENTRY_SIZE)
				continue;
			target = (u64 *)(base + entries[i].r_offset);
			*target = base + entries[i].r_addend;
			break;
		default:
			return -1;
		}
	}

	return 0;
}
