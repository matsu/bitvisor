/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <core.h>
#include <core/arith.h>
#include <core/cpu.h>
#include <core/iccard.h>
#include <core/process.h>
#include <core/time.h>
#include <core/timer.h>
#include <core/vpnsys.h>
#include <IDMan.h>
#include "vpn_msg.h"

#define NUM_OF_HANDLE 32

struct nicdata {
	SE_HANDLE ph, vh;
	struct nicfunc func;
};

#ifdef VPN_PD
static struct mempool *mp;
static int vpnkernel_desc, desc;
static void *handle[NUM_OF_HANDLE];
static spinlock_t handle_lock;	/* new only */
static SE_HANDLE vpn_timer_handle;

static void
callsub (int c, struct msgbuf *buf, int bufcnt)
{
	int r;

	for (;;) {
		r = msgsendbuf (desc, c, buf, bufcnt);
		if (r == 0)
			break;
		panic ("vpn msgsendbuf failed (%d)", c);
	}
}

void
vpn_user_init (struct config_data_vpn *vpn, char *seed, int len)
{
	int d, d1;
	struct config_data_vpn *p;
	char *q;
	struct msgbuf buf[2];

	mp = mempool_new (0, 1, true);
	d = newprocess ("vpn");
	if (d < 0)
		panic ("newprocess vpn");
	d1 = msgopen ("ttyout");
	if (d1 < 0)
		panic ("msgopen ttyout");
	msgsenddesc (d, d1);
	msgsenddesc (d, d1);
	msgclose (d1);
	p = mempool_allocmem (mp, sizeof *p);
	memcpy (p, vpn, sizeof *p);
	q = mempool_allocmem (mp, len);
	memcpy (q, seed, len);
	setmsgbuf (&buf[0], p, sizeof *p, 0);
	setmsgbuf (&buf[1], q, sizeof *q, 0);
	if (msgsendbuf (d, 0, buf, 2))
		panic ("vpn init failed");
	mempool_freemem (mp, q);
	mempool_freemem (mp, p);
	msgclose (d);
}

SE_HANDLE
vpn_start (void *data)
{
	struct vpn_msg_start *arg;
	struct msgbuf buf[1];
	SE_HANDLE ret;
	int i;

	spinlock_lock (&handle_lock);
	for (i = 0; i < NUM_OF_HANDLE; i++)
		if (handle[i] == NULL)
			goto found;
	panic ("vpn_start: handle full");
found:
	handle[i] = data;
	spinlock_unlock (&handle_lock);
	arg = mempool_allocmem (mp, sizeof *arg);
	arg->handle = i;
	arg->cpu = get_cpu_id ();
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (VPN_MSG_START, buf, 1);
	ret = arg->retval;
	mempool_freemem (mp, arg);
	return ret;
}

static void
sendphysicalnicrecv_premap (SE_HANDLE nic_handle, UINT num_packets,
			    void **packets, UINT *packet_sizes, void *param,
			    long *premap)
{
	struct vpn_msg_physicalnicrecv *arg;
	struct msgbuf *buf;
	UINT i;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->nic_handle = nic_handle;
	arg->param = param;
	arg->num_packets = num_packets;
	arg->cpu = get_cpu_id ();
	buf = alloc (sizeof *buf * (1 + num_packets));
	setmsgbuf (&buf[0], arg, sizeof *arg, 0);
	if (premap) {
		for (i = 0; i < num_packets; i++)
			setmsgbuf_premap (&buf[1 + i], packets[i],
					  packet_sizes[i], 0, premap[i]);
	} else {
		for (i = 0; i < num_packets; i++)
			setmsgbuf (&buf[1 + i], packets[i], packet_sizes[i],
				   0);
	}
	callsub (VPN_MSG_PHYSICALNICRECV, buf, num_packets + 1);
	free (buf);
	mempool_freemem (mp, arg);
}

static void
sendphysicalnicrecv (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		     UINT *packet_sizes, void *param)
{
	sendphysicalnicrecv_premap (nic_handle, num_packets, packets,
				    packet_sizes, param, NULL);
}

static void
sendvirtualnicrecv_premap (SE_HANDLE nic_handle, UINT num_packets,
			   void **packets, UINT *packet_sizes, void *param,
			   long *premap)
{
	struct vpn_msg_virtualnicrecv *arg;
	struct msgbuf *buf;
	UINT i;

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->nic_handle = nic_handle;
	arg->param = param;
	arg->num_packets = num_packets;
	arg->cpu = get_cpu_id ();
	buf = alloc (sizeof *buf * (1 + num_packets));
	setmsgbuf (&buf[0], arg, sizeof *arg, 0);
	if (premap) {
		for (i = 0; i < num_packets; i++)
			setmsgbuf_premap (&buf[1 + i], packets[i],
					  packet_sizes[i], 0, premap[i]);
	} else {
		for (i = 0; i < num_packets; i++)
			setmsgbuf (&buf[1 + i], packets[i], packet_sizes[i],
				   0);
	}
	callsub (VPN_MSG_VIRTUALNICRECV, buf, num_packets + 1);
	free (buf);
	mempool_freemem (mp, arg);
}

static void
sendvirtualnicrecv (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		    UINT *packet_sizes, void *param)
{
	sendvirtualnicrecv_premap (nic_handle, num_packets, packets,
				   packet_sizes, param, NULL);
}

static void
vpn_timer_callback (void *handle, void *data)
{
	struct vpn_msg_timer *arg;
	struct msgbuf buf[1];

	arg = mempool_allocmem (mp, sizeof *arg);
	arg->now = vpn_GetTickCount ();
	arg->cpu = get_cpu_id ();
	setmsgbuf (&buf[0], arg, sizeof *arg, 1);
	callsub (VPN_MSG_TIMER, buf, 1);
	mempool_freemem (mp, arg);
}

static int
vpnkernel_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (c == VPNKERNEL_MSG_IC_RSA_SIGN) {
		struct vpnkernel_msg_ic_rsa_sign *arg;

		if (bufcnt != 4)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		if (buf[2].len > 65536)
			return -1;
		arg = buf[0].base;
		arg->retval = vpn_ic_rsa_sign (buf[1].base, buf[2].base,
					       buf[2].len, buf[3].base,
					       &arg->sign_buf_size);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_GETPHYSICALNICINFO) {
		struct vpnkernel_msg_getphysicalnicinfo *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_GetPhysicalNicInfo (handle[h], &arg->info);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_GETVIRTUALNICINFO) {
		struct vpnkernel_msg_getvirtualnicinfo *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_GetVirtualNicInfo (handle[h], &arg->info);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SENDPHYSICALNIC) {
		struct vpnkernel_msg_sendphysicalnic *arg;
		int h;
		UINT num_packets;
		void **packets;
		UINT *packet_sizes;
		int i;

		if (bufcnt < 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		num_packets = arg->num_packets;
		if (bufcnt != 1 + num_packets)
			return -1;
		if (num_packets > 255) { /* maximum number of packets */
			printf ("sending too many packets (physical)\n");
			goto skip1;
		}
		if (num_packets == 0)
			goto skip1;
		packets = alloc (sizeof *packets * num_packets);
		packet_sizes = alloc (sizeof *packet_sizes * num_packets);
		for (i = 0; i < num_packets; i++) {
			packets[i] = buf[i + 1].base;
			packet_sizes[i] = buf[i + 1].len;
		}
		vpn_SendPhysicalNic (handle[h], num_packets, packets,
				     packet_sizes);
		free (packets);
		free (packet_sizes);
	skip1:
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SENDVIRTUALNIC) {
		struct vpnkernel_msg_sendvirtualnic *arg;
		int h;
		UINT num_packets;
		void **packets;
		UINT *packet_sizes;
		int i;

		if (bufcnt < 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		num_packets = arg->num_packets;
		if (bufcnt != 1 + num_packets)
			return -1;
		if (num_packets > 255) { /* maximum number of packets */
			printf ("sending too many packets (virtual)\n");
			goto skip2;
		}
		if (num_packets == 0)
			goto skip2;
		packets = alloc (sizeof *packets * num_packets);
		packet_sizes = alloc (sizeof *packet_sizes * num_packets);
		for (i = 0; i < num_packets; i++) {
			packets[i] = buf[i + 1].base;
			packet_sizes[i] = buf[i + 1].len;
		}
		vpn_SendVirtualNic (handle[h], num_packets, packets,
				     packet_sizes);
		free (packets);
		free (packet_sizes);
	skip2:
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SETPHYSICALNICRECVCALLBACK) {
		struct vpnkernel_msg_setphysicalnicrecvcallback *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_SetPhysicalNicRecvCallback (handle[h], sendphysicalnicrecv,
						arg->param);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SETVIRTUALNICRECVCALLBACK) {
		struct vpnkernel_msg_setvirtualnicrecvcallback *arg;
		int h;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		h = arg->handle;
		if (h < 0 || h >= NUM_OF_HANDLE)
			return -1;
		vpn_SetVirtualNicRecvCallback (handle[h], sendvirtualnicrecv,
					       arg->param);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_SET_TIMER) {
		struct vpnkernel_msg_set_timer *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		vpn_SetTimer (vpn_timer_handle, arg->interval);
		arg->cpu = get_cpu_id ();
		return 0;
	} else if (c == VPNKERNEL_MSG_GETTICKCOUNT) {
		struct vpnkernel_msg_gettickcount *arg;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		arg->retval = vpn_GetTickCount ();
		arg->cpu = get_cpu_id ();
		return 0;
	} else {
		return -1;
	}
}
#endif /* VPN_PD */

UINT
vpn_GetCurrentCpuId (void)
{
	return get_cpu_id ();
}

UINT
vpn_GetTickCount (void)
{
	u64 d[2], q[2];

	d[0] = get_time ();
	d[1] = 0;
	mpudiv_128_32 (d, 1000, q);
	return (UINT)q[0];
}

SE_HANDLE
vpn_NewTimer (SE_SYS_CALLBACK_TIMER *callback, void *param)
{
	void *p;

	p = timer_new (callback, param);
	if (!p)
		panic ("NewTimer failed");
	return p;
}

void
vpn_SetTimer (SE_HANDLE timer_handle, UINT interval)
{
	u64 usec;

	usec = interval;
	usec *= 1000;
	timer_set (timer_handle, usec);
}

void
vpn_FreeTimer (SE_HANDLE timer_handle)
{
	timer_free (timer_handle);
}

void
vpn_GetPhysicalNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct nicdata *nic = nic_handle;

	nic->func.GetPhysicalNicInfo (nic->ph, info);
}

void
vpn_SendPhysicalNic (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		     UINT *packet_sizes)
{
	struct nicdata *nic = nic_handle;

	nic->func.SendPhysicalNic (nic->ph, num_packets, packets,
				   packet_sizes);
}

void
vpn_SetPhysicalNicRecvCallback (SE_HANDLE nic_handle,
				SE_SYS_CALLBACK_RECV_NIC *callback,
				void *param)
{
	struct nicdata *nic = nic_handle;

	nic->func.SetPhysicalNicRecvCallback (nic->ph, callback, param);
}

void
vpn_GetVirtualNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct nicdata *nic = nic_handle;

	nic->func.GetVirtualNicInfo (nic->vh, info);
}

void
vpn_SendVirtualNic (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		    UINT *packet_sizes)
{
	struct nicdata *nic = nic_handle;

	nic->func.SendVirtualNic (nic->vh, num_packets, packets, packet_sizes);
}

void
vpn_SetVirtualNicRecvCallback (SE_HANDLE nic_handle,
			       SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	struct nicdata *nic = nic_handle;

	nic->func.SetVirtualNicRecvCallback (nic->vh, callback, param);
}

void
vpn_premap_PhysicalNicRecv (SE_SYS_CALLBACK_RECV_NIC *callback,
			    SE_HANDLE nic_handle, UINT num_packets,
			    void **packets, UINT *packet_sizes, void *param,
			    long *premap)
{
#ifdef VPN_PD
	if (callback == sendphysicalnicrecv) {
		sendphysicalnicrecv_premap (nic_handle, num_packets, packets,
					    packet_sizes, param, premap);
		return;
	}
#endif /* VPN_PD */
	callback (nic_handle, num_packets, packets, packet_sizes, param);
}

void
vpn_premap_VirtualNicRecv (SE_SYS_CALLBACK_RECV_NIC *callback,
			   SE_HANDLE nic_handle, UINT num_packets,
			   void **packets, UINT *packet_sizes, void *param,
			   long *premap)
{
#ifdef VPN_PD
	if (callback == sendvirtualnicrecv) {
		sendvirtualnicrecv_premap (nic_handle, num_packets, packets,
					   packet_sizes, param, premap);
		return;
	}
#endif /* VPN_PD */
	callback (nic_handle, num_packets, packets, packet_sizes, param);
}

long
vpn_premap_recvbuf (void *buf, unsigned int len)
{
#ifdef VPN_PD
	struct msgbuf mbuf;

	setmsgbuf (&mbuf, buf, len, 0);
	return msgpremapbuf (desc, &mbuf);
#endif /* VPN_PD */
	return 0;
}

bool
vpn_ic_rsa_sign (char *key_name, void *data, UINT data_size, void *sign,
		 UINT *sign_buf_size)
{
	unsigned long idman_session;
	unsigned short int signlen;
	int idman_ret;

#ifdef IDMAN
	if (!get_idman_session (&idman_session)) {
		printf ("RsaSign: card reader not ready\n");
		return false;
	}
	signlen = *sign_buf_size;
	idman_ret = IDMan_EncryptByIndex (idman_session, 2, data, data_size,
					  sign, &signlen, 0x220);
	if (!idman_ret) {
		*sign_buf_size = signlen;
		return true;
	}
#endif /* IDMAN */
	printf ("RsaSign: IDMan_EncryptByIndex()"
		" ret=%d data_size=%d\n", idman_ret, data_size);
	return false;
}

SE_HANDLE
vpn_new_nic (SE_HANDLE ph, SE_HANDLE vh, struct nicfunc *func)
{
	struct nicdata *p;

#ifdef VPN_PASS_MODE
	pass_init (ph, vh, func);
	return "PASS MODE";
#endif /* VPN_PASS_MODE */
	p = alloc (sizeof *p);
	p->ph = ph;
	p->vh = vh;
	memcpy (&p->func, func, sizeof *func);
	return vpn_start (p);
}

static void
vpn_kernel_init (void)
{
	void vpn_user_init (struct config_data_vpn *vpn, char *seed, int len);

#ifdef VPN_PD
	int i;

	spinlock_init (&handle_lock);
	for (i = 0; i < NUM_OF_HANDLE; i++)
		handle[i] = NULL;
	vpn_timer_handle = vpn_NewTimer (vpn_timer_callback, NULL);
	vpnkernel_desc = msgregister ("vpnkernel", vpnkernel_msghandler);
	if (vpnkernel_desc < 0)
		panic ("register vpnkernel");
#endif /* VPN_PD */
	vpn_user_init (&config.vpn, config.vmm.randomSeed,
		       sizeof config.vmm.randomSeed);
#ifdef VPN_PD
	desc = msgopen ("vpn");
	if (desc < 0)
		panic ("open vpn");
#endif
}

INITFUNC ("driver1", vpn_kernel_init);
