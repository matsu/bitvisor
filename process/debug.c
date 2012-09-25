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

#include <lib_ctype.h>
#include <lib_lineinput.h>
#include <lib_printf.h>
#include <lib_string.h>
#include <lib_syscalls.h>

enum memdump_type {
	MEMDUMP_GPHYS,
	MEMDUMP_HVIRT,
	MEMDUMP_GVIRT,
	MEMDUMP_HPHYS,
};

typedef unsigned long long u64;
typedef unsigned long ulong;

struct memdump_data {
	u64 physaddr;
	u64 cr0, cr3, cr4, efer;
	ulong virtaddr;
};

unsigned char memdata[128];
enum memdump_type dtype;
u64 daddr;
int dinit;

char logbuf[65536];
int loglen;
int logoff;

struct {
	u64 rip, rsp, gdtrbase, idtrbase;
	u64 cr0, cr3, cr4, efer;
} guest;

void
command_error (void)
{
	printf ("error.\n");
}

void
print_help (char *buf)
{
	printf ("dp guest-physical-address(hex) : dump memory\n");
	printf ("dv guest-virtual-address(hex) : dump memory\n");
	printf ("Dp vmm-physical-address(hex) : dump memory\n");
	printf ("Dv vmm-virtual-address(hex) : dump memory\n");
	printf ("h value1(hex) value2(hex) : add/sub\n");
	printf ("r [register-name register-value(hex)]: print/set guest registers\n");
	printf ("n : get next guest state from log\n");
	printf ("! command : call external command\n");
	printf ("q : quit\n");
}

char *
skip_space (char *buf)
{
	while (*buf && isspace (*buf))
		buf++;
	return buf;
}

int
parse_hex (char **buf, unsigned long long *v)
{
	int c = 0;

	*v = 0;
	for (;;++*buf) {
		if (**buf == '\0')
			break;
		if (**buf <= ' ')
			break;
		if (**buf >= '0' && **buf <= '9') {
			c++;
			*v = (*v << 4) | (**buf - '0');
			continue;
		}
		if (**buf >= 'A' && **buf <= 'F') {
			c++;
			*v = (*v << 4) | (**buf - 'A' + 0xA);
			continue;
		}
		if (**buf >= 'a' && **buf <= 'f') {
			c++;
			*v = (*v << 4) | (**buf - 'a' + 0xA);
			continue;
		}
		break;
	}
	return c;
}

void
dump_mem (char *buf, int t)
{
	int c = 0;
	unsigned long long i, v;
	int tmp[16], j, k;
	int a;
	struct memdump_data d;
	struct msgbuf mbuf[3];
	char errbuf[64];

	buf++;
	buf = skip_space (buf);
	if (*buf == '\0')
		c = 1;
	if (c) {
		if (dinit == 0) {
			command_error ();
			return;
		}
	} else {
		dinit = 0;
		switch (*buf) {
		case 'p':
		case 'P':
			dtype = t ? MEMDUMP_HPHYS : MEMDUMP_GPHYS;
			buf++;
			break;
		case 'v':
		case 'V':
			dtype = t ? MEMDUMP_HVIRT : MEMDUMP_GVIRT;
			buf++;
			break;
		default:
			command_error ();
			return;
		}
		buf = skip_space (buf);
		if (!parse_hex (&buf, &daddr)) {
			command_error ();
			return;
		}
		dinit = 1;
	}
	d.physaddr = daddr;
	d.cr3 = guest.cr3;
	d.cr0 = guest.cr0;
	d.cr4 = guest.cr4;
	d.efer = guest.efer;
	d.virtaddr = (ulong)daddr;
	a = msgopen ("memdump");
	if (a < 0) {
		printf ("msgopen failed.\n");
		return;
	}
	setmsgbuf (&mbuf[0], &d, sizeof d, 0);
	setmsgbuf (&mbuf[1], memdata, sizeof memdata, 1);
	setmsgbuf (&mbuf[2], errbuf, sizeof errbuf, 1);
	if (msgsendbuf (a, dtype, mbuf, 3)) {
		printf ("msgsendbuf failed.\n");
	} else {
		v = daddr;
		i = v & ~0xFULL;
		k = -(int)(v & 0xF);
		do {
			for (j = 0; j < 16; j++) {
				if (k >= 0 && k < sizeof memdata)
					tmp[j] = memdata[k];
				else
					tmp[j] = -1;
				k++;
			}
			printf ("%08llX ", i); /* address */
			for (j = 0; j < 8; j++) { /* hex dump */
				if (tmp[j] < 0)
					printf ("   ");
				else
					printf (" %02X", tmp[j]);
			}
			if (tmp[7] < 0 || tmp[8] < 0)
				printf (" ");
			else
				printf ("-");
			for (j = 8; j < 16; j++) {
				if (tmp[j] < 0)
					printf ("   ");
				else
					printf ("%02X ", tmp[j]);
			}
			printf ("  ");
			for (j = 0; j < 16; j++) {	/* string */
				if (tmp[j] < 0)
					printf (" ");
				else if (isprint (tmp[j]))
					printf ("%c", tmp[j]);
				else
					printf (".");
			}
			printf ("\n");
			i += 16;
		} while (k < sizeof memdata);
		v = v + sizeof memdata;
		daddr = v;
		if (errbuf[0])
			printf ("Error: %s\n", errbuf);
	}
	msgclose (a);
}

void
getlog (void)
{
	int d;
	struct msgbuf mbuf;

	memset (logbuf, 0, sizeof logbuf);
	d = msgopen ("ttylog");
	if (d >= 0) {
		setmsgbuf (&mbuf, logbuf, sizeof logbuf, 1);
		loglen = msgsendbuf (d, 0, &mbuf, 1);
		msgclose (d);
		if (loglen > sizeof logbuf)
			loglen = sizeof logbuf;
	} else {
		printf ("ttylog not found.\n");
		loglen = 0;
	}
}

int
findlog (int off, int end, char *key, int len, int *ii)
{
	int i;

	for (i = off; i < end; i++) {
		if (logbuf[i] == key[0]) {
			if (memcmp (&logbuf[i], key, len) == 0) {
				*ii = i + len;
				return 1;
			}
		}
	}
	return 0;
}

void
getnextstate (void)
{
	int i, j;
	char *p;
	static char key1[] = "\nGuest state and registers of ";
	static struct {
		char *key;
		u64 *data;
	} *pp, l[] = {
		{ "RSP ", &guest.rsp },
		{ "CR0 ", &guest.cr0 },
		{ "CR3 ", &guest.cr3 },
		{ "CR4 ", &guest.cr4 },
		{ "EFER ", &guest.efer },
		{ "RIP ", &guest.rip },
		{ "GDTR ", &guest.gdtrbase },
		{ "IDTR ", &guest.idtrbase },
		{ 0, 0 }
	};

	if (findlog (logoff, loglen - strlen (key1), key1, strlen (key1), &i))
		goto found;
	if (findlog (0, logoff, key1, strlen (key1), &i))
		goto found;
	printf ("guest state not found\n");
	return;
found:
	logoff = i;
	for (; i < loglen && logbuf[i] != '-'; i++)
		printf ("%c", logbuf[i]);
	for (pp = l; pp->key; pp++) {
		if (!findlog (i, loglen - 3, pp->key, strlen (pp->key), &j)) {
			printf ("%s not found\n", pp->key);
			continue;
		}
		p = &logbuf[j];
		p = skip_space (p);
		if (!parse_hex (&p, pp->data)) {
			printf ("%s parse failed\n", pp->key);
			continue;
		}
	}
	printf (" guest state loaded\n");
}

void
guestreg (char *buf)
{
	static struct {
		char *fmt;
		char *name;
		u64 *data;
	} *pp, l[] = {
		{ "%s  %08llX   ", "RIP",  &guest.rip },
		{ "%s  %08llX   ", "RSP",  &guest.rsp },
		{ "%s %08llX   ",  "GDTR", &guest.gdtrbase },
		{ "%s %08llX\n",   "IDTR", &guest.idtrbase },
		{ "%s  %08llX   ", "CR0",  &guest.cr0 },
		{ "%s  %08llX   ", "CR3",  &guest.cr3 },
		{ "%s  %08llX   ", "CR4",  &guest.cr4 },
		{ "%s %08llX\n",   "EFER", &guest.efer },
		{ 0, 0, 0 }
	};
	u64 tmp;

	if (buf[1] == '\0') {
		for (pp = l; pp->fmt; pp++)
			printf (pp->fmt, pp->name, *pp->data);
	} else {
		buf++;
		buf = skip_space (buf);
		for (pp = l; pp->fmt; pp++) {
			if (memcmp (buf, pp->name, strlen (pp->name)) == 0)
				goto next1;
		}
		printf ("bad register name\n");
		return;
	next1:
		buf += strlen (pp->name);
		buf = skip_space (buf);
		if (!parse_hex (&buf, &tmp)) {
			printf ("bad register value\n");
			return;
		}
		*pp->data = tmp;
	}
}

void
hexaddsub (char *buf)
{
	u64 val1, val2;


	buf++;
	buf = skip_space (buf);
	if (!parse_hex (&buf, &val1)) {
		command_error ();
		return;
	}
	buf = skip_space (buf);
	if (!parse_hex (&buf, &val2)) {
		command_error ();
		return;
	}
	printf ("%llX %llX\n", val1 + val2, val1 - val2);
}

void
callprog (char *buf)
{
	int d;

	buf++;
	buf = skip_space (buf);
	d = newprocess (buf);
	if (d >= 0) {
		msgsenddesc (d, 0);
		msgsenddesc (d, 1);
		msgsendint (d, 0);
		msgclose (d);
	} else
		printf ("%s not found.\n", buf);
}

int
_start (int a1, int a2)
{
	char buf[100];

	getlog ();
	logoff = 0;
	getnextstate ();
	for (;;) {
		printf ("debug> ");
		lineinput (buf, 100);
		switch (buf[0]) {
		case 'h':
			if (buf[1] != '\0' && buf[1] != 'e') {
				hexaddsub (buf);
				break;
			}
			/* fall through */
		case '?':
			print_help (buf);
			break;
		case 'd':
			dump_mem (buf, 0);
			break;
		case 'D':
			dump_mem (buf, 1);
			break;
		case 'q':
			exitprocess (0);
			break;
		case 'r':
			guestreg (buf);
			break;
		case 'n':
			getnextstate ();
			break;
		case '!':
			callprog (buf);
			break;
		default:
			command_error ();
			break;
		}
	}
}
