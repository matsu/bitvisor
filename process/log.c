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

char buf[65536];

void
print_log (char *buf, int len)
{
	int col = 80, row = 23, x, y, i, c, kbd = 0, top = 0, n = 0;

	goto first;
loop:
	n = 0;
	printf ("\n\e[2J\e[H\r       \r");
first:
	x = 0;
	y = 0;
	for (i = 0; i < len; i++) {
		if (y == top + row) {
			if (n)
				goto pagenext;
			for (;;) {
				printf ("-- more --");
				c = msgsendint (kbd, 0);
				printf ("\r          \r");
				switch (c) {
				case 'f':
				case 'v' & 31:
				case ' ':
					n = row;
				pagenext:
					n--;
				case 'j':
				case 'n' & 31:
				case '\n':
				case '\r':
					top++;
					goto cont;
				case 'k':
				case 'p' & 31:
					if (top == 0)
						break;
					top--;
					goto loop;
				case 'b':
				case 128+'v':
					if (top == 0)
						break;
					if (top < row)
						top = 0;
					else
						top -= row;
					goto loop;
				case 'q':
					return;
				}
			}
		}
	cont:
		if (y >= top) {
			if (buf[i] == '\n' || (buf[i] >= ' ' && buf[i] <= '~'))
				printf ("%c", buf[i]);
			else
				printf (" ");
		}
		if (buf[i] == '\n' || ++x == col)
			x = 0, ++y;
	}
	for (;;) {
		printf ("-- end --");
		c = msgsendint (kbd, 0);
		printf ("\r         \r");
		switch (c) {
		case 'k':
		case 'p' & 31:
			if (top == 0)
				break;
			top--;
			goto loop;
		case 'b':
		case 128+'v':
			if (top == 0)
				break;
			if (top < row)
				top = 0;
			else
				top -= row;
			goto loop;
		case 'q':
			return;
		}
	}
}

int
_start (int a1, int a2)
{
	int d, len;
	struct msgbuf mbuf;

	d = msgopen ("ttylog");
	if (d >= 0) {
		setmsgbuf (&mbuf, buf, sizeof buf, 1);
		len = msgsendbuf (d, 0, &mbuf, 1);
		msgclose (d);
		if (len > sizeof buf)
			len = sizeof buf;
		print_log (buf, len);
	} else
		printf ("ttylog not found.\n");
	exitprocess (0);
	return 0;
}
