#include <ip.h>
#include "wireguard-setup.h"
#include "wireguard-lwip/src/wireguard-platform.h"
#include "wireguard-lwip/src/wireguardif.h"

struct wireguard_setup_data {
	int wg_listen_port;
	int peer_endpoint_port;
	ip_addr_t ipaddr;
	ip_addr_t netmask;
	ip_addr_t gateway;
	ip_addr_t ipaddr_end_point;
	ip_addr_t peer_allowed_ip;
	ip_addr_t peer_allowed_mask;
	char *wg_private_key;
	char *peer_public_key;
};

static void
parse_wireguard_config (struct wireguard_setup_data *wg_config,
			struct wireguard_setup_arg *arg)
{
	unsigned char *ip_format_temp;

	/* Get get config value and store to the local variables */
	ip_format_temp = arg->ipaddr;
	IP4_ADDR (&wg_config->ipaddr, ip_format_temp[0], ip_format_temp[1],
		  ip_format_temp[2], ip_format_temp[3]);
	ip_format_temp = arg->netmask;
	IP4_ADDR (&wg_config->netmask, ip_format_temp[0], ip_format_temp[1],
		  ip_format_temp[2], ip_format_temp[3]);
	ip_format_temp = arg->gateway;
	IP4_ADDR (&wg_config->gateway, ip_format_temp[0], ip_format_temp[1],
		  ip_format_temp[2], ip_format_temp[3]);
	ip_format_temp = arg->ipaddr_end_point;
	IP4_ADDR (&wg_config->ipaddr_end_point, ip_format_temp[0],
		  ip_format_temp[1], ip_format_temp[2], ip_format_temp[3]);
	ip_format_temp = arg->peer_allowed_ip;
	IP4_ADDR (&wg_config->peer_allowed_ip, ip_format_temp[0],
		  ip_format_temp[1], ip_format_temp[2], ip_format_temp[3]);
	ip_format_temp = arg->peer_allowed_mask;
	IP4_ADDR (&wg_config->peer_allowed_mask, ip_format_temp[0],
		  ip_format_temp[1], ip_format_temp[2], ip_format_temp[3]);
	wg_config->peer_endpoint_port = arg->peer_endpoint_port;
	wg_config->wg_listen_port = arg->wg_listen_port;
	wg_config->wg_private_key = arg->wg_private_key;
	wg_config->peer_public_key = arg->peer_public_key;
}

struct netif *
wireguard_setup (struct wireguard_setup_arg *arg)
{
	struct wireguardif_init_data wg;
	struct wireguardif_peer peer;
	struct wireguard_setup_data wg_config;
	char lwip_netif_name[] = "vm0";

	/* Setup the WireGuard device structure */
	parse_wireguard_config (&wg_config, arg);
	wg.private_key = wg_config.wg_private_key;
	wg.listen_port = wg_config.wg_listen_port;
	if (!wg.listen_port) {
		uint16_t port_delta;
		wireguard_random_bytes (&port_delta, sizeof port_delta);
		/* Assign the random port number from 0xC000~0xFFFF */
		wg.listen_port = 0xC000 + (port_delta) % 0x3FFE;
	}
	wg.bind_netif = netif_find (lwip_netif_name);
	if (wg.bind_netif == NULL)
		return NULL;
	/* Register the new WireGuard network interface with lwIP */
	struct netif *netif_new = mem_calloc (1, sizeof *netif_new);
	if (netif_new == NULL)
		return NULL;
	struct netif *wg_netif = netif_add (netif_new, &wg_config.ipaddr,
					    &wg_config.netmask,
					    &wg_config.gateway, &wg,
					    &wireguardif_init, &ip_input);
	if (!wg_netif) {
		mem_free (netif_new);
		return NULL;
	}
	/* Mark the interface as administratively up, link up flag is set */
	/* automatically when peer connects */
	netif_set_up (wg_netif);
	/* Initialise the first WireGuard peer structure */
	wireguardif_peer_init (&peer);
	peer.public_key = wg_config.peer_public_key;
	peer.preshared_key = NULL;
	/* Allow all IPs through tunnel */
	peer.allowed_ip = wg_config.peer_allowed_ip;
	peer.allowed_mask = wg_config.peer_allowed_mask;
	/* If we know the endpoint's address can add here */
	peer.endpoint_ip = wg_config.ipaddr_end_point;
	peer.endport_port = wg_config.peer_endpoint_port;
	/* Register the new WireGuard peer with the netwok interface */
	uint8_t peer_index = WIREGUARDIF_INVALID_INDEX;
	err_t ret = wireguardif_add_peer (wg_netif, &peer, &peer_index);

	if (peer_index != WIREGUARDIF_INVALID_INDEX &&
	    !ip_addr_isany (&peer.endpoint_ip)) {
		/* Start outbound connection to peer */
		ret = wireguardif_connect (wg_netif, peer_index);
	}
	if (ret == ERR_OK)
		return wg_netif;
	netif_remove (wg_netif);
	mem_free (netif_new);
	return NULL;
}
