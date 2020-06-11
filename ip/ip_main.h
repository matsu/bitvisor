enum ip_main_destination {
	IP_MAIN_DESTINATION_OTHERS,
	IP_MAIN_DESTINATION_ALL,
	IP_MAIN_DESTINATION_ME,
};

struct ip_main_netif {
	void *handle;
	int use_as_default;
	int use_dhcp;
	unsigned char *ipaddr;
	unsigned char *netmask;
	unsigned char *gateway;
};

enum ip_main_destination ip_main_input_test (void *arg, void *buf,
					     unsigned int len);
enum ip_main_destination ip_main_input_test_destination (void *arg, void *buf,
							 unsigned int len);
void ip_main_input (void *arg, void *buf, unsigned int len,
		    void (*free) (void *free_arg), void *free_arg);
void ip_main_init (struct ip_main_netif *netif_arg, int netif_num);
void ip_main_task (void);
