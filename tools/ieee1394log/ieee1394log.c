/*
 * Copyright (c) 2014 Yushi Omote
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <libraw1394/raw1394.h>
#include <unistd.h>
#include <errno.h>

#define MAX_PORTS 16

typedef struct {
	raw1394handle_t handle;
	int port;
	int node;
	unsigned long long int addr;
} ieee1394log_t;

static int retry_count = 0;

static int
bus_reset_handler (raw1394handle_t handle, unsigned int generation)
{
	raw1394_update_generation (handle, generation);
	printf ("Bus reset.\n");
	return 0;
}

static int
raw1394_read_retry (raw1394handle_t handle, nodeid_t node, nodeaddr_t addr,
		    size_t length, quadlet_t *buffer)
{
	int i = 0;
	int ret;

	do {
		if (i)
			fprintf (stderr, "%d\b", i % 10);
		ret = raw1394_read (handle, node, addr, length, buffer);
		if (i)
			fprintf (stderr, " \b");
		if (ret >= 0 || errno != EAGAIN)
			return ret;
	} while (i++ < retry_count);
	return ret;
}

#define raw1394_read raw1394_read_retry

static int
setup (ieee1394log_t *ieee1394log, int port, int node)
{
	printf ("========================================\n");
	printf ("Scanning IEEE1394 ports...\n");
	printf ("========================================\n");

	raw1394handle_t handle;
	handle = raw1394_new_handle ();
	if (!handle) {
		perror ("Couldn't get handle");
		return -1;
	}

	int p, port_num;
	struct raw1394_portinfo ports[16];
	port_num = raw1394_get_port_info (handle, ports, MAX_PORTS);
	if (port_num <= 0) {
		fprintf (stderr, "No ports detected.\n");
		return -1;
	}
	printf ("%d ports detected.\n", port_num);

	int n, node_num, local_node;
	int i, r, r0, r1;
	unsigned char id[8];
	for (p = 0; p < port_num; p++) {
		raw1394_set_port (handle, p);
		node_num = raw1394_get_nodecount (handle);
		local_node = raw1394_get_local_id (handle) & 0x3f;
		printf ("Port(%d) %d nodes available:\n", p, node_num);
		for (n = 0; n < node_num; n++) {
			memset (id, 0, 8);
			r0 = raw1394_read (handle, n | 0xffc0,
					   0xfffff000040CULL, 4,
					   (quadlet_t *)id);
			r1 = raw1394_read (handle, n | 0xffc0,
					   0xfffff0000410ULL, 4,
					   (quadlet_t *)(id + 4));
			printf ("  * Node(%d) ", n);
			if (r0 >= 0 && r1 >= 0) {
				printf ("Identifier: ");
				for (i = 0; i < 8; i++)
					printf ("%02x", id[i]);
				printf (", ");
			} else {
				printf ("Identifier: (Unknown), ");
			}
			if (n == local_node)
				printf ("Connection: Local\n");
			else
				printf ("Connection: Remote\n");

			/* node: use first local by default */
			if (n != local_node && node == -1)
				node = n;
		}
	}
	if (node == -1) {
		fprintf (stderr, "No available nodes detected.\n");
		return -1;
	}
	/* port: use first one by default */
	if (port == -1)
		port = 0;

	r = raw1394_set_port (handle, port);
	if (r < 0) {
		perror ("Couldn't to set port");
		return -1;
	}
	raw1394_set_bus_reset_handler (handle, bus_reset_handler);

	ieee1394log->handle = handle;
	ieee1394log->port = port;
	ieee1394log->node = node;

	return 0;
}

static int
log_range (ieee1394log_t *ieee1394log, int from, int to)
{
	int i, j, len;
	char buf[4];

	for (i = from; i <= to; i += 4) {
		if (raw1394_read (ieee1394log->handle,
				  ieee1394log->node | 0xffc0,
				  ieee1394log->addr + 8 + i, 4,
				  (quadlet_t *)&buf) < 0) {
			perror ("Couldn't print range");
			return -1;
		}
		if (i + 4 <= to)
			len = 4;
		else
			len = to - i + 1;
		for (j = 0; j < len; j++)
			printf ("%c", buf[j]);
	}
	fflush (stdout);

	return 0;
}

static int
print_log (ieee1394log_t *ieee1394log)
{
	unsigned int logoffset, loglen;
	unsigned int plogoffset = -1, ploglen = -1;

	printf ("========================================\n");
	printf ("Log will appear below...\n");
	printf ("========================================\n");

	for (;;) {
		if (raw1394_read (ieee1394log->handle,
				  ieee1394log->node | 0xffc0,
				  ieee1394log->addr, 4, &logoffset) < 0) {
			perror ("Couldn't read log offset");
			return -1;
		}
		if (raw1394_read (ieee1394log->handle,
				  ieee1394log->node | 0xffc0,
				  ieee1394log->addr + 4, 4, &loglen) < 0) {
			perror ("Couldn't read log length");
			return -1;
		}

		if (plogoffset == -1) {
			if (log_range (ieee1394log, 0, loglen - 1) < 0) {
				fprintf (stderr,
					 "Couldn't print first all.\n");
				return -1;
			}
		} else {
			if (ploglen != loglen &&
			    log_range (ieee1394log, ploglen, loglen - 1) < 0) {
				fprintf (stderr, "Couldn't print diff.\n");
				return -1;
			}
			if (plogoffset != logoffset &&
			    log_range (ieee1394log, plogoffset,
				       logoffset - 1) < 0) {
				fprintf (stderr, "Couldn't print diff.\n");
				return -1;
			}
		}
		plogoffset = logoffset;
		ploglen = loglen;
		usleep (200 * 1000);
	}

	return 0;
}

static void
usage (char *prog)
{
	printf ("Usage: %s [-p port_no] [-n node_no] [-r retry] -i\n", prog);
	printf ("   or: %s [-p port_no] [-n node_no] [-r retry] address\n",
		prog);
	printf ("   -p port_no  : Target port number (optional).\n");
	printf ("   -n node_no  : Target node number (optional).\n");
	printf ("   -r retry    : Retry count (optional).\n");
	printf ("   -i          : Show port information.\n");
	printf ("   address     : Buffer address in target machine.\n");
}

int
main (int argc, char *argv[])
{
	int info_only = 0;
	int port = -1;
	int node = -1;
	ieee1394log_t *ieee1394log;
	int opt;

	while ((opt = getopt (argc, argv, "ip:n:r:")) != -1) {
		switch (opt) {
		case 'i':
			info_only = 1;
			break;
		case 'p':
			port = atoi (optarg);
			break;
		case 'n':
			node = atoi (optarg);
			break;
		case 'r':
			retry_count = atoi (optarg);
			break;
		default:
			usage (argv[0]);
			return 1;
		}
	}
	ieee1394log = malloc (sizeof *ieee1394log);
	if (!ieee1394log) {
		perror ("Couldn't allocate memory");
		return 1;
	}
	if (!info_only) {
		if (optind != argc - 1) {
			usage (argv[0]);
			return 1;
		}
		ieee1394log->addr = strtoll (argv[optind], NULL, 0);
	} else if (optind != argc) {
		usage (argv[0]);
		return 1;
	}
	if (setup (ieee1394log, port, node) < 0) {
		fprintf (stderr, "Couldn't setup ieee1394log...\n");
		return 1;
	}
	if (info_only)
		return 0;

	printf ("[%d:%d] Snooping 0x%08llx...\n",
		ieee1394log->port, ieee1394log->node, ieee1394log->addr);
	if (print_log (ieee1394log) < 0)
		printf ("Aborted.\n");

	return 0;
}
