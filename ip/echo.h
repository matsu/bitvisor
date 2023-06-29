#ifndef ECHO_H
#define ECHO_H

void echo_server_init (int port, char *netif_name);
int echo_client_send (void);
void echo_client_init (int *ipaddr, int port, char *netif_name);

#endif	/* ECHO_H */
