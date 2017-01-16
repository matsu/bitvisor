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

#include "acpi.h"
#include "acpi_dsdt.h"
#include "assert.h"
#include "beep.h"
#include "constants.h"
#include "current.h"
#include "initfunc.h"
#include "io_io.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "sleep.h"
#include "string.h"
#include "time.h"
#include "uefi.h"
#include "wakeup.h"
#include "passthrough/vtd.h"
#include "passthrough/iodom.h"

#define FIND_RSDP_NOT_FOUND	0xFFFFFFFFFFFFFFFFULL
#define RSDP_SIGNATURE		"RSD PTR"
#define RSDP_SIGNATURE_LEN	7
#define ADDRESS_SPACE_ID_MEM	0
#define ADDRESS_SPACE_ID_IO	1
#define SIGNATURE_LEN		4
#define RSDT_SIGNATURE		"RSDT"
#define XSDT_SIGNATURE		"XSDT"
#define FACP_SIGNATURE		"FACP"
#define FACS_SIGNATURE		"FACS"
#define MCFG_SIGNATURE		"MCFG"
#define DMAR_SIGNATURE		"DMAR"
#define SSDT_SIGNATURE		"SSDT"
#define PM1_CNT_SLP_TYPX_MASK	0x1C00
#define PM1_CNT_SLP_TYPX_SHIFT	10
#define PM1_CNT_SLP_EN_BIT	0x2000
#define IS_STRUCT_SIZE_OK(l, h, m) \
	((l) >= ((u8 *)&(m) - (u8 *)(h)) + sizeof (m))
#define ACCESS_SIZE_UNDEFINED	0
#define ACCESS_SIZE_BYTE	1
#define ACCESS_SIZE_WORD	2
#define ACCESS_SIZE_DWORD	3
#define ACCESS_SIZE_QWORD	4

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

struct description_header {
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

struct rsdt {
	struct description_header header;
	u32 entry[];
} __attribute__ ((packed));

struct xsdt {
	struct description_header header;
	u64 entry[];
} __attribute__ ((packed));

struct gas {
	u8 address_space_id;
	u8 register_bit_width;
	u8 register_bit_offset;
	u8 access_size;
	u64 address;
} __attribute__ ((packed));

struct facp {
	struct description_header header;
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
	struct description_header header;
	u8 reserved[8];
	struct {
		u64 base;	/* 0 */
		u16 seg_group;	/* 8 */
		u8 bus_start;	/* 10 */
		u8 bus_end;	/* 11 */
		u8 reserved[4];	/* 12 */
	} __attribute__ ((packed)) configs[1];
} __attribute__ ((packed));

static bool rsdp_found;
static struct rsdpv2 rsdp_copy;
static bool pm1a_cnt_found;
static u32 pm1a_cnt_ioaddr;
static u32 pm_tmr_ioaddr;
static u64 facs_addr[4];
static u32 smi_cmd;
static struct gas reset_reg;
static u8 reset_value;
#ifdef ACPI_DSDT
static u32 dsdt_addr;
#endif
static struct mcfg *saved_mcfg;

static u8
acpi_checksum (void *p, int len)
{
	u8 *q, s;

	s = 0;
	for (q = p; len > 0; len--)
		s += *q++;
	return s;
}

static void *
acpi_mapmem (u64 addr, int len)
{
	static void *oldmap;
	static int oldlen = 0;

	if (oldlen)
		unmapmem (oldmap, oldlen);
	oldlen = len;
	oldmap = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, addr, len);
	ASSERT (oldmap);
	return oldmap;
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

static void *
foreach_entry_in_rsdt (void *(*func) (void *data, u64 entry), void *data)
{
	struct rsdt *p;
	void *ret = NULL;
	int i, n, len = 0;

	if (!rsdp_found)
		return NULL;
	p = mapmem_hphys (rsdp_copy.v1.rsdt_address, sizeof *p, 0);
	if (!memcmp (p->header.signature, RSDT_SIGNATURE, SIGNATURE_LEN))
		len = p->header.length;
	unmapmem (p, sizeof *p);
	if (len < sizeof *p)
		return NULL;
	p = mapmem_hphys (rsdp_copy.v1.rsdt_address, len, 0);
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
	if (!memcmp (p->header.signature, XSDT_SIGNATURE, SIGNATURE_LEN))
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
	struct description_header *q;
	char *signature = data;

	q = acpi_mapmem (entry, sizeof *q);
	if (memcmp (q->signature, signature, SIGNATURE_LEN))
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

static bool
acpi_pm1_sleep (u32 v)
{
	struct facs *facs;
	u32 new_waking_vector;
	u32 old_waking_vector;
	int i;
#ifdef ACPI_DSDT
	bool m[6];
	u8 n;
#endif

#ifdef ACPI_DSDT
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
	old_waking_vector = 0;
	for (i = 0; i < 4; i++) {
		if (!facs_addr[i])
			continue;
		facs = acpi_mapmem (facs_addr[i], sizeof *facs);
		if (IS_STRUCT_SIZE_OK (facs->length, facs,
				       facs->x_firmware_waking_vector))
			facs->x_firmware_waking_vector = 0;
		if (!IS_STRUCT_SIZE_OK (facs->length, facs,
					facs->firmware_waking_vector)) {
			printf ("FACS ERROR\n");
			return true;
		}
		if (!facs->firmware_waking_vector)
			continue;
		if (!old_waking_vector)
			old_waking_vector = facs->firmware_waking_vector;
		else if (old_waking_vector != facs->firmware_waking_vector)
			printf ("Multiple waking vector found\n");
	}
	new_waking_vector = prepare_for_sleep (old_waking_vector);
	for (i = 0; i < 4; i++) {
		if (!facs_addr[i])
			continue;
		facs = acpi_mapmem (facs_addr[i], sizeof *facs);
		if (!old_waking_vector || facs->firmware_waking_vector)
			facs->firmware_waking_vector = new_waking_vector;
	}
	get_cpu_time ();	/* update lastcputime */
	/* Flush all write back caches including the internal caches
	   on the other processors, or the processors will lose them
	   and the VMM will not work correctly. */
	mm_flush_wb_cache ();
	asm_outl (pm1a_cnt_ioaddr, v);
	cancel_sleep ();
	return true;
}

static enum ioact
acpi_io_monitor (enum iotype type, u32 port, void *data)
{
	u32 v;

	if (pm1a_cnt_found && port == pm1a_cnt_ioaddr) {
		switch (type) {
		case IOTYPE_OUTB:
			v = *(u8 *)data;
			break;
		case IOTYPE_OUTW:
			v = *(u16 *)data;
			break;
		case IOTYPE_OUTL:
			v = *(u32 *)data;
			break;
		default:
			goto def;
		}
		if (v & PM1_CNT_SLP_EN_BIT)
			if (acpi_pm1_sleep (v))
				return IOACT_CONT;
		goto def;
	}
def:
	return do_io_default (type, port, data);
}

static enum ioact
acpi_smi_monitor (enum iotype type, u32 port, void *data)
{
	if (current->acpi.smi_hook_disabled)
		panic ("SMI monitor called while SMI hook is disabled");
	current->vmctl.paging_map_1mb ();
	current->vmctl.iopass (smi_cmd, true);
	current->acpi.smi_hook_disabled = true;
	return IOACT_RERUN;
}

void
acpi_smi_hook (void)
{
	if (!current->vcpu0->acpi.iopass)
		return;
	if (current->acpi.smi_hook_disabled) {
		current->vmctl.iopass (smi_cmd, false);
		current->acpi.smi_hook_disabled = false;
	}
}

void
acpi_iohook (void)
{
	if (pm1a_cnt_found)
		set_iofunc (pm1a_cnt_ioaddr, acpi_io_monitor);
	if (smi_cmd > 0 && smi_cmd <= 0xFFFF) {
		current->vcpu0->acpi.iopass = true;
		set_iofunc (smi_cmd, acpi_smi_monitor);
	}
}

static void
get_pm1a_cnt_ioaddr (struct facp *q)
{
	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->x_pm1a_cnt_blk)) {
		if (q->x_pm1a_cnt_blk.address_space_id !=
		    ADDRESS_SPACE_ID_IO)
			panic ("X_PM1a_CNT_BLK is not I/O address");
		pm1a_cnt_ioaddr = q->x_pm1a_cnt_blk.address;
	} else if (IS_STRUCT_SIZE_OK (q->header.length, q, q->pm1a_cnt_blk)) {
		pm1a_cnt_ioaddr = q->pm1a_cnt_blk;
	} else {
		panic ("ACPI FACP is too short");
	}
	if (pm1a_cnt_ioaddr > 0xFFFF)
		panic ("PM1a control port > 0xFFFF");
}

static void
get_pm_tmr_ioaddr (struct facp *q)
{
	if (IS_STRUCT_SIZE_OK (q->header.length, q, q->x_pm_tmr_blk) &&
	    q->x_pm_tmr_blk.address_space_id == ADDRESS_SPACE_ID_IO &&
	    q->x_pm_tmr_blk.address <= 0xFFFF) {
		pm_tmr_ioaddr = q->x_pm_tmr_blk.address;
	} else if (IS_STRUCT_SIZE_OK (q->header.length, q, q->pm_tmr_blk)) {
		pm_tmr_ioaddr = q->pm_tmr_blk;
	} else {
		pm_tmr_ioaddr = 0;
	}
	if (pm_tmr_ioaddr > 0xFFFF)
		pm_tmr_ioaddr = 0;
}

static void
get_facs_addr (u64 facs[2], struct facp *facp)
{
	facs[0] = 0;
	facs[1] = 0;
	if (IS_STRUCT_SIZE_OK (facp->header.length, facp,
			       facp->x_firmware_ctrl))
		facs[0] = facp->x_firmware_ctrl;
	if (IS_STRUCT_SIZE_OK (facp->header.length, facp,
			       facp->firmware_ctrl))
		facs[1] = facp->firmware_ctrl;
	else
		panic ("ACPI FACP is too short");
}

static void
get_reset_info (struct facp *facp)
{
	if (IS_STRUCT_SIZE_OK (facp->header.length, facp, facp->reset_reg) &&
	    IS_STRUCT_SIZE_OK (facp->header.length, facp, facp->reset_value)) {
		reset_reg = facp->reset_reg;
		reset_value = facp->reset_value;
	}
}

static bool
gas_write (struct gas *addr, u64 value)
{
	void *p;
	int len;

	switch (addr->address_space_id) {
	case ADDRESS_SPACE_ID_MEM:
		switch (addr->access_size) {
		case ACCESS_SIZE_UNDEFINED:
		case ACCESS_SIZE_BYTE:
			len = 1;
			break;
		case ACCESS_SIZE_WORD:
			len = 2;
			break;
		case ACCESS_SIZE_DWORD:
			len = 4;
			break;
		case ACCESS_SIZE_QWORD:
			len = 8;
			break;
		default:
			return false;
		}
		p = mapmem_hphys (addr->address, len, MAPMEM_WRITE |
				  MAPMEM_PCD | MAPMEM_PWT);
		memcpy (p, &value, len);
		unmapmem (p, len);
		return true;
	case ADDRESS_SPACE_ID_IO:
		switch (addr->access_size) {
		case ACCESS_SIZE_UNDEFINED:
		case ACCESS_SIZE_BYTE:
			asm_outb (addr->address, value);
			break;
		case ACCESS_SIZE_WORD:
			asm_outw (addr->address, value);
			break;
		case ACCESS_SIZE_DWORD:
			asm_outl (addr->address, value);
			break;
		case ACCESS_SIZE_QWORD:
		default:
			return false;
		}
		return true;
	default:
		return false;
	}
}

void
acpi_reset (void)
{
	if (reset_reg.register_bit_width != 8) /* width must be 8 */
		return;
	if (reset_reg.register_bit_offset != 0) /* offset must be 0 */
		return;
	/* The address space ID must be system memory, system I/O or
	 * PCI configuration space. PCI configuration space support is
	 * not yet implemented. */
	switch (reset_reg.address_space_id) {
	case ADDRESS_SPACE_ID_MEM:
	case ADDRESS_SPACE_ID_IO:
		gas_write (&reset_reg, reset_value);
		break;
	default:
		break;
	}
}

void
acpi_poweroff (void)
{
	u32 data, typx;

	if (!pm1a_cnt_found)
		return;
	if (!acpi_dsdt_system_state[5][0])
		return;
	typx = acpi_dsdt_system_state[5][1] << PM1_CNT_SLP_TYPX_SHIFT;
	/* FIXME: how to handle pm1b_cnt? */
	asm_inl (pm1a_cnt_ioaddr, &data);
	data &= ~PM1_CNT_SLP_TYPX_MASK;
	data |= typx & PM1_CNT_SLP_TYPX_MASK;
	data |= PM1_CNT_SLP_EN_BIT;
	asm_outl (pm1a_cnt_ioaddr, data);
}

bool
get_acpi_time_raw (u32 *r)
{
	u32 tmp;

	if (pm_tmr_ioaddr) {
		asm_inl (pm_tmr_ioaddr, &tmp);
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
	struct description_header *p;
	int *n = data;
	u32 len = 0;
	u8 *q;

	p = mapmem_hphys (entry, sizeof *p, 0);
	if (!memcmp (p->signature, SSDT_SIGNATURE, SIGNATURE_LEN))
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

static void
disable_vtd (void *dmar)
{
#ifdef DISABLE_VTD
	struct description_header *header;
	struct {
		u16 type;
		u16 length;
		u8 flags;
		u8 reserved;
		u16 segment_number;
		u64 register_base_address;
	} __attribute__ ((packed)) *q;
	struct {
		u32 version_register;
		u32 reserved;
		u64 capability_register;
		u64 extended_capability_register;
		u32 global_command_register;
		u32 global_status_register;
	} __attribute__ ((packed)) *r;
	u32 offset;

	header = dmar;
	offset = 0x30;		/* Remapping structures */
	while (offset + sizeof *q <= header->length) {
		q = dmar + offset;
		if (!q->type) {	/* DRHD */
			r = mapmem_hphys (q->register_base_address, sizeof *r,
					  MAPMEM_WRITE);
			if (r->global_status_register & 0x80000000) {
				printf ("DMAR: [0x%llX] Disable translation\n",
					q->register_base_address);
				r->global_command_register = 0;
			}
			unmapmem (r, sizeof *r);
		}
		offset += q->length;
	}
#endif
}

static void
acpi_init_global (void)
{
	u64 rsdp;
	struct facp *q;
	struct acpi_ent_dmar *r;
	struct domain *create_dom() ;

	wakeup_init ();
	rsdp_found = false;
	pm1a_cnt_found = false;

	rsdp = find_rsdp ();
	if (rsdp == FIND_RSDP_NOT_FOUND) {
		printf ("ACPI RSDP not found.\n");
		return;
	}
	copy_rsdp (rsdp, &rsdp_copy);
	rsdp_found = true;

	r=find_entry(DMAR_SIGNATURE);
	if (!r) {
		printf ("ACPI DMAR not found.\n");
		iommu_detected=0;
	} else {
		int i ;
		printf ("ACPI DMAR found.\n");
		iommu_detected=1;
		
		parse_dmar_bios_report(r) ;
		num_dom=0 ;
		for (i=0 ; i<MAX_IO_DOM ; i++)
			dom_io[i]=create_dom(i) ;
		memset (r, 0, 4); /* Clear DMAR to prevent guest OS from
				     using iommu */
		disable_vtd (r);
	}

	q = find_facp ();
	if (!q) {
		printf ("ACPI FACP not found.\n");
		return;
	}
#ifdef ACPI_DSDT
	dsdt_addr = q->dsdt;
#endif
	get_pm1a_cnt_ioaddr (q);
	get_pm_tmr_ioaddr (q);
	get_facs_addr (&facs_addr[0], q);
	get_reset_info (q);
	smi_cmd = q->smi_cmd;
	if (0)
		debug_dump (q, q->header.length);
	if (0)
		printf ("PM1a control port is 0x%X\n", pm1a_cnt_ioaddr);
	q = find_entry_in_rsdt (FACP_SIGNATURE);
	if (q) {
		get_facs_addr (&facs_addr[2], q);
		remove_dup_facs_addr (facs_addr, 4);
	} else {
		remove_dup_facs_addr (facs_addr, 2);
	}
	pm1a_cnt_found = true;
	save_mcfg ();
}

INITFUNC ("global3", acpi_init_global);
INITFUNC ("paral30", acpi_init_paral);
