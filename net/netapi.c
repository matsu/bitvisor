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

#include <core/config.h>
#include <core/initfunc.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/string.h>
#include <core/tty.h>
#include <net/netapi.h>

struct netdata {
	struct netfunc *func;
	void *handle;
	bool tty;
	void *tty_phys_handle;
	struct nicfunc *tty_phys_func;
	unsigned char mac_address[6];
};

struct netlist {
	struct netlist *next;
	char *name;
	struct netfunc *func;
	void *param;
};

struct net_null_data {
	void *phys_handle;
	struct nicfunc *phys_func;
	void *virt_handle;
	struct nicfunc *virt_func;
};

struct net_pass_data2 {
	void *handle;
	struct nicfunc *func;
};

struct net_pass_data {
	struct net_pass_data2 phys, virt;
};

static struct netlist *netlist_head = NULL;

static void
netapi_net_null_recv_callback (void *handle, unsigned int num_packets,
			       void **packets, unsigned int *packet_sizes,
			       void *param, long *premap)
{
	/* Do nothing. */
}

static void *
netapi_net_null_new_nic (char *arg, void *param)
{
	struct net_null_data *p;

	p = alloc (sizeof *p);
	return p;
}

static bool
netapi_net_null_init (void *handle, void *phys_handle,
		      struct nicfunc *phys_func, void *virt_handle,
		      struct nicfunc *virt_func)
{
	struct net_null_data *p = handle;

	p->phys_handle = phys_handle;
	p->phys_func = phys_func;
	p->virt_handle = virt_handle;
	p->virt_func = virt_func;
	return true;
}

static void
netapi_net_null_start (void *handle)
{
	struct net_null_data *p = handle;

	p->phys_func->set_recv_callback (p->phys_handle,
					 netapi_net_null_recv_callback, NULL);
	if (p->virt_func)
		p->virt_func->set_recv_callback (p->virt_handle,
						 netapi_net_null_recv_callback,
						 NULL);
}

static void
netapi_net_pass_recv_callback (void *handle, unsigned int num_packets,
			       void **packets, unsigned int *packet_sizes,
			       void *param, long *premap)
{
	struct net_pass_data2 *p = param;

	p->func->send (p->handle, num_packets, packets, packet_sizes, true);
}

static void *
netapi_net_pass_new_nic (char *arg, void *param)
{
	struct net_pass_data *p;

	p = alloc (sizeof *p);
	return p;
}

static bool
netapi_net_pass_init (void *handle, void *phys_handle,
		      struct nicfunc *phys_func, void *virt_handle,
		      struct nicfunc *virt_func)
{
	struct net_pass_data *p = handle;

	if (!virt_func)
		return false;
	p->phys.handle = phys_handle;
	p->phys.func = phys_func;
	p->virt.handle = virt_handle;
	p->virt.func = virt_func;
	return true;
}

static void
netapi_net_pass_start (void *handle)
{
	struct net_pass_data *p = handle;

	p->phys.func->set_recv_callback (p->phys.handle,
					 netapi_net_pass_recv_callback,
					 &p->virt);
	p->virt.func->set_recv_callback (p->virt.handle,
					 netapi_net_pass_recv_callback,
					 &p->phys);
}

void
net_register (char *netname, struct netfunc *func, void *param)
{
	struct netlist *p;

	p = alloc (sizeof *p);
	p->name = netname;
	p->func = func;
	p->param = param;
	p->next = netlist_head;
	netlist_head = p;
}

struct netdata *
net_new_nic (char *arg_net, bool tty)
{
	int i;
	char *arg = NULL;
	void *param;
	struct netlist *p;
	struct netfunc *func;
	struct netdata *handle;

	if (!arg_net)
		arg_net = "";
	for (p = netlist_head; p; p = p->next) {
		for (i = 0;; i++) {
			if (p->name[i] == '\0') {
				if (arg_net[i] == ':') {
					arg = &arg_net[i + 1];
					goto matched;
				}
				if (arg_net[i] == '\0')
					goto matched;
				break;
			}
			if (arg_net[i] != p->name[i])
				break;
		}
	}
	panic ("net_new_nic: invalid name net=%s", arg_net);
matched:
	func = p->func;
	param = p->param;
	handle = alloc (sizeof *handle);
	handle->func = func;
	handle->tty = tty;
	handle->tty_phys_func = NULL;
	handle->handle = handle->func->new_nic (arg, param);
	return handle;
}

static void
net_tty_send (void *tty_handle, void *packet, unsigned int packet_size)
{
	struct netdata *handle = tty_handle;
	char *pkt;

	pkt = packet;
	memcpy (pkt + 0, config.vmm.tty_mac_address, 6);
	memcpy (pkt + 6, handle->mac_address, 6);
	handle->tty_phys_func->send (handle->tty_phys_handle, 1, &packet,
				     &packet_size, false);
}

bool
net_init (struct netdata *handle, void *phys_handle, struct nicfunc *phys_func,
	  void *virt_handle, struct nicfunc *virt_func)
{
	if (!handle->func->init (handle->handle, phys_handle, phys_func,
				 virt_handle, virt_func))
		return false;
	if (handle->tty) {
		handle->tty_phys_handle = phys_handle;
		handle->tty_phys_func = phys_func;
	}
	return true;
}

void
net_start (struct netdata *handle)
{
	struct nicinfo info;

	if (handle->tty) {
		handle->tty_phys_func->get_nic_info (handle->tty_phys_handle,
						     &info);
		memcpy (handle->mac_address, info.mac_address,
			sizeof handle->mac_address);
		tty_udp_register (net_tty_send, handle);
	}
	handle->func->start (handle->handle);
}

long
net_premap_recvbuf (struct netdata *handle, void *buf, unsigned int len)
{
	if (!handle->func->premap_recvbuf)
		return 0;
	return handle->func->premap_recvbuf (handle->handle, buf, len);
}

static void
netapi_init (void)
{
	static struct netfunc net_null = {
		.new_nic = netapi_net_null_new_nic,
		.init = netapi_net_null_init,
		.start = netapi_net_null_start,
	};
	static struct netfunc net_pass = {
		.new_nic = netapi_net_pass_new_nic,
		.init = netapi_net_pass_init,
		.start = netapi_net_pass_start,
	};

	net_register ("", &net_null, NULL);
	net_register ("pass", &net_pass, NULL);
}

INITFUNC ("driver0", netapi_init);
