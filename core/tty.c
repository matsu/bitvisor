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

#include "debug.h"
#include "initfunc.h"
#include "printf.h"
#include "process.h"
#include "putchar.h"
#include "serial.h"
#include "spinlock.h"
#include "tty.h"
#include "vramwrite.h"

static int ttyin, ttyout;
static unsigned char log[65536];
static int logoffset, loglen;
static spinlock_t putchar_lock;
static bool logflag;

static int
ttyin_msghandler (int m, int c)
{
	if (m == 0)
		return msgsendint (ttyin, c);
	return 0;
}

static int
ttyout_msghandler (int m, int c)
{
	if (m == 0)
		tty_putchar ((unsigned char)c);
	return 0;
}

static int
ttylog_msghandler (int m, int c, void *recvbuf, int recvlen, void *sendbuf,
		   int sendlen)
{
	int i;
	unsigned char *q;

	if (m == 1) {
		q = sendbuf;
		for (i = 0; i < sendlen && i < loglen; i++)
			q[i] = log[(logoffset + i) % sizeof log];
	}
	return loglen;
}

void
ttylog_stop (void)
{
	logflag = false;
}

void
tty_putchar (unsigned char c)
{
	if (logflag) {
		spinlock_lock (&putchar_lock);
		log[(logoffset + loglen) % sizeof log] = c;
		if (loglen == sizeof log)
			logoffset = (logoffset + 1) % sizeof log;
		else
			loglen++;
		spinlock_unlock (&putchar_lock);
	}
#ifdef TTY_PRO1000
	pro1000_putchar (c);
#endif
#ifdef TTY_SERIAL
	serial_putchar (c);
#else
	vramwrite_putchar (c);
#endif
}

static void
tty_init_global2 (void)
{
	char buf[100];

	snprintf (buf, 100, "tty:log=%p,logoffset=%p,loglen=%p,logmax=%lu\n",
		  log, &logoffset, &loglen, (unsigned long)sizeof log);
	debug_addstr (buf);
}

static void
tty_init_global (void)
{
	logoffset = 0;
	loglen = 0;
	logflag = true;
	spinlock_init (&putchar_lock);
	vramwrite_init_global ((void *)0x800B8000);
	putchar_set_func (tty_putchar, NULL);
}

void
tty_init_iohook (void)
{
#ifdef TTY_SERIAL
	serial_init_iohook ();
#endif
}

static void
tty_init_msg (void)
{
#ifdef TTY_SERIAL
	ttyin = msgopen ("serialin");
	ttyout = msgopen ("serialout");
#else
	ttyin = msgopen ("keyboard");
	ttyout = msgopen ("vramwrite");
#endif
	msgregister ("ttyin", ttyin_msghandler);
	msgregister ("ttyout", ttyout_msghandler);
	msgregister ("ttylog", ttylog_msghandler);
}

INITFUNC ("global0", tty_init_global);
INITFUNC ("global3", tty_init_global2);
INITFUNC ("msg1", tty_init_msg);
