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

#include <core.h>
#include <core/process.h>
#include <IDMan.h>
#include <usb.h>
#include <usb_device.h>
#include "idman_msg.h"

#define NUM_OF_DEVICES 128
#define NUM_OF_BUSSES 16

static struct config_data_idman *cfg;
static int usb_desc;
static int idman_desc;

static char usb_bus[(sizeof (struct usb_bus) +
		     (sizeof (struct usb_device) +
		      sizeof (struct usb_config_descriptor) +
		      sizeof (struct usb_interface) +
		      sizeof (struct usb_interface_descriptor) +
		      sizeof (struct usb_endpoint_descriptor) +
		      16 /* extra */) * NUM_OF_DEVICES) * NUM_OF_BUSSES];

static int
idman_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (c == IDMAN_MSG_ENCRYPTBYINDEX) {
		struct idman_msg_encryptbyindex *arg;

		if (bufcnt != 2)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = IDMan_EncryptByIndex (arg->SessionHandle,
						    arg->PkcxIndex,
						    arg->datad, arg->dataLen,
						    buf[1].base,
						    &arg->signatureLend,
						    arg->algorithm);
		return 0;
	} else if (c == IDMAN_MSG_IPINITIALIZEREADER) {
		struct idman_msg_ipinitializereader *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = IDMan_IPInitializeReader ();
		return 0;
	} else if (c == IDMAN_MSG_IPINITIALIZE) {
		struct idman_msg_ipinitialize *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = IDMan_IPInitialize (arg->PINd,
						  &arg->SessionHandled);
		return 0;
	} else if (c == IDMAN_MSG_IPFINALIZE) {
		struct idman_msg_ipfinalize *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = IDMan_IPFinalize (arg->SessionHandle);
		return 0;
	} else if (c == IDMAN_MSG_IPFINALIZEREADER) {
		struct idman_msg_ipfinalizereader *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = IDMan_IPFinalizeReader ();
		return 0;
	} else if (c == IDMAN_MSG_CHECKCARDSTATUS) {
		struct idman_msg_checkcardstatus *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = IDMan_CheckCardStatus (arg->SessionHandle);
		return 0;
	} else {
		return -1;
	}
}

static void
callsub (int c, struct msgbuf *buf, int bufcnt)
{
	int r;

	for (;;) {
		r = msgsendbuf (usb_desc, c, buf, bufcnt);
		if (r == 0)
			break;
		panic ("idman_user msgsendbuf failed (%d)", c);
	}
}

int
msg_usb_bulk_read (usb_dev_handle *dev, int ep, char *bytes, int size,
		   int timeout)
{
	struct usb_msg_bulk_read arg;
	struct msgbuf buf[2];

	arg.handle = *(int *)dev;
	arg.ep = ep;
	arg.timeout = timeout;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	setmsgbuf (&buf[1], bytes, size, 1);
	callsub (USB_MSG_BULK_READ, buf, 2);
	return arg.retval;
}

int
msg_usb_bulk_write (usb_dev_handle *dev, int ep, char *bytes, int size,
		    int timeout)
{
	struct usb_msg_bulk_write arg;
	struct msgbuf buf[2];

	arg.handle = *(int *)dev;
	arg.ep = ep;
	arg.timeout = timeout;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	setmsgbuf (&buf[1], bytes, size, 1);
	callsub (USB_MSG_BULK_WRITE, buf, 2);
	return arg.retval;
}

int
msg_usb_claim_interface (usb_dev_handle *dev, int interface)
{
	struct usb_msg_claim_interface arg;
	struct msgbuf buf[1];

	arg.handle = *(int *)dev;
	arg.interface = interface;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (USB_MSG_CLAIM_INTERFACE, buf, 1);
	return arg.retval;
}

int
msg_usb_close (usb_dev_handle *dev)
{
	struct usb_msg_close arg;
	struct msgbuf buf[1];

	arg.handle = *(int *)dev;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (USB_MSG_CLOSE, buf, 1);
	if (!arg.retval)
		free (dev);
	return arg.retval;
}

int
msg_usb_control_msg (usb_dev_handle *dev, int requesttype, int request,
		     int value, int index, char *bytes, int size, int timeout)
{
	struct usb_msg_control_msg arg;
	struct msgbuf buf[2];

	arg.handle = *(int *)dev;
	arg.requesttype = requesttype;
	arg.request = request;
	arg.value = value;
	arg.index = index;
	arg.timeout = timeout;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	if (size)
		setmsgbuf (&buf[1], bytes, size, 1);
	callsub (USB_MSG_CONTROL_MSG, buf, size ? 2 : 1);
	return arg.retval;
}

int 
msg_usb_find_busses (void)
{
	struct usb_msg_find_busses arg;
	struct msgbuf buf[1];

	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (USB_MSG_FIND_BUSSES, buf, 1);
	return arg.retval;
}

int
msg_usb_find_devices (void)
{
	struct usb_msg_find_devices arg;
	struct msgbuf buf[1];

	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (USB_MSG_FIND_DEVICES, buf, 1);
	return arg.retval;
}

struct usb_bus *
msg_usb_get_busses (void)
{
	struct usb_msg_get_busses arg;
	struct msgbuf buf[2];

	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	setmsgbuf (&buf[1], usb_bus, sizeof usb_bus, 1);
	callsub (USB_MSG_GET_BUSSES, buf, 2);
	return arg.err ? NULL : (struct usb_bus *)buf[1].base;
}

int
msg_usb_get_string_simple (usb_dev_handle *dev, int index, char *buf,
			   size_t buflen)
{
	struct usb_msg_get_string_simple arg;
	struct msgbuf buff[2];

	arg.handle = *(int *)dev;
	arg.index = index;
	setmsgbuf (&buff[0], &arg, sizeof arg, 1);
	setmsgbuf (&buff[1], buf, buflen, 1);
	callsub (USB_MSG_GET_STRING_SIMPLE, buff, 2);
	return arg.retval;
}

void
msg_usb_init (void)
{
	msgsendint (usb_desc, USB_MSG_INIT);
}

usb_dev_handle *
msg_usb_open (struct usb_device *dev)
{
	struct usb_msg_open arg;
	struct msgbuf buf[1];
	int *p;

	p = alloc (sizeof *p);
	if (!p)
		return NULL;
	arg.dev = dev;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (USB_MSG_OPEN, buf, 1);
	if (arg.retval < 0) {
		free (p);
		return NULL;
	}
	*p = arg.retval;
	return (void *)p;
}

int
msg_usb_release_interface (usb_dev_handle *dev, int interface)
{
	struct usb_msg_release_interface arg;
	struct msgbuf buf[1];

	arg.handle = *(int *)dev;
	arg.interface = interface;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (USB_MSG_RELEASE_INTERFACE, buf, 1);
	return arg.retval;
}

int
msg_usb_reset (usb_dev_handle *dev)
{
	struct usb_msg_reset arg;
	struct msgbuf buf[1];

	arg.handle = *(int *)dev;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (USB_MSG_RESET, buf, 1);
	return arg.retval;
}

int
IDMan_StReadSetData (char *pMemberName, char *pInfo, unsigned long int *len)
{
	int i;

	if (!strcmp (pMemberName, "CRL01")) {
		memcpy (pInfo, cfg->crl01, 4096);
		for (i = 0; i < 4096; i++) {
			if (cfg->crl01[i]) {
				*len = 4096;
				return 0;
			}
		}
		/* return success */
		*len = 0;
		return 0;
	}
	if (!strcmp (pMemberName, "CRL02")) {
		memcpy (pInfo, cfg->crl02, 4096);
		for (i = 0; i < 4096; i++) {
			if (cfg->crl02[i]) {
				*len = 4096;
				return 0;
			}
		}
		/* return success */
		*len = 0;
		return 0;
	}
	if (!strcmp (pMemberName, "CRL03")) {
		memcpy (pInfo, cfg->crl03, 4096);
		for (i = 0; i < 4096; i++) {
			if (cfg->crl03[i]) {
				*len = 4096;
				return 0;
			}
		}
		/* return success */
		*len = 0;
		return 0;
	}
	if (!strcmp (pMemberName, "TrustAnchorCert01")) {
		for (i = 0; i < 4096; i++) {
			if (cfg->pkc01[i]) {
				memcpy (pInfo, cfg->pkc01, 4096);
				*len = 1105;//4096;
				return 0;
			}
		}
		return -1;
	}
	if (!strcmp (pMemberName, "TrustAnchorCert02")) {
		for (i = 0; i < 4096; i++) {
			if (cfg->pkc02[i]) {
				memcpy (pInfo, cfg->pkc02, 4096);
				*len = 4096;
				return 0;
			}
		}
		return -1;
	}
	if (!strcmp (pMemberName, "TrustAnchorCert03")) {
		for (i = 0; i < 4096; i++) {
			if (cfg->pkc03[i]) {
				memcpy (pInfo, cfg->pkc03, 4096);
				*len = 4096;
				return 0;
			}
		}
		return -1;
	}
	if (!strcmp (pMemberName, "RandomSeedSize")) {
		*len = snprintf (pInfo, 16, "%u", cfg->randomSeedSize);
		return 0;
	}
	if (!strcmp (pMemberName, "MaxPinLen")) {
		*len = snprintf (pInfo, 16, "%u", cfg->maxPinLen);
		return 0;
	}
	if (!strcmp (pMemberName, "MinPinLen")) {
		*len = snprintf (pInfo, 16, "%u", cfg->minPinLen);
		return 0;
	}
	return -1;
}

int
idman_user_init (struct config_data_idman *idman, char *seed, int len)
{
	void InitCryptoLibrary (unsigned char *, int);

	cfg = idman;
	idman_desc = msgregister ("idman", idman_msghandler);
	if (idman_desc < 0)
		return -1;
	InitCryptoLibrary ((unsigned char *)seed, len);
#ifdef IDMAN_PD
	usb_desc = msgopen ("usb");
	if (usb_desc < 0)
		return -1;
	if (msgsendint (usb_desc, USB_MSG_CLOSEALL) < 0)
		return -1;
#endif /* IDMAN_PD */
	return 0;
}
