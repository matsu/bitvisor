/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2014 Yushi Omote
 * Copyright (c) 2014 Igel Co., Ltd
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

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 */

#include "lwip/init.h"
#include "lwip/dhcp.h"
#include "lwip/tcpip.h"
#include "lwip/autoip.h"
#include "lwip/opt.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include "netif/etharp.h"
#include "net_main.h"
#include "ip_main.h"
#include "tcpip.h"
#include "wireguard/wg_main.h"

struct tcpip_context {
	ip_addr_t *oldip_addr;
	struct netif *netif;
	int netif_num;
};

struct custom_pbuf {
	struct pbuf_custom p;
	void (*free) (void *free_arg);
	void *free_arg;
	void *buf;
};

static struct tcpip_context *tcpip_context;

static void
tcpip_netif_ipaddr_check_sub (struct netif *netif, ip_addr_t *oldip_addr)
{
	char ipaddr1[16];
	char ipaddr2[16];
	char *p, *q;

	if (!ip_addr_cmp (&netif->ip_addr, oldip_addr)) {
		p = ipaddr1;
		q = ipaddr_ntoa (oldip_addr);
		while ((*p++ = *q++) != '\0');
		p = ipaddr2;
		q = ipaddr_ntoa (&netif->ip_addr);
		while ((*p++ = *q++) != '\0');
		printf ("IP address changed: %s -> %s\n", ipaddr1, ipaddr2);
		*oldip_addr = netif->ip_addr;
	}
}

static void
tcpip_netif_ipaddr_check (void)
{
	int i;

	LWIP_ASSERT ("tcpip_context", tcpip_context);
	LWIP_ASSERT ("tcpip_context->netif", tcpip_context->netif);
	for (i = 0; i < tcpip_context->netif_num; i++)
		tcpip_netif_ipaddr_check_sub (&tcpip_context->netif[i],
					      &tcpip_context->oldip_addr[i]);
}

static err_t
ip_netif_output (struct netif *netif, struct pbuf *p)
{
	char buf[1600];
	int offset = 0, len;

	if (p && !p->next) {
		/* Fast path */
		net_main_send (netif->state, p->payload, p->len);
		return ERR_OK;
	}
	for (; p; p = p->next) {
		len = p->len;
		if (offset + len > sizeof buf)
			len = sizeof buf - offset;
		memcpy (buf + offset, p->payload, len);
		offset += len;
	}
	net_main_send (netif->state, buf, offset);
	return ERR_OK;
}

static void
tcpip_netif_poll (void)
{
	int i;

	LWIP_ASSERT ("tcpip_context", tcpip_context);
	LWIP_ASSERT ("tcpip_context->netif", tcpip_context->netif);
	for (i = 0; i < tcpip_context->netif_num; i++)
		net_main_poll (tcpip_context->netif[i].state);
}

static void
custom_free (struct pbuf *p)
{
	struct custom_pbuf *cpbuf = (void *)p;

	cpbuf->free (cpbuf->free_arg);
	mem_free (cpbuf);
}

/* This function is not called on a network thread */
enum ip_main_destination
ip_main_input_test (void *arg, void *buf, unsigned int len)
{
	struct eth_hdr *ethhdr;

	ethhdr = buf;
	if (len >= sizeof *ethhdr) {
		switch (ethhdr->type) {
		case PP_HTONS (ETHTYPE_IP):
		case PP_HTONS (ETHTYPE_ARP):
			return IP_MAIN_DESTINATION_ALL;
		}
	}
	return IP_MAIN_DESTINATION_OTHERS;
}

/* This function is not called on a network thread */
enum ip_main_destination
ip_main_input_test_destination (void *arg, void *buf, unsigned int len)
{
	struct netif *netif = arg;
	struct eth_hdr *ethhdr;
	struct ip_hdr *iphdr;
	ip_addr_t dest;

	if (len < sizeof *ethhdr + sizeof *iphdr)
		return ip_main_input_test (arg, buf, len);
	ethhdr = buf;
	switch (ethhdr->type) {
	case PP_HTONS (ETHTYPE_IP):
		break;
	case PP_HTONS (ETHTYPE_ARP):
		return IP_MAIN_DESTINATION_ALL;
	default:
		return IP_MAIN_DESTINATION_OTHERS;
	}
	/* EtherType IP */
	iphdr = buf + sizeof *ethhdr;
	if (IPH_PROTO (iphdr) != IP_PROTO_TCP)
		return IP_MAIN_DESTINATION_ALL;
	/* Protocol TCP */
	ip_addr_copy_from_ip4 (dest, iphdr->dest);
	if (!ip_addr_cmp (&netif->ip_addr, &dest))
		return IP_MAIN_DESTINATION_OTHERS;
	/* Destination IP address == netif->ip_addr */
	return IP_MAIN_DESTINATION_ME;
}

void
ip_main_input (void *arg, void *buf, unsigned int len,
	       void (*free) (void *free_arg), void *free_arg)
{
	struct netif *netif = arg;
	struct custom_pbuf *cpbuf;
	struct pbuf *p;

	cpbuf = mem_malloc (sizeof *cpbuf);
	LWIP_ASSERT ("mem_malloc cpbuf", cpbuf);
	cpbuf->free = free;
	cpbuf->free_arg = free_arg;
	cpbuf->buf = buf;
	cpbuf->p.custom_free_function = custom_free;
	p = pbuf_alloced_custom (PBUF_RAW, len, PBUF_REF, &cpbuf->p,
				 buf, len);
	LWIP_ASSERT ("pbuf_alloc", p);
	LWIP_ASSERT ("p->tot_len == len", p->tot_len == len);
	if (netif->input (p, netif) != ERR_OK) {
		pbuf_free (p);
		printf ("IP/ARP Input Error.\n");
	}
}

void
tcpip_begin (tcpip_task_fn_t *func, void *arg)
{
	net_main_task_add (func, arg);
}

static err_t
ip_netif_init (struct netif *netif)
{
	netif->name[0] = 'v';
	netif->name[1] = 'm';
	netif->output = etharp_output;
	netif->linkoutput = ip_netif_output;
	netif->hwaddr_len = ETHARP_HWADDR_LEN;
	net_main_get_mac_address (netif->state, netif->hwaddr);
	netif->mtu = 1500;
	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP |
		NETIF_FLAG_LINK_UP;
	net_main_set_recv_arg (netif->state, netif);
	return ERR_OK;
}

static void
tcpip_netif_conf_sub (struct netif *netif, unsigned char *ipaddr_a,
		      unsigned char *netmask_a, unsigned char *gw_a,
		      int use_as_default, int use_dhcp, void *handle,
		      ip_addr_t *oldip_addr)
{
	ip_addr_t ipaddr, netmask, gw;

	/* Setup IP address etc. */
	IP4_ADDR (&ipaddr, ipaddr_a[0], ipaddr_a[1], ipaddr_a[2], ipaddr_a[3]);
	IP4_ADDR (&netmask, netmask_a[0], netmask_a[1], netmask_a[2],
		  netmask_a[3]);
	IP4_ADDR (&gw, gw_a[0], gw_a[1], gw_a[2], gw_a[3]);
	IP4_ADDR (oldip_addr, 0, 0, 0, 0);

	/* Register the network interface. */
	netif_add (netif, &ipaddr, &netmask, &gw, handle, ip_netif_init,
		   ethernet_input);

	/* Some additional configuration... */
	if (use_as_default) {
		/* Use the interface as default one. */
		netif_set_default (netif);
	}
	if (use_dhcp) {
		/* Dynamic IP assignment by DHCP. */
		netif_set_up (netif);
		dhcp_start (netif);
	} else {
		/* Static IP assignment. */
		netif_set_up (netif);
	}
}

static void
tcpip_netif_conf (struct ip_main_netif *netif_arg, int netif_num)
{
	struct netif *netif;
	int i;

	/* Allocate network interface. */
	netif = mem_malloc (sizeof *tcpip_context->netif * netif_num);
	LWIP_ASSERT ("netif", netif);
	tcpip_context->netif = netif;
	tcpip_context->netif_num = netif_num;
	tcpip_context->oldip_addr = mem_malloc (sizeof *tcpip_context->
						oldip_addr * netif_num);
	LWIP_ASSERT ("tcpip_context->oldip_addr", tcpip_context->oldip_addr);

	for (i = 0; i < netif_num; i++)
		tcpip_netif_conf_sub (&netif[i],
				      netif_arg[i].ipaddr,
				      netif_arg[i].netmask,
				      netif_arg[i].gateway,
				      netif_arg[i].use_as_default,
				      netif_arg[i].use_dhcp,
				      netif_arg[i].handle,
				      &tcpip_context->oldip_addr[i]);
}

void
ip_main_init (struct ip_main_netif *netif_arg, int netif_num)
{
	/* Initialize TCP/IP Stack. */
	lwip_init ();

	/* Allocate context. */
	tcpip_context = mem_malloc (sizeof *tcpip_context);
	LWIP_ASSERT ("tcpip_context", tcpip_context);
	memset (tcpip_context, 0, sizeof *tcpip_context);

	/* Configure and add network interface. */
	tcpip_netif_conf (netif_arg, netif_num);
#ifdef WIREGUARD_VMM
	net_wg_init ();
#endif
}

void
ip_main_task (void)
{
	/* Check IP address change of network interface. */
	tcpip_netif_ipaddr_check ();

	/* Handle packet reception. */
	tcpip_netif_poll ();

	/* Do timer related tasks. */
	sys_check_timeouts ();
}
