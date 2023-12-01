/**
 * @file
 *
 * lwIP Options Configuration
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
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 *
 */
#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* --- Platform --- */
#define SYS_LIGHTWEIGHT_PROT            0
#define NO_SYS                          1
#define MEM_ALIGNMENT                   4 /* 4-byte alignment */
#define MEM_SIZE                        (16*1024*1024) /* Heap size (16MB) */
#ifdef WIREGUARD_VMM
#define MEMP_NUM_SYS_TIMEOUT		(LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)
#endif
#ifdef MBEDTLS_VMM
#define LWIP_ALTCP_TLS				1
#define LWIP_ALTCP_TLS_MBEDTLS			1
#define LWIP_ALTCP				1
#define MEMP_NUM_ALTCP_PCB			10
#define ALTCP_MBEDTLS_USE_SESSION_TICKETS	1
#define ALTCP_MBEDTLS_USE_SESSION_CACHE		1
#endif

/* --- DHCP --- */
#define DHCP_DOES_ARP_CHECK             0 /* Don't Check Binded Addr */

/* --- Protocols --- */
#define LWIP_ARP                        1 /* Use ARP */
#define LWIP_ICMP                       1 /* Use TCMP */
#define LWIP_RAW                        1 /* Use Raw API */
#define LWIP_DHCP                       1 /* Use DHCP */
#define LWIP_AUTOIP                     1 /* Use Auto IP */
#define LWIP_SNMP                       0 /* Don't Use SNMP */
#define LWIP_IGMP                       0 /* Don't Use IGMP */
#define LWIP_DNS                        0 /* Use DNS */
#define LWIP_UDP                        1 /* Use UDP */
#define LWIP_TCP                        1 /* Use TCP */

/* --- PBuf --- */
#define PBUF_LINK_HLEN                  16
#define PBUF_POOL_BUFSIZE               LWIP_MEM_ALIGN_SIZE(TCP_MSS+40+PBUF_LINK_HLEN)

/* --- APIs --- */
#define LWIP_NETCONN                    0 /* Use netconn API */
#define LWIP_SOCKET                     0 /* Don't Use socket API */

/* --- Misc --- */
#define LWIP_STATS                      0 /* Don't Use statistics. */

#endif /* __LWIPOPTS_H__ */
