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

#include <common.h>
#include <core.h>
#include "pci.h"

#define PCI_CLASS_LPC		0x060100
#define PCI_CLASS_LPC_MASK	0xFFFF00

#define LPC_INDEX_PORT		0x2E
#define LPC_DATA_PORT		0x2F

static const char driver_name[] = "lpc_driver";
static const char driver_longname[] = "LPC Interface Bridge driver 0.1";

static bool config_mode = false;
static int logical_device_number = 0;
static int index = 0;

static int lpc_config_read(struct pci_device *pci_device, core_io_t ioaddr, u8 offset, union mem *data)
{
	return CORE_IO_RET_DEFAULT;
}

static int lpc_config_write(struct pci_device *pci_device, core_io_t ioaddr, u8 offset, union mem *data)
{
	struct pci_config_space *config_space = &pci_device->config_space;

	switch (offset & 0xFC) {
	case 0x80:
		printf("%s: %04x:%04x %02x:%08x => %02x:%08x\n", __func__,
		       pci_device->config_space.vendor_id, pci_device->config_space.device_id,
		       offset, *(u32 *)data, offset & 0xFC, pci_read_config_data_port());
//		memcpy(data, config_space->regs8 + offset, ioaddr.size);
//		config_space->regs8[0x80] = 0x10;
//		config_space->regs8[0x82] = 0x01;
//		memcpy(config_space->regs8 + offset, &reg, ioaddr.size);
//		pci_handle_default_config_write(pci_device, ioaddr, offset, &reg);
		return CORE_IO_RET_DONE;
	}
	return CORE_IO_RET_DEFAULT;
}


static int lpc_index_handler(core_io_t io, union mem *data, void *arg)
{
	if (io.dir != CORE_IO_DIR_OUT)
		return CORE_IO_RET_DEFAULT;

	printf("%s: %02x\n", data->byte);
	index = data->byte;
	if (index == 0x55) {
		config_mode = true;
		logical_device_number = 0;
	} else if (index == 0xAA)
		config_mode = false;
	return CORE_IO_RET_DEFAULT;
}

static int lpc_data_handler(core_io_t io, union mem *data, void *arg)
{
	if (io.dir != CORE_IO_DIR_OUT)
		return CORE_IO_RET_DEFAULT;
	if (!config_mode)
		return CORE_IO_RET_DEFAULT;

	printf("%s: %02x\n", data->byte);
	switch (logical_device_number) {
	case 0: // GLOBAL CONFIGURATION
		switch (index) {
		case 0x07:
			logical_device_number = data->byte;
			break;
		}
		break;

	case 4: // Serial Port 1
		return CORE_IO_RET_DONE; // block

	case 5: // Serial Port 2
		break;

	}
	return CORE_IO_RET_DEFAULT;
}


static void lpc_new(struct pci_device *pci_device)
{
	core_io_register_handler(LPC_INDEX_PORT, 1, lpc_index_handler, NULL, CORE_IO_PRIO_EXCLUSIVE, driver_name);
	core_io_register_handler(LPC_DATA_PORT,  1, lpc_data_handler, NULL, CORE_IO_PRIO_EXCLUSIVE, driver_name);
	return;
}

static struct pci_driver lpc_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.id		= { PCI_ID_ANY, PCI_ID_ANY_MASK },
	.class		= { 0x060100, 0xFFFF00 },
	.new		= lpc_new,
	.config_read	= lpc_config_read,
	.config_write	= lpc_config_write,
};

void lpc_init(void) __initcode__
{
	pci_register_driver(&lpc_driver);
	return;
}
//PCI_DRIVER_INIT(lpc_init);
