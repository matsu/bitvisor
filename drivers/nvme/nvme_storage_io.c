/*
 * Copyright (c) 2017 Igel Co., Ltd
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

/**
 * @file	drivers/nvme/nvme_storage_io.c
 * @brief	NVMe module for storage I/O interfaces
 * @author	Ake Koomsin
 */

/*
 * Note on the current implementation
 *
 * Current implementation is not efficient because of storage_io API.
 *
 */

#include <core.h>
#include <core/thread.h>
#include <storage_io.h>

#include "pci.h"

#include "nvme_io.h"

#define MAX_STORAGE (8)

#define ATA_OPCODE_READ	 (0x25)
#define ATA_OPCODE_WRITE (0x35)

struct nvme_storage_meta {
	struct storage_hc_driver *hc;
	struct storage_hc_addr hc_addr;
};

static struct nvme_storage_meta storage_metas[MAX_STORAGE];
static uint storage_count;

struct nvme_ata_wrapper {
	struct storage_hc_dev_atacmd *atacmd;
	struct nvme_io_dmabuf *dmabuf;
	u8 opcode;
};

static void
ata_rw_callback (struct nvme_host *host,
		 u8 status_type,
		 u8 status,
		 u32 cmd_specific,
		 void *arg)
{
	struct nvme_ata_wrapper *ata_wrapper = arg;

	struct storage_hc_dev_atacmd *atacmd = ata_wrapper->atacmd;

	if (ata_wrapper->opcode == ATA_OPCODE_READ) {
		if (status_type == 0 && status == 0) {
			struct nvme_io_dmabuf *dmabuf = ata_wrapper->dmabuf;
			memcpy (atacmd->buf, dmabuf->buf, atacmd->buf_len);
		} else {
			printf ("Error occurs, not copying back\n");
		}
	}

	nvme_io_free_dmabuf (ata_wrapper->dmabuf);
	free (ata_wrapper);

	atacmd->callback (atacmd->data, atacmd);
}

static void
cmd_wait (void *arg)
{
	struct nvme_io_req_handle *req_handle = arg;
	nvme_io_error_t error;

	error = nvme_io_wait_for_completion (req_handle, 3);
	if (error)
		printf ("cmd error with code 0x%X\n", error);

	thread_exit ();
}

static int
nvme_io_ata_rw_request (struct nvme_host *host,
			u8 opcode,
			u32 dev_no,
			struct storage_hc_dev_atacmd *atacmd)
{
	nvme_io_error_t error;

	if (opcode != ATA_OPCODE_READ &&
	    opcode != ATA_OPCODE_WRITE) {
		printf ("wrong opcode\n");
		return 0;
	}

	u64 lba;
	lba = atacmd->cyl_high_exp;
	lba = (lba << 8) | atacmd->cyl_low_exp;
	lba = (lba << 8) | atacmd->sector_number_exp;
	lba = (lba << 8) | atacmd->cyl_high;
	lba = (lba << 8) | atacmd->cyl_low;
	lba = (lba << 8) | atacmd->sector_number;

	u32 lba_nbytes;
	error = nvme_io_get_lba_nbytes (host, dev_no, &lba_nbytes);
	if (error) {
		printf ("Get lba_nbytes error 0x%X\n", error);
		return 0;
	}

	uint remain = atacmd->buf_len % lba_nbytes;
	uint n_lbas;
	n_lbas = atacmd->buf_len / lba_nbytes;
	n_lbas += ((remain) ? 1 : 0);

	uint n_pages = atacmd->buf_len / PAGE_NBYTES;
	remain  = atacmd->buf_len % PAGE_NBYTES;
	n_pages += remain ? 1 : 0;

	u16 max_n_lbas;
	error = nvme_io_get_max_n_lbas (host, dev_no, &max_n_lbas);
	if (error) {
		printf ("Get max_n_lbas error 0x%X\n", error);
		return 0;
	}

	if (n_lbas > max_n_lbas) {
		printf ("Request is too large\n");
		return 0;
	}

	if (atacmd->buf_len == 0) {
		printf ("Invalid atacmd buf length\n");
		return 0;
	}

	/*
	 * Need NVMe's own dmabuf due to insufficient buffer information
	 * from atacmd object. storage_io needs to be updated.
	 */
	struct nvme_io_dmabuf *dmabuf;
	dmabuf = nvme_io_alloc_dmabuf (atacmd->buf_len);

	struct nvme_io_descriptor *io_desc;
	io_desc = nvme_io_init_descriptor (host,
					   dev_no, /* AKA nsid */
					   lba,
					   n_lbas);
	if (!io_desc) {
		printf ("Not able to create I/O descriptor\n");
		return 0;
	}

	error = nvme_io_set_phys_buffers (host,
					  io_desc,
				  	  dmabuf->dma_list,
				  	  dmabuf->dma_list_phys,
				  	  n_pages,
				  	  0);
	if (error) {
		printf ("Not able to setup buffer, error 0x%X\n", error);
		return 0;
	}

	struct nvme_ata_wrapper *ata_wrapper;
	ata_wrapper = alloc (sizeof (*ata_wrapper));
	ata_wrapper->atacmd = atacmd;
	ata_wrapper->dmabuf = dmabuf;
	ata_wrapper->opcode = opcode;

	struct nvme_io_req_handle *req_handle;
	if (opcode == ATA_OPCODE_WRITE) {
		memcpy (dmabuf->buf, atacmd->buf, atacmd->buf_len);
		error = nvme_io_write_request (host, io_desc, 1,
					       ata_rw_callback, ata_wrapper,
					       &req_handle);
	} else {
		error = nvme_io_read_request (host, io_desc, 1,
					      ata_rw_callback, ata_wrapper,
					      &req_handle);
	}

	if (error) {
		printf ("Fail to submit the command, error 0x%X\n", error);
		return 0;
	}

	thread_new (cmd_wait, req_handle, VMM_STACKSIZE);

	return 1;
}

static int
nvme_scandev (void *drvdata, int port_no,
	      storage_hc_scandev_callback_t *callback, void *data)
{
	nvme_io_error_t error;

	if (port_no != 0) {
		printf ("port is not zero\n");
		return 0;
	}

	struct nvme_host *host = drvdata;

	error = nvme_io_host_ready (host);
	if (error) {
		printf ("Cannot scan, controller is not ready, error 0x%X\n",
			error);
		return 0;
	}

	uint n_ns;
	error = nvme_io_get_n_ns (host, &n_ns);
	if (error) {
		printf ("Cannot get number of namespace, error 0x%X\n", error);
		return 0;
	}

	uint i;
	for (i = 1; i <= n_ns; i++)
		callback (data, i);

	return 1;
}

static bool
nvme_openable (void *drvdata, int port_no, int dev_no)
{
	struct nvme_host *host = drvdata;
	nvme_io_error_t error;

	if (port_no != 0) {
		printf ("Cannot open because port is not zero\n");
		return false;
	}

	uint n_ns;
	error = nvme_io_get_n_ns (host, &n_ns);
	if (error) {
		printf ("Cannot get number of namespace, error 0x%X\n", error);
		return false;
	}

	if (!(dev_no >= 1 && dev_no <= n_ns)) {
		printf ("Cannot open because dev_no is wrong\n");
		return false;
	}

	return nvme_io_host_ready (host) == NVME_IO_ERROR_OK;
}

static bool
ata_to_nvme_command (void *drvdata, int port_no, int dev_no,
		     struct storage_hc_dev_atacmd *cmd, int cmdsize)
{
	struct nvme_host *host = drvdata;

	if (cmdsize != sizeof (struct storage_hc_dev_atacmd))
		return false;

	if (!nvme_openable (drvdata, port_no, dev_no)) {
		printf ("Not ready\n");
		return false;
	}

	/* XXX: support only read and write commands */
	switch (cmd->command_status) {
		case ATA_OPCODE_READ:
			return nvme_io_ata_rw_request (host,
						       ATA_OPCODE_READ,
						       dev_no,
						       cmd);
		case ATA_OPCODE_WRITE:
			return nvme_io_ata_rw_request (host,
						       ATA_OPCODE_WRITE,
						       dev_no,
						       cmd);
		default:
			break;
	}

	printf ("Unknown command\n");

	return false;
}

static nvme_io_error_t
nvme_storage_io_init (struct nvme_host *host)
{
	nvme_io_error_t error;

	static struct storage_hc_driver_func hc_driver_func = {
		.scandev    = nvme_scandev,
		.openable   = nvme_openable,
		.atacommand = ata_to_nvme_command,
	};

	if (storage_count > MAX_STORAGE) {
		printf ("Number of NVMe Storage HC exceeds limit");
		return NVME_IO_ERROR_INTERNAL_ERROR;
	}

	struct pci_device *device;
	error = nvme_io_get_pci_device (host, &device);
	if (error) {
		printf ("Cannot get pci device, error 0x%X\n", error);
		return NVME_IO_ERROR_INTERNAL_ERROR;
	}

	struct nvme_storage_meta *storage_meta;
	storage_meta = &storage_metas[storage_count];

	STORAGE_HC_ADDR_PCI (storage_meta->hc_addr.addr, device);
	storage_meta->hc_addr.num_ports = 1;
	storage_meta->hc = storage_hc_register (&storage_meta->hc_addr,
						&hc_driver_func,
						host);
	printf ("NVMe Storage HC %u registered\n", storage_count);
	storage_count++;

	return NVME_IO_ERROR_OK;
}

static void
nvme_storage_io_ext (void)
{
	nvme_io_register_ext ("storage_io", nvme_storage_io_init);
}

INITFUNC ("driver1", nvme_storage_io_ext);
