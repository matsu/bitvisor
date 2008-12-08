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

// Secure VM Project
// VPN Client Module (IPsec Driver) Source Code
// 
// Developed by Daiyuu Nobori (dnobori@cs.tsukuba.ac.jp)

// SeVpn6.h
// 概要: SeVpn6.c のヘッダ

#ifndef	SEVPN6_H
#define	SEVPN6_H

// インターフェイス
struct SE_VPN6
{
	SE_VPN *Vpn;
	SE_SEC *Sec;
	SE_IPV6_ADDR GuestIpAddress;
	SE_SEC_TIMER_CALLBACK *TimerCallback;
	void *TimerCallbackParam;
	SE_SEC_UDP_RECV_CALLBACK *UdpRecvCallback;
	void *UdpRecvCallbackParam;
	SE_SEC_ESP_RECV_CALLBACK *EspRecvCallback;
	void *EspRecvCallbackParam;
	SE_SEC_VIRTUAL_IP_RECV_CALLBACK *VirtualIpRecvCallback;
	void *VirtualIpRecvCallbackParam;
	UINT64 PingVar;
	USHORT icmp_id;
	USHORT icmp_seq_no;
	UINT icmp_ip_packet_id;
};


// 関数プロトタイプ
SE_VPN6 *SeVpn6Init(SE_VPN *vpn);
void SeVpn6Free(SE_VPN6 *v6);

void SeVpn6RecvPacketVirtual(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size);
void SeVpn6RecvPacketPhysical(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size);

void SeVpn6SetTimer(SE_VPN6 *v6, UINT interval);
UINT64 SeVpn6Tick(SE_VPN6 *v6);
bool SeVpn6DoInterval(SE_VPN6 *v6, UINT64 *var, UINT interval);

void SeVpn6MainProc(SE_VPN6 *v6);

void SeVpn6InitSecConfig(SE_VPN6 *v6, SE_SEC_CONFIG *c);
void SeVpn6InitIPsec(SE_VPN6 *v6);
void SeVpn6FreeIPsec(SE_VPN6 *v6);
void SeVpn6IPsecMainProc(SE_VPN6 *v6);

void SeVpn6IPsecSendPing(SE_VPN6 *v6);

void SeVpn6IPsecInputFromVirtual(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size);
void SeVpn6IPsecInputFromPhysical(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size);

UINT64 SeVpn6ClientGetTick(void *param);
void SeVpn6ClientSetTimerCallback(SE_SEC_TIMER_CALLBACK *callback, void *callback_param, void *param);
void SeVpn6ClientAddTimer(UINT interval, void *param);
void SeVpn6ClientSetRecvUdpCallback(SE_SEC_UDP_RECV_CALLBACK *callback, void *callback_param, void *param);
void SeVpn6ClientSetRecvEspCallback(SE_SEC_ESP_RECV_CALLBACK *callback, void *callback_param, void *param);
void SeVpn6ClientSetRecvVirtualIpCallback(SE_SEC_VIRTUAL_IP_RECV_CALLBACK *callback, void *callback_param, void *param);
void SeVpn6ClientSendUdp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param);
void SeVpn6ClientSendEsp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param);
void SeVpn6ClientSendVirtualIp(void *data, UINT size, void *param);

void SeVpn6UpdateGuestOsIpAddress(SE_VPN6 *v6, SE_IPV6_ADDR a);

#endif	// SEVPN6_H


