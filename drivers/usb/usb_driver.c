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
#include <core/printf.h>
#include <core/spinlock.h>
#include <token.h>
#include "usb_driver.h"
#include "usb_match.h"

static LIST1_DEFINE_HEAD (struct usb_driver, usb_driver_list);
static spinlock_t usb_driver_lock;

void
usb_register_driver (struct usb_driver *driver)
{
	spinlock_lock (&usb_driver_lock);
	LIST1_ADD (usb_driver_list, driver);
	spinlock_unlock (&usb_driver_lock);
	if (driver->longname)
		printf ("%s registered\n", driver->longname);
}

struct usb_driver *
usb_find_driver_by_token (struct token *name)
{
	struct usb_driver *driver;
	int i;

	spinlock_lock (&usb_driver_lock);
	LIST1_FOREACH (usb_driver_list, driver) {
		if (!driver->name || driver->name[0] == '\0')
			continue;
		for (i = 0; &name->start[i] != name->end; i++)
			if (driver->name[i] != name->start[i])
				break;
		if (&name->start[i] == name->end && driver->name[i] == '\0')
			break;
	}
	spinlock_unlock (&usb_driver_lock);
	return driver;
}

static struct usb_driver *
usb_find_default_driver (struct usb_host *host, struct usb_device *dev)
{
	struct usb_driver *driver;
	spinlock_lock (&usb_driver_lock);
	LIST1_FOREACH (usb_driver_list, driver) {
		if (driver->compat && usb_match (host, dev, driver))
			break;
	}
	spinlock_unlock (&usb_driver_lock);
	return driver;
}

/* If config.vmm.driver.usb is empty, activate all compat drivers for
 * compatibility. */
struct usb_driver *
usb_find_driver (struct usb_host *host, struct usb_device *dev)
{
	char *usb_driver = config.vmm.driver.usb;
	if (usb_driver[0] == '\0')
		return usb_find_default_driver (host, dev);
	return usb_match_find_driver (host, dev, usb_driver);
}

static void
usb_driver_init (void)
{
	LIST1_HEAD_INIT (usb_driver_list);
	spinlock_init (&usb_driver_lock);
	call_initfunc ("usbdrv");
}

DRIVER_INIT (usb_driver_init);
