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

/**
 * @file
 * TCP echo server example using raw API.
 *
 * Echos all bytes sent by connecting client,
 * and passively closes when client is done.
 *
 */

/* The implementation is based on echo-server.c" */

#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "mbedtls_helpers.h"
#include "tls-echo.h"

#if LWIP_TCP

static struct altcp_pcb *echo_pcb;

enum echo_states
{
  ES_NONE = 0,
  ES_ACCEPTED,
  ES_RECEIVED,
  ES_CLOSING
};

struct echo_state
{
  u8_t state;
  u8_t retries;
  struct altcp_pcb *pcb;
  /* pbuf (chain) to recycle */
  struct pbuf *p;
};

static err_t tls_echo_accept (void *arg, struct altcp_pcb *newpcb, err_t err);
static err_t tls_echo_recv (void *arg, struct altcp_pcb *tpcb, struct pbuf *p,
			    err_t err);
static void tls_echo_error (void *arg, err_t err);
static err_t tls_echo_poll (void *arg, struct altcp_pcb *tpcb);
static err_t tls_echo_sent (void *arg, struct altcp_pcb *tpcb, u16_t len);
static void tls_echo_send (struct altcp_pcb *tpcb, struct echo_state *es);
static void tls_echo_close (struct altcp_pcb *tpcb, struct echo_state *es);

void
tls_echo_server_init (int port)
{
  u8_t ca_cert_der[4096];
  u8_t srv_cert_der[4096];
  u8_t srv_key_der[4096];
  size_t ca_cert_der_len = sizeof ca_cert_der;
  size_t srv_cert_der_len = sizeof srv_cert_der;
  size_t srv_key_der_len = sizeof srv_key_der;

  if (convert_pem_to_der ((u8_t *)ECHO_TLS_CA_CRT,
    strlen (ECHO_TLS_CA_CRT), ca_cert_der, &ca_cert_der_len)
    || convert_pem_to_der ((u8_t *)ECHO_TLS_SERVER_CRT,
    strlen (ECHO_TLS_SERVER_CRT), srv_cert_der, &srv_cert_der_len)
    || convert_pem_to_der ((u8_t *)ECHO_TLS_SERVER_KEY,
    strlen (ECHO_TLS_SERVER_KEY), srv_key_der, &srv_key_der_len))
      printf ("Convert certification PEM to DER failed\n");

  struct altcp_tls_config *conf =
  altcp_tls_create_config_server_privkey_cert_cacert (srv_key_der,
    srv_key_der_len, NULL, 0, srv_cert_der, srv_cert_der_len,
    ca_cert_der, ca_cert_der_len);

  if (!conf)
    printf ("Invalid server certificates.\n");

  altcp_allocator_t tls_allocator = { altcp_tls_alloc, conf };
  echo_pcb = altcp_new (&tls_allocator);
  if (echo_pcb != NULL)
  {
    err_t err;

    err = altcp_bind (echo_pcb, IP_ADDR_ANY, port);
    if (err == ERR_OK)
    {
      echo_pcb = altcp_listen (echo_pcb);
      altcp_accept (echo_pcb, tls_echo_accept);
    }
    else 
    {
      /* abort? output diagnostic? */
    }
  }
  else
  {
    /* abort? output diagnostic? */
  }
}


static err_t
tls_echo_accept (void *arg, struct altcp_pcb *newpcb, err_t err)
{
  err_t ret_err;
  struct echo_state *es;

  LWIP_UNUSED_ARG(arg);
  LWIP_UNUSED_ARG(err);

  /* commonly observed practive to call tcp_setprio(), why? */
  altcp_setprio (newpcb, TCP_PRIO_MIN);

  es = (struct echo_state *)mem_malloc(sizeof(struct echo_state));
  if (es != NULL)
  {
    es->state = ES_ACCEPTED;
    es->pcb = newpcb;
    es->retries = 0;
    es->p = NULL;
    /* pass newly allocated es to our callbacks */
    altcp_arg (newpcb, es);
    altcp_recv (newpcb, tls_echo_recv);
    altcp_err (newpcb, tls_echo_error);
    altcp_poll (newpcb, tls_echo_poll, 0);
    ret_err = ERR_OK;
  }
  else
  {
    ret_err = ERR_MEM;
  }
  return ret_err;  
}

static err_t
tls_echo_recv (void *arg, struct altcp_pcb *tpcb, struct pbuf *p, err_t err)
{
  struct echo_state *es;
  err_t ret_err;

  LWIP_ASSERT("arg != NULL",arg != NULL);
  es = (struct echo_state *)arg;
  if (p == NULL)
  {
    /* remote host closed connection */
    es->state = ES_CLOSING;
    if(es->p == NULL)
    {
       /* we're done sending, close it */
       tls_echo_close (tpcb, es);
    }
    else
    {
      /* we're not done yet */
      altcp_sent (tpcb, tls_echo_sent);
      tls_echo_send (tpcb, es);
    }
    ret_err = ERR_OK;
  }
  else if(err != ERR_OK)
  {
    /* cleanup, for unkown reason */
    if (p != NULL)
    {
      es->p = NULL;
      pbuf_free(p);
    }
    ret_err = err;
  }
  else if(es->state == ES_ACCEPTED)
  {
    /* first data chunk in p->payload */
    es->state = ES_RECEIVED;
    /* store reference to incoming pbuf (chain) */
    es->p = p;
    /* install send completion notifier */
    altcp_sent (tpcb, tls_echo_sent);
    tls_echo_send (tpcb, es);
    ret_err = ERR_OK;
  }
  else if (es->state == ES_RECEIVED)
  {
    /* read some more data */
    if(es->p == NULL)
    {
      es->p = p;
      altcp_sent (tpcb, tls_echo_sent);
      tls_echo_send (tpcb, es);
    }
    else
    {
      struct pbuf *ptr;

      /* chain pbufs to the end of what we recv'ed previously  */
      ptr = es->p;
      pbuf_chain(ptr,p);
    }
    ret_err = ERR_OK;
  }
  else if(es->state == ES_CLOSING)
  {
    /* odd case, remote side closing twice, trash data */
    altcp_recved (tpcb, p->tot_len);
    es->p = NULL;
    pbuf_free(p);
    ret_err = ERR_OK;
  }
  else
  {
    /* unkown es->state, trash data  */
    altcp_recved (tpcb, p->tot_len);
    es->p = NULL;
    pbuf_free(p);
    ret_err = ERR_OK;
  }
  return ret_err;
}

static void
tls_echo_error (void *arg, err_t err)
{
  struct echo_state *es;

  LWIP_UNUSED_ARG(err);

  es = (struct echo_state *)arg;
  if (es != NULL)
  {
    mem_free(es);
  }
}

static err_t
tls_echo_poll (void *arg, struct altcp_pcb *tpcb)
{
  err_t ret_err;
  struct echo_state *es;

  es = (struct echo_state *)arg;
  if (es != NULL)
  {
    if (es->p != NULL)
    {
      /* there is a remaining pbuf (chain)  */
      altcp_sent (tpcb, tls_echo_sent);
      tls_echo_send (tpcb, es);
    }
    else
    {
      /* no remaining pbuf (chain)  */
      if(es->state == ES_CLOSING)
      {
        tls_echo_close (tpcb, es);
      }
    }
    ret_err = ERR_OK;
  }
  else
  {
    /* nothing to be done */
    altcp_abort (tpcb);
    ret_err = ERR_ABRT;
  }
  return ret_err;
}

static err_t
tls_echo_sent (void *arg, struct altcp_pcb *tpcb, u16_t len)
{
  struct echo_state *es;

  LWIP_UNUSED_ARG(len);

  es = (struct echo_state *)arg;
  es->retries = 0;
  
  if(es->p != NULL)
  {
    /* still got pbufs to send */
    altcp_sent (tpcb, tls_echo_sent);
    tls_echo_send (tpcb, es);
  }
  else
  {
    /* no more pbufs to send */
    if(es->state == ES_CLOSING)
    {
      tls_echo_close (tpcb, es);
    }
  }
  return ERR_OK;
}

static void
tls_echo_send (struct altcp_pcb *tpcb, struct echo_state *es)
{
  struct pbuf *ptr;
  err_t wr_err = ERR_OK;
 
  while ((wr_err == ERR_OK) &&
         (es->p != NULL) && 
         (es->p->len <= altcp_sndbuf (tpcb)))
  {
  ptr = es->p;

  /* enqueue data for transmission */
  wr_err = altcp_write (tpcb, ptr->payload, ptr->len, 1);
  if (wr_err == ERR_OK)
  {
     u16_t plen;
      u8_t freed;

     plen = ptr->len;
     /* continue with next pbuf in chain (if any) */
     es->p = ptr->next;
     if(es->p != NULL)
     {
       /* new reference! */
       pbuf_ref(es->p);
     }
     /* chop first pbuf from chain */
      do
      {
        /* try hard to free pbuf */
        freed = pbuf_free(ptr);
      }
      while(freed == 0);
     /* we can read more data now */
     altcp_recved (tpcb, plen);
   }
   else if(wr_err == ERR_MEM)
   {
      /* we are low on memory, try later / harder, defer to poll */
     es->p = ptr;
   }
   else
   {
     /* other problem ?? */
   }
  }
}

static void
tls_echo_close(struct altcp_pcb *tpcb, struct echo_state *es)
{
  altcp_arg (tpcb, NULL);
  altcp_sent (tpcb, NULL);
  altcp_recv (tpcb, NULL);
  altcp_err (tpcb, NULL);
  altcp_poll (tpcb, NULL, 0);
  
  if (es != NULL)
  {
    mem_free(es);
  }  
  altcp_close (tpcb);
}

#endif /* LWIP_TCP */
