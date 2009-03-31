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

#ifdef CRYPTO_VPN
#include <IDMan.h>
#include "arith.h"
#include "asm.h"
#include "config.h"
#include "cpu.h"
#include "crypt.h"
#include "iccard.h"
#include "initfunc.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "time.h"
#include "timer.h"
#include "vpnsys.h"

#define CONFIG_NAME "config.txt"
#define CERTV4_NAME "certnamev4"
#define CACERTV4_NAME "cacertnamev4"
#define RSAKEYV4_NAME "rsakeynamev4"
#define CERTV6_NAME "certnamev6"
#define CACERTV6_NAME "cacertnamev6"
#define RSAKEYV6_NAME "rsakeynamev6"
#define VPNAUTHMETHOD_CERTIC "Cert-IC"
#define VPNAUTHMETHOD_CERT "Cert"

struct nicdata {
	SE_HANDLE ph, vh;
	struct nicfunc func;
};

struct vpnconfig {
	char *name;
	char *value;
};

static char vpnauthmethodv4[] = "VpnAuthMethodV4";
static char vpnauthmethodv6[] = "VpnAuthMethodV6";
static char c_vpnrsakeynamev4[] = "#VpnRsaKeyNameV4";
static char c_vpnrsakeynamev6[] = "#VpnRsaKeyNameV6";

static struct vpnconfig vpncfg[] = {
	{ "Mode", config.vpn.mode },
	{ "VirtualGatewayMacAddress", config.vpn.virtualGatewayMacAddress },
	{ "BindV4", config.vpn.bindV4 },
	{ "GuestIpAddressV4", config.vpn.guestIpAddressV4 },
	{ "GuestIpSubnetV4", config.vpn.guestIpSubnetV4 },
	{ "GuestMtuV4", config.vpn.guestMtuV4 },
	{ "GuestVirtualGatewayIpAddressV4", config.vpn.guestVirtualGatewayIpAddressV4 },
	{ "DhcpV4", config.vpn.dhcpV4 },
	{ "DhcpLeaseExpiresV4", config.vpn.dhcpLeaseExpiresV4 },
	{ "DhcpDnsV4", config.vpn.dhcpDnsV4 },
	{ "DhcpDomainV4", config.vpn.dhcpDomainV4 },
	{ "AdjustTcpMssV4", config.vpn.adjustTcpMssV4 },
	{ "HostIpAddressV4", config.vpn.hostIpAddressV4 },
	{ "HostIpSubnetV4", config.vpn.hostIpSubnetV4 },
	{ "HostMtuV4", config.vpn.hostMtuV4 },
	{ "HostIpDefaultGatewayV4", config.vpn.hostIpDefaultGatewayV4 },
	{ "OptionV4ArpExpires", config.vpn.optionV4ArpExpires },
	{ "OptionV4ArpDontUpdateExpires", config.vpn.optionV4ArpDontUpdateExpires },
	{ "VpnGatewayAddressV4", config.vpn.vpnGatewayAddressV4 },
	{ vpnauthmethodv4, config.vpn.vpnAuthMethodV4 },
	{ "VpnPasswordV4", config.vpn.vpnPasswordV4 },
	{ "VpnIdStringV4", config.vpn.vpnIdStringV4 },
	{ "#VpnCertNameV4", CERTV4_NAME },
	{ "#VpnCaCertNameV4", CACERTV4_NAME },
	{ c_vpnrsakeynamev4, RSAKEYV4_NAME },
	{ "VpnSpecifyIssuerV4", config.vpn.vpnSpecifyIssuerV4 },
	{ "VpnPhase1CryptoV4", config.vpn.vpnPhase1CryptoV4 },
	{ "VpnPhase1HashV4", config.vpn.vpnPhase1HashV4 },
	{ "VpnPhase1LifeSecondsV4", config.vpn.vpnPhase1LifeSecondsV4 },
	{ "VpnPhase1LifeKilobytesV4", config.vpn.vpnPhase1LifeKilobytesV4 },
	{ "VpnWaitPhase2BlankSpanV4", config.vpn.vpnWaitPhase2BlankSpanV4 },
	{ "VpnPhase2CryptoV4", config.vpn.vpnPhase2CryptoV4 },
	{ "VpnPhase2HashV4", config.vpn.vpnPhase2HashV4 },
	{ "VpnPhase2LifeSecondsV4", config.vpn.vpnPhase2LifeSecondsV4 },
	{ "VpnPhase2LifeKilobytesV4", config.vpn.vpnPhase2LifeKilobytesV4 },
	{ "VpnConnectTimeoutV4", config.vpn.vpnConnectTimeoutV4 },
	{ "VpnIdleTimeoutV4", config.vpn.vpnIdleTimeoutV4 },
	{ "VpnPingTargetV4", config.vpn.vpnPingTargetV4 },
	{ "VpnPingIntervalV4", config.vpn.vpnPingIntervalV4 },
	{ "VpnPingMsgSizeV4", config.vpn.vpnPingMsgSizeV4 },
	{ "BindV6", config.vpn.bindV6 },
	{ "GuestIpAddressPrefixV6", config.vpn.guestIpAddressPrefixV6 },
	{ "GuestIpAddressSubnetV6", config.vpn.guestIpAddressSubnetV6 },
	{ "GuestMtuV6", config.vpn.guestMtuV6 },
	{ "GuestVirtualGatewayIpAddressV6", config.vpn.guestVirtualGatewayIpAddressV6 },
	{ "RaV6", config.vpn.raV6 },
	{ "RaLifetimeV6", config.vpn.raLifetimeV6 },
	{ "RaDnsV6", config.vpn.raDnsV6 },
	{ "HostIpAddressV6", config.vpn.hostIpAddressV6 },
	{ "HostIpAddressSubnetV6", config.vpn.hostIpAddressSubnetV6 },
	{ "HostMtuV6", config.vpn.hostMtuV6 },
	{ "HostIpDefaultGatewayV6", config.vpn.hostIpDefaultGatewayV6 },
	{ "OptionV6NeighborExpires", config.vpn.optionV6NeighborExpires },
	{ "VpnGatewayAddressV6", config.vpn.vpnGatewayAddressV6 },
	{ vpnauthmethodv6, config.vpn.vpnAuthMethodV6 },
	{ "VpnPasswordV6", config.vpn.vpnPasswordV6 },
	{ "VpnIdStringV6", config.vpn.vpnIdStringV6 },
	{ "#VpnCertNameV6", CERTV6_NAME },
	{ "#VpnCaCertNameV6", CACERTV6_NAME },
	{ c_vpnrsakeynamev6, RSAKEYV6_NAME },
	{ "VpnSpecifyIssuerV6", config.vpn.vpnSpecifyIssuerV6 },
	{ "VpnPhase1CryptoV6", config.vpn.vpnPhase1CryptoV6 },
	{ "VpnPhase1HashV6", config.vpn.vpnPhase1HashV6 },
	{ "VpnPhase1LifeSecondsV6", config.vpn.vpnPhase1LifeSecondsV6 },
	{ "VpnPhase1LifeKilobytesV6", config.vpn.vpnPhase1LifeKilobytesV6 },
	{ "VpnWaitPhase2BlankSpanV6", config.vpn.vpnWaitPhase2BlankSpanV6 },
	{ "VpnPhase2CryptoV6", config.vpn.vpnPhase2CryptoV6 },
	{ "VpnPhase2HashV6", config.vpn.vpnPhase2HashV6 },
	{ "VpnPhase2LifeSecondsV6", config.vpn.vpnPhase2LifeSecondsV6 },
	{ "VpnPhase2LifeKilobytesV6", config.vpn.vpnPhase2LifeKilobytesV6 },
	{ "VpnPhase2StrictIdV6", config.vpn.vpnPhase2StrictIdV6 },
	{ "VpnConnectTimeoutV6", config.vpn.vpnConnectTimeoutV6 },
	{ "VpnIdleTimeoutV6", config.vpn.vpnIdleTimeoutV6 },
	{ "VpnPingTargetV6", config.vpn.vpnPingTargetV6 },
	{ "VpnPingIntervalV6", config.vpn.vpnPingIntervalV6 },
	{ "VpnPingMsgSizeV6", config.vpn.vpnPingMsgSizeV6 },
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

static UINT
GetCurrentCpuId (void)
{
	return get_cpu_id ();
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

static UINT
GetTickCount (void)
{
	u64 d[2], q[2];

	d[0] = get_cpu_time ();
	d[1] = 0;
	mpudiv_128_32 (d, 1000, q);
	return (UINT)q[0];
}

static SE_HANDLE
NewTimer (SE_SYS_CALLBACK_TIMER *callback, void *param)
{
	void *p;

	p = timer_new (callback, param);
	if (!p)
		panic ("NewTimer failed");
	return p;
}

static void
SetTimer (SE_HANDLE timer_handle, UINT interval)
{
	u64 usec;

	usec = interval;
	usec *= 1000;
	timer_set (timer_handle, usec);
}

static void
FreeTimer (SE_HANDLE timer_handle)
{
	timer_free (timer_handle);
}

static void
GetPhysicalNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct nicdata *nic = nic_handle;

	nic->func.GetPhysicalNicInfo (nic->ph, info);
}

static void
SendPhysicalNic (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		 UINT *packet_sizes)
{
	struct nicdata *nic = nic_handle;

	nic->func.SendPhysicalNic (nic->ph, num_packets, packets,
				   packet_sizes);
}

static void
SetPhysicalNicRecvCallback (SE_HANDLE nic_handle,
			    SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	struct nicdata *nic = nic_handle;

	nic->func.SetPhysicalNicRecvCallback (nic->ph, callback, param);
}

static void
GetVirtualNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info)
{
	struct nicdata *nic = nic_handle;

	nic->func.GetVirtualNicInfo (nic->vh, info);
}

static void
SendVirtualNic (SE_HANDLE nic_handle, UINT num_packets, void **packets,
		UINT *packet_sizes)
{
	struct nicdata *nic = nic_handle;

	nic->func.SendVirtualNic (nic->vh, num_packets, packets, packet_sizes);
}

static void
SetVirtualNicRecvCallback (SE_HANDLE nic_handle,
			   SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	struct nicdata *nic = nic_handle;

	nic->func.SetVirtualNicRecvCallback (nic->vh, callback, param);
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
		*buf = config.vpn.vpnCertV4;
		*len = sizeof config.vpn.vpnCertV4;
	} else if (!strcmp (name, CACERTV4_NAME)) {
		*buf = config.vpn.vpnCaCertV4;
		*len = sizeof config.vpn.vpnCaCertV4;
	} else if (!strcmp (name, RSAKEYV4_NAME)) {
		*buf = config.vpn.vpnRsaKeyV4;
		*len = sizeof config.vpn.vpnRsaKeyV4;
	} else if (!strcmp (name, CERTV6_NAME)) {
		*buf = config.vpn.vpnCertV6;
		*len = sizeof config.vpn.vpnCertV6;
	} else if (!strcmp (name, CACERTV6_NAME)) {
		*buf = config.vpn.vpnCaCertV6;
		*len = sizeof config.vpn.vpnCaCertV6;
	} else if (!strcmp (name, RSAKEYV6_NAME)) {
		*buf = config.vpn.vpnRsaKeyV6;
		*len = sizeof config.vpn.vpnRsaKeyV6;
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
					method = config.vpn.vpnAuthMethodV4;
				else if (pname == c_vpnrsakeynamev6)
					method = config.vpn.vpnAuthMethodV6;
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
	int idman_ret;
	unsigned long idman_session;
	unsigned short int signlen;

	if (!strcmp (key_name, RSAKEYV4_NAME))
		method = config.vpn.vpnAuthMethodV4;
	else if (!strcmp (key_name, RSAKEYV6_NAME))
		method = config.vpn.vpnAuthMethodV6;
	else
		return false;
	if (!strcmp (method, "Cert-IC")) {
#ifdef IDMAN
		if (!get_idman_session (&idman_session)) {
			printf ("RsaSign: card reader not ready\n");
			return false;
		}
		idman_ret = IDMan_EncryptByIndex (idman_session, 2, data,
						  data_size, sign, &signlen,
						  0x220);
		if (!idman_ret) {
			*sign_buf_size = signlen;
			return true;
		}
#endif
		printf ("RsaSign: IDMan_EncryptByIndex()"
			" ret=%d data_size=%d\n", idman_ret, data_size);
		return false;
	}
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
	.SysGetCurrentCpuId = GetCurrentCpuId,
	.SysNewLock = NewLock,
	.SysLock = Lock,
	.SysUnlock = Unlock,
	.SysFreeLock = FreeLock,
	.SysGetTickCount = GetTickCount,
	.SysNewTimer = NewTimer,
	.SysSetTimer = SetTimer,
	.SysFreeTimer = FreeTimer,
	.SysGetPhysicalNicInfo = GetPhysicalNicInfo,
	.SysSendPhysicalNic = SendPhysicalNic,
	.SysSetPhysicalNicRecvCallback = SetPhysicalNicRecvCallback,
	.SysGetVirtualNicInfo = GetVirtualNicInfo,
	.SysSendVirtualNic = SendVirtualNic,
	.SysSetVirtualNicRecvCallback = SetVirtualNicRecvCallback,
	.SysSaveData = SaveData,
	.SysLoadData = LoadData,
	.SysFreeData = FreeData,
	.SysRsaSign = RsaSign,
	.SysLog = Log,
};

static void
get_randseed (void **seed, int *seedlen)
{
	static u8 randseed[6];
	int i;

	*seed = config.vmm.randomSeed;
	*seedlen = sizeof config.vmm.randomSeed;
	for (i = 0; i < sizeof config.vmm.randomSeed; i++)
		if (config.vmm.randomSeed[i])
			return;
	/* FIXME: bad randseed */
	for (i = 0; i < 6; i++) {
		asm_outb (0x70, "\0\2\4\7\10\11"[i]);
		asm_inb (0x71, &randseed[i]);
	}
	*seed = randseed;
	*seedlen = sizeof randseed;
}

static void
vpnsys_init (void)
{
	void *randseed;
	int randseedlen;
	static bool initialized = false;

	if (initialized)
		return;
	initialized = true;
	get_randseed (&randseed, &randseedlen);
	InitCryptoLibrary (&vpnsys, randseed, randseedlen);
	VPN_IPsec_Init (&vpnsys, false);
	printf ("vpnsys init\n");
}

static void
vpnsys_init_global (void)
{
	preinit_crypto_library (&vpnsys);
}

#ifdef VPN_PASS_MODE
struct passdata {
	SE_HANDLE handle;
	void (*send_func) (SE_HANDLE nic_handle, UINT num_packets,
			   void **packets, UINT *packet_sizes);
};

static void
recv_pass (SE_HANDLE nic_handle, UINT num_packets, void **packets,
	   UINT *packet_sizes, void *param)
{
	struct passdata *p = param;

	p->send_func (p->handle, num_packets, packets, packet_sizes);
}

static void
pass_init (SE_HANDLE ph, SE_HANDLE vh, struct nicfunc *func)
{
	struct passdata *p, *v;

	p = alloc (sizeof *p);
	v = alloc (sizeof *v);
	p->handle = ph;
	p->send_func = func->SendPhysicalNic;
	v->handle = vh;
	v->send_func = func->SendVirtualNic;
	func->SetPhysicalNicRecvCallback (ph, recv_pass, v);
	func->SetVirtualNicRecvCallback (vh, recv_pass, p);
}
#endif /* VPN_PASS_MODE */

SE_HANDLE
vpn_new_nic (SE_HANDLE ph, SE_HANDLE vh, struct nicfunc *func)
{
	struct nicdata *p;

#ifdef VPN_PASS_MODE
	pass_init (ph, vh, func);
	return "PASS MODE";
#endif /* VPN_PASS_MODE */
	vpnsys_init ();
	p = alloc (sizeof *p);
	p->ph = ph;
	p->vh = vh;
	memcpy (&p->func, func, sizeof *func);
	return VPN_IPsec_Client_Start (p, p, CONFIG_NAME);
}

INITFUNC ("global4", vpnsys_init_global);
INITFUNC ("driver0", vpnsys_init);
#endif /* CRYPTO_VPN */
