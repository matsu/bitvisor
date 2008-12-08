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
 * @file	drivers/ehci.c
 * @brief	generic EHCI para pass-through driver
 * @author	K. Matsubara
 */
#include <core.h>
#include "pci.h"

static const char driver_name[] = "ehci_generic_driver";
static const char driver_longname[] = 
	"Generic EHCI para pass-through driver 0.1";
static const char virtual_model[40] = 
	"BitVisor Virtual USB2 Host Controller   ";
static const char virtual_revision[8] = "1.0     "; // 8 chars

static void 
ehci_new(struct pci_device *pci_device)
{
	printf("An EHCI host controller found. Disable it.\n");
	return;
}

static int 
ehci_config_read(struct pci_device *pci_device, 
		 core_io_t io, u8 offset, union mem *data)
{
	/* provide fake values 
	   for reading the PCI configration space. */
	data->dword = 0UL;
	return CORE_IO_RET_DONE;
}

static int 
ehci_config_write(struct pci_device *pci_device, 
		  core_io_t io, u8 offset, union mem *data)
{
	/* do nothing, ignore any writing. */
	return CORE_IO_RET_DONE;
}

static struct pci_driver ehci_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.id		= { PCI_ID_ANY, PCI_ID_ANY_MASK },
	.class		= { 0x0C0320, 0xFFFFFF },
	.new		= ehci_new,	
	.config_read	= ehci_config_read,
	.config_write	= ehci_config_write,
};

/**
 * @brief	driver init function automatically called at boot time
 */
void 
ehci_init(void) __initcode__
{
#if defined(EHCI_CONCEALER)
	pci_register_driver(&ehci_driver);
#endif
	return;
}
PCI_DRIVER_INIT(ehci_init);
