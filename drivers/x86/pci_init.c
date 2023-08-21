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
#include <arch/pci_init.h>
#include <core/x86/acpi.h>
#include <core/mmio.h>
#include <pci.h>
#include "../pci_internal.h"

#define PTE_P_BIT			0x1
#define PTE_RW_BIT			0x2

static u64
dmar_translate (void *data, unsigned int *npages, u64 address)
{
	struct pci_device *dev = data;
	unsigned int i;
	u64 ret;
	u64 tmp;

	ret = acpi_dmar_translate (dev->dmar_info, dev->initial_bus_no,
				   dev->address.device_no,
				   dev->address.func_no, address);
	if (!(ret & PTE_P_BIT) || !(ret & PTE_RW_BIT))
		goto end;
	for (i = 1; i < *npages; i++) {
		tmp = acpi_dmar_translate (dev->dmar_info,
					   dev->initial_bus_no,
					   dev->address.device_no,
					   dev->address.func_no,
					   address + i * PAGESIZE);
		if (tmp != ret + i * PAGESIZE) {
			*npages = i;
			break;
		}
	}
end:
	return ret;
}

static u64
dmar_msi_to_icr (void *data, u32 maddr, u32 mupper, u16 mdata)
{
	struct pci_device *dev = data;

	return acpi_dmar_msi_to_icr (dev->dmar_info, maddr, mupper, mdata);
}

static u64
virtual_dmar_translate (void *data, unsigned int *npages, u64 address)
{
	struct pci_virtual_device *dev = data;
	unsigned int i;
	u64 ret;
	u64 tmp;

	ret = acpi_dmar_translate (dev->dmar_info, 0, dev->address.device_no,
				   dev->address.func_no, address);
	if (!(ret & PTE_P_BIT) || !(ret & PTE_RW_BIT))
		goto end;
	for (i = 1; i < *npages; i++) {
		tmp = acpi_dmar_translate (dev->dmar_info, 0,
					   dev->address.device_no,
					   dev->address.func_no,
					   address + i * PAGESIZE);
		if (tmp != ret + i * PAGESIZE) {
			*npages = i;
			break;
		}
	}
end:
	return ret;
}

static u64
virtual_dmar_msi_to_icr (void *data, u32 maddr, u32 mupper, u16 mdata)
{
	struct pci_virtual_device *dev = data;

	return acpi_dmar_msi_to_icr (dev->dmar_info, maddr, mupper, mdata);
}

static const struct mm_as *
do_init_as_dma (struct pci_device *dev, struct pci_device *pdev,
		struct acpi_pci_addr *next)
{
	struct acpi_pci_addr addr;

	addr.bus = pdev->initial_bus_no; /* -1 for hotplug */
	addr.dev = pdev->address.device_no;
	addr.func = pdev->address.func_no;
	addr.next = next;
	if (pdev->parent_bridge)
		return do_init_as_dma (dev, pdev->parent_bridge, &addr);
	dev->dmar_info = acpi_dmar_add_pci_device (0, &addr,
						   !!dev->bridge.yes);
	if (dev->dmar_info) {
		dev->as_dma_dmar.translate = dmar_translate;
		dev->as_dma_dmar.msi_to_icr = dmar_msi_to_icr;
		dev->as_dma_dmar.data = dev;
		return &dev->as_dma_dmar;
	}
	return as_passvm;
}

const struct mm_as *
pci_init_arch_as_dma (struct pci_device *dev, struct pci_device *pdev)
{
	return do_init_as_dma (dev, pdev, NULL);
}

const struct mm_as *
pci_init_arch_virtual_as_dma (struct pci_virtual_device *dev)
{
	struct acpi_pci_addr addr;

	ASSERT (!dev->address.bus_no);
	addr.bus = 0;
	addr.dev = dev->address.device_no;
	addr.func = dev->address.func_no;
	addr.next = NULL;
	dev->dmar_info = acpi_dmar_add_pci_device (0, &addr, false);
	if (dev->dmar_info) {
		dev->as_dma_dmar.translate = virtual_dmar_translate;
		dev->as_dma_dmar.msi_to_icr = virtual_dmar_msi_to_icr;
		dev->as_dma_dmar.data = dev;
		return &dev->as_dma_dmar;
	}
	return as_passvm;
}
