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
#include <core/config.h>
#include <core/process.h>
#include "crypt.h"
#include "vpn_msg.h"

#define CONFIG_NAME "config.txt"
#define CERTV4_NAME "certnamev4"
#define CACERTV4_NAME "cacertnamev4"
#define RSAKEYV4_NAME "rsakeynamev4"
#define CERTV6_NAME "certnamev6"
#define CACERTV6_NAME "cacertnamev6"
#define RSAKEYV6_NAME "rsakeynamev6"
#define VPNAUTHMETHOD_CERTIC "Cert-IC"
#define VPNAUTHMETHOD_CERT "Cert"

struct vpnconfig {
	char *name;
	char *value;
};

struct vpnhandle {
	int handle;
	SE_SYS_CALLBACK_RECV_NIC *recvphys_func, *recvvirt_func;
	void *recvphys_param, *recvvirt_param;
};

struct vpn_timer {
	struct vpn_timer *next;
	SE_SYS_CALLBACK_TIMER *callback;
	void *param;
	bool enable;
	UINT settime;
	UINT interval;
};

static char vpnauthmethodv4[] = "VpnAuthMethodV4";
static char vpnauthmethodv6[] = "VpnAuthMethodV6";
static char c_vpnrsakeynamev4[] = "#VpnRsaKeyNameV4";
static char c_vpnrsakeynamev6[] = "#VpnRsaKeyNameV6";

static int vpnkernel_desc, vpn_desc;
static struct config_data_vpn config_vpn;
static struct vpn_timer *vpn_timer_head;
static spinlock_t timer_lock;

static struct vpnconfig vpncfg[] = {
	{ "Mode", config_vpn.mode },
	{ "VirtualGatewayMacAddress", config_vpn.virtualGatewayMacAddress },
	{ "BindV4", config_vpn.bindV4 },
	{ "GuestIpAddressV4", config_vpn.guestIpAddressV4 },
	{ "GuestIpSubnetV4", config_vpn.guestIpSubnetV4 },
	{ "GuestMtuV4", config_vpn.guestMtuV4 },
	{ "GuestVirtualGatewayIpAddressV4", config_vpn.guestVirtualGatewayIpAddressV4 },
	{ "DhcpV4", config_vpn.dhcpV4 },
	{ "DhcpLeaseExpiresV4", config_vpn.dhcpLeaseExpiresV4 },
	{ "DhcpDnsV4", config_vpn.dhcpDnsV4 },
	{ "DhcpDomainV4", config_vpn.dhcpDomainV4 },
	{ "AdjustTcpMssV4", config_vpn.adjustTcpMssV4 },
	{ "HostIpAddressV4", config_vpn.hostIpAddressV4 },
	{ "HostIpSubnetV4", config_vpn.hostIpSubnetV4 },
	{ "HostMtuV4", config_vpn.hostMtuV4 },
	{ "HostIpDefaultGatewayV4", config_vpn.hostIpDefaultGatewayV4 },
	{ "OptionV4ArpExpires", config_vpn.optionV4ArpExpires },
	{ "OptionV4ArpDontUpdateExpires", config_vpn.optionV4ArpDontUpdateExpires },
	{ "VpnGatewayAddressV4", config_vpn.vpnGatewayAddressV4 },
	{ vpnauthmethodv4, config_vpn.vpnAuthMethodV4 },
	{ "VpnPasswordV4", config_vpn.vpnPasswordV4 },
	{ "VpnIdStringV4", config_vpn.vpnIdStringV4 },
	{ "#VpnCertNameV4", CERTV4_NAME },
	{ "#VpnCaCertNameV4", CACERTV4_NAME },
	{ c_vpnrsakeynamev4, RSAKEYV4_NAME },
	{ "VpnSpecifyIssuerV4", config_vpn.vpnSpecifyIssuerV4 },
	{ "VpnPhase1ModeV4", config_vpn.vpnPhase1ModeV4 },
	{ "VpnPhase1CryptoV4", config_vpn.vpnPhase1CryptoV4 },
	{ "VpnPhase1HashV4", config_vpn.vpnPhase1HashV4 },
	{ "VpnPhase1LifeSecondsV4", config_vpn.vpnPhase1LifeSecondsV4 },
	{ "VpnPhase1LifeKilobytesV4", config_vpn.vpnPhase1LifeKilobytesV4 },
	{ "VpnWaitPhase2BlankSpanV4", config_vpn.vpnWaitPhase2BlankSpanV4 },
	{ "VpnPhase2CryptoV4", config_vpn.vpnPhase2CryptoV4 },
	{ "VpnPhase2HashV4", config_vpn.vpnPhase2HashV4 },
	{ "VpnPhase2LifeSecondsV4", config_vpn.vpnPhase2LifeSecondsV4 },
	{ "VpnPhase2LifeKilobytesV4", config_vpn.vpnPhase2LifeKilobytesV4 },
	{ "VpnConnectTimeoutV4", config_vpn.vpnConnectTimeoutV4 },
	{ "VpnIdleTimeoutV4", config_vpn.vpnIdleTimeoutV4 },
	{ "VpnPingTargetV4", config_vpn.vpnPingTargetV4 },
	{ "VpnPingIntervalV4", config_vpn.vpnPingIntervalV4 },
	{ "VpnPingMsgSizeV4", config_vpn.vpnPingMsgSizeV4 },
	{ "BindV6", config_vpn.bindV6 },
	{ "GuestIpAddressPrefixV6", config_vpn.guestIpAddressPrefixV6 },
	{ "GuestIpAddressSubnetV6", config_vpn.guestIpAddressSubnetV6 },
	{ "GuestMtuV6", config_vpn.guestMtuV6 },
	{ "GuestVirtualGatewayIpAddressV6", config_vpn.guestVirtualGatewayIpAddressV6 },
	{ "RaV6", config_vpn.raV6 },
	{ "RaLifetimeV6", config_vpn.raLifetimeV6 },
	{ "RaDnsV6", config_vpn.raDnsV6 },
	{ "HostIpAddressV6", config_vpn.hostIpAddressV6 },
	{ "HostIpAddressSubnetV6", config_vpn.hostIpAddressSubnetV6 },
	{ "HostMtuV6", config_vpn.hostMtuV6 },
	{ "HostIpDefaultGatewayV6", config_vpn.hostIpDefaultGatewayV6 },
	{ "OptionV6NeighborExpires", config_vpn.optionV6NeighborExpires },
	{ "VpnGatewayAddressV6", config_vpn.vpnGatewayAddressV6 },
	{ vpnauthmethodv6, config_vpn.vpnAuthMethodV6 },
	{ "VpnPasswordV6", config_vpn.vpnPasswordV6 },
	{ "VpnIdStringV6", config_vpn.vpnIdStringV6 },
	{ "#VpnCertNameV6", CERTV6_NAME },
	{ "#VpnCaCertNameV6", CACERTV6_NAME },
	{ c_vpnrsakeynamev6, RSAKEYV6_NAME },
	{ "VpnSpecifyIssuerV6", config_vpn.vpnSpecifyIssuerV6 },
	{ "VpnPhase1ModeV6", config_vpn.vpnPhase1ModeV6 },
	{ "VpnPhase1CryptoV6", config_vpn.vpnPhase1CryptoV6 },
	{ "VpnPhase1HashV6", config_vpn.vpnPhase1HashV6 },
	{ "VpnPhase1LifeSecondsV6", config_vpn.vpnPhase1LifeSecondsV6 },
	{ "VpnPhase1LifeKilobytesV6", config_vpn.vpnPhase1LifeKilobytesV6 },
	{ "VpnWaitPhase2BlankSpanV6", config_vpn.vpnWaitPhase2BlankSpanV6 },
	{ "VpnPhase2CryptoV6", config_vpn.vpnPhase2CryptoV6 },
	{ "VpnPhase2HashV6", config_vpn.vpnPhase2HashV6 },
	{ "VpnPhase2LifeSecondsV6", config_vpn.vpnPhase2LifeSecondsV6 },
	{ "VpnPhase2LifeKilobytesV6", config_vpn.vpnPhase2LifeKilobytesV6 },
	{ "VpnPhase2StrictIdV6", config_vpn.vpnPhase2StrictIdV6 },
	{ "VpnConnectTimeoutV6", config_vpn.vpnConnectTimeoutV6 },
	{ "VpnIdleTimeoutV6", config_vpn.vpnIdleTimeoutV6 },
	{ "VpnPingTargetV6", config_vpn.vpnPingTargetV6 },
	{ "VpnPingIntervalV6", config_vpn.vpnPingIntervalV6 },
	{ "VpnPingMsgSizeV6", config_vpn.vpnPingMsgSizeV6 },
	{ NULL, NULL }
};

static void *
MemoryAlloc (UINT size)
{
	return alloc (size);
}

static void *
MemoryReAlloc (void *addr, UINT size)
{
	return realloc (addr, size);
}

static void
MemoryFree (void *addr)
{
	return free (addr);
}

static SE_HANDLE
NewLock (void)
{
	spinlock_t *p;

	p = alloc (sizeof *p);
	if (!p)
		panic ("NewLock failed");
	spinlock_init (p);
	return p;
}

static void
Lock (SE_HANDLE lock_handle)
{
	spinlock_t *p = lock_handle;

	spinlock_lock (p);
}

static void
Unlock (SE_HANDLE lock_handle)
{
	spinlock_t *p = lock_handle;

	spinlock_unlock (p);
}

static void
FreeLock (SE_HANDLE lock_handle)
{
	spinlock_t *p = lock_handle;

	free (p);
}

static bool
SaveData (char *name, void *data, UINT data_size)
{
	printf ("SaveData \"%s\"\n", name);
	return false;
}

static bool
getcertbuf (char *name, char **buf, unsigned int *len)
{
	if (!strcmp (name, CERTV4_NAME)) {
		*buf = config_vpn.vpnCertV4;
		*len = sizeof config_vpn.vpnCertV4;
	} else if (!strcmp (name, CACERTV4_NAME)) {
		*buf = config_vpn.vpnCaCertV4;
		*len = sizeof config_vpn.vpnCaCertV4;
	} else if (!strcmp (name, RSAKEYV4_NAME)) {
		*buf = config_vpn.vpnRsaKeyV4;
		*len = sizeof config_vpn.vpnRsaKeyV4;
	} else if (!strcmp (name, CERTV6_NAME)) {
		*buf = config_vpn.vpnCertV6;
		*len = sizeof config_vpn.vpnCertV6;
	} else if (!strcmp (name, CACERTV6_NAME)) {
		*buf = config_vpn.vpnCaCertV6;
		*len = sizeof config_vpn.vpnCaCertV6;
	} else if (!strcmp (name, RSAKEYV6_NAME)) {
		*buf = config_vpn.vpnRsaKeyV6;
		*len = sizeof config_vpn.vpnRsaKeyV6;
	} else {
		return false;
	}
	return true;
}

static bool
certempty (char *name)
{
	char *buf;
	unsigned int len;

	if (!getcertbuf (name, &buf, &len))
		return true;
	while (len--)
		if (*buf++)
			return false;
	return true;
}

static bool
LoadData (char *name, void **data, UINT *data_size)
{
	unsigned int size, len;
	struct vpnconfig *p;
	char *q, *pname, *pvalue, *buf;
	char *method;

	if (strcmp (name, CONFIG_NAME) == 0) {
		size = 0;
		for (p = vpncfg; p->name; p++) {
			if (p->value[0] == '\0')
				continue;
			size += strlen (p->name) + 1 + strlen (p->value) + 1;
		}
		size++;
		q = alloc (size);
		if (!q)
			return false;
		*data = q;
		*data_size = size;
		for (p = vpncfg; p->name; p++) {
			if (p->value[0] == '\0')
				continue;
			pname = p->name;
			pvalue = p->value;
			if (*pname == '#') {
				method = NULL;
				if (!certempty (pvalue))
					pname++;
				else if (pname == c_vpnrsakeynamev4)
					method = config_vpn.vpnAuthMethodV4;
				else if (pname == c_vpnrsakeynamev6)
					method = config_vpn.vpnAuthMethodV6;
				if (method && !strcmp (method, "Cert-IC"))
					pname++;
			}
			if (pname == vpnauthmethodv4)
				if (strcmp (pvalue, VPNAUTHMETHOD_CERTIC) == 0)
					pvalue = VPNAUTHMETHOD_CERT;
			if (pname == vpnauthmethodv6)
				if (strcmp (pvalue, VPNAUTHMETHOD_CERTIC) == 0)
					pvalue = VPNAUTHMETHOD_CERT;
			len = snprintf (q, size, "%s %s\n", pname, pvalue);
			q += len;
			if (size >= len) {
				size -= len;
			} else {
				free (*data);
				return false;
			}
		}
		*data_size -= size;
		return true;
	} else if (getcertbuf (name, &buf, &len)) {
		q = alloc (len);
		if (!q)
			return false;
		memcpy (q, buf, len);
		*data = q;
		*data_size = len;
		return true;
	}
	return false;
}

static void
FreeData (void *data)
{
	free (data);
}

static bool
RsaSign (char *key_name, void *data, UINT data_size, void *sign,
	 UINT *sign_buf_size)
{
	char *buf, *method;
	unsigned int len;

	if (!strcmp (key_name, RSAKEYV4_NAME))
		method = config_vpn.vpnAuthMethodV4;
	else if (!strcmp (key_name, RSAKEYV6_NAME))
		method = config_vpn.vpnAuthMethodV6;
	else
		return false;
	if (!strcmp (method, "Cert-IC"))
		return vpn_ic_rsa_sign (key_name, data, data_size, sign,
					sign_buf_size);
	if (getcertbuf (key_name, &buf, &len))
		return crypt_sys_rsa_sign (buf, len, data, data_size, sign,
					   sign_buf_size);
	return false;
}

static void
Log (char *type, char *message)
{
	switch (type ? type[0] : 0) {
	case 'I':		/* INFO */
		/* break; */
	case 'D':		/* DEBUG */
	case 'W':		/* WARNING */
	case 'E':		/* ERROR */
	case 'F':		/* FATAL */
	default:
		printf ("VPN Log [%s]: %s\n", type ? type : "NULL", message);
	}
}

static struct SE_SYSCALL_TABLE vpnsys = {
	.SysMemoryAlloc = MemoryAlloc,
	.SysMemoryReAlloc = MemoryReAlloc,
	.SysMemoryFree = MemoryFree,
	.SysGetCurrentCpuId = vpn_GetCurrentCpuId,
	.SysNewLock = NewLock,
	.SysLock = Lock,
	.SysUnlock = Unlock,
	.SysFreeLock = FreeLock,
	.SysGetTickCount = vpn_GetTickCount,
	.SysNewTimer = vpn_NewTimer,
	.SysSetTimer = vpn_SetTimer,
	.SysFreeTimer = vpn_FreeTimer,
	.SysGetPhysicalNicInfo = vpn_GetPhysicalNicInfo,
	.SysSendPhysicalNic = vpn_SendPhysicalNic,
	.SysSetPhysicalNicRecvCallback = vpn_SetPhysicalNicRecvCallback,
	.SysGetVirtualNicInfo = vpn_GetVirtualNicInfo,
	.SysSendVirtualNic = vpn_SendVirtualNic,
	.SysSetVirtualNicRecvCallback = vpn_SetVirtualNicRecvCallback,
	.SysSaveData = SaveData,
	.SysLoadData = LoadData,
	.SysFreeData = FreeData,
	.SysRsaSign = RsaSign,
	.SysLog = Log,
};

static void
callsub (int c, struct msgbuf *buf, int bufcnt)
{
	int r;

	for (;;) {
		r = msgsendbuf (vpnkernel_desc, c, buf, bufcnt);
		if (r == 0)
			break;
		panic ("vpn_user msgsendbuf failed (%d)", c);
	}
}

static void
set_cpu_id (UINT num)
{
	UINT *p;

	/* FIXME: saved to stack page...dirty code */
	p = (UINT *)(((unsigned long)&p) & ~0x3FFF);
	*p = num;
}

UINT
msg_vpn_GetCurrentCpuId (void)
{
	UINT *p;

	p = (UINT *)(((unsigned long)&p) & ~0x3FFF);
	return *p;
}

bool
msg_vpn_ic_rsa_sign (char *key_name, void *data, UINT data_size, void *sign,
		     UINT *sign_buf_size)
{
	struct vpnkernel_msg_ic_rsa_sign arg;
	struct msgbuf buf[4];

	arg.sign_buf_size = *sign_buf_size;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	setmsgbuf (&buf[1], key_name, strlen (key_name) + 1, 1);
	setmsgbuf (&buf[2], data, data_size, 1);
	setmsgbuf (&buf[3], sign, *sign_buf_size, 1);
	callsub (VPNKERNEL_MSG_IC_RSA_SIGN, buf, 4);
	*sign_buf_size = arg.sign_buf_size;
	set_cpu_id (arg.cpu);
	return arg.retval;
}

UINT
msg_vpn_GetTickCount (void)
{
	struct vpnkernel_msg_gettickcount arg;
	struct msgbuf buf[1];

	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (VPNKERNEL_MSG_GETTICKCOUNT, buf, 1);
	set_cpu_id (arg.cpu);
	return arg.retval;
}

void
msg_vpn_GetPhysicalNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct vpnkernel_msg_getphysicalnicinfo arg;
	struct vpnhandle *p = nic_handle;
	struct msgbuf buf[1];

	arg.handle = p->handle;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (VPNKERNEL_MSG_GETPHYSICALNICINFO, buf, 1);
	memcpy (info, &arg.info, sizeof *info);
	set_cpu_id (arg.cpu);
}

void
msg_vpn_SendPhysicalNic (SE_HANDLE nic_handle, UINT num_packets,
			 void **packets, UINT *packet_sizes)
{
	struct vpnkernel_msg_sendphysicalnic arg;
	struct vpnhandle *p = nic_handle;
	struct msgbuf *buf;
	UINT i;

	arg.handle = p->handle;
	arg.num_packets = num_packets;
	buf = alloc (sizeof *buf * (1 + num_packets));
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	for (i = 0; i < num_packets; i++) {
		setmsgbuf (&buf[1 + i], packets[i], packet_sizes[i], 1);
	}
	callsub (VPNKERNEL_MSG_SENDPHYSICALNIC, buf, num_packets + 1);
	free (buf);
	set_cpu_id (arg.cpu);
}

void
msg_vpn_SetPhysicalNicRecvCallback (SE_HANDLE nic_handle,
				    SE_SYS_CALLBACK_RECV_NIC *callback,
				    void *param)
{
	struct vpnkernel_msg_setphysicalnicrecvcallback arg;
	struct vpnhandle *p = nic_handle;
	struct msgbuf buf[1];

	p->recvphys_func = callback;
	p->recvphys_param = param;
	arg.handle = p->handle;
	arg.param = p;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (VPNKERNEL_MSG_SETPHYSICALNICRECVCALLBACK, buf, 1);
	set_cpu_id (arg.cpu);
}

void
msg_vpn_GetVirtualNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct vpnkernel_msg_getvirtualnicinfo arg;
	int *data = nic_handle;
	struct msgbuf buf[1];

	arg.handle = *data;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (VPNKERNEL_MSG_GETVIRTUALNICINFO, buf, 1);
	memcpy (info, &arg.info, sizeof *info);
	set_cpu_id (arg.cpu);
}

void
msg_vpn_SendVirtualNic (SE_HANDLE nic_handle, UINT num_packets, void **packets,
			UINT *packet_sizes)
{
	struct vpnkernel_msg_sendvirtualnic arg;
	int *data = nic_handle;
	struct msgbuf *buf;
	UINT i;

	arg.handle = *data;
	arg.num_packets = num_packets;
	buf = alloc (sizeof *buf * (1 + num_packets));
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	for (i = 0; i < num_packets; i++) {
		setmsgbuf (&buf[1 + i], packets[i], packet_sizes[i], 1);
	}
	callsub (VPNKERNEL_MSG_SENDVIRTUALNIC, buf, num_packets + 1);
	free (buf);
	set_cpu_id (arg.cpu);
}

void
msg_vpn_SetVirtualNicRecvCallback (SE_HANDLE nic_handle,
				   SE_SYS_CALLBACK_RECV_NIC *callback,
				   void *param)
{
	struct vpnkernel_msg_setvirtualnicrecvcallback arg;
	struct vpnhandle *p = nic_handle;
	struct msgbuf buf[1];

	p->recvvirt_func = callback;
	p->recvvirt_param = param;
	arg.handle = p->handle;
	arg.param = p;
	setmsgbuf (&buf[0], &arg, sizeof arg, 1);
	callsub (VPNKERNEL_MSG_SETVIRTUALNICRECVCALLBACK, buf, 1);
	set_cpu_id (arg.cpu);
}

SE_HANDLE
msg_vpn_NewTimer (SE_SYS_CALLBACK_TIMER *callback, void *param)
{
	struct vpn_timer *p;

	p = alloc (sizeof *p);
	p->callback = callback;
	p->param = param;
	p->enable = false;
	spinlock_lock (&timer_lock);
	p->next = vpn_timer_head;
	vpn_timer_head = p;
	spinlock_unlock (&timer_lock);
	return p;
}

void
msg_vpn_SetTimer (SE_HANDLE timer_handle, UINT interval)
{
	struct vpnkernel_msg_set_timer arg;
	struct msgbuf buf[1];
	struct vpn_timer *p = timer_handle;
	UINT now, first;
	bool set;

	now = vpn_GetTickCount ();
	spinlock_lock (&timer_lock);
	p->enable = false;
	p->settime = now;
	p->interval = first = interval;
	p->enable = true;
	set = true;
	for (p = vpn_timer_head; p; p = p->next) {
		if (p->enable) {
			if (p->interval <= (now - p->settime)) {
				set = false;
				break;
			}
			if (first > p->interval - (now - p->settime))
				first = p->interval - (now - p->settime);
		}
	}
	spinlock_unlock (&timer_lock);
	if (set) {
		arg.interval = first;
		setmsgbuf (&buf[0], &arg, sizeof arg, 1);
		callsub (VPNKERNEL_MSG_SET_TIMER, buf, 1);
		set_cpu_id (arg.cpu);
	}
}

void
msg_vpn_FreeTimer (SE_HANDLE timer_handle)
{
	struct vpn_timer **p, *q;

	spinlock_lock (&timer_lock);
	for (p = &vpn_timer_head; *p; p = &(*p)->next) {
		if (*p == timer_handle) {
			q = *p;
			*p = q->next;
			free (q);
			break;
		}
	}
	spinlock_unlock (&timer_lock);
}

static int
vpn_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	if (m != MSG_BUF)
		return -1;
	if (c == VPN_MSG_START) {
		struct vpn_msg_start *arg;
		struct vpnhandle *p;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		set_cpu_id (arg->cpu);
		p = alloc (sizeof *p);
		p->handle = arg->handle;
		p->recvphys_func = NULL;
		p->recvvirt_func = NULL;
		p->recvphys_param = NULL;
		p->recvvirt_param = NULL;
		arg->retval = vpn_start (p);
		return 0;
	} else if (c == VPN_MSG_PHYSICALNICRECV) {
		struct vpn_msg_physicalnicrecv *arg;
		struct vpnhandle *p;
		void **packets;
		UINT *packet_sizes;
		int i;

		if (bufcnt < 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		set_cpu_id (arg->cpu);
		if (bufcnt != 1 + arg->num_packets)
			return -1;
		if (arg->num_packets == 0)
			return 0;
		p = arg->param;
		packets = alloc (sizeof *packets * arg->num_packets);
		packet_sizes = alloc (sizeof *packet_sizes * arg->num_packets);
		for (i = 0; i < arg->num_packets; i++) {
			packets[i] = buf[i + 1].base;
			packet_sizes[i] = buf[i + 1].len;
		}
		p->recvphys_func (arg->nic_handle, arg->num_packets,
				  packets, packet_sizes, p->recvphys_param);
		free (packets);
		free (packet_sizes);
		return 0;
	} else if (c == VPN_MSG_VIRTUALNICRECV) {
		struct vpn_msg_virtualnicrecv *arg;
		struct vpnhandle *p;
		void **packets;
		UINT *packet_sizes;
		int i;

		if (bufcnt < 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		set_cpu_id (arg->cpu);
		if (bufcnt != 1 + arg->num_packets)
			return -1;
		if (arg->num_packets == 0)
			return 0;
		p = arg->param;
		packets = alloc (sizeof *packets * arg->num_packets);
		packet_sizes = alloc (sizeof *packet_sizes * arg->num_packets);
		for (i = 0; i < arg->num_packets; i++) {
			packets[i] = buf[i + 1].base;
			packet_sizes[i] = buf[i + 1].len;
		}
		p->recvvirt_func (arg->nic_handle, arg->num_packets,
				  packets, packet_sizes, p->recvvirt_param);
		free (packets);
		free (packet_sizes);
		return 0;
	} else if (c == VPN_MSG_TIMER) {
		struct vpn_msg_timer *arg;
		struct vpn_timer *p;
		SE_SYS_CALLBACK_TIMER *callback;
		void *param;
		struct vpnkernel_msg_set_timer arg2;
		struct msgbuf buf2[1];
		UINT diff, first = first;
		bool set;

		if (bufcnt != 1)
			return -1;
		if (buf[0].len != sizeof *arg)
			return -1;
		arg = buf[0].base;
		set_cpu_id (arg->cpu);
		goto timer_loop;
	timer_found:
		callback = p->callback;
		param = p->param;
		p->enable = false;
		spinlock_unlock (&timer_lock);
		callback (p, param);
	timer_loop:
		spinlock_lock (&timer_lock);
		set = false;
		for (p = vpn_timer_head; p; p = p->next) {
			if (p->enable) {
				if (p->interval <= (arg->now - p->settime))
					goto timer_found;
				diff = p->interval - (arg->now - p->settime);
				if (set && first <= diff)
					continue;
				set = true;
				first = diff;
			}
		}
		spinlock_unlock (&timer_lock);
		if (set) {
			arg2.interval = first;
			setmsgbuf (&buf2[0], &arg2, sizeof arg2, 1);
			callsub (VPNKERNEL_MSG_SET_TIMER, buf2, 1);
			set_cpu_id (arg2.cpu);
		}
		return 0;
	} else {
		return -1;
	}
}

static void
vpnsys_init (void)
{
	static bool initialized = false;

	if (initialized)
		return;
	initialized = true;
	VPN_IPsec_Init (&vpnsys, false);
	printf ("vpn init\n");
}

SE_HANDLE
vpn_start (void *data)
{
	vpnsys_init ();
	return VPN_IPsec_Client_Start (data, data, CONFIG_NAME);
}

void
vpn_user_init (struct config_data_vpn *vpn, char *seed, int len)
{
	void InitCryptoLibrary (char *, int);

#ifdef VPN_PD
	vpn_timer_head = NULL;
	spinlock_init (&timer_lock);
	vpnkernel_desc = msgopen ("vpnkernel");
	if (vpnkernel_desc < 0)
		panic ("open vpnkernel");
#endif /* VPN_PD */
	InitCryptoLibrary (seed, len);
	vpn_desc = msgregister ("vpn", vpn_msghandler);
	if (vpn_desc < 0)
		panic ("register vpn");
	memcpy (&config_vpn, vpn, sizeof *vpn);
}
