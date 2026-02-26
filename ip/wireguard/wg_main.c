#include <core/config.h>
#include "wg_main.h"
#include "wireguard-setup.h"

struct netif *
net_wg_init (struct config_data_wireguard *config_wg)
{
	struct wireguard_setup_arg arg = {
		.ipaddr = config_wg->ipaddr,
		.netmask = config_wg->netmask,
		.gateway = config_wg->gateway,
		.ipaddr_end_point = config_wg->ipaddr_end_point,
		.peer_allowed_ip = config_wg->peer_allowed_ip,
		.peer_allowed_mask = config_wg->peer_allowed_mask,
		.wg_private_key = config_wg->wg_private_key,
		.peer_public_key = config_wg->peer_public_key,
		.wg_listen_port = config_wg->wg_listen_port,
		.peer_endpoint_port = config_wg->peer_endpoint_port,
	};
	if (arg.wg_private_key[0] != '\0')
		return wireguard_setup (&arg);
	return NULL;
}
