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
struct pci_config_mmio_data *pci_config_mmio_data_head;

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

static bool
pci_config_mmio_emulate_base_address_mask (struct pci_device *dev,
					   unsigned int reg_offset, bool wr,
					   union mem *data, uint len)
{
	int index = (reg_offset - 0x10) >> 2;

	if (reg_offset < 0x10)
		return false;
	if (! ((0 <= index && index <= 5) || index == 8) )
		return false;
	if (index == 8)		/* expansion ROM base address */
		index -= 2;
	if (wr) {
		if (len == 4 && (data->dword & 0xFFFFFFF0) == 0xFFFFFFF0) {
			BIT_SET (dev->in_base_address_mask_emulation, index);
			return true;
		} else {
			BIT_CLEAR (dev->in_base_address_mask_emulation, index);
		}
	} else {
		if (len == 4 &&
		    BIT_TEST (dev->in_base_address_mask_emulation, index)) {
			data->dword = dev->base_address_mask[index];
			return true;
		}
	}
	return false;
}

int pci_config_data_handler(core_io_t io, union mem *data, void *arg)
{
	int ioret = CORE_IO_RET_DEFAULT;
	struct pci_device *dev;
	pci_config_address_t caddr, caddr0;
	u8 offset;
	int (*func) (struct pci_device *dev, u8 iosize, u16 offset,
		     union mem *data);
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
		ioret = func (dev, io.size, offset, data);
	if (ioret == CORE_IO_RET_DEFAULT && dev && dev->config_mmio) {
		pci_readwrite_config_mmio (dev->config_mmio,
					   io.dir == CORE_IO_DIR_OUT,
					   dev->address.bus_no,
					   dev->address.device_no,
					   dev->address.func_no,
					   offset, io.size, data);
		ioret = CORE_IO_RET_DONE;
	}
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

void
pci_readwrite_config_mmio (struct pci_config_mmio_data *p, bool wr,
			   uint bus_no, uint device_no, uint func_no,
			   uint offset, uint iosize, void *data)
{
	u8 *q = p->map;
	u64 phys = p->base;

	phys += (bus_no << 20) + (device_no << 15) + (func_no << 12) + offset;
	if (phys < p->phys || phys + iosize > p->phys + p->len)
		panic ("pci_readwrite_config_mmio: error"
		       " base=0x%llX seg_group=0x%X"
		       " bus_start=0x%X bus_end=0x%X"
		       " phys=0x%llX len=0x%X"
		       " bus_no=0x%X device_no=0x%X func_no=0x%X offset=0x%X",
		       p->base, p->seg_group, p->bus_start, p->bus_end,
		       p->phys, p->len, bus_no, device_no, func_no, offset);
	q += phys - p->phys;
	if (wr)
		memcpy (q, data, iosize);
	else
		memcpy (data, q, iosize);
}

void
pci_read_config_mmio (struct pci_config_mmio_data *p, uint bus_no,
		      uint device_no, uint func_no, uint offset, uint iosize,
		      void *data)
{
	pci_readwrite_config_mmio (p, false, bus_no, device_no, func_no,
				   offset, iosize, data);
}

void
pci_write_config_mmio (struct pci_config_mmio_data *p, uint bus_no,
		       uint device_no, uint func_no, uint offset, uint iosize,
		       void *data)
{
	pci_readwrite_config_mmio (p, true, bus_no, device_no, func_no,
				   offset, iosize, data);
}

/**
 * @brief		
 */
void
pci_handle_default_config_write (struct pci_device *pci_device, u8 iosize,
				 u16 offset, union mem *data)
{
	u32 reg;
	core_io_t io;

	if (pci_device->config_mmio) {
		pci_write_config_mmio (pci_device->config_mmio,
				       pci_device->address.bus_no,
				       pci_device->address.device_no,
				       pci_device->address.func_no,
				       offset, iosize, data);
		if (offset >= 256)
			return;
		pci_read_config_mmio (pci_device->config_mmio,
				      pci_device->address.bus_no,
				      pci_device->address.device_no,
				      pci_device->address.func_no,
				      offset & ~3, 4, &reg);
		pci_device->config_space.regs32[offset >> 2] = reg;
		return;
	}
	if (offset >= 256)
		panic ("pci_handle_default_config_write: offset %u >= 256",
		       offset);
	io.port = PCI_CONFIG_DATA_PORT + (offset & 3);
	io.dir = CORE_IO_DIR_OUT;
	io.size = iosize;
	core_io_handle_default(io, data);
	reg = pci_read_config_data_port();
	pci_device->config_space.regs32[offset >> 2] = reg;
}

void
pci_handle_default_config_read (struct pci_device *pci_device, u8 iosize,
				u16 offset, union mem *data)
{
	core_io_t io;

	if (pci_device->config_mmio) {
		pci_read_config_mmio (pci_device->config_mmio,
				      pci_device->address.bus_no,
				      pci_device->address.device_no,
				      pci_device->address.func_no,
				      offset, iosize, data);
		return;
	}
	if (offset >= 256)
		panic ("pci_handle_default_config_read: offset %u >= 256",
		       offset);
	io.port = PCI_CONFIG_DATA_PORT + (offset & 3);
	io.dir = CORE_IO_DIR_IN;
	io.size = iosize;
	core_io_handle_default (io, data);
}

int
pci_config_mmio_handler (void *data, phys_t gphys, bool wr, void *buf,
			 uint len, u32 flags)
{
	struct pci_config_mmio_data *d = data;
	union {
		struct {
			unsigned int reg_offset : 12;
			unsigned int func_no : 3;
			unsigned int dev_no : 5;
			unsigned int bus_no : 8;
		} s;
		phys_t offset;
	} addr;
	enum core_io_ret ioret;
	struct pci_device *dev;
	int (*func) (struct pci_device *dev, u8 iosize, u16 offset,
		     union mem *data);

	addr.offset = gphys - d->base;
	spinlock_lock (&pci_config_lock);
	LIST_FOREACH (pci_device_list, dev) {
		if (dev->address.bus_no == addr.s.bus_no &&
		    dev->address.device_no == addr.s.dev_no &&
		    dev->address.func_no == addr.s.func_no) {
			spinlock_unlock (&pci_config_lock);
			goto found;
		}
		if (addr.s.func_no != 0 &&
		    dev->address.bus_no == addr.s.bus_no &&
		    dev->address.device_no == addr.s.dev_no &&
		    dev->address.func_no == 0 &&
		    dev->config_space.multi_function == 0) {
			/* The guest OS is trying to access a PCI
			   configuration header of a single-function
			   device with function number 1 to 7. The
			   access will be concealed. */
			spinlock_unlock (&pci_config_lock);
			goto conceal;
		}
	}
	/* TODO: possible new device */
	spinlock_unlock (&pci_config_lock);
	if (!wr)
		memset (buf, 0xFF, len);
	return 1;
found:
	if (dev->conceal) {
	conceal:
		if (!wr)
			memset (buf, 0xFF, len);
		return 1;
	}
	if (dev->driver == NULL)
		return 0;
	if (dev->driver->options.use_base_address_mask_emulation) {
		if (pci_config_mmio_emulate_base_address_mask (dev, addr.s.
							       reg_offset, wr,
							       buf, len))
		    return 1;
	}
	func = wr ? dev->driver->config_write : dev->driver->config_read;
	if (func) {
		ioret = func (dev, len, addr.s.reg_offset, buf);
		return ioret == CORE_IO_RET_DONE;
	}
	return 0;
}
