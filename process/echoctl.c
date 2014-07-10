/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2014 Yushi Omote
 * Copyright (c) 2014 Igel Co., Ltd
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
#include <lib_stdlib.h>
#include <lib_string.h>
#include <lib_syscalls.h>

static int
send_echoctl (int d, int cmd, int ipaddr, int port)
{
	int ret = -1;
	unsigned long array[3];
	struct msgbuf mbuf;

	array[0] = cmd;
	array[1] = ipaddr;
	array[2] = port;
	setmsgbuf (&mbuf, array, sizeof array, 0);
	ret = msgsendbuf (d, 0, &mbuf, 1);
	return ret;
}

static void
usage (char *hoge)
{
	printf ("%s",
		"usage:\n"
		"  client connect <ipaddr> <port>  Connect to echo server.\n"
		"  client send                     Send a message to client.\n"
		"  server start <port>             Start echo server.\n");
}

static int
conv_ipv4addr (char *str)
{
	char *p;
	int ret = 0, shift = 32, tmp;

	tmp = (int)strtol (str, &p, 0);
	while (*p == '.' && shift) {
		ret |= (tmp & 255) << (shift -= 8);
		tmp = (int)strtol (p + 1, &p, 0) & ((1 << shift) - 1);
	}
	return ret | tmp;
}

static int
action (int d, int argc, char **argv)
{
	int r, cmd, ipaddr = 0, port = 0;
	unsigned char ipaddr_a[4];

	if (argc < 3) {
		usage (argv[0]);
		return -1;
	}
	if (!strcmp (argv[1], "client")) {
		if (!strcmp (argv[2], "connect")) {
			if (argc != 5) {
				usage (argv[0]);
				return -1;
			}
			ipaddr = conv_ipv4addr (argv[3]);
			ipaddr_a[0] = ipaddr >> 24;
			ipaddr_a[1] = ipaddr >> 16;
			ipaddr_a[2] = ipaddr >> 8;
			ipaddr_a[3] = ipaddr;
			port = (int)strtol (argv[4], NULL, 0);
			printf ("Connecting to server at"
				" %d.%d.%d.%d:%d (%08x)\n", ipaddr_a[0],
				ipaddr_a[1], ipaddr_a[2], ipaddr_a[3], port,
				ipaddr);
			cmd = 0;
		} else if (!strcmp (argv[2], "send")) {
			if (argc != 3) {
				usage (argv[0]);
				return -1;
			}
			printf ("Sending a message...\n");
			cmd = 1;
		} else {
			usage (argv[0]);
			return -1;
		}
	} else if (!strcmp (argv[1], "server")) {
		if (!strcmp (argv[2], "start")) {
			if (argc != 4) {
				usage (argv[0]);
				return -1;
			}
			port = (int)strtol (argv[3], NULL, 0);
			printf ("Starting server (Port:%d)...\n", port);
			cmd = 2;
		} else {
			usage (argv[0]);
			return -1;
		}
	} else {
		usage (argv[0]);
		return -1;
	}
	r = send_echoctl (d, cmd, ipaddr, port);
	if (r)
		printf ("Error Code: %d\n", r);
	printf ("Done.\n");
	return 0;
}

static void
parsearg (char *buf, int *argc, char **argv, int maxargc)
{
	while (*buf == ' ')
		buf++;
	*argc = 1;
	argv[0] = "";
	while (*buf != '\0' && *argc < maxargc) {
		argv[(*argc)++] = buf;
		buf = strchr (buf, ' ');
		if (!buf)
			break;
		*buf++ = '\0';
		while (*buf == ' ')
			buf++;
	}
}

int
_start (int a1, int a2)
{
	char buf[100];
	int argc;
	char *argv[10];
	int d;

	d = msgopen ("echoctl");
	if (d < 0) {
		printf ("echoctl not found.\n");
		exitprocess (1);
	}
	for (;;) {
		printf ("echoctl> ");
		lineinput (buf, 100);
		if (!strcmp (buf, ""))
			break;
		parsearg (buf, &argc, argv, 10);
		action (d, argc, argv);
	}
	msgclose (d);
	exitprocess (0);
	return 0;
}
