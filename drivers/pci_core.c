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
#include <core/exint_pass.h>
#include <core/process.h>
#include <core/strtol.h>
#include <token.h>
#include "pci.h"
#include "pci_internal.h"
#include "pci_match.h"

struct pci_msi {
	u8 cap;
	struct pci_device *dev;
};

struct pci_bridge_callback_list {
	struct pci_bridge_callback_list *next;
	struct pci_device *dev;
	struct pci_bridge_callback *callback;
};

enum addrlock_mode {
	ADDR_LOCK,
	ADDR_RESTORE_UNLOCK,
	ADDR_UNLOCK,
};

static void pci_config_pmio_addrlock (enum addrlock_mode mode);
static void pci_config_pmio_do (bool wr, pci_config_address_t addr,
				uint offset, uint iosize, union mem *data);

static spinlock_t pci_config_io_lock = SPINLOCK_INITIALIZER;
static pci_config_address_t current_config_addr;

/********************************************************************************
 * PCI internal interfaces
 ********************************************************************************/

LIST_DEFINE_HEAD(pci_device_list);
LIST_DEFINE_HEAD(pci_driver_list);
struct pci_config_mmio_data *pci_config_mmio_data_head;

void pci_save_config_addr(void)
{
	pci_config_pmio_addrlock (ADDR_LOCK);
	in32(PCI_CONFIG_ADDR_PORT, &current_config_addr.value);
	pci_config_pmio_addrlock (ADDR_UNLOCK);
}

void pci_append_device(struct pci_device *dev)
{
	LIST_APPEND(pci_device_list, dev);
	// pci_print_device(addr, &dev->config_space);
}

#define BIT_SET(flag, index)	(flag |=  (1 << index))
#define BIT_CLEAR(flag, index)	(flag &= ~(1 << index))
#define BIT_TEST(flag, index)	(flag &   (1 << index))

static bool
pci_config_emulate_base_address_mask (struct pci_device *dev,
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

void
pci_dump_pci_dev_list (void)
{
#ifdef DUMP_PCI_DEV_LIST
	struct pci_device *device;
	struct pci_driver *driver;
	int c, d, n;
	char *msg, *p;

	d = msgopen ("ttyin");
begin:
	msg = "-- more --  q:continue  g:go to 1st line  r:reboot"
		"  SPC:next page  CR:next line";
	printf ("\nDUMP_PCI_DEV_LIST:\n");
	n = 1;
	LIST_FOREACH (pci_device_list, device) {
		if (!--n && d >= 0) {
		wait:
			printf ("%s", msg);
			for (;;) {
				c = msgsendint (d, 0);
				if (device) {
					if (c == ' ') {
						n = 23;
						break;
					}
					if (c == '\r' || c == '\n') {
						n = 1;
						break;
					}
				}
				if (c == 'q') {
					n = -1;
					break;
				}
				if (c == 'g') {
					n = 0;
					break;
				}
				if (c == 'r') {
					c = msgopen ("reboot");
					if (c >= 0) {
						msgsendint (c, 0);
						msgclose (c);
					}
				}
			}
			for (p = msg; *p; p++)
				printf ("\b");
			for (p = msg; *p; p++)
				printf (" ");
			for (p = msg; *p; p++)
				printf ("\b");
			if (n < 0)
				goto end;
			if (!n)
				goto begin;
		}
		printf ("[%02X:%02X.%X] %06X: %04X:%04X",
			device->address.bus_no,
			device->address.device_no,
			device->address.func_no,
			device->config_space.class_code,
			device->config_space.vendor_id,
			device->config_space.device_id);
		LIST_FOREACH (pci_driver_list, driver)
			if (pci_match (device, driver))
				printf (" (%s)", driver->name);
		if (device->driver)
			printf (" %s", device->driver->name);
		printf ("\n");
	}
	msg = "-- end --   q:continue  g:go to 1st line  r:reboot";
	if (d >= 0)
		goto wait;
end:
	msgclose (d);
#endif
}

struct pci_driver *
pci_find_driver_for_device (struct pci_device *device)
{
	return pci_match_find_driver (device);
}

struct pci_driver *
pci_find_driver_by_token (struct token *name)
{
	struct pci_driver *driver;
	int i;

	LIST_FOREACH (pci_driver_list, driver) {
		if (!driver->name || driver->name[0] == '\0')
			continue;
		for (i = 0; &name->start[i] != name->end; i++)
			if (driver->name[i] != name->start[i])
				break;
		if (&name->start[i] == name->end && driver->name[i] == '\0')
			return driver;
	}
	return NULL;
}

static void
device_disconnect (struct pci_device *dev)
{
	struct pci_device *bridge;

	if (!dev->disconnect) {
		printf ("[%02X:%02X.%X] %06X: %04X:%04X disconnect\n",
			dev->address.bus_no,
			dev->address.device_no,
			dev->address.func_no,
			dev->config_space.class_code,
			dev->config_space.vendor_id,
			dev->config_space.device_id);
		dev->disconnect = 1;
		if (dev->driver && dev->driver->disconnect)
			dev->driver->disconnect (dev);
	}
	if (!dev->bridge.yes)
		return;
	bridge = dev;
	LIST_FOREACH (pci_device_list, dev) {
		if (dev->parent_bridge != bridge)
			continue;
		device_disconnect (dev);
	}
}

static void
pci_handle_bridge_pre_config_write (struct pci_device *bridge, u8 iosize,
				    u16 offset, union mem *data)
{
	struct pci_bridge_callback_list *list;
	struct pci_bridge_callback *c;
	int cmd = 0;
	u8 force_cmd = 0;

	if (offset <= 4 && offset + iosize > 4)
		cmd = 1;
	spinlock_lock (&bridge->bridge.callback_lock);
	for (list = bridge->bridge.callback_list; list; list = list->next) {
		c = list->callback;
		if (!c)		/* Never happen */
			continue;
		if (c->pre_config_write)
			c->pre_config_write (list->dev, bridge, iosize,
					     offset, data);
		if (cmd && c->force_command)
			force_cmd |= c->force_command (list->dev, bridge);
	}
	spinlock_unlock (&bridge->bridge.callback_lock);
	if (cmd) {
		bridge->fake_command_mask = force_cmd;
		bridge->fake_command_fixed = force_cmd;
	}
}

static void
pci_handle_bridge_post_config_write (struct pci_device *bridge, u8 iosize,
				     u16 offset, union mem *data)
{
	struct pci_bridge_callback_list *list;
	struct pci_bridge_callback *c;

	spinlock_lock (&bridge->bridge.callback_lock);
	for (list = bridge->bridge.callback_list; list; list = list->next) {
		c = list->callback;
		if (!c)		/* Never happen */
			continue;
		if (c->post_config_write)
			c->post_config_write (list->dev, bridge, iosize,
					      offset, data);
	}
	spinlock_unlock (&bridge->bridge.callback_lock);
}

static void
pci_handle_bridge_config_write (struct pci_device *bridge, u8 iosize,
				u16 offset, union mem *data)
{
	struct pci_device *dev;
	u8 old_secondary_bus_no = bridge->bridge.secondary_bus_no;
	u8 old_subordinate_bus_no = bridge->bridge.subordinate_bus_no;
	u8 new_secondary_bus_no = old_secondary_bus_no;
	u8 new_subordinate_bus_no = old_subordinate_bus_no;

	if (offset <= 0x19 && offset + iosize > 0x19)
		new_secondary_bus_no = (&data->byte)[0x19 - offset];
	if (offset <= 0x1A && offset + iosize > 0x1A)
		new_subordinate_bus_no = (&data->byte)[0x1A - offset];
	if (new_secondary_bus_no == old_secondary_bus_no &&
	    new_subordinate_bus_no == old_subordinate_bus_no)
		return;
	printf ("[%02X:%02X.%X] bridge bus_no change %02X-%02X -> %02X-%02X\n",
		bridge->address.bus_no,
		bridge->address.device_no,
		bridge->address.func_no,
		old_secondary_bus_no, old_subordinate_bus_no,
		new_secondary_bus_no, new_subordinate_bus_no);
	LIST_FOREACH (pci_device_list, dev) {
		if (dev->parent_bridge == bridge &&
		    new_secondary_bus_no != old_secondary_bus_no) {
			if (!dev->disconnect &&
			    dev->address.bus_no != old_secondary_bus_no)
				panic ("[%02X:%02X.%X] %06X: %04X:%04X"
				       " bus_no != %02X",
				       dev->address.bus_no,
				       dev->address.device_no,
				       dev->address.func_no,
				       dev->config_space.class_code,
				       dev->config_space.vendor_id,
				       dev->config_space.device_id,
				       old_secondary_bus_no);
			device_disconnect (dev);
			dev->address.bus_no = new_secondary_bus_no;
			/* Clear the dev->config_mmio because the new
			 * bus number may be out of range of current
			 * MCFG space.  It will be set again in
			 * pci_reconnect_device() if necessary. */
			dev->config_mmio = NULL;
		} else if (!dev->disconnect &&
			   old_secondary_bus_no &&
			   dev->address.bus_no > old_secondary_bus_no &&
			   dev->address.bus_no <= old_subordinate_bus_no &&
			   !(new_secondary_bus_no == old_secondary_bus_no &&
			     dev->address.bus_no > new_secondary_bus_no &&
			     dev->address.bus_no <= new_subordinate_bus_no)) {
			device_disconnect (dev);
		}
	}
	pci_set_bridge_from_bus_no (new_secondary_bus_no, bridge);
	bridge->bridge.secondary_bus_no = new_secondary_bus_no;
	bridge->bridge.subordinate_bus_no = new_subordinate_bus_no;
}

static void
pci_config_io_handler (struct pci_config_mmio_data *d, bool wr,
		       uint bus_no, uint device_no, uint func_no,
		       uint offset, uint len, void *buf)
{
	enum core_io_ret ioret;
	struct pci_device *dev;
	int (*func) (struct pci_device *dev, u8 iosize, u16 offset,
		     union mem *data);
	pci_config_address_t new_dev_addr;
	static struct pci_device *last_dev;
	static uint last_bus_no, last_device_no, last_func_no;

	spinlock_lock (&pci_config_io_lock);
	if (last_bus_no == bus_no && last_device_no == device_no &&
	    last_func_no == func_no && last_dev &&
	    last_dev->address.bus_no == bus_no &&
	    last_dev->address.device_no == device_no &&
	    last_dev->address.func_no == func_no) {
		/* Fast path */
		dev = last_dev;
		goto dev_found;
	}
	LIST_FOREACH (pci_device_list, dev) {
		if (dev->address.bus_no == bus_no &&
		    dev->address.device_no == device_no &&
		    dev->address.func_no == func_no) {
			last_bus_no = bus_no;
			last_device_no = device_no;
			last_func_no = func_no;
			last_dev = dev;
			goto dev_found;
		}
		if (func_no != 0 &&
		    dev->address.bus_no == bus_no &&
		    dev->address.device_no == device_no &&
		    dev->address.func_no == 0 &&
		    dev->config_space.multi_function == 0) {
			/* The guest OS is trying to access a PCI
			   configuration header of a single-function
			   device with function number 1 to 7. The
			   access will be concealed. */
			if (!wr)
				memset (buf, 0xFF, len);
			goto ret;
		}
	}
	new_dev_addr.value = 0;
	new_dev_addr.bus_no = bus_no;
	new_dev_addr.device_no = device_no;
	new_dev_addr.func_no = func_no;
	dev = pci_possible_new_device (new_dev_addr, d);
new_device:
	if (dev) {
		struct pci_driver *driver;

		printf ("[%02X:%02X.%X] New PCI device found.\n",
			bus_no, device_no, func_no);
		driver = pci_find_driver_for_device (dev);
		if (driver) {
			dev->driver = driver;
			driver->new (dev);
		}
		goto connected_dev_found;
	}
	/* Passthrough accesses to PCI configuration space of
	 * non-existent devices.  This behavior is apparently required
	 * for EFI variable access on iMac (Retina 5K, 27-inch, Late
	 * 2015) and MacBook (Retina, 12-inch, Early 2016). */
	if (d)
		pci_readwrite_config_mmio (d, wr, bus_no, device_no, func_no,
					   offset, len, buf);
	else
		pci_readwrite_config_pmio (wr, bus_no, device_no, func_no,
					   offset, len, buf);
	goto ret;
dev_found:
	if (dev->disconnect && pci_reconnect_device (dev, dev->address, d))
		goto new_device;
	if (dev->disconnect) {
		dev = NULL;
		goto new_device;
	}
connected_dev_found:
	if (dev->bridge.yes && wr)
		pci_handle_bridge_pre_config_write (dev, len, offset, buf);
	if (dev->bridge.yes && wr)
		pci_handle_bridge_config_write (dev, len, offset, buf);
	if (dev->driver == NULL)
		goto def;
	if (dev->driver->options.use_base_address_mask_emulation) {
		if (pci_config_emulate_base_address_mask (dev, offset, wr,
							  buf, len))
			goto ret2;
	}
	func = wr ? dev->driver->config_write : dev->driver->config_read;
	if (func) {
		ioret = func (dev, len, offset, buf);
		if (ioret == CORE_IO_RET_DONE)
			goto ret2;
	}
def:
	if (wr)
		pci_handle_default_config_write (dev, len, offset, buf);
	else
		pci_handle_default_config_read (dev, len, offset, buf);
ret2:
	if (dev->bridge.yes && wr)
		pci_handle_bridge_post_config_write (dev, len, offset, buf);
ret:
	spinlock_unlock (&pci_config_io_lock);
}

int pci_config_data_handler(core_io_t io, union mem *data, void *arg)
{
	pci_config_address_t caddr;
	u8 offset;

	pci_config_pmio_enter ();
	caddr = current_config_addr;
	if (!caddr.allow) {
		/* Not configuration access */
		pci_config_pmio_do (io.dir != CORE_IO_DIR_IN, caddr,
				    io.port - PCI_CONFIG_DATA_PORT, io.size,
				    data);
		goto leave;
	}
	offset = caddr.reg_no * sizeof(u32) + (io.port - PCI_CONFIG_DATA_PORT);
	pci_config_io_handler (NULL, io.dir != CORE_IO_DIR_IN, caddr.bus_no,
			       caddr.device_no, caddr.func_no, offset, io.size,
			       data);
leave:
	pci_config_pmio_leave ();
	return CORE_IO_RET_DONE;
}

int pci_config_addr_handler(core_io_t io, union mem *data, void *arg)
{
	switch (io.type) {
	case CORE_IO_TYPE_IN32:
	case CORE_IO_TYPE_IN16:
	case CORE_IO_TYPE_IN8:
		pci_config_pmio_addrlock (ADDR_LOCK);
		core_io_handle_default (io, data);
		pci_config_pmio_addrlock (ADDR_UNLOCK);
		break;
	case CORE_IO_TYPE_OUT32:
		pci_config_pmio_addrlock (ADDR_LOCK);
		current_config_addr.value = data->dword;
		pci_config_pmio_addrlock (ADDR_RESTORE_UNLOCK);
		break;
	case CORE_IO_TYPE_OUT16:
	case CORE_IO_TYPE_OUT8:
		pci_config_pmio_addrlock (ADDR_LOCK);
		core_io_handle_default (io, data);
		in32 (PCI_CONFIG_ADDR_PORT, &current_config_addr.value);
		/* Update last_addr */
		pci_config_pmio_addrlock (ADDR_RESTORE_UNLOCK);
		break;
	default:
		panic("pci_config_addr_handler: unknown iotype\n");
	}
	return CORE_IO_RET_DONE;
}

/* Set port range of a PCI bridge that the pci_device is connected to.
 * The length is 0x1000. */
void
pci_set_bridge_io (struct pci_device *pci_device)
{
	u32 tmp;
	u16 port_list = 3;
	struct pci_bar_info bar_info;
	struct pci_device *dev, *bridge = NULL;
	u8 bus_no_start, bus_no_end, port_start, port_end;

	if (!pci_device->address.bus_no)
		return;		/* No bridges are used for this
				 * device. */
	if (pci_device->hotplug)
		return;		/* Hot-plug devices should be
				 * configured by the guest operating
				 * system. */
	LIST_FOREACH (pci_device_list, dev) {
		if ((dev->config_space.class_code & 0xFFFF00) == 0x060400) {
			/* The dev is a PCI bridge. */
			tmp = dev->config_space.base_address[2];
			bus_no_start = tmp >> 8;
			bus_no_end = tmp >> 16;
			tmp = dev->config_space.base_address[3];
			port_start = (tmp >> 4) & 0xF;
			port_end = (tmp >> 12) & 0xF;
			if (bus_no_start <= pci_device->address.bus_no &&
			    bus_no_end >= pci_device->address.bus_no) {
				/* The dev is the bridge that the
				 * pci_device is connected to. */
				bridge = dev;
				if (port_start != 0xF) {
					/* Port already assigned by
					 * firmware. */
					printf ("[%02x:%02x.%01x]"
						" bridge [%02x:%02x.%01x]"
						" port assigned\n",
						pci_device->address.bus_no,
						pci_device->address.device_no,
						pci_device->address.func_no,
						bridge->address.bus_no,
						bridge->address.device_no,
						bridge->address.func_no);
					return;
				}
			}
			printf ("[%02x:%02x.%01x] port 0x%X000-0x%XFFF\n",
				dev->address.bus_no, dev->address.device_no,
				dev->address.func_no, port_start, port_end);
			port_list |= (1 << port_end) |
				(((1 << port_end) - 1) &
				 ~((1 << port_start) - 1));
		} else {
			for (tmp = 0; tmp < 6; tmp++) {
				pci_get_bar_info (dev, tmp, &bar_info);
				if (bar_info.type != PCI_BAR_INFO_TYPE_IO)
					continue;
				port_start = (bar_info.base >> 12) & 0xF;
				port_end = ((bar_info.base +
					     bar_info.len - 1) >> 12) & 0xF;
				printf ("[%02x:%02x.%01x]"
					" port 0x%X000-0x%XFFF\n",
					dev->address.bus_no,
					dev->address.device_no,
					dev->address.func_no,
					port_start, port_end);
				port_list |= (1 << port_end) |
					(((1 << port_end) - 1) &
					 ~((1 << port_start) - 1));
			}
		}
	}
	if (!bridge) {
		printf ("[%02x:%02x.%01x] bridge not found!\n",
			pci_device->address.bus_no,
			pci_device->address.device_no,
			pci_device->address.func_no);
		return;
	}
	for (tmp = 0; tmp <= 0xF; tmp++)
		if (!(port_list & (1 << tmp)))
			goto found;
	printf ("[%02x:%02x.%01x] bridge [%02x:%02x.%01x]"
		" port_list 0x%X no ports are available!\n",
		pci_device->address.bus_no, pci_device->address.device_no,
		pci_device->address.func_no,
		bridge->address.bus_no, bridge->address.device_no,
		bridge->address.func_no, port_list);
	return;
found:
	printf ("[%02x:%02x.%01x] bridge [%02x:%02x.%01x]"
		" port_list 0x%X use 0x%X000-0x%XFFF\n",
		pci_device->address.bus_no, pci_device->address.device_no,
		pci_device->address.func_no,
		bridge->address.bus_no, bridge->address.device_no,
		bridge->address.func_no, port_list, tmp, tmp);
	tmp = (tmp << 4) | 1;
	pci_config_write (bridge, &tmp, 1, 0x1C);
	pci_config_write (bridge, &tmp, 1, 0x1D);
}

void
pci_set_bridge_callback (struct pci_device *pci_device,
			 struct pci_bridge_callback *bridge_callback)
{
	struct pci_device *bridge;
	struct pci_bridge_callback_list **list, *p;

	for (bridge = pci_device->parent_bridge; bridge;
	     bridge = bridge->parent_bridge) {
		spinlock_lock (&bridge->bridge.callback_lock);
		for (list = &bridge->bridge.callback_list; (p = *list);
		     list = &p->next) {
			if (p->dev == pci_device) {
				*list = p->next;
				free (p);
				break;
			}
		}
		if (bridge_callback) {
			p = alloc (sizeof *p);
			p->dev = pci_device;
			p->callback = bridge_callback;
			p->next = *list;
			*list = p;
		}
		spinlock_unlock (&bridge->bridge.callback_lock);
	}
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
	LIST_APPEND(pci_driver_list, driver);
	if (driver->longname)
		printf ("%s registered\n", driver->longname);
	return;
}

void
pci_register_intr_callback (int (*callback) (void *data, int num), void *data)
{
	if (!callback || !data)
		return;

	exint_pass_intr_register_callback (callback, data);
}

/* ------------------------------------------------------------------------------
   PCI configuration registers access
 ------------------------------------------------------------------------------ */

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

static void
pci_config_pmio_do (bool wr, pci_config_address_t addr, uint offset,
		    uint iosize, union mem *data)
{
	static spinlock_t pmio_do_lock = SPINLOCK_INITIALIZER;
	static u32 last_addr;
	ioport_t data_port = PCI_CONFIG_DATA_PORT + (offset & 3);

	spinlock_lock (&pmio_do_lock);
	if (last_addr != addr.value) {
		out32 (PCI_CONFIG_ADDR_PORT, addr.value);
		last_addr = addr.value;
	}
	switch (iosize) {
	case 0:
		break;
	case 1:
		if (wr)
			out8 (data_port, data->byte);
		else
			in8 (data_port, &data->byte);
		break;
	case 2:
		if (wr)
			out16 (data_port, data->word);
		else
			in16 (data_port, &data->word);
		break;
	case 4:
		if (wr)
			out32 (data_port, data->dword);
		else
			in32 (data_port, &data->dword);
		break;
	}
	spinlock_unlock (&pmio_do_lock);
}

static inline void
pci_config_pmio_addrlock (enum addrlock_mode mode)
{
	static spinlock_t pmio_addr_lock = SPINLOCK_INITIALIZER;

	switch (mode) {
	case ADDR_LOCK:
		spinlock_lock (&pmio_addr_lock);
		break;
	case ADDR_RESTORE_UNLOCK:
		pci_config_pmio_do (false, current_config_addr, 0, 0, NULL);
		/* Fall through */
	case ADDR_UNLOCK:
		spinlock_unlock (&pmio_addr_lock);
		break;
	}
}

/*
  >0: addrlock if count is 0, then count++
  <0: count--, then restore addr and addrunlock if count is 0

  Note:
    Read current_config_addr while count > 0 or addrlock.
    Modify current_config_addr while count == 0 and addrlock.
    Use pci_config_pmio_do() while count > 0 or addrlock.
    Access addr or data port directly instead of using pci_config_pmio_do()
    while count == 0 and addrlock, and restore addr after modifying addr.
*/
static inline void
pci_config_pmio_count (int add)
{
	static spinlock_t pmio_count_lock = SPINLOCK_INITIALIZER;
	static int count;

	spinlock_lock (&pmio_count_lock);
	if (add > 0) {
		if (!count)
			pci_config_pmio_addrlock (ADDR_LOCK);
		count++;
	} else {
		count--;
		if (!count)
			pci_config_pmio_addrlock (ADDR_RESTORE_UNLOCK);
	}
	spinlock_unlock (&pmio_count_lock);
}

void
pci_config_pmio_enter (void)
{
	pci_config_pmio_count (1);
}

void
pci_config_pmio_leave (void)
{
	pci_config_pmio_count (-1);
}

void
pci_readwrite_config_pmio (bool wr, uint bus_no, uint device_no, uint func_no,
			   uint offset, uint iosize, void *data)
{
	pci_config_address_t addr;

	if (bus_no > 0xFF || device_no > 0x1F || func_no > 0x7 ||
	    offset > 0xFF || !(iosize == 1 || iosize == 2 || iosize == 4))
		panic ("pci_readwrite_config_pmio: invalid parameter"
		       " wr %d [%02X:%02X.%X] offset %02X iosize %X",
		       wr, bus_no, device_no, func_no, offset, iosize);
	addr.value = 0;
	addr.allow = 1;
	addr.bus_no = bus_no;
	addr.device_no = device_no;
	addr.func_no = func_no;
	addr.reg_no = offset >> 2;
	pci_config_pmio_enter ();
	pci_config_pmio_do (wr, addr, offset, iosize, data);
	pci_config_pmio_leave ();
}

void
pci_read_config_pmio (uint bus_no, uint device_no, uint func_no, uint offset,
		      uint iosize, void *data)
{
	pci_readwrite_config_pmio (false, bus_no, device_no, func_no, offset,
				   iosize, data);
}

void
pci_write_config_pmio (uint bus_no, uint device_no, uint func_no, uint offset,
		       uint iosize, void *data)
{
	pci_readwrite_config_pmio (true, bus_no, device_no, func_no, offset,
				   iosize, data);
}

void
pci_config_read (struct pci_device *pci_device, void *data, uint iosize,
		 uint offset)
{
	if (pci_device->config_mmio)
		pci_read_config_mmio (pci_device->config_mmio,
				      pci_device->address.bus_no,
				      pci_device->address.device_no,
				      pci_device->address.func_no,
				      offset, iosize, data);
	else
		pci_read_config_pmio (pci_device->address.bus_no,
				      pci_device->address.device_no,
				      pci_device->address.func_no,
				      offset, iosize, data);
}

void
pci_config_write (struct pci_device *pci_device, void *data, uint iosize,
		  uint offset)
{
	if (pci_device->config_mmio)
		pci_write_config_mmio (pci_device->config_mmio,
				       pci_device->address.bus_no,
				       pci_device->address.device_no,
				       pci_device->address.func_no,
				       offset, iosize, data);
	else
		pci_write_config_pmio (pci_device->address.bus_no,
				       pci_device->address.device_no,
				       pci_device->address.func_no,
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
	union mem data_fake;

	if (pci_device->fake_command_mask &&
	    offset <= 4 && offset + iosize > 4) {
		u8 *p = &(&data_fake.byte)[4 - offset];

		memcpy (&data_fake, data, iosize);
		data = &data_fake;
		pci_device->fake_command_virtual = *p;
		*p = (*p & ~pci_device->fake_command_mask) |
			pci_device->fake_command_fixed;
	}
	pci_config_write (pci_device, data, iosize, offset);
	if (offset >= 0x40) /* size that pci_read_config_space saves */
		return;
	pci_config_read (pci_device, &reg, sizeof reg, offset & ~3);
	pci_device->config_space.regs32[offset >> 2] = reg;
}

void
pci_handle_default_config_read (struct pci_device *pci_device, u8 iosize,
				u16 offset, union mem *data)
{
	pci_config_read (pci_device, data, iosize, offset);
	if (pci_device->fake_command_mask &&
	    offset <= 4 && offset + iosize > 4) {
		(&data->byte)[4 - offset] = ((&data->byte)[4 - offset] &
					     ~pci_device->fake_command_mask) |
			(pci_device->fake_command_virtual &
			 pci_device->fake_command_mask);
	}
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

	addr.offset = gphys - d->base;
	pci_config_io_handler (d, wr, addr.s.bus_no, addr.s.dev_no,
			       addr.s.func_no, addr.s.reg_offset, len, buf);
	return 1;
}

static u64
pci_get_bar_info_internal (struct pci_device *pci_device, int n,
			   struct pci_bar_info *bar_info, u16 offset,
			   union mem *data)
{
	enum pci_bar_info_type type;
	u32 low, high, mask, and;
	u32 match_offset;
	u64 newbase;

	if (n < 0 || n >= PCI_CONFIG_BASE_ADDRESS_NUMS)
		goto err;
	if (!(pci_device->base_address_mask_valid & (1 << n)))
		goto err;
	low = pci_device->config_space.base_address[n];
	high = 0;
	mask = pci_device->base_address_mask[n];
	match_offset = 0x10 + 4 * n;
	if ((mask & PCI_CONFIG_BASE_ADDRESS_SPACEMASK) ==
	    PCI_CONFIG_BASE_ADDRESS_MEMSPACE) {
		if ((mask & PCI_CONFIG_BASE_ADDRESS_TYPEMASK) ==
		    PCI_CONFIG_BASE_ADDRESS_TYPE64 &&
		    n + 1 < PCI_CONFIG_BASE_ADDRESS_NUMS)
			high = pci_device->config_space.base_address[n + 1];
		and = PCI_CONFIG_BASE_ADDRESS_MEMMASK;
		type = PCI_BAR_INFO_TYPE_MEM;
	} else {
		and = PCI_CONFIG_BASE_ADDRESS_IOMASK;
		type = PCI_BAR_INFO_TYPE_IO;
	}
	if (!(mask & and))
		goto err;
	bar_info->base = newbase = (low & mask & and) | (u64)high << 32;
	if (offset == match_offset)
		newbase = (data->dword & mask & and) | (u64)high << 32;
	else if (offset == match_offset + 4)
		newbase = (low & mask & and) | (u64)data->dword << 32;
	/* The bit 63 must be cleared for CPU access.  If it is set,
	 * the BAR access is for size detection. */
	if (newbase == (mask & and) || (newbase & 0x8000000000000000ULL))
		goto err;
	bar_info->len = (mask & and) & (~(mask & and) + 1);
	bar_info->type = type;
	return newbase;
err:
	bar_info->type = PCI_BAR_INFO_TYPE_NONE;
	return 0;
}

void
pci_get_bar_info (struct pci_device *pci_device, int n,
		  struct pci_bar_info *bar_info)
{
	pci_get_bar_info_internal (pci_device, n, bar_info, 0, NULL);
}

int
pci_get_modifying_bar_info (struct pci_device *pci_device,
			    struct pci_bar_info *bar_info, u8 iosize,
			    u16 offset, union mem *data)
{
	int n = -1;
	u64 newbase;

	if (offset + iosize - 1 >= 0x10 &&
	    offset < 0x10 + 4 * PCI_CONFIG_BASE_ADDRESS_NUMS) {
		n = (offset - 0x10) >> 2;
		if ((offset & 3) || iosize != 4 || n < 0 ||
		    n >= PCI_CONFIG_BASE_ADDRESS_NUMS)
			panic ("%s: invalid BAR access"
			       "  iosize=%X offset=0x%02X data=0x%08X",
			       __FUNCTION__, iosize, offset, data->dword);
		if (!(pci_device->base_address_mask_valid & (1 << n)) &&
		    n > 0 &&
		    (pci_device->base_address_mask_valid & (1 << (n - 1))) &&
		    (pci_device->base_address_mask[n - 1] &
		     (PCI_CONFIG_BASE_ADDRESS_SPACEMASK |
		      PCI_CONFIG_BASE_ADDRESS_TYPEMASK)) ==
		    (PCI_CONFIG_BASE_ADDRESS_MEMSPACE |
		     PCI_CONFIG_BASE_ADDRESS_TYPE64))
			n--;
		newbase = pci_get_bar_info_internal (pci_device, n, bar_info,
						     offset, data);
		if (bar_info->type == PCI_BAR_INFO_TYPE_NONE ||
		    bar_info->base == newbase)
			n = -1;
		else
			bar_info->base = newbase;
	}
	return n;
}

int
pci_driver_option_get_int (char *option, char **e, int base)
{
	char *p;
	long int ret;

	ret = strtol (option, &p, base);
	if (p == option)
		panic ("pci_driver_option_get_int: invalid value %s",
		       option);
	/* -0x7FFFFFFF - 1 is signed, -0x80000000 is unsigned */
	if (ret < -0x7FFFFFFF - 1)
		ret = -0x7FFFFFFF - 1;
	if (ret > 0x7FFFFFFF)
		ret = 0x7FFFFFFF;
	if (e)
		*e = p;
	else if (*p != '\0')
		panic ("pci_driver_option_get_int: invalid value %s",
		       option);
	return ret;
}

bool
pci_driver_option_get_bool (char *option, char **e)
{
	long int value;
	int i, j;
	char *p;
	static const char *k[][2] = {
		{ "YES", "yes1" },
		{ "NO", "no0" },
		{ "ON", "on1" },
		{ "OFF", "off0" },
		{ "TRUE", "true1" },
		{ "FALSE", "false0" },
		{ "Y", "y1" },
		{ "N", "n0" },
		{ "T", "t1" },
		{ "F", "f0" },
		{ NULL, NULL }
	};

	value = strtol (option, &p, 0);
	if (option != p) {
		if (e)
			*e = p;
		else if (*p != '\0')
			goto error;
		return !!value;
	}
	for (i = 0; k[i][0]; i++) {
		for (j = 0;; j++) {
			if (k[i][0][j] == '\0') {
				if (e)
					*e = &option[j];
				else if (option[j] != '\0')
					break;
				return k[i][1][j] == '1';
			}
			if (option[j] != k[i][0][j] && option[j] != k[i][1][j])
				break;
		}
	}
error:
	panic ("pci_driver_option_get_bool: invalid value %s", option);
}

struct pci_msi *
pci_msi_init (struct pci_device *pci_device,
	      int (*callback) (void *data, int num), void *data)
{
	u32 cmd;
	u8 cap;
	u32 capval;
	int num;
	struct pci_msi *msi;
	u32 maddr, mupper;
	u16 mdata;

	if (!pci_device)
		return NULL;
	pci_config_read (pci_device, &cmd, sizeof cmd, PCI_CONFIG_COMMAND);
	if (!(cmd & 0x100000))	/* Capabilities */
		return NULL;
	pci_config_read (pci_device, &cap, sizeof cap, 0x34); /* CAP */
	while (cap >= 0x40) {
		pci_config_read (pci_device, &capval, sizeof capval, cap);
		if ((capval & 0xFF) == 0x05)
			goto found;
		cap = capval >> 8;
	}
	return NULL;
found:
	num = exint_pass_intr_alloc (callback, data);
	if (num < 0 || num > 0xFF)
		return NULL;
	msi = alloc (sizeof *msi);
	msi->cap = cap;
	msi->dev = pci_device;
	maddr = 0xFEEFF000;
	mupper = 0;
	mdata = 0x4100 | num;
	pci_config_write (pci_device, &maddr, sizeof maddr, cap + 4);
	pci_config_write (pci_device, &mupper, sizeof mupper, cap + 8);
	pci_config_write (pci_device, &mdata, sizeof mdata, cap + 12);
	return msi;
}

void
pci_msi_enable (struct pci_msi *msi)
{
	struct pci_device *pci_device = msi->dev;
	u8 cap = msi->cap;
	u16 mctl = 1;

	pci_config_write (pci_device, &mctl, sizeof mctl, cap + 2);
}

void
pci_msi_disable (struct pci_msi *msi)
{
	struct pci_device *pci_device = msi->dev;
	u8 cap = msi->cap;
	u16 mctl = 0;

	pci_config_write (pci_device, &mctl, sizeof mctl, cap + 2);
}
