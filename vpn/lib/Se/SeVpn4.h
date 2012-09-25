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

// SeVpn4.h
// 概要: SeVpn4.c のヘッダ

#ifndef	SEVPN4_H
#define	SEVPN4_H

// インターフェイス
struct SE_VPN4
{
	SE_VPN *Vpn;
	SE_SEC *Sec;
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
};


// 関数プロトタイプ
SE_VPN4 *SeVpn4Init(SE_VPN *vpn);
void SeVpn4Free(SE_VPN4 *v4);
void SeVpn4RecvPacketPhysical(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size);
void SeVpn4RecvPacketVirtual(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size);
void SeVpn4MainProc(SE_VPN4 *v4);
void SeVpn4SetTimer(SE_VPN4 *v4, UINT interval);
UINT64 SeVpn4Tick(SE_VPN4 *v4);
bool SeVpn4DoInterval(SE_VPN4 *v4, UINT64 *var, UINT interval);

void SeVpn4DhcpServer(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size);
bool SeVpn4DhcpParseOptionList(SE_DHCPV4_OPTION_LIST *opt, void *data, UINT size);
SE_LIST *SeVpn4DhcpParseOptionListMain(void *data, UINT size);
void SeVpn4DhcpFreeOptionList(SE_LIST *o);
SE_DHCPV4_OPTION *SeVpn4DhcpSearchOption(SE_LIST *o, UINT id);
SE_DHCPV4_OPTION *SeVpn4NewDhcpOption(UINT id, void *data, UINT size);
SE_LIST *SeVpn4DhcpBuildDhcpOptionList(SE_DHCPV4_OPTION_LIST *opt);
SE_BUF *SeVpn4DhcpOptionsToBuf(SE_LIST *o);
void SeVpn4DhcpSendResponse(SE_IPV4 *p, UINT tran_id, SE_IPV4_ADDR dest_ip, UINT dest_port,
							SE_IPV4_ADDR new_ip, UCHAR *mac_address, SE_BUF *b);

void SeVpn4InitIPsec(SE_VPN4 *v4);
void SeVpn4FreeIPsec(SE_VPN4 *v4);
void SeVpn4IPsecMainProc(SE_VPN4 *v4);
void SeVpn4IPsecInputFromPhysical(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size);
void SeVpn4IPsecInputFromVirtual(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size);

void SeVpn4IPsecSendPing(SE_VPN4 *v4);

void SeVpn4InitSecConfig(SE_VPN4 *v4, SE_SEC_CONFIG *c);

UINT64 SeVpn4ClientGetTick(void *param);
void SeVpn4ClientSetTimerCallback(SE_SEC_TIMER_CALLBACK *callback, void *callback_param, void *param);
void SeVpn4ClientAddTimer(UINT interval, void *param);
void SeVpn4ClientSetRecvUdpCallback(SE_SEC_UDP_RECV_CALLBACK *callback, void *callback_param, void *param);
void SeVpn4ClientSetRecvEspCallback(SE_SEC_ESP_RECV_CALLBACK *callback, void *callback_param, void *param);
void SeVpn4ClientSetRecvVirtualIpCallback(SE_SEC_VIRTUAL_IP_RECV_CALLBACK *callback, void *callback_param, void *param);
void SeVpn4ClientSendUdp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param);
void SeVpn4ClientSendEsp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param);
void SeVpn4ClientSendVirtualIp(void *data, UINT size, void *param);

#endif	// SEVPN4_H
