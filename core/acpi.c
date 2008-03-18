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
#include "assert.h"
#include "beep.h"
#include "constants.h"
#include "initfunc.h"
#include "io_io.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "sleep.h"
#include "string.h"

#define FIND_RSDP_NOT_FOUND	0xFFFFFFFFFFFFFFFFULL
#define RSDP_SIGNATURE		"RSD PTR"
#define RSDP_SIGNATURE_LEN	7
#define ADDRESS_SPACE_ID_MEM	0
#define ADDRESS_SPACE_ID_IO	1
#define SIGNATURE_LEN		4
#define RSDT_SIGNATURE		"RSDT"
#define FACP_SIGNATURE		"FACP"
#define FACS_SIGNATURE		"FACS"
#define PM1_CNT_SLP_EN_BIT	0x2000
#define IS_STRUCT_SIZE_OK(l, h, m) \
	((l) >= ((u8 *)&(m) - (u8 *)(h)) + sizeof (m))

struct rsdp {
	u8 signature[8];
	u8 checksum;
	u8 oemid[6];
	u8 revision;
	u32 rsdt_address;
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
	u32 sci_cmd;
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
	u8 reset_reg[12];
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

static bool rsdp_found;
static struct rsdp rsdp_copy;
static bool pm1a_cnt_found;
static u32 pm1a_cnt_ioaddr;

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
find_entry (char *signature)
{
	struct rsdt *p;
	struct description_header *q;
	int i, n, len;
	u32 entry;

	if (!rsdp_found)
		return NULL;
	p = acpi_mapmem (rsdp_copy.rsdt_address, sizeof *p);
	if (memcmp (p->header.signature, RSDT_SIGNATURE, SIGNATURE_LEN))
		return NULL;
	len = p->header.length;
	p = acpi_mapmem (rsdp_copy.rsdt_address, len);
	if (acpi_checksum (p, len))
		return NULL;
	n = (p->header.length - sizeof p->header) / sizeof entry;
	for (i = 0; i < n; i++) {
		p = acpi_mapmem (rsdp_copy.rsdt_address, len);
		entry = p->entry[i];
		q = acpi_mapmem (entry, sizeof *q);
		if (memcmp (q->signature, signature, SIGNATURE_LEN))
			continue;
		q = acpi_mapmem (entry, q->length);
		if (acpi_checksum (q, q->length))
			continue;
		return q;
	}
	return NULL;
}

static struct facp *
find_facp (void)
{
	return find_entry (FACP_SIGNATURE);
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
acpi_io_monitor (enum iotype type, u32 port, void *data)
{
	u32 v;
	struct facp *facp;
	struct facs *facs;
	u64 facs_addr;

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
		if (v & PM1_CNT_SLP_EN_BIT) {
			facp = find_facp ();
			if (facp == NULL)
				goto def;
			if (IS_STRUCT_SIZE_OK (facp->header.length, facp,
					       facp->x_firmware_ctrl))
				facs_addr = facp->x_firmware_ctrl;
			else if (IS_STRUCT_SIZE_OK (facp->header.length, facp,
						    facp->firmware_ctrl))
				facs_addr = facp->firmware_ctrl;
			else
				for (;;); /* FIXME: ERROR */
			facs = acpi_mapmem (facs_addr, sizeof *facs);
			if (IS_STRUCT_SIZE_OK (facs->length, facs,
					       facs->x_firmware_waking_vector))
				facs->x_firmware_waking_vector = 0;
			if (IS_STRUCT_SIZE_OK (facs->length, facs,
					       facs->firmware_waking_vector))
				facs->firmware_waking_vector = 0xFFFF0;
			else
				for (;;); /* FIXME: ERROR */
			/* DEBUG */
			/* the computer is going to sleep */
			/* most devices are suspended */
			/* vmm can't write anything to a screen here */
			/* vmm can't input from/output to a keyboard here */
			/* vmm can beep! */
			beep_on ();
			beep_set_freq (880);
			usleep (500000);
			beep_set_freq (440);
			usleep (500000);
			beep_set_freq (880);
			usleep (500000);
			beep_set_freq (440);
			usleep (500000);
			beep_set_freq (880);
			usleep (500000);
			beep_set_freq (440);
			usleep (500000);
			beep_set_freq (880);
			goto def;
		}
	}
def:
	do_io_default (type, port, data);
}

void
acpi_iohook (void)
{
	if (pm1a_cnt_found)
		set_iofunc (pm1a_cnt_ioaddr, acpi_io_monitor);
}

static void
acpi_init_global (void)
{
	u64 rsdp;
	struct rsdp *p;
	struct facp *q;

	rsdp_found = false;
	pm1a_cnt_found = false;

	rsdp = find_rsdp ();
	if (rsdp == FIND_RSDP_NOT_FOUND) {
		printf ("ACPI RSDP not found.\n");
		return;
	}
	p = acpi_mapmem (rsdp, sizeof *p);
	memcpy (&rsdp_copy, p, sizeof *p);
	rsdp_found = true;

	q = find_facp ();
	if (!q) {
		printf ("ACPI FACP not found.\n");
		return;
	}
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
	if (0)
		debug_dump (q, q->header.length);
	if (0)
		printf ("PM1a control port is 0x%X\n", pm1a_cnt_ioaddr);
	pm1a_cnt_found = true;
}

INITFUNC ("global3", acpi_init_global);
