/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2010 Igel Co., Ltd
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
#include <core/mmio.h>
#include "pci.h"
#include "pci_conceal.h"

static int
iohandler (core_io_t io, union mem *data, void *arg)
{
	if (io.dir == CORE_IO_DIR_IN)
		memset (data, 0xFF, io.size);
	return CORE_IO_RET_DONE;
}

static int
mmhandler (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	if (!wr)
		memset (buf, 0xFF, len);
	return 1;
}

int
pci_conceal_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
			 union mem *data)
{
	/* Provide fake values for reading the PCI configration space. */
	memset (data, 0xFF, iosize);
	return CORE_IO_RET_DONE;
}

int
pci_conceal_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
			  union mem *data)
{
	/* Do nothing, ignore any writing. */
	return CORE_IO_RET_DONE;
}

void
pci_conceal_new (struct pci_device *pci_device)
{
	int i;
	struct pci_bar_info bar;

	printf ("[%02X:%02X.%u/%04X:%04X] concealed\n",
		pci_device->address.bus_no, pci_device->address.device_no,
		pci_device->address.func_no,
		pci_device->config_space.vendor_id,
		pci_device->config_space.device_id);
	pci_system_disconnect (pci_device);
	for (i = 0; i < PCI_CONFIG_BASE_ADDRESS_NUMS; i++) {
		pci_get_bar_info (pci_device, i, &bar);
		if (bar.type == PCI_BAR_INFO_TYPE_IO) {
			core_io_register_handler
				(bar.base, bar.len, iohandler, NULL,
				 CORE_IO_PRIO_EXCLUSIVE, "pci_conceal");
		} else if (bar.type == PCI_BAR_INFO_TYPE_MEM) {
			mmio_register (bar.base, bar.len, mmhandler, NULL);
		}
	}
}

static struct pci_driver pci_conceal_driver = {
	.name		= "conceal",
	.longname	= "PCI device concealer",
	.device		= "id=:", /* this matches no devices */
	.new		= pci_conceal_new,
	.config_read	= pci_conceal_config_read,
	.config_write	= pci_conceal_config_write,
};

static void
pci_conceal_init (void)
{
	pci_register_driver (&pci_conceal_driver);
}

PCI_DRIVER_INIT (pci_conceal_init);
