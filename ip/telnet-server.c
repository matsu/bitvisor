/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2014 Igel Co., Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of and a contribution to the lwIP TCP/IP stack.
 *
 * Credits go to Adam Dunkels (and the current maintainers) of this software.
 *
 * Christiaan Simons rewrote this file to get a more stable echo example.
 */

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "tcpip.h"
#include "telnet-server.h"

#define TELNET_PORT 23
#define TELNET_BUFSIZE (1 << 11)

struct telnet_input_data {
	struct telnet_input_data *next;
	struct pbuf *p;
	struct pbuf *pp;
	struct tcp_pcb *tpcb;
};

static struct tcp_pcb *client_tpcb;
static unsigned char telnet_outbuf[TELNET_BUFSIZE];
static unsigned int telnet_outbuf_in;
static unsigned int telnet_outbuf_out;
static unsigned int telnet_outbuf_sent;
static struct tcp_pcb *telnet_outbuf_close_tpcb;
static unsigned int telnet_outbuf_close_out;
static struct telnet_input_data *input_list;
static unsigned char *telnet_inbuf;
static unsigned int telnet_inbuf_len;
static struct telnet_input_data *telnet_inbuf_data;
static unsigned int telnet_iac_count;
static unsigned char telnet_command;
static int sent_will_echo;
static int sent_do_suppress_go_ahead;

static void
set_next_input (struct telnet_input_data *q)
{
	telnet_inbuf = q->pp->payload;
	telnet_inbuf_data = q;
	asm volatile ("sfence" : : : "memory");
	telnet_inbuf_len = q->pp->len;
}

static void
input_done (void *arg)
{
	struct telnet_input_data *q = arg;

	q->pp = q->pp->next;
	if (q->pp) {
		set_next_input (q);
	} else {
		if (client_tpcb == q->tpcb)
			tcp_recved (q->tpcb, q->p->tot_len);
		pbuf_free (q->p);
		struct telnet_input_data *qq = q->next;
		mem_free (q);
		if (qq)
			set_next_input (qq);
		else
			input_list = NULL;
	}
}

static void
telnet_input_done (struct telnet_input_data *q)
{
	tcpip_begin (input_done, q);
}

static void
ack_option (void *arg)
{
	unsigned char *argbase = NULL;
	unsigned char *argp = arg;
	unsigned int argoffset = argp - argbase;
	unsigned char command = argoffset >> 8;
	unsigned char option = argoffset;
	unsigned char buf[3];
	if (!client_tpcb)
		return;
	buf[0] = 0xFF;		/* IAC */
	buf[1] = command;
	buf[2] = option;
	if (tcp_write (client_tpcb, buf, 3, TCP_WRITE_FLAG_COPY) == ERR_OK)
		telnet_outbuf_sent -= 3;
	/* FIXME: Need to handle errors? */
}

static void
telnet_ack_option (unsigned char command, unsigned char option)
{
	unsigned char *argbase = NULL;
	unsigned int argoffset = (unsigned int)command << 8 | option;
	tcpip_begin (ack_option, argbase + argoffset);
}

static int
do_input (void)
{
	if (!telnet_inbuf_len)
		return -1;
	telnet_inbuf_len--;
	int ret = *telnet_inbuf++;
	if (!telnet_inbuf_len)
		telnet_input_done (telnet_inbuf_data);
	return ret;
}

int
telnet_server_input (void)
{
	int c;

	c = do_input ();
	if (c < 0)
		return c;
	if (!telnet_iac_count && c == 0xFF) { /* IAC */
		telnet_iac_count++;
		return -1;
	}
	if (telnet_iac_count == 1) { /* Command */
		switch (c) {
		case 240:	/* SE */
		case 241:	/* NOP */
		case 242:	/* Data Mark */
		case 243:	/* Break */
		case 244:	/* Interrupt Process */
		case 245:	/* Abort Output */
		case 246:	/* Are You There */
		case 247:	/* Erase Character */
		case 248:	/* Erase Line */
		case 249:	/* Go Ahead */
		case 250:	/* SB */
			telnet_iac_count = 0;
			return -1;
		case 251:	/* WILL */
		case 252:	/* WON'T */
		case 253:	/* DO */
		case 254:	/* DON'T */
			telnet_command = c;
			telnet_iac_count++;
			return -1;
		case 255:	/* IAC */
			return -1;
		default:	/* Unknown */
			telnet_iac_count = 0;
			return c;
		}
	}
	if (telnet_iac_count == 2) { /* Option code */
		switch (telnet_command) {
		case 251:	/* WILL */
			if (c == 0x03) { /* SUPPRESS-GO-AHEAD */
				if (sent_do_suppress_go_ahead)
					sent_do_suppress_go_ahead = 0;
				else
					telnet_ack_option (253, c); /* DO */
			} else {
				telnet_ack_option (254, c); /* DON'T */
			}
			break;
		case 252:	/* WON'T */
			break;
		case 253:	/* DO */
			if (c == 0x01) { /* ECHO */
				if (sent_will_echo)
					sent_will_echo = 0;
				else
					telnet_ack_option (251, c); /* WILL */
			} else if (c == 0x03) { /* SUPPRESS-GO-AHEAD */
				telnet_ack_option (251, c); /* WILL */
			} else {
				telnet_ack_option (252, c); /* WON'T */
			}
			break;
		case 254:	/* DON'T */
			break;
		}
		telnet_iac_count = 0;
		return -1;
	}
	return c;
}

static void
telnet_close (struct tcp_pcb *tpcb)
{
	if (client_tpcb == tpcb)
		client_tpcb = NULL;
	tcp_sent (tpcb, NULL);
	tcp_recv (tpcb, NULL);
	tcp_close (tpcb);
}

static err_t
telnet_recv (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	if (!p || p->tot_len <= 0) {
		telnet_close (tpcb);
		return ERR_OK;
	}
	if (err != ERR_OK) {
		if (p != NULL)
			pbuf_free (p);
		return err;
	}
	if (client_tpcb != tpcb) {
		tcp_recved (tpcb, p->tot_len);
		pbuf_free (p);
		return ERR_OK;
	}
	struct telnet_input_data *q = mem_malloc (sizeof *q);
	if (!q) {
		tcp_recved (tpcb, p->tot_len);
		pbuf_free (p);
		return ERR_MEM;
	}
	q->next = NULL;
	q->p = p;
	q->pp = p;
	q->tpcb = tpcb;
	if (input_list) {
		input_list->next = q;
		input_list = q;
	} else {
		input_list = q;
		set_next_input (q);
	}
	return ERR_OK;
}

static void
telnet_send (void *arg)
{
	int output = 0;
	if (!client_tpcb || telnet_outbuf_out == telnet_outbuf_in)
		return;
	for (;;) {
		unsigned int len = TELNET_BUFSIZE -
			telnet_outbuf_in % TELNET_BUFSIZE;
		if (len > telnet_outbuf_out - telnet_outbuf_in)
			len = telnet_outbuf_out - telnet_outbuf_in;
		if (telnet_outbuf_close_tpcb == client_tpcb &&
		    len > telnet_outbuf_close_out - telnet_outbuf_in)
			len = telnet_outbuf_close_out - telnet_outbuf_in;
		int buflen = tcp_sndbuf (client_tpcb);
		if (buflen < 0)
			buflen = 0;
		if (len > buflen)
			len = buflen;
		if (!len)
			break;
		err_t err = tcp_write (client_tpcb,
				       &telnet_outbuf[telnet_outbuf_in %
						      TELNET_BUFSIZE],
				       len, 0);
		if (err != ERR_OK)
			break;
		telnet_outbuf_in += len;
		output = 1;
	}
	if (output)
		tcp_output (client_tpcb);
}

static err_t
telnet_sent (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	telnet_outbuf_sent += len;
	if (telnet_outbuf_close_tpcb == client_tpcb &&
	    telnet_outbuf_close_out == telnet_outbuf_sent)
		telnet_close (client_tpcb);
	else
		telnet_send (NULL);
	return ERR_OK;
}

static err_t
telnet_poll (void *arg, struct tcp_pcb *tpcb)
{
	telnet_send (NULL);
	return ERR_OK;
}

int
telnet_server_output (int c)
{
	if (telnet_outbuf_out - telnet_outbuf_sent >= TELNET_BUFSIZE)
		return -1;
	if (c < 0) {
		telnet_outbuf_close_tpcb = client_tpcb;
		telnet_outbuf_close_out = telnet_outbuf_out + 1;
		c = 0;		/* NUL */
	}
	if (c >= 0 && c <= 127 &&
	    telnet_outbuf_out - telnet_outbuf_sent < TELNET_BUFSIZE) {
		telnet_outbuf[telnet_outbuf_out++ % TELNET_BUFSIZE] = c;
		tcpip_begin (telnet_send, NULL);
	}
	return 0;
}

static void
telnet_err (void *arg, err_t err)
{
	struct tcp_pcb *tpcb = arg;

	if (client_tpcb == tpcb)
		client_tpcb = NULL;
}

static err_t
telnet_accept (void *arg, struct tcp_pcb *newpcb, err_t err)
{
	tcp_arg (newpcb, newpcb);
	tcp_recv (newpcb, telnet_recv);
	tcp_sent (newpcb, telnet_sent);
	tcp_poll (newpcb, telnet_poll, 0);
	tcp_err (newpcb, telnet_err);

	/* Send IAC WILL ECHO IAC DO SUPPRESS-GO-AHEAD */
	err = tcp_write (newpcb, "\xFF\xFB\x01\xFF\xFD\x03", 6,
			 TCP_WRITE_FLAG_COPY);
	if (err != ERR_OK) {
		telnet_close (newpcb);
		return err;
	}
	sent_will_echo = 1;
	sent_do_suppress_go_ahead = 1;

	/* Clear input buffers and close a previous connection */
	while (telnet_inbuf_len) {
		telnet_inbuf_len = 0;
		input_done (telnet_inbuf_data);
	}
	telnet_iac_count = 0;
	if (client_tpcb)
		telnet_close (client_tpcb);
	telnet_outbuf_close_tpcb = NULL;
	telnet_outbuf_sent = telnet_outbuf_in - 6;
	client_tpcb = newpcb;
	return ERR_OK;
}

static void
do_init (void *arg)
{
	struct tcp_pcb *telnet_pcb = tcp_new ();
	char *name = arg;

	if (telnet_pcb != NULL) {
		err_t err = tcp_bind (telnet_pcb, IP_ADDR_ANY, TELNET_PORT);
		if (err == ERR_OK) {
			telnet_pcb = tcp_listen (telnet_pcb);
			tcp_accept (telnet_pcb, telnet_accept);
			printf ("telnet server \"%s\" ready\n", name);
		}
	}
}

void
telnet_server_init (char *name)
{
	tcpip_begin (do_init, name);
}
