#ifndef _WIREGUARD_SETUP_H_
#define _WIREGUARD_SETUP_H_

struct wireguard_setup_arg {
	unsigned char *ipaddr;
	unsigned char *netmask;
	unsigned char *gateway;
	unsigned char *ipaddr_end_point;
	unsigned char *peer_allowed_ip;
	unsigned char *peer_allowed_mask;
	char *wg_private_key;
	char *peer_public_key;
	int wg_listen_port;
	int peer_endpoint_port;
};

int wireguard_setup (struct wireguard_setup_arg *arg);

#endif
