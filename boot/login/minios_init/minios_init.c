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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/klog.h>
#include <sys/mount.h>
#include "boot.h"
#include "configparser.h"
#include "keymapload.h"
#include "passinput.h"
#include "usbmem.h"
#include "IDMan.h"

#define INDEX_VPN 2
#define ENCKEY_INDEX 3

static unsigned long int session;
static int idmaninit, secondboot;

static void
get_ic_static_passwd (int index, void **val, int *len)
{
        struct idPasswordList *id, *pid;
	int ret, i;

	ret = IDMan_getStaticPassword (session, &id);
	if (ret) {
		fprintf (stderr, "IDMan_getStaticPassword %d\n", ret);
		exit (EXIT_FAILURE);
	}
	for (pid = id, i = 1; pid && i < index; i++)
		pid = pid->next;
	if (!pid) {
		fprintf (stderr, "pid error\n");
		exit (EXIT_FAILURE);
	}
	free (*val);
	*val = malloc (pid->passwordLen);
	if (!*val) {
		perror ("malloc");
		exit (EXIT_FAILURE);
	}
	*len = pid->passwordLen;
	memcpy (*val, pid->password, pid->passwordLen);
	while (id) {
		pid = id;
		id = id->next;
		free (pid);
	}
}

void
loadic (char **name, void **val, int *len)
{
	/* load a storage encryption key from IC card */
	if (!secondboot)
		return;
	if (!idmaninit) {
		fprintf (stderr, "storage configuration error\n");
		exit (EXIT_FAILURE);
	}
	get_ic_static_passwd (ENCKEY_INDEX, val, len);
}

void
loadusb (char **name, void **val, int *len)
{
	/* load a storage encryption key from USB storage */
	if (!secondboot)
		return;
	fprintf (stderr, "USB not implemented\n");
	exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
	FILE *fp;
	static char buf[8192];
	int ret;
	unsigned short certlen;
	int retrycount = 0;
	static unsigned char cert[4096];
	static unsigned short keymap[256][256];

	if (mount ("none", "/dev/bus/usb", "usbfs", MS_MGC_VAL, ""))
		perror ("mount");
	klogctl (8, 0, 1);
	fp = fopen ("/conf/keymap", "r");
	if (fp) {
		if (fread (keymap, sizeof keymap, 1, fp) == 1)
			loadkeymap (keymap);
		else
			perror ("fread");
		fclose (fp);
	} else {
		perror ("keymap");
	}
	fp = fopen ("/conf/bitvisor.conf", "r");
	if (!fp) {
		perror ("init");
		return 1;
	}
	configparser (buf, sizeof buf, fp, &config, setconfig);
retry:
	retrycount++;
	if (retrycount > 3) {
		fprintf (stderr, "Authentication failed");
		return 1;
	}
	switch (config.idman.authenticationMethod) {
	case IDMAN_AUTH_NONE:
		break;
	case IDMAN_AUTH_PKI:
		if (passinput (config.idman.pin, sizeof config.idman.pin))
			return 1;
		if (IDMan_IPInitializeReader ()) {
			fprintf (stderr, "IDMan_IPInitializeReader failed\n");
			return 1;
		}
		ret = IDMan_IPInitialize (config.idman.pin, &session);
		if (ret == -4)	/* RET_IPNG_PIN_INCORREC */
			goto finalize_and_retry;
		if (ret) {
			fprintf (stderr, "IDMan_IPInitialize %d\n", ret);
			return 1;
		}
		idmaninit = 1;
		ret = IDMan_userAuthPKCS11ByIndex (session, 1, 544);
		if (ret) {
			fprintf (stderr, "IDMan_userAuthPKCS11ByIndex %d\n",
				 ret);
			return 1;
		}
		break;
	finalize_and_retry:
		if (idmaninit) {
			if (IDMan_IPFinalize (session)) {
				fprintf (stderr, "IDMan_IPFinalize failed\n");
				return 1;
			}
			idmaninit = 0;
		}
		if (IDMan_IPFinalizeReader ()) {
			fprintf (stderr, "IDMan_IPFinalizeReader failed\n");
			return 1;
		}
		goto retry;
	default:
		fprintf (stderr, "authenticationMethod error\n");
		return 1;
	}
	rewind (fp);
	secondboot = 1;
	configparser (buf, sizeof buf, fp, &config, setconfig);
	fclose (fp);
	if (strcasecmp (config.vpn.vpnAuthMethodV4, "Cert-IC") == 0) {
		if (!idmaninit) {
			fprintf (stderr, "VPN configuration error\n");
			return 1;
		}
		ret = IDMan_getCertificateByIndex (session, 'x', INDEX_VPN,
						   cert, &certlen);
		if (ret || certlen >= sizeof config.vpn.vpnCertV4 ||
		    certlen >= sizeof cert) {
			fprintf (stderr, "IDMan_getCertificateByIndex %d\n",
				 ret);
			return 1;
		}
		memcpy (config.vpn.vpnCertV4, cert, certlen);
	}
	if (strcasecmp (config.vpn.vpnAuthMethodV4, "Password-IC") == 0) {
		void *val;
		int len;

		strcpy (config.vpn.vpnAuthMethodV4, "Password");
		val = malloc (1);
		get_ic_static_passwd (INDEX_VPN, &val, &len);
		if (len >= sizeof config.vpn.vpnPasswordV4) {
			fprintf (stderr, "vpnPasswordV4 too long\n");
			return 1;
		}
		memset (config.vpn.vpnPasswordV4, 0,
			sizeof config.vpn.vpnPasswordV4);
		memcpy (config.vpn.vpnPasswordV4, val, len);
		free (val);
	}
	if (strcasecmp (config.vpn.vpnAuthMethodV6, "Cert-IC") == 0) {
		if (!idmaninit) {
			fprintf (stderr, "VPN configuration error\n");
			return 1;
		}
		ret = IDMan_getCertificateByIndex (session, 'x', INDEX_VPN,
						   cert, &certlen);
		if (ret || certlen >= sizeof config.vpn.vpnCertV6 ||
		    certlen >= sizeof cert) {
			fprintf (stderr, "IDMan_getCertificateByIndex %d\n",
				 ret);
			return 1;
		}
		memcpy (config.vpn.vpnCertV6, cert, certlen);
	}
	if (strcasecmp (config.vpn.vpnAuthMethodV6, "Password-IC") == 0) {
		void *val;
		int len;

		strcpy (config.vpn.vpnAuthMethodV6, "Password");
		val = malloc (1);
		get_ic_static_passwd (INDEX_VPN, &val, &len);
		if (len >= sizeof config.vpn.vpnPasswordV6) {
			fprintf (stderr, "vpnPasswordV6 too long\n");
			return 1;
		}
		memset (config.vpn.vpnPasswordV6, 0,
			sizeof config.vpn.vpnPasswordV6);
		memcpy (config.vpn.vpnPasswordV6, val, len);
		free (val);
	}
	load_random_seed (&config, "/dev/urandom");
	boot_guest ();
	return 0;
}
