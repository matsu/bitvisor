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

#include <lib_lineinput.h>
#include <lib_mm.h>
#include <lib_printf.h>
#include <lib_stdlib.h>
#include <lib_string.h>
#include <lib_syscalls.h>

struct pci_monitor_log {
	unsigned long long time;
	unsigned long long value;
	unsigned long long base;
	unsigned int offset;
	unsigned int count;
	unsigned char n, rw, type, len;
};

int heap[65536 * 8], heaplen = 65536 * 8;

int
_start (int a1, int a2)
{
	int d, i, n;
	char buf[100], *p;
	struct msgbuf mbuf[2];
	struct pci_monitor_log *log;

	d = msgopen ("pci_monitor");
	if (d < 0) {
		printf ("pci_monitor not found\n");
		exitprocess (0);
	}
	n = msgsendint (d, -1);
	if (n < 0) {
		printf ("sendint -1 failed\n");
		goto close_and_exit;
	}
	for (i = 0; i < n; i++) {
		setmsgbuf (&mbuf[0], buf, sizeof buf, 1);
		if (msgsendbuf (d, i, mbuf, 1) < 0)
			printf ("%d: msgsendbuf error\n", i);
		else
			printf ("%d: %s\n", i, buf);
	}
	for (;;) {
		printf ("monitor> ");
		lineinput (buf, 100);
		if (!strcmp (buf, ""))
			goto close_and_exit;
		i = (int)strtol (buf, &p, 0);
		if (buf == p) {
			printf ("Invalid number\n");
			continue;
		}
		n = msgsendint (d, i);
		if (n <= 0) {
			printf ("%d: msgsendint error\n", i);
			continue;
		}
		log = alloc (sizeof *log * n);
		setmsgbuf (&mbuf[0], "", 1, 0);
		setmsgbuf (&mbuf[1], log, sizeof *log * n, 1);
		n = msgsendbuf (d, i, mbuf, 2);
		if (n < 0)
			printf ("%d: msgsendbuf error\n", i);
		for (i = 0; i < n; i++) {
			printf ("%20llu", log[i].time);
			switch (log[i].n) {
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				printf (" bar%u", log[i].n);
				break;
			case 6:
				printf (" conf");
				break;
			default:
				printf (" ----");
			}
			printf (" %s %s 0x%08llX 0x%08X",
				log[i].type == 1 ? "i/o" :
				log[i].type == 2 ? "mem" : "---",
				log[i].rw ? "w" : "r",
				log[i].base + log[i].offset, log[i].offset);
			printf (log[i].len == 1 ? " b 0x%02llX" :
				log[i].len == 2 ? " w 0x%04llX" :
				log[i].len == 4 ? " l 0x%08llX" :
				log[i].len == 8 ? " q 0x%016llX" :
				" - 0x%08llX",
				log[i].value);
			if (log[i].count)
				printf (" %u", log[i].count);
			printf ("\n");
		}
		free (log);
	}
close_and_exit:
	msgclose (d);
	exitprocess (0);
	return 0;
}
