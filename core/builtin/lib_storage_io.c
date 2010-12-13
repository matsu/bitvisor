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

#include <lib_assert.h>
#include <lib_mm.h>
#include <lib_printf.h>
#include <lib_storage_io.h>
#include <lib_string.h>
#include <lib_syscalls.h>

static int rdesc, registered = 0;
static int desc, opened = 0;

static void
callsub (int c, struct msgbuf *buf, int bufcnt)
{
	int r;

	if (!opened) {
		desc = msgopen ("storage_io");
		if (desc < 0) {
			printf ("could not open storage_io\n");
			exitprocess (1);
		}
		opened = 1;
	}
	for (;;) {
		r = msgsendbuf (desc, c, buf, bufcnt);
		if (r == 0)
			break;
		panic ("lib_storage_io msgsendbuf failed (%d)", c);
	}
}

int
storage_io_init (void)
{
	struct storage_io_msg_init arg;
	struct msgbuf buf[1];

	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (STORAGE_IO_INIT, buf, 1);
	return arg.retval;
}

void
storage_io_deinit (int id)
{
	struct storage_io_msg_deinit arg;
	struct msgbuf buf[1];

	arg.id = id;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (STORAGE_IO_DEINIT, buf, 1);
}

int
storage_io_get_num_devices (int id)
{
	struct storage_io_msg_get_num_devices arg;
	struct msgbuf buf[1];

	arg.id = id;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (STORAGE_IO_GET_NUM_DEVICES, buf, 1);
	return arg.retval;
}

static int
lib_storage_io_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (c == STORAGE_IO_RGET_SIZE) {
		struct storage_io_msg_rget_size *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->callback (arg->data, arg->size);
		return 0;
	} else if (c == STORAGE_IO_RREADWRITE) {
		struct storage_io_msg_rreadwrite *arg;

		if (bufcnt != 1 && bufcnt != 2)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		if (bufcnt == 2)
			memcpy (arg->buf, buf[1].base, buf[1].len);
		arg->callback (arg->data, arg->len);
		return 0;
	} else {
		return -1;
	}
}

int
storage_io_aget_size (int id, int devno,
		      void (*callback) (void *data, long long size),
		      void *data)
{
	struct storage_io_msg_aget_size arg;
	struct msgbuf buf[1];

	arg.id = id;
	arg.devno = devno;
	arg.callback = callback;
	arg.data = data;
	if (!registered) {
		rdesc = msgregister ("lib_storage_io",
				    lib_storage_io_msghandler);
		if (rdesc < 0)
			return -1;
		registered = 1;
	}
	memcpy (arg.msgname, "lib_storage_io", 15);
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (STORAGE_IO_AGET_SIZE, buf, 1);
	return arg.retval;
}

int
storage_io_aread (int id, int devno, void *buf, int len, long long offset,
		  void (*callback) (void *data, int len), void *data)
{
	struct storage_io_msg_areadwrite arg;
	struct msgbuf mbuf[2];

	arg.id = id;
	arg.devno = devno;
	arg.offset = offset;
	arg.write = 0;
	arg.callback = callback;
	arg.data = data;
	arg.buf = buf;
	arg.len = len;
	if (!registered) {
		rdesc = msgregister ("lib_storage_io",
				    lib_storage_io_msghandler);
		if (rdesc < 0)
			return -1;
		registered = 1;
	}
	memcpy (arg.msgname, "lib_storage_io", 15);
	setmsgbuf (&mbuf[0], &arg, sizeof arg, 1);
	setmsgbuf (&mbuf[1], buf, len, 1);
	callsub (STORAGE_IO_AREADWRITE, mbuf, 2);
	return arg.retval;
}

int
storage_io_awrite (int id, int devno, void *buf, int len, long long offset,
		   void (*callback) (void *data, int len), void *data)
{
	struct storage_io_msg_areadwrite arg;
	struct msgbuf mbuf[2];

	arg.id = id;
	arg.devno = devno;
	arg.offset = offset;
	arg.write = 1;
	arg.callback = callback;
	arg.data = data;
	arg.buf = buf;
	arg.len = len;
	if (!registered) {
		rdesc = msgregister ("lib_storage_io",
				    lib_storage_io_msghandler);
		if (rdesc < 0)
			return -1;
		registered = 1;
	}
	memcpy (arg.msgname, "lib_storage_io", 15);
	setmsgbuf (&mbuf[0], &arg, sizeof arg, 1);
	setmsgbuf (&mbuf[1], buf, len, 1);
	callsub (STORAGE_IO_AREADWRITE, mbuf, 2);
	return arg.retval;
}
