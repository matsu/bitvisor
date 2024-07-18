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

#include <arch/pci.h>
#include <core/aarch64/acpi.h>
#include <core/aarch64/gic.h>
#include <core/dres.h>
#include <pci.h>
#include "../pci_internal.h"

/*
 * Requester ID format
 * Bit [15:8] bus number
 * Bit [7:3] device number
 * Bit [2:0] function number
 */
#define PCI_ADDR_TO_RID(pci_config_address) \
	(((pci_config_address).value >> 8) & 0xFFFF)

void
pci_arch_find_devices_end (void)
{
	/* Do nothing */
}

void
pci_arch_msi_to_ipi (pci_config_address_t pci_config_addr,
		     const struct mm_as *as, u32 maddr, u32 mupper, u16 mdata)
{
	gic_its_int_set (PCI_ADDR_TO_RID (pci_config_addr), mdata);
}

int
pci_arch_msi_callback (void *data, int num)
{
	struct pci_msi_callback *p;
	u32 dev_id, event_id;
	int hit, ok;
	bool valid;

	if (num < GIC_LPI_START)
		return num;
	hit = 0;
	ok = 0;
	for (p = pci_msi_callback_list; p; p = p->next) {
		if (!p->enable)
			continue;
		dev_id = PCI_ADDR_TO_RID (p->pci_device->address);
		event_id = p->mdata;
		if (!gic_its_pintd_match (num, dev_id, event_id, &valid))
			continue; /* No mapping found */
		if (!valid)
			continue; /* Current interrupt mapping is invalid */
		hit++;
		if (p->callback (p->pci_device, p->data))
			ok++;
	}
	if (hit != ok) {
		/*
		 * The interrupt handler will deactivate the interrupt when
		 * a callback returns -1.
		 */
		if (!ok)
			return -1;
		printf ("MSI(0x%02X): %d callbacks in %d callbacks"
			" wants to drop.\n", num, hit - ok, hit);
	}
	return num;
}

void
pci_iommu_arch_force_map (struct pci_device *dev)
{
	/* Do nothing */
}

enum dres_err_t
pci_arch_dres_reg_translate (struct pci_device *dev, phys_t dev_addr,
			     size_t len, enum dres_reg_t dev_addr_type,
			     phys_t *cpu_addr, enum dres_reg_t *real_addr_type)
{
	bool translated;
	bool is_io_addr, is_io_addr_out;
	uint segment;

	segment = dev->segment->seg_no;
	is_io_addr = dev_addr_type == DRES_REG_TYPE_IO;
	translated = acpi_pci_addr_translate (segment, dev_addr, len,
					      is_io_addr, cpu_addr,
					      &is_io_addr_out);
	if (translated) {
		*real_addr_type = is_io_addr_out ? DRES_REG_TYPE_IO :
						   DRES_REG_TYPE_MM;
		if (*real_addr_type == DRES_REG_TYPE_IO) {
			/*
			 * This happens on QEMU. _TTP is not set on IO
			 * resources. There is no IO anyway. So, force set type
			 * to DRES_REG_TYPE_MM.
			 */
			*real_addr_type = DRES_REG_TYPE_MM;
		}
		return DRES_ERR_NONE;
	}

	/* TODO: Translation by devicetree */

	return DRES_ERR_ERROR;
}
