#include <core/config.h>
#include "wg_main.h"
#include "wireguard-setup.h"

void
net_wg_init (void)
{
	struct wireguard_setup_arg arg = {
		.ipaddr = config.wg.ipaddr,
		.netmask = config.wg.netmask,
		.gateway = config.wg.gateway,
		.ipaddr_end_point = config.wg.ipaddr_end_point,
		.peer_allowed_ip = config.wg.peer_allowed_ip,
		.peer_allowed_mask = config.wg.peer_allowed_mask,
		.wg_private_key = config.wg.wg_private_key,
		.peer_public_key = config.wg.peer_public_key,
		.wg_listen_port = config.wg.wg_listen_port,
		.peer_endpoint_port = config.wg.peer_endpoint_port,
	};
	wireguard_setup (&arg);
}
