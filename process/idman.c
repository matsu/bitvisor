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

#include <lib_mm.h>
#include <lib_printf.h>
#include <lib_string.h>
#include <lib_syscalls.h>

int heap[65536], heaplen = 65536;
static int usleep_desc;

void
usleep (unsigned int usec)
{
	msgsendint (usleep_desc, usec);
}

int
usb_bulk_read (void *dev, int ep, char *bytes, int size, int timeout)
{
	int msg_usb_bulk_read (void *dev, int ep, char *bytes, int size,
			       int timeout);

	return msg_usb_bulk_read (dev, ep, bytes, size, timeout);
}

int
usb_bulk_write (void *dev, int ep, char *bytes, int size, int timeout)
{
	int msg_usb_bulk_write (void *dev, int ep, char *bytes, int size,
				int timeout);

	return msg_usb_bulk_write (dev, ep, bytes, size, timeout);
}

int
usb_claim_interface (void *dev, int interface)
{
	int msg_usb_claim_interface (void *dev, int interface);

	return msg_usb_claim_interface (dev, interface);
}

int
usb_close (void *dev)
{
	int msg_usb_close (void *dev);

	return msg_usb_close (dev);
}

int
usb_control_msg (void *dev, int requesttype, int request, int value,
		 int index, char *bytes, int size, int timeout)
{
	int msg_usb_control_msg (void *dev, int requesttype, int request,
				 int value, int index, char *bytes, int size,
				 int timeout);

	return msg_usb_control_msg (dev, requesttype, request, value, index,
				    bytes, size, timeout);
}

int
usb_find_busses (void)
{
	int msg_usb_find_busses (void);

	return msg_usb_find_busses ();
}

int
usb_find_devices (void)
{
	int msg_usb_find_devices (void);

	return msg_usb_find_devices ();
}

void *
usb_get_busses (void)
{
	void *msg_usb_get_busses (void);

	return msg_usb_get_busses ();
}

int
usb_get_string_simple (void *dev, int index, char *buf, unsigned long buflen)
{
	int msg_usb_get_string_simple (void *dev, int index, char *buf,
				       unsigned long buflen);

	return msg_usb_get_string_simple (dev, index, buf, buflen);
}

void
usb_init (void)
{
	void msg_usb_init (void);

	msg_usb_init ();
}

void *
usb_open (void *dev)
{
	void *msg_usb_open (void *dev);

	return msg_usb_open (dev);
}

int
usb_release_interface (void *dev, int interface)
{
	int msg_usb_release_interface (void *dev, int interface);

	return msg_usb_release_interface (dev, interface);
}

int
usb_reset (void *dev)
{
	int msg_usb_reset (void *dev);

	return msg_usb_reset (dev);
}

int
_start (int m, int c, struct msgbuf *buf, int bufcnt)
{
	int idman_user_init (void *, char *, int);
	void *tmp;

	if (m != MSG_BUF)
		exitprocess (1);
	printf ("idman (user) init\n");
	usleep_desc = msgopen ("usleep");
	if (usleep_desc < 0) {
		printf ("open usleep failed\n");
		exitprocess (1);
	}
	tmp = alloc (buf[0].len);
	memcpy (tmp, buf[0].base, buf[0].len);
	if (idman_user_init (tmp, buf[1].base, buf[1].len)) {
		printf ("idman init failed\n");
		exitprocess (1);
	}
	if (setlimit (16384, 8 * 16384)) {
		printf ("idman restrict failed\n");
		exitprocess (1);
	}
	printf ("ready\n");
	return 0;
}
