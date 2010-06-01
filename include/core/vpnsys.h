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

#ifndef __CORE_VPNSYS_H
#define __CORE_VPNSYS_H

#include <Se/Se.h>

struct nicfunc {
	void (*GetPhysicalNicInfo) (SE_HANDLE nic_handle, SE_NICINFO *info);
	void (*SendPhysicalNic) (SE_HANDLE nic_handle, UINT num_packets,
				 void **packets, UINT *packet_sizes);
	void (*SetPhysicalNicRecvCallback) (SE_HANDLE nic_handle,
					    SE_SYS_CALLBACK_RECV_NIC *callback,
					    void *param);
	void (*GetVirtualNicInfo) (SE_HANDLE nic_handle, SE_NICINFO *info);
	void (*SendVirtualNic) (SE_HANDLE nic_handle, UINT num_packets,
				void **packets, UINT *packet_sizes);
	void (*SetVirtualNicRecvCallback) (SE_HANDLE nic_handle,
					   SE_SYS_CALLBACK_RECV_NIC *callback,
					   void *param);
};	

SE_HANDLE vpn_new_nic (SE_HANDLE ph, SE_HANDLE vh, struct nicfunc *func);
void vpn_premap_PhysicalNicRecv (SE_SYS_CALLBACK_RECV_NIC *callback,
				 SE_HANDLE nic_handle, UINT num_packets,
				 void **packets, UINT *packet_sizes,
				 void *param, long *premap);
void vpn_premap_VirtualNicRecv (SE_SYS_CALLBACK_RECV_NIC *callback,
				SE_HANDLE nic_handle, UINT num_packets,
				void **packets, UINT *packet_sizes,
				void *param, long *premap);
long vpn_premap_recvbuf (void *buf, unsigned int len);

#endif
