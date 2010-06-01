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
#include <token.h>
#include "pci.h"
#include "pci_conceal.h"

/*
   slot=%02x:%02x.%u           (bus_no, device_no, func_no)
   class=%04x                  (class_code >> 8)
   id=%04x:%04x                (vendor_id, device_id)
   subsystem=%04x:%04x         (sub_vendor_id, sub_device_id)
   revision=%02x               (revision_id)
   rev=%02x                    (revision_id)
   programming_interface=%02x  (programming_interface)
   if=%02x                     (programming_interface)
   class_code=%06x             (class_code)
   header_type=%02x            (header_type)

   example:
   - hide all vendor_id=0x1234 devices, except slot 11:22.3
     vmm.driver.pci_conceal=slot=11:22.3, action=allow, id=1234:*, action=deny
   - hide all network controllers
     vmm.driver.pci_conceal=class=02*, action=deny
   - hide all network controllers except vendor_id=0x1234 devices
     vmm.driver.pci_conceal=class=02*, id=1234:*, action=allow,
     class=02*, action=deny
*/
static bool
get_value (char *buf, int bufsize, struct token *tname, struct pci_device *dev)
{
	if (match_token ("slot", tname)) {
		snprintf (buf, bufsize, "%02x:%02x.%u", dev->address.bus_no,
			  dev->address.device_no, dev->address.func_no);
		return true;
	}
	if (match_token ("class", tname)) {
		snprintf (buf, bufsize, "%04x",
			  dev->config_space.class_code >> 8);
		return true;
	}
	if (match_token ("id", tname)) {
		snprintf (buf, bufsize, "%04x:%04x",
			  dev->config_space.vendor_id,
			  dev->config_space.device_id);
		return true;
	}
	if (match_token ("subsystem", tname)) {
		snprintf (buf, bufsize, "%04x:%04x",
			  dev->config_space.sub_vendor_id,
			  dev->config_space.sub_device_id);
		return true;
	}
	if (match_token ("revision", tname) || match_token ("rev", tname)) {
		snprintf (buf, bufsize, "%02x", dev->config_space.revision_id);
		return true;
	}
	if (match_token ("programming_interface", tname) ||
	    match_token ("if", tname)) {
		snprintf (buf, bufsize, "%02x",
			  dev->config_space.programming_interface);
		return true;
	}
	if (match_token ("class_code", tname)) {
		snprintf (buf, bufsize, "%06x", dev->config_space.class_code);
		return true;
	}
	if (match_token ("header_type", tname)) {
		snprintf (buf, bufsize, "%02x", dev->config_space.header_type);
		return true;
	}
	return false;
}

static bool
pci_conceal (struct pci_device *dev, char *p)
{
	char c;
	struct token tname, tvalue;
	char buf[32];
	bool ac, skip;

	skip = false;
	for (;;) {
		c = get_token (p, &tname);
		if (tname.start == tname.end)
			break;
		if (!tname.start)
			panic ("pci_conceal: syntax error 1 %s", p);
		if (c != '=')
			panic ("pci_conceal: syntax error 2 %s", p);
		c = get_token (tname.next, &tvalue);
		if (!tvalue.start)
			panic ("pci_conceal: syntax error 3 %s", p);
		if (c != ',' && c != '\0')
			panic ("pci_conceal: syntax error 4 %s", p);
		if (match_token ("action", &tname)) {
			if (match_token ("allow", &tvalue))
				ac = false;
			else if (match_token ("deny", &tvalue))
				ac = true;
			else
				panic ("pci_conceal: invalid action %s", p);
			if (skip)
				skip = false;
			else
				return ac;
		} else if (!get_value (buf, sizeof buf, &tname, dev)) {
			panic ("pci_conceal: invalid name %s", p);
		} else if (!skip && !match_token (buf, &tvalue)) {
			skip = true;
		}
		p = tvalue.next;
	}
	return false;
}

static u32
getnum (u32 b)
{
	u32 r;

	for (r = 1; !(b & 1); b >>= 1)
		r <<= 1;
	return r;
}

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

bool
pci_conceal_new_device (struct pci_device *dev)
{
	int i;
	bool r;
	u32 a, b, num;

	r = pci_conceal (dev, config.vmm.driver.pci_conceal);
	if (!r)
		return false;
	for (i = 0; i < 6; i++) {
		a = dev->config_space.base_address[i];
		b = dev->base_address_mask[i];
		if (a == 0)
			continue;
		if ((a & PCI_CONFIG_BASE_ADDRESS_SPACEMASK) ==
		    PCI_CONFIG_BASE_ADDRESS_IOSPACE) {
			a &= PCI_CONFIG_BASE_ADDRESS_IOMASK;
			b &= PCI_CONFIG_BASE_ADDRESS_IOMASK;
			num = getnum (b);
			core_io_register_handler
				(a, num, iohandler, NULL,
				 CORE_IO_PRIO_EXCLUSIVE, "pci_conceal");
		} else {
			a &= PCI_CONFIG_BASE_ADDRESS_MEMMASK;
			b &= PCI_CONFIG_BASE_ADDRESS_MEMMASK;
			num = getnum (b);
			mmio_register (a, num, mmhandler, NULL);
		}
	}
	return true;
}

int
pci_conceal_config_data_handler (core_io_t io, union mem *data, void *arg)
{
	return iohandler (io, data, arg);
}
