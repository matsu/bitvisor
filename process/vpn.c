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

#include <lib_mm.h>
#include <lib_printf.h>
#include <lib_string.h>
#include <lib_syscalls.h>

int heap[1048576], heaplen = 1048576;

unsigned int
vpn_GetCurrentCpuId (void)
{
	unsigned int msg_vpn_GetCurrentCpuId (void);

	return msg_vpn_GetCurrentCpuId ();
}

int
vpn_ic_rsa_sign (char *key_name, void *data, unsigned int data_size,
		 void *sign, unsigned int *sign_buf_size)
{
	int msg_vpn_ic_rsa_sign (char *key_name, void *data,
				 unsigned int data_size, void *sign,
				 unsigned int *sign_buf_size);

	return msg_vpn_ic_rsa_sign (key_name, data, data_size, sign,
				    sign_buf_size);
}

unsigned int
vpn_GetTickCount (void)
{
	unsigned int msg_vpn_GetTickCount (void);

	return msg_vpn_GetTickCount ();
}

void
vpn_GetPhysicalNicInfo (void *nic_handle, void *info)
{
	void msg_vpn_GetPhysicalNicInfo (void *nic_handle, void *info);

	return msg_vpn_GetPhysicalNicInfo (nic_handle, info);
}

void
vpn_SendPhysicalNic (void *nic_handle, unsigned int num_packets,
		     void **packets, unsigned int *packet_sizes)
{
	void msg_vpn_SendPhysicalNic (void *nic_handle,
				      unsigned int num_packets,
				      void **packets,
				      unsigned int *packet_sizes);

	return msg_vpn_SendPhysicalNic (nic_handle, num_packets, packets,
					packet_sizes);
}

void
vpn_SetPhysicalNicRecvCallback (void *nic_handle, void *callback, void *param)
{
	void msg_vpn_SetPhysicalNicRecvCallback (void *nic_handle,
						 void *callback, void *param);

	return msg_vpn_SetPhysicalNicRecvCallback (nic_handle, callback,
						   param);
}

void
vpn_GetVirtualNicInfo (void *nic_handle, void *info)
{
	void msg_vpn_GetVirtualNicInfo (void *nic_handle, void *info);

	return msg_vpn_GetVirtualNicInfo (nic_handle, info);
}

void
vpn_SendVirtualNic (void *nic_handle, unsigned int num_packets,
		    void **packets, unsigned int *packet_sizes)
{
	void msg_vpn_SendVirtualNic (void *nic_handle,
				     unsigned int num_packets, void **packets,
				     unsigned int *packet_sizes);

	return msg_vpn_SendVirtualNic (nic_handle, num_packets, packets,
				       packet_sizes);
}

void
vpn_SetVirtualNicRecvCallback (void *nic_handle, void *callback, void *param)
{
	void msg_vpn_SetVirtualNicRecvCallback (void *nic_handle,
						void *callback, void *param);

	return msg_vpn_SetVirtualNicRecvCallback (nic_handle, callback, param);
}

void *
vpn_NewTimer (void *callback, void *param)
{
	void *msg_vpn_NewTimer (void *callback, void *param);

	return msg_vpn_NewTimer (callback, param);
}

void
vpn_SetTimer (void *timer_handle, unsigned int interval)
{
	void msg_vpn_SetTimer (void *timer_handle, unsigned int interval);

	return msg_vpn_SetTimer (timer_handle, interval);
}

void
vpn_FreeTimer (void *timer_handle)
{
	void msg_vpn_FreeTimer (void *timer_handle);

	return msg_vpn_FreeTimer (timer_handle);
}

int
_start (int m, int c, struct msgbuf *buf, int bufcnt)
{
	void vpn_user_init (void *, char *, int);

	if (m != MSG_BUF)
		exitprocess (1);
	printf ("vpn (user) init\n");
	vpn_user_init (buf[0].base, buf[1].base, buf[1].len);
	if (setlimit (16384, 8 * 16384)) { /* stack must be aligned to 16KiB */
		printf ("vpn restrict failed\n");
		exitprocess (1);
	}
	return 0;
}
