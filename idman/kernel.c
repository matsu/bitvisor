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

#define NUM_OF_HANDLE 32

#ifdef IDMAN_PD
struct devlist {
	struct devlist *next;
	struct usb_device *kernel, *user;
};

static int desc, usb_desc;
static struct mempool *mp;
static usb_dev_handle *handle[NUM_OF_HANDLE];
static struct devlist *devlist_head;
static spinlock_t usb_lock;
static char usbbuf[65536];

static void
callsub (int c, struct msgbuf *buf, int bufcnt)
{
	int r;

	for (;;) {
		r = msgsendbuf (desc, c, buf, bufcnt);
		if (r == 0)
			break;
		printf ("ret %d\n", r);
		for (r = 0; r < bufcnt; r++)
			printf ("buf[%d] %p, %d, %d\n", r, buf[r].base,
				buf[r].len, buf[r].rw);
		panic ("idman msgsendbuf failed (%d)", c);
	}
}

int
IDMan_EncryptByIndex (unsigned long int SessionHandle, int PkcxIndex,
		      unsigned char *data, unsigned short int dataLen,
		      unsigned char *signature,
		      unsigned short int *signatureLen, int algorithm)
{
	struct idman_msg_encryptbyindex *arg;
	struct msgbuf buf[2];
	int ret;

	if (dataLen > sizeof arg->datad)
		return -1;
	arg = mempool_allocmem (mp, sizeof *arg);
	arg->SessionHandle = SessionHandle;
	arg->PkcxIndex = PkcxIndex;
	memcpy (arg->datad, data, dataLen);
	arg->dataLen = dataLen;
	arg->signatureLend = *signatureLen;
	arg->algorithm = algorithm;
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	setmsgbuf (&buf[1], mempool_allocmem (mp, arg->signatureLend),
		   arg->signatureLend, 1);
	callsub (IDMAN_MSG_ENCRYPTBYINDEX, buf, 2);
	ret = arg->retval;
	*signatureLen = arg->signatureLend;
	memcpy (signature, buf[1].base, arg->signatureLend);
	mempool_freemem (mp, buf[1].base);
	mempool_freemem (mp, arg);
	return ret;
}

int
IDMan_IPInitializeReader (void)
{
	struct idman_msg_ipinitializereader *arg;
	struct msgbuf buf[1];
	int ret;

	arg = mempool_allocmem (mp, sizeof *arg);
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (IDMAN_MSG_IPINITIALIZEREADER, buf, 1);
	ret = arg->retval;
	mempool_freemem (mp, arg);
	return ret;
}

int
IDMan_IPInitialize (char *PIN, unsigned long int *SessionHandle)
{
	struct idman_msg_ipinitialize *arg;
	struct msgbuf buf[1];
	int ret;

	if (strlen (PIN) + 1 > sizeof arg->PINd)
		return -1;
	arg = mempool_allocmem (mp, sizeof *arg);
	memcpy (arg->PINd, PIN, strlen (PIN) + 1);
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (IDMAN_MSG_IPINITIALIZE, buf, 1);
	ret = arg->retval;
	*SessionHandle = arg->SessionHandled;
	mempool_freemem (mp, arg);
	return ret;
}

int
IDMan_IPFinalize (unsigned long int SessionHandle)
{
	struct idman_msg_ipfinalize *arg;
	struct msgbuf buf[1];
	int ret;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->SessionHandle = SessionHandle;
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (IDMAN_MSG_IPFINALIZE, buf, 1);
	ret = arg->retval;
	mempool_freemem (mp, arg);
	return ret;
}

int
IDMan_IPFinalizeReader (void)
{
	struct idman_msg_ipfinalizereader *arg;
	struct msgbuf buf[1];
	int ret;

	arg = mempool_allocmem (mp, sizeof *arg);
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (IDMAN_MSG_IPFINALIZEREADER, buf, 1);
	ret = arg->retval;
	mempool_freemem (mp, arg);
	return ret;
}

int
IDMan_CheckCardStatus (unsigned long int SessionHandle)
{
	struct idman_msg_checkcardstatus *arg;
	struct msgbuf buf[1];
	int ret;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->SessionHandle = SessionHandle;
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (IDMAN_MSG_CHECKCARDSTATUS, buf, 1);
	ret = arg->retval;
	mempool_freemem (mp, arg);
	return ret;
}

int
idman_user_init (struct config_data_idman *idman, char *seed, int len)
{
	int d, d1;
	struct config_data_idman *p;
	char *q;
	struct msgbuf buf[2];

	mp = mempool_new (0, 1, true);
	d = newprocess ("idman");
	if (d < 0)
		panic ("newprocess idman");
	d1 = msgopen ("ttyout");
	if (d1 < 0)
		panic ("msgopen ttyout");
	msgsenddesc (d, d1);
	msgsenddesc (d, d1);
	msgclose (d1);
	p = mempool_allocmem (mp, sizeof *p);
	memcpy (p, idman, sizeof *p);
	q = mempool_allocmem (mp, len);
	memcpy (q, seed, len);
	setmsgbuf (&buf[0], p, sizeof *p, 0);
	setmsgbuf (&buf[1], q, sizeof *q, 0);
	if (msgsendbuf (d, 0, buf, 2))
		panic ("idman init failed");
	mempool_freemem (mp, q);
	mempool_freemem (mp, p);
	msgclose (d);
	return 0;
}

static int
copy_usb_bus (struct usb_bus *buf, int len, struct usb_bus *bus)
{
	struct usb_bus *pb, *qb;
	void *qbhead, **qqb;
	struct usb_device *pd, *qd, **qqd;
	void *next;
	struct devlist *dl;
	int i, j, k;

	while (devlist_head) {
		dl = devlist_head->next;
		free (devlist_head);
		devlist_head = dl;
	}
	if (!bus)
		return -1;
	next = buf;
	qqb = &qbhead;
	for (pb = bus; pb; pb = LIST_NEXT (usb_busses, pb)) {
		qb = next;
		if (len < sizeof *qb)
			return -1;
		len -= sizeof *qb;
		next = qb + 1;
		*qqb = qb;
		qqb = &LIST_NEXT (usb_busses, qb);
		*qqb = NULL;
		qb->host = NULL;
		qb->device = NULL;
		qb->busnum = pb->busnum;
		qqd = &qb->device;
		for (pd = pb->device; pd; pd = pd->next) {
			qd = next;
			if (len < sizeof *qd)
				return -1;
			dl = alloc (sizeof *dl);
			dl->kernel = pd;
			dl->user = qd;
			dl->next = devlist_head;
			devlist_head = dl;
			len -= sizeof *qd;
			next = qd + 1;
			*qqd = qd;
			qqd = &qd->next;
			*qqd = NULL;
			qd->prev = NULL;
			qd->bus = NULL;
			memcpy (&qd->descriptor, &pd->descriptor,
				sizeof qd->descriptor);
#define DATA_COPY(NAME, LABEL, NUM) \
	if (!pd->NAME) \
		goto LABEL; \
	if (!NUM) \
		goto LABEL; \
	qd->NAME = next; \
	if (len < NUM * sizeof *qd->NAME) \
		return -1; \
	len -= NUM * sizeof *qd->NAME; \
	next = qd->NAME + NUM; \
	memcpy (qd->NAME, pd->NAME, NUM * sizeof *qd->NAME)
#define STRUCT_COPY(NAME, NUM, LABEL) \
	DATA_COPY(NAME, LABEL, NUM)
#define EXTRA_COPY(NAME, LABEL) DATA_COPY(NAME, LABEL, pd->NAME##len)
			qd->config = NULL;
			STRUCT_COPY (config, 1, skip1);
			i = pd->config->bNumInterfaces;
			STRUCT_COPY (config->interface, i, skip2);
			while (i-- > 0) {
				j = pd->config->interface[i].num_altsetting;
				STRUCT_COPY (config->interface[i].altsetting,
					     j, skip3);
				while (j-- > 0) {
					k = pd->config->interface[i].
						altsetting[j].bNumEndpoints;
					STRUCT_COPY (config->interface[i].
						     altsetting[j].endpoint, k,
						     skip4);
					while (k-- > 0) {
						EXTRA_COPY (config->
							    interface[i].
							    altsetting[j].
							    endpoint[k].
							    extra, skip5);
					skip5:
						;
					}
				skip4:
					if (k > 0) {
						printf ("libusb warning:"
							" bNumEndpoints %d"
							" endpoint %p\n",
							pd->config->
							interface[i].
							altsetting[j].
							bNumEndpoints,
							pd->config->
							interface[i].
							altsetting[j].
							endpoint);
						pd->config->interface[i].
							altsetting[j].
							bNumEndpoints = 0;
					}
					EXTRA_COPY (config->interface[i].
						    altsetting[j].extra,
						    skip6);
				skip6:
					;
				}
			skip3:
				if (j > 0) {
					printf ("libusb warning:"
						" num_altsetting %d"
						" altsetting %p\n",
						pd->config->interface[i].
						num_altsetting,
						pd->config->interface[i].
						altsetting);
					pd->config->interface[i].
						num_altsetting = 0;
				}
			}
		skip2:
			if (i > 0) {
				printf ("libusb warning: bNumInterfaces %d"
					" interface %p\n",
					pd->config->bNumInterfaces,
					pd->config->interface);
				pd->config->bNumInterfaces = 0;
			}
			EXTRA_COPY (config->extra, skip1);
		skip1:
			qd->dev = NULL;
			qd->lock_dev = 0;
			qd->portno = pd->portno;
			qd->devnum = pd->devnum;
			qd->host = NULL;
			qd->bStatus = pd->bStatus;
			qd->handle = NULL;
			qd->hook = NULL;
			qd->hooknum = pd->hooknum;
		}
	}
	return 0;
}

static int
usb_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	int i, r;

	spinlock_lock (&usb_lock);
	if (m == MSG_INT) {
		if (c == USB_MSG_INIT) {
			usb_init ();
			r = 0;
			goto ret;
		} else if (c == USB_MSG_CLOSEALL) {
			for (i = 0; i < NUM_OF_HANDLE; i++) {
				if (handle[i]) {
					usb_close (handle[i]);
					handle[i] = NULL;
				}
			}
			r = 0;
			goto ret;
		} else {
			r = -1;
			goto ret;
		}
	} else if (m == MSG_BUF) {
		if (c == USB_MSG_BULK_READ) {
			struct usb_msg_bulk_read *arg;
			int h;

			if (bufcnt != 2) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			if (buf[1].len > sizeof usbbuf) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg->retval = usb_bulk_read (handle[h],
						     arg->ep, usbbuf,
						     buf[1].len, arg->timeout);
			memcpy (buf[1].base, usbbuf, buf[1].len);
			r = 0;
			goto ret;
		} else if (c == USB_MSG_BULK_WRITE) {
			struct usb_msg_bulk_write *arg;
			int h;

			if (bufcnt != 2) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			if (buf[1].len > sizeof usbbuf) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			memcpy (usbbuf, buf[1].base, buf[1].len);
			arg->retval = usb_bulk_write (handle[h],
						      arg->ep, usbbuf,
						      buf[1].len,
						      arg->timeout);
			r = 0;
			goto ret;
		} else if (c == USB_MSG_CLAIM_INTERFACE) {
			struct usb_msg_claim_interface *arg;
			int h;

			if (bufcnt != 1) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg->retval = usb_claim_interface (handle[h],
							   arg->interface);
			r = 0;
			goto ret;
		} else if (c == USB_MSG_CLOSE) {
			struct usb_msg_close *arg;
			int h;

			if (bufcnt != 1) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg->retval = usb_close (handle[h]);
			if (!arg->retval)
				handle[h] = NULL;
			r = 0;
			goto ret;
		} else if (c == USB_MSG_CONTROL_MSG) {
			struct usb_msg_control_msg *arg;
			int h;

			if (bufcnt != 1 && bufcnt != 2) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			if (bufcnt == 2 && buf[1].len > sizeof usbbuf) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg->retval = usb_control_msg (handle[h],
						       arg->requesttype,
						       arg->request,
						       arg->value, arg->index,
						       usbbuf, bufcnt == 2 ?
						       buf[1].len : 0,
						       arg->timeout);
			if (bufcnt == 2)
				memcpy (buf[1].base, usbbuf, buf[1].len);
			r = 0;
			goto ret;
		} else if (c == USB_MSG_FIND_BUSSES) {
			struct usb_msg_find_busses *arg;

			if (bufcnt != 1) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			arg->retval = usb_find_busses ();
			r = 0;
			goto ret;
		} else if (c == USB_MSG_FIND_DEVICES) {
			struct usb_msg_find_devices *arg;

			if (bufcnt != 1) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			arg->retval = usb_find_devices ();
			r = 0;
			goto ret;
		} else if (c == USB_MSG_GET_BUSSES) {
			struct usb_msg_get_busses *arg;
			struct usb_bus *bus;

			if (bufcnt != 2) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			bus = usb_get_busses ();
			arg->err = copy_usb_bus (buf[1].base, buf[1].len, bus);
			r = 0;
			goto ret;
		} else if (c == USB_MSG_GET_STRING_SIMPLE) {
			struct usb_msg_get_string_simple *arg;
			int h;

			if (bufcnt != 2) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			if (buf[1].len > sizeof usbbuf) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg->retval = usb_get_string_simple (handle[h],
							     arg->index,
							     usbbuf,
							     buf[1].len);
			memcpy (buf[1].base, usbbuf, buf[1].len);
			r = 0;
			goto ret;
		} else if (c == USB_MSG_OPEN) {
			struct usb_msg_open *arg;
			struct devlist *dl;

			if (bufcnt != 1) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			for (i = 0; i < NUM_OF_HANDLE; i++)
				if (!handle[i])
					break;
			if (i >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			for (dl = devlist_head; dl; dl = dl->next)
				if (dl->user == arg->dev)
					break;
			if (dl) {
				handle[i] = usb_open (dl->kernel);
			} else {
				printf ("devlist not found %p\n", arg->dev);
				handle[i] = NULL;
			}
			if (handle[i])
				arg->retval = i;
			else
				arg->retval = -1;
			r = 0;
			goto ret;
		} else if (c == USB_MSG_RELEASE_INTERFACE) {
			struct usb_msg_release_interface *arg;
			int h;

			if (bufcnt != 1) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg->retval = usb_release_interface (handle[h],
							     arg->interface);
			r = 0;
			goto ret;
		} else if (c == USB_MSG_RESET) {
			struct usb_msg_reset *arg;
			int h;

			if (bufcnt != 1) {
				r = -1;
				goto ret;
			}
			if (buf[0].len != sizeof *arg) {
				r = -1;
				goto ret;
			}
			arg = buf[0].base;
			h = arg->handle;
			if (h < 0 || h >= NUM_OF_HANDLE) {
				r = -1;
				goto ret;
			}
			arg->retval = usb_reset (handle[h]);
			r = 0;
			goto ret;
		} else {
			r = -1;
			goto ret;
		}
	} else {
		r = -1;
		goto ret;
	}
ret:
	spinlock_unlock (&usb_lock);
	return r;
}
#endif /* IDMAN_PD */

static void
idman_kernel_init (void)
{
#ifdef IDMAN_PD
	int i;

	for (i = 0; i < NUM_OF_HANDLE; i++)
		handle[i] = NULL;
	devlist_head = NULL;
	spinlock_init (&usb_lock);
	usb_desc = msgregister ("usb", usb_msghandler);
	if (usb_desc < 0)
		panic ("register usb");
#endif /* IDMAN_PD */
	if (idman_user_init (&config.idman, config.vmm.randomSeed,
			     sizeof config.vmm.randomSeed) < 0)
		panic ("idman_user_init");
#ifdef IDMAN_PD
	desc = msgopen ("idman");
	if (desc < 0)
		panic ("open idman");
#endif /* IDMAN_PD */
}

INITFUNC ("driver1", idman_kernel_init);
