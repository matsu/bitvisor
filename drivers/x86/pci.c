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
#include <core/acpi.h>
#include <core/ap.h>
#include <pci.h>
#include "../pci_internal.h"

void
pci_arch_find_devices_end (void)
{
	acpi_dmar_done_pci_device ();
}

void
pci_arch_msi_to_ipi (pci_config_address_t pci_config_addr,
		     const struct mm_as *as, u32 maddr, u32 mupper, u16 mdata)
{
	u64 icr = mm_as_msi_to_icr (as, maddr, mupper, mdata);
	send_ipi (icr);
}

int
pci_arch_msi_callback (void *data, int num)
{
	struct pci_msi_callback *p;

	if (num < 0x10)
		return num;
	int hit = 0;
	int ok = 0;
	for (p = pci_msi_callback_list; p; p = p->next) {
		if (!p->enable)
			continue;
		u64 icr = mm_as_msi_to_icr (p->pci_device->as_dma, p->maddr,
					    p->mupper, p->mdata);
		if (!~icr)
			/* Invalid address */
			continue;
		if ((icr & 0xFF) != num)
			/* Vector is different */
			continue;
		if ((icr & 0x700) > 0x100)
			/* Delivery Mode is not Fixed Mode or Lowest
			 * Priority */
			continue;
		if (!is_icr_destination_me (icr))
			/* Not to me */
			continue;
		hit++;
		if (p->callback (p->pci_device, p->data))
			ok++;
	}
	if (hit != ok) {
		if (!ok) {
			eoi ();
			return -1;
		}
		printf ("MSI(0x%02X): %d callbacks in %d callbacks"
			" wants to drop.\n", num, hit - ok, hit);
	}
	return num;
}

void
pci_iommu_arch_force_map (struct pci_device *dev)
{
	if (dev->as_dma != &dev->as_dma_dmar)
		return;
	acpi_dmar_force_map (dev->dmar_info, dev->initial_bus_no,
			     dev->address.device_no, dev->address.func_no);
}
