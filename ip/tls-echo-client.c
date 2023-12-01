/* The implementation is based on echo-client.c" */

#include <mbedtls_helpers.h>
#include "lwip/altcp_tls.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "tls-echo.h"

static ip_addr_t destip;
static int destport;
static struct altcp_pcb *tls_echo_client_pcb;

#if 0				/* debug? */
#define printd(X...) do { printf (X); } while (0)
#else
#define printd(X...)
#endif

#define SEND_BUFSIZE 1446
#define TCP_SND_BUFFER 8192
static char *send_buf = "Hello, BitVisor with TLS!\n";

int
tls_echo_client_send (void)
{
	err_t err;
	struct altcp_pcb *pcb = tls_echo_client_pcb;
	int buflen;

	if (!tls_echo_client_pcb) {
		printd ("No connection.\n");
		return -1;
	} else {
		printd ("Connection found.\n");
	}

	buflen = altcp_sndbuf (pcb);
	if (buflen >= strlen (send_buf)) {
		printd ("Space available: %d\n", buflen);
		err = altcp_write (pcb, send_buf, strlen (send_buf),
				   TCP_WRITE_FLAG_COPY);
		if (err == ERR_OK) {
			printd ("Enqueue succeeded.\n");
			err = altcp_output (pcb);
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
tls_echo_client_sent (void *arg, struct altcp_pcb *tpcb, u16_t len)
{
	printd ("Sent.\n");
	return ERR_OK;
}

static void
tls_echo_client_error (void *arg, err_t err)
{
	printd ("Error: %d\n", err);
}

static err_t
tls_echo_client_recv (void *arg, struct altcp_pcb *pcb, struct pbuf *p,
		      err_t err)
{
	int i;
	char *str;

	printd ("Received.\n");
	if (!p) {
		/* Disconnected by remote. */
		printd ("Disconnected!\n");
		altcp_close (pcb);
		tls_echo_client_pcb = NULL;
		return ERR_OK;
	} else if (err != ERR_OK) {
		/* Error occurred. */
		printd ("Error: %d\n", err);
		return err;
	} else {
		/* Really received. */
		altcp_recved (pcb, p->len);
		str = p->payload;
		for (i = 0; i < p->len; i++)
			printf ("%c", str[i]);
		pbuf_free (p);
		return ERR_OK;
	}
}

static err_t
tls_echo_client_connected (void *arg, struct altcp_pcb *pcb, err_t err)
{
	if (err == ERR_OK) {
		printf ("Connection established!\n");
		tls_echo_client_pcb = pcb;
		altcp_arg (pcb, NULL);
		altcp_sent (pcb, tls_echo_client_sent);
		altcp_recv (pcb, tls_echo_client_recv);
		altcp_err (pcb, tls_echo_client_error);
	} else {
		printf ("Connection missed!\n");
	}
	return ERR_OK;
}

void
tls_echo_client_init (int *ipaddr, int port)
{
	u8 ca_cert_der[4096];
	struct altcp_tls_config *conf;
	size_t ca_cert_der_len = sizeof ca_cert_der;
	struct altcp_pcb *pcb;
	err_t e;

	IP4_ADDR (&destip,
		  ipaddr[0],
		  ipaddr[1],
		  ipaddr[2],
		  ipaddr[3]);
	destport = port;

	printd ("New Connection.\n");
	if (convert_pem_to_der ((u8 *)ECHO_TLS_CA_CRT,
				strlen (ECHO_TLS_CA_CRT), ca_cert_der,
				&ca_cert_der_len));

	conf = altcp_tls_create_config_client ((u8 *)ca_cert_der,
					       ca_cert_der_len);
	if (conf == NULL) {
		printf ("Invalid TLS config.\n");
		return;
	}
	altcp_allocator_t tls_allocator = { altcp_tls_alloc, conf };
	pcb = altcp_new (&tls_allocator);
	if (pcb) {
		e = altcp_connect (pcb, &destip, destport,
				   tls_echo_client_connected);
		if (e == ERR_OK) {
			printf ("Connecting...\n");
		} else {
			printf ("Connect failed!\n");
		}
	} else {
		printf ("New context failed.\n");
	}
}
