/*
 * Copyright (c) 2024 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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
#include <core/arith.h>
#include <core/config.h>
#include <net/netapi.h>
#include "lwip/etharp.h"
#include "lwip/prot/iana.h"
#include "lwip/prot/udp.h"
#include "lwip/prot/dhcp.h"
#include "lwip/prot/ip.h"
#include "net_main_internal.h"
#include "net_main_wg.h"

#define ARP_ASK_GATEWAY 1
#define ARP_ASK_OTHERS	2
#define PACK_IP_ADDR(ip) \
	(((u32)(ip)[3] << 24) | ((u32)(ip)[2] << 16) | ((u32)(ip)[1] << 8) | \
	 (u32)(ip)[0])

struct arp_data {
	struct eth_hdr ethhdr;
	struct etharp_hdr etharphdr;
	u8 padding[18];
};

struct dhcp_data {
	struct eth_hdr ethhdr;
	struct ip_hdr iphdr;
	struct udp_hdr udphdr;
	struct dhcp_msg dhcpmsg;
};

struct wg_gos_data {
	u8 guest_mac[6];
	u8 fake_eth[14];
	ip_addr_t gateway_ip;
};

struct wg_gos_data *
wg_gos_new (u8 guest_mac[6])
{
	/* Note: mem_malloc is not usable here */
	struct wg_gos_data *wg_gos_data;

	wg_gos_data = alloc (sizeof *wg_gos_data);
	memset (wg_gos_data, 0, sizeof *wg_gos_data);
	memcpy (&wg_gos_data->guest_mac, guest_mac, 6);
	memcpy (&(wg_gos_data->fake_eth[6]), config.wg_gos.mac_gateway, 6);
	wg_gos_data->fake_eth[12] = ETHTYPE_IP >> 8;
	wg_gos_data->fake_eth[13] = (u8)ETHTYPE_IP;
	IP4_ADDR (&wg_gos_data->gateway_ip, config.wg.gateway[0],
		  config.wg.gateway[1], config.wg.gateway[2],
		  config.wg.gateway[3]);
	return wg_gos_data;
}

static int
arp_checker (void *packet, int size, struct wg_gos_data *wg_gos_data)
{
	if (size < (SIZEOF_ETH_HDR + SIZEOF_ETHARP_HDR))
		return 0;
	struct etharp_hdr *arp_hdr = packet + SIZEOF_ETH_HDR;
	ip4_addr_t dest_ip;

	SMEMCPY (&dest_ip, &arp_hdr->dipaddr, sizeof dest_ip);
#ifdef DEBUG_ARP_MAC
	unsigned char *source_mac = ((unsigned char *)packet) + 6;
	unsigned char *dest_mac = ((unsigned char *)packet);
	unsigned char *source_ip = ((unsigned char *)packet) + 28;
	printf ("ARP Packet Details:\n");
	printf ("Source MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", source_mac[0],
		source_mac[1], source_mac[2], source_mac[3], source_mac[4],
		source_mac[5]);
	printf ("Destination MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
		dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3],
		dest_mac[4], dest_mac[5]);
	printf ("Source IP: %d.%d.%d.%d\n", source_ip[0], source_ip[1],
		source_ip[2], source_ip[3]);
	printf ("Destination IP: %d.%d.%d.%d\n", dest_ip[0], dest_ip[1],
		dest_ip[2], dest_ip[3]);
#endif
	if (ip_addr_cmp (&dest_ip, &wg_gos_data->gateway_ip))
		return ARP_ASK_GATEWAY;
	else
		return ARP_ASK_OTHERS;
}

static void
arp_maker (void *packet, struct arp_data *arp_packet,
	   struct wg_gos_data *wg_gos_data)
{
	if (!arp_packet)
		return;
	*arp_packet = (struct arp_data) {
		.ethhdr.dest.addr = { wg_gos_data->guest_mac[0],
				      wg_gos_data->guest_mac[1],
				      wg_gos_data->guest_mac[2],
				      wg_gos_data->guest_mac[3],
				      wg_gos_data->guest_mac[4],
				      wg_gos_data->guest_mac[5] },
		.ethhdr.src.addr = { config.wg_gos.mac_gateway[0],
				     config.wg_gos.mac_gateway[1],
				     config.wg_gos.mac_gateway[2],
				     config.wg_gos.mac_gateway[3],
				     config.wg_gos.mac_gateway[4],
				     config.wg_gos.mac_gateway[5] },
		.ethhdr.type = PP_HTONS (ETHTYPE_ARP),
		.etharphdr.hwtype = PP_HTONS (LWIP_IANA_HWTYPE_ETHERNET),
		.etharphdr.proto = NETIF_FLAG_ETHARP,
		.etharphdr.hwlen = ETH_HWADDR_LEN,
		.etharphdr.protolen = 0x04,
		.etharphdr.opcode = PP_HTONS (ARP_REPLY),
		.etharphdr.shwaddr.addr = { config.wg_gos.mac_gateway[0],
					    config.wg_gos.mac_gateway[1],
					    config.wg_gos.mac_gateway[2],
					    config.wg_gos.mac_gateway[3],
					    config.wg_gos.mac_gateway[4],
					    config.wg_gos.mac_gateway[5] },
		.etharphdr.sipaddr.addrw[0] = (u16)config.wg.gateway[1] << 8 |
			config.wg.gateway[0],
		.etharphdr.sipaddr.addrw[1] = (u16)config.wg.gateway[3] << 8 |
			config.wg.gateway[2],
		.etharphdr.dhwaddr.addr = { wg_gos_data->guest_mac[0],
					    wg_gos_data->guest_mac[1],
					    wg_gos_data->guest_mac[2],
					    wg_gos_data->guest_mac[3],
					    wg_gos_data->guest_mac[4],
					    wg_gos_data->guest_mac[5] },
		.etharphdr.dipaddr.addrw[0] = (u16)config.wg.ipaddr[1] << 8 |
			config.wg.ipaddr[0],
		.etharphdr.dipaddr.addrw[1] = (u16)config.wg.ipaddr[3] << 8 |
			config.wg.ipaddr[2],
		.padding = { 0 },
	};
}

static int
dhcp_checker (void *packet, int size, u32 *transaction_id)
{
	struct dhcp_data *d = packet;
	if (d->iphdr._proto == IP_PROTO_UDP &&
	    d->udphdr.src == PP_HTONS (LWIP_IANA_PORT_DHCP_CLIENT) &&
	    d->udphdr.dest == PP_HTONS (LWIP_IANA_PORT_DHCP_SERVER)) {
		*transaction_id = d->dhcpmsg.xid;
		u8 *options = d->dhcpmsg.options;
		u8 *end = packet + size;
		while (options < end) {
			if (*options == DHCP_OPTION_MESSAGE_TYPE) {
				if (*(options + 2) == DHCP_DISCOVER)
					return DHCP_DISCOVER;
				if (*(options + 2) == DHCP_REQUEST)
					return DHCP_REQUEST;
			}
			/* Jump to next option by adding option length + 2 */
			options += *(options + 1) + 2;
		}
	}
	return 0;
}

static void
dhcp_maker (struct dhcp_data *d, u32 x_id, u8 type,
	    struct wg_gos_data *wg_gos_data)
{
	struct ip_hdr *iphdr;

	if (!d)
		return;
	*d = (struct dhcp_data) {
		.ethhdr.src.addr = { wg_gos_data->guest_mac[0],
				     wg_gos_data->guest_mac[1],
				     wg_gos_data->guest_mac[2],
				     wg_gos_data->guest_mac[3],
				     wg_gos_data->guest_mac[4],
				     wg_gos_data->guest_mac[5] },
		/* Fake MAC */
		.ethhdr.dest.addr = { config.wg_gos.mac_gateway[0],
				      config.wg_gos.mac_gateway[1],
				      config.wg_gos.mac_gateway[2],
				      config.wg_gos.mac_gateway[3],
				      config.wg_gos.mac_gateway[4],
				      config.wg_gos.mac_gateway[5] },
		.ethhdr.type = PP_HTONS (ETHTYPE_IP),
		/* IP Header */
		.iphdr._v_hl = 0x45,
		.iphdr._tos = 0x10,
		.iphdr._len = 0x2e01,
		.iphdr._id = 0x0500,
		.iphdr._offset = 0,
		.iphdr._ttl = 0x40,
		.iphdr._proto = IP_PROTO_UDP,
		.iphdr._chksum = 0,
		.iphdr.src.addr = PACK_IP_ADDR (config.wg.gateway),
		.iphdr.dest.addr = IPADDR_BROADCAST,
		/* UDP protocol */
		.udphdr.src = PP_HTONS (LWIP_IANA_PORT_DHCP_SERVER),
		.udphdr.dest = PP_HTONS (LWIP_IANA_PORT_DHCP_CLIENT),
		.udphdr.len = 0x1a01,
		.udphdr.chksum = 0,
		/* DHCP Message */
		.dhcpmsg.op = DHCP_BOOTREPLY,
		.dhcpmsg.htype = LWIP_IANA_HWTYPE_ETHERNET,
		.dhcpmsg.hlen = ETH_HWADDR_LEN,
		.dhcpmsg.hops = 0,
		.dhcpmsg.xid = 0,
		.dhcpmsg.secs = 0,
		.dhcpmsg.flags = 0,
		.dhcpmsg.ciaddr.addr = 0,
		.dhcpmsg.yiaddr.addr = PACK_IP_ADDR (config.wg_gos.ipaddr),
		.dhcpmsg.siaddr.addr = PACK_IP_ADDR (config.wg.gateway),
		.dhcpmsg.giaddr.addr = 0,
		.dhcpmsg.chaddr = { wg_gos_data->guest_mac[0],
				    wg_gos_data->guest_mac[1],
				    wg_gos_data->guest_mac[2],
				    wg_gos_data->guest_mac[3],
				    wg_gos_data->guest_mac[4],
				    wg_gos_data->guest_mac[5] },
		.dhcpmsg.sname = { 0 },
		.dhcpmsg.file = { 0 },
		.dhcpmsg.cookie = PP_HTONL (DHCP_MAGIC_COOKIE),
		.dhcpmsg.options = { DHCP_OPTION_MESSAGE_TYPE,
				     1,
				     type,
				     DHCP_OPTION_SERVER_ID,
				     4,
				     config.wg.gateway[0],
				     config.wg.gateway[1],
				     config.wg.gateway[2],
				     config.wg.gateway[3],
				     DHCP_OPTION_SUBNET_MASK,
				     4,
				     config.wg.netmask[0],
				     config.wg.netmask[1],
				     config.wg.netmask[2],
				     config.wg.netmask[3],
				     DHCP_OPTION_ROUTER,
				     4,
				     config.wg.gateway[0],
				     config.wg.gateway[1],
				     config.wg.gateway[2],
				     config.wg.gateway[3],
				     DHCP_OPTION_DNS_SERVER,
				     4,
				     config.wg_gos.dns[0],
				     config.wg_gos.dns[1],
				     config.wg_gos.dns[2],
				     config.wg_gos.dns[3],
				     DHCP_OPTION_LEASE_TIME,
				     4,
				     0x00,
				     0x01,
				     0x51,
				     0x80,
				     DHCP_OPTION_END }
	};
	d->dhcpmsg.xid = x_id;
	iphdr = (struct ip_hdr *)((u8 *)d + SIZEOF_ETH_HDR);
	iphdr->_chksum = ipchecksum ((u8 *)iphdr, 20);
}

static void
send_to_wg (void *packet, int size)
{
	struct pbuf *q;
	ip4_addr_t ipaddr;
	struct netif *netif;

	netif = netif_find ("wg1");
	if (netif == NULL) {
		printf ("wg1 network interface not found\n");
		return;
	}

	q = pbuf_alloc (PBUF_RAW, size - SIZEOF_ETH_HDR, PBUF_POOL);
	pbuf_take (q, packet + SIZEOF_ETH_HDR, size - SIZEOF_ETH_HDR);
	ipaddr.addr = 0;
	netif->output (netif, q, &ipaddr);
	pbuf_free (q);
}

static int
send_to_vm_guest (struct pbuf *pbuf)
{
	struct net_ip_data *p;
	unsigned int total_size;
	void *buffer;
	struct netif *netif;

	netif = netif_find ("vm0");
	if (netif == NULL) {
		printf ("vm0 network interface not found and retain pbuf\n");
		return 0;
	}
	p = netif->state;
	total_size = pbuf->len + sizeof p->wg_gos_data->fake_eth;
	buffer = mem_calloc (1, total_size);
	if (buffer == NULL) {
		panic ("No memory to alloc");
	}
	memcpy (buffer, p->wg_gos_data->fake_eth,
		sizeof p->wg_gos_data->fake_eth);
	memcpy (buffer, p->wg_gos_data->guest_mac, ETH_HWADDR_LEN);
	memcpy (buffer + sizeof p->wg_gos_data->fake_eth, pbuf->payload,
		pbuf->len);
	net_main_send_virt (p, 1, &buffer, &total_size, true);
	mem_free (buffer);
	pbuf_free (pbuf);
	return total_size;
}

int
wg_ip4_input_hook (struct pbuf *p, struct netif *inp)
{
	if (inp->name[0] == 'w' && inp->name[1] == 'g') {
		if (p != NULL) {
			ip_addr_t dest_ip;
			ip_addr_t wg_gos_ip;
			struct ip_hdr *ip_header = p->payload;

			ip4_addr_set_u32 (&dest_ip,
					  ip4_addr_get_u32 (&ip_header->dest));
			IP_ADDR4 (&wg_gos_ip, config.wg_gos.ipaddr[0],
				  config.wg_gos.ipaddr[1],
				  config.wg_gos.ipaddr[2],
				  config.wg_gos.ipaddr[3]);
			if (ip_addr_cmp (&dest_ip, &wg_gos_ip))
				return send_to_vm_guest (p);
		} else {
			printf ("wg_ip4_input_hook : pbuf is NULL\n");
		}
	}
	return 0;
}

static void
reply_arp (struct eth_hdr *ethhdr, void *packet,
	   struct wg_gos_data *wg_gos_data, int size,
	   struct net_ip_data *handle)
{
	u8 arp_type = arp_checker (packet, size, wg_gos_data);

	if (arp_type == ARP_ASK_GATEWAY) {
		struct arp_data arp_response;
		unsigned int arp_size = sizeof arp_response;
		void *pointer_arp = &arp_response;

		arp_maker (packet, &arp_response, wg_gos_data);
		net_main_send_virt (handle, 1, &pointer_arp, &arp_size, true);
	}
}

static bool
reply_dhcp (void *packet, struct wg_gos_data *wg_gos_data, int size,
	    struct net_ip_data *handle)
{
	u32 x_id;
	int dhcp_type = dhcp_checker (packet, size, &x_id);
	struct dhcp_data dhcp_response;
	if (dhcp_type == DHCP_DISCOVER)
		dhcp_maker (&dhcp_response, x_id, DHCP_OFFER, wg_gos_data);
	else if (dhcp_type == DHCP_REQUEST)
		dhcp_maker (&dhcp_response, x_id, DHCP_ACK, wg_gos_data);
	else
		return false;
	unsigned int dhcp_size = sizeof dhcp_response;
	void *pointer_dhcp = &dhcp_response;
	net_main_send_virt (handle, 1, &pointer_dhcp, &dhcp_size, true);
	return true;
}

void
wg_gos_routing (u32 num_packets, void **packets, u32 *packet_sizes,
		void *param)
{
	struct net_ip_data *p = param;
	int size;
	void *packet;

	for (u32 i = 0; i < num_packets; i++) {
		packet = packets[i];
		size = packet_sizes[i];
		struct eth_hdr *ethhdr = packet;
		if (size < sizeof *ethhdr)
			continue;
		switch (PP_HTONS (ethhdr->type)) {
		case ETHTYPE_ARP:
			reply_arp (ethhdr, packet, p->wg_gos_data, size, p);
			break;
		case ETHTYPE_IP:
			if (!reply_dhcp (packet, p->wg_gos_data, size, p))
				send_to_wg (packet, size);
			break;
		default:
			break;
		}
	}
}
