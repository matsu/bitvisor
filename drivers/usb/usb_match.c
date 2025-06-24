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
#include <token.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_driver.h"
#include "usb_match.h"

/* usb_match implementation based on drivers/pci_match.c
 *
 * Since a USB device can have multiple interfaces, get_value() like
 * pci_match.c cannot be used.  Instead, match_value() is added.
 */

enum match_ret {
	INVALID_NAME,
	NOT_MATCHED,
	MATCHED,
};

struct usb_match_number_list {
	struct usb_match_number_list *next;
	char *p;
	int number;
};

/*
   [<device selection>, ]driver=<driver name>[, <driver options>][, and, ...]

   * device selection
   id=%04x:%04x                (vendor_id, product_id)
   port=%u-%u                  (host_id, portno)
     port=%u-%u.%u             if a device is connected to a hub
     port=%u-%u.%u.%u          if multiple hubs are connected
   class=%02x                  (bDeviceClass or bInterfaceClass)
   subclass=%02x               (bDeviceSubClass or bInterfaceSubClass)
   protocol=%02x               (bDeviceProtocol or bInterfaceProtocol)
   csp=%02x-%02x-%02x          (class, subclass, protocol packed)
   serial=%s                   (serial)
   hosttype=%s                 (uhci, ehci or xhci)

   Examples:
   - Enable MSCD driver
     vmm.driver.usb=driver=mscd
   - Enable MSCD driver for vendor_id=0x1234 only
     vmm.driver.usb=device=mscd, id=1234:*, driver=mscd
   - Conceal vendor_id=0x1234 devices, except port 1-1
     vmm.driver.usb=port=1-1*, driver=none, and, id=1234:*, driver=conceal
   - Conceal human interface devices
     vmm.driver.usb=class=03, driver=conceal
   - Conceal all devices except human interface devices
     vmm.driver.usb=class=03, driver=none, and, id=*, driver=conceal
*/
static enum match_ret
match_value (struct token *tname, struct token *tvalue, struct usb_host *host,
	     struct usb_device *dev)
{
	char buf[64];
	const int bufsize = sizeof buf;
	char tmpbuf[8][5];
	u64 portno;
	u8 port;
	int i;

	if (match_token ("id", tname)) {
		snprintf (buf, bufsize, "%04x:%04x",
			  dev->descriptor.idVendor,
			  dev->descriptor.idProduct);
		return match_token (buf, tvalue) ? MATCHED : NOT_MATCHED;
	}
	if (match_token ("port", tname)) {
		portno = dev->portno;
		for (i = 0; i < 8; i++) {
			if (portno) {
				port = portno & 0xFF;
				portno >>= 8;
				snprintf (tmpbuf[i], 5, "%c%u",
					  portno ? '.' : '-', port);
			} else {
				tmpbuf[i][0] = '\0';
			}
		}
		snprintf (buf, bufsize, "%u%s%s%s%s%s%s%s%s", host->host_id,
			  tmpbuf[7], tmpbuf[6], tmpbuf[5], tmpbuf[4],
			  tmpbuf[3], tmpbuf[2], tmpbuf[1], tmpbuf[0]);
		return match_token (buf, tvalue) ? MATCHED : NOT_MATCHED;
	}
	if (match_token ("class", tname)) {
		snprintf (buf, bufsize, "%02x", dev->descriptor.bDeviceClass);
		if (match_token (buf, tvalue))
			return MATCHED;
		i = 0;
		if (dev->config && dev->config->interface &&
		    dev->config->interface->altsetting)
			i = dev->config->interface->num_altsetting;
		while (i-- > 0) {
			snprintf (buf, bufsize, "%02x",
				  dev->config->interface->altsetting[i].
				  bInterfaceClass);
			if (match_token (buf, tvalue))
				return MATCHED;
		}
		return NOT_MATCHED;
	}
	if (match_token ("subclass", tname)) {
		snprintf (buf, bufsize, "%02x",
			  dev->descriptor.bDeviceSubClass);
		if (match_token (buf, tvalue))
			return MATCHED;
		i = 0;
		if (dev->config && dev->config->interface &&
		    dev->config->interface->altsetting)
			i = dev->config->interface->num_altsetting;
		while (i-- > 0) {
			snprintf (buf, bufsize, "%02x",
				  dev->config->interface->altsetting[i].
				  bInterfaceSubClass);
			if (match_token (buf, tvalue))
				return MATCHED;
		}
		return NOT_MATCHED;
	}
	if (match_token ("protocol", tname)) {
		snprintf (buf, bufsize, "%02x",
			  dev->descriptor.bDeviceProtocol);
		if (match_token (buf, tvalue))
			return MATCHED;
		i = 0;
		if (dev->config && dev->config->interface &&
		    dev->config->interface->altsetting)
			i = dev->config->interface->num_altsetting;
		while (i-- > 0) {
			snprintf (buf, bufsize, "%02x",
				  dev->config->interface->altsetting[i].
				  bInterfaceProtocol);
			if (match_token (buf, tvalue))
				return MATCHED;
		}
		return NOT_MATCHED;
	}
	if (match_token ("csp", tname)) {
		snprintf (buf, bufsize, "%02x-%02x-%02x",
			  dev->descriptor.bDeviceClass,
			  dev->descriptor.bDeviceSubClass,
			  dev->descriptor.bDeviceProtocol);
		if (match_token (buf, tvalue))
			return MATCHED;
		i = 0;
		if (dev->config && dev->config->interface &&
		    dev->config->interface->altsetting)
			i = dev->config->interface->num_altsetting;
		while (i-- > 0) {
			snprintf (buf, bufsize, "%02x-%02x-%02x",
				  dev->config->interface->altsetting[i].
				  bInterfaceClass,
				  dev->config->interface->altsetting[i].
				  bInterfaceSubClass,
				  dev->config->interface->altsetting[i].
				  bInterfaceProtocol);
			if (match_token (buf, tvalue))
				return MATCHED;
		}
		return NOT_MATCHED;
	}
	if (match_token ("serial", tname)) {
		buf[0] = '\0';
		if (dev->descriptor.iSerialNumber != 0) {
			usb_dev_handle *handle;

			handle = usb_open (dev);
			if (handle) {
				char *tmp;
				int len;

				tmp = alloc (128);
				len = usb_get_string (handle, dev->descriptor.
						      iSerialNumber, 0x0409,
						      tmp, 128);
				if (len > 0) {
					for (i = 0; i < 63 && i * 2 + 2 < len;
					     i++) {
						buf[i] = tmp[i * 2 + 2];
						if (tmp[i * 2 + 3] != '\0' ||
						    buf[i] < ' ' ||
						    buf[i] > '~')
							buf[i] = '?';
					}
					buf[i] = '\0';
				}
				free (tmp);
				usb_close (handle);
			}
		}
		return match_token (buf, tvalue) ? MATCHED : NOT_MATCHED;
	}
	if (match_token ("hosttype", tname)) {
		switch (host->type) {
		case USB_HOST_TYPE_UHCI:
			if (match_token ("uhci", tvalue))
				return MATCHED;
			break;
		case USB_HOST_TYPE_EHCI:
			if (match_token ("ehci", tvalue))
				return MATCHED;
			break;
		case USB_HOST_TYPE_XHCI:
			if (match_token ("xhci", tvalue))
				return MATCHED;
			break;
		}
		return NOT_MATCHED;
	}
	return INVALID_NAME;
}

bool
usb_match (struct usb_host *host, struct usb_device *dev,
	   struct usb_driver *driver)
{
	struct token tname, tvalue;
	char *p = driver->device;
	char c;

	for (;;) {
		c = get_token (p, &tname);
		if (tname.start == tname.end)
			break;
		if (!tname.start)
			panic ("usb_match: syntax error 1 %s", p);
		if (c != '=')
			panic ("usb_match: syntax error 2 %s", p);
		c = get_token (tname.next, &tvalue);
		if (!tvalue.start)
			panic ("usb_match: syntax error 3 %s", p);
		if (c != ',' && c != '\0')
			panic ("usb_match: syntax error 4 %s", p);
		switch (match_value (&tname, &tvalue, host, dev)) {
		case INVALID_NAME:
			panic ("%s: invalid name %s", __func__, p);
		case NOT_MATCHED:
			return false;
		case MATCHED:
			break;
		}
		p = tvalue.next;
	}
	return true;
}

static void
alloc_driver_options (struct usb_device *device, struct usb_driver *driver)
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
save_driver_options (struct usb_device *device, struct usb_driver *driver,
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
usb_match_device_selection (struct usb_host *host, struct usb_device *dev,
			    char **p, struct token *tname,
			    struct token *tvalue, char c,
			    unsigned int *device_selection)
{
	static struct usb_match_number_list *number_list = NULL;
	struct usb_match_number_list *np;
	struct usb_driver *driver;
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
			driver = usb_find_driver_by_token (tvalue);
			if (!driver)
				panic ("%s: invalid device name %s",
				       __func__, *p);
			if (!usb_match (host, dev, driver))
				*device_selection |= 2;
			continue;
		}
		switch (match_value (tname, tvalue, host, dev)) {
		case INVALID_NAME:
			panic ("%s: invalid name %s", __func__, *p);
		case NOT_MATCHED:
			*device_selection |= 2;
			break;
		case MATCHED:
			break;
		}
	}
}

static void
usb_match_driver_selection (struct usb_host *host, struct usb_device *dev,
			    char **p, struct token *tname,
			    struct token *tvalue,
			    unsigned int device_selection,
			    struct usb_driver **ret_drv, bool *driver_selected)
{
	bool match = !(device_selection & 2);
	struct usb_driver *driver;
	char c;

	if (match_token ("none", tvalue)) {
		driver = NULL;
	} else {
		driver = usb_find_driver_by_token (tvalue);
		if (!driver)
			panic ("%s: invalid driver name %s", __func__, *p);
	}
	if (!(device_selection & 1))
		if (!driver || !usb_match (host, dev, driver))
			match = false;
	if (*driver_selected)
		match = false;
	if (match) {
		*ret_drv = driver;
		*driver_selected = true;
		if (driver)
			alloc_driver_options (dev, driver);
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
		save_driver_options (match ? dev : NULL, driver, tname,
				     tvalue);
	}
}

struct usb_driver *
usb_match_find_driver (struct usb_host *host, struct usb_device *dev, char *p)
{
	struct usb_driver *driver = NULL;
	struct token tname, tvalue;
	char c;
	unsigned int device_selection;
	bool driver_selected = false;

	if (dev->driver_options)
		panic ("usb_match_find_driver: dev->driver_options != NULL");
	for (;;) {
		c = get_token (p, &tname);
		if (tname.start == tname.end)
			break;
		usb_match_device_selection (host, dev, &p, &tname, &tvalue,
					    c, &device_selection);
		usb_match_driver_selection (host, dev, &p, &tname, &tvalue,
					    device_selection, &driver,
					    &driver_selected);
	}
	return driver;
}
