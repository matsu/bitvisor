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
 * @file	drivers/ata_vendor.c
 * @brief	ATA vendor/device specific functions
 * @author	T. Shinagawa
 */

#include <core.h>
#include "ata.h"
#include "debug.h"

void ata_init_vendor(struct ata_host *host)
{
#if 0
	u8 command;
	int offset = offsetof(struct pci_config_space, command);
	struct pci_device *pci_device = host->pci_device;

	/* disable PCI MMIO */
	command = pci_read_config_data8(pci_device->address, offset);
	command &= ~0x02; // disable MMIO
	pci_write_config_data8(pci_device->address, offset, command);
#endif

	switch(host->pci_device->config_space.vendor_id) {
	case 0x11AB: // Marvell
		// bit 0 is used for cable detect
		host->channel[0]->device_specific.mask.bm_ds1 = 0x01;
		host->channel[1]->device_specific.mask.bm_ds1 = 0x01;
	}
}

int ata_bm_handle_device_specific(struct ata_channel *channel, core_io_t io, union mem *data)
{
	union mem reg;
	int regname = ata_get_regname(channel, io, ATA_ID_BM);
	u8 mask = (regname == ATA_BM_DeviceSpecific1) ?
		channel->device_specific.mask.bm_ds1 : channel->device_specific.mask.bm_ds3;

	if (io.dir == CORE_IO_DIR_IN) {
		core_io_handle_default(io, data);
		if (data->byte & ~mask)
			dbg_printf("umimplemented bit is read: regname=%d, data=%02x\n", regname, data->byte);
		data->byte &= mask;
	} else {
		if (data->byte & ~mask)
			dbg_printf("umimplemented bit is write: regname=%d, data=%02x\n", regname, data->byte);
		reg.byte = data->byte & mask;
		core_io_handle_default(io, &reg);
	}
	return CORE_IO_RET_DONE;
}
