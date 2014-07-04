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

#include <core/config.h>
#include <core/initfunc.h>
#include <token.h>
#include "pci.h"
#include "pci_match.h"

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
static void
convert_pci_conceal (char *p)
{
	char c, *buf;
	int buf_size, buf_offset;
	struct token tname, tvalue;

	if (*p == '\0')
		return;
	buf_size = strlen (p) * 2;
	buf = alloc (buf_size);
	buf_offset = 0;
	for (;;) {
		c = get_token (p, &tname);
		if (tname.start == tname.end)
			break;
		if (!tname.start)
			panic ("%s: syntax error 1 %s", __func__, p);
		if (c != '=')
			panic ("%s: syntax error 2 %s", __func__, p);
		c = get_token (tname.next, &tvalue);
		if (!tvalue.start)
			panic ("%s: syntax error 3 %s", __func__, p);
		if (c != ',' && c != '\0')
			panic ("%s: syntax error 4 %s", __func__, p);
		if (match_token ("action", &tname)) {
			if (match_token ("allow", &tvalue))
				p = "driver=none,and,";
			else if (match_token ("deny", &tvalue))
				p = "driver=conceal,and,";
			else
				panic ("%s: invalid action %s", __func__, p);
			while (*p != '\0') {
				buf[buf_offset++] = *p++;
				if (buf_offset >= buf_size)
					panic ("%s: buffer overflow",
					       __func__);
			}
		} else {
			while (p != tvalue.next) {
				buf[buf_offset++] = *p++;
				if (buf_offset >= buf_size)
					panic ("%s: buffer overflow",
					       __func__);
			}
		}
		p = tvalue.next;
	}
	buf[buf_offset] = '\0';
	pci_match_add_compat (buf);
}

static void
pci_match_compat_init_pro1000 (void)
{
	static char buf[256];

	buf[0] = '\0';
	if (config.vmm.driver.concealPRO1000)
		snprintf (buf, sizeof buf, "%s",
#ifdef NET_PRO1000
			  config.vmm.tty_pro1000 ? "driver=pro1000,tty=1" :
#endif
			  "device=pro1000,driver=conceal");
#ifdef NET_PRO1000
	else if (config.vmm.driver.vpn.PRO1000)
		snprintf (buf, sizeof buf, "driver=pro1000,net=vpn%s",
			  config.vmm.tty_pro1000 ? ",tty=1" : "");
	else if (config.vmm.tty_pro1000)
		snprintf (buf, sizeof buf, "driver=pro1000,tty=1");
#endif
	if (buf[0] != '\0')
		pci_match_add_compat (buf);
}

static void
pci_match_compat_init_rtl8169 (void)
{
#ifdef NET_RTL8169
	static char buf[256];

	if (config.vmm.driver.vpn.RTL8169 || config.vmm.tty_rtl8169) {
		snprintf (buf, sizeof buf, "driver=rtl8169%s%s",
			  config.vmm.driver.vpn.RTL8169 ?
			  ",net=vpn" : "",
			  config.vmm.tty_rtl8169 ? ",tty=1" :
			  "");
		pci_match_add_compat (buf);
	}
#endif
}

static void
pci_match_compat_init (void)
{
	convert_pci_conceal (config.vmm.driver.pci_conceal);
	if (config.vmm.driver.usb.uhci)
		pci_match_add_compat ("driver=uhci");
	if (config.vmm.driver.concealEHCI)
		pci_match_add_compat ("driver=ehci_conceal");
	else if (config.vmm.driver.usb.ehci)
		pci_match_add_compat ("driver=ehci");
	if (config.vmm.driver.vga_intel)
		pci_match_add_compat ("driver=vga_intel");
	if (config.vmm.driver.conceal1394 && !config.vmm.tty_ieee1394)
		pci_match_add_compat ("driver=ieee1394");
	if (config.vmm.driver.ata)
		pci_match_add_compat ("driver=ata,and,driver=ahci,and,"
				      "driver=raid");
	pci_match_compat_init_pro1000 ();
	if (config.vmm.driver.vpn.PRO100)
		pci_match_add_compat ("driver=pro100,net=vpn");
	pci_match_compat_init_rtl8169 ();
	if (config.vmm.tty_ieee1394)
		pci_match_add_compat ("driver=ieee1394log,and,"
				      "driver=thunderbolt_conceal");
	if (config.vmm.tty_x540)
		pci_match_add_compat ("driver=x540");
}

INITFUNC ("driver0", pci_match_compat_init);
