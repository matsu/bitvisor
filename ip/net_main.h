void net_main_send (void *handle, void *buf, unsigned int len);
void net_main_get_mac_address (void *handle, unsigned char *mac_address);
void net_main_set_recv_arg (void *handle, void *arg);
void net_main_poll (void *handle);
void net_main_task_add (void (*func) (void *arg), void *arg);
