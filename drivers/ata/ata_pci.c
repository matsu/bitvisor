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
 * @file	drivers/ata_pci_config.c
 * @brief	generic ATA para pass-through driver (PCI configration space handling)
 * @author	T. Shinagawa
 */

#include <core.h>
#include "ata.h"
#include "ata_pci.h"
#include "ahci.h"

/********************************************************************************
 * ATA handlers install
 ********************************************************************************/
static void ata_set_handler(struct ata_channel *channel, int id, u32 base, int num, core_io_handler_t handler)
{
	channel->base[id] = base;
	channel->hd[id] = (channel->hd[id] >= 0) ?
		core_io_modify_handler(channel->hd[id], base, num) :
		ata_init_io_handler(base, num, handler, channel);
}

void ata_set_bm_handler(struct ata_host *host)
{
	u32 bm_base;
	ata_api_t api;
	struct pci_config_space *config_space = &host->pci_device->config_space;

	api.value = config_space->programming_interface;
	if (0/*api.bus_master==0 on AHCI controller*/ && !api.bus_master)
		return;

	bm_base = config_space->base_address[4] & PCI_CONFIG_BASE_ADDRESS_IOMASK;
	ata_set_handler(host->channel[0], ATA_ID_BM, bm_base + ATA_BM_PRI_OFFSET, ATA_BM_PORT_NUMS, ata_bm_handler);
	ata_set_handler(host->channel[1], ATA_ID_BM, bm_base + ATA_BM_SEC_OFFSET, ATA_BM_PORT_NUMS, ata_bm_handler);
	return;
}

static u32 ata_get_base_address(struct ata_host *host, int index)
{
	u32 *bar, base;
	int mode, ch = index / 2;
	struct pci_config_space *config_space = &host->pci_device->config_space;
	static u32 compat_bar[] = {
		ATA_COMPAT_PRI_CMD_BASE,
		ATA_COMPAT_PRI_CTL_BASE - 2,
		ATA_COMPAT_SEC_CMD_BASE,
		ATA_COMPAT_SEC_CTL_BASE - 2,
	};

	mode = (config_space->programming_interface >> (ch * 2)) & 0x01;
	bar = (mode == ATA_NATIVE_PCI_MODE) ? config_space->base_address : compat_bar;
	base = bar[index] & PCI_CONFIG_BASE_ADDRESS_IOMASK;
	return base;
}

void ata_set_cmdblk_handler(struct ata_host *host, int ch)
{
	u32 base;

	base = ata_get_base_address(host, ch * 2);
	ata_set_handler(host->channel[ch], ATA_ID_CMD, base, ATA_CMD_PORT_NUMS, ata_cmdblk_handler);
}

void ata_set_ctlblk_handler(struct ata_host *host, int ch)
{
	u32 base;

	base = ata_get_base_address(host, ch * 2 + 1);
	ata_set_handler(host->channel[ch], ATA_ID_CTL, base + 2, ATA_CTL_PORT_NUMS, ata_ctlblk_handler);
}

/********************************************************************************
 * PCI configuration space handler
 ********************************************************************************/
int
ata_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
		 union mem *data)
{
	struct ata_host *ata_host = pci_device->host;
	struct pci_config_space *config_space = &pci_device->config_space;

	if (ahci_config_read (ata_host->ahci_data, pci_device, iosize, offset,
			      data))
		return CORE_IO_RET_DONE;
	ASSERT (offset < 0x100);
	pci_handle_default_config_read (pci_device, iosize, offset, data);
	switch (offset & 0xFC) {
	case 0x00: // can virtualize Vendor/Device ID
		regcpy(data, config_space->regs8 + offset, (size_t)iosize);
	}
	return CORE_IO_RET_DONE;
}

int
ata_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
		  union mem *data)
{
	struct ata_host *ata_host = pci_device->host;
	struct pci_config_space *config_space = &pci_device->config_space;
	union mem *regptr = (union mem *)&config_space->regs8[offset];

	if (ahci_config_write (ata_host->ahci_data, pci_device, iosize, offset,
			       data))
		return CORE_IO_RET_DONE;
	ASSERT (offset < 0x100);

	/* To avoid TOCTTOU, lock the devices while the base addresses may be changed. */
	ata_channel_lock (ata_host->channel[0]);
	ata_channel_lock (ata_host->channel[1]);

	/* update cache */
	regcpy(regptr, data, (size_t)iosize);

	/* pre-write */
	switch (offset & 0xFC) {
#if 0
	case 0x04:
		config_space->command &= ~0x02; // disable MMIO;
		break;
	case PCI_CONFIG_BASE_ADDRESS5:
		config_space->base_address[5] = 0; // base address 5 is not supported
#endif
	}

	/* do write */
	pci_handle_default_config_write (pci_device, iosize, offset, regptr);

	/* post-write */
	switch (offset & 0xFC) {
	case 0x08: /* The API may be switched between compatibilty and native mode */
		ata_set_cmdblk_handler(ata_host, 0);
		ata_set_ctlblk_handler(ata_host, 0);
		ata_set_cmdblk_handler(ata_host, 1);
		ata_set_ctlblk_handler(ata_host, 1);
		break;

	case PCI_CONFIG_BASE_ADDRESS0:
		ata_set_cmdblk_handler(ata_host, 0);
		break;

	case PCI_CONFIG_BASE_ADDRESS1:
		ata_set_ctlblk_handler(ata_host, 0);
		break;

	case PCI_CONFIG_BASE_ADDRESS2:
		ata_set_cmdblk_handler(ata_host, 1);
		break;

	case PCI_CONFIG_BASE_ADDRESS3:
		ata_set_ctlblk_handler(ata_host, 1);
		break;

	case PCI_CONFIG_BASE_ADDRESS4:
		ata_set_bm_handler(ata_host);
		break;
	}

	ata_channel_unlock (ata_host->channel[1]);
	ata_channel_unlock (ata_host->channel[0]);
	return CORE_IO_RET_DONE;
}
