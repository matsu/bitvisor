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

#include "arith.h"
#include "calluefi.h"
#include "cpu.h"
#include "initfunc.h"
#include "list.h"
#include "mm.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h"
#include "putchar.h"
#include "serial.h"
#include "spinlock.h"
#include "string.h"
#include "tty.h"
#include "uefi.h"
#include "vramwrite.h"

struct tty_udp_data {
	LIST1_DEFINE (struct tty_udp_data);
	void (*tty_send) (void *handle, void *packet,
			  unsigned int packet_size);
	void *handle;
};

static struct {
	unsigned int logoffset, loglen;
	unsigned char log[65536];
}  __attribute__ ((aligned (0x1000), packed)) logbuf;
static int ttyin, ttyout;
static spinlock_t putchar_lock;
static bool logflag;
static LIST1_DEFINE_HEAD (struct tty_udp_data, tty_udp_list);
static unsigned char uefi_log[1024];
static int uefi_logoffset;

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
ttylog_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	int i;
	unsigned char *q;

	if (m == 1 && bufcnt >= 1) {
		q = buf[0].base;
		for (i = 0; i < buf[0].len && i < logbuf.loglen; i++)
			q[i] = logbuf.log[(logbuf.logoffset + i) %
					  sizeof logbuf.log];
	}
	return logbuf.loglen;
}

void
ttylog_stop (void)
{
	logflag = false;
}

static void
wshort (char *off, unsigned short x)
{
	off[0] = (x >> 8);
	off[1] = x;
}

static int
mkudp (char *buf, char *src, int sport, char *dst, int dport,
       char *data, int datalen)
{
	u16 sum;

	memcpy (buf, "\x45\x00\x00\x00\x00\x01\x00\x00\x01\x11\x00\x00", 12);
	wshort (buf + 2,  datalen + 8 + 20);
	memcpy (buf + 12, src, 4);
	memcpy (buf + 16, dst, 4);
	wshort (buf + 20, sport);
	wshort (buf + 22, dport);
	wshort (buf + 24, datalen + 8);
	memcpy (buf + 26, "\x00\x11", 2);
	memcpy (buf + 28, data, datalen);
	sum = ~ipchecksum (buf + 12, datalen + 16);
	memcpy (buf + 26, &sum, 2);
	sum = ipchecksum (buf + 24, 4);
	memcpy (buf + 26, &sum, 2);
	sum = ipchecksum (buf, 20);
	memcpy (buf + 10, &sum, 2);
	return datalen + 8 + 20;
}

/* how to receive the messages:
   perl -e '$|=1;use Socket;
   socket(S, PF_INET, SOCK_DGRAM, 0);
   bind(S, pack_sockaddr_in(10101,INADDR_ANY));
   while(recv(S,$buf,1,0)){print $buf;}' */
static void
tty_udp_putchar (unsigned char c)
{
	struct tty_udp_data *p;
	unsigned int pktsiz;
	char pkt[64];

	LIST1_FOREACH (tty_udp_list, p) {
		memcpy (pkt + 12, "\x08\x00", 2);
		pktsiz = mkudp (pkt + 14, "\x00\x00\x00\x00", 10,
				"\xE0\x00\x00\x01", 10101, (char *)&c, 1) + 14;
		p->tty_send (p->handle, pkt, pktsiz);
	}
}

void
tty_putchar (unsigned char c)
{
	int i;

	if (logflag) {
		spinlock_lock (&putchar_lock);
		logbuf.log[(logbuf.logoffset + logbuf.loglen) %
			   sizeof logbuf.log] = c;
		if (logbuf.loglen == sizeof logbuf.log)
			logbuf.logoffset = (logbuf.logoffset + 1) %
				sizeof logbuf.log;
		else
			logbuf.loglen++;
		spinlock_unlock (&putchar_lock);
	}
	tty_udp_putchar (c);
#ifdef TTY_SERIAL
	serial_putchar (c);
#else
	if (uefi_booted) {
		if (currentcpu_available () && currentcpu->pass_vm_created) {
			spinlock_lock (&putchar_lock);
			for (i = 0; i < uefi_logoffset; i++)
				vramwrite_putchar (uefi_log[i]);
			uefi_logoffset = 0;
			spinlock_unlock (&putchar_lock);
			vramwrite_putchar (c);
		} else if (currentcpu_available () && get_cpu_id () == 0) {
			spinlock_lock (&putchar_lock);
			for (i = 0; i < uefi_logoffset; i++)
				call_uefi_putchar (uefi_log[i]);
			uefi_logoffset = 0;
			call_uefi_putchar (c);
			spinlock_unlock (&putchar_lock);
		} else {
			spinlock_lock (&putchar_lock);
			if (uefi_logoffset < sizeof uefi_log)
				uefi_log[uefi_logoffset++] = c;
			spinlock_unlock (&putchar_lock);
		}
	} else {
		vramwrite_putchar (c);
	}
#endif
}

void
tty_udp_register (void (*tty_send) (void *handle, void *packet,
				    unsigned int packet_size), void *handle)
{
	struct tty_udp_data *p;

	p = alloc (sizeof *p);
	p->tty_send = tty_send;
	p->handle = handle;
	LIST1_ADD (tty_udp_list, p);
}

void
tty_get_logbuf_info (virt_t *virt, phys_t *phys, uint *size)
{
	if (virt)
		*virt = (virt_t)&logbuf;
	if (phys)
		*phys = sym_to_phys (&logbuf);
	if (size)
		*size = sizeof logbuf;
}

static void
tty_init_global2 (void)
{
	LIST1_HEAD_INIT (tty_udp_list);
}

static void
tty_init_global (void)
{
	logbuf.logoffset = 0;
	logbuf.loglen = 0;
	logflag = true;
	spinlock_init (&putchar_lock);
	if (!uefi_booted)
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
