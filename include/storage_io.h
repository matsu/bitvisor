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

#ifndef _STORAGE_IO_H_
#define _STORAGE_IO_H_

#include <core/config.h>

/* Storage I/O API */

#define STORAGE_HC_ADDR_PCI(addr, pci_device) \
	snprintf (addr, sizeof addr, "PCI%02X:%02X.%X", \
		  pci_device->address.bus_no, pci_device->address.device_no, \
		  pci_device->address.func_no)

enum storage_hc_type {
	STORAGE_HC_TYPE_ATA,
	STORAGE_HC_TYPE_AHCI,
};

struct storage_hc;
struct storage_hc_addr;
struct storage_hc_dev;
struct storage_hc_dev_atacmd;
struct storage_hc_hook;

typedef void storage_hc_hook_callback_t (void *data,
					 struct storage_hc *handle,
					 const struct storage_hc_addr *addr);
typedef bool storage_hc_scandev_callback_t (void *data, int dev_no);
typedef void storage_hc_dev_atacommand_callback_t (void *data,
						struct storage_hc_dev_atacmd
						*cmd);

struct storage_hc_addr {
	char addr[16];		   /* Host controller address */
	enum storage_hc_type type; /* Host controller type */
	int num_ports;		   /* Number of ports */
	bool ncq;		   /* Native Command Queuing support */
};

struct storage_hc_dev_atacmd {
	u8 command_status;	/* Command | Status */
	u8 features_error;	/* Features | Error */
	u8 sector_number;	/* Sector Number */
	u8 cyl_low;		/* Cylinder Low */
	u8 cyl_high;		/* Cylinder High */
	u8 dev_head;		/* Device/Head */
	u8 sector_number_exp;	/* Sector Number exp */
	u8 cyl_low_exp;		/* Cylinder Low exp */
	u8 cyl_high_exp;	/* Cylinder High exp */
	u8 features_exp;	/* Features exp */
	u8 sector_count;	/* Sector Count */
	u8 sector_count_exp;	/* Sector Count exp */
	u8 control;		/* Device Control */
	u8 atapi_len;		/* ATAPI command length (0: ATA command) */
	u8 atapi[16];		/* ATAPI command */
	void *buf;
	phys_t buf_phys;
	int buf_len;
	storage_hc_dev_atacommand_callback_t *callback;
	void *data;
	bool write;		/* Direction */
	bool pio;		/* PIO/DMA */
	int ncq;		/* Native Command Queuing: max queue depth */
	int timeout_ready;	/* Timeout (usec) waiting for ready */
	int timeout_complete; /* Timeout (usec) waiting for completion */
};

int storage_get_num_hc (void);
struct storage_hc *storage_hc_open (int index, struct storage_hc_addr *addr,
				    struct storage_hc_hook *hook);
void storage_hc_close (struct storage_hc *handle);
struct storage_hc_hook *storage_hc_hook_register (storage_hc_hook_callback_t *
						  callback, void *data);
void storage_hc_hook_unregister (struct storage_hc_hook *handle);
int storage_hc_scandev (struct storage_hc *handle, int port_no,
			storage_hc_scandev_callback_t *callback, void *data);
struct storage_hc_dev *storage_hc_dev_open (struct storage_hc *handle, 
					    int port_no, int dev_no);
bool storage_hc_dev_atacommand (struct storage_hc_dev *handle,
			     struct storage_hc_dev_atacmd *cmd, int cmdsize);
void storage_hc_dev_close (struct storage_hc_dev *handle);

/* Driver API */

struct storage_hc_driver;

struct storage_hc_driver_func {
	int (*scandev) (void *drvdata, int port_no,
			storage_hc_scandev_callback_t *callback, void *data);
	bool (*openable) (void *drvdata, int port_no, int dev_no);
	bool (*atacommand) (void *drvdata, int port_no, int dev_no,
			    struct storage_hc_dev_atacmd *cmd, int cmdsize);
};

struct storage_hc_driver *storage_hc_register (struct storage_hc_addr *addr,
					       struct storage_hc_driver_func
					       *func, void *data);
void storage_hc_unregister (struct storage_hc_driver *handle);

#endif
