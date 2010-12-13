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
 * @brief	PCI driver (core)
 * @author	T. Shinagawa
 */

#include <common.h>
#include <core.h>
#include "pci.h"
#include "pci_internal.h"
#include "pci_conceal.h"

static spinlock_t pci_config_lock = SPINLOCK_INITIALIZER;
static pci_config_address_t current_config_addr;

/********************************************************************************
 * PCI internal interfaces
 ********************************************************************************/

LIST_DEFINE_HEAD(pci_device_list);
LIST_DEFINE_HEAD(pci_driver_list);

void pci_save_config_addr(void)
{
	in32(PCI_CONFIG_ADDR_PORT, &current_config_addr.value);
}

void pci_restore_config_addr(void)
{
	out32(PCI_CONFIG_ADDR_PORT, current_config_addr.value);
}

void pci_append_device(struct pci_device *dev)
{
	LIST_APPEND(pci_device_list, dev);
	// pci_print_device(addr, &dev->config_space);
}

#define BIT_SET(flag, index)	(flag |=  (1 << index))
#define BIT_CLEAR(flag, index)	(flag &= ~(1 << index))
#define BIT_TEST(flag, index)	(flag &   (1 << index))

static int pci_config_emulate_base_address_mask(struct pci_device *dev, core_io_t io, union mem *data)
{
	int index = current_config_addr.reg_no - PCI_CONFIG_ADDRESS_GET_REG_NO(base_address[0]);

	if (! ((0 <= index && index <= 5) || index == 8) )
		return CORE_IO_RET_DEFAULT;

	if (index == 8) // expansion ROM base address
		index -= 2;

	if (io.dir == CORE_IO_DIR_OUT) {
		if (io.size == 4 && (data->dword & 0xFFFFFFF0) == 0xFFFFFFF0) {
			BIT_SET(dev->in_base_address_mask_emulation, index);
			return CORE_IO_RET_DONE;
		} else {
			BIT_CLEAR(dev->in_base_address_mask_emulation, index);
		}
	} else {
		if (io.size == 4 && BIT_TEST(dev->in_base_address_mask_emulation, index)) {
			data->dword = dev->base_address_mask[index];
			return CORE_IO_RET_DONE;
		}
	}
	return CORE_IO_RET_DEFAULT;
}

int pci_config_data_handler(core_io_t io, union mem *data, void *arg)
{
	int ioret = CORE_IO_RET_DEFAULT;
	struct pci_device *dev;
	pci_config_address_t caddr, caddr0;
	u8 offset;
	int (*func)(struct pci_device *dev, core_io_t io, u8 offset, union mem *data);
	static spinlock_t config_data_lock = SPINLOCK_INITIALIZER;

	if (current_config_addr.allow == 0)
		return CORE_IO_RET_NEXT;	// not configration access

	func = NULL;
	spinlock_lock (&config_data_lock);
	spinlock_lock (&pci_config_lock);
	offset = current_config_addr.reg_no * sizeof(u32) + (io.port - PCI_CONFIG_DATA_PORT);
	caddr = current_config_addr; caddr.reserved = caddr.reg_no = caddr.type = 0;
	caddr0 = caddr, caddr0.func_no = 0;
	LIST_FOREACH (pci_device_list, dev) {
		if (dev->address.value == caddr.value) {
			spinlock_unlock (&pci_config_lock);
			goto found;
		}
		if (caddr.func_no != 0 && dev->address.value == caddr0.value &&
		    dev->config_space.multi_function == 0) {
			/* The guest OS is trying to access a PCI
			   configuration header of a single-function
			   device with function number 1 to 7. The
			   access will be concealed. */
			spinlock_unlock (&pci_config_lock);
			goto conceal;
		}
	}
	dev = pci_possible_new_device (caddr);
	pci_restore_config_addr ();
	spinlock_unlock (&pci_config_lock);
	if (dev && dev->conceal)
		goto found;
	if (dev) {
		u32 id = dev->config_space.regs32[0];
		u32 class = dev->config_space.class_code;
		struct pci_driver *driver;

		printf ("[%02X:%02X.%X] New PCI device found.\n",
			caddr.bus_no, caddr.device_no, caddr.func_no);
		LIST_FOREACH (pci_driver_list, driver) {
			if (idmask_match (id, driver->id) &&
			    idmask_match (class, driver->class)) {
				dev->driver = driver;
				driver->new (dev);
				goto found;
			}
		}
	}
	goto ret;
found:
	if (dev->conceal) {
	conceal:
		ioret = pci_conceal_config_data_handler (io, data, arg);
		goto ret;
	}
	if (dev->driver == NULL)
		goto ret;
	if (dev->driver->options.use_base_address_mask_emulation) {
		ioret = pci_config_emulate_base_address_mask(dev, io, data);
		if (ioret == CORE_IO_RET_DONE)
			goto ret;
	}

	func = io.dir == CORE_IO_DIR_IN ? dev->driver->config_read : dev->driver->config_write;
ret:
	spinlock_unlock (&config_data_lock);
	if (func)
		ioret = func(dev, io, offset, data);
	return ioret;
}

int pci_config_addr_handler(core_io_t io, union mem *data, void *arg)
{
	if (io.type == CORE_IO_TYPE_OUT32) {
		spinlock_lock(&pci_config_lock);
		current_config_addr.value = data->dword;
		pci_restore_config_addr();
		spinlock_unlock(&pci_config_lock);
		return CORE_IO_RET_DONE;
	}
	return CORE_IO_RET_NEXT;
}

/********************************************************************************
 * PCI service functions exported to PCI device drivers
 ********************************************************************************/
/* ------------------------------------------------------------------------------
   PCI driver registration
 ------------------------------------------------------------------------------ */

/**
 * @brief		PCI driver registration function
 * @param  driver	pointer to struct pci_driver
 */
void pci_register_driver(struct pci_driver *driver)
{
	struct pci_device *dev;

	LIST_APPEND(pci_driver_list, driver);
	LIST_FOREACH (pci_device_list, dev) {
		u32 id = dev->config_space.regs32[0];
		u32 class = dev->config_space.class_code;

		if (dev->conceal)
			continue;
		if (idmask_match(id, driver->id) && idmask_match(class, driver->class)) {
			dev->driver = driver;
			driver->new(dev);
		}
	}
	if (driver->longname)
		printf ("%s registered\n", driver->longname);
	return;
}

/* ------------------------------------------------------------------------------
   PCI configuration registers access
 ------------------------------------------------------------------------------ */

#define DEFINE_pci_read_config_data(size)				\
	u##size pci_read_config_data##size(pci_config_address_t addr, int offset) \
	{								\
		u##size data;						\
		spinlock_lock(&pci_config_lock);			\
		data = pci_read_config_data##size##_without_lock(addr, offset);	\
		pci_restore_config_addr();				\
		spinlock_unlock(&pci_config_lock);			\
		return data;						\
	}
#define DEFINE_pci_write_config_data(size)				\
	void pci_write_config_data##size(pci_config_address_t addr, int offset, u##size data) \
	{								\
		spinlock_lock(&pci_config_lock);			\
		pci_write_config_data##size##_without_lock(addr, offset, data);	\
		pci_restore_config_addr();				\
		spinlock_unlock(&pci_config_lock);			\
	}
DEFINE_pci_read_config_data(8)
DEFINE_pci_read_config_data(16)
DEFINE_pci_read_config_data(32)
DEFINE_pci_write_config_data(8)
DEFINE_pci_write_config_data(16)
DEFINE_pci_write_config_data(32)

/**
 * @brief	read the current value of the PCI configuration data register (address is set by the guest)
 */
u32 pci_read_config_data_port()
{
	u32 data;
	spinlock_lock(&pci_config_lock);
	data = pci_read_config_data_port_without_lock();
	spinlock_unlock(&pci_config_lock);
	return data;
}

/**
 * @brief	write data to the PCI configuration data register (address is set by the guest)
 * @param data	data to be written
 */
void pci_write_config_data_port(u32 data)
{
	spinlock_lock(&pci_config_lock);
	pci_read_config_data_port_without_lock(data);
	spinlock_unlock(&pci_config_lock);
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

