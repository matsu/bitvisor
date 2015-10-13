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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned char		u8;
typedef unsigned short int	u16;
typedef unsigned int		u32;
typedef unsigned long long int	u64;
#define __CORE_TYPES_H
#include "core/config.h"

#include "configparser.h"

void loadic (char **name, void **val, int *len);
void loadusb (char **name, void **val, int *len);

static void
file (char **name, void **val, int *len)
{
	FILE *fp;
	long size;

	fp = fopen (*val, "r");
	if (!fp) {
		perror (*val);
		exit (EXIT_FAILURE);
	}
	if (fseek (fp, 0L, SEEK_END) < 0) {
		perror ("fseek");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	size = ftell (fp);
	if (size < 0) {
		perror ("ftell");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	rewind (fp);
	free (*val);
	*val = malloc (size);
	*len = size;
	if (!*val) {
		perror ("malloc");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	if (fread (*val, size, 1, fp) != 1) {
		perror ("fread");
		fclose (fp);
		exit (EXIT_FAILURE);
	}
	fclose (fp);
}

void
setdst (char *name1, char *name2, void *p, int plen, void **dst, int *dstlen)
{
	if (strcasecmp (name1, name2) == 0) {
		*dst = p;
		*dstlen = plen;
	}
}

static void
ss (void (*func) (char **name, void **val, int *len), char **name,
    void **val, int *len, char *nameorig, char *namenew)
{
	if (strcasecmp (*name, nameorig) != 0)
		return;
	free (*name);
	*name = strdup (namenew);
	func (name, val, len);
}

static void
ssi (void (*func) (char **name, void **val, int *len), char **name,
     void **val, int *len, char *nameorig, char *namenew, int i)
{
	char tmpname[128], tmpname2[128];

	snprintf (tmpname, sizeof tmpname, nameorig, i);
	snprintf (tmpname2, sizeof tmpname2, namenew, i);
	ss (func, name, val, len, tmpname, tmpname2);
}

static void
keyplace (char **name, void **val, int *len)
{
	char *value;

	value = *val;
	if (strcasecmp (value, "IC") == 0)
		loadic (name, val, len);
	else if (strcasecmp (value, "USB") == 0)
		loadusb (name, val, len);
	else if (value[0] == '/' || (value[0] == '.' && value[1] == '/'))
		file (name, val, len);
	else {
		fprintf (stderr, "syntax error\n");
		exit (EXIT_FAILURE);
	}
}

static void
storage_conf_guid (char **name, void **val, int *len)
{
	/* GUID? */
	*len = 0;
}

static void
storage_conf_type (char **name, void **val, int *len)
{
	enum storage_type t;

	if (strcasecmp (*val, "NULL") == 0)
		t = STORAGE_TYPE_NULL;
	else if (strcasecmp (*val, "ATA") == 0)
		t = STORAGE_TYPE_ATA;
	else if (strcasecmp (*val, "ATAPI") == 0)
		t = STORAGE_TYPE_ATAPI;
	else if (strcasecmp (*val, "USB") == 0)
		t = STORAGE_TYPE_USB;
	else if (strcasecmp (*val, "AHCI") == 0)
		t = STORAGE_TYPE_AHCI;
	else if (strcasecmp (*val, "AHCI_ATAPI") == 0)
		t = STORAGE_TYPE_AHCI_ATAPI;
	else if (strcasecmp (*val, "ANY") == 0)
		t = STORAGE_TYPE_ANY;
	else {
		fprintf (stderr, "storage_conf_type error\n");
		exit (EXIT_FAILURE);
	}
	free (*val);
	*val = malloc (sizeof t);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &t, sizeof t);
	*len = sizeof t;
}

static void
u8num (char **name, void **val, int *len)
{
	u8 t;

	t = strtol (*val, NULL, 0);
	free (*val);
	*val = malloc (sizeof t);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &t, sizeof t);
	*len = sizeof t;
}

static void
u16num (char **name, void **val, int *len)
{
	u16 t;

	t = strtol (*val, NULL, 0);
	free (*val);
	*val = malloc (sizeof t);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &t, sizeof t);
	*len = sizeof t;
}

static void
uintnum (char **name, void **val, int *len)
{
	unsigned int t;

	t = strtol (*val, NULL, 0);
	free (*val);
	*val = malloc (sizeof t);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &t, sizeof t);
	*len = sizeof t;
}

static void
u64num (char **name, void **val, int *len)
{
	u64 t;

	t = strtoll (*val, NULL, 0);
	free (*val);
	*val = malloc (sizeof t);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &t, sizeof t);
	*len = sizeof t;
}

static void
mac_addr (char **name, void **val, int *len)
{
	u8 mac_address[6];
	int i;
	char *tmp, *tmp2;

	tmp = tmp2 = *val;
	mac_address[0] = strtol (tmp, &tmp2, 16);
	if (tmp == tmp2)
		goto fail;
	for (i = 1; i < 6; i++) {
		if (*tmp2 == '\0')
			goto fail;
		tmp = tmp2 + 1;
		mac_address[i] = strtol (tmp, &tmp2, 16);
		if (tmp == tmp2)
			goto fail;
	}
	if (*tmp2 != '\0') {
	fail:
		fprintf (stderr, "invalid MAC address\n");
		exit (EXIT_FAILURE);
	}
	free (*val);
	*val = malloc (sizeof mac_address);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &mac_address, sizeof mac_address);
	*len = sizeof mac_address;
}

static int
conv_ipv4addr (char *str)
{
	char *p;
	int ret = 0, shift = 32, tmp;

	tmp = (int)strtol (str, &p, 0);
	while (*p == '.' && shift) {
		ret |= (tmp & 255) << (shift -= 8);
		tmp = (int)strtol (p + 1, &p, 0) & ((1 << shift) - 1);
	}
	if (*p != '\0' || !shift) {
		fprintf (stderr, "invalid IPv4 address\n");
		exit (EXIT_FAILURE);
	}
	return ret | tmp;
}

static void
ipv4_addr (char **name, void **val, int *len)
{
	u8 ipv4_address[4];
	int addr;

	addr = conv_ipv4addr (*val);
	ipv4_address[0] = addr >> 24;
	ipv4_address[1] = addr >> 16;
	ipv4_address[2] = addr >> 8;
	ipv4_address[3] = addr;
	free (*val);
	*val = malloc (sizeof ipv4_address);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &ipv4_address, sizeof ipv4_address);
	*len = sizeof ipv4_address;
}

static void
noconv (char **name, void **val, int *len)
{
}

static void
authmethod (char **name, void **val, int *len)
{
	enum idman_authmethod a;

	if (strcasecmp (*val, "None") == 0)
		a = IDMAN_AUTH_NONE;
	else if (strcasecmp (*val, "PKI") == 0)
		a = IDMAN_AUTH_PKI;
	else if (strcasecmp (*val, "ID/Password") == 0)
		a = IDMAN_AUTH_IDPASSWD;
	else if (strcasecmp (*val, "Password") == 0)
		a = IDMAN_AUTH_PASSWD;
	else {
		fprintf (stderr, "config error\n");
		exit (EXIT_FAILURE);
	}
	free (*val);
	*val = malloc (sizeof a);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	memcpy (*val, &a, sizeof a);
	*len = sizeof a;
}

void
setconfig (char *name, char *value, struct config_data *cfg)
{
	int len, dstlen;
	void *src, *dst;
	char tmpname[128];
	int i;

#define CONF0(NAMESTR, CFG) \
	setdst (name, NAMESTR, &CFG, sizeof CFG, &dst, &dstlen)
#define CONF(NAME) \
	CONF0 (#NAME, cfg->NAME)
#define CONF1(NAMEFMT, ARG1, CFG) \
	snprintf (tmpname, sizeof tmpname, NAMEFMT, ARG1), \
	CONF0 (tmpname, CFG)

	name = strdup (name);
	len = strlen (value);
	src = strdup (value);
	dst = NULL;
	dstlen = 0;
	/* idman */
	ss (file, &name, &src, &len, "idman.crl01File", "idman.crl01");
	ss (file, &name, &src, &len, "idman.crl02File", "idman.crl02");
	ss (file, &name, &src, &len, "idman.crl03File", "idman.crl03");
	ss (file, &name, &src, &len, "idman.pkc01File", "idman.pkc01");
	ss (file, &name, &src, &len, "idman.pkc02File", "idman.pkc02");
	ss (file, &name, &src, &len, "idman.pkc03File", "idman.pkc03");
	ss (uintnum, &name, &src, &len, "idman.randomSeedSize",
	    "idman.randomSeedSize");
	ss (uintnum, &name, &src, &len, "idman.maxPinLen", "idman.maxPinLen");
	ss (uintnum, &name, &src, &len, "idman.minPinLen", "idman.minPinLen");
	ss (authmethod, &name, &src, &len, "idman.authenticationMethod",
	    "idman.authenticationMethod");
	/* VPN */
	ss (file, &name, &src, &len, "vpn.vpnCertFileV4", "vpn.vpnCertV4");
	ss (file, &name, &src, &len, "vpn.vpnCaCertFileV4", "vpn.vpnCaCertV4");
	ss (file, &name, &src, &len, "vpn.vpnRsaKeyFileV4", "vpn.vpnRsaKeyV4");
	ss (file, &name, &src, &len, "vpn.vpnCertFileV6", "vpn.vpnCertV6");
	ss (file, &name, &src, &len, "vpn.vpnCaCertFileV6", "vpn.vpnCaCertV6");
	ss (file, &name, &src, &len, "vpn.vpnRsaKeyFileV6", "vpn.vpnRsaKeyV6");
	/* TCP/IP */
	ss (ipv4_addr, &name, &src, &len, "ip.ipaddr", "ip.ipaddr");
	ss (ipv4_addr, &name, &src, &len, "ip.netmask", "ip.netmask");
	ss (ipv4_addr, &name, &src, &len, "ip.gateway", "ip.gateway");
	ss (uintnum, &name, &src, &len, "ip.use_dhcp", "ip.use_dhcp");
	/* storage */
	for (i = 0; i < NUM_OF_STORAGE_KEYS; i++)
		ssi (keyplace, &name, &src, &len,
		     "storage.encryptionKey%d.place", "storage.keys[%d]", i);
	for (i = 0; i < NUM_OF_STORAGE_KEYS_CONF; i++) {
		ssi (storage_conf_guid, &name, &src, &len,
		     "storage.conf%d.guid", "storage.keys_conf[%d].guid", i);
		ssi (storage_conf_type, &name, &src, &len,
		     "storage.conf%d.type", "storage.keys_conf[%d].type", i);
		ssi (u8num, &name, &src, &len, "storage.conf%d.host_id",
		     "storage.keys_conf[%d].host_id", i);
		ssi (u16num, &name, &src, &len, "storage.conf%d.device_id",
		     "storage.keys_conf[%d].device_id", i);
		ssi (u64num, &name, &src, &len, "storage.conf%d.lba_low",
		     "storage.keys_conf[%d].lba_low", i);
		ssi (u64num, &name, &src, &len, "storage.conf%d.lba_high",
		     "storage.keys_conf[%d].lba_high", i);
		ssi (noconv, &name, &src, &len, "storage.conf%d.crypto_name",
		     "storage.keys_conf[%d].crypto_name", i);
		ssi (u8num, &name, &src, &len, "storage.conf%d.keyindex",
		     "storage.keys_conf[%d].keyindex", i);
		ssi (u16num, &name, &src, &len, "storage.conf%d.keybits",
		     "storage.keys_conf[%d].keybits", i);
		ssi (noconv, &name, &src, &len, "storage.conf%d.extend",
		     "storage.keys_conf[%d].extend", i);
	}
	/* vmm */
	ss (uintnum, &name, &src, &len, "vmm.f11panic", "vmm.f11panic");
	ss (uintnum, &name, &src, &len, "vmm.f12msg", "vmm.f12msg");
	ss (uintnum, &name, &src, &len, "vmm.auto_reboot", "vmm.auto_reboot");
	ss (uintnum, &name, &src, &len, "vmm.panic_reboot",
	    "vmm.panic_reboot");
	ss (uintnum, &name, &src, &len, "vmm.shell", "vmm.shell");
	ss (uintnum, &name, &src, &len, "vmm.dbgsh", "vmm.dbgsh");
	ss (uintnum, &name, &src, &len, "vmm.status", "vmm.status");
	ss (uintnum, &name, &src, &len, "vmm.boot_active", "vmm.boot_active");
	ss (uintnum, &name, &src, &len, "vmm.no_intr_intercept",
	    "vmm.no_intr_intercept");
	ss (uintnum, &name, &src, &len, "vmm.ignore_tsc_invariant",
	    "vmm.ignore_tsc_invariant");
	ss (uintnum, &name, &src, &len, "vmm.unsafe_nested_virtualization",
	    "vmm.unsafe_nested_virtualization");
	ss (mac_addr, &name, &src, &len, "vmm.tty_mac_address",
	    "vmm.tty_mac_address");
	ss (uintnum, &name, &src, &len, "vmm.tty_syslog.enable",
	    "vmm.tty_syslog.enable");
	ss (ipv4_addr, &name, &src, &len, "vmm.tty_syslog.src_ipaddr",
	    "vmm.tty_syslog.src_ipaddr");
	ss (ipv4_addr, &name, &src, &len, "vmm.tty_syslog.dst_ipaddr",
	    "vmm.tty_syslog.dst_ipaddr");
	ss (uintnum, &name, &src, &len, "vmm.tty_pro1000", "vmm.tty_pro1000");
	ss (uintnum, &name, &src, &len, "vmm.tty_rtl8169", "vmm.tty_rtl8169");
	ss (uintnum, &name, &src, &len, "vmm.tty_x540", "vmm.tty_x540");
	ss (uintnum, &name, &src, &len, "vmm.tty_ieee1394", "vmm.tty_ieee1394");
	ss (uintnum, &name, &src, &len, "vmm.driver.ata", "vmm.driver.ata");
	ss (uintnum, &name, &src, &len, "vmm.driver.usb.uhci", "vmm.driver.usb.uhci");
	ss (uintnum, &name, &src, &len, "vmm.driver.usb.ehci", "vmm.driver.usb.ehci");
	ss (uintnum, &name, &src, &len, "vmm.driver.concealEHCI",
	    "vmm.driver.concealEHCI");
	ss (uintnum, &name, &src, &len, "vmm.driver.conceal1394",
	    "vmm.driver.conceal1394");
	ss (uintnum, &name, &src, &len, "vmm.driver.concealPRO1000",
	    "vmm.driver.concealPRO1000");
	ss (uintnum, &name, &src, &len, "vmm.driver.vga_intel",
	    "vmm.driver.vga_intel");
	ss (uintnum, &name, &src, &len, "vmm.driver.vpn.PRO100",
	    "vmm.driver.vpn.PRO100");
	ss (uintnum, &name, &src, &len, "vmm.driver.vpn.PRO1000",
	    "vmm.driver.vpn.PRO1000");
	ss (uintnum, &name, &src, &len, "vmm.driver.vpn.RTL8169",
	    "vmm.driver.vpn.RTL8169");
	ss (uintnum, &name, &src, &len, "vmm.driver.vpn.ve",
	    "vmm.driver.vpn.ve");
	ss (uintnum, &name, &src, &len, "vmm.iccard.enable",
	    "vmm.iccard.enable");
	ss (uintnum, &name, &src, &len, "vmm.iccard.status",
	    "vmm.iccard.status");
	/* idman */
	CONF (idman.crl01);
	CONF (idman.crl02);
	CONF (idman.crl03);
	CONF (idman.pkc01);
	CONF (idman.pkc02);
	CONF (idman.pkc03);
	CONF (idman.randomSeedSize);
	CONF (idman.maxPinLen);
	CONF (idman.minPinLen);
	CONF (idman.authenticationMethod);
	/* VPN */
	CONF (vpn.mode);
	CONF (vpn.virtualGatewayMacAddress);
	CONF (vpn.bindV4);
	CONF (vpn.guestIpAddressV4);
	CONF (vpn.guestIpSubnetV4);
	CONF (vpn.guestMtuV4);
	CONF (vpn.guestVirtualGatewayIpAddressV4);
	CONF (vpn.dhcpV4);
	CONF (vpn.dhcpLeaseExpiresV4);
	CONF (vpn.dhcpDnsV4);
	CONF (vpn.dhcpDomainV4);
	CONF (vpn.adjustTcpMssV4);
	CONF (vpn.hostIpAddressV4);
	CONF (vpn.hostIpSubnetV4);
	CONF (vpn.hostMtuV4);
	CONF (vpn.hostIpDefaultGatewayV4);
	CONF (vpn.optionV4ArpExpires);
	CONF (vpn.optionV4ArpDontUpdateExpires);
	CONF (vpn.vpnGatewayAddressV4);
	CONF (vpn.vpnAuthMethodV4);
	CONF (vpn.vpnPasswordV4);
	CONF (vpn.vpnIdStringV4);
	CONF (vpn.vpnCertV4);
	CONF (vpn.vpnCaCertV4);
	CONF (vpn.vpnRsaKeyV4);
	CONF (vpn.vpnSpecifyIssuerV4);
	CONF (vpn.vpnPhase1ModeV4);
	CONF (vpn.vpnPhase1CryptoV4);
	CONF (vpn.vpnPhase1HashV4);
	CONF (vpn.vpnPhase1LifeSecondsV4);
	CONF (vpn.vpnPhase1LifeKilobytesV4);
	CONF (vpn.vpnWaitPhase2BlankSpanV4);
	CONF (vpn.vpnPhase2CryptoV4);
	CONF (vpn.vpnPhase2HashV4);
	CONF (vpn.vpnPhase2LifeSecondsV4);
	CONF (vpn.vpnPhase2LifeKilobytesV4);
	CONF (vpn.vpnConnectTimeoutV4);
	CONF (vpn.vpnIdleTimeoutV4);
	CONF (vpn.vpnPingTargetV4);
	CONF (vpn.vpnPingIntervalV4);
	CONF (vpn.vpnPingMsgSizeV4);
	CONF (vpn.bindV6);
	CONF (vpn.guestIpAddressPrefixV6);
	CONF (vpn.guestIpAddressSubnetV6);
	CONF (vpn.guestMtuV6);
	CONF (vpn.guestVirtualGatewayIpAddressV6);
	CONF (vpn.raV6);
	CONF (vpn.raLifetimeV6);
	CONF (vpn.raDnsV6);
	CONF (vpn.hostIpAddressV6);
	CONF (vpn.hostIpAddressSubnetV6);
	CONF (vpn.hostMtuV6);
	CONF (vpn.hostIpDefaultGatewayV6);
	CONF (vpn.optionV6NeighborExpires);
	CONF (vpn.vpnGatewayAddressV6);
	CONF (vpn.vpnAuthMethodV6);
	CONF (vpn.vpnPasswordV6);
	CONF (vpn.vpnIdStringV6);
	CONF (vpn.vpnCertV6);
	CONF (vpn.vpnCaCertV6);
	CONF (vpn.vpnRsaKeyV6);
	CONF (vpn.vpnSpecifyIssuerV6);
	CONF (vpn.vpnPhase1ModeV6);
	CONF (vpn.vpnPhase1CryptoV6);
	CONF (vpn.vpnPhase1HashV6);
	CONF (vpn.vpnPhase1LifeSecondsV6);
	CONF (vpn.vpnPhase1LifeKilobytesV6);
	CONF (vpn.vpnWaitPhase2BlankSpanV6);
	CONF (vpn.vpnPhase2CryptoV6);
	CONF (vpn.vpnPhase2HashV6);
	CONF (vpn.vpnPhase2LifeSecondsV6);
	CONF (vpn.vpnPhase2LifeKilobytesV6);
	CONF (vpn.vpnPhase2StrictIdV6);
	CONF (vpn.vpnConnectTimeoutV6);
	CONF (vpn.vpnIdleTimeoutV6);
	CONF (vpn.vpnPingTargetV6);
	CONF (vpn.vpnPingIntervalV6);
	CONF (vpn.vpnPingMsgSizeV6);
	/* TCP/IP */
	CONF (ip.ipaddr);
	CONF (ip.netmask);
	CONF (ip.gateway);
	CONF (ip.use_dhcp);
	/* storage */
	for (i = 0; i < NUM_OF_STORAGE_KEYS; i++)
		CONF1 ("storage.keys[%d]", i, cfg->storage.keys[i]);
	for (i = 0; i < NUM_OF_STORAGE_KEYS_CONF; i++) {
		CONF1 ("storage.keys_conf[%d].guid", i,
		       cfg->storage.keys_conf[i].guid);
		CONF1 ("storage.keys_conf[%d].type", i,
		       cfg->storage.keys_conf[i].type);
		CONF1 ("storage.keys_conf[%d].host_id", i,
		       cfg->storage.keys_conf[i].host_id);
		CONF1 ("storage.keys_conf[%d].device_id", i,
		       cfg->storage.keys_conf[i].device_id);
		CONF1 ("storage.keys_conf[%d].lba_low", i,
		       cfg->storage.keys_conf[i].lba_low);
		CONF1 ("storage.keys_conf[%d].lba_high", i,
		       cfg->storage.keys_conf[i].lba_high);
		CONF1 ("storage.keys_conf[%d].crypto_name", i,
		       cfg->storage.keys_conf[i].crypto_name);
		CONF1 ("storage.keys_conf[%d].keyindex", i,
		       cfg->storage.keys_conf[i].keyindex);
		CONF1 ("storage.keys_conf[%d].keybits", i,
		       cfg->storage.keys_conf[i].keybits);
		CONF1 ("storage.keys_conf[%d].extend", i,
		       cfg->storage.keys_conf[i].extend);
	}
	/* vmm */
	CONF (vmm.f11panic);
	CONF (vmm.f12msg);
	CONF (vmm.auto_reboot);
	CONF (vmm.panic_reboot);
	CONF (vmm.shell);
	CONF (vmm.dbgsh);
	CONF (vmm.status);
	CONF (vmm.boot_active);
	CONF (vmm.no_intr_intercept);
	CONF (vmm.ignore_tsc_invariant);
	CONF (vmm.unsafe_nested_virtualization);
	CONF (vmm.tty_mac_address);
	CONF (vmm.tty_syslog.enable);
	CONF (vmm.tty_syslog.src_ipaddr);
	CONF (vmm.tty_syslog.dst_ipaddr);
	CONF (vmm.tty_pro1000);
	CONF (vmm.tty_rtl8169);
	CONF (vmm.tty_x540);
	CONF (vmm.tty_ieee1394);
	CONF (vmm.driver.ata);
	CONF (vmm.driver.usb.uhci);
	CONF (vmm.driver.usb.ehci);
	CONF (vmm.driver.concealEHCI);
	CONF (vmm.driver.conceal1394);
	CONF (vmm.driver.concealPRO1000);
	CONF (vmm.driver.vga_intel);
	CONF (vmm.driver.vpn.PRO100);
	CONF (vmm.driver.vpn.PRO1000);
	CONF (vmm.driver.vpn.RTL8169);
	CONF (vmm.driver.vpn.ve);
	CONF (vmm.driver.pci_conceal);
	CONF (vmm.driver.pci);
	CONF (vmm.iccard.enable);
	CONF (vmm.iccard.status);
	if (!dst) {
		fprintf (stderr, "unknown config \"%s\"\n", name);
		exit (EXIT_FAILURE);
	}
	if (len > dstlen) {
		fprintf (stderr, "config \"%s\" is too long\n", name);
		exit (EXIT_FAILURE);
	}
	memset (dst, 0, dstlen);
	memcpy (dst, src, len);
	free (name);
	free (src);
}

void
configparser (char *buf, int bufsize, FILE *fp, struct config_data *cfg,
	      void (*setcfg) (char *, char *, struct config_data *))
{
	char *p, *value, *name;
	int i, len;

	while (fgets (buf, bufsize, fp)) {
		p = strchr (buf, '#');
		if (p) {
			p[0] = '\n';
			p[1] = '\0';
		}
		len = strlen (buf);
		if (len > 0 && buf[len - 1] != '\n') {
			fprintf (stderr, "line too long\n");
			exit (EXIT_FAILURE);
		}
		for (i = len; i > 0; i--) {
			if (!isspace (buf[i - 1])) {
				buf[i] = '\0';
				goto ok;
			}
		}
		continue;
	ok:
		for (p = buf; *p != '\0' && isspace (*p); p++);
		name = p;
		p = strchr (p, '=');
		if (!p) {
			fprintf (stderr, "syntax error\n");
			exit (EXIT_FAILURE);
		}
		*p = '\0';
		value = p + 1;
		setcfg (name, value, cfg);
	}
}

void
load_random_seed (struct config_data *cfg, char *devname)
{
	int r;
	FILE *fp;

	fp = fopen (devname, "r");
	if (!fp) {
		perror ("urandom");
		exit (EXIT_FAILURE);
	}
	r = fread (cfg->vmm.randomSeed, sizeof cfg->vmm.randomSeed, 1, fp);
	if (r != 1) {
		perror ("urandom");
		exit (EXIT_FAILURE);
	}
	fclose (fp);
}
