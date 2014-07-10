/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#ifndef __NET_NETAPI_H
#define __NET_NETAPI_H

typedef void net_recv_callback_t (void *handle, unsigned int num_packets,
				  void **packets, unsigned int *packet_sizes,
				  void *param, long *premap);

struct nicinfo {
	unsigned char mac_address[6];
	unsigned int mtu;
	unsigned long long int media_speed;
};

struct nicfunc {
	void (*get_nic_info) (void *handle, struct nicinfo *info);
	void (*send) (void *handle, unsigned int num_packets, void **packets,
		      unsigned int *packet_sizes, bool print_ok);
	void (*set_recv_callback) (void *handle, net_recv_callback_t *callback,
				   void *param);
	void (*poll) (void *handle); /* optional */
};

struct netfunc {
	void *(*new_nic) (char *arg, void *param);
	bool (*init) (void *handle, void *phys_handle,
		      struct nicfunc *phys_func, void *virt_handle,
		      struct nicfunc *virt_func);
	void (*start) (void *handle);
	long (*premap_recvbuf) (void *handle, void *buf,
				unsigned int len); /* optional */
};

struct netdata;

struct netdata *net_new_nic (char *arg_net, bool tty);
bool net_init (struct netdata *handle, void *phys_handle,
	       struct nicfunc *phys_func, void *virt_handle,
	       struct nicfunc *virt_func);
void net_start (struct netdata *handle);
long net_premap_recvbuf (struct netdata *handle, void *buf, unsigned int len);
void net_register (char *netname, struct netfunc *func, void *param);

#endif
