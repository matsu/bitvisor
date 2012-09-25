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
#include <lib_syscalls.h>

static int
recvex_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	int i;
	void *recvbuf, *sendbuf;
	int recvlen, sendlen;

	printf ("***recvexample start***\n");
	if (bufcnt < 2) {
		printf ("bufcnt %d < 2\n", bufcnt);
		return -1;
	}
	recvbuf = buf[0].base;
	recvlen = buf[0].len;
	sendbuf = buf[1].base;
	sendlen = buf[1].len;
	printf ("recvbuf %p recvlen 0x%x\n", recvbuf, recvlen);
	printf ("sendbuf %p sendlen 0x%x\n", sendbuf, sendlen);
	printf ("recvbuf: ");
	for (i = 0; i < 16 && i < recvlen; i++)
		printf ("0x%02x ", ((unsigned char *)recvbuf)[i]);
	printf ("\n");
	printf ("sendbuf: ");
	for (i = 0; i < 16 && i < sendlen; i++)
		printf ("0x%02x ", ((unsigned char *)sendbuf)[i] =
			((unsigned char *)recvbuf)[i] + 1);
	printf ("\n");
	printf ("***recvexample end***\n");
	return 0;
}

int
_start (int a1, int a2)
{
	printf ("recvex registered %d\n",
		msgregister ("recvex", recvex_msghandler));
	return 0;
}
