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

#include <core/mm.h>
#include <core/panic.h>
#include <net/netapi.h>

struct netdata {
	struct netfunc *func;
	void *handle;
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
net_new_nic (char *arg_net)
{
	int i;
	char *arg = NULL;
	void *param = NULL;
	struct netlist *p;
	struct netfunc *func;
	struct netdata *handle;
	static struct netfunc net_null = {
		.new_nic = netapi_net_null_new_nic,
		.init = netapi_net_null_init,
		.start = netapi_net_null_start,
	};

	if (arg_net && arg_net[0] != '\0' && arg_net[0] != ':') {
		for (p = netlist_head; p; p = p->next) {
			for (i = 0;; i++) {
				if (arg_net[i] == ':') {
					arg = &arg_net[i + 1];
					goto matched;
				}
				if (arg_net[i] != p->name[i])
					break;
				if (arg_net[i] == '\0')
					goto matched;
			}
		}
		panic ("net_new_nic: invalid name net=%s", arg_net);
	matched:
		func = p->func;
		param = p->param;
	} else {
		func = &net_null;
		if (arg_net && arg_net[0] == ':')
			arg = &arg_net[1];
	}
	handle = alloc (sizeof *handle);
	handle->func = func;
	handle->handle = handle->func->new_nic (arg, param);
	return handle;
}

bool
net_init (struct netdata *handle, void *phys_handle, struct nicfunc *phys_func,
	  void *virt_handle, struct nicfunc *virt_func)
{
	return handle->func->init (handle->handle, phys_handle, phys_func,
				   virt_handle, virt_func);
}

void
net_start (struct netdata *handle)
{
	handle->func->start (handle->handle);
}

long
net_premap_recvbuf (struct netdata *handle, void *buf, unsigned int len)
{
	if (!handle->func->premap_recvbuf)
		return 0;
	return handle->func->premap_recvbuf (handle->handle, buf, len);
}
