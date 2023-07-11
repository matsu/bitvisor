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

#include <core/initfunc.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include "acpi.h"
#include "acpi_constants.h"
#include "acpi_dsdt.h"
#include "assert.h"
#include "calluefi.h"
#include "constants.h"
#include "uefi.h"

#define FIND_RSDP_NOT_FOUND	0xFFFFFFFFFFFFFFFFULL
#define RSDP_SIGNATURE		"RSD PTR"
#define RSDP_SIGNATURE_LEN	7
#define ADDRESS_SPACE_ID_MEM	0
#define ADDRESS_SPACE_ID_IO	1
#define RSDT_SIGNATURE		"RSDT"
#define XSDT_SIGNATURE		"XSDT"
#define FACP_SIGNATURE		"FACP"
#define FACS_SIGNATURE		"FACS"
#define MCFG_SIGNATURE		"MCFG"
#define SSDT_SIGNATURE		"SSDT"
#define PM1_CNT_SLP_TYPX_MASK	0x1C00
#define PM1_CNT_SLP_TYPX_SHIFT	10
#define PM1_CNT_SLP_EN_BIT	ACPI_PM1_CNT_SLP_EN_BIT
#define IS_STRUCT_SIZE_OK(l, h, m) \
	((l) >= ((u8 *)&(m) - (u8 *)(h)) + sizeof (m))
#define ACCESS_SIZE_UNDEFINED	0
#define ACCESS_SIZE_BYTE	1
#define ACCESS_SIZE_WORD	2
#define ACCESS_SIZE_DWORD	3
#define ACCESS_SIZE_QWORD	4
#define NFACS_ADDR		6
#define FACP_FLAGS_RESET_REG_SUP_BIT 0x400

struct rsdp {
	u8 signature[8];
	u8 checksum;
	u8 oemid[6];
	u8 revision;
	u32 rsdt_address;
} __attribute__ ((packed));

struct rsdpv2 {
	struct rsdp v1;
	u32 length;
	u64 xsdt_address;
	u8 checksum;
	u8 reserved[3];
} __attribute__ ((packed));

struct rsdt {
	struct acpi_description_header header;
	u32 entry[];
} __attribute__ ((packed));

struct xsdt {
	struct acpi_description_header header;
	u64 entry[];
} __attribute__ ((packed));

struct gas {
	u8 address_space_id;
	u8 register_bit_width;
	u8 register_bit_offset;
	u8 access_size; /* ACPI 3.0+, undefined(0) otherwise */
	u64 address;
} __attribute__ ((packed));

struct facp {
	struct acpi_description_header header;
	u32 firmware_ctrl;
	u32 dsdt;
	u8 reserved1;
	u8 preferred_pm_profile;
	u16 sci_int;
	u32 smi_cmd;
	u8 acpi_enable;
	u8 acpi_disable;
	u8 s4bios_req;
	u8 pstate_cnt;
	u32 pm1a_evt_blk;
	u32 pm1b_evt_blk;
	u32 pm1a_cnt_blk;
	u32 pm1b_cnt_blk;
	u32 pm2_cnt_blk;
	u32 pm_tmr_blk;
	u32 gpe0_blk;
	u32 gpe1_blk;
	u8 pm1_evt_len;
	u8 pm1_cnt_len;
	u8 pm2_cnt_len;
	u8 pm_tmr_len;
	u8 gpe0_blk_len;
	u8 gpe1_blk_len;
	u8 gpe1_base;
	u8 cst_cnt;
	u16 p_lvl2_lat;
	u16 p_lvl3_lat;
	u16 flush_size;
	u16 flush_stride;
	u8 duty_offset;
	u8 duty_width;
	u8 day_alrm;
	u8 mon_alrm;
	u8 century;
	u16 iapc_boot_arch;
	u8 reserved2;
	u32 flags;
	struct gas reset_reg;
	u8 reset_value;
	u8 reserved3[3];
	u64 x_firmware_ctrl;
	u64 x_dsdt;
	struct gas x_pm1a_evt_blk;
	struct gas x_pm1b_evt_blk;
	struct gas x_pm1a_cnt_blk;
	struct gas x_pm1b_cnt_blk;
	struct gas x_pm2_cnt_blk;
	struct gas x_pm_tmr_blk;
	struct gas x_gpe0_blk;
	struct gas x_gpe1_blk;
} __attribute__ ((packed));

struct facs {
	u8 signature[4];
	u32 length;
	u8 hardware_signature[4];
	u32 firmware_waking_vector;
	u32 global_lock;
	u32 flags;
	u64 x_firmware_waking_vector;
	u8 version;
	u8 reserved[31];
} __attribute__ ((packed));

struct mcfg {
	struct acpi_description_header header;
	u8 reserved[8];
	struct {
		u64 base;	/* 0 */
		u16 seg_group;	/* 8 */
		u8 bus_start;	/* 10 */
		u8 bus_end;	/* 11 */
		u8 reserved[4];	/* 12 */
	} __attribute__ ((packed)) configs[1];
} __attribute__ ((packed));

enum acpi_reg_check {
	ACPI_REG_CHECK_NULL, /* Do not change ACPI_REG_CHECK_NULL position */
	ACPI_REG_CHECK_SPACE_ID_UNSUPPORTED,
	ACPI_REG_CHECK_BIT_WIDTH_ZERO,
	ACPI_REG_CHECK_BIT_OFFSET,
	ACPI_REG_CHECK_UNKNOWN_ACCESS_SIZE,
	ACPI_REG_CHECK_MEM_INVALID_ADDR,
	ACPI_REG_CHECK_IO_INVALID_ADDR,
	ACPI_REG_CHECK_OK_MEM,
	ACPI_REG_CHECK_OK_IO,
};

struct acpi_reg {
	char *name;
	enum acpi_reg_check result;
	u8 size;
	u64 addr;
	void *mapped_addr;
};

static struct acpi_reg acpi_reg_pm1a_cnt = { "ACPI_REG_PM1A_CNT" };
static struct acpi_reg acpi_reg_pm_tmr = { "ACPI_REG_PM_TMR" };
static struct acpi_reg acpi_reg_reset = { "ACPI_REG_RESET" };
static struct acpi_reg acpi_reg_smi_cmd = { "ACPI_REG_SMI_CMD" };

static bool rsdp_found;
static struct rsdpv2 rsdp_copy;
static bool rsdp1_found;
static struct rsdp rsdp1_copy;
static u64 facs_addr[NFACS_ADDR];
static u8 reset_value;
#ifdef ACPI_DSDT
static u32 dsdt_addr;
#endif
static struct mcfg *saved_mcfg;

u8
acpi_checksum (void *p, int len)
{
	u8 *q, s;

	s = 0;
	for (q = p; len > 0; len--)
		s += *q++;
	return s;
}

void *
acpi_mapmem (u64 addr, int len)
{
	static void *oldmap;
	static int oldlen = 0;

	if (oldlen)
		unmapmem (oldmap, oldlen);
	oldlen = len;
	oldmap = mapmem_hphys (addr, len, MAPMEM_WRITE);
	ASSERT (oldmap);
	return oldmap;
}

static bool
acpi_reg_read (struct acpi_reg *r, u64 *value)
{
	union mem tmp;

	tmp.qword = 0;

	switch (r->result) {
	case ACPI_REG_CHECK_OK_MEM:
		memcpy (&tmp, r->mapped_addr, r->size);
		*value = tmp.qword;
		break;
	case ACPI_REG_CHECK_OK_IO:
		switch (r->size) {
		case 1:
			in8 (r->addr, &tmp.byte);
			break;
		case 2:
			in16 (r->addr, &tmp.word);
			break;
		case 4:
			in32 (r->addr, &tmp.dword);
			break;
		default:
			return false;
		}
		*value = tmp.qword;
		break;
	default:
		return false;
	}

	return true;
}

static bool
acpi_reg_write (struct acpi_reg *r, u64 value)
{
	switch (r->result) {
	case ACPI_REG_CHECK_OK_MEM:
		memcpy (r->mapped_addr, &value, r->size);
		break;
	case ACPI_REG_CHECK_OK_IO:
		switch (r->size) {
		case 1:
			out8 (r->addr, value);
			break;
		case 2:
			out16 (r->addr, value);
			break;
		case 4:
			out32 (r->addr, value);
			break;
		default:
			return false;
		}
		break;
	default:
		return false;
	}

	return true;
}

static bool
acpi_reg_get_ioaddr (struct acpi_reg *r, u32 *ioaddr)
{
	bool ret = false;

	if (ioaddr) {
		if (r->result == ACPI_REG_CHECK_OK_IO) {
			*ioaddr = r->addr;
			ret = true;
		}
	}

	return ret;
}

bool
acpi_reg_pm1a_cnt_write (u64 val)
{
	return acpi_reg_write (&acpi_reg_pm1a_cnt, val);
}

bool
acpi_reg_get_pm1a_cnt_ioaddr (u32 *ioaddr)
{
	return acpi_reg_get_ioaddr (&acpi_reg_pm1a_cnt, ioaddr);
}

bool
acpi_reg_get_smi_cmd_ioaddr (u32 *ioaddr)
{
	return acpi_reg_get_ioaddr (&acpi_reg_smi_cmd, ioaddr);
}

static void
acpi_reg_skip_init_log (struct acpi_reg *r)
{
	printf ("acpi: skip %s initialization\n", r->name);
}

static void
get_reg_info_with_gas_log (struct acpi_reg *r, struct gas *g)
{
	switch (r->result) {
	case ACPI_REG_CHECK_NULL:
		printf ("acpi: %s gas structure is NULL\n", r->name);
		break;
	case ACPI_REG_CHECK_SPACE_ID_UNSUPPORTED:
		printf ("acpi: %s gas unsupported address space id %u\n",
			r->name, g->address_space_id);
		break;
	case ACPI_REG_CHECK_BIT_WIDTH_ZERO:
		printf ("acpi: %s gas unexpected bit width zero\n", r->name);
		break;
	case ACPI_REG_CHECK_BIT_OFFSET:
		printf ("acpi: %s gas unexpected bit offset %u\n", r->name,
			g->register_bit_offset);
		break;
	case ACPI_REG_CHECK_UNKNOWN_ACCESS_SIZE:
		printf ("acpi: %s gas structure unknown access size %u\n",
			r->name, g->access_size);
		break;
	case ACPI_REG_CHECK_MEM_INVALID_ADDR:
		printf ("acpi: %s gas MEM invalid address 0x%llX\n", r->name,
			g->address);
		break;
	case ACPI_REG_CHECK_IO_INVALID_ADDR:
		printf ("acpi: %s gas IO invalid address 0x%llX\n", r->name,
			g->address);
		break;
	case ACPI_REG_CHECK_OK_MEM:
	case ACPI_REG_CHECK_OK_IO:
		/* Do nothing */
		break;
	default:
		printf ("acpi: %s gas unknown result %u\n", r->name,
			r->result);
		break;
	}
}

static u64
get_ebda_address (void)
{
	u16 *p;

	if (uefi_acpi_20_table != ~0UL)
		return uefi_acpi_20_table;
	if (uefi_acpi_table != ~0UL)
		return uefi_acpi_table;
	p = acpi_mapmem (0x40E, sizeof *p);
	return ((u64)*p) << 4;
}

static u64
find_rsdp_iapc_sub (u64 start, u64 end)
{
	struct rsdp *p;
	u64 i;

	for (i = start; i < end; i += 16) {
		p = acpi_mapmem (i, sizeof *p);
		if (!memcmp (p->signature, RSDP_SIGNATURE, RSDP_SIGNATURE_LEN)
		    && !acpi_checksum (p, sizeof *p))
			return i;
	}
	return FIND_RSDP_NOT_FOUND;
}

static u64
find_rsdp_iapc (void)
{
	u64 ebda;
	u64 rsdp;

	ebda = get_ebda_address ();
	rsdp = find_rsdp_iapc_sub (ebda, ebda + 0x3FF);
	if (rsdp == FIND_RSDP_NOT_FOUND)
		rsdp = find_rsdp_iapc_sub (0xE0000, 0xFFFFF);
	return rsdp;
}

static u64
find_rsdp (void)
{
	return find_rsdp_iapc ();
}

/* Return ACPIv1 table address if UEFI firmware provides ACPIv1 table
 * at different address from ACPIv2 table. */
static u64
find_rsdp1 (void)
{
	/* If uefi_acpi_20_table == ~0UL && uefi_acpi_table != ~0UL,
	 * uefi_acpi_table is returned by find_rsdp(). */
	/* If uefi_acpi_20_table == ~0UL && uefi_acpi_table == ~0UL,
	 * it is BIOS booted. */
	/* If uefi_acpi_20_table != ~0UL && uefi_acpi_table == ~0UL,
	 * ACPIv1 table is not provided. */
	if (uefi_acpi_20_table == ~0UL || uefi_acpi_table == ~0UL)
		return FIND_RSDP_NOT_FOUND;
	/* If uefi_acpi_20_table == uefi_acpi_table, uefi_acpi_table
	 * is returned by find_rsdp(). */
	if (uefi_acpi_20_table == uefi_acpi_table)
		return FIND_RSDP_NOT_FOUND;
	u64 ebda = uefi_acpi_table;
	return find_rsdp_iapc_sub (ebda, ebda + 0x3FF);
}

static void *
foreach_entry_in_rsdt_at (void *(*func) (void *data, u64 entry), void *data,
			  u64 rsdt_address)
{
	struct rsdt *p;
	void *ret = NULL;
	int i, n, len = 0;

	p = mapmem_hphys (rsdt_address, sizeof *p, 0);
	if (!memcmp (p->header.signature, RSDT_SIGNATURE, ACPI_SIGNATURE_LEN))
		len = p->header.length;
	unmapmem (p, sizeof *p);
	if (len < sizeof *p)
		return NULL;
	p = mapmem_hphys (rsdt_address, len, 0);
	if (!acpi_checksum (p, len)) {
		n = (p->header.length - sizeof p->header) / sizeof p->entry[0];
		for (i = 0; i < n; i++) {
			ret = func (data, p->entry[i]);
			if (ret)
				break;
		}
	}
	unmapmem (p, len);
	return ret;
}

static void *
foreach_entry_in_rsdt (void *(*func) (void *data, u64 entry), void *data)
{
	/* rsdp_copy.v1.rsdt_address can be NULL depending on the platform */
	if (!rsdp_found || !rsdp_copy.v1.rsdt_address)
		return NULL;
	return foreach_entry_in_rsdt_at (func, data,
					 rsdp_copy.v1.rsdt_address);
}

static void *
foreach_entry_in_rsdt1 (void *(*func) (void *data, u64 entry), void *data)
{
	if (!rsdp1_found)
		return NULL;
	return foreach_entry_in_rsdt_at (func, data,
					 rsdp1_copy.rsdt_address);
}

static void *
foreach_entry_in_xsdt (void *(*func) (void *data, u64 entry), void *data)
{
	struct xsdt *p;
	void *ret = NULL;
	int i, n, len = 0;

	if (!rsdp_found)
		return NULL;
	if (!rsdp_copy.length)
		return NULL;
	p = mapmem_hphys (rsdp_copy.xsdt_address, sizeof *p, 0);
	if (!memcmp (p->header.signature, XSDT_SIGNATURE, ACPI_SIGNATURE_LEN))
		len = p->header.length;
	unmapmem (p, sizeof *p);
	if (len < sizeof *p)
		return NULL;
	p = mapmem_hphys (rsdp_copy.xsdt_address, len, 0);
	if (!acpi_checksum (p, len)) {
		n = (p->header.length - sizeof p->header) / sizeof p->entry[0];
		for (i = 0; i < n; i++) {
			ret = func (data, p->entry[i]);
			if (ret)
				break;
		}
	}
	unmapmem (p, len);
	return ret;
}

static void *
find_entry_sub (void *data, u64 entry)
{
	struct acpi_description_header *q;
	char *signature = data;

	q = acpi_mapmem (entry, sizeof *q);
	if (memcmp (q->signature, signature, ACPI_SIGNATURE_LEN))
		return NULL;
	q = acpi_mapmem (entry, q->length);
	if (acpi_checksum (q, q->length))
		return NULL;
	return q;
}

static void *
find_entry_in_rsdt (char *signature)
{
	return foreach_entry_in_rsdt (find_entry_sub, signature);
}

static void *
find_entry_in_rsdt1 (char *signature)
{
	return foreach_entry_in_rsdt1 (find_entry_sub, signature);
}

static void *
find_entry_in_xsdt (char *signature)
{
	return foreach_entry_in_xsdt (find_entry_sub, signature);
}

static void *
find_entry (char *signature)
{
	void *ret;

	ret = find_entry_in_xsdt (signature);
	if (!ret)
		ret = find_entry_in_rsdt (signature);
	return ret;
}

static struct facp *
find_facp (void)
{
	return find_entry (FACP_SIGNATURE);
}

static void
save_mcfg (void)
{
	struct mcfg *d;

	d = find_entry (MCFG_SIGNATURE);
	if (d) {
		saved_mcfg = alloc (d->header.length);
		memcpy (saved_mcfg, d, d->header.length);
	}
}

static void
debug_dump (void *p, int len)
{
	u8 *q;
	int i, j;

	q = p;
	for (i = 0; i < len; i += 16) {
		printf ("%08X ", i);
		for (j = 0; j < 16; j++)
			printf ("%02X%c", q[i + j], j == 7 ? '-' : ' ');
		for (j = 0; j < 16; j++)
			printf ("%c", q[i + j] >= 0x20 && q[i + j] <= 0x7E
				? q[i + j] : '.');
		printf ("\n");
	}
}

static void
debug_dump_reg (struct acpi_reg *r)
{
	switch (r->result) {
	case ACPI_REG_CHECK_OK_MEM:
		printf ("acpi: %s at 0x%llX (MEM space)\n", r->name, r->addr);
		break;
	case ACPI_REG_CHECK_OK_IO:
		printf ("acpi: %s at 0x%llX (IO space)\n", r->name, r->addr);
		break;
	default:
		printf ("acpi: %s not found, check result %u\n",
			r->name, r->result);
		break;
	}
}

static bool
ioaddr_is_valid (u32 ioaddr)
{
	return ioaddr <= 0xFFFF;
}

static bool
get_reg_info_with_gas (struct acpi_reg *r, struct gas *g, u32 default_size)
{
	u32 reg_size, res_ok;

	/* We validate GAS structure first */
	if (g->address_space_id == 0 && g->register_bit_width == 0 &&
	    g->register_bit_offset == 0 && g->access_size == 0 &&
	    g->address == 0) {
		r->result = ACPI_REG_CHECK_NULL;
		goto end;
	}
	switch (g->address_space_id) {
	case ADDRESS_SPACE_ID_MEM:
		if (g->address == 0) {
			r->result = ACPI_REG_CHECK_MEM_INVALID_ADDR;
			goto end;
		}
		res_ok = ACPI_REG_CHECK_OK_MEM;
		break;
	case ADDRESS_SPACE_ID_IO:
		if (!ioaddr_is_valid (g->address)) {
			r->result = ACPI_REG_CHECK_IO_INVALID_ADDR;
			goto end;
		}
		res_ok = ACPI_REG_CHECK_OK_IO;
		break;
	default:
		/* We currently support only MMIO and IO */
		r->result = ACPI_REG_CHECK_SPACE_ID_UNSUPPORTED;
		goto end;
	}
	if (g->address_space_id != ADDRESS_SPACE_ID_MEM &&
	    g->address_space_id != ADDRESS_SPACE_ID_IO) {
		/* We currently support only MMIO and IO */
		r->result = ACPI_REG_CHECK_SPACE_ID_UNSUPPORTED;
		goto end;
	}
	if (g->register_bit_width == 0) {
		r->result = ACPI_REG_CHECK_BIT_WIDTH_ZERO;
		goto end;
	}
	if (g->register_bit_offset != 0) {
		r->result = ACPI_REG_CHECK_BIT_OFFSET;
		goto end;
	}

	/* We validate GAS access_size info next */
	switch (g->access_size) {
	case ACCESS_SIZE_UNDEFINED: /* For old ACPI 2.0 or earlier */
		reg_size = default_size;
		break;
	case ACCESS_SIZE_BYTE:
		reg_size = 1;
		break;
	case ACCESS_SIZE_WORD:
		reg_size = 2;
		break;
	case ACCESS_SIZE_DWORD:
		reg_size = 4;
		break;
	case ACCESS_SIZE_QWORD:
		reg_size = 8;
		break;
	default:
		r->result = ACPI_REG_CHECK_UNKNOWN_ACCESS_SIZE;
		goto end;
	}

	r->result = res_ok;
	r->size	= reg_size;
	r->addr = g->address;
	if (res_ok == ACPI_REG_CHECK_OK_MEM)
		r->mapped_addr = mapmem_hphys (r->addr, reg_size,
					       MAPMEM_WRITE | MAPMEM_UC);
end:
	get_reg_info_with_gas_log (r, g);
	return r->result == ACPI_REG_CHECK_OK_MEM ||
	       r->result == ACPI_REG_CHECK_OK_IO;
}

static bool
get_reg_info_with_io (struct acpi_reg *r, u32 ioaddr, u32 default_size)
{
	if (!ioaddr_is_valid (ioaddr)) {
		printf ("acpi: %s IO invalid address 0x%X\n", r->name,
			ioaddr);
		r->result = ACPI_REG_CHECK_IO_INVALID_ADDR;
		goto end;
	}

	r->result = ACPI_REG_CHECK_OK_IO;
	r->size	= default_size;
	r->addr = ioaddr;
end:
	return r->result == ACPI_REG_CHECK_OK_IO;
}

static void
get_pm1a_cnt_info (struct facp *q)
{
	bool pm1a_gas_ok = false;
	bool pm1a_old_ok = false;

	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->x_pm1a_cnt_blk))
		pm1a_gas_ok = get_reg_info_with_gas (&acpi_reg_pm1a_cnt,
						     &q->x_pm1a_cnt_blk, 4);
	if (!pm1a_gas_ok) {
		if (IS_STRUCT_SIZE_OK (q->header.length, q, q->pm1a_cnt_blk))
			pm1a_old_ok = get_reg_info_with_io (&acpi_reg_pm1a_cnt,
							    q->pm1a_cnt_blk,
							    4);
		else
			panic ("acpi: FACP is too short");
	}

	if (!pm1a_gas_ok && !pm1a_old_ok)
		panic ("acpi: PM1a control not found");
}

static void
get_pm_tmr_info (struct facp *q)
{
	bool pm_tmr_gas_ok = false;
	bool pm_tmr_old_ok = false;

	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->x_pm_tmr_blk))
		pm_tmr_gas_ok = get_reg_info_with_gas (&acpi_reg_pm_tmr,
						       &q->x_pm_tmr_blk, 4);
	if (!pm_tmr_gas_ok) {
		if (IS_STRUCT_SIZE_OK (q->header.length, q, q->pm_tmr_blk))
			pm_tmr_old_ok = get_reg_info_with_io (&acpi_reg_pm_tmr,
							      q->pm_tmr_blk,
							      4);
	}

	if (!pm_tmr_gas_ok && !pm_tmr_old_ok)
		acpi_reg_skip_init_log (&acpi_reg_pm_tmr);
}

static void
get_facs_addr (u64 facs[2], struct facp *q)
{
	facs[0] = 0;
	facs[1] = 0;
	if (!q)
		return;
	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->x_firmware_ctrl))
		facs[0] = q->x_firmware_ctrl;
	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->firmware_ctrl))
		facs[1] = q->firmware_ctrl;
	else
		panic ("ACPI FACP is too short");
}

static void
get_reset_info (struct facp *q)
{
	bool reset_info_ok = false;

	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->flags) &&
	    (q->flags & FACP_FLAGS_RESET_REG_SUP_BIT) &&
	    IS_STRUCT_SIZE_OK (q->header.length, q, q->reset_reg) &&
	    IS_STRUCT_SIZE_OK (q->header.length, q, q->reset_value)) {
		reset_info_ok = get_reg_info_with_gas (&acpi_reg_reset,
						       &q->reset_reg, 1);
		reset_value = q->reset_value;
	}

	if (!reset_info_ok)
		acpi_reg_skip_init_log (&acpi_reg_reset);
}

static void
get_smi_cmd_info (struct facp *q)
{
	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->smi_cmd))
		get_reg_info_with_io (&acpi_reg_smi_cmd, q->smi_cmd, 4);
}

void
acpi_reset (void)
{
	acpi_reg_write (&acpi_reg_reset, reset_value);
}

void
acpi_poweroff (void)
{
	u64 data, typx;

	if (!acpi_dsdt_system_state[5][0])
		return;
	/* FIXME: how to handle pm1b_cnt? */
	if (acpi_reg_read (&acpi_reg_pm1a_cnt, &data)) {
		typx = acpi_dsdt_system_state[5][1] << PM1_CNT_SLP_TYPX_SHIFT;
		data &= ~PM1_CNT_SLP_TYPX_MASK;
		data |= typx & PM1_CNT_SLP_TYPX_MASK;
		data |= PM1_CNT_SLP_EN_BIT;
		acpi_reg_write (&acpi_reg_pm1a_cnt, data);
	}
}

bool
get_acpi_time_raw (u32 *r)
{
	u64 tmp;

	if (acpi_reg_read (&acpi_reg_pm_tmr, &tmp)) {
		tmp &= 16777215;
		*r = tmp;
		return true;
	}
	return false;
}

bool
acpi_read_mcfg (uint n, u64 *base, u16 *seg_group, u8 *bus_start,
		u8 *bus_end)
{
	if (!saved_mcfg)
		return false;
	if ((u8 *)&saved_mcfg->configs[n + 1] - (u8 *)saved_mcfg >
	    saved_mcfg->header.length)
		return false;
	*base = saved_mcfg->configs[n].base;
	*seg_group = saved_mcfg->configs[n].seg_group;
	*bus_start = saved_mcfg->configs[n].bus_start;
	*bus_end = saved_mcfg->configs[n].bus_end;
	return true;
}

#ifdef ACPI_DSDT
static void *
call_ssdt_parse (void *data, u64 entry)
{
	struct acpi_description_header *p;
	int *n = data;
	u32 len = 0;
	u8 *q;

	p = mapmem_hphys (entry, sizeof *p, 0);
	if (!memcmp (p->signature, SSDT_SIGNATURE, ACPI_SIGNATURE_LEN))
		len = p->length;
	unmapmem (p, sizeof *p);
	if (len > sizeof *p) {
		q = mapmem_hphys (entry, len, MAPMEM_WRITE);
		if (!acpi_checksum (q, len)) {
			acpi_ssdt_parse (q, len);
			++*n;
		}
		unmapmem (q, len);
	}
	return NULL;
}
#endif

static void
acpi_init_paral (void)
{
#ifdef ACPI_DSDT
	int n = 0;

	acpi_dsdt_parse (dsdt_addr);
	foreach_entry_in_xsdt (call_ssdt_parse, &n);
	if (!n)
		foreach_entry_in_rsdt (call_ssdt_parse, &n);
#endif
}

static void
copy_rsdp (u64 rsdp, struct rsdpv2 *copyto)
{
	struct rsdpv2 *p;
	bool v2 = false;

	p = acpi_mapmem (rsdp, sizeof *p);
	if (p->v1.revision >= 2 && p->length >= sizeof *p) {
		p = acpi_mapmem (rsdp, p->length);
		if (!acpi_checksum (p, p->length))
			v2 = true;
	}
	copyto->length = 0;
	if (v2)
		memcpy (copyto, p, sizeof *copyto);
	else
		memcpy (&copyto->v1, &p->v1, sizeof copyto->v1);
}

static void
copy_rsdp1 (u64 rsdp, struct rsdp *copyto)
{
	struct rsdp *p;

	p = acpi_mapmem (rsdp, sizeof *p);
	memcpy (copyto, p, sizeof *copyto);
}

static void
remove_dup_facs_addr (u64 facs[], int n)
{
	int i, j;
	bool dumpaddr = true;

	for (i = 1; i < n; i++) {
		if (!facs[i])
			continue;
		for (j = 0; j < i; j++) {
			if (facs[i] == facs[j]) {
				facs[i] = 0;
				break;
			}
		}
	}
	if (dumpaddr) {
		printf ("FACS address");
		for (i = 0; i < n; i++)
			if (facs[i])
				printf (" 0x%llX", facs[i]);
		printf ("\n");
	}
}

static u32
modify_acpi_table_len (u64 address)
{
	struct acpi_description_header *p;
	u32 len;

	p = mapmem_hphys (address, sizeof *p, 0);
	len = p->length;
	unmapmem (p, sizeof *p);
	return len;
}

static u64
modify_acpi_table_do (u64 entry, char *signature, u64 address)
{
	struct acpi_description_header *p;

	p = mapmem_hphys (entry, sizeof *p, MAPMEM_WRITE);
	if (!memcmp (p->signature, signature, ACPI_SIGNATURE_LEN)) {
		if (address)
			entry = address;
		else
			memset (p->signature, 0, ACPI_SIGNATURE_LEN);
	}
	unmapmem (p, sizeof *p);
	return entry;
}

static void
modify_acpi_table_rsdt (struct rsdt *p, char *signature, u64 address)
{
	int i, n;

	if (!acpi_checksum (p, p->header.length)) {
		n = (p->header.length - sizeof p->header) / sizeof p->entry[0];
		for (i = 0; i < n; i++)
			p->entry[i] = modify_acpi_table_do (p->entry[i],
							    signature,
							    address);
		p->header.checksum -= acpi_checksum (p, p->header.length);
	}
}

static void
modify_acpi_table_xsdt (struct xsdt *p, char *signature, u64 address)
{
	int i, n;

	if (!acpi_checksum (p, p->header.length)) {
		n = (p->header.length - sizeof p->header) / sizeof p->entry[0];
		for (i = 0; i < n; i++)
			p->entry[i] = modify_acpi_table_do (p->entry[i],
							    signature,
							    address);
		p->header.checksum -= acpi_checksum (p, p->header.length);
	}
}

void *
acpi_find_entry (char *signature)
{
	return find_entry (signature);
}

void
acpi_itr_rsdt1_entry (void *(*func) (void *data, u64 entry), void *data)
{
	foreach_entry_in_rsdt1 (func, data);
}

void
acpi_itr_rsdt_entry (void *(*func) (void *data, u64 entry), void *data)
{
	foreach_entry_in_rsdt (func, data);
}

void
acpi_itr_xsdt_entry (void *(*func) (void *data, u64 entry), void *data)
{
	foreach_entry_in_xsdt (func, data);
}

void
acpi_modify_table (char *signature, u64 address)
{
	void *p;
	u32 len;

	if (uefi_booted) {
		call_uefi_boot_acpi_table_mod (signature, address);
	} else {
		len = !rsdp1_found ? 0 :
			modify_acpi_table_len (rsdp1_copy.rsdt_address);
		if (len) {
			p = mapmem_hphys (rsdp1_copy.rsdt_address, len,
					  MAPMEM_WRITE);
			modify_acpi_table_rsdt (p, signature, address);
			unmapmem (p, len);
		}
		len = !rsdp_found ? 0 :
			modify_acpi_table_len (rsdp_copy.v1.rsdt_address);
		if (len) {
			p = mapmem_hphys (rsdp_copy.v1.rsdt_address, len,
					  MAPMEM_WRITE);
			modify_acpi_table_rsdt (p, signature, address);
			unmapmem (p, len);
		}
		len = !rsdp_found || !rsdp_copy.length ? 0 :
			modify_acpi_table_len (rsdp_copy.xsdt_address);
		if (len) {
			p = mapmem_hphys (rsdp_copy.xsdt_address, len,
					  MAPMEM_WRITE);
			modify_acpi_table_xsdt (p, signature, address);
			unmapmem (p, len);
		}
	}
}

bool
acpi_dsdt_pm1_cnt_slp_typx_check (u32 v)
{
#ifdef ACPI_DSDT
	int i;
	bool m[6];
	u8 n;

	n = (v & PM1_CNT_SLP_TYPX_MASK) >> PM1_CNT_SLP_TYPX_SHIFT;
	for (i = 0; i <= 5; i++) {
	if (acpi_dsdt_system_state[i][0] &&
	    acpi_dsdt_system_state[i][1] == n)
		m[i] = true;
	else
		m[i] = false;
	}
	if (!m[2] && !m[3])
		return false;
#endif
	return true;
}

bool
acpi_get_waking_vector (u32 *old_waking_vector, bool *error)
{
	struct facs *facs;
	int i;
	u32 owv;
	bool err;

	if (!old_waking_vector || !error)
		return false;

	owv = 0;
	err = false;
	for (i = 0; i < NFACS_ADDR; i++) {
		if (!facs_addr[i])
			continue;
		facs = acpi_mapmem (facs_addr[i], sizeof *facs);
		if (IS_STRUCT_SIZE_OK (facs->length, facs,
				       facs->x_firmware_waking_vector))
		facs->x_firmware_waking_vector = 0;
		if (!IS_STRUCT_SIZE_OK (facs->length, facs,
					facs->firmware_waking_vector)) {
			printf ("FACS ERROR\n");
			err = true;
			break;
		}
		if (!facs->firmware_waking_vector)
			continue;
		if (!owv)
			owv = facs->firmware_waking_vector;
		else if (owv != facs->firmware_waking_vector)
			printf ("Multiple waking vector found\n");
	}

	if (!err)
		*old_waking_vector = owv;
	*error = err;

	return true;
}

void
acpi_set_waking_vector (u32 new_waking_vector, u32 old_waking_vector_ref)
{
	struct facs *facs;
	int i;

	for (i = 0; i < NFACS_ADDR; i++) {
		if (!facs_addr[i])
			continue;
		facs = acpi_mapmem (facs_addr[i], sizeof *facs);
		if (!old_waking_vector_ref || facs->firmware_waking_vector)
			facs->firmware_waking_vector = new_waking_vector;
	}
}

static void
acpi_init_global (void)
{
	u64 rsdp;
	u64 rsdp1;
	struct facp *q;

	rsdp_found = false;
	rsdp1_found = false;

	rsdp1 = find_rsdp1 ();
	if (rsdp1 != FIND_RSDP_NOT_FOUND) {
		copy_rsdp1 (rsdp1, &rsdp1_copy);
		rsdp1_found = true;
	}
	rsdp = find_rsdp ();
	if (rsdp == FIND_RSDP_NOT_FOUND) {
		printf ("ACPI RSDP not found.\n");
		return;
	}
	copy_rsdp (rsdp, &rsdp_copy);
	rsdp_found = true;

	call_initfunc ("acpi");

	q = find_facp ();
	if (!q) {
		printf ("ACPI FACP not found.\n");
		return;
	}
#ifdef ACPI_DSDT
	dsdt_addr = q->dsdt;
#endif
	get_pm1a_cnt_info (q);
	get_pm_tmr_info (q);
	ASSERT (NFACS_ADDR >= 0 + 2);
	get_facs_addr (&facs_addr[0], q);
	get_reset_info (q);
	get_smi_cmd_info (q);
	if (0)
		debug_dump (q, q->header.length);
	if (0)
		debug_dump_reg (&acpi_reg_pm1a_cnt);
	q = find_entry_in_rsdt (FACP_SIGNATURE);
	ASSERT (NFACS_ADDR >= 2 + 2);
	get_facs_addr (&facs_addr[2], q);
	q = find_entry_in_rsdt1 (FACP_SIGNATURE);
	ASSERT (NFACS_ADDR >= 4 + 2);
	get_facs_addr (&facs_addr[4], q);
	remove_dup_facs_addr (facs_addr, NFACS_ADDR);
	save_mcfg ();
}

INITFUNC ("global3", acpi_init_global);
INITFUNC ("paral30", acpi_init_paral);
