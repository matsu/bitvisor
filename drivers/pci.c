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
 * @file	drivers/pci.c
 * @brief	PCI device and driver manager
 * @author	T. Shinagawa
 */

#include <common.h>
#include <core.h>
#include "pci.h"

#define PCI_MAX_BUSES		256
#define PCI_MAX_DEVICES		32
#define PCI_MAX_FUNCS		8

#define PCI_CONFIG_ADDR_PORT	0x0CF8
#define PCI_CONFIG_DATA_PORT	0x0CFC

static const char driver_name[] = "pci_generic_driver";
static pci_config_address_t config_addr_reg;
static spinlock_t config_data_reg_lock;

LIST_DEFINE_HEAD(pci_device_list);
LIST_DEFINE_HEAD(pci_driver_list);

DEFINE_ALLOC_FUNC(pci_device)

static void pci_print_device(pci_config_address_t addr, struct pci_config_space *cs)
{
//	int i;
	printf("%02x:%02x.%1d %02x:%02x:%02x(%x) %04x:%04x %04x,%04x\n",
	       addr.bus_num, addr.device_num, addr.func_num,
	       cs->base_class, cs->sub_class, cs->programming_interface, cs->class_code,
	       cs->vendor_id, cs->device_id,
	       cs->command, cs->status);
#if 0
	for (i = 0; i < PCI_CONFIG_REGS_NUM; i++)
		printf("%08x ", cs->regs[i]);
	printf("\n");
#endif
}

/********************************************************************************
 * PCI driver registration
 ********************************************************************************/
/**
 * @brief		register a PCI device driver
 * @param  driver	a pointer to struct pci_driver
 */
void pci_register_driver(struct pci_driver *driver)
{
	struct pci_device *dev;

	LIST_APPEND(pci_driver_list, driver);
	LIST_FOREACH (pci_device_list, dev) {
		u32 id = dev->config_space.regs32[0];
		u32 class = dev->config_space.class_code;

		if (idmask_match(id, driver->id) && idmask_match(class, driver->class)) {
			dev->driver = driver;
			pci_print_device(dev->address, &dev->config_space);
			driver->new(dev);
		}
	}
	printf("%s registered\n", driver->longname);
	return;
}

/********************************************************************************
 * PCI configuration space register access
 ********************************************************************************/
#define DEFINE_pci_read_config_data_without_lock(size)			\
	static inline u##size __pci_read_config_data##size(pci_config_address_t addr, int offset) \
	{								\
		u##size data;						\
		out32(PCI_CONFIG_ADDR_PORT, addr.value);		\
		in##size(PCI_CONFIG_DATA_PORT + offset, &data);		\
		return data;						\
	}

#define DEFINE_pci_write_config_data_without_lock(size)					\
	static inline void __pci_write_config_data##size(pci_config_address_t addr, int offset, u##size data) \
	{								\
		out32(PCI_CONFIG_ADDR_PORT, addr.value);		\
		out##size(PCI_CONFIG_DATA_PORT + offset, data);		\
	}
DEFINE_pci_read_config_data_without_lock(8)
DEFINE_pci_read_config_data_without_lock(16)
DEFINE_pci_read_config_data_without_lock(32)
DEFINE_pci_write_config_data_without_lock(8)
DEFINE_pci_write_config_data_without_lock(16)
DEFINE_pci_write_config_data_without_lock(32)

#define DEFINE_pci_read_config_data(size)				\
	u##size pci_read_config_data##size(pci_config_address_t addr, int offset) \
	{								\
		u##size data;						\
		spinlock_lock(&config_data_reg_lock);			\
		data = __pci_read_config_data##size(addr, offset);	\
		out32(PCI_CONFIG_ADDR_PORT, config_addr_reg.value);	\
		spinlock_unlock(&config_data_reg_lock);			\
		return data;						\
	}
#define DEFINE_pci_write_config_data(size)				\
	void pci_write_config_data##size(pci_config_address_t addr, int offset, u##size data) \
	{								\
		spinlock_lock(&config_data_reg_lock);			\
		__pci_write_config_data##size(addr, offset, data);	\
		out32(PCI_CONFIG_ADDR_PORT, config_addr_reg.value);	\
		spinlock_unlock(&config_data_reg_lock);			\
	}
DEFINE_pci_read_config_data(8)
DEFINE_pci_read_config_data(16)
DEFINE_pci_read_config_data(32)
DEFINE_pci_write_config_data(8)
DEFINE_pci_write_config_data(16)
DEFINE_pci_write_config_data(32)

/**
 * @brief		
 */
u32 pci_read_config_data_port()
{
	u32 data;
	spinlock_lock(&config_data_reg_lock);
	in32(PCI_CONFIG_DATA_PORT, &data);
	spinlock_unlock(&config_data_reg_lock);
	return data;
}

/**
 * @brief		
 * @param data
 */
void pci_write_config_data_port(u32 data)
{
	spinlock_lock(&config_data_reg_lock);
	out32(PCI_CONFIG_DATA_PORT, data);
	spinlock_unlock(&config_data_reg_lock);
}

/**
 * @brief		
 */
void pci_handle_default_config_write(struct pci_device *pci_device, core_io_t io, u8 offset, union mem *data)
{
	u32 reg;

	core_io_handle_default(io, data);
	reg = pci_read_config_data_port();
	pci_device->config_space.regs32[offset >> 2] = reg;
}

/********************************************************************************
 * PCI configuration space handler
 ********************************************************************************/

static int pci_config_data_handler(core_io_t io, union mem *data, void *arg)
{
	int ioret = CORE_IO_RET_DEFAULT;
	struct pci_device *dev;
	pci_config_address_t caddr;
	u8 offset;
	int (*func)(struct pci_device *dev, core_io_t io, u8 offset, union mem *data);

	if (config_addr_reg.allow == 0)
		return CORE_IO_RET_NEXT;	// not configration access

	offset = config_addr_reg.reg_num * sizeof(u32) + (io.port - PCI_CONFIG_DATA_PORT);
	caddr = config_addr_reg; caddr.reserved = caddr.reg_num = caddr.type = 0;
	LIST_FOREACH (pci_device_list, dev) {
		if (dev->address.value != caddr.value)
			continue;
		if (dev->driver == NULL)
			break;

		func = io.dir == CORE_IO_DIR_IN ? dev->driver->config_read : dev->driver->config_write;
		ioret = func(dev, io, offset, data);
	}
	return ioret;
}

static int pci_config_addr_handler(core_io_t io, union mem *data, void *arg)
{
	if (io.type == CORE_IO_TYPE_OUT32) {
		config_addr_reg.value = data->dword;
		return CORE_IO_RET_DEFAULT;
	}
	return CORE_IO_RET_NEXT;
}

/********************************************************************************
 * PCI initialization
 ********************************************************************************/
static inline pci_config_address_t pci_make_config_address(int bus, int dev, int fn, int reg)
{
	pci_config_address_t addr;

	addr.allow = 1;
	addr.reserved = 0;
	addr.bus_num = bus;
	addr.device_num = dev;
	addr.func_num = fn;
	addr.reg_num = reg;
	addr.type = 0;
	return addr;
}

static void pci_read_config_space(struct pci_device *dev)
{
	int i;
	pci_config_address_t addr = dev->address;
	struct pci_config_space *cs = &dev->config_space;

//	for (i = 0; i < PCI_CONFIG_REGS_NUM / sizeof(u32); i++) {
	for (i = 0; i < 16; i++) {
		addr.reg_num = i;
		cs->regs32[i] = __pci_read_config_data32(addr, 0);
	}
}

static struct pci_device *pci_new_device(pci_config_address_t addr)
{
	struct pci_device *dev;

	dev = alloc_pci_device();
	if (dev != NULL) {
		dev->driver = NULL;
		dev->address = addr;
		pci_read_config_space(dev);
		LIST_APPEND(pci_device_list, dev);
	}
	return dev;
}

/**
 * no lock, because the BSP does this at boot time
 */
static void pci_find_devices() __initcode__
{
	int bn, dn, fn, num = 0;
	struct pci_device *dev;
	pci_config_address_t addr;
	u16 data;

	printf("PCI: find devices ");
	in32(PCI_CONFIG_ADDR_PORT, &config_addr_reg.value); // save PCI config address register
	for (bn = 0; bn < PCI_MAX_BUSES; bn++)
	  for (dn = 0; dn < PCI_MAX_DEVICES; dn++)
	    for (fn = 0; fn < PCI_MAX_FUNCS; fn++) {
		addr = pci_make_config_address(bn, dn, fn, 0);
		data = __pci_read_config_data16(addr, 0);
		if (data == 0xFFFF) /* not exist */
			continue;

		dev = pci_new_device(addr);
		if (dev == NULL)
			goto oom;
		// pci_print_device(addr, &dev->config_space);
		printf("."); num++; 

		if (fn == 0 && dev->config_space.multi_function == 0)
			break;
	    }
	out32(PCI_CONFIG_ADDR_PORT, config_addr_reg.value); // restore PCI config address register
	printf(" %d devices found\n", num);
	return;

oom:
	panic_oom();
}

/**
 * @brief	PCI device init function
 *
 * automatically called by defining DRIVER_INIT
 */
void pci_init() __initcode__
{
	spinlock_init(&config_data_reg_lock);
	pci_find_devices();
	core_io_register_handler(PCI_CONFIG_ADDR_PORT, 1, pci_config_addr_handler, NULL, CORE_IO_PRIO_HIGH, driver_name);
	core_io_register_handler(PCI_CONFIG_DATA_PORT, 4, pci_config_data_handler, NULL, CORE_IO_PRIO_HIGH, driver_name);
	return;
}
DRIVER_INIT(pci_init);
