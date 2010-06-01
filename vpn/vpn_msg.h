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

enum {
	VPNKERNEL_MSG_IC_RSA_SIGN,
	VPNKERNEL_MSG_GETPHYSICALNICINFO,
	VPNKERNEL_MSG_SENDPHYSICALNIC,
	VPNKERNEL_MSG_SETPHYSICALNICRECVCALLBACK,
	VPNKERNEL_MSG_GETVIRTUALNICINFO,
	VPNKERNEL_MSG_SENDVIRTUALNIC,
	VPNKERNEL_MSG_SETVIRTUALNICRECVCALLBACK,
	VPNKERNEL_MSG_SET_TIMER,
	VPNKERNEL_MSG_GETTICKCOUNT,
};

enum {
	VPN_MSG_START,
	VPN_MSG_PHYSICALNICRECV,
	VPN_MSG_VIRTUALNICRECV,
	VPN_MSG_TIMER,
};

struct vpnkernel_msg_set_timer {
	UINT interval;
	UINT cpu;
};

struct vpnkernel_msg_getphysicalnicinfo {
	int handle;
	SE_NICINFO info;
	UINT cpu;
};

struct vpnkernel_msg_getvirtualnicinfo {
	int handle;
	SE_NICINFO info;
	UINT cpu;
};

struct vpnkernel_msg_sendphysicalnic {
	int handle;
	UINT num_packets;
	UINT cpu;
};

struct vpnkernel_msg_sendvirtualnic {
	int handle;
	UINT num_packets;
	UINT cpu;
};

struct vpnkernel_msg_setphysicalnicrecvcallback {
	int handle;
	void *param;
	UINT cpu;
};

struct vpnkernel_msg_setvirtualnicrecvcallback {
	int handle;
	void *param;
	UINT cpu;
};

struct vpnkernel_msg_ic_rsa_sign {
	UINT sign_buf_size;
	bool retval;
	UINT cpu;
};

struct vpnkernel_msg_gettickcount {
	UINT retval;
	UINT cpu;
};

struct vpn_msg_start {
	int handle;
	UINT cpu;
	SE_HANDLE retval;
};

struct vpn_msg_physicalnicrecv {
	SE_HANDLE nic_handle;
	void *param;
	UINT num_packets;
	UINT cpu;
};

struct vpn_msg_virtualnicrecv {
	SE_HANDLE nic_handle;
	void *param;
	UINT num_packets;
	UINT cpu;
};

struct vpn_msg_timer {
	UINT now;
	UINT cpu;
};

UINT vpn_GetCurrentCpuId (void);
UINT vpn_GetTickCount (void);
SE_HANDLE vpn_NewTimer (SE_SYS_CALLBACK_TIMER *callback, void *param);
void vpn_SetTimer (SE_HANDLE timer_handle, UINT interval);
void vpn_FreeTimer (SE_HANDLE timer_handle);
void vpn_GetPhysicalNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info);
void vpn_SendPhysicalNic (SE_HANDLE nic_handle, UINT num_packets,
			  void **packets, UINT *packet_sizes);
void vpn_SetPhysicalNicRecvCallback (SE_HANDLE nic_handle,
				     SE_SYS_CALLBACK_RECV_NIC *callback,
				     void *param);
void vpn_GetVirtualNicInfo (SE_HANDLE nic_handle, SE_NICINFO *info);
void vpn_SendVirtualNic (SE_HANDLE nic_handle, UINT num_packets,
			 void **packets, UINT *packet_sizes);
void vpn_SetVirtualNicRecvCallback (SE_HANDLE nic_handle,
				    SE_SYS_CALLBACK_RECV_NIC *callback,
				    void *param);
bool vpn_ic_rsa_sign (char *key_name, void *data, UINT data_size, void *sign,
		      UINT *sign_buf_size);
SE_HANDLE vpn_start (void *data);
