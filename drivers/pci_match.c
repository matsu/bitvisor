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
#include "pci_match.h"

/*
   [<device selection>, ]driver=<driver name>[, <driver options>][, and, ...]

   * device selection
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
   device=%s                   (device name)
   number=%d                   (number)

   example:
   - enable uhci and ehci driver
     vmm.driver.pci=driver=uhci, and, driver=ehci
   - conceal ehci and pro1000
     vmm.driver.pci=device=ehci, driver=ehci_conceal, and,
     device=pro1000, driver=conceal
   - use first pro1000 for tty
     vmm.driver.pci=device=pro1000, number=0, driver=pro1000, tty=1
   - hide all vendor_id=0x1234 devices, except slot 11:22.3
     vmm.driver.pci=slot=11:22.3, driver=none, and, id=1234:*, driver=conceal
*/

struct pci_match_number_list {
	struct pci_match_number_list *next;
	char *p;
	int number;
};

struct compat_list {
	struct compat_list *next;
	char *str;
};

static struct compat_list *compat_list_head = NULL;

static bool
get_value (char *buf, int bufsize, struct token *tname, struct pci_device *dev)
{
	if (match_token ("slot", tname)) {
		snprintf (buf, bufsize, "%02x:%02x.%u", dev->initial_bus_no,
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

bool
pci_match (struct pci_device *device, struct pci_driver *driver)
{
	struct token tname, tvalue;
	char *p = driver->device;
	char buf[32], c;

	for (;;) {
		c = get_token (p, &tname);
		if (tname.start == tname.end)
			break;
		if (!tname.start)
			panic ("pci_match: syntax error 1 %s", p);
		if (c != '=')
			panic ("pci_match: syntax error 2 %s", p);
		c = get_token (tname.next, &tvalue);
		if (!tvalue.start)
			panic ("pci_match: syntax error 3 %s", p);
		if (c != ',' && c != '\0')
			panic ("pci_match: syntax error 4 %s", p);
		if (!get_value (buf, sizeof buf, &tname, device))
			panic ("pci_match: invalid name %s", p);
		else if (!match_token (buf, &tvalue))
			return false;
		p = tvalue.next;
	}
	return true;
}

static void
alloc_driver_options (struct pci_device *device, struct pci_driver *driver)
{
	int i, n = 0;
	struct token t;
	char c, *p = driver->driver_options;

	if (!p)
		return;
	for (;; p = t.next, n++) {
		c = get_token (p, &t);
		if (t.start == t.end)
			break;
		if (!t.start)
			panic ("%s: driver_options syntax error 1 %s",
			       __func__, p);
		if (c != ',' && c != '\0')
			panic ("%s: driver_options syntax error 2 %s",
			       __func__, p);
	}
	device->driver_options = alloc (sizeof *device->driver_options * n);
	for (i = 0; i < n; i++)
		device->driver_options[i] = NULL;
}

static void
save_driver_options (struct pci_device *device, struct pci_driver *driver,
		     struct token *name, struct token *value)
{
	int i = 0;
	struct token t;
	char c, *p = driver->driver_options;

	for (;; p = t.next, i++) {
		c = get_token (p, &t);
		if (t.start == t.end)
			break;
		if (!t.start)
			panic ("%s: driver_options syntax error 1 %s",
			       __func__, p);
		if (c != ',' && c != '\0')
			panic ("%s: driver_options syntax error 2 %s",
			       __func__, p);
		if (t.end - t.start != name->end - name->start)
			continue;
		if (memcmp (t.start, name->start, t.end - t.start))
			continue;
		goto found;
	}
	panic ("%s: invalid option name %s", __func__, name->start);
found:
	if (!device)
		return;
	if (device->driver_options[i])
		free (device->driver_options[i]);
	device->driver_options[i] = alloc (value->end - value->start + 1);
	memcpy (device->driver_options[i], value->start,
		value->end - value->start);
	device->driver_options[i][value->end - value->start] = '\0';
}

static void
pci_match_device_selection (struct pci_device *device, char **p,
			    struct token *tname, struct token *tvalue, char c,
			    unsigned int *device_selection)
{
	static struct pci_match_number_list *number_list = NULL;
	struct pci_match_number_list *np;
	struct pci_driver *driver;
	char buf[32];

	*device_selection = 0;
	goto start;
	for (;;) {
		*p = tvalue->next;
		c = get_token (*p, tname);
		if (tname->start == tname->end)
			panic ("%s: syntax error 0 %s", __func__, *p);
	start:
		if (!tname->start)
			panic ("%s: syntax error 1 %s", __func__, *p);
		if (c != '=')
			panic ("%s: syntax error 2 %s", __func__, *p);
		c = get_token (tname->next, tvalue);
		if (!tvalue->start)
			panic ("%s: syntax error 3 %s", __func__, *p);
		if (c != ',' && c != '\0')
			panic ("%s: syntax error 4 %s", __func__, *p);
		if (match_token ("driver", tname))
			return;
		if ((*device_selection & 1) && match_token ("number", tname)) {
			if (*device_selection & 2)
				continue;
			for (np = number_list; np; np = np->next)
				if (np->p == *p)
					break;
			if (!np) {
				np = alloc (sizeof *np);
				np->p = *p;
				np->number = 0;
				np->next = number_list;
				number_list = np;
			}
			snprintf (buf, sizeof buf, "%d", np->number++);
			if (!match_token (buf, tvalue))
				*device_selection |= 2;
			continue;
		}
		*device_selection |= 1;
		if (match_token ("device", tname)) {
			driver = pci_find_driver_by_token (tvalue);
			if (!driver)
				panic ("%s: invalid device name %s",
				       __func__, *p);
			if (!pci_match (device, driver))
				*device_selection |= 2;
			continue;
		}
		if (!get_value (buf, sizeof buf, tname, device))
			panic ("%s: invalid name %s", __func__, *p);
		if (!(*device_selection & 2) && !match_token (buf, tvalue))
			*device_selection |= 2;
	}
}

static void
pci_match_driver_selection (struct pci_device *device, char **p,
			    struct token *tname, struct token *tvalue,
			    unsigned int device_selection,
			    struct pci_driver **ret_drv, bool *driver_selected)
{
	bool match = !(device_selection & 2);
	struct pci_driver *driver;
	char c;

	if (match_token ("none", tvalue)) {
		driver = NULL;
	} else {
		driver = pci_find_driver_by_token (tvalue);
		if (!driver)
			panic ("%s: invalid driver name %s", __func__, *p);
	}
	if (!(device_selection & 1))
		if (!driver || !pci_match (device, driver))
			match = false;
	if (*driver_selected)
		match = false;
	if (match) {
		*ret_drv = driver;
		*driver_selected = true;
		if (driver)
			alloc_driver_options (device, driver);
	}
	for (;;) {
		*p = tvalue->next;
		c = get_token (*p, tname);
		if (tname->start == tname->end)
			return;
		if (!tname->start)
			panic ("%s: syntax error 1 %s", __func__, *p);
		if (match_token ("and", tname) && c == ',') {
			*p = tname->next;
			return;
		}
		if (c != '=')
			panic ("%s: syntax error 2 %s", __func__, *p);
		c = get_token (tname->next, tvalue);
		if (!tvalue->start)
			panic ("%s: syntax error 3 %s", __func__, *p);
		if (c != ',' && c != '\0')
			panic ("%s: syntax error 4 %s", __func__, *p);
		if (!driver)
			panic ("%s: syntax error 5 %s", __func__, *p);
		save_driver_options (match ? device : NULL, driver, tname,
				     tvalue);
	}
}

static struct pci_driver *
pci_match_find_driver_sub (struct pci_device *device, char *p)
{
	struct pci_driver *driver = NULL;
	struct token tname, tvalue;
	char c;
	unsigned int device_selection;
	bool driver_selected = false;

	if (device->driver_options)
		panic ("pci_match_find_driver:"
		       " device->driver_options != NULL");
	for (;;) {
		c = get_token (p, &tname);
		if (tname.start == tname.end)
			break;
		pci_match_device_selection (device, &p, &tname, &tvalue, c,
					    &device_selection);
		pci_match_driver_selection (device, &p, &tname, &tvalue,
					    device_selection, &driver,
					    &driver_selected);
	}
	return driver;
}

void
pci_match_add_compat (char *str)
{
	static struct compat_list **compat_list_next = &compat_list_head;
	struct compat_list *p;

	p = alloc (sizeof *p);
	p->str = str;
	p->next = NULL;
	*compat_list_next = p;
	compat_list_next = &p->next;
}

struct pci_driver *
pci_match_find_driver (struct pci_device *device)
{
	struct pci_driver *driver;
	struct compat_list *p;

	driver = pci_match_find_driver_sub (device, config.vmm.driver.pci);
	if (driver)
		return driver;
	for (p = compat_list_head; p; p = p->next) {
		driver = pci_match_find_driver_sub (device, p->str);
		if (driver)
			return driver;
	}
	return NULL;
}
