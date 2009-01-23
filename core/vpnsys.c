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
#include "arith.h"
#include "asm.h"
#include "cpu.h"
#include "crypt.h"
#include "initfunc.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "time.h"
#include "timer.h"
#include "vpnsys.h"

struct nicdata {
	SE_HANDLE ph, vh;
	struct nicfunc func;
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
	return crypt_sys_save_data (name, data, data_size);
}

static bool
LoadData (char *name, void **data, UINT *data_size)
{
	return crypt_sys_load_data (name, data, data_size);
}

static void
FreeData (void *data)
{
	crypt_sys_free_data (data);
}

static bool
RsaSign (char *key_name, void *data, UINT data_size, void *sign,
	 UINT *sign_buf_size)
{
	return crypt_sys_rsa_sign (key_name, data, data_size, sign,
				   sign_buf_size);
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
	/* FIXME: bad randseed */
	static u8 randseed[6];
	int i;

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
	InitCryptoLibrary (&vpnsys, randseed, sizeof randseed);
	VPN_IPsec_Init (&vpnsys, false);
	printf ("vpnsys init\n");
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
	return VPN_IPsec_Client_Start (p, p, "config.txt");
}

INITFUNC ("driver0", vpnsys_init);
#endif /* CRYPTO_VPN */
