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
#include <arch/pci.h>
#include <arch/pci_init.h>
#include <core/acpi.h>
#include <core/mmio.h>
#include <core/uefiutil.h>
#include <pci.h>
#include "pci_init.h"
#include "pci_internal.h"
#include "pci_match.h"

static const char driver_name[] = "pci_driver";

DEFINE_ALLOC_FUNC (pci_device)

static pci_config_address_t
pci_make_config_address (int bus, int dev, int fn, int reg)
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

static void
pci_save_base_address_masks (struct pci_device *dev)
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

static void
pci_read_config_space (struct pci_device *dev)
{
	int i;
	struct pci_config_space *cs = &dev->config_space;

	/* for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++) { */
	for (i = 0; i < 16; i++) {
		pci_config_read (dev, &cs->regs32[i], sizeof cs->regs32[i],
				 PCI_CONFIG_SPACE_GET_OFFSET (regs32[i]));
	}
}

static struct pci_config_mmio_data *
pci_search_config_mmio (u32 seg_group, u8 bus_no)
{
	struct pci_segment *s;
	struct pci_config_mmio_data *p;

	s = pci_get_segment (seg_group);
	if (s) {
		p = s->mmio;
		if (p && p->bus_start <= bus_no && bus_no <= p->bus_end)
			return p;
	}
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
pci_bridge_from_bus_no (struct pci_segment *s, u8 bus_no)
{
	return bus_no ? &s->bridge_from_bus_no[bus_no] : NULL;
}

struct pci_device *
pci_get_bridge_from_bus_no (struct pci_segment *s, u8 bus_no)
{
	struct pci_device **p = pci_bridge_from_bus_no (s, bus_no);
	struct pci_device *ret;

	if (!p)
		return NULL;
	ret = *p;
	if (!ret || !ret->bridge.yes || ret->bridge.secondary_bus_no != bus_no)
		return NULL;
	return ret;
}

void
pci_set_bridge_from_bus_no (struct pci_segment *s, u8 bus_no,
			    struct pci_device *bridge)
{
	struct pci_device **p = pci_bridge_from_bus_no (s, bus_no);

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
	dev->config_mmio = pci_search_config_mmio (dev->segment->seg_no,
						   dev->address.bus_no);
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

static struct pci_device *
pci_new_device (struct pci_segment *s, pci_config_address_t addr)
{
	struct pci_device *dev;

	dev = alloc_pci_device ();
	if (dev != NULL) {
		memset (dev, 0, sizeof *dev);
		dev->driver = NULL;
		dev->address = addr;
		if (addr.bus_no)
			dev->initial_bus_no = -1;
		else
			dev->initial_bus_no = 0;
		dev->config_mmio = pci_search_config_mmio (s->seg_no,
							   addr.bus_no);
		dev->segment = s;
		dev->as_dma = as_passvm;
		pci_config_pmio_enter ();
		pci_read_config_space (dev);
		pci_save_base_address_masks (dev);
		pci_save_bridge_info (dev);
		pci_config_pmio_leave ();
		pci_append_device (s, dev);
	}
	return dev;
}

struct pci_device *
pci_possible_new_device (struct pci_segment *s, pci_config_address_t addr)
{
	u16 data;
	struct pci_device *ret = NULL;
	struct pci_config_mmio_data *mmio = s->mmio;

	if (mmio)
		pci_read_config_mmio (mmio, addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data, &data);
	else
		pci_read_config_pmio (addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data, &data);
	if (data != 0xFFFF)
		ret = pci_new_device (s, addr);
	if (ret) {
		ret->hotplug = 1;
		ret->parent_bridge =
			pci_get_bridge_from_bus_no (s, ret->address.bus_no);
		if (ret->parent_bridge)
			ret->initial_bus_no = ret->parent_bridge->bridge.
				initial_secondary_bus_no;
		ret->as_dma = pci_init_arch_as_dma (ret, ret);
	}
	return ret;
}

static struct pci_device *
pci_try_add_device (struct pci_segment *s, pci_config_address_t addr,
		    u32 *bus0_devs)
{
	u16 data;
	struct pci_device *dev;
	struct pci_config_mmio_data *mmio = s->mmio;

	if (mmio)
		pci_read_config_mmio (mmio, addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data, &data);
	else
		pci_read_config_pmio (addr.bus_no, addr.device_no,
				      addr.func_no, 0, sizeof data, &data);
	if (data == 0xFFFF) /* not exist */
		return NULL;

	dev = pci_new_device (s, addr);
	if (!dev)
		panic ("Out of memory");
	if (addr.bus_no == 0)
		*bus0_devs |= 1 << addr.device_no;

	dev->initial_bus_no = addr.bus_no;
	if (dev->bridge.yes) {
		dev->bridge.initial_secondary_bus_no =
			dev->bridge.secondary_bus_no;
		pci_set_bridge_from_bus_no (s, dev->bridge.secondary_bus_no,
					    dev);
	}

	return dev;
}

static void
pci_find_devices_on_segment (struct pci_segment *s)
{
	int bn, dn, fn, num = 0;
	struct pci_device *dev;
	pci_config_address_t addr;
	struct pci_driver *driver;
	u32 bus0_devs = 0;
	int vnum = 0;
	char *pci_virtual;
	struct pci_virtual_device *virtual_device;

	for (bn = 0; bn < PCI_MAX_BUSES; bn++)
		pci_set_bridge_from_bus_no (s, bn, NULL);
	for (bn = 0; bn < PCI_MAX_BUSES; bn++) {
		for (dn = 0; dn < PCI_MAX_DEVICES; dn++) {
			for (fn = 0; fn < PCI_MAX_FUNCS; fn++) {
				addr = pci_make_config_address (bn, dn, fn, 0);
				dev = pci_try_add_device (s, addr, &bus0_devs);
				if (!dev)
					continue;
				num++;
				if (fn == 0 &&
				    dev->config_space.multi_function == 0)
					break;
			}
		}
	}
	LIST1_FOREACH (s->pci_device_list, dev)
		dev->parent_bridge =
			pci_get_bridge_from_bus_no (s, dev->address.bus_no);
	LIST1_FOREACH (s->pci_device_list, dev)
		dev->as_dma = pci_init_arch_as_dma (dev, dev);
	LIST1_FOREACH (s->pci_device_list, dev) {
		driver = pci_find_driver_for_device (dev);
		if (driver) {
			dev->driver = driver;
			driver->new (dev);
		}
	}
	pci_virtual = NULL;
	for (;;) {
		virtual_device = pci_match_get_virtual_device (&pci_virtual);
		if (!virtual_device)
			break;
		pci_assign_virtual_device (s, virtual_device, bus0_devs,
					   &dn, &fn);
		virtual_device->address =
			pci_make_config_address (0, dn, fn, 0);
		virtual_device->as_dma =
			pci_init_arch_virtual_as_dma (virtual_device);
		virtual_device->driver->new (virtual_device);
		vnum++;
	}
	printf ("PCI: %d devices found on PCI segment %u\n", num, s->seg_no);
	if (vnum)
		printf ("PCI: %d virtual devices created on PCI segment %u\n",
			vnum, s->seg_no);
	pci_dump_pci_dev_list (s);
}

static void
pci_find_devices (void)
{
	struct pci_segment *s;

	printf ("PCI: finding devices...\n");
	pci_pmio_save_config_addr ();
	pci_config_pmio_enter ();
	LIST1_FOREACH (pci_segment_list.head, s)
		pci_find_devices_on_segment (s);
	pci_arch_find_devices_end ();
	pci_config_pmio_leave ();
}

static struct pci_segment *
pci_segment_alloc (struct pci_config_mmio_data *d_to_clone)
{
	struct pci_segment *s;
	uint i;

	s = alloc (sizeof *s);
	LIST1_HEAD_INIT (s->pci_device_list);
	if (d_to_clone) {
		s->mmio = alloc (sizeof *s->mmio);
		memcpy (s->mmio, d_to_clone, sizeof *s->mmio);
		s->seg_no = d_to_clone->seg_group;
	} else {
		s->mmio = NULL;
		s->seg_no = 0; /* Default number from PCI Firmware spec */
	}
	for (i = 0; i < PCI_MAX_VIRT_DEVICES; i++)
		s->pci_virtual_devices[i] = NULL;
	for (i = 0; i < PCI_MAX_BUSES; i++)
		s->bridge_from_bus_no[i] = NULL;

	return s;
}

static bool
pci_find_segment (void)
{
	uint n;
	struct pci_segment *s;
	struct pci_config_mmio_data d;

	/*
	 * According to PCI Firmware specification, it allows only one entry
	 * per segment only. So, PCI segment and its MMIO is 1-to-1.
	 */
	for (n = 0; acpi_read_mcfg (n, &d.base, &d.seg_group, &d.bus_start,
				    &d.bus_end); n++) {
		d.phys = d.base + (d.bus_start << 20);
		d.len = ((u32)(d.bus_end - d.bus_start) + 1) << 20;
		d.map = mapmem_hphys (d.phys, d.len, MAPMEM_WRITE | MAPMEM_UC);
		s = pci_segment_alloc (&d);
		LIST1_ADD (pci_segment_list.head, s);
	}

	/*
	 * If there is no MMIO configuration found and PMIO exists, it means
	 * the machine has one PCI segment for legacy PCI bus.
	 */
	if (n == 0 && pci_arch_pmio_exist ()) {
		s = pci_segment_alloc (NULL);
		LIST1_ADD (pci_segment_list.head, s);
		n++;
	}

	if (n == 0)
		printf ("%s(): no PCI segment found\n", __func__);

	return n > 0;
}

static void
pci_mcfg_register_mmio_handler (void)
{
	uint n = 0;
	struct pci_segment *s;

	LIST1_FOREACH (pci_segment_list.head, s) {
		struct pci_config_mmio_data *p = s->mmio;
		if (p) {
			printf ("MCFG [%u] %04X:%02X-%02X (%llX,%X)\n",
				n, p->seg_group, p->bus_start, p->bus_end,
				p->phys, p->len);
			mmio_register_unlocked (p->phys, p->len,
						pci_config_mmio_handler, s);
			n++;
		}
	}
}

/* Disconnect the device from a firmware driver */
void
pci_system_disconnect (struct pci_device *pci_device)
{
	uefiutil_disconnect_pcidev_driver (pci_device->segment->seg_no,
					   pci_device->address.bus_no,
					   pci_device->address.device_no,
					   pci_device->address.func_no);
}

static void
pci_init (void)
{
	LIST1_HEAD_INIT (pci_segment_list.head);
	if (!pci_find_segment ())
		return;
	alloc2 (4, &pci_msi_dummyaddr);
	pci_find_devices ();
	if (core_io_arch_iospace_exist ()) {
		/* core_io involves with only segment 0 */
		struct pci_segment *s = pci_get_segment (0);
		core_io_register_handler (PCI_CONFIG_ADDR_PORT, 1,
					  pci_config_addr_handler, s,
					  CORE_IO_PRIO_HIGH, driver_name);
		core_io_register_handler (PCI_CONFIG_DATA_PORT, 4,
					  pci_config_data_handler, s,
					  CORE_IO_PRIO_HIGH, driver_name);
	}
	pci_mcfg_register_mmio_handler ();
}

INITFUNC ("driver90", pci_init);
