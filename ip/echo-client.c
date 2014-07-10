#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"

static ip_addr_t destip;
static int destport;
static struct tcp_pcb *echo_client_pcb;

#if 0				/* debug? */
#define printd(X...) do { printf (X); } while (0)
#else
#define printd(X...)
#endif

#define SEND_BUFSIZE 1446
#define TCP_SND_BUFFER 8192
static char *send_buf = "Hello, BitVisor!\n";

int
echo_client_send (void)
{
	err_t err;
	struct tcp_pcb *pcb = echo_client_pcb;
	int buflen;

	if (!echo_client_pcb) {
		printd ("No connection.\n");
		return -1;
	} else {
		printd ("Connection found.\n");
	}

	buflen = tcp_sndbuf (pcb);
	if (buflen >= strlen (send_buf)) {
		printd ("Space available: %d\n", buflen);
		err = tcp_write (pcb, send_buf, strlen (send_buf),
				 TCP_WRITE_FLAG_COPY);
		if (err == ERR_OK) {
			printd ("Enqueue succeeded.\n");
			err = tcp_output (pcb);
			if (err == ERR_OK) {
				printd ("Send succeeded.\n");
			} else {
				printd ("Send failed.\n");
				return -2;
			}
		} else {
			printd ("Enqueue failed.\n");
			return -3;
		}
	} else {
		printd ("Space unavailable: %d\n", buflen);
		return -4;
	}
	return 0;
}

static err_t
echo_client_sent (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	printd ("Sent.\n");
	return ERR_OK;
}

static void
echo_client_error (void *arg, err_t err)
{
	printd ("Error: %d\n", err);
}

static err_t
echo_client_recv (void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	int i;
	char *str;

	printd ("Received.\n");
	if (!p) {
		/* Disconnected by remote. */
		printd ("Disconnected!\n");
		echo_client_pcb = NULL;
		return ERR_OK;
	} else if (err != ERR_OK) {
		/* Error occurred. */
		printd ("Error: %d\n", err);
		return err;
	} else {
		/* Really received. */
		tcp_recved (pcb, p->len);
		str = p->payload;
		for (i = 0; i < p->len; i++)
			printf ("%c", str[i]);
		return ERR_OK;
	}
}

static err_t
echo_client_connected (void *arg, struct tcp_pcb *pcb, err_t err)
{
	if (err == ERR_OK) {
		printf ("Connection established!\n");
		tcp_arg (pcb, NULL);
		tcp_sent (pcb, echo_client_sent);
		tcp_recv (pcb, echo_client_recv);
		tcp_err (pcb, echo_client_error);
	} else {
		printf ("Connection missed!\n");
	}
	return ERR_OK;
}

void
echo_client_init (int *ipaddr, int port)
{
	struct tcp_pcb *pcb;
	err_t e;

	IP4_ADDR (&destip,
		  ipaddr[0],
		  ipaddr[1],
		  ipaddr[2],
		  ipaddr[3]);
	destport = port;

	printd ("New Connection.\n");
	pcb = tcp_new ();
	if (pcb) {
		e = tcp_connect (pcb, &destip, destport,
				 echo_client_connected);
		if (e == ERR_OK) {
			printf ("Connecting...\n");
			echo_client_pcb = pcb;
		} else {
			printf ("Connect failed!\n");
		}
	} else {
		printf ("New context failed.\n");
	}
}
