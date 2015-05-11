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

/**
 * @file	drivers/pci_init.c
 * @brief	PCI driver (init)
 * @author	T. Shinagawa
 */
#include "pci.h"
#include "pci_internal.h"
#include "pci_init.h"
#include <core/acpi.h>
#include <core/mmio.h>

static const char driver_name[] = "pci_driver";

DEFINE_ALLOC_FUNC(pci_device)

static pci_config_address_t pci_make_config_address(int bus, int dev, int fn, int reg)
{
	pci_config_address_t addr;

	addr.allow = 1;
	addr.reserved = 0;
	addr.bus_no = bus;
	addr.device_no = dev;
	addr.func_no = fn;
	addr.reg_no = reg;
	addr.type = 0;
	return addr;
}

static u32
pci_get_base_address_mask (struct pci_device *dev, pci_config_address_t addr)
{
	u32 tmp, mask;

	if (dev->config_mmio) {
		mask = 0xFFFFFFFF;
		pci_read_config_mmio (dev->config_mmio, addr.bus_no,
				      addr.device_no, addr.func_no,
				      addr.reg_no * 4, sizeof tmp, &tmp);
		pci_write_config_mmio (dev->config_mmio, addr.bus_no,
				       addr.device_no, addr.func_no,
				       addr.reg_no * 4, sizeof mask, &mask);
		pci_read_config_mmio (dev->config_mmio, addr.bus_no,
				      addr.device_no, addr.func_no,
				      addr.reg_no * 4, sizeof mask, &mask);
		pci_write_config_mmio (dev->config_mmio, addr.bus_no,
				       addr.device_no, addr.func_no,
				       addr.reg_no * 4, sizeof tmp, &tmp);
		return mask;
	}
	tmp = pci_read_config_data32_without_lock(addr, 0);
	pci_write_config_data_port_without_lock(0xFFFFFFFF);
	mask = pci_read_config_data_port_without_lock();
	pci_write_config_data_port_without_lock(tmp);
	return mask;
}

static void pci_save_base_address_masks(struct pci_device *dev)
{
	int i;
	bool flag64;
	pci_config_address_t addr = dev->address;

	dev->base_address_mask_valid = 0;
	if (dev->config_space.type != 0)
		return;
	flag64 = false;
	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		addr.reg_no = PCI_CONFIG_ADDRESS_GET_REG_NO(base_address) + i;
		dev->base_address_mask[i] =
			pci_get_base_address_mask (dev, addr);
		if (flag64) {
			/* The mask should be ~0. The mask will not be
			 * used. */
			flag64 = false;
			continue;
		}
		dev->base_address_mask_valid |= 1 << i;
		if ((dev->base_address_mask[i] &
		     (PCI_CONFIG_BASE_ADDRESS_SPACEMASK |
		      PCI_CONFIG_BASE_ADDRESS_TYPEMASK)) ==
		    (PCI_CONFIG_BASE_ADDRESS_MEMSPACE |
		     PCI_CONFIG_BASE_ADDRESS_TYPE64))
			flag64 = true;
	}
	addr.reg_no = PCI_CONFIG_ADDRESS_GET_REG_NO(ext_rom_base);
	dev->base_address_mask[6] = pci_get_base_address_mask (dev, addr);
}

static void pci_read_config_space(struct pci_device *dev)
{
	int i;
	pci_config_address_t addr = dev->address;
	struct pci_config_space *cs = &dev->config_space;

	if (dev->config_mmio) {
		for (i = 0; i < 16; i++)
			pci_read_config_mmio (dev->config_mmio, addr.bus_no,
					      addr.device_no, addr.func_no,
					      i * sizeof cs->regs32[0],
					      sizeof cs->regs32[i],
					      &cs->regs32[i]);
		return;
	}
//	for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++) {
	for (i = 0; i < 16; i++) {
		addr.reg_no = i;
		cs->regs32[i] = pci_read_config_data32_without_lock(addr, 0);
	}
}

static struct pci_config_mmio_data *
pci_search_config_mmio (u16 seg_group, u8 bus_no)
{
	struct pci_config_mmio_data *p;

	for (p = pci_config_mmio_data_head; p; p = p->next)
		if (p->seg_group == seg_group &&
		    p->bus_start <= bus_no && bus_no <= p->bus_end)
			return p;
	return NULL;
}

static struct pci_device *pci_new_device(pci_config_address_t addr)
{
	struct pci_device *dev;

	dev = alloc_pci_device();
	if (dev != NULL) {
		memset(dev, 0, sizeof(*dev));
		dev->driver = NULL;
		dev->address = addr;
		dev->config_mmio = pci_search_config_mmio (0, addr.bus_no);
		pci_read_config_space(dev);
		pci_save_base_address_masks(dev);
		pci_append_device(dev);
	}
	return dev;
}

struct pci_device *
pci_possible_new_device (pci_config_address_t addr,
			 struct pci_config_mmio_data *mmio)
{
	u16 data;
	struct pci_device *ret = NULL;

	if (mmio)
		pci_read_config_mmio (mmio, addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data, &data);
	else
		data = pci_read_config_data16_without_lock (addr, 0);
	if (data != 0xFFFF)
		ret = pci_new_device (addr);
	return ret;
}

static void pci_find_devices()
{
	int bn, dn, fn, num = 0;
	struct pci_device *dev;
	pci_config_address_t addr;
	u16 data;
	struct pci_driver *driver;

	printf ("PCI: finding devices...\n");
	pci_save_config_addr();
	for (bn = 0; bn < PCI_MAX_BUSES; bn++)
	  for (dn = 0; dn < PCI_MAX_DEVICES; dn++)
	    for (fn = 0; fn < PCI_MAX_FUNCS; fn++) {
		addr = pci_make_config_address(bn, dn, fn, 0);
		data = pci_read_config_data16_without_lock(addr, 0);
		if (data == 0xFFFF) /* not exist */
			continue;

		dev = pci_new_device(addr);
		if (dev == NULL)
			goto oom;
		num++;

		driver = pci_find_driver_for_device (dev);
		if (driver) {
			dev->driver = driver;
			driver->new (dev);
		}

		if (fn == 0 && dev->config_space.multi_function == 0)
			break;
	    }
	pci_restore_config_addr();
	printf ("PCI: %d devices found\n", num);
	pci_dump_pci_dev_list ();
	return;

oom:
	panic_oom();
}

static void
pci_read_mcfg (void)
{
	uint n;
	struct pci_config_mmio_data d, *tmp, **pnext;

	pnext = &pci_config_mmio_data_head;
	*pnext = NULL;
	for (n = 0; acpi_read_mcfg (n, &d.base, &d.seg_group, &d.bus_start,
				    &d.bus_end); n++) {
		d.next = NULL;
		d.phys = d.base + (d.bus_start << 20);
		d.len = ((u32)(d.bus_end - d.bus_start) + 1) << 20;
		d.map = mapmem_hphys (d.phys, d.len, MAPMEM_WRITE |
				      MAPMEM_PCD | MAPMEM_PWT);
		tmp = alloc (sizeof *tmp);
		memcpy (tmp, &d, sizeof *tmp);
		*pnext = tmp;
		pnext = &tmp->next;
	}
}

static void
pci_mcfg_register_handler (void)
{
	uint n;
	struct pci_config_mmio_data *p;

	for (n = 0, p = pci_config_mmio_data_head; p; n++, p = p->next) {
		printf ("MCFG [%u] %04X:%02X-%02X (%llX,%X)\n",
			n, p->seg_group, p->bus_start, p->bus_end, p->phys,
			p->len);
		mmio_register_unlocked (p->phys, p->len,
					pci_config_mmio_handler, p);
	}
}

static void pci_init()
{
	pci_read_mcfg ();
	pci_find_devices();
	core_io_register_handler(PCI_CONFIG_ADDR_PORT, 1, pci_config_addr_handler, NULL,
				 CORE_IO_PRIO_HIGH, driver_name);
	core_io_register_handler(PCI_CONFIG_DATA_PORT, 4, pci_config_data_handler, NULL,
				 CORE_IO_PRIO_HIGH, driver_name);
	pci_mcfg_register_handler ();
	return;
}

INITFUNC ("driver90", pci_init);
