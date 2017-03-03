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
 * @file	drivers/ieee1394.c
 * @brief	generic IEEE1394 para pass-through driver based on ehci.c
 * @author	K. Matsubara, H. Eiraku
 */
#include <core.h>
#include "pci.h"

static const char driver_name[] = "ieee1394";
static const char driver_longname[] = 
	"Generic IEEE1394 para pass-through driver 0.1";

static void 
ieee1394_new(struct pci_device *pci_device)
{
	printf("An IEEE1394 host controller found. Disable it.\n");
	pci_system_disconnect (pci_device);
	return;
}

static int
ieee1394_config_read (struct pci_device *pci_device, u8 iosize,
		      u16 offset, union mem *data)
{
	ulong zero = 0UL;

	/* provide fake values 
	   for reading the PCI configration space. */
	memcpy (data, &zero, iosize);
	return CORE_IO_RET_DONE;
}

static int
ieee1394_config_write (struct pci_device *pci_device, u8 iosize,
		       u16 offset, union mem *data)
{
	/* do nothing, ignore any writing. */
	return CORE_IO_RET_DONE;
}

static struct pci_driver ieee1394_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.device		= "class_code=0c0010",
	.new		= ieee1394_new,	
	.config_read	= ieee1394_config_read,
	.config_write	= ieee1394_config_write,
};

/**
 * @brief	driver init function automatically called at boot time
 */
void 
ieee1394_init(void) __initcode__
{
#if defined(IEEE1394_CONCEALER)
	pci_register_driver(&ieee1394_driver);
#endif
	return;
}
PCI_DRIVER_INIT(ieee1394_init);
