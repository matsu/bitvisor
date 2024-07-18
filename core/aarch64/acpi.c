/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <arch/acpi.h>
#include <bits.h>
#include <core/aarch64/acpi.h>
#include <core/initfunc.h>
#include <core/list.h>
#include <core/mm.h>
#include <core/printf.h>
#include <core/string.h>
#include "../acpi.h"
#include "../acpi_dsdt.h"

#define ASRD_TYPE_MM 0x0
#define ASRD_TYPE_IO 0x1

#define ASRD_MM_SFLAGS_MM_TO_IO BIT (5)

#define ASRD_IO_SFLAGS_IO_TO_MM	 BIT (4)
#define ASRD_IO_SFLAGS_SPARSE_TL BIT (5)

#define ASDR_IO_SPARSE_ADDR(base) \
	((((base) & 0xFFFC) << 10) | ((base) & 0xFFF))

#define ASRD_TAG_IS_LARGE_RES(v) !!((v) & BIT (7))

#define ASRD_TAG_END   0x79
#define ASRD_TAG_WORD  0x88
#define ASRD_TAG_DWORD 0x87
#define ASRD_TAG_QWORD 0x8A
#define ASRD_TAG_EXT   0x8B

struct acpi_res_large_header {
	u8 tag;
	u16 size;
} __attribute__ ((packed));

struct acpi_asrd_header {
	struct acpi_res_large_header res;
	u8 type;
	u8 flags;
	u8 specific_flags;
} __attribute__ ((packed));

struct acpi_asrd_word {
	struct acpi_asrd_header header;
	u16 granularity;
	u16 range_min;
	u16 range_max;
	u16 translation_offset;
	u16 length;
	u8 extra[];
} __attribute__ ((packed));

struct acpi_asrd_dword {
	struct acpi_asrd_header header;
	u32 granularity;
	u32 range_min;
	u32 range_max;
	u32 translation_offset;
	u32 length;
	u8 extra[];
} __attribute__ ((packed));

struct acpi_asrd_qword {
	struct acpi_asrd_header header;
	u64 granularity;
	u64 range_min;
	u64 range_max;
	u64 translation_offset;
	u64 length;
	u8 extra[];
} __attribute__ ((packed));

struct acpi_asrd_ext {
	struct acpi_asrd_header header;
	u8 revision_id;
	u8 rsvd;
	u64 granularity;
	u64 range_min;
	u64 range_max;
	u64 translation_offset;
	u64 length;
	u64 specific_attr;
} __attribute__ ((packed));

struct acpi_pci_range {
	LIST1_DEFINE (struct acpi_pci_range);
	u64 range_min;
	u64 range_max;
	u64 translation_offset;
	u64 length;
	bool io;
	bool io_to_mm;
	bool io_sparse;
};

struct acpi_pci_res {
	LIST1_DEFINE (struct acpi_pci_res);
	LIST1_DEFINE_HEAD (struct acpi_pci_range, ranges);
	uint segment;
};

#define ASRD_COMMON_EXTRACT(a, r_start, r_end, tl_offset, len) \
	do { \
		*(r_start) = (a)->range_min; \
		*(r_end) = (a)->range_max; \
		*(tl_offset) = (a)->translation_offset; \
		*(len) = (a)->length; \
	} while (0)

#ifdef ACPI_DSDT

static LIST1_DEFINE_HEAD_INIT (struct acpi_pci_res, pci_res_list);

u64
acpi_arch_find_rsdp (void)
{
	return ACPI_RSDP_NOT_FOUND;
}

static void
asrd_word_extract (struct acpi_asrd_header *h, phys_t *r_start, phys_t *r_end,
		   phys_t *tl_offset, u64 *len)
{
	struct acpi_asrd_word *a = (struct acpi_asrd_word *)h;

	ASRD_COMMON_EXTRACT (a, r_start, r_end, tl_offset, len);
}

static void
asrd_dword_extract (struct acpi_asrd_header *h, phys_t *r_start, phys_t *r_end,
		    phys_t *tl_offset, u64 *len)
{
	struct acpi_asrd_dword *a = (struct acpi_asrd_dword *)h;

	ASRD_COMMON_EXTRACT (a, r_start, r_end, tl_offset, len);
}

static void
asrd_qword_extract (struct acpi_asrd_header *h, phys_t *r_start, phys_t *r_end,
		    phys_t *tl_offset, u64 *len)
{
	struct acpi_asrd_qword *a = (struct acpi_asrd_qword *)h;

	ASRD_COMMON_EXTRACT (a, r_start, r_end, tl_offset, len);
}

static void
asrd_ext_extract (struct acpi_asrd_header *h, phys_t *r_start, phys_t *r_end,
		  phys_t *tl_offset, u64 *len)
{
	asrd_qword_extract (h, r_start, r_end, tl_offset, len);
}

static struct acpi_pci_range *
acpi_record_res (struct acpi_asrd_header *h)
{
	struct acpi_pci_range *new_r;
	phys_t r_start, r_end, tl_offset;
	u64 len;
	u8 tag, type, sflags;

	tag = h->res.tag;
	switch (tag) {
	case ASRD_TAG_WORD:
		asrd_word_extract (h, &r_start, &r_end, &tl_offset, &len);
		break;
	case ASRD_TAG_DWORD:
		asrd_dword_extract (h, &r_start, &r_end, &tl_offset, &len);
		break;
	case ASRD_TAG_QWORD:
		asrd_qword_extract (h, &r_start, &r_end, &tl_offset, &len);
		break;
	case ASRD_TAG_EXT:
		asrd_ext_extract (h, &r_start, &r_end, &tl_offset, &len);
		break;
	default:
		printf ("%s(): unknown Address Space Resource 0x%X, "
			" skip record\n",
			__func__, tag);
		return NULL;
	}

	type = h->type;
	sflags = h->specific_flags;
	if (type == ASRD_TYPE_MM && (sflags & ASRD_MM_SFLAGS_MM_TO_IO)) {
		printf ("%s(): MM to IO on 0x%llX-0x%llX? skip record\n",
			__func__, r_start, r_end);
		return NULL;
	}

	new_r = alloc (sizeof *new_r);
	new_r->range_min = r_start;
	new_r->range_max = r_end;
	new_r->translation_offset = tl_offset;
	new_r->length = len;
	new_r->io = type == ASRD_TYPE_IO;
	new_r->io_to_mm = new_r->io && !!(sflags & ASRD_IO_SFLAGS_IO_TO_MM);
	new_r->io_sparse = new_r->io_to_mm &&
		!!(sflags & ASRD_IO_SFLAGS_SPARSE_TL);

	printf ("%s(): res [0x%llX-0x%llX] tl_offset 0x%llX %s\n", __func__,
		r_start, r_end, tl_offset, new_r->io ? "io" : "mm");

	return new_r;
}

static void
acpi_try_record_res (void *data, struct acpi_pci_res *pr)
{
	struct acpi_asrd_header *h = data;

	if (h->type == ASRD_TYPE_IO || h->type == ASRD_TYPE_MM) {
		struct acpi_pci_range *r = acpi_record_res (h);
		if (r)
			LIST1_ADD (pr->ranges, r);
	}
}

static void
acpi_record_pci_crs (u8 *start, u8 *end, struct acpi_pci_res *pr)
{
	struct acpi_res_large_header *h;
	u64 pkglen = 0, remaining = 0;
	uint stage = 0, i, size;
	u8 *c = start, bytecount, tag;

loop:
	if (c >= end)
		goto exit;
	switch (stage) {
	case 0:
		if (*c != 0x11) /* BufferOp */
			goto exit;
		c++;
		stage = 1;
		break;
	case 1:
		/* Calculate for the actual end */
		bytecount = (*c >> 6) & 0x3;
		pkglen = *c & (0x3F >> (2 - ((!!bytecount) << 1)));
		c++;
		for (i = 0; c < end && i < bytecount; i++) {
			pkglen |= (uint)*c << (4 + (8 * i));
			c++;
		}
		if (c + pkglen - 1 - bytecount <= end)
			end = c + pkglen - 1 - bytecount;
		stage = 2;
		break;
	case 2:
		if (*c != 0xA) /* ByteConst */
			goto exit;
		remaining = *(c + 1);
		c += 2;
		stage = 3;
		break;
	case 3:
		if (remaining == 0) {
			stage = 4;
		} else {
			tag = *c;
			if (tag == ASRD_TAG_END)
				goto exit;
			/*
			 * We currently expect the address translation to be in
			 * large resource type. There is no Address Translation
			 * Offset property in Small Resource Data Type.
			 */
			if (ASRD_TAG_IS_LARGE_RES (tag)) {
				h = (struct acpi_res_large_header *)c;
				size = h->size + sizeof *h;
				acpi_try_record_res (c, pr);
			} else {
				size = (tag & 0x7) + sizeof tag;
			}
			c += size;
			remaining -= size;
		}
		break;
	default:
		goto exit;
	}
	goto loop;
exit:
	return;
}
#endif

bool
acpi_pci_addr_translate (uint segment, phys_t addr, size_t len,
			 bool is_io_addr, phys_t *cpu_addr,
			 bool *is_io_addr_out)
{
	struct acpi_pci_res *pr;
	struct acpi_pci_range *r;

#ifdef ACPI_DSDT
	/*
	 * TODO: remove this once we know how to deal with multiple segments
	 * properly.
	 */
	if (segment != 0) {
		printf ("%s(): currently expect segment 0, returning false\n",
			__func__);
		return false;
	}

	LIST1_FOREACH (pci_res_list, pr) {
		if (pr->segment != segment)
			continue;
		LIST1_FOREACH (pr->ranges, r) {
			phys_t addr_end, r_start, r_end, base;

			if (is_io_addr) {
				if (!r->io)
					continue;
			} else {
				if (r->io)
					continue;
			}

			addr_end = addr + len - 1;
			r_start = r->range_min;
			r_end = r->range_max;
			if (addr < r_start || addr_end > r_end)
				continue;

			base = addr - r_start;
			if (r->io_sparse)
				base = ASDR_IO_SPARSE_ADDR (base);
			*cpu_addr = base + r_start + r->translation_offset;
			*is_io_addr_out = r->io_to_mm;
			return true;
		}
	}
#endif
	return false;
}

#ifdef ACPI_DSDT
static void
acpi_pci_init (void)
{
	struct acpi_pci_res *new_r;
	phys_t dsdt_addr, crs_start;
	u64 readable_len;
	u8 *s, *e;
	bool found = false;

	dsdt_addr = acpi_get_dsdt_addr ();
	if (!dsdt_addr)
		return;

	/*
	 * TODO: deal with multiple segment situation. We currently don't know
	 * how to ACPI DSDT looks like when multiple segments exist.
	 */
	new_r = alloc (sizeof *new_r);
	LIST1_HEAD_INIT (new_r->ranges);
	new_r->segment = 0;

	found = acpi_dsdt_search_ns (dsdt_addr, "_SB_PCI0_CRS", 12,
					&crs_start, &readable_len);
	if (found) {
		s = mapmem_hphys (crs_start, readable_len, 0);
		e = s + readable_len - 1;
		acpi_record_pci_crs (s, e, new_r);
		unmapmem (s, readable_len);
	}

	LIST1_ADD (pci_res_list, new_r);
}

INITFUNC ("global6", acpi_pci_init);
#endif
