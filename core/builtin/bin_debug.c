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

unsigned char memdata[256];

void
command_error (void)
{
	printf ("error.\n");
}

void
print_help (char *buf)
{
	printf ("d guest-physical-address(hex) : dump memory\n");
	printf ("D vmm-virtual-address(hex) : dump memory \n");
	printf ("q : quit\n");
}

void
dump_mem (char *buf, int t)
{
	int c = 0;
	static unsigned long long v = 0;
	unsigned long long i;
	int tmp[16], j, k;
	int a;

	if (buf[1] == '\0')
		c = 1;
	else
		v = 0;
	for (;;) {
		buf++;
		if (*buf == '\0')
			break;
		if (*buf <= ' ')
			continue;
		if (*buf >= '0' && *buf <= '9') {
			c++;
			v = (v << 4) | (*buf - '0');
			continue;
		}
		if (*buf >= 'A' && *buf <= 'F') {
			c++;
			v = (v << 4) | (*buf - 'A' + 0xA);
			continue;
		}
		if (*buf >= 'a' && *buf <= 'f') {
			c++;
			v = (v << 4) | (*buf - 'a' + 0xA);
			continue;
		}
		c = 0;
		break;
	}
	if (c > 0) {
		a = msgopen ("memdump");
		if (a < 0) {
			printf ("msgopen failed.\n");
			return;
		}
		if (msgsendbuf (a, t, &v, sizeof v, memdata, sizeof memdata)) {
			printf ("msgsendbuf failed.\n");
		} else {
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
		}
		msgclose (a);
	} else {
		command_error ();
	}
}

int
_start (int a1, int a2)
{
	char buf[100];

	for (;;) {
		printf ("debug> ");
		lineinput (buf, 100);
		switch (buf[0]) {
		case 'h':
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
		default:
			command_error ();
			break;
		}
	}
}
