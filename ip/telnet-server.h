#ifndef TELNET_SERVER_H
#define TELNET_SERVER_H

int telnet_server_input (void);
int telnet_server_output (int c);
void telnet_server_init (char *name);

#endif	/* TELNET_SERVER_H */
