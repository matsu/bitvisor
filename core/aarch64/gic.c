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

#include <bits.h>
#include <common.h>
#include <constants.h>
#include <core/aarch64/gic.h>
#include <core/assert.h>
#include <core/dres.h>
#include <core/initfunc.h>
#include <core/mm.h>
#include <core/mmio.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/sleep.h>
#include <core/spinlock.h>
#include <core/types.h>
#include "../acpi.h"
#include "../exint_pass.h"
#include "asm.h"
#include "exception.h"
#include "gic.h"
#include "gic_regs.h"
#include "pcpu.h"
#include "tpidr.h"
#include "vm.h"

#define ACPI_MADT_SIGNATURE "APIC"

enum acpi_ic_type_t {
	ACPI_IC_TYPE_PROC_LOCAL_APIC,
	ACPI_IC_TYPE_IO_APIC,
	ACPI_IC_TYPE_INTR_SRC_OVERRIDE,
	ACPI_IC_TYPE_NMI_SRC,
	ACPI_IC_TYPE_LOCAL_APIC_ADDR_OVERRIDE,
	ACPI_IC_TYPE_LOCAL_APIC_NMI,
	APCI_IC_TYPE_IO_SAPIC,
	APCI_IC_TYPE_LOCAL_SAPIC,
	APCI_IC_TYPE_PLATFORM_INTR_SRC,
	ACPI_IC_TYPE_PROC_LOCAL_x2APIC,
	ACPI_IC_TYPE_LOCAL_x2APIC_NMI,
	ACPI_IC_TYPE_GICC,
	ACPI_IC_TYPE_GICD,
	ACPI_IC_TYPE_GIC_MSI_FRAME,
	ACPI_IC_TYPE_GICR,
	ACPI_IC_TYPE_GIC_ITS,
};

struct acpi_ic_header {
	u8 type;
	u8 length;
} __attribute__ ((packed));

struct acpi_gicd {
	struct acpi_ic_header header;
	u16 reserved0;
	u32 gicd_id;
	u64 phys_addr;
	u32 system_vector_base;
	u8 gic_version;
	u8 reserved1[3];
} __attribute__ ((packed));

struct acpi_gic_its {
	struct acpi_ic_header header;
	u16 reserved0;
	u32 gic_its_id;
	u64 phys_addr;
	u32 reserved1;
} __attribute__ ((packed));

struct acpi_madt {
	struct acpi_description_header header;
	u32 local_ic_addr;
	u32 flags;
	struct acpi_ic_header ics[];
} __attribute__ ((packed));

#define INTR_MAINTENANCE_NUM 25
#define INTR_RSVD_NUM_1020   1020
#define INTR_RSVD_NUM_1024   1024

#define GIC_N_INITD_WATERMARK (1 << 16)

#define GICD_TYPER 0x4

#define GICD_TYPER_LPI_BITS(v) ((((v) >> 11) & 0x1F) + 1) /* 0 based */
#define GICD_TYPER_ID_BITS(v)  ((((v) >> 19) & 0x1F) + 1) /* 0 based */
#define GICD_TYPER_LPIS	       BIT (17)

/* Currently we support only GICv3. 64KB is enough for now. */
#define GITS_SIZE (64 * KB)

#define GITS_CTLR_BASE	   0x0
#define GITS_IT_SPACE_BASE 0x10000

#define GITS_CTLR     (GITS_CTLR_BASE + 0x0)
#define GITS_IIDR     (GITS_CTLR_BASE + 0x4)
#define GITS_TYPER    (GITS_CTLR_BASE + 0x8)
#define GITS_MPAMIDR  (GITS_CTLR_BASE + 0x10)
#define GITS_PARTIDR  (GITS_CTLR_BASE + 0x14)
#define GITS_MPIDR    (GITS_CTLR_BASE + 0x18)
#define GITS_STATUSR  (GITS_CTLR_BASE + 0x40)
#define GITS_UMSIR    (GITS_CTLR_BASE + 0x48)
#define GITS_CBASER   (GITS_CTLR_BASE + 0x80)
#define GITS_CWRITER  (GITS_CTLR_BASE + 0x88)
#define GITS_CREADR   (GITS_CTLR_BASE + 0x90)
#define GITS_BASER(n) (GITS_CTLR_BASE + (0x100 + ((n) * 8)))

#define GITS_TRANSLATER (GITS_IT_SPACE_BASE + 0x40)

#define ITS_POLL_LIMIT 5000

#define CBASER_MASK \
	~(BIT (8) | BIT (9) | BIT (52) | BIT (56) | BIT (57) | BIT (58) | \
	  BIT (62))
#define CBASER_ADDR_MASK 0xFFFFFFFFFF000ULL
#define CBASER_ADDR(v)	 ((v) & CBASER_ADDR_MASK)
#define CBASER_NPAGES(v) (((v) & 0xFF) + 1) /* 0 based */
#define CBASER_VALID	 BIT (63)

#define CWRITER_IDX_MASK  0x7FFF
#define CWRITER_IDX_SHIFT 5
#define CWRITER_RETRY	  BIT (0)
#define CWRITER_IDX(v)	  (((v) >> CWRITER_IDX_SHIFT) & CWRITER_IDX_MASK)

#define CREADR_IDX_MASK	 0x7FFF
#define CREADR_IDX_SHIFT 5
#define CREADR_STALL	 BIT (0)
#define CREADR_IDX(v)	 (((v) >> CREADR_IDX_SHIFT) & CREADR_IDX_MASK)

#define GUEST_CMD false
#define HOST_CMD  true

#define GITS_CMD_INT	 0x3ULL
#define GITS_CMD_CLEAR	 0x4ULL
#define GITS_CMD_MAPD	 0x8ULL
#define GITS_CMD_MAPTI	 0xAULL
#define GITS_CMD_MAPI	 0xBULL
#define GITS_CMD_DISCARD 0xFULL

#define GITS_MAPD_VALID BIT (63)

#define LR_STATE_INACTIVE	    0ULL
#define LR_STATE_PENDING	    1ULL
#define LR_STATE_ACTIVE		    2ULL
#define LR_STATE_PENDING_AND_ACTIVE 3ULL

#define INITIAL_FREE_LR 16

#define ICC_IAR_MASK 0xFFFFFF

#define ICC_RPR_MASK 0xFF

#define ICC_CTLR_CBPR_MASK     0x1
#define ICC_CTLR_CBPR_SHIFT    0
#define ICC_CTLR_EOIMODE       BIT (1)
#define ICC_CTLR_EOIMODE_MASK  0x1
#define ICC_CTLR_EOIMODE_SHIFT 1

#define ICH_VTR_MASK  0x1F
#define ICH_VTR_SHIFT 0x0

#define ICH_VMCR_VENG0	  BIT (0)
#define ICH_VMCR_VENG1	  BIT (1)
#define ICH_VMCR_VACKCTL  BIT (2)
#define ICH_VMCR_VFIQEN	  BIT (3)
#define ICH_VMCR_VCBPR	  BIT (4)
#define ICH_VMCR_VEOIM	  BIT (9)
#define ICH_VMCR_VBPR1(v) (((v) & 0x7) << 18)
#define ICH_VMCR_VBPR0(v) (((v) & 0x7) << 21)
#define ICH_VMCR_VPMR(v)  (((v) & 0xFF) << 24)

#define ICH_HCR_EN	     BIT (0)
#define ICH_HCR_UIE	     BIT (1)
#define ICH_HCR_LRENPIE	     BIT (2)
#define ICH_HCR_NPIE	     BIT (3)
#define ICH_HCR_VGRP0EIE     BIT (4)
#define ICH_HCR_VGRP0DIE     BIT (5)
#define ICH_HCR_VGRP1EIE     BIT (6)
#define ICH_HCR_VGRP1DIE     BIT (7)
#define ICH_HCR_VSGIEOICOUNT BIT (8)
#define ICH_HCR_TC	     BIT (10)
#define ICH_HCR_TALL0	     BIT (11)
#define ICH_HCR_TALL1	     BIT (12)
#define ICH_HCR_TSEI	     BIT (13)
#define ICH_HCR_TDIR	     BIT (14)
#define ICH_HCR_DVIM	     BIT (15)

#define ICC_SRE_SYSREG_IF_EN 0x1

#define ICH_LR_VINTID(v)   (((v) & 0xFFFFFFFF) << 0)
#define ICH_LR_PINTID(v)   (((v) & 0x1FFF) << 32)
#define ICH_LR_PRIORITY(v) (((v) & 0xff) << 48)
#define ICH_LR_GROUP(v)	   (((v) & 0x1) << 60)
#define ICH_LR_HW	   BIT (61)
#define ICH_LR_STATE(v)	   (((v) & 0x3) << 62)

struct its_cmd {
	u64 data[4];
} __attribute__ ((packed));

struct its_pending_cmd {
	LIST1_DEFINE (struct its_pending_cmd);
	struct its_cmd cmd;
};

struct event_data {
	LIST1_DEFINE (struct event_data);
	u32 event_id;
	u32 pint_id;
	bool valid;
};

struct dev_data {
	LIST1_DEFINE (struct dev_data);
	LIST1_DEFINE_HEAD (struct event_data, mapped_event);
	u64 itt_base;
	u32 dev_id;
	bool valid;
};

struct pint_map {
	u32 dev_id;
	u32 event_id;
};

struct gicd_host {
	phys_t base_phys;
	u32 nids;
	u32 n_lpis;
	bool its_support;
};

struct its_host {
	LIST1_DEFINE_HEAD (struct its_pending_cmd, free_cmds);
	LIST1_DEFINE_HEAD (struct its_pending_cmd, h_pending_cmds);
	LIST1_DEFINE_HEAD (struct its_pending_cmd, g_pending_cmds);
	LIST1_DEFINE_HEAD (struct dev_data, mapped_dev);

	phys_t base_phys;
	struct dres_reg *r;

	struct pint_map *pimap;

	phys_t h_cbase_phys;
	struct its_cmd *h_cbase;

	phys_t g_cbase_raw;
	phys_t g_cbase_phys;
	struct its_cmd *g_cbase;
	u64 cbase_nbytes;
	u64 nidx;

	u32 dev_id_mask;
	u32 event_id_mask;

	uint h_cmd_cur_head;
	uint h_cmd_cur_tail;

	uint g_cmd_cur_head;
	uint g_cmd_cur_tail;
	uint g_running_cmds;

	bool cmd_ready;
	spinlock_t lock;
};

struct gic_lr_list {
	LIST1_DEFINE (struct gic_lr_list);
	u64 lr_val;
};

struct init_icc {
	u64 icc_bpr0_el1;
	u64 icc_bpr1_el1;
	u64 icc_ctlr_el1;
	u64 icc_pmr_el1;
	u64 icc_sre_el2;
	u64 ich_hcr_el2;
	u64 ich_vmcr_el2;
};

static struct init_icc ii;
static struct gicd_host *gicd;
static struct its_host *its;

static void
enqueue_lr (struct pcpu *currentcpu, u64 val)
{
	struct gic_lr_list *l;

	l = LIST1_POP (currentcpu->int_freelist);
	if (!l)
		l = alloc (sizeof *l);

	l->lr_val = val;
	LIST1_ADD (currentcpu->int_pending, l);
}

static bool
dequeue_lr (struct pcpu *currentcpu, u64 *lr_val)
{
	struct gic_lr_list *l;

	ASSERT (lr_val);

	l = LIST1_POP (currentcpu->int_pending);
	if (l) {
		*lr_val = l->lr_val;
		LIST1_PUSH (currentcpu->int_freelist, l);
	}

	return !!l;
}

static void
set_lr (uint lr_idx, u64 val)
{
	switch (lr_idx) {
	case 0:
		msr (GIC_ICH_LR0_EL2, val);
		break;
	case 1:
		msr (GIC_ICH_LR1_EL2, val);
		break;
	case 2:
		msr (GIC_ICH_LR2_EL2, val);
		break;
	case 3:
		msr (GIC_ICH_LR3_EL2, val);
		break;
	case 4:
		msr (GIC_ICH_LR4_EL2, val);
		break;
	case 5:
		msr (GIC_ICH_LR5_EL2, val);
		break;
	case 6:
		msr (GIC_ICH_LR6_EL2, val);
		break;
	case 7:
		msr (GIC_ICH_LR7_EL2, val);
		break;
	case 8:
		msr (GIC_ICH_LR8_EL2, val);
		break;
	case 9:
		msr (GIC_ICH_LR9_EL2, val);
		break;
	case 10:
		msr (GIC_ICH_LR10_EL2, val);
		break;
	case 11:
		msr (GIC_ICH_LR11_EL2, val);
		break;
	case 12:
		msr (GIC_ICH_LR12_EL2, val);
		break;
	case 13:
		msr (GIC_ICH_LR13_EL2, val);
		break;
	case 14:
		msr (GIC_ICH_LR14_EL2, val);
		break;
	case 15:
		msr (GIC_ICH_LR15_EL2, val);
		break;
	default:
		panic ("lr_idx out of bound");
		break;
	}
}

static void
handle_mint (uint intid)
{
	printf ("We don't handle maintenance interrupt now\n");
}

static void
try_inject_vint (u64 intid, u64 rpr, uint group)
{
	struct pcpu *currentcpu;
	u64 elrsr, val, lr_val, g;
	uint i;
	bool empty = false;

	currentcpu = tpidr_get_pcpu ();

	/* Currently vintid = pintid */
	g = !!group;
	val = ICH_LR_VINTID (intid) | ICH_LR_PINTID (intid) |
		ICH_LR_PRIORITY (rpr) | ICH_LR_GROUP (g) | ICH_LR_HW |
		ICH_LR_STATE (LR_STATE_PENDING);
	enqueue_lr (currentcpu, val);

	elrsr = mrs (GIC_ICH_ELRSR_EL2);
	for (i = 0; elrsr != 0 && i < currentcpu->max_int_slot; i++) {
		empty = !!(elrsr & 0x1);
		if (empty) {
			if (dequeue_lr (currentcpu, &lr_val))
				set_lr (i, lr_val);
			else
				break;
		}
		elrsr >>= 1;
	}
}

static enum exception_handle_return
gic_handle_fiq (union exception_saved_regs *r)
{
	uint intid, rpr;
	int num;

	/* Acknowledge the interrupt */
	intid = mrs (GIC_ICC_IAR0_EL1) & ICC_IAR_MASK;
	rpr = mrs (GIC_ICC_RPR_EL1) & ICC_RPR_MASK;

	/* TODO: what to do with special INTID? */
	if (intid >= INTR_RSVD_NUM_1020 && intid <= INTR_RSVD_NUM_1024)
		goto end;

	num = exint_pass_intr_call (intid);

	/* Priority drop, want to drop after msr() */
	msr (GIC_ICC_EOIR0_EL1, intid);
	isb ();

	if (intid == INTR_MAINTENANCE_NUM) {
		handle_mint (intid);
		msr (GIC_ICC_DIR_EL1, intid); /* Deactivate the interrupt */
	} else {
		if (num != -1)
			try_inject_vint (intid, rpr, 0);
		else
			msr (GIC_ICC_DIR_EL1, intid);
	}
end:
	return EXCEPTION_HANDLE_RETURN_OK;
}

static enum exception_handle_return
gic_handle_irq (union exception_saved_regs *r)
{
	uint intid, rpr;
	int num;

	/* Acknowledge the interrupt */
	intid = mrs (GIC_ICC_IAR1_EL1) & ICC_IAR_MASK;
	rpr = mrs (GIC_ICC_RPR_EL1) & ICC_RPR_MASK;

	/* TODO: what to do with special INTID? */
	if (intid >= INTR_RSVD_NUM_1020 && intid <= INTR_RSVD_NUM_1024)
		goto end;

	num = exint_pass_intr_call (intid);

	/* Priority drop, want to drop after msr() */
	msr (GIC_ICC_EOIR1_EL1, intid);
	isb ();

	if (intid == INTR_MAINTENANCE_NUM) {
		handle_mint (intid);
		msr (GIC_ICC_DIR_EL1, intid); /* Deactivate the interrupt */
	} else {
		if (num != -1)
			try_inject_vint (intid, rpr, 1);
		else
			msr (GIC_ICC_DIR_EL1, intid);
	}
end:
	return EXCEPTION_HANDLE_RETURN_OK;
}

/* We currently rely on initialization by the firmware */
void
gic_setup_virtual_gic (void)
{
	struct pcpu *currentcpu;
	u64 vmcr, val;
	uint i;

	currentcpu = tpidr_get_pcpu ();

	val = (mrs (GIC_ICH_VTR_EL2) >> ICH_VTR_SHIFT) & ICH_VTR_MASK;
	val += 1; /* 0 based value */
	ASSERT (val <= 16);
	currentcpu->max_int_slot = val;

	/*
	 * According to the specification, LR reg value is unknown on warm
	 * reset. Let's clear them for to make sure that all of them are in
	 * inactive state. We need boundary because accessing non-existing
	 * LR register causes an error.
	 */
	for (i = 0; i < val; i++)
		set_lr (i, 0x0);

	if (currentcpu->cpunum == 0) {
		/* Copy current ICC state to ICV */
		vmcr = 0;
		val = mrs (GIC_ICC_PMR_EL1);
		vmcr |= ICH_VMCR_VPMR (val);
		val = mrs (GIC_ICC_BPR0_EL1);
		vmcr |= ICH_VMCR_VBPR0 (val);
		val = mrs (GIC_ICC_BPR1_EL1);
		vmcr |= ICH_VMCR_VBPR1 (val);
		val = (mrs (GIC_ICC_CTLR_EL1) >> ICC_CTLR_EOIMODE_SHIFT) &
			ICC_CTLR_EOIMODE_MASK;
		vmcr |= (ICH_VMCR_VEOIM & -val); /* -val is masking trick */
		val = (mrs (GIC_ICC_CTLR_EL1) >> ICC_CTLR_CBPR_SHIFT) &
			ICC_CTLR_CBPR_MASK;
		vmcr |= (ICH_VMCR_VCBPR & -val);
		vmcr |= ICH_VMCR_VFIQEN;
		val = mrs (GIC_ICC_IGRPEN1_EL1);
		vmcr |= (ICH_VMCR_VENG1 & -val);
		val = mrs (GIC_ICC_IGRPEN0_EL1);
		vmcr |= (ICH_VMCR_VENG0 & -val);

		msr (GIC_ICH_HCR_EL2, ICH_HCR_EN);
		msr (GIC_ICH_VMCR_EL2, vmcr);
		/* Make writing to EOIR0/1 priority drop only */
		msr (GIC_ICC_CTLR_EL1,
		     mrs (GIC_ICC_CTLR_EL1) | ICC_CTLR_EOIMODE);
		msr (GIC_ICC_IGRPEN0_EL1, 0x1);
		msr (GIC_ICC_IGRPEN1_EL1, 0x1);
		/* We don't deal with legacy MMIO interface currently */
		msr (GIC_ICC_SRE_EL2,
		     mrs (GIC_ICC_SRE_EL2) | ICC_SRE_SYSREG_IF_EN);

		ii.icc_bpr0_el1 = mrs (GIC_ICC_BPR0_EL1);
		ii.icc_bpr1_el1 = mrs (GIC_ICC_BPR1_EL1);
		ii.icc_ctlr_el1 = mrs (GIC_ICC_CTLR_EL1);
		ii.icc_pmr_el1 = mrs (GIC_ICC_PMR_EL1);
		ii.icc_sre_el2 = mrs (GIC_ICC_SRE_EL2);
		ii.ich_hcr_el2 = mrs (GIC_ICH_HCR_EL2);
		ii.ich_vmcr_el2 = mrs (GIC_ICH_VMCR_EL2);

		exception_set_handler (gic_handle_irq, gic_handle_fiq);
	} else {
		msr (GIC_ICC_BPR0_EL1, ii.icc_bpr0_el1);
		msr (GIC_ICC_BPR1_EL1, ii.icc_bpr1_el1);
		msr (GIC_ICC_CTLR_EL1, ii.icc_ctlr_el1);
		msr (GIC_ICC_PMR_EL1, ii.icc_pmr_el1);
		msr (GIC_ICC_SRE_EL2, ii.icc_sre_el2);
		msr (GIC_ICH_HCR_EL2, ii.ich_hcr_el2);
		msr (GIC_ICH_VMCR_EL2, ii.ich_vmcr_el2);
		msr (GIC_ICC_IGRPEN0_EL1, 0x1);
		msr (GIC_ICC_IGRPEN1_EL1, 0x1);
	}
}

int
gic_sgi_handle (uint group, u64 *reg, bool wr)
{
	int error = 0;

	if (wr) {
		switch (group) {
		case 0:
			msr (GIC_ICC_SGI0R_EL1, *reg);
			break;
		case 1:
			msr (GIC_ICC_SGI1R_EL1, *reg);
			break;
		default:
			error = 1;
			break;
		}
	} else {
		*reg = 0;
	}

	return error;
}

int
gic_asgi_handle (u64 *reg, bool wr)
{
	if (wr)
		msr (GIC_ICC_ASGI1R_EL1, *reg);
	else
		*reg = 0;

	return 0;
}

static void
its_handle_cbaser (struct its_host *its, u64 rbase, bool wr, union mem *data)
{
	phys_t base, p;
	u64 raw, nbytes, old_nbytes;
	bool ready;

	if (wr) {
		raw = data->qword;
		old_nbytes = its->cbase_nbytes;

		its->g_cbase_raw = raw;
		ready = !!(raw & CBASER_VALID);

		if (!ready)
			return;

		base = CBASER_ADDR (raw);
		nbytes = CBASER_NPAGES (raw) * PAGESIZE;

		if (!base || nbytes == 0) {
			printf ("NULL CBASER or queue size is 0 when ready\n");
			return;
		}

		if (base != its->g_cbase_phys || nbytes != old_nbytes) {
			if (its->g_cbase)
				unmapmem (its->g_cbase, old_nbytes);

			if (nbytes != old_nbytes) {
				if (its->h_cbase)
					free (its->h_cbase);
				its->h_cbase = alloc2 (nbytes, &p);
				its->h_cbase_phys = p;
			}

			its->g_cbase_phys = base;
			its->cbase_nbytes = nbytes;
			its->nidx = nbytes / sizeof (struct its_cmd);
			its->g_cbase = mapmem_as (vm_get_current_as (), base,
						  nbytes, 0);
			ASSERT (its->g_cbase);
		}

		ASSERT (its->h_cbase);
		memset (its->h_cbase, 0, nbytes);

		its->h_cmd_cur_head = 0;
		its->h_cmd_cur_tail = 0;

		its->g_cmd_cur_head = 0;
		its->g_cmd_cur_tail = 0;
		its->g_running_cmds = 0;

		its->cmd_ready = ready;

		raw = raw & ~CBASER_ADDR_MASK;
		raw |= CBASER_ADDR (its->h_cbase_phys);
		dres_reg_write64 (its->r, rbase, raw);
	} else {
		data->qword = its->g_cbase_raw & CBASER_MASK;
	}
}

static struct dev_data *
find_dev_data (struct its_host *its, u32 dev_id)
{
	struct dev_data *dd;

	LIST1_FOREACH (its->mapped_dev, dd) {
		if (dd->dev_id == dev_id)
			break;
	}

	return dd;
}

static struct event_data *
find_event_data (struct dev_data *dd, u32 event_id)
{
	struct event_data *ed;

	LIST1_FOREACH (dd->mapped_event, ed) {
		if (ed->event_id == event_id)
			break;
	}

	return ed;
}

static void
update_dev_data (struct its_host *its, u32 dev_id, u64 itt_base, bool valid)
{
	struct dev_data *dd;
	struct event_data *ed;

	dd = find_dev_data (its, dev_id);
	if (dd) {
		/*
		 * According to the spec, ITT should be empty. Otherwise, it
		 * is unpredictable. In case there is a base change, we assume
		 * that the new ITT is empty. So, we remove existing registered
		 * event_ids.
		 */
		if (itt_base != dd->itt_base) {
			printf ("%s(): dev_id %u itt_base change from 0x%llX "
				"to 0x%llX\n",
				__func__, dev_id, dd->itt_base, itt_base);
			while ((ed = LIST1_POP (dd->mapped_event)))
				free (ed);
			dd->itt_base = itt_base;
			memset (its->pimap, 0,
				sizeof (struct pint_map) * gicd->n_lpis);
		}
	} else {
		dd = alloc (sizeof *dd);
		LIST1_HEAD_INIT (dd->mapped_event);
		dd->dev_id = dev_id;
		dd->itt_base = itt_base;
		LIST1_ADD (its->mapped_dev, dd);
	}
	dd->valid = valid;
}

static void
update_event_data (struct dev_data *dd, u32 event_id, u32 pint_id)
{
	struct event_data *ed;

	ed = find_event_data (dd, event_id);
	if (ed) {
		/* If it already exists, update pint_id */
		ed->pint_id = pint_id;
	} else {
		ed = alloc (sizeof *ed);
		ed->event_id = event_id;
		ed->pint_id = pint_id;
		LIST1_ADD (dd->mapped_event, ed);
	}
	ed->valid = true;
}

static void
discard_event_data (struct dev_data *dd, u32 event_id)
{
	struct event_data *ed;

	ed = find_event_data (dd, event_id);
	if (ed)
		ed->valid = false;
	else
		printf ("%s(): event_id %u not found\n", __func__, event_id);
}

static bool
check_id_range (struct its_host *its, u32 dev_id, u32 event_id)
{
	return (dev_id & ~its->dev_id_mask) == 0 &&
		(event_id & ~its->event_id_mask) == 0;
}

static void
gits_cmd_mapd_hook (struct its_host *its, struct its_cmd *cmd)
{
	u64 itt_base;
	u32 dev_id;
	bool valid;

	dev_id = cmd->data[0] >> 32;
	itt_base = (cmd->data[2] >> 8) & (BIT (48) - 1);
	valid = !!(cmd->data[2] & GITS_MAPD_VALID);

	if (check_id_range (its, dev_id, 0))
		update_dev_data (its, dev_id, itt_base, valid);
	else
		printf ("%s(): dev_id %u out of range\n", __func__, dev_id);
}

static void
do_map_event_hook (struct its_host *its, u32 dev_id, u32 event_id, u32 pint_id)
{
	struct dev_data *dd;
	uint i;

	if (check_id_range (its, dev_id, event_id)) {
		ASSERT (pint_id >= GIC_LPI_START && pint_id < gicd->nids);
		i = pint_id - GIC_LPI_START;
		dd = find_dev_data (its, dev_id);
		if (dd) {
			if (pint_id >= GIC_LPI_START && pint_id < gicd->nids) {
				update_event_data (dd, event_id, pint_id);
				its->pimap[i].dev_id = dev_id;
				its->pimap[i].event_id = event_id;
			} else {
				printf ("%s(): dev_id %u event_id %u invalid "
					"pint_id %u\n",
					__func__, dev_id, event_id, pint_id);
			}
		} else {
			printf ("%s(): dev_id %u not found, do nothing \n",
				__func__, dev_id);
		}
	} else {
		printf ("%s(): dev_id %u or event_id %u out of range\n",
			__func__, dev_id, event_id);
	}
}

static void
gits_cmd_mapti_hook (struct its_host *its, struct its_cmd *cmd)
{
	u32 dev_id, event_id, pint_id;

	dev_id = cmd->data[0] >> 32;
	event_id = cmd->data[1] & 0xFFFFFFFF;
	pint_id = cmd->data[1] >> 32;

	do_map_event_hook (its, dev_id, event_id, pint_id);
}

static void
gits_cmd_mapi_hook (struct its_host *its, struct its_cmd *cmd)
{
	u32 dev_id, event_id;

	dev_id = cmd->data[0] >> 32;
	event_id = cmd->data[1] & 0xFFFFFFFF;

	do_map_event_hook (its, dev_id, event_id, event_id);
}

static void
gits_cmd_discard_hook (struct its_host *its, struct its_cmd *cmd)
{
	struct dev_data *dd;
	u32 dev_id, event_id;

	dev_id = cmd->data[0] >> 32;
	event_id = cmd->data[1] & 0xFFFFFFFF;

	if (check_id_range (its, dev_id, event_id)) {
		dd = find_dev_data (its, dev_id);
		if (dd)
			discard_event_data (dd, event_id);
		else
			printf ("%s(): dev_id %u not found, do nothing \n",
				__func__, dev_id);
	} else {
		printf ("%s(): dev_id %u or event_id %u out of range\n",
			__func__, dev_id, event_id);
	}
}

static void
its_hook_cmd (struct its_host *its, struct its_cmd *cmd)
{
	switch (cmd->data[0] & 0xFF) {
	case GITS_CMD_MAPD:
		gits_cmd_mapd_hook (its, cmd);
		break;
	case GITS_CMD_MAPTI:
		gits_cmd_mapti_hook (its, cmd);
		break;
	case GITS_CMD_MAPI:
		gits_cmd_mapi_hook (its, cmd);
		break;
	case GITS_CMD_DISCARD:
		gits_cmd_discard_hook (its, cmd);
		break;
	default:
		break;
	}
}

static u64
its_submit_cmds (struct its_host *its, bool host_cmd, u64 flags)
{
	struct its_pending_cmd *pending;
	u64 i, end, count, raw, nidx;

	nidx = its->nidx;
	end = its->h_cmd_cur_head;
	count = 0;
	for (i = its->h_cmd_cur_tail; ((i + 1) % nidx) != end;
	     i = (i + 1) % nidx) {
		pending = host_cmd ? LIST1_POP (its->h_pending_cmds) :
				     LIST1_POP (its->g_pending_cmds);
		if (!pending)
			break;
		its->h_cbase[i] = pending->cmd;
		if (!host_cmd)
			its_hook_cmd (its, &its->h_cbase[i]);
		count++;
		LIST1_PUSH (its->free_cmds, pending);
	}

	if (count > 0) {
		its->h_cmd_cur_tail = i;
		/* Make sure all commands are globally visable */
		asm volatile ("dsb sy" : : : "memory");
		raw = ((i & CWRITER_IDX_MASK) << CWRITER_IDX_SHIFT) | flags;
		dres_reg_write64 (its->r, GITS_CWRITER, raw);
	}

	return count;
}

static void
its_wait_cmd (struct its_host *its)
{
	u64 i, v, head;
	bool stall = false;

	for (i = 0; i < ITS_POLL_LIMIT; i++) {
		dres_reg_read64 (its->r, GITS_CREADR, &v);
		head = CREADR_IDX (v);
		stall = !!(v & CREADR_STALL);
		if (stall)
			break;
		if (head == its->h_cmd_cur_tail) {
			its->h_cmd_cur_head = head;
			break;
		}
		usleep (1);
	}

	if (stall)
		panic ("%s(): stall detected", __func__);
	if (i >= ITS_POLL_LIMIT)
		panic ("%s(): timeout", __func__);
}

static struct its_pending_cmd *
get_free_entry (struct its_host *its)
{
	struct its_pending_cmd *free_entry;

	free_entry = LIST1_POP (its->free_cmds);
	if (!free_entry)
		free_entry = alloc (sizeof *free_entry);

	return free_entry;
}

static void
its_handle_cwriter (struct its_host *its, u64 rbase, bool wr, union mem *data)
{
	struct its_pending_cmd *free_entry;
	u64 raw, i, end, nidx;

	if (wr) {
		raw = data->qword;
		nidx = its->nidx;
		end = CWRITER_IDX (data->qword);

		for (i = its->g_cmd_cur_tail; i != end; i = (i + 1) % nidx) {
			free_entry = get_free_entry (its);

			free_entry->cmd = its->g_cbase[i];

			LIST1_ADD (its->g_pending_cmds, free_entry);
		}
		its->g_cmd_cur_tail = i;

		its->g_running_cmds += its_submit_cmds (its, GUEST_CMD,
							raw & CWRITER_RETRY);
	} else {
		data->qword = its->g_cmd_cur_tail << CWRITER_IDX_SHIFT;
	}
}

static void
its_handle_creadr (struct its_host *its, u64 rbase, bool wr, union mem *data)
{
	u64 raw, nidx, diff, stall_flag;
	u64 h_new_head, g_new_head;

	if (wr)
		return;

	dres_reg_read64 (its->r, rbase, &raw);
	stall_flag = raw & CREADR_STALL;

	if (stall_flag)
		panic ("We currently don't expect CREADR stall bit");

	h_new_head = CREADR_IDX (raw);
	nidx = its->nidx;
	diff = ((h_new_head + nidx) - its->h_cmd_cur_head) % nidx;
	ASSERT (its->g_running_cmds >= diff);
	its->g_running_cmds -= diff;
	g_new_head = (its->g_cmd_cur_head + diff) % nidx;

	its->h_cmd_cur_head = h_new_head;
	its->g_cmd_cur_head = g_new_head;

	/* Run commands from host first if there exists */
	if (its->g_running_cmds == 0) {
		if (its_submit_cmds (its, HOST_CMD, 0) > 0)
			its_wait_cmd (its);
	}

	/* Try to run guest's pending commands if there exists */
	its->g_running_cmds += its_submit_cmds (its, GUEST_CMD, 0);

	data->qword = (its->g_cmd_cur_head << CREADR_IDX_SHIFT) | stall_flag;
}

static void
its_default_dword (struct its_host *its, u64 rbase, bool wr, union mem *data)
{
	if (wr)
		dres_reg_write32 (its->r, rbase, data->dword);
	else
		dres_reg_read32 (its->r, rbase, data);
}

static void
its_default_qword (struct its_host *its, u64 rbase, bool wr, union mem *data)
{
	if (wr)
		dres_reg_write64 (its->r, rbase, data->qword);
	else
		dres_reg_read64 (its->r, rbase, data);
}

enum dres_reg_ret_t
gic_its_handler (const struct dres_reg *r, void *handle, phys_t offset,
		 bool wr, void *buf, uint len)
{
	struct reg_handler {
		u64 rbase;
		uint size;
		uint iosize;
		void (*handler) (struct its_host *its, u64 base, bool wr,
				 union mem *data);
	};

	static const struct reg_handler rh[] = {
		{ 0x0, 4, 4, its_default_dword }, /* CTLR */
		{ 0x4, 4, 4, its_default_dword }, /* IIDR */
		{ 0x8, 8, 8, its_default_qword }, /* TYPER */
		{ 0x10, 4, 4, its_default_dword }, /* MPAMIDR */
		{ 0x14, 4, 4, its_default_dword }, /* PARTIDR */
		{ 0x18, 4, 4, its_default_dword }, /* MPIDR */
		{ 0x1C, 36, 4, its_default_dword },
		{ 0x40, 4, 4, its_default_dword }, /* STATUSR */
		{ 0x44, 4, 4, its_default_dword },
		{ 0x48, 8, 8, its_default_qword }, /* UMSIR */
		{ 0x50, 48, 8, its_default_dword },
		{ 0x80, 8, 8, its_handle_cbaser }, /* CBASER */
		{ 0x88, 8, 8, its_handle_cwriter }, /* CWRITER */
		{ 0x90, 8, 8, its_handle_creadr }, /* CREADR */
		{ 0x98, 104, 8, its_default_dword },
		{ 0x100, 8, 8, its_default_qword }, /* BASER0 */
		{ 0x108, 8, 8, its_default_qword }, /* BASER1 */
		{ 0x110, 8, 8, its_default_qword }, /* BASER2 */
		{ 0x118, 8, 8, its_default_qword }, /* BASER3 */
		{ 0x120, 8, 8, its_default_qword }, /* BASER4 */
		{ 0x128, 8, 8, its_default_qword }, /* BASER5 */
		{ 0x130, 8, 8, its_default_qword }, /* BASER6 */
		{ 0x138, 8, 8, its_default_qword }, /* BASER7 */
		{ 0x140, 65216, 8, its_default_dword },
		{ 0, 0, 0, NULL },
	};
	const struct reg_handler *h;
	union mem tmp;
	u64 rbase;
	uint iosize, area_remain;

	h = rh;

	spinlock_lock (&its->lock);

	if (!wr)
		memset (buf, 0, len);

	/* Firstly, seek to the first handler */
	rbase = h->rbase;
	area_remain = h->size;
	iosize = h->iosize;
	while (area_remain && offset >= iosize) {
		rbase += iosize;
		area_remain -= iosize;
		offset -= iosize;
		if (area_remain == 0) {
			h++;
			rbase = h->rbase;
			area_remain = h->size;
			iosize = h->iosize;
		}
	}
	/* Deal with unaligned access first */
	if (area_remain && offset > 0) {
		h->handler (its, rbase, false, &tmp);
		void *p = &tmp.byte + offset;
		u32 s = len - offset;
		if (s > len)
			s = len;
		if (wr) {
			memcpy (p, buf, s);
			h->handler (its, rbase, true, &tmp);
		} else {
			memcpy (buf, p, s);
		}
		buf += s;
		len -= s;
		if (len == 0)
			goto end;
		offset = 0;
		rbase += iosize;
		area_remain -= iosize;
		if (area_remain == 0) {
			h++;
			rbase = h->rbase;
			area_remain = h->size;
			iosize = h->iosize;
		}
	}
	/* From this point onward, all accesses are aligned */
	while (area_remain && len >= iosize) {
		h->handler (its, rbase, wr, buf);
		buf += iosize;
		len -= iosize;
		rbase += iosize;
		area_remain -= iosize;
		if (area_remain == 0) {
			h++;
			rbase = h->rbase;
			area_remain = h->size;
			iosize = h->iosize;
		}
	}
	/* Deal with partial accesses */
	if (area_remain && len) {
		h->handler (its, rbase, false, &tmp);
		if (wr) {
			memcpy (&tmp, buf, len);
			h->handler (its, rbase, true, &tmp);
		} else {
			memcpy (buf, &tmp, len);
		}
		len -= len;
	}
end:
	ASSERT (len == 0);

	spinlock_unlock (&its->lock);

	return DRES_REG_RET_DONE;
}

static bool
its_check_valid_map (u32 dev_id, u32 event_id)
{
	struct dev_data *dd;
	struct event_data *ed;
	bool ok = false;

	if (!check_id_range (its, dev_id, event_id)) {
		printf ("%s(): dev_id %u or event_id %u out of range\n",
			__func__, dev_id, event_id);
		goto end;
	}

	dd = find_dev_data (its, dev_id);
	if (!dd) {
		printf ("%s(): dev_id %u not found\n", __func__, dev_id);
		goto end;
	} else if (!dd->valid) {
		printf ("%s(): dev_id %u is not valid\n", __func__, dev_id);
		goto end;
	}

	ed = find_event_data (dd, event_id);
	if (!ed) {
		printf ("%s(): dev_id %u event_id %u not found\n", __func__,
			dev_id, event_id);
		goto end;
	} else if (!ed->valid) {
		printf ("%s(): dev_id %u event_id %u is not valid\n", __func__,
			dev_id, event_id);
		goto end;
	}

	ok = true;
end:
	return ok;
}

void
gic_its_int_set (u32 dev_id, u32 event_id)
{
	struct its_pending_cmd *e;

	spinlock_lock (&its->lock);

	if (its_check_valid_map (dev_id, event_id) && its->cmd_ready) {
		e = get_free_entry (its);

		e->cmd.data[0] = GITS_CMD_INT | ((u64)dev_id << 32);
		e->cmd.data[1] = event_id & 0xFFFFFFFF;
		e->cmd.data[2] = 0x0;
		e->cmd.data[3] = 0x0;

		LIST1_ADD (its->h_pending_cmds, e);

		if (its->g_running_cmds == 0) {
			if (its_submit_cmds (its, HOST_CMD, 0) > 0)
				its_wait_cmd (its);
		}
	}

	spinlock_unlock (&its->lock);
}

bool
gic_its_pintd_match (u32 pint, u32 dev_id, u32 event_id, bool *valid)
{
	struct pint_map *p;
	bool match = false;

	if (pint < GIC_LPI_START || pint >= gicd->nids)
		goto end;

	spinlock_lock (&its->lock);

	p = &its->pimap[pint - GIC_LPI_START];

	match = dev_id == p->dev_id && event_id == p->event_id;
	if (match && valid)
		*valid = its_check_valid_map (dev_id, event_id);

	spinlock_unlock (&its->lock);
end:
	return match;
}

static void
gic_its_init (phys_t base_phys)
{
	u64 data, s;
	enum dres_err_t err;

	if (its)
		panic ("%s(): currently support only single ITS", __func__);

	printf ("GIC-ITS base 0x%llX\n", base_phys);

	its = alloc (sizeof *its);
	ASSERT (its);
	LIST1_HEAD_INIT (its->free_cmds);
	LIST1_HEAD_INIT (its->h_pending_cmds);
	LIST1_HEAD_INIT (its->g_pending_cmds);
	LIST1_HEAD_INIT (its->mapped_dev);
	its->base_phys = base_phys;
	its->r = dres_reg_alloc (base_phys, GITS_SIZE, DRES_REG_TYPE_MM,
				 dres_reg_translate_1to1, NULL, 0);
	ASSERT (its->r);
	err = dres_reg_register_handler (its->r, gic_its_handler, its);
	ASSERT (err == DRES_ERR_NONE);
	s = sizeof (struct pint_map) * gicd->n_lpis;
	its->pimap = alloc (s);
	memset (its->pimap, 0, s);
	its->h_cbase_phys = 0x0;
	its->h_cbase = NULL;
	its->g_cbase_raw = 0x0;
	its->g_cbase_phys = 0x0;
	its->g_cbase = NULL;
	its->cbase_nbytes = 0;
	dres_reg_read64 (its->r, GITS_TYPER, &data);
	its->dev_id_mask = BIT (((data >> 13) & 0x1F) + 1) - 1;
	its->event_id_mask = BIT (((data >> 8) & 0x1F) + 1) - 1;
	its->h_cmd_cur_head = 0;
	its->h_cmd_cur_tail = 0;
	its->g_cmd_cur_head = 0;
	its->g_cmd_cur_tail = 0;
	its->g_running_cmds = 0;
	its->cmd_ready = false;
	spinlock_init (&its->lock);
}

static void
acpi_madt_handle_gicd (struct acpi_ic_header *h)
{
	struct acpi_gicd *entry;
	phys_t base, typer;
	u32 *gicd_typer, v;

	entry = (struct acpi_gicd *)h;
	base = entry->phys_addr;
	ASSERT (base);

	typer = base + GICD_TYPER;
	gicd_typer = mapmem_hphys (typer, sizeof *gicd_typer, MAPMEM_UC);
	ASSERT (gicd_typer);
	v = *gicd_typer;
	unmapmem (gicd_typer, sizeof *gicd_typer);

	if (!(v & GICD_TYPER_LPIS))
		panic ("%s(): LPI not supported", __func__);

	gicd = alloc (sizeof *gicd);
	gicd->base_phys = base;
	gicd->nids = 1 << GICD_TYPER_ID_BITS (v);
	/* If LPI_BITS is 1 (Raw LPI_BITS is 0), calculate from ID_BITS */
	if (GICD_TYPER_LPI_BITS (v) == 1)
		gicd->n_lpis = gicd->nids - GIC_LPI_START;
	else
		gicd->n_lpis = 1 << GICD_TYPER_LPI_BITS (v);
	gicd->its_support = true;

	printf ("GICD base 0x%llX\n", gicd->base_phys);
	printf ("GICD total INTID %u\n", gicd->nids);
	printf ("GICD n_lpis %u\n", gicd->n_lpis);
	if (gicd->nids > GIC_N_INITD_WATERMARK)
		printf ("%s(): total INTID numbers more that %u\n", __func__,
			GIC_N_INITD_WATERMARK);
}

static void
acpi_madt_handle_gic_its (struct acpi_ic_header *h)
{
	struct acpi_gic_its *entry;
	phys_t base;

	entry = (struct acpi_gic_its *)h;
	base = entry->phys_addr;
	ASSERT (base);

	gic_its_init (base);
}

static void
acpi_madt_walk (struct acpi_madt *m)
{
	struct acpi_ic_header *h;
	u64 ic_size, ic_offset;

	ic_offset = offsetof (struct acpi_madt, ics);

	/* In the first pass, search for GICD first for ITS and LPI limit */
	ic_size = m->header.length - ic_offset;
	h = (struct acpi_ic_header *)&m->ics;
	while (ic_size) {
		if (h->length <= ic_size && h->type == ACPI_IC_TYPE_GICD)
			acpi_madt_handle_gicd (h);
		ic_size -= h->length;
		h = (struct acpi_ic_header *)((u8 *)h + h->length);
	}

	if (!gicd)
		panic ("%s(): GICD not initialized", __func__);

	/* In the second pass, search for ITS record and initialize */
	ic_size = m->header.length - ic_offset;
	h = (struct acpi_ic_header *)&m->ics;
	while (ic_size) {
		if (h->length <= ic_size && h->type == ACPI_IC_TYPE_GIC_ITS)
			acpi_madt_handle_gic_its (h);
		ic_size -= h->length;
		h = (struct acpi_ic_header *)((u8 *)h + h->length);
	}

	if (!its)
		panic ("%s(): GIC-ITS not initialized", __func__);
}

static void
gic_init (void)
{
	struct acpi_madt *m;

	m = acpi_find_entry (ACPI_MADT_SIGNATURE);
	if (!m)
		panic ("ACPI MADT not found\n");

	acpi_madt_walk (m);
}

static void
gic_pcpu_lr_list_init (void)
{
	struct pcpu *currentcpu = tpidr_get_pcpu ();
	struct gic_lr_list *l;
	uint i;

	LIST1_HEAD_INIT (currentcpu->int_freelist);
	LIST1_HEAD_INIT (currentcpu->int_pending);
	for (i = 0; i < INITIAL_FREE_LR; i++) {
		l = alloc (sizeof *l);
		LIST1_PUSH (currentcpu->int_freelist, l);
	}
}

static void
gic_intr_off (void)
{
	msr (GIC_ICC_IGRPEN0_EL1, 0x0);
	msr (GIC_ICC_IGRPEN1_EL1, 0x0);
}

INITFUNC ("acpi0", gic_init);
INITFUNC ("pcpu0", gic_pcpu_lr_list_init);
INITFUNC ("panic_dump_done0", gic_intr_off);
