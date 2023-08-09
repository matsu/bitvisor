/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2015 Igel Co., Ltd
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

#include <core.h>
#include <core/ap.h>
#include <core/initfunc.h>
#include <core/list.h>
#include <core/mmio.h>
#include <net/netapi.h>
#include <pci.h>
#include "virtio_net.h"

/* virtio-net virtual driver

   Provides a virtual virtio-net device.  MSI-X is required.  The
   "net" parameter is similar to para pass-through network drivers
   except that a guest operating system is connected to a *physical*
   side of the netapi.  The following example creates a virtual
   virtio-net device which is connected to a TCP/IP stack:

   vmm.driver.pci_virtual=driver=virtio-net, net=ip
 */

struct data {
	struct netdata *nethandle;
	void *virtio_net;
	u8 macaddr[6];
	const struct mm_as *as_dma;
	int msix_qvec[3];
	struct msix_table *msix_tbl;
};

static void
getinfo_virtnic (void *handle, struct nicinfo *info)
{
	struct data *d = handle;

	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, d->macaddr, sizeof d->macaddr);
}

static void
send_virtnic (void *handle, unsigned int num_packets, void **packets,
	      unsigned int *packet_sizes, bool print_ok)
{
}

static void
setrecv_virtnic (void *handle, net_recv_callback_t *callback, void *param)
{
}

static void
poll_virtnic (void *handle)
{
}

static struct nicfunc virt_func = {
	.get_nic_info = getinfo_virtnic,
	.send = send_virtnic,
	.set_recv_callback = setrecv_virtnic,
	.poll = poll_virtnic,
};

static void
virtual_virtio_net_intr_clear (void *param)
{
}

static void
virtual_virtio_net_intr_set (void *param)
{
}

static void
virtual_virtio_net_intr_disable (void *param)
{
}

static void
virtual_virtio_net_intr_enable (void *param)
{
}

static void
virtual_virtio_net_msix_disable (void *param)
{
}

static void
virtual_virtio_net_msix_enable (void *param)
{
}

static void
virtual_virtio_net_msix_vector_change (void *param, unsigned int queue,
				       int vector)
{
	struct data *d = param;

	if (queue < 3)
		d->msix_qvec[queue] = vector;
}

static void
virtual_virtio_net_msix_generate (void *param, unsigned int queue)
{
	struct data *d = param;
	struct msix_table m = { 0, 0, 0, 1 };

	if (queue < 3)
		m = d->msix_tbl[d->msix_qvec[queue]];
	if (!(m.mask & 1))
		pci_msi_to_ipi (d->as_dma, m.addr, m.upper, m.data);
}

static void
virtual_virtio_net_msix_mmio_update (void *param)
{
}

static void
virtual_virtio_net_new (struct pci_virtual_device *dev)
{
	char *option_net = dev->driver_options[1];
	bool option_tty = false;
	struct data *d = alloc (sizeof *d);
	struct nicfunc *virtio_net_func;
	static u8 devcount;

	dev->host = d;
	if (dev->driver_options[0] &&
	    pci_driver_option_get_bool (dev->driver_options[0], NULL))
		option_tty = true;
	d->nethandle = net_new_nic (option_net, option_tty);
	d->macaddr[0] = 0x02;
	d->macaddr[1] = 0x48;
	d->macaddr[2] = 0x84;
	d->macaddr[3] = 0x76;
	d->macaddr[4] = 0x70;
	d->macaddr[5] = devcount++;
	d->virtio_net = virtio_net_init (&virtio_net_func,
					 d->macaddr,
					 dev->as_dma,
					 virtual_virtio_net_intr_clear,
					 virtual_virtio_net_intr_set,
					 virtual_virtio_net_intr_disable,
					 virtual_virtio_net_intr_enable, d);
	if (d->virtio_net) {
		d->as_dma = dev->as_dma;
		d->msix_qvec[0] = -1;
		d->msix_qvec[1] = -1;
		d->msix_qvec[2] = -1;
		/* BAR5 for MSI-X tables. */
		d->msix_tbl = virtio_net_set_msix
			(d->virtio_net,
			 virtual_virtio_net_msix_disable,
			 virtual_virtio_net_msix_enable,
			 virtual_virtio_net_msix_vector_change,
			 virtual_virtio_net_msix_generate,
			 virtual_virtio_net_msix_mmio_update, d);
		/* Use virtio_net_func for phys_func. */
		net_init (d->nethandle, d->virtio_net, virtio_net_func, d,
			  &virt_func);
		net_start (d->nethandle);
	}
}

static void
virtual_virtio_net_config_read (struct pci_virtual_device *dev, u8 iosize,
				u16 offset, union mem *data)
{
	struct data *d = dev->host;

	if (!d->virtio_net) {
		memset (data, 0xFF, iosize);
		return;
	}
	virtio_net_handle_config_read (d->virtio_net, iosize, offset, data);
}

static void
virtual_virtio_net_config_write (struct pci_virtual_device *dev, u8 iosize,
				 u16 offset, union mem *data)
{
	struct data *d = dev->host;

	if (!d->virtio_net)
		return;
	virtio_net_handle_config_write (d->virtio_net, iosize, offset, data);
}

static struct pci_virtual_driver virtual_virtio_net_driver = {
	.name		= "virtio-net",
	.longname	= "virtio-net virtual driver",
	.driver_options	= "tty,net",
	.new		= virtual_virtio_net_new,
	.config_read	= virtual_virtio_net_config_read,
	.config_write	= virtual_virtio_net_config_write,
};

static void
virtual_virtio_net_init (void)
{
	pci_register_virtual_driver (&virtual_virtio_net_driver);
}

PCI_DRIVER_INIT (virtual_virtio_net_init);
