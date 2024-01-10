#include <core/types.h>
#include <net/netapi.h>

struct net_ip_data {
	int pass;
	void *phys_handle, *virt_handle;
	struct nicfunc *phys_func, *virt_func;
	bool input_ok;
	u8 filter_count;
	void *input_arg;
	u8 wg_gos;
	struct wg_gos_data *wg_gos_data;
};

void net_main_send_virt (struct net_ip_data *handle, unsigned int num_packets,
			 void **packets, unsigned int *packet_sizes,
			 bool print_ok);
