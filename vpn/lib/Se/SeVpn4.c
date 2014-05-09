/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008 
 *      National Institute of Information and Communications Technology
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

// SeVpn4.c
// 概要: VPN 処理 (IPv4)

#define SE_INTERNAL
#include <Se/Se.h>

// IPsec メインプロシージャ
void SeVpn4IPsecMainProc(SE_VPN4 *v4)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;

	if (v4->TimerCallback != NULL)
	{
		v4->TimerCallback(SeVpn4Tick(v4), v4->TimerCallbackParam);
	}

	if (Se4IsZeroIP(c->VpnPingTargetV4) == false)
	{
		if (SeVpn4DoInterval(v4, &v4->PingVar, c->VpnPingIntervalV4 * 1000))
		{
			SeVpn4IPsecSendPing(v4);
		}
	}
}

// VPN トンネル内で ping を送信する
void SeVpn4IPsecSendPing(SE_VPN4 *v4)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_IPV4_HEADER *ip;
	UINT ip_size;
	SE_BUF *icmp;
	UINT i, data_size;
	UCHAR *data;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;

	v4->icmp_id++;
	v4->icmp_seq_no++;

	data_size = c->VpnPingMsgSizeV4;

	data = SeMalloc(data_size);
	for (i = 0;i < data_size;i++)
	{
		data[i] = 'A' + (i % ('Z' - 'A'));
	}

	icmp = Se4BuildIcmpEchoPacket(SE_ICMPV4_TYPE_ECHO_REQUEST, 0,
		v4->icmp_id, v4->icmp_seq_no, data, data_size);

	SeFree(data);

	ip_size = sizeof(SE_IPV4_HEADER) + icmp->Size;
	ip = SeZeroMalloc(ip_size);
	SE_IPV4_SET_VERSION(ip, 4);
	SE_IPV4_SET_HEADER_LEN(ip, 5);
	ip->TotalLength = SeEndian16((USHORT)ip_size);
	ip->Identification = SeEndian16(v4->icmp_id);
	ip->TimeToLive = SE_IPV4_SEND_TTL;
	ip->Protocol = SE_IP_PROTO_ICMPV4;
	ip->SrcIP = Se4IPToUINT(c->GuestIpAddressV4);
	ip->DstIP = Se4IPToUINT(c->VpnPingTargetV4);
	ip->Checksum = Se4IpChecksum(ip, sizeof(SE_IPV4_HEADER));
	SeCopy(((UCHAR *)ip) + sizeof(SE_IPV4_HEADER), icmp->Buf, icmp->Size);

	if (v4->VirtualIpRecvCallback != NULL)
	{
		v4->VirtualIpRecvCallback(ip, ip_size, v4->VirtualIpRecvCallbackParam);
	}

	SeFreeBuf(icmp);
	SeFree(ip);
}

// IPsec 処理 (物理 ETH からの入力)
void SeVpn4IPsecInputFromPhysical(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_SEC *sec;
	SE_IKE_IP_ADDR dest_addr;
	SE_IKE_IP_ADDR src_addr;
	// 引数チェック
	if (v4 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;
	sec = v4->Sec;

	SeIkeInitIPv4Address(&dest_addr, &info->DestIpAddress);
	SeIkeInitIPv4Address(&src_addr, &info->SrcIpAddress);

	if (info->TypeL4 == SE_L4_UDPV4)
	{
		// UDPv4
		if (v4->UdpRecvCallback != NULL)
		{
			v4->UdpRecvCallback(&dest_addr, &src_addr,
				info->UDPv4Info->DstPort,
				info->UDPv4Info->SrcPort,
				info->UDPv4Info->Data,
				info->UDPv4Info->DataSize,
				v4->UdpRecvCallbackParam);
		}
	}
	else if (info->TypeL4 == SE_L4_ESPV4)
	{
		// ESPv4
		if (v4->EspRecvCallback != NULL)
		{
			v4->EspRecvCallback(&dest_addr, &src_addr,
				data, size,
				v4->EspRecvCallbackParam);
		}
	}
}

// IPsec 処理 (仮想 ETH からの入力)
void SeVpn4IPsecInputFromVirtual(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_SEC *sec;
	// 引数チェック
	if (v4 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;
	sec = v4->Sec;

	if (info->IsRawIpPacket == false)
	{
		// Raw IP パケットでない
		return;
	}

	if (c->AdjustTcpMssV4 != 0)
	{
		// TCP MSS の調整
		Se4AdjustTcpMss(data, size, c->AdjustTcpMssV4);
	}

	if (v4->VirtualIpRecvCallback != NULL)
	{
		v4->VirtualIpRecvCallback(data, size, v4->VirtualIpRecvCallbackParam);
	}
}

// IPsec 初期化
void SeVpn4InitIPsec(SE_VPN4 *v4)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_SEC_CONFIG conf;
	SE_SEC_CLIENT_FUNCTION_TABLE func_table;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;

	// IPsec モジュールの初期化
	SeVpn4InitSecConfig(v4, &conf);

	SeZero(&func_table, sizeof(func_table));
	func_table.ClientGetTick = SeVpn4ClientGetTick;
	func_table.ClientSetTimerCallback = SeVpn4ClientSetTimerCallback;
	func_table.ClientAddTimer = SeVpn4ClientAddTimer;
	func_table.ClientSetRecvUdpCallback = SeVpn4ClientSetRecvUdpCallback;
	func_table.ClientSetRecvEspCallback = SeVpn4ClientSetRecvEspCallback;
	func_table.ClientSetRecvVirtualIpCallback = SeVpn4ClientSetRecvVirtualIpCallback;
	func_table.ClientSendUdp = SeVpn4ClientSendUdp;
	func_table.ClientSendEsp = SeVpn4ClientSendEsp;
	func_table.ClientSendVirtualIp = SeVpn4ClientSendVirtualIp;

	v4->Sec = SeSecInit(v, false, &conf, &func_table, v4, false);
}

// IPsec 解放
void SeVpn4FreeIPsec(SE_VPN4 *v4)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;

	SeSecFree(v4->Sec);
}

// 現在時刻取得
UINT64 SeVpn4ClientGetTick(void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	// 引数チェック
	if (v4 == NULL)
	{
		return 0;
	}

	return SeVpn4Tick(v4);
}

// タイマコールバック設定
void SeVpn4ClientSetTimerCallback(SE_SEC_TIMER_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v4->TimerCallback = callback;
	v4->TimerCallbackParam = callback_param;
}

// タイマ追加
void SeVpn4ClientAddTimer(UINT interval, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	SeVpn4SetTimer(v4, interval);
}

// UDP パケット受信コールバック設定
void SeVpn4ClientSetRecvUdpCallback(SE_SEC_UDP_RECV_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v4->UdpRecvCallback = callback;
	v4->UdpRecvCallbackParam = callback_param;
}

// ESP パケット受信コールバック設定
void SeVpn4ClientSetRecvEspCallback(SE_SEC_ESP_RECV_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v4->EspRecvCallback = callback;
	v4->EspRecvCallbackParam = callback_param;
}

// 仮想 IP パケット受信コールバック設定
void SeVpn4ClientSetRecvVirtualIpCallback(SE_SEC_VIRTUAL_IP_RECV_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v4->VirtualIpRecvCallback = callback;
	v4->VirtualIpRecvCallbackParam = callback_param;
}

// UDP パケット送信
void SeVpn4ClientSendUdp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	SE_VPN *v;
	SE_IPV4 *ip;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;
	ip = v->IPv4_Physical;

	Se4SendUdp(ip, SeIkeGetIPv4Address(dest_addr), dest_port, SeIkeGetIPv4Address(src_addr), src_port, data, size, NULL);
}

// ESP パケット送信
void SeVpn4ClientSendEsp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	SE_VPN *v;
	SE_IPV4 *ip;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;
	ip = v->IPv4_Physical;

	Se4SendIp(ip, SeIkeGetIPv4Address(dest_addr), SeIkeGetIPv4Address(src_addr),
		SE_IP_PROTO_ESP, 0, data, size, NULL);
}

// 仮想 IP パケット送信
void SeVpn4ClientSendVirtualIp(void *data, UINT size, void *param)
{
	SE_VPN4 *v4 = (SE_VPN4 *)param;
	SE_VPN *v;
	SE_IPV4 *ip;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;
	ip = v->IPv4_Virtual;

	// IPv4 パケットである
	Se4SendRawIp(ip, SeClone(data, size), size, NULL);
}

// SE_SEC_CONFIG の初期化
void SeVpn4InitSecConfig(SE_VPN4 *v4, SE_SEC_CONFIG *c)
{
	SE_VPN_CONFIG *vc;
	// 引数チェック
	if (c == NULL || v4 == NULL)
	{
		return;
	}

	vc = v4->Vpn->Config;

	SeZero(c, sizeof(SE_SEC_CONFIG));

	SeIkeInitIPv4Address(&c->MyIpAddress, &v4->Vpn->IPv4_Physical->IpAddress);
	SeIkeInitIPv4Address(&c->MyVirtualIpAddress, &vc->GuestIpAddressV4);

	SeIkeInitIPv4Address(&c->VpnGatewayAddress, &vc->VpnGatewayAddressV4);
	c->VpnAuthMethod = vc->VpnAuthMethodV4;
	SeStrCpy(c->VpnPassword, sizeof(c->VpnPassword), vc->VpnPasswordV4);
	SeStrCpy(c->VpnIdString, sizeof(c->VpnIdString), vc->VpnIdStringV4);
	SeStrCpy(c->VpnCertName, sizeof(c->VpnCertName), vc->VpnCertNameV4);
	SeStrCpy(c->VpnCaCertName, sizeof(c->VpnCaCertName), vc->VpnCaCertNameV4);
	SeStrCpy(c->VpnRsaKeyName, sizeof(c->VpnRsaKeyName), vc->VpnRsaKeyNameV4);
	c->VpnPhase1Mode = vc->VpnPhase1ModeV4;
	c->VpnPhase1Crypto = vc->VpnPhase1CryptoV4;
	c->VpnPhase1Hash = vc->VpnPhase1HashV4;
	c->VpnPhase1LifeKilobytes = vc->VpnPhase1LifeKilobytesV4;
	c->VpnPhase1LifeSeconds = vc->VpnPhase1LifeSecondsV4;
	c->VpnWaitPhase2BlankSpan = vc->VpnWaitPhase2BlankSpanV4;
	c->VpnPhase2Crypto = vc->VpnPhase2CryptoV4;
	c->VpnPhase2Hash = vc->VpnPhase2HashV4;
	c->VpnPhase2LifeKilobytes = vc->VpnPhase2LifeKilobytesV4;
	c->VpnPhase2LifeSeconds = vc->VpnPhase2LifeSecondsV4;
	c->VpnConnectTimeout = vc->VpnConnectTimeoutV4;
	c->VpnIdleTimeout = vc->VpnIdleTimeoutV4;
	SeIkeInitIPv4Address(&c->VpnPingTarget, &vc->VpnPingTargetV4);
	c->VpnPingInterval = vc->VpnPingIntervalV4;
	c->VpnPingMsgSize = vc->VpnPingMsgSizeV4;
	c->VpnSpecifyIssuer = vc->VpnSpecifyIssuerV4;
}

// メインプロシージャ
void SeVpn4MainProc(SE_VPN4 *v4)
{
	SE_VPN *v;
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	v = v4->Vpn;

	if (v->Config->Mode == SE_CONFIG_MODE_L3IPSEC)
	{
		// IPsec メインプロシージャ
		SeVpn4IPsecMainProc(v4);
	}
}

// インターバル実行
bool SeVpn4DoInterval(SE_VPN4 *v4, UINT64 *var, UINT interval)
{
	// 引数チェック
	if (v4 == NULL)
	{
		return false;
	}

	return SeVpnDoInterval(v4->Vpn, var, interval);
}

// 現在時刻の取得
UINT64 SeVpn4Tick(SE_VPN4 *v4)
{
	// 引数チェック
	if (v4 == NULL)
	{
		return 0;
	}

	return SeVpnTick(v4->Vpn);
}

// タイマのセット
void SeVpn4SetTimer(SE_VPN4 *v4, UINT interval)
{
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	SeVpnAddTimer(v4->Vpn, interval);
}

// DHCP サーバー処理
void SeVpn4DhcpServer(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_DHCPV4_HEADER *dhcp;
	UINT tran_id;
	UCHAR *dhcp_data;
	UINT dhcp_data_size;
	bool ok = false;
	UINT magic_cookie = SeEndian32(SE_DHCPV4_MAGIC_COOKIE);
	SE_DHCPV4_OPTION_LIST opt;
	// 引数チェック
	if (v4 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;

	// パケットサイズの検査
	if (size < sizeof(SE_DHCPV4_HEADER))
	{
		return;
	}

	dhcp = (SE_DHCPV4_HEADER *)data;
	tran_id = SeEndian32(dhcp->TransactionId);

	dhcp_data = ((UCHAR *)dhcp) + sizeof(SE_DHCPV4_HEADER);
	dhcp_data_size = size - sizeof(SE_DHCPV4_HEADER);

	if (dhcp_data_size < 5)
	{
		// データサイズが小さすぎる
		return;
	}

	// Magic Cookie の検索
	while (dhcp_data_size >= 5)
	{
		if (SeCmp(dhcp_data, &magic_cookie, sizeof(UINT)) == 0)
		{
			// 発見
			dhcp_data_size -= sizeof(UINT);
			dhcp_data += sizeof(UINT);
			ok = true;
			break;
		}

		dhcp_data++;
		dhcp_data_size--;
	}

	if (ok == false)
	{
		// Magic Cookie の取得に失敗
		return;
	}

	// DHCP オプションリストのパース
	if (SeVpn4DhcpParseOptionList(&opt, dhcp_data, dhcp_data_size) == false)
	{
		// パケット不正
		return;
	}

	if (dhcp->OpCode == 1 && (opt.Opcode == SE_DHCPV4_DISCOVER || opt.Opcode == SE_DHCPV4_REQUEST))
	{
		// DHCP サーバーとしての動作を行う
		SE_IPV4_ADDR assign_ip = c->GuestIpAddressV4;
		SE_DHCPV4_OPTION_LIST ret;
		SE_LIST *o;
		char tmp1[MAX_PATH], tmp2[MAX_PATH];

		SeZero(&ret, sizeof(ret));

		ret.Opcode = (opt.Opcode == SE_DHCPV4_DISCOVER ? SE_DHCPV4_OFFER : SE_DHCPV4_ACK);
		if (ret.Opcode == SE_DHCPV4_ACK && Se4Cmp (opt.RequestedIp, assign_ip) != 0 && Se4Cmp (opt.RequestedIp, Se4UINTToIP (0)) != 0) {
			ret.Opcode = SE_DHCPV4_NACK;
		}
		ret.ServerAddress = p->IpAddress;
		ret.LeaseTime = c->DhcpLeaseExpiresV4;

		SeStrCpy(ret.DomainName, sizeof(ret.DomainName), c->DhcpDomainV4);
		ret.SubnetMask = p->SubnetMask;
		ret.DnsServer = c->DhcpDnsV4;
		ret.Gateway = p->IpAddress;

		ret.Mtu = c->GuestMtuV4;

		Se4IPToStr(tmp1, assign_ip);
		SeMacToStr(tmp2, sizeof(tmp2), dhcp->ClientMacAddress);
		if (ret.Opcode == SE_DHCPV4_ACK)
		{
			SeLog(SE_LOG_INFO, "DHCP Service Assigned %s to %s", tmp1, tmp2);
		}

		o = SeVpn4DhcpBuildDhcpOptionList(&ret);
		if (o != NULL)
		{
			SE_BUF *b = SeVpn4DhcpOptionsToBuf(o);

			if (b != NULL)
			{
				SE_IPV4_ADDR dest_ip = info->SrcIpAddress;
				UCHAR *dest_mac = dhcp->ClientMacAddress;

				if (Se4IsZeroIP(dest_ip))
				{
					dest_ip = Se4UINTToIP(0xffffffff);
				}

				// 送信
				SeVpn4DhcpSendResponse(p, tran_id, dest_ip, info->UDPv4Info->SrcPort,
					assign_ip, dest_mac, b);

				SeFreeBuf(b);
			}

			SeVpn4DhcpFreeOptionList(o);
		}
	}
}

// DHCP 応答パケットの送信
void SeVpn4DhcpSendResponse(SE_IPV4 *p, UINT tran_id, SE_IPV4_ADDR dest_ip, UINT dest_port,
							SE_IPV4_ADDR new_ip, UCHAR *mac_address, SE_BUF *b)
{
	UINT blank_size = 128 + 64;
	UINT dhcp_packet_size;
	UINT magic_cookie = SeEndian32(SE_DHCPV4_MAGIC_COOKIE);
	SE_DHCPV4_HEADER *dhcp;
	void *magic_cookie_addr;
	void *buffer_addr;
	// 引数チェック
	if (p == NULL || b == NULL)
	{
		return;
	}

	// DHCP パケットサイズを計算
	dhcp_packet_size = blank_size + sizeof(SE_DHCPV4_HEADER) + sizeof(magic_cookie) + b->Size;

	// ヘッダ作成
	dhcp = SeZeroMalloc(dhcp_packet_size);

	dhcp->OpCode = 2;
	dhcp->HardwareType = (UCHAR)SE_ARPV4_HARDWARE_TYPE_ETHERNET;
	dhcp->HardwareAddressSize = SE_ETHERNET_MAC_ADDR_SIZE;
	dhcp->TransactionId = SeEndian32(tran_id);
	dhcp->YourIP = Se4IPToUINT(new_ip);
	dhcp->ServerIP = Se4IPToUINT(p->IpAddress);
	SeCopy(dhcp->ClientMacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE);

	magic_cookie_addr = (((UCHAR *)dhcp) + sizeof(SE_DHCPV4_HEADER) + blank_size);
	buffer_addr = ((UCHAR *)magic_cookie_addr) + sizeof(magic_cookie);

	// Magic Cookie
	SeCopy(magic_cookie_addr, &magic_cookie, sizeof(magic_cookie));

	// データ本体
	SeCopy(buffer_addr, b->Buf, b->Size);

	// 送信
	Se4SendUdp(p, dest_ip, dest_port, p->IpAddress, SE_IPV4_DHCP_SERVER_PORT, dhcp, dhcp_packet_size, mac_address);

	SeFree(dhcp);
}

// DHCP オプションリストをバッファに変換する
SE_BUF *SeVpn4DhcpOptionsToBuf(SE_LIST *o)
{
	SE_BUF *b;
	UINT i;
	UCHAR id, sz;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();
	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_DHCPV4_OPTION *d = SE_LIST_DATA(o, i);

		id = (UCHAR)d->Id;
		sz = (UCHAR)d->Size;

		SeWriteBuf(b, &id, 1);
		SeWriteBuf(b, &sz, 1);
		SeWriteBuf(b, d->Data, d->Size);
	}

	id = 0xff;
	SeWriteBuf(b, &id, 1);

	SeSeekBuf(b, 0, 0);

	return b;
}

// DHCP オプションリストをビルドする
SE_LIST *SeVpn4DhcpBuildDhcpOptionList(SE_DHCPV4_OPTION_LIST *opt)
{
	SE_LIST *o;
	UCHAR opcode;
	UINT i;
	USHORT word;
	// 引数チェック
	if (opt == NULL)
	{
		return NULL;
	}

	o = SeNewList(NULL);

	// オペコード
	opcode = (UCHAR)opt->Opcode;
	SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_MESSAGE_TYPE, &opcode, sizeof(opcode)));

	// DHCP サーバーの IP アドレス
	i = Se4IPToUINT(opt->ServerAddress);
	SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_SERVER_ADDRESS, &i, sizeof(i)));

	// リース時間
	i = SeEndian32(opt->LeaseTime);
	SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_LEASE_TIME, &i, sizeof(i)));

	// ドメイン名
	if (SeIsEmptyStr(opt->DomainName) == false && Se4IsZeroIP(opt->DnsServer) == false)
	{
		SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_DOMAIN_NAME, opt->DomainName, SeStrLen(opt->DomainName)));
	}

	// サブネットマスク
	if (Se4IsZeroIP(opt->SubnetMask) == false)
	{
		i = Se4IPToUINT(opt->SubnetMask);
		SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_SUBNET_MASK, &i, sizeof(i)));
	}

	// デフォルトゲートウェイ
	if (Se4IsZeroIP(opt->Gateway) == false)
	{
		i = Se4IPToUINT(opt->Gateway);
		SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_GATEWAY_ADDR, &i, sizeof(i)));
	}

	// DNS サーバーアドレス
	if (Se4IsZeroIP(opt->DnsServer) == false)
	{
		i = Se4IPToUINT(opt->DnsServer);
		SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_DNS_ADDR, &i, sizeof(i)));
	}

	// MTU の値
	if (opt->Mtu != 0)
	{
		word = SeEndian16((USHORT)opt->Mtu);
		SeAdd(o, SeVpn4NewDhcpOption(SE_DHCPV4_ID_MTU, &word, sizeof(word)));
	}

	return o;
}

// 新しい DHCP オプション項目の作成
SE_DHCPV4_OPTION *SeVpn4NewDhcpOption(UINT id, void *data, UINT size)
{
	SE_DHCPV4_OPTION *ret;
	// 引数チェック
	if (size != 0 && data == NULL)
	{
		return NULL;
	}

	ret = SeZeroMalloc(sizeof(SE_DHCPV4_OPTION));
	ret->Data = SeClone(data, size);
	ret->Size = size;
	ret->Id = id;

	return ret;
}

// DHCP オプションリストのパース
bool SeVpn4DhcpParseOptionList(SE_DHCPV4_OPTION_LIST *opt, void *data, UINT size)
{
	// 引数チェック
	SE_LIST *o;
	SE_DHCPV4_OPTION *a;
	if (opt == NULL || data == NULL)
	{
		return false;
	}

	// リストのパース
	o = SeVpn4DhcpParseOptionListMain(data, size);
	if (o == NULL)
	{
		return false;
	}

	SeZero(opt, sizeof(SE_DHCPV4_OPTION_LIST));

	// オペコードの取得
	a = SeVpn4DhcpSearchOption(o, SE_DHCPV4_ID_MESSAGE_TYPE);
	if (a != NULL)
	{
		if (a->Size == 1)
		{
			opt->Opcode = *((UCHAR *)a->Data);
		}
	}

	switch (opt->Opcode)
	{
	case SE_DHCPV4_DISCOVER:
	case SE_DHCPV4_REQUEST:
		// 要求された IP アドレス
		a = SeVpn4DhcpSearchOption(o, SE_DHCPV4_ID_REQUEST_IP_ADDRESS);
		if (a != NULL && a->Size == 4)
		{
			UINT *ip = (UINT *)a->Data;

			opt->RequestedIp = Se4UINTToIP(*ip);
		}

		// ホスト名
		a = SeVpn4DhcpSearchOption(o, SE_DHCPV4_ID_HOST_NAME);
		if (a != NULL)
		{
			if (a->Size > 1)
			{
				SeCopy(opt->Hostname, a->Data, MIN(a->Size, sizeof(opt->Hostname) - 1));
			}
		}
		break;
	}

	// リスト解放
	SeVpn4DhcpFreeOptionList(o);

	return true;
}

// DHCP オプションの検索
SE_DHCPV4_OPTION *SeVpn4DhcpSearchOption(SE_LIST *o, UINT id)
{
	UINT i;
	SE_DHCPV4_OPTION *ret = NULL;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_DHCPV4_OPTION *opt = SE_LIST_DATA(o, i);

		if (opt->Id == id)
		{
			ret = opt;
		}
	}

	return ret;
}

// DHCP オプションリストの解放
void SeVpn4DhcpFreeOptionList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_DHCPV4_OPTION *opt = SE_LIST_DATA(o, i);

		SeFree(opt->Data);
		SeFree(opt);
	}

	SeFreeList(o);
}

// DHCP オプションリストのパースのメイン処理
SE_LIST *SeVpn4DhcpParseOptionListMain(void *data, UINT size)
{
	SE_LIST *o;
	SE_BUF *b;
	// 引数チェック
	if (data == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();
	SeWriteBuf(b, data, size);
	SeSeekBuf(b, 0, 0);

	o = SeNewList(NULL);

	while (true)
	{
		UCHAR c = 0, sz = 0;
		SE_DHCPV4_OPTION *opt;

		if (SeReadBuf(b, &c, 1) != 1)
		{
			break;
		}
		if (c == 0xff)
		{
			break;
		}
		if (SeReadBuf(b, &sz, 1) != 1)
		{
			break;
		}

		opt = SeZeroMalloc(sizeof(SE_DHCPV4_OPTION));
		opt->Id = (UINT)c;
		opt->Size = (UINT)sz;
		opt->Data = SeZeroMalloc(opt->Size);
		SeReadBuf(b, opt->Data, opt->Size);
		SeAdd(o, opt);
	}

	SeFreeBuf(b);

	return o;
}

// 物理 ETH からの IP パケットの受信
void SeVpn4RecvPacketPhysical(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v4 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;

	if (info->UnicastForMe == false)
	{
		return;
	}

	if (Se4Cmp(info->DestIpAddress, c->HostIpAddressV4) != 0)
	{
		return;
	}

	if (c->Mode == SE_CONFIG_MODE_L3TRANS)
	{
		// 仮想ネットワークに転送する
		Se4SendIp(v->IPv4_Virtual, info->DestIpAddress, info->SrcIpAddress,
			info->Protocol, info->Ttl, data, size, NULL);
	}
	else
	{
		// IPsec モジュールに解読してもらう
		SeVpn4IPsecInputFromPhysical(v4, p, info, data, size);
	}
}

// 仮想 ETH からの IP パケットの受信
void SeVpn4RecvPacketVirtual(SE_VPN4 *v4, SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	bool b = false;
	bool is_dhcp_client = false;
	// 引数チェック
	if (v4 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v4->Vpn;
	c = v->Config;

	// 送信元 IP アドレスがゲスト OS が使用すべき IP アドレスであるかどうか確認する
	if (Se4Cmp(info->SrcIpAddress, c->GuestIpAddressV4) == 0)
	{
		// ゲスト OS の IP アドレス
		b = true;

		if (info->TypeL4 == SE_L4_UDPV4 &&
			info->UDPv4Info->DstPort == SE_IPV4_DHCP_SERVER_PORT)
		{
			// DHCP パケットである
			is_dhcp_client = true;
		}
	}
	if (Se4Cmp(info->SrcIpAddress, Se4ZeroIP()) == 0)
	{
		if (info->TypeL4 == SE_L4_UDPV4 &&
			info->UDPv4Info->DstPort == SE_IPV4_DHCP_SERVER_PORT)
		{
			// 例外として DHCP クライアントが投げる IP アドレス
			// (送信元 IP が 0.0.0.0) は許可する
			b = true;
			is_dhcp_client = true;
		}
	}

	if (b == false)
	{
		// パケットを破棄する
		return;
	}

	if (is_dhcp_client)
	{
		if (info->IsRawIpPacket == false)
		{
			// DHCP クライアントからの要求パケットの処理
			SeVpn4DhcpServer(v4, p, info, info->UDPv4Info->Data, info->UDPv4Info->DataSize);
		}
	}
	else
	{
		// 通常のパケットの処理
		if (c->Mode == SE_CONFIG_MODE_L3TRANS)
		{
			if (info->IsRawIpPacket == false)
			{
				// 物理ネットワークに転送する
				Se4SendIp(v->IPv4_Physical, info->DestIpAddress, c->HostIpAddressV4,
					info->Protocol, info->Ttl, data, size, NULL);
			}
		}
		else
		{
			if (info->IsRawIpPacket)
			{
				// IPsec モジュールを用いて送信する
				SeVpn4IPsecInputFromVirtual(v4, p, info, data, size);
			}
		}
	}
}

// VPN の初期化
SE_VPN4 *SeVpn4Init(SE_VPN *vpn)
{
	SE_VPN4 *v4;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (vpn == NULL)
	{
		return NULL;
	}

	// 初期化
	v4 = SeZeroMalloc(sizeof(SE_VPN4));
	v4->Vpn = vpn;

	c = vpn->Config;

	// 仮想 LAN 側でプロキシ ARP を有効にする
	vpn->IPv4_Virtual->UseProxyArp = true;
	vpn->IPv4_Virtual->ProxyArpExceptionAddress = c->GuestIpAddressV4;

	// 仮想 LAN 側で Raw IP を有効にする
	vpn->IPv4_Virtual->UseRawIp = true;

	// IPsec 初期化
	if (c->Mode == SE_CONFIG_MODE_L3IPSEC)
	{
		SeVpn4InitIPsec(v4);
	}

	// メイン関数を 1 度だけ呼び出す
	SeVpn4MainProc(v4);

	return v4;
}

// VPN の解放
void SeVpn4Free(SE_VPN4 *v4)
{
	// 引数チェック
	if (v4 == NULL)
	{
		return;
	}

	// IPsec 解放
	if (v4->Vpn->Config->Mode == SE_CONFIG_MODE_L3IPSEC)
	{
		SeVpn4FreeIPsec(v4);
	}

	SeFree(v4);
}

