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

#include <arch/vmm_mem.h>
#include <core/mmio.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
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
#include "sleep.h"
#include "uefi.h"
#include "vmm_mem.h"
#include "wakeup.h"
#include "passthrough/vtd.h"
#include "passthrough/iodom.h"

#define DMAR_SIGNATURE		"DMAR"

#define NSLPTCACHE		7
#define DMAR_GLOBAL_STATUS_TES_BIT (1 << 31)
#define DMAR_GLOBAL_STATUS_IRES_BIT (1 << 25)
#define CAP_REG_SLLPS_21_BIT	0x400000000ULL

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

static struct dmar_pass vp;

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
	struct acpi_description_header *header;
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
	struct acpi_description_header *header;
	struct dmar_rmrr *q;
	phys_t start, limit;

	start = vmm_mem_start_phys ();
	limit = start + VMMSIZE_ALL - 1;
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
	q->region_base_address = start;
	q->region_limit_address = limit;
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
	struct acpi_description_header *header;
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
	phys_t end_address = vmm_mem_start_phys () + VMMSIZE_ALL - 1;
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
	phys_t address = vmm_mem_start_phys ();
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
	struct acpi_description_header *q;
	struct dmar_info *dmar = data;

	q = acpi_mapmem (entry, sizeof *q);
	if (memcmp (q->signature, DMAR_SIGNATURE, ACPI_SIGNATURE_LEN))
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
		addr = vmm_mem_alloc_uppermem (npages * PAGESIZE);
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
	acpi_itr_rsdt1_entry (get_dmar_address, &vp.rsdt1_dmar);
	acpi_itr_rsdt_entry (get_dmar_address, &vp.rsdt_dmar);
	acpi_itr_xsdt_entry (get_dmar_address, &vp.xsdt_dmar);
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
		struct acpi_description_header *header = vp.new_dmar;
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

static void
acpi_dmar_init (void)
{
	u64 dmar_address;
	struct acpi_ent_dmar *r;
	struct domain *create_dom ();

	r = acpi_find_entry (DMAR_SIGNATURE);
	if (!r) {
		printf ("ACPI DMAR not found.\n");
		iommu_detected = 0;
	} else {
		int i;
		printf ("ACPI DMAR found.\n");
		iommu_detected = 1;

		parse_dmar_bios_report (r);
		num_dom = 0;
		for (i = 0; i < MAX_IO_DOM; i++)
			dom_io[i] = create_dom (i);
		dmar_address = dmar_pass_through_prepare ();
		/* dmar_pass_through_prepare() uses acpi_mapmem() so
		 * apci_find_entry() must be called again. If
		 * call_uefi_boot_acpi_table_mod() succeeds, it
		 * modifies ACPI tables, so find_entry() must be
		 * called before the modification. */
		r = acpi_find_entry (DMAR_SIGNATURE);
		ASSERT (r);
		if (dmar_address)
			printf ("Installing a modified DMAR table"
				" at 0x%llx.\n", dmar_address);
		acpi_modify_table (DMAR_SIGNATURE, dmar_address);
		memset (r, 0, 4); /* Clear DMAR to prevent guest OS from
				     using iommu */
		disable_vtd (r);
	}
}

static void
acpi_init_pass (void)
{
	if (current == current->vcpu0)
		dmar_pass_through_mmio_register ();
}

INITFUNC ("acpi0", acpi_dmar_init);
INITFUNC ("pass5", acpi_init_pass);
