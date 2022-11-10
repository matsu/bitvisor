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
#include "ap.h"
#include "assert.h"
#include "beep.h"
#include "calluefi.h"
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
#define NFACS_ADDR		6
#define NSLPTCACHE		7
#define DMAR_GLOBAL_STATUS_TES_BIT (1 << 31)
#define DMAR_GLOBAL_STATUS_IRES_BIT (1 << 25)
#define CAP_REG_SLLPS_21_BIT	0x400000000ULL
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

struct remapping_structures_header {
	u16 type;
	u16 length;
} __attribute__ ((packed));

struct dmar_rmrr {
	struct remapping_structures_header header;
	u16 reserved;
	u16 segment_number;
	u64 region_base_address;
	u64 region_limit_address;
} __attribute__ ((packed));

struct dmar_device_scope_pci {
	u8 type;
	u8 length;
	u16 reserved;
	u8 enumeration_id;
	u8 start_bus_number;
	struct {
		u8 device_number;
		u8 function_number;
	} __attribute__ ((packed)) pci_path[1];
} __attribute__ ((packed));

struct dmar_drhd {
	struct remapping_structures_header header;
	u8 flags;
	u8 reserved;
	u16 segment_number;
	u64 register_base_address;
} __attribute__ ((packed));

struct dmar_drhd_reg {
	u32 version_register;
	u32 reserved;
	u64 capability_register;
	u64 extended_capability_register;
	u32 global_command_register;
	u32 global_status_register;
	u64 root_table_address_register;
	u32 unused[(0xB8 - 0x28) / sizeof (u32)];
	u64 interrupt_remapping_table_address_register;
} __attribute__ ((packed));

struct drhd_devlist {
	struct drhd_devlist *next;
	u8 type;
	u8 bus;
	u8 pathlen;
	u8 path[];
} __attribute__ ((packed));

struct dmar_drhd_reg_data {
	void *mmio;
	struct dmar_drhd_reg *reg;
	u64 base;
	u16 segment;
	struct drhd_devlist *devlist;
	spinlock_t lock;
	bool cached;
	struct {
		struct {
			u64 entry[512];
		} *slpt[NSLPTCACHE];
		struct {
			struct {
				u64 low;
				u64 high;
			} entry[256];
		} *root_table;
		struct {
			struct {
				u64 low;
				u64 high;
			} entry[32][8];
		} *context_table;
		struct {
			struct {
				u64 low;
				u64 high;
			} entry[65536];
		} *intr_remap_table;
		u64 capability;
		u64 global_status;
		u64 root_table_addr_reg;
		u64 intr_remap_table_addr_reg;
		u64 slptptr[NSLPTCACHE];
		u64 root_table_addr;
		u64 context_table_ptr;
		u64 intr_remap_table_addr;
		u32 intr_remap_table_len;
	} cache;
};

struct dmar_info {
	u64 table;
	u32 length;
	bool valid;
};

struct dmar_pass {
	struct dmar_info rsdt1_dmar;
	struct dmar_info rsdt_dmar;
	struct dmar_info xsdt_dmar;
	u64 new_dmar_addr;
	void *new_dmar;
	void *new_rmrr;
	unsigned int npages;
	unsigned int drhd_num;
	u8 host_address_width;
	struct dmar_drhd_reg_data *reg;
};

static bool rsdp_found;
static struct rsdpv2 rsdp_copy;
static bool rsdp1_found;
static struct rsdp rsdp1_copy;
static bool pm1a_cnt_found;
static u32 pm1a_cnt_ioaddr;
static u32 pm_tmr_ioaddr;
static u64 facs_addr[NFACS_ADDR];
static u32 smi_cmd;
static struct gas reset_reg;
static u8 reset_value;
#ifdef ACPI_DSDT
static u32 dsdt_addr;
#endif
static struct mcfg *saved_mcfg;
static struct dmar_pass vp;

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
	oldmap = mapmem_hphys (addr, len, MAPMEM_WRITE);
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
	if (!memcmp (p->header.signature, RSDT_SIGNATURE, SIGNATURE_LEN))
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
	for (i = 0; i < NFACS_ADDR; i++) {
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
	if (!facp)
		return;
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
	if (IS_STRUCT_SIZE_OK (facp->header.length, facp, facp->flags) &&
	    (facp->flags & FACP_FLAGS_RESET_REG_SUP_BIT) &&
	    IS_STRUCT_SIZE_OK (facp->header.length, facp, facp->reset_reg) &&
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

#ifdef DISABLE_VTD
/* Return true if there is an interrupt remapping table entry which is
 * present.  For example this is true on recent (2018) Mac machines;
 * apparently Mac machines until 2017 use PIC and PIT for timer
 * services in firmware but recent Mac machines use HPET and interrupt
 * remapping. */
static bool
is_interrupt_remapping_used (u64 irta_reg)
{
	static const u64 irte_p_bit = 1 << 0;
	u32 nentries = 2 << (irta_reg & 0xF);
	u64 irta = irta_reg & ~PAGESIZE_MASK;
	u64 *irt;
	u32 len = nentries * 2 * sizeof *irt;
	u32 i;
	bool ret = false;

	irt = mapmem_hphys (irta, len, 0);
	for (i = 0; i < nentries; i++) {
		if (irt[2 * i] & irte_p_bit) {
			ret = true;
			break;
		}
	}
	unmapmem (irt, len);
	return ret;
}

static void
disable_vtd_sub (u64 regbase)
{
	struct dmar_drhd_reg *r;
	static const u32 interrupt_remapping_bit = DMAR_GLOBAL_STATUS_IRES_BIT;
	static const u32 translation_bit = DMAR_GLOBAL_STATUS_TES_BIT;
	bool interrupt_remapping_used = false;
	u32 status;
	void *event;
	static u64 clear_u32_uefi;
	u8 *p;

	r = mapmem_hphys (regbase, sizeof *r, MAPMEM_WRITE);
	status = r->global_status_register;
	if (status & interrupt_remapping_bit) {
		u64 irta_reg = r->interrupt_remapping_table_address_register;
		if (is_interrupt_remapping_used (irta_reg)) {
			printf ("DMAR: [0x%llX] Interrupt remapping will be"
				" disabled at ExitBootServices()\n", regbase);
#ifdef VTD_TRANS
			panic ("%s: Conflict with VTD_TRANS", __func__);
#endif
			interrupt_remapping_used = true;
		} else {
			printf ("DMAR: [0x%llX] Disable interrupt remapping\n",
				regbase);
		}
	}
	if (status & translation_bit)
		printf ("DMAR: [0x%llX] Disable translation\n", regbase);
	if (interrupt_remapping_used) {
		r->global_command_register = status & ~translation_bit;
		if (!uefi_booted)
			panic ("%s: Interrupt remapping has been enabled"
			       " by BIOS?", __func__);
		if (call_uefi_allocate_pages (0, /* AllocateAnyPages */
					      3, /* EfiBootServicesCode */
					      1, &clear_u32_uefi))
			panic ("%s: AllocatePages failed", __func__);
		p = mapmem_hphys (clear_u32_uefi, 5, MAPMEM_WRITE);
		p[0] = 0x31;	/* xor %eax,%eax */
		p[1] = 0xC0;
		p[2] = 0x89;	/* mov %eax,(%rdx) */
		p[3] = 0x02;
		p[4] = 0xC3;	/* ret */
		unmapmem (p, 5);
		if (call_uefi_create_event_exit_boot_services
		    (clear_u32_uefi, regbase +
		     ((ulong)&r->global_command_register - (ulong)r), &event))
			panic ("%s: CreateEvent failed", __func__);
	} else {
		r->global_command_register = 0;
	}
	unmapmem (r, sizeof *r);
}

static bool
disable_vtd_drhd (void *data, void *entry)
{
	struct dmar_drhd *q = entry;

	if (q->header.length >= sizeof *q && !q->header.type) /* DRHD */
		disable_vtd_sub (q->register_base_address);
	return false;
}
#endif

static void *
foreach_entry_in_dmar (void *dmar, bool (*func) (void *data, void *entry),
		       void *data)
{
	struct description_header *header;
	struct remapping_structures_header *p;
	u32 offset;

	header = dmar;
	offset = 0x30;		/* Remapping structures */
	while (offset + sizeof *p <= header->length) {
		p = dmar + offset;
		if (offset + p->length > header->length)
			break;
		if (func (data, p))
			break;
		offset += p->length;
	}
	return dmar + offset;
}

/* Insert length bytes to where */
static void
dmar_pass_insert (void *where, unsigned int length, u8 *end)
{
	u8 *dst = where + length;
	u8 *src = where;
	unsigned int len = end - src;

	while (len--)
		dst[len] = src[len];
}

static bool
find_end_of_rmrr (void *data, void *entry)
{
	struct remapping_structures_header *p = entry;

	if (p->type > 1)	/* 1(RMRR) */
		return true;
	return false;
}

static void *
dmar_pass_create_rmrr (u16 segment)
{
	struct description_header *header;
	struct dmar_rmrr *q;

	header = vp.new_dmar;
	/* The remapping structures must be in numerical order.  So
	 * skip the DRHD and RMRR part. */
	q = foreach_entry_in_dmar (header, find_end_of_rmrr, NULL);
	/* Move the remaining part and create a new RMRR structure. */
	if (header->length + sizeof *q > vp.npages * PAGESIZE)
		panic ("%s: No space left", __func__);
	dmar_pass_insert (q, sizeof *q, vp.new_dmar + header->length);
	header->length += sizeof *q;
	q->header.type = 1;	/* RMRR */
	q->header.length = sizeof *q;
	q->reserved = 0;
	q->segment_number = segment;
	q->region_base_address = vmm_start_inf ();
	q->region_limit_address = vmm_term_inf () - 1;
	return q;
}

static struct dmar_drhd_reg_data *
dmar_pass_find_reg (u16 segment, const struct acpi_pci_addr *addr)
{
	unsigned int i, j;
	struct drhd_devlist *devlist;
	const struct acpi_pci_addr *a;

	for (i = 0; i < vp.drhd_num; i++) {
		if (vp.reg[i].segment != segment)
			continue;
		for (devlist = vp.reg[i].devlist; devlist;
		     devlist = devlist->next) {
			/* Check for the INCLUDE_PCI_ALL case. */
			if (!devlist->type)
				return &vp.reg[i];
			/* No path is not allowed. */
			if (devlist->pathlen < 2)
				goto next;
			/* Find the bus number in the addr list. */
			for (a = addr; a; a = a->next)
				if (devlist->bus == a->bus)
					goto found;
		next:
			continue;
		found:
			/* Compare the {Device, Function} pairs. */
			for (j = 0; j + 1 < devlist->pathlen; j += 2) {
				/* (!a): the path is longer than the
				   addr. */
				if (!a || devlist->path[j] != a->dev ||
				    devlist->path[j + 1] != a->func)
					goto next;
				a = a->next;
			}
			/* (!a): the addr and the path are matched. */
			/* (a): the addr is longer than the path i.e.
			 * downstream of the path. */
			/* (devlist->type == 1): PCI Endpoint Device.
			 * The addr and the path must be matched. */
			/* (devlist->type == 2): PCI Sub-hierarchy.
			 * The addr can be one of its downstream
			 * devices. */
			if (a && devlist->type == 1) /* PCI Endpoint Device */
				goto next;
			return &vp.reg[i];
		}
	}
	return NULL;
}

static void
dmar_add_pci_device (u16 segment, u8 bus, u8 dev, u8 func, bool bridge)
{
	struct description_header *header;
	struct dmar_rmrr *p;
	struct dmar_device_scope_pci *q;

	if (segment)
		return;
	if (!vp.new_rmrr)
		vp.new_rmrr = dmar_pass_create_rmrr (segment);
	header = vp.new_dmar;
	p = vp.new_rmrr;
	/* Find insertion point to avoid overlap and sort entries. */
	for (q = vp.new_rmrr + sizeof *p; q != vp.new_rmrr + p->header.length;
	     q++) {
		if (q->start_bus_number > bus)
			break;
		if (q->start_bus_number < bus)
			continue;
		if (q->pci_path[0].device_number > dev)
			break;
		if (q->pci_path[0].device_number == dev) {
			if (q->pci_path[0].function_number > func)
				break;
			if (q->pci_path[0].function_number == func)
				return;
		}
	}
	/* Move the remaining part and create a new device scope. */
	if (header->length + sizeof *q > vp.npages * PAGESIZE)
		panic ("%s: No space left", __func__);
	dmar_pass_insert (q, sizeof *q, vp.new_dmar + header->length);
	header->length += sizeof *q;
	p->header.length += sizeof *q;
	if (bridge)
		q->type = 2;	/* PCI Sub-hierarchy */
	else
		q->type = 1;	/* PCI Endpoint Device */
	q->length = sizeof *q;
	q->reserved = 0;
	q->enumeration_id = 0;
	q->start_bus_number = bus;
	q->pci_path[0].device_number = dev;
	q->pci_path[0].function_number = func;
	header->checksum -= acpi_checksum (header, header->length);
}

static void *
dmar_replace_map (void *oldmap, u64 addr, uint len)
{
	if (oldmap)
		unmapmem (oldmap, len);
	return mapmem_hphys (addr, len, 0);
}

/* Lookup the translation table and return a translated address as a
 * page table entry.  PTE_RW_BIT is set for every page because some
 * para pass-through drivers use MAPMEM_WRITE flag for read-only
 * buffers.  Drivers should not write read-only pages and should not
 * read write-only pages. */
static u64
dmar_lookup (struct dmar_drhd_reg_data *d, u64 address, u64 ptr,
	     unsigned int bit, unsigned int cache_index)
{
	ASSERT (cache_index < NSLPTCACHE);
	u64 mask = ~(~0ULL << vp.host_address_width) & ~PAGESIZE_MASK;
	if (d->cache.slptptr[cache_index] != ptr) {
		d->cache.slpt[cache_index] =
			dmar_replace_map (d->cache.slpt[cache_index], ptr,
					  sizeof *d->cache.slpt[cache_index]);
		d->cache.slptptr[cache_index] = ptr;
	}
	u64 entry = d->cache.slpt[cache_index]->entry[address >> bit & 0777];
	if (!(entry & 3)) /* ReadWrite = 0 */
		return 0;
	if (bit <= 30 && bit > 12 && (entry & 0x80)) /* Page Size = 1 */
		return ((entry & mask) >> bit << bit) |
			(address & ((1 << bit) - PAGESIZE)) | PTE_P_BIT |
			PTE_US_BIT | PTE_RW_BIT;
	if (bit <= 12)
		return (entry & mask) | PTE_P_BIT | PTE_US_BIT | PTE_RW_BIT;
	return dmar_lookup (d, address, entry & mask, bit - 9,
			    cache_index + 1);
}

static void
acpi_dmar_cachereg (struct dmar_drhd_reg_data *d)
{
	if (!d->cached) {
		d->cache.capability = d->reg->capability_register;
		d->cache.global_status = d->reg->global_status_register;
		if (d->cache.global_status &
		    DMAR_GLOBAL_STATUS_TES_BIT)
			d->cache.root_table_addr_reg =
				d->reg->root_table_address_register;
		if (d->cache.global_status &
		    DMAR_GLOBAL_STATUS_IRES_BIT)
			d->cache.intr_remap_table_addr_reg =
				d->reg->
				interrupt_remapping_table_address_register;
		d->cached = true;
	}
}

static u64
do_acpi_dmar_translate (struct dmar_drhd_reg_data *d, u8 bus, u8 dev, u8 func,
			u64 address,
			u64 (*lookup) (struct dmar_drhd_reg_data *d,
				       u64 address, u64 ptr, unsigned int bit,
				       unsigned int cache_index))
{
	u64 mask = ~(~0ULL << vp.host_address_width) & ~PAGESIZE_MASK;
	u64 ret = 0;
	spinlock_lock (&d->lock);
	acpi_dmar_cachereg (d);
	if (!(d->cache.global_status & DMAR_GLOBAL_STATUS_TES_BIT)) {
		/* If DMA remapping is not enabled, return the
		 * address. */
	passthrough:
		ret = (address & ~PAGESIZE_MASK) | PTE_P_BIT | PTE_RW_BIT |
			PTE_US_BIT;
		goto end;
	}
	if (d->cache.root_table_addr_reg & (1 << 11)) {
		d->cache.global_status = 0;
		spinlock_unlock (&d->lock);
		panic ("%s: Root Table Type is Extended Root Table!",
		       __func__);
	}
	u64 rta = d->cache.root_table_addr_reg & mask; /* Root Table
							* Address */
	if (d->cache.root_table_addr != rta) {
		d->cache.root_table =
			dmar_replace_map (d->cache.root_table, rta,
					  sizeof *d->cache.root_table);
		d->cache.root_table_addr = rta;
	}
	/* Lookup translation tables. */
	u64 root_entry_low = d->cache.root_table->entry[bus].low;
	if (!(root_entry_low & 1)) /* Present = 0 */
		goto end;
	u64 ctp = root_entry_low & mask; /* Context-table Pointer */
	if (d->cache.context_table_ptr != ctp) {
		d->cache.context_table =
			dmar_replace_map (d->cache.context_table, ctp,
					  sizeof *d->cache.context_table);
		d->cache.context_table_ptr = ctp;
	}
	u64 context_entry_low = d->cache.context_table->entry[dev][func].low;
	u64 context_entry_high = d->cache.context_table->entry[dev][func].high;
	if (!(context_entry_low & 1)) /* Present = 0 */
		goto end;
	if ((context_entry_low & 0xc) == 0x8) /* Translation Type is
					       * pass-through */
		goto passthrough;
	u8 aw = context_entry_high & 7; /* Address Width */
	if (aw > 4)
		goto end;
	u64 slptptr = context_entry_low & mask; /* Second Level Page
						 * Translation
						 * Pointer */
	unsigned int bits = 30 + 9 * aw;
	ret = lookup (d, address, slptptr, bits - 9, 0);
end:
	spinlock_unlock (&d->lock);
	return ret;
}

u64
acpi_dmar_translate (struct dmar_drhd_reg_data *d, u8 bus, u8 dev, u8 func,
		     u64 address)
{
	return do_acpi_dmar_translate (d, bus, dev, func, address,
				       dmar_lookup);
}

u64
acpi_dmar_msi_to_icr (struct dmar_drhd_reg_data *d, u32 maddr, u32 mupper,
		      u16 mdata)
{
	if (mupper)
		goto no_remap;	/* Not a interrupt request */
	if ((maddr & ~0xFFFFF) != 0xFEE00000)
		goto no_remap;	/* Not a interrupt request */
	if (!(maddr & (1 << 4)))
		goto no_remap;	/* Not remappable format */
	u64 mask = ~(~0ULL << vp.host_address_width) & ~PAGESIZE_MASK;
	spinlock_lock (&d->lock);
	acpi_dmar_cachereg (d);
	if (!(d->cache.global_status & DMAR_GLOBAL_STATUS_IRES_BIT)) {
		/* Interrupt-remapping is not enabled. */
		goto unmap_no_remap;
	}
	u64 irta = d->cache.intr_remap_table_addr_reg & mask; /* Interrupt
							       * Remapping
							       * Table
							       * Address */
	u32 size = d->cache.intr_remap_table_addr_reg & 0xF;
	u32 nentries = 2 << size;
	u32 len = nentries * 16;
	if (d->cache.intr_remap_table_addr != irta ||
	    d->cache.intr_remap_table_len != len) {
		/* The dmar_replace_map() function cannot be used
		 * directly for the pointer replacement because the
		 * size is not fixed.  The unmapmem() function needs
		 * correct length. */
		if (d->cache.intr_remap_table_len)
			unmapmem (d->cache.intr_remap_table,
				  d->cache.intr_remap_table_len);
		d->cache.intr_remap_table = dmar_replace_map (NULL, irta, len);
		d->cache.intr_remap_table_addr = irta;
		d->cache.intr_remap_table_len = len;
	}
	u16 handle = ((maddr >> 5) & 077777) | (maddr >> 2 << 15);
	u16 shv = (maddr >> 3) & 1;
	u16 subhandle = shv ? mdata : 0;
	u32 interrupt_index = handle + subhandle;
	u64 entry_low = 0;
	if (interrupt_index < nentries)
		entry_low =
			d->cache.intr_remap_table->entry[interrupt_index].low;
	spinlock_unlock (&d->lock);
	return dmar_irte_to_icr (entry_low);
unmap_no_remap:
	spinlock_unlock (&d->lock);
no_remap:
	return msi_to_icr (maddr, mupper, mdata);
}

struct dmar_drhd_reg_data *
acpi_dmar_add_pci_device (u16 segment, const struct acpi_pci_addr *addr,
			  bool bridge)
{
	if (!vp.npages || !addr)
		return NULL;
	if (addr->next)
		bridge = true;
	if (vp.new_dmar && addr->bus >= 0)
		dmar_add_pci_device (segment, addr->bus, addr->dev, addr->func,
				     bridge);
	return dmar_pass_find_reg (segment, addr);
}

static void
dmar_pass_free_devlist (struct drhd_devlist *q)
{
	if (q->next)
		dmar_pass_free_devlist (q->next);
	free (q);
}

void
acpi_dmar_done_pci_device (void)
{
	unsigned int i;

	if (!vp.new_dmar)
		return;
	if (!vp.new_rmrr) {
		/* No PCI devices found. */
		for (i = 0; i < vp.drhd_num; i++) {
			dmar_pass_free_devlist (vp.reg[i].devlist);
			mmio_unregister (vp.reg[i].mmio);
			unmapmem (vp.reg[i].reg, sizeof *vp.reg[i].reg);
		}
		free (vp.reg);
		vp.reg = NULL;
		vp.drhd_num = 0;
	}
	/* Unmap the new_dmar to stop updating the DMAR. */
	unmapmem (vp.new_dmar, vp.npages * PAGESIZE);
	vp.new_dmar = NULL;
}

static u64
dmar_force_map (struct dmar_drhd_reg_data *d, u64 address, u64 ptr,
		unsigned int bit, unsigned int cache_index)
{
	u64 ret;
	u64 mask = ~(~0ULL << vp.host_address_width) & ~PAGESIZE_MASK;
	struct {
		u64 entry[512];
	} *slpt = mapmem_hphys (ptr, sizeof *slpt, MAPMEM_WRITE);
	u64 flush_address = address;
	u64 end_address = vmm_term_inf ();
	bool need_flush = false;
again:;
	u64 entry = slpt->entry[address >> bit & 0777];
	if (!(entry & 3)) { /* ReadWrite = 0 */
		if (!(address & 1))
			goto done;
		if (bit <= 12) {
			entry = (address & ~PAGESIZE_MASK) | 3; /* ReadWrite */
		} else if (bit == 21 && (d->cache.capability &
					 CAP_REG_SLLPS_21_BIT)) {
			entry = (address & ~PAGESIZE2M_MASK) |
				0x83; /* ReadWrite | Page Size */
		} else {
			void *virt;
			phys_t phys;
			alloc_page (&virt, &phys);
			memset (virt, 0, PAGESIZE);
			dmar_force_map (d, address, phys, bit - 9,
					cache_index + 1);
			entry = phys | 3; /* ReadWrite */
		}
		slpt->entry[address >> bit & 0777] = entry;
		need_flush = true;
	done:
		ret = PTE_P_BIT;
		if ((address >> bit & 0777) == 0777)
			goto end;
		if (end_address - address <= (1ULL << bit))
			goto end;
		address += 1ULL << bit;
		goto again;
	}
	ret = 0;
	if (bit <= 30 && bit > 12 && (entry & 0x80)) /* Page Size = 1 */
		goto end;
	if (bit <= 12)
		goto end;
	ret = dmar_force_map (d, address, entry & mask, bit - 9,
			      cache_index + 1);
	if (ret)
		goto done;
end:
	if (need_flush) {
		for (;;) {
			asm volatile ("clflush %0" : : "m"
				      (slpt->entry[flush_address >> bit &
						   0777]));
			if (flush_address == address)
				break;
			flush_address += 1ULL << bit;
		}
	}
	unmapmem (slpt, sizeof *slpt);
	return ret;
}

void
acpi_dmar_force_map (struct dmar_drhd_reg_data *d, u8 bus, u8 dev, u8 func)
{
	u64 address = vmm_start_inf ();
	u64 ret = do_acpi_dmar_translate (d, bus, dev, func, address,
					  dmar_force_map);
	/* Check whether pages of VMM address are properly mapped.
	 * ret=0: at least one page is mapped
	 * ret=PTE_P_BIT: every page is not mapped (RMRR is ignored)
	 * otherwise: DMA remapping is not enabled or translation-type
	 * is pass-through */
	if (ret != PTE_P_BIT)
		return;
	ret = do_acpi_dmar_translate (d, bus, dev, func, address | 1,
				      dmar_force_map);
	if (ret != PTE_P_BIT)
		return;
	printf ("%s: force map for %02X:%02X.%X\n", __func__, bus, dev, func);
}

#ifdef DMAR_PASS_THROUGH
static void *
get_dmar_address (void *data, u64 entry)
{
	struct description_header *q;
	struct dmar_info *dmar = data;

	q = acpi_mapmem (entry, sizeof *q);
	if (memcmp (q->signature, DMAR_SIGNATURE, SIGNATURE_LEN))
		return NULL;
	q = acpi_mapmem (entry, q->length);
	if (acpi_checksum (q, q->length))
		return NULL;
	dmar->table = entry;
	dmar->length = q->length;
	dmar->valid = true;
	return q;
}

static u64
alloc_acpi_pages (unsigned int npages)
{
	u64 addr;

	if (uefi_booted) {
		/* Allocate memory for ACPI tables.  Use
		 * AllocateMaxAddress to keep address in 32bit range
		 * for RSDT. */
		addr = 0xFFFFFFFF;
		if (call_uefi_allocate_pages (1, /* AllocateMaxAddress */
					      9, /* EfiACPIReclaimMemory */
					      npages, &addr))
			panic ("%s: AllocatePages failed", __func__);
	} else {
		addr = alloc_uppermem (npages * PAGESIZE);
		if (!addr)
			panic ("%s: alloc_uppermem failed", __func__);
	}
	return addr;
}
#endif

static u64
dmar_pass_through_prepare (void)
{
#ifdef DMAR_PASS_THROUGH
	struct dmar_info *dmar;

	vp.npages = 0;
	foreach_entry_in_rsdt1 (get_dmar_address, &vp.rsdt1_dmar);
	foreach_entry_in_rsdt (get_dmar_address, &vp.rsdt_dmar);
	foreach_entry_in_xsdt (get_dmar_address, &vp.xsdt_dmar);
	if (!vp.rsdt1_dmar.valid && !vp.rsdt_dmar.valid && !vp.xsdt_dmar.valid)
		return 0;
	if (vp.rsdt1_dmar.valid && vp.rsdt_dmar.valid &&
	    vp.rsdt1_dmar.table != vp.rsdt_dmar.table)
		panic ("Two DMAR tables found: 0x%llX in RSDT and 0x%llX in"
		       " another RSDT", vp.rsdt1_dmar.table,
		       vp.rsdt_dmar.table);
	if (vp.rsdt_dmar.valid && vp.xsdt_dmar.valid &&
	    vp.rsdt_dmar.table != vp.xsdt_dmar.table)
		panic ("Two DMAR tables found: 0x%llX in RSDT and 0x%llX in"
		       " XSDT", vp.rsdt_dmar.table, vp.xsdt_dmar.table);
	dmar = (vp.xsdt_dmar.valid ? &vp.xsdt_dmar :
		vp.rsdt_dmar.valid ? &vp.rsdt_dmar : &vp.rsdt1_dmar);
	vp.npages = (dmar->length + PAGESIZE - 1) / PAGESIZE + 2;
	vp.new_dmar_addr = alloc_acpi_pages (vp.npages);
	vp.new_dmar = mapmem_hphys (vp.new_dmar_addr, vp.npages * PAGESIZE,
				    MAPMEM_WRITE);
	memcpy (vp.new_dmar, acpi_mapmem (dmar->table, dmar->length),
		dmar->length);
	memcpy (&vp.host_address_width, vp.new_dmar + 36, 1);
#ifdef DISABLE_VTD
	/* Clear DMA_CTRL_PLATFORM_OPT_IN_FLAG (offset 37 bit 2) */
	u8 flags;
	memcpy (&flags, vp.new_dmar + 37, 1);
	if (flags & 4) {
		flags &= ~4;
		memcpy (vp.new_dmar + 37, &flags, 1);
		struct description_header *header = vp.new_dmar;
		header->checksum -= acpi_checksum (header, header->length);
	}
#endif
	return vp.new_dmar_addr;
#else
	return 0;
#endif
}

static bool
drhd_num_count (void *data, void *entry)
{
	struct dmar_drhd *q = entry;
	unsigned int *num = data;

	if (q->header.length >= sizeof *q && !q->header.type) /* DRHD */
		++*num;
	return false;
}

static void
filter_data64 (u64 *data, u64 base, u64 off, uint len, u64 filter)
{
	if (off < base + 8 && off + len > base) {
		if (off >= base)
			*data &= ~(filter >> (off - base) * 8);
		else
			*data &= ~(filter << (base - off) * 8);
	}
}

static int
drhd_reghandler (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 f)
{
	struct dmar_drhd_reg_data *d = data;
	u64 off = gphys - d->base;

	if (wr && off < 0x28) {	/* Global Command Register, Root Table
				 * Address Register, etc. */
		spinlock_lock (&d->lock);
		d->cached = false;
		memcpy ((void *)d->reg + off, buf, len);
		spinlock_unlock (&d->lock);
		return 1;
	}
	/* For Capability Register and Extended Capability Register read */
	if (!wr && off < 0x18 && off + len > 0x8) {
		u64 tmp;
		memcpy (&tmp, (void *)d->reg + off, len);
		/*
		 * For Capability Register, we conceal FLTS related
		 * capabilities. We also conceal reserved bits and deprecated
		 * bits for correctness.
		 */
		filter_data64 (&tmp, 0x8, off, len, 0x370000400080E000ULL);
		/*
		 * For Extended Capability Register, we conceal SMTS, PASID,
		 * and Device-TLB related capabilities for simplification.
		 * We also conceal reserved bits and deprecated bits for
		 * correctness.
		 */
		filter_data64 (&tmp, 0x10, off, len, 0xFFFFFFFFFF0C0024ULL);
		memcpy (buf, &tmp, len);
		return 1;
	}
	return 0;
}

static struct drhd_devlist *
dmar_pass_create_devlist_all (void)
{
	struct drhd_devlist *q;

	q = alloc (sizeof *q);
	q->next = NULL;
	q->type = 0;
	q->bus = 0;
	q->pathlen = 0;
	return q;
}

static struct drhd_devlist *
dmar_pass_create_devlist (void *device_scope, unsigned int length)
{
	struct {
		u8 type;
		u8 length;
		u16 reserved;
		u8 enumeration_id;
		u8 start_bus_number;
		u8 path[];
	} __attribute__ ((packed)) *p = device_scope;
	struct drhd_devlist *n = NULL, *q;
	unsigned int pathlen;

	if (!p->length || p->length > length)
		return NULL;
	if (p->length < length)
		n = dmar_pass_create_devlist (device_scope + p->length,
					      length - p->length);
	if (p->length <= sizeof *p)
		return n;
	if (p->type != 1 &&	/* ! PCI Endpoint Device */
	    p->type != 2)	/* ! PCI Sub-hierarchy */
		return n;
	pathlen = p->length - sizeof *p;
	q = alloc (sizeof *q + pathlen);
	q->next = n;
	q->type = p->type;
	q->bus = p->start_bus_number;
	q->pathlen = pathlen;
	memcpy (q->path, p->path, pathlen);
	return q;
}

static bool
drhd_mmio_register (void *data, void *entry)
{
	struct dmar_drhd *q = entry;
	unsigned int *num = data;
	int i;

	if (q->header.length < sizeof *q || q->header.type) /* !DRHD */
		return false;
	vp.reg[*num].reg = mapmem_hphys (q->register_base_address,
					 sizeof *vp.reg[*num].reg,
					 MAPMEM_WRITE);
	vp.reg[*num].base = q->register_base_address;
	vp.reg[*num].cached = false;
	vp.reg[*num].cache.root_table = NULL;
	vp.reg[*num].cache.root_table_addr = ~0;
	vp.reg[*num].cache.context_table = NULL;
	vp.reg[*num].cache.context_table_ptr = ~0;
	vp.reg[*num].cache.intr_remap_table = NULL;
	vp.reg[*num].cache.intr_remap_table_len = 0;
	for (i = 0; i < NSLPTCACHE; i++) {
		vp.reg[*num].cache.slpt[i] = NULL;
		vp.reg[*num].cache.slptptr[i] = ~0;
	}
	vp.reg[*num].mmio = mmio_register (q->register_base_address,
					   sizeof *vp.reg[*num].reg,
					   drhd_reghandler, &vp.reg[*num]);
	vp.reg[*num].segment = q->segment_number;
	vp.reg[*num].devlist = q->flags & 1 ? /* INCLUDE_PCI_ALL */
		dmar_pass_create_devlist_all () :
		dmar_pass_create_devlist (entry + sizeof *q,
					  q->header.length - sizeof *q);
	spinlock_init (&vp.reg[*num].lock);
	if (++*num == vp.drhd_num)
		return true;
	return false;
}

static void
dmar_pass_through_mmio_register (void)
{
	unsigned int i, n;

	if (!vp.new_dmar)
		return;
	n = 0;
	foreach_entry_in_dmar (vp.new_dmar, drhd_num_count, &n);
	vp.drhd_num = n;
	if (!n)
		return;
	vp.reg = alloc (sizeof *vp.reg * n);
	i = 0;
	foreach_entry_in_dmar (vp.new_dmar, drhd_mmio_register, &i);
}

static void
disable_vtd (void *dmar)
{
#ifdef DISABLE_VTD
	foreach_entry_in_dmar (dmar, disable_vtd_drhd, NULL);
#endif
}

static u32
modify_acpi_table_len (u64 address)
{
	struct description_header *p;
	u32 len;

	p = mapmem_hphys (address, sizeof *p, 0);
	len = p->length;
	unmapmem (p, sizeof *p);
	return len;
}

static u64
modify_acpi_table_do (u64 entry, char *signature, u64 address)
{
	struct description_header *p;

	p = mapmem_hphys (entry, sizeof *p, MAPMEM_WRITE);
	if (!memcmp (p->signature, signature, SIGNATURE_LEN)) {
		if (address)
			entry = address;
		else
			memset (p->signature, 0, SIGNATURE_LEN);
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

void
modify_acpi_table (char *signature, u64 address)
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

static void
acpi_init_pass (void)
{
	if (current == current->vcpu0)
		dmar_pass_through_mmio_register ();
}

static void
acpi_init_global (void)
{
	u64 rsdp;
	u64 rsdp1;
	u64 dmar_address;
	struct facp *q;
	struct acpi_ent_dmar *r;
	struct domain *create_dom() ;

	wakeup_init ();
	rsdp_found = false;
	rsdp1_found = false;
	pm1a_cnt_found = false;

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
		dmar_address = dmar_pass_through_prepare ();
		/* dmar_pass_through_prepare() uses acpi_mapmem() so
		 * find_entry() must be called again.  If
		 * call_uefi_boot_acpi_table_mod() succeeds, it
		 * modifies ACPI tables, so find_entry() must be
		 * called before the modification. */
		r = find_entry (DMAR_SIGNATURE);
		ASSERT (r);
		if (dmar_address)
			printf ("Installing a modified DMAR table"
				" at 0x%llx.\n", dmar_address);
		modify_acpi_table (DMAR_SIGNATURE, dmar_address);
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
	ASSERT (NFACS_ADDR >= 0 + 2);
	get_facs_addr (&facs_addr[0], q);
	get_reset_info (q);
	smi_cmd = q->smi_cmd;
	if (0)
		debug_dump (q, q->header.length);
	if (0)
		printf ("PM1a control port is 0x%X\n", pm1a_cnt_ioaddr);
	q = find_entry_in_rsdt (FACP_SIGNATURE);
	ASSERT (NFACS_ADDR >= 2 + 2);
	get_facs_addr (&facs_addr[2], q);
	q = find_entry_in_rsdt1 (FACP_SIGNATURE);
	ASSERT (NFACS_ADDR >= 4 + 2);
	get_facs_addr (&facs_addr[4], q);
	remove_dup_facs_addr (facs_addr, NFACS_ADDR);
	pm1a_cnt_found = true;
	save_mcfg ();
}

INITFUNC ("global3", acpi_init_global);
INITFUNC ("paral30", acpi_init_paral);
INITFUNC ("pass5", acpi_init_pass);
