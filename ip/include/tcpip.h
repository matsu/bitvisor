#ifndef __TCPIP_H
#define __TCPIP_H

typedef void tcpip_task_fn_t (void *arg);
void tcpip_begin (tcpip_task_fn_t *func, void *arg);

#endif	/* __TCPIP_H */
