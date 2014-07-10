struct ip_main_netif {
	void *handle;
	int use_as_default;
	int use_dhcp;
	unsigned char *ipaddr;
	unsigned char *netmask;
	unsigned char *gateway;
};

void ip_main_input (void *arg, void *buf, unsigned int len);
void ip_main_init (struct ip_main_netif *netif_arg, int netif_num);
void ip_main_task (void);
