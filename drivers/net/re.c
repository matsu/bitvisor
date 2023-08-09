/*
 * Copyright (c) 2018 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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
#include <core/mmio.h>
#include <net/netapi.h>
#include <pci.h>
#include "re_core.h"
#include "virtio_net.h"

static const char driver_name[] = "re";
static const char driver_longname[] = "Realtek Ethernet Driver";

static LIST1_DEFINE_HEAD (struct re_host, re_host_list);

static void
getinfo_physnic (void *handle, struct nicinfo *info)
{
	struct re_host *host = handle;

	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, host->mac, ETHER_ADDR_LEN);
}

static void
send_physnic (void *handle,
	      uint n_packets,
	      void **packets,
	      uint *packet_sizes,
	      bool print_ok)
{
	struct re_host *host = handle;

	re_core_handle_send (host, n_packets, packets, packet_sizes);
}

static void
setrecv_physnic (void *handle, net_recv_callback_t *callback, void *param)
{
	struct re_host *host = handle;

	host->recvphys_func = callback;
	host->recvphys_param = param;
}

static void
poll_physnic (void *handle)
{
	struct re_host *host = handle;

	re_core_handle_recv (host);
}

static struct nicfunc phys_func = {
	.get_nic_info = getinfo_physnic,
	.send = send_physnic,
	.set_recv_callback = setrecv_physnic,
	.poll = poll_physnic,
};

static void
re_new (struct pci_device *dev)
{
	struct re_host *host;
	char *option_net;
	int option_tty = 0;
	int option_virtio = 0;
	u32 command_orig, command;
	struct nicfunc *virtio_net_func = NULL;
	struct pci_bar_info bar_info;

	pci_system_disconnect (dev);

	printf ("[%02x:%02x.%01x] A Realtek Ethernet device found\n",
		dev->address.bus_no,
		dev->address.device_no,
		dev->address.func_no);

	host = alloc (sizeof (*host));
	memset (host, 0, sizeof (*host));

	spinlock_init (&host->tx_lock);
	spinlock_init (&host->rx_lock);

	dev->host = host;

	host->dev = dev;

	command = PCI_CONFIG_COMMAND_BUSMASTER | PCI_CONFIG_COMMAND_MEMENABLE;
	pci_enable_device (dev, command);

	/* Disable IO space */
	pci_config_read (dev,
			 &command_orig,
			 sizeof command_orig,
			 PCI_CONFIG_COMMAND);
	command = command_orig & ~PCI_CONFIG_COMMAND_IOENABLE;
	if (command != command_orig)
		pci_config_write (dev,
				  &command,
				  sizeof command,
				  PCI_CONFIG_COMMAND);

	re_core_init (host);

	re_core_start (host);

	if (dev->driver_options[0] &&
	    pci_driver_option_get_bool (dev->driver_options[0], NULL))
		option_tty = true;
	if (dev->driver_options[2] &&
	    pci_driver_option_get_bool (dev->driver_options[2], NULL))
		option_virtio = true;

	option_net = dev->driver_options[1];
	host->nethandle = net_new_nic (option_net, option_tty);

	re_core_current_mmio_bar (host, &bar_info);

	if (option_virtio) {
		host->virtio_net = virtio_net_init (&virtio_net_func,
						    host->mac,
						    dev->as_dma,
						    re_core_intr_clear,
						    re_core_intr_set,
						    re_core_intr_disable,
						    re_core_intr_enable,
						    host);
		virtio_net_set_pci_device (host->virtio_net, dev, &bar_info,
					   re_core_mmio_change, host);
	}

	if (!net_init (host->nethandle,
		       host,
		       &phys_func,
		       host->virtio_net,
		       virtio_net_func))
		panic ("net_init() fails");

	net_start (host->nethandle);

	LIST1_PUSH (re_host_list, host);

	printf ("re: initializtion done\n");
	printf ("re: MAC Address is %02X:%02X:%02X:%02X:%02X:%02X\n",
		host->mac[0],
		host->mac[1],
		host->mac[2],
		host->mac[3],
		host->mac[4],
		host->mac[5]);
}

static int
re_config_read (struct pci_device *dev,
		u8 iosize,
		u16 offset,
		union mem *data)
{
	struct re_host *host = dev->host;

	if (host->virtio_net) {
		virtio_net_handle_config_read (host->virtio_net,
					       iosize,
					       offset,
					       data);
	} else {
		memset (data, 0, iosize);
	}

	return CORE_IO_RET_DONE;
}

static int
re_config_write (struct pci_device *dev,
		 u8 iosize,
		 u16 offset,
		 union mem *data)
{
	struct re_host *host = dev->host;

	if (host->virtio_net) {
		virtio_net_handle_config_write (host->virtio_net,
						iosize,
						offset,
						data);
	}

	return CORE_IO_RET_DONE;
}

static struct pci_driver re_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.driver_options	= "tty,net,virtio",
	.device		= "class_code=020000,id="
			  "10ec:8129|"
			  "10ec:8139|"
			  "10ec:8169|" /* RTL8169   */
			  "10ec:8167|" /* RTL8169SC */
			  "10ec:8168|" /* RTL8168B  */
			  "10ec:8161|" /* RTL8168 Series add-on card */
			  "10ec:8136", /* RTL8101E  */
	.new		= re_new,
	.config_read	= re_config_read,
	.config_write	= re_config_write,
};

static void
re_init (void)
{
	LIST1_HEAD_INIT (re_host_list);
	pci_register_driver (&re_driver);
}

static void
re_suspend (void)
{
	struct re_host *host;

	LIST1_FOREACH (re_host_list, host)
		re_core_suspend (host);
}

static void
re_resume (void)
{
	struct re_host *host;
	u32 command;

	command = PCI_CONFIG_COMMAND_BUSMASTER | PCI_CONFIG_COMMAND_MEMENABLE;

	LIST1_FOREACH (re_host_list, host) {
		pci_enable_device (host->dev, command);

		/* Restore PCI config space */
		uint i;
		for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++)
			pci_config_write (host->dev,
					  &host->regs_at_init[i],
					  sizeof (host->regs_at_init[i]),
					  sizeof (host->regs_at_init[i]) * i);

		re_core_resume (host);
	}
}

PCI_DRIVER_INIT (re_init);
INITFUNC ("suspend1", re_suspend);
INITFUNC ("resume1", re_resume);
