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
 * @file	drivers/ata_init.c
 * @brief	generic ATA para pass-through driver (initialization)
 * @author	T. Shinagawa
 */

#include <core.h>
#include "ata.h"
#include "ata_init.h"

const char ata_driver_name[] = "ata_generic_driver";
const char ata_driver_longname[] = "Generic ATA/ATAPI para pass-through driver 0.4";

static unsigned int host_id = 0;
static unsigned int device_id = 0;

/* FIXME: should use slab allocator ? */
DEFINE_ALLOC_FUNC(ata_host)
DEFINE_ALLOC_FUNC(ata_channel)

int ata_init_io_handler(ioport_t start, size_t num, core_io_handler_t handler, void *arg)
{
	return core_io_register_handler(start, num, handler, arg,
					CORE_IO_PRIO_EXCLUSIVE, ata_driver_name);
}

// allocate and set a shadow DMA buffer and PRD table
static void ata_init_prd(struct ata_channel *channel)
{
	int i;
	void *buf;
	ata_prd_table_t *prd, prd_entry;
	u64 buf_phys, prd_phys;

	// allocate DMA shadow buffer (align: 64KB, location < 4GB)
	alloc_pages(&buf, &buf_phys, ATA_BM_TOTAL_BUFSIZE / PAGESIZE);
	// buf = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE | MAPMEM_PCD, buf_phys, ATA_BM_TOTAL_BUFSIZE);
	if (buf == NULL)
		goto error;
	if (buf_phys > ATA_BM_BUFADDR_LIMIT || buf_phys & ATA_BM_BUFADDR_ALIGN)
		goto error;

	// allocate PRD table (align: dword (not cross a 64KB boundary), location < 4GB)
	alloc_page((void *)&prd, &prd_phys);
	// prd = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE | MAPMEM_PCD, prd_phys, PAGESIZE);
	if (prd == NULL)
		goto error;
	if (prd_phys > ATA_BM_PRDADDR_LIMIT)
		goto error;

	for (i = 0; i < ATA_BM_BUFNUM; i++) {
		prd_entry.value = 0;
		prd_entry.base = buf_phys + (ATA_BM_BUFSIZE * i);
		prd[i].value = prd_entry.value;
	}
	prd[i].eot = 1;

	// set shadow prd table
	channel->shadow_prd = prd;
	channel->shadow_prd_phys = prd_phys;
	channel->shadow_buf = buf;
	return;

error:
	panic("ATA: %s: buf=%p, buf_phys=%lld, prd=%p, prd_phys=%lld\n",
	      __func__, buf, buf_phys, prd, prd_phys);
}

static void ata_init_ata_device(struct ata_device *ata_device)
{
	ata_device->storage_device = storage_new(STORAGE_TYPE_ATA, host_id, device_id, NULL, 512);
	ata_device->heads_per_cylinder = ATA_DEFAULT_HEADS;
	ata_device->sectors_per_track = ATA_DEFAULT_SECTORS;
	ata_device->packet_length = 12;
	device_id++;
}

static struct ata_channel *ata_new_channel(struct ata_host* host)
{
	struct ata_channel *channel;

	channel = alloc_ata_channel();
	memset(channel, 0, sizeof(*channel));
	spinlock_init(&channel->lock);
	channel->hd[ATA_ID_CMD] = -1;
	channel->hd[ATA_ID_CTL] = -1;
	channel->hd[ATA_ID_BM] = -1;
	ata_init_ata_device(&channel->device[0]);
	ata_init_ata_device(&channel->device[1]);
	channel->host = host;
	channel->state = ATA_STATE_READY;
	ata_init_prd(channel);
	host_id++; device_id = 0;
	return channel;
}

static void ata_new(struct pci_device *pci_device)
{
	struct ata_host *host;

	/* initialize ata_host and ata_channel structures */
	host = alloc_ata_host();
	host->pci_device = pci_device;
	host->interrupt_line = pci_device->config_space.interrupt_line;
	host->channel[0] = ata_new_channel(host);
	host->channel[1] = ata_new_channel(host);
	pci_device->host = host;

	/* initialize primary and secondary channels */
	ata_set_cmdblk_handler(host, 0);
	ata_set_ctlblk_handler(host, 0);
	ata_set_cmdblk_handler(host, 1);
	ata_set_ctlblk_handler(host, 1);

	/* initialize bus master */
	ata_set_bm_handler(host);

	/* vendor specific init */
	ata_init_vendor(host);
	return;
}

static struct pci_driver ata_driver = {
	.name		= ata_driver_name,
	.longname	= ata_driver_longname,
	.id		= { PCI_ID_ANY, PCI_ID_ANY_MASK },	/* match with any VendorID:DeviceID */
	.class		= { 0x010100, 0xFFFF00 },		/* class = Mass Storage, subclass = IDE */
	.new		= ata_new,		/* called when a new PCI ATA device is found */
	.config_read	= ata_config_read,	/* called when a config register is read */
	.config_write	= ata_config_write,	/* called when a config register is written */
};

static void ata_init(void)
{
	ASSERT(CORE_IO_DIR_IN == STORAGE_READ);
	pci_register_driver(&ata_driver);
	/* may need to initialize the compatible host even if no PCI ATA device exists */
	/* FIXME: some controller may be hidden! (see ICH8 5.16: D31, F1 is diabled by D30, F0, offset F2h, bit1) */
	return;
}
PCI_DRIVER_INIT(ata_init);
