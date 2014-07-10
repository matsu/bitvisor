#include <core/mm.h>
#include <core/process.h>
#include <core/initfunc.h>
#include "echo.h"
#include "tcpip.h"

enum echo_command {
	ECHO_CMD_CLIENT_CONNECT = 0,
	ECHO_CMD_CLIENT_SEND = 1,
	ECHO_CMD_SERVER_START = 2,
};

struct arg {
	int ipaddr_a[4];
	int port;
};

static void
echoctl_echo_client_connect (void *arg)
{
	struct arg *a = arg;

	echo_client_init (a->ipaddr_a, a->port);
	free (a);
}

static void
echoctl_echo_client_send (void *arg)
{
	echo_client_send ();
}

static void
echoctl_echo_server_start (void *arg)
{
	struct arg *a = arg;

	echo_server_init (a->port);
	free (a);
}

static int
echoctl_sub (unsigned long (*array)[3], int len)
{
	int ret;
	ulong cmd;
	ulong ipaddr, port;
	struct arg *a;

	if (len != sizeof *array)
		return -1;
	cmd = (*array)[0];
	ipaddr = (*array)[1];
	port = (*array)[2];
	switch (cmd) {
	case ECHO_CMD_CLIENT_CONNECT:
		/* Connect to echo server. */
		a = alloc (sizeof *a);
		if (a) {
			a->ipaddr_a[0] = (ipaddr >> 24) & 0xff;
			a->ipaddr_a[1] = (ipaddr >> 16) & 0xff;
			a->ipaddr_a[2] = (ipaddr >>  8) & 0xff;
			a->ipaddr_a[3] = (ipaddr >>  0) & 0xff;
			a->port = (int)port;

			tcpip_begin (echoctl_echo_client_connect, a);
			ret = 0;
		} else {
			ret = -1;
		}
		break;
	case ECHO_CMD_CLIENT_SEND:
		/* Send a message to echo server. */
		tcpip_begin (echoctl_echo_client_send, NULL);
		ret = 0;
		break;
	case ECHO_CMD_SERVER_START:
		/* Start echo server. */
		a = alloc (sizeof *a);
		if (a) {
			a->port = (int)port;
			tcpip_begin (echoctl_echo_server_start, a);
			ret = 0;
		} else {
			ret = -1;
		}
		break;
	default:
		ret = -1;
	}
	return ret;
}

static int
echoctl_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (bufcnt != 1)
		return -1;
	return echoctl_sub (buf[0].base, buf[0].len);
}

static void
echoctl_init_msg (void)
{
	msgregister ("echoctl", echoctl_msghandler);
}

INITFUNC ("msg0", echoctl_init_msg);
