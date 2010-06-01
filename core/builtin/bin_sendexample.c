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
#include <lib_printf.h>
#include <lib_string.h>
#include <lib_syscalls.h>

void
command_error (void)
{
	printf ("error.\n");
}

void
print_help (char *buf)
{
	printf ("s data : send\n");
	printf ("q : quit\n");
}

void
sendex (char *buf)
{
	unsigned char d[32];
	int a, i;
	struct msgbuf mbuf[2];

	a = msgopen ("recvex");
	if (a < 0) {
		printf ("msgopen failed.\n");
		return;
	}
	memset (d, 0, 32);
	printf ("sending buf %p len 0x%x recvbuf %p len 0x%lx\n",
		buf, strlen (buf), d, (unsigned long)sizeof d);
	setmsgbuf (&mbuf[0], buf, strlen (buf), 0);
	setmsgbuf (&mbuf[1], d, sizeof d, 1);
	if (msgsendbuf (a, 0, mbuf, 2)) {
		printf ("msgsendbuf failed.\n");
		msgclose (a);
		return;
	}
	msgclose (a);
	printf ("received: ");
	for (i = 0; i < 32; i++)
		printf ("0x%02x ", d[i]);
	printf ("\n");
}

int
_start (int a1, int a2)
{
	char buf[100];

	for (;;) {
		printf ("sendexample> ");
		lineinput (buf, 100);
		switch (buf[0]) {
		case 'h':
			print_help (buf);
			break;
		case 's':
			sendex (buf);
			break;
		case 'q':
			exitprocess (0);
			break;
		default:
			command_error ();
			break;
		}
	}
}
