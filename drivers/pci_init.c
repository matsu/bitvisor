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
#include <core/disconnect.h>
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
pci_get_base_address_mask (struct pci_device *dev, uint offset)
{
	u32 tmp, mask;

	mask = 0xFFFFFFFF;
	pci_config_read (dev, &tmp, sizeof tmp, offset);
	pci_config_write (dev, &mask, sizeof mask, offset);
	pci_config_read (dev, &mask, sizeof mask, offset);
	pci_config_write (dev, &tmp, sizeof tmp, offset);
	return mask;
}

static void pci_save_base_address_masks(struct pci_device *dev)
{
	int i;
	bool flag64;
	uint offset;

	dev->base_address_mask_valid = 0;
	if (dev->config_space.type != 0)
		return;
	flag64 = false;
	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		offset = PCI_CONFIG_SPACE_GET_OFFSET (base_address[i]);
		dev->base_address_mask[i] =
			pci_get_base_address_mask (dev, offset);
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
	offset = PCI_CONFIG_SPACE_GET_OFFSET (ext_rom_base);
	dev->base_address_mask[6] = pci_get_base_address_mask (dev, offset);
}

static void pci_read_config_space(struct pci_device *dev)
{
	int i;
	struct pci_config_space *cs = &dev->config_space;

//	for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++) {
	for (i = 0; i < 16; i++) {
		pci_config_read (dev, &cs->regs32[i], sizeof cs->regs32[i],
				 PCI_CONFIG_SPACE_GET_OFFSET (regs32[i]));
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

static void
pci_save_bridge_info (struct pci_device *dev)
{
	dev->bridge.yes = 0;
	dev->bridge.initial_secondary_bus_no = -1;
	if ((dev->config_space.class_code & 0xFFFF00) == 0x060400) {
		/* The dev is a PCI bridge. */
		dev->bridge.yes = 1;
		dev->bridge.secondary_bus_no =
			dev->config_space.base_address[2] >> 8;
		dev->bridge.subordinate_bus_no =
			dev->config_space.base_address[2] >> 16;
		spinlock_init (&dev->bridge.callback_lock);
		dev->bridge.callback_list = NULL;
	}
}

static struct pci_device **
pci_bridge_from_bus_no (u8 bus_no)
{
	static struct pci_device *bridge_from_bus_no[PCI_MAX_BUSES];

	if (bus_no)
		return &bridge_from_bus_no[bus_no];
	return NULL;
}

struct pci_device *
pci_get_bridge_from_bus_no (u8 bus_no)
{
	struct pci_device **p = pci_bridge_from_bus_no (bus_no);
	struct pci_device *ret;

	if (!p)
		return NULL;
	ret = *p;
	if (!ret || !ret->bridge.yes || ret->bridge.secondary_bus_no != bus_no)
		return NULL;
	return ret;
}

void
pci_set_bridge_from_bus_no (u8 bus_no, struct pci_device *bridge)
{
	struct pci_device **p = pci_bridge_from_bus_no (bus_no);

	if (p)
		*p = bridge;
}

int
pci_reconnect_device (struct pci_device *dev, pci_config_address_t addr,
		      struct pci_config_mmio_data *mmio)
{
	u32 data0, data8;

	/* Read device ID, vendor ID and class code.  If the vendor ID
	 * is 0xFFFF, the device remains disconnected. */
	if (mmio) {
		pci_read_config_mmio (mmio, addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data0, &data0);
		if ((data0 & 0xFFFF) == 0xFFFF)
			return 0;
		pci_read_config_mmio (mmio, addr.bus_no, addr.device_no,
				      addr.func_no, 8, sizeof data8, &data8);
	} else {
		pci_read_config_pmio (addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data0, &data0);
		if ((data0 & 0xFFFF) == 0xFFFF)
			return 0;
		pci_read_config_pmio (addr.bus_no, addr.device_no,
				      addr.func_no, 8, sizeof data8, &data8);
	}
	dev->config_mmio = pci_search_config_mmio (0, dev->address.bus_no);
	/* Compare the read data with data stored in the dev
	 * structure. */
	if (dev->config_space.regs32[0] == data0 &&
	    dev->config_space.regs32[2] == data8) {
		printf ("[%02X:%02X.%X] %06X: %04X:%04X reconnected\n",
			dev->address.bus_no,
			dev->address.device_no,
			dev->address.func_no,
			dev->config_space.class_code,
			dev->config_space.vendor_id,
			dev->config_space.device_id);
		dev->disconnect = 0;
		if (dev->driver && dev->driver->reconnect)
			dev->driver->reconnect (dev);
		return 0;
	}
	/* The device has been changed.  If a driver has been loaded
	 * for the device, panic, because there is no unloading driver
	 * function. */
	if (dev->driver)
		panic ("[%02X:%02X.%X] cannot handle device change"
		       " %06X: %04X:%04X -> %06X: %04X:%04X",
		       dev->address.bus_no,
		       dev->address.device_no,
		       dev->address.func_no,
		       dev->config_space.class_code,
		       dev->config_space.vendor_id,
		       dev->config_space.device_id,
		       data8 >> 8, data0 & 0xFFFF, data0 >> 16);
	/* New device! */
	printf ("[%02X:%02X.%X] device change"
		" %06X: %04X:%04X -> %06X: %04X:%04X\n",
		dev->address.bus_no,
		dev->address.device_no,
		dev->address.func_no,
		dev->config_space.class_code,
		dev->config_space.vendor_id,
		dev->config_space.device_id,
		data8 >> 8, data0 & 0xFFFF, data0 >> 16);
	dev->disconnect = 0;
	pci_read_config_space (dev);
	pci_save_base_address_masks (dev);
	pci_save_bridge_info (dev);
	return 1;
}

static struct pci_device *pci_new_device(pci_config_address_t addr)
{
	struct pci_device *dev;

	dev = alloc_pci_device();
	if (dev != NULL) {
		memset(dev, 0, sizeof(*dev));
		dev->driver = NULL;
		dev->address = addr;
		if (addr.bus_no)
			dev->initial_bus_no = -1;
		else
			dev->initial_bus_no = 0;
		dev->config_mmio = pci_search_config_mmio (0, addr.bus_no);
		pci_config_pmio_enter ();
		pci_read_config_space(dev);
		pci_save_base_address_masks(dev);
		pci_save_bridge_info (dev);
		pci_config_pmio_leave ();
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
		pci_read_config_pmio (addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data, &data);
	if (data != 0xFFFF)
		ret = pci_new_device (addr);
	if (ret) {
		ret->hotplug = 1;
		ret->parent_bridge =
			pci_get_bridge_from_bus_no (ret->address.bus_no);
		if (ret->parent_bridge)
			ret->initial_bus_no = ret->parent_bridge->bridge.
				initial_secondary_bus_no;
	}
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
	pci_config_pmio_enter ();
	for (bn = 0; bn < PCI_MAX_BUSES; bn++)
		pci_set_bridge_from_bus_no (bn, NULL);
	for (bn = 0; bn < PCI_MAX_BUSES; bn++)
	  for (dn = 0; dn < PCI_MAX_DEVICES; dn++)
	    for (fn = 0; fn < PCI_MAX_FUNCS; fn++) {
		addr = pci_make_config_address(bn, dn, fn, 0);
		pci_read_config_pmio (addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data, &data);
		if (data == 0xFFFF) /* not exist */
			continue;

		dev = pci_new_device(addr);
		if (dev == NULL)
			goto oom;
		num++;

		dev->initial_bus_no = bn;
		if (dev->bridge.yes) {
			dev->bridge.initial_secondary_bus_no =
				dev->bridge.secondary_bus_no;
			pci_set_bridge_from_bus_no (dev->bridge.
						    secondary_bus_no, dev);
		}

		if (fn == 0 && dev->config_space.multi_function == 0)
			break;
	    }
	LIST_FOREACH (pci_device_list, dev) {
		dev->parent_bridge =
			pci_get_bridge_from_bus_no (dev->address.bus_no);
		driver = pci_find_driver_for_device (dev);
		if (driver) {
			dev->driver = driver;
			driver->new (dev);
		}
	}
	pci_config_pmio_leave ();
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

/* Disconnect the device from a firmware driver */
void
pci_system_disconnect (struct pci_device *pci_device)
{
	disconnect_pcidev_driver (0, pci_device->address.bus_no,
				  pci_device->address.device_no,
				  pci_device->address.func_no);
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
