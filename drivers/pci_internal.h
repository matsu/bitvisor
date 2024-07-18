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

#ifndef _PCI_INTERNAL_H
#define _PCI_INTERNAL_H

#include <core/list.h>
#include <core/types.h>
#include <io.h>
#include "pci_init.h"

struct pci_device;

struct pci_config_mmio_data {
	u64 base;
	u16 seg_group;
	u8 bus_start;
	u8 bus_end;
	phys_t phys;
	uint len;
	void *map;
};

struct pci_segment {
	LIST1_DEFINE (struct pci_segment);
	LIST1_DEFINE_HEAD (struct pci_device, pci_device_list);
	struct pci_config_mmio_data *mmio;
	struct pci_virtual_device **pci_virtual_devices[32];
	struct pci_device *bridge_from_bus_no[PCI_MAX_BUSES];
	u16 seg_no; /* Store segment number separatedly as mmio can be NULL */
};

struct pci_segment_list {
	LIST1_DEFINE_HEAD (struct pci_segment, head);
};

struct pci_msi_callback {
	struct pci_msi_callback *next;
	struct pci_device *pci_device;
	bool (*callback) (struct pci_device *pci_device, void *data);
	void *data;
	u32 maddr;
	u32 mupper;
	u16 mdata;
	bool enable;
};

/******************************************************************************
 * PCI internal definitions and interfaces
 *****************************************************************************/
#define PCI_CONFIG_ADDR_PORT	0x0CF8
#define PCI_CONFIG_DATA_PORT	0x0CFC

extern int pci_config_data_handler (core_io_t io, union mem *data, void *arg);
extern int pci_config_addr_handler (core_io_t io, union mem *data, void *arg);
struct pci_segment *pci_get_segment (u16 seg_no);
void pci_pmio_save_config_addr (void);
void pci_append_device (struct pci_segment *s, struct pci_device *dev);
int pci_config_mmio_handler (void *data, phys_t gphys, bool wr, void *buf,
			     uint len, u32 flags);
void pci_config_pmio_enter (void);
void pci_config_pmio_leave (void);

extern struct pci_segment_list pci_segment_list;
extern struct pci_msi_callback *pci_msi_callback_list;

#endif
