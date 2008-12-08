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

static u32 pci_get_base_address_mask(pci_config_address_t addr)
{
	u32 tmp, mask;

	tmp = pci_read_config_data32_without_lock(addr, 0);
	pci_write_config_data_port_without_lock(0xFFFFFFFF);
	mask = pci_read_config_data_port_without_lock();
	pci_write_config_data_port_without_lock(tmp);
	return mask;
}

static void pci_save_base_address_masks(struct pci_device *dev)
{
	int i;
	pci_config_address_t addr = dev->address;

	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		addr.reg_no = PCI_CONFIG_ADDRESS_GET_REG_NO(base_address) + i;
		dev->base_address_mask[i] = pci_get_base_address_mask(addr);
	}
	addr.reg_no = PCI_CONFIG_ADDRESS_GET_REG_NO(ext_rom_base);
	dev->base_address_mask[6] = pci_get_base_address_mask(addr);
}

static void pci_read_config_space(struct pci_device *dev)
{
	int i;
	pci_config_address_t addr = dev->address;
	struct pci_config_space *cs = &dev->config_space;

//	for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++) {
	for (i = 0; i < 16; i++) {
		addr.reg_no = i;
		cs->regs32[i] = pci_read_config_data32_without_lock(addr, 0);
	}
}

static struct pci_device *pci_new_device(pci_config_address_t addr)
{
	struct pci_device *dev;

	dev = alloc_pci_device();
	if (dev != NULL) {
		memset(dev, 0, sizeof(*dev));
		dev->driver = NULL;
		dev->address = addr;
		pci_read_config_space(dev);
		pci_save_base_address_masks(dev);
		pci_append_device(dev);
	}
	return dev;
}

static void pci_find_devices()
{
	int bn, dn, fn, num = 0;
	struct pci_device *dev;
	pci_config_address_t addr;
	u16 data;

	printf("PCI: finding devices ");
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
		printf("."); num++; 

		if (fn == 0 && dev->config_space.multi_function == 0)
			break;
	    }
	pci_restore_config_addr();
	printf(" %d devices found\n", num);
	return;

oom:
	panic_oom();
}

static void pci_init()
{
	pci_find_devices();
	core_io_register_handler(PCI_CONFIG_ADDR_PORT, 1, pci_config_addr_handler, NULL,
				 CORE_IO_PRIO_HIGH, driver_name);
	core_io_register_handler(PCI_CONFIG_DATA_PORT, 4, pci_config_data_handler, NULL,
				 CORE_IO_PRIO_HIGH, driver_name);
	return;
}
DRIVER_INIT(pci_init);
