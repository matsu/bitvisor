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
#include <core/list.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/spinlock.h>
#include <core/string.h>
#include <core/thread.h>
#include <net/netapi.h>
#include "ip_main.h"

struct net_task {
	LIST1_DEFINE (struct net_task);
	void (*func) (void *arg);
	void *arg;
};

struct net_ip_data {
	int pass;
	void *phys_handle, *virt_handle;
	struct nicfunc *phys_func, *virt_func;
	bool input_ok;
	void *input_arg;
};

struct net_ip_input_data {
	struct net_ip_data *p;
	void *buf;
	unsigned int len;
};

static LIST1_DEFINE_HEAD (struct net_task, net_task_list);
static spinlock_t net_task_lock;

static void
net_task_call (void)
{
	struct net_task *p;

	spinlock_lock (&net_task_lock);
	while ((p = LIST1_POP (net_task_list))) {
		spinlock_unlock (&net_task_lock);
		p->func (p->arg);
		free (p);
		spinlock_lock (&net_task_lock);
	}
	spinlock_unlock (&net_task_lock);
}

void
net_main_task_add (void (*func) (void *arg), void *arg)
{
	struct net_task *p;

	p = alloc (sizeof *p);
	p->func = func;
	p->arg = arg;
	spinlock_lock (&net_task_lock);
	LIST1_ADD (net_task_list, p);
	spinlock_unlock (&net_task_lock);
}

static void
net_thread (void *arg)
{
	struct net_ip_data *p = arg;
	struct ip_main_netif netif_arg[1];

	netif_arg[0].handle = p;
	netif_arg[0].use_as_default = 1;
	netif_arg[0].use_dhcp = config.ip.use_dhcp;
	netif_arg[0].ipaddr = config.ip.ipaddr;
	netif_arg[0].netmask = config.ip.netmask;
	netif_arg[0].gateway = config.ip.gateway;
	ip_main_init (netif_arg, 1);
	for (;;) {
		ip_main_task ();
		net_task_call ();
		schedule ();
	}
}

static void *
net_ip_new_nic (char *arg, void *param)
{
	struct net_ip_data *p;
	static int flag;

	if (flag)
		panic ("net=ip does not work with multiple network"
		       " interfaces");
	flag = 1;
	p = alloc (sizeof *p);
	p->pass = !!param;
	p->input_ok = false;
	return p;
}

static bool
net_ip_init (void *handle, void *phys_handle, struct nicfunc *phys_func,
	     void *virt_handle, struct nicfunc *virt_func)
{
	struct net_ip_data *p = handle;

	if (!virt_func && p->pass)
		return false;
	p->phys_handle = phys_handle;
	p->phys_func = phys_func;
	p->virt_handle = virt_handle;
	p->virt_func = virt_func;
	return true;
}

static void
net_ip_virt_recv (void *handle, unsigned int num_packets, void **packets,
		  unsigned int *packet_sizes, void *param, long *premap)
{
	struct net_ip_data *p = param;

	if (p->pass)
		p->phys_func->send (p->phys_handle, num_packets, packets,
				    packet_sizes, true);
}

static void
net_main_input_direct (void *arg)
{
	struct net_ip_input_data *data = arg;

	ip_main_input (data->p->input_arg, data->buf, data->len);
	free (data->buf);
	free (data);
}

static void
net_main_input_queue (struct net_ip_data *p, void **packets,
		      unsigned int *packet_sizes, unsigned int num_packets)
{
	struct net_ip_input_data *data;
	unsigned int i;

	for (i = 0; i < num_packets; i++) {
		/* Note: pbuf_alloc() must be called in the network
		 * thread, but this function is not. */
		/* FIXME: memcpy() is called twice for every input
		 * packet. Copying to pbuf directly is better. */
		data = alloc (sizeof *data);
		data->p = p;
		data->buf = alloc (packet_sizes[i]);
		data->len = packet_sizes[i];
		memcpy (data->buf, packets[i], packet_sizes[i]);
		net_main_task_add (net_main_input_direct, data);
	}
}

static void
net_ip_phys_recv (void *handle, unsigned int num_packets, void **packets,
		  unsigned int *packet_sizes, void *param, long *premap)
{
	struct net_ip_data *p = param;

	if (p->pass)
		p->virt_func->send (p->virt_handle, num_packets, packets,
				    packet_sizes, true);
	if (p->input_ok)
		net_main_input_queue (p, packets, packet_sizes, num_packets);
}

void
net_main_send (void *handle, void *buf, unsigned int len)
{
	struct net_ip_data *p = handle;

	p->phys_func->send (p->phys_handle, 1, &buf, &len, true);
}

void
net_main_poll (void *handle)
{
	struct net_ip_data *p = handle;

	if (p->phys_func->poll)
		p->phys_func->poll (p->phys_handle);
}

void
net_main_get_mac_address (void *handle, unsigned char *mac_address)
{
	struct net_ip_data *p = handle;
	struct nicinfo info;
	int i;

	p->phys_func->get_nic_info (p->phys_handle, &info);
	for (i = 0; i < 6; i++)
		mac_address[i] = info.mac_address[i];
}

void
net_main_set_recv_arg (void *handle, void *arg)
{
	struct net_ip_data *p = handle;

	p->input_arg = arg;
	p->input_ok = true;
}

static void
net_ip_start (void *handle)
{
	struct net_ip_data *p = handle;

	p->phys_func->set_recv_callback (p->phys_handle, net_ip_phys_recv, p);
	if (p->virt_func)
		p->virt_func->set_recv_callback (p->virt_handle,
						 net_ip_virt_recv, p);
	thread_new (net_thread, p, VMM_STACKSIZE);
}

static struct netfunc net_ip_func = {
	.new_nic = net_ip_new_nic,
	.init = net_ip_init,
	.start = net_ip_start,
};

static void
net_main_init (void)
{
	spinlock_init (&net_task_lock);
	LIST1_HEAD_INIT (net_task_list);
	net_register ("ip", &net_ip_func, NULL);
	net_register ("ippass", &net_ip_func, "");
}

INITFUNC ("driver1", net_main_init);
