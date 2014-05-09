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

// SeVpn6.c
// 概要: VPN 処理 (IPv6)

#define SE_INTERNAL
#include <Se/Se.h>

// IPsec メインプロシージャ
void SeVpn6IPsecMainProc(SE_VPN6 *v6)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;

	if (v6->TimerCallback != NULL)
	{
		v6->TimerCallback(SeVpn6Tick(v6), v6->TimerCallbackParam);
	}

	if (Se6IsZeroIP(c->VpnPingTargetV6) == false)
	{
		if (SeVpn6DoInterval(v6, &v6->PingVar, c->VpnPingIntervalV6 * 1000))
		{
			SeVpn6IPsecSendPing(v6);
		}
	}
}

// VPN 内における ping の送信
void SeVpn6IPsecSendPing(SE_VPN6 *v6)
{
	SE_VPN *v;
	SE_BUF *icmp_buf;
	SE_VPN_CONFIG *c;
	UINT i, data_size;
	UCHAR *data;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;

	if (Se6IsZeroIP(v6->GuestIpAddress))
	{
		return;
	}

	data_size = c->VpnPingMsgSizeV6;

	data = SeMalloc(data_size);
	for (i = 0;i < data_size;i++)
	{
		data[i] = 'A' + (i % ('Z' - 'A'));
	}

	icmp_buf = Se6BuildIcmpEchoPacket(v6->GuestIpAddress, c->VpnPingTargetV6,
		SE_ICMPV6_TYPE_ECHO_REQUEST, 0,
		v6->icmp_id++, v6->icmp_seq_no++, data, data_size);

	if (icmp_buf != NULL)
	{
		SE_IPV6_HEADER_PACKET_INFO info;
		SE_IPV6_HEADER ip_header;
		SE_LIST *o;
		UINT i;

		// IPv6 ヘッダ
		SeZero(&ip_header, sizeof(ip_header));
		SE_IPV6_SET_VERSION(&ip_header, 6);
		ip_header.HopLimit = SE_IPV6_HOP_DEFAULT;
		ip_header.SrcAddress = v6->GuestIpAddress;
		ip_header.DestAddress = c->VpnPingTargetV6;

		// IPv6 パケットの構築
		SeZero(&info, sizeof(info));
		info.Protocol = SE_IP_PROTO_ICMPV6;
		info.IPv6Header = &ip_header;
		info.Payload = icmp_buf->Buf;
		info.PayloadSize = icmp_buf->Size;

		o = SeBuildIPv6PacketWithFragment(&info, v6->icmp_ip_packet_id++,
			v->IPv6_Virtual->Mtu);

		for (i = 0;i < SE_LIST_NUM(o);i++)
		{
			SE_BUF *buf = SE_LIST_DATA(o, i);

			if (v6->VirtualIpRecvCallback != NULL)
			{
				v6->VirtualIpRecvCallback(buf->Buf, buf->Size, v6->VirtualIpRecvCallbackParam);
			}
		}

		SeFreePacketList(o);
	}

	SeFreeBuf(icmp_buf);

	SeFree(data);
}

// ゲスト OS が使用していると推定される IPv6 アドレスの更新
void SeVpn6UpdateGuestOsIpAddress(SE_VPN6 *v6, SE_IPV6_ADDR a)
{
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	if (Se6Cmp(a, v6->GuestIpAddress) != 0)
	{
		if (Se6IsZeroIP(a) == false)
		{
			// IP アドレスが変更された
			char tmp[MAX_PATH];

			v6->GuestIpAddress = a;
			Se6IPToStr(tmp, a);
			SeInfo("IPv6: GuestOS IP: %s", tmp);

			if (v6->Sec != NULL)
			{
				SeIkeInitIPv6Address(&v6->Sec->Config.MyVirtualIpAddress, &v6->GuestIpAddress);

				if (v6->Vpn->Config->VpnPhase2StrictIdV6)
				{
					SeSecForceReconnect(v6->Sec);
				}

				SeVpnStatusChanged(v6->Vpn);
			}
		}
	}
}

// IPsec 処理 (物理 ETH からの入力)
void SeVpn6IPsecInputFromPhysical(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_SEC *sec;
	SE_IKE_IP_ADDR dest_addr;
	SE_IKE_IP_ADDR src_addr;
	// 引数チェック
	if (v6 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;
	sec = v6->Sec;

	SeIkeInitIPv6Address(&dest_addr, &info->DestIpAddress);
	SeIkeInitIPv6Address(&src_addr, &info->SrcIpAddress);

	if (info->TypeL4 == SE_L4_UDPV6)
	{
		// UDPv6
		if (v6->UdpRecvCallback != NULL)
		{
			v6->UdpRecvCallback(&dest_addr, &src_addr,
				info->UDPv6Info->DstPort,
				info->UDPv6Info->SrcPort,
				info->UDPv6Info->Data,
				info->UDPv6Info->DataSize,
				v6->UdpRecvCallbackParam);
		}
	}
	else if (info->TypeL4 == SE_L4_ESPV6)
	{
		// ESPv6
		if (v6->EspRecvCallback != NULL)
		{
			v6->EspRecvCallback(&dest_addr, &src_addr,
				data, size,
				v6->EspRecvCallbackParam);
		}
	}
}

// IPsec 処理 (仮想 ETH からの入力)
void SeVpn6IPsecInputFromVirtual(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_SEC *sec;
	// 引数チェック
	if (v6 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;
	sec = v6->Sec;

	if (info->IsRawIpPacket == false)
	{
		// Raw IP パケットでない
		return;
	}

	if (v6->VirtualIpRecvCallback != NULL)
	{
		v6->VirtualIpRecvCallback(data, size, v6->VirtualIpRecvCallbackParam);
	}
}

// SE_SEC_CONFIG の初期化
void SeVpn6InitSecConfig(SE_VPN6 *v6, SE_SEC_CONFIG *c)
{
	SE_VPN_CONFIG *vc;
	SE_IPV6_ADDR zero_ip = Se6ZeroIP();
	// 引数チェック
	if (v6 == NULL || c == NULL)
	{
		return;
	}

	vc = v6->Vpn->Config;

	SeZero(c, sizeof(SE_SEC_CONFIG));

	SeIkeInitIPv6Address(&c->MyIpAddress, &v6->Vpn->IPv6_Physical->GlobalIpAddress);
	SeIkeInitIPv6Address(&c->MyVirtualIpAddress, &zero_ip);

	SeIkeInitIPv6Address(&c->VpnGatewayAddress, &vc->VpnGatewayAddressV6);
	c->VpnAuthMethod = vc->VpnAuthMethodV6;
	SeStrCpy(c->VpnPassword, sizeof(c->VpnPassword), vc->VpnPasswordV6);
	SeStrCpy(c->VpnIdString, sizeof(c->VpnIdString), vc->VpnIdStringV6);
	SeStrCpy(c->VpnCertName, sizeof(c->VpnCertName), vc->VpnCertNameV6);
	SeStrCpy(c->VpnCaCertName, sizeof(c->VpnCaCertName), vc->VpnCaCertNameV6);
	SeStrCpy(c->VpnRsaKeyName, sizeof(c->VpnRsaKeyName), vc->VpnRsaKeyNameV6);
	c->VpnPhase1Mode = vc->VpnPhase1ModeV6;
	c->VpnPhase1Crypto = vc->VpnPhase1CryptoV6;
	c->VpnPhase1Hash = vc->VpnPhase1HashV6;
	c->VpnPhase1LifeKilobytes = vc->VpnPhase1LifeKilobytesV6;
	c->VpnPhase1LifeSeconds = vc->VpnPhase1LifeSecondsV6;
	c->VpnWaitPhase2BlankSpan = vc->VpnWaitPhase2BlankSpanV6;
	c->VpnPhase2Crypto = vc->VpnPhase2CryptoV6;
	c->VpnPhase2Hash = vc->VpnPhase2HashV6;
	c->VpnPhase2LifeKilobytes = vc->VpnPhase2LifeKilobytesV6;
	c->VpnPhase2LifeSeconds = vc->VpnPhase2LifeSecondsV6;
	c->VpnConnectTimeout = vc->VpnConnectTimeoutV6;
	c->VpnIdleTimeout = vc->VpnIdleTimeoutV6;
	SeIkeInitIPv6Address(&c->VpnPingTarget, &vc->VpnPingTargetV6);
	c->VpnPingInterval = vc->VpnPingIntervalV6;
	c->VpnPingMsgSize = vc->VpnPingMsgSizeV6;
	c->VpnSpecifyIssuer = vc->VpnSpecifyIssuerV6;
}

// IPsec 初期化
void SeVpn6InitIPsec(SE_VPN6 *v6)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	SE_SEC_CONFIG conf;
	SE_SEC_CLIENT_FUNCTION_TABLE func_table;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;

	// IPsec モジュールの初期化
	SeVpn6InitSecConfig(v6, &conf);

	SeZero(&func_table, sizeof(func_table));
	func_table.ClientGetTick = SeVpn6ClientGetTick;
	func_table.ClientSetTimerCallback = SeVpn6ClientSetTimerCallback;
	func_table.ClientAddTimer = SeVpn6ClientAddTimer;
	func_table.ClientSetRecvUdpCallback = SeVpn6ClientSetRecvUdpCallback;
	func_table.ClientSetRecvEspCallback = SeVpn6ClientSetRecvEspCallback;
	func_table.ClientSetRecvVirtualIpCallback = SeVpn6ClientSetRecvVirtualIpCallback;
	func_table.ClientSendUdp = SeVpn6ClientSendUdp;
	func_table.ClientSendEsp = SeVpn6ClientSendEsp;
	func_table.ClientSendVirtualIp = SeVpn6ClientSendVirtualIp;

	v6->Sec = SeSecInit(v, true, &conf, &func_table, v6, c->VpnPhase2StrictIdV6);
}

// 現在時刻取得
UINT64 SeVpn6ClientGetTick(void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	// 引数チェック
	if (v6 == NULL)
	{
		return 0;
	}

	return SeVpn6Tick(v6);
}

// タイマコールバック設定
void SeVpn6ClientSetTimerCallback(SE_SEC_TIMER_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v6->TimerCallback = callback;
	v6->TimerCallbackParam = callback_param;
}

// タイマ追加
void SeVpn6ClientAddTimer(UINT interval, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	SeVpn6SetTimer(v6, interval);
}

// UDP パケット受信コールバック設定
void SeVpn6ClientSetRecvUdpCallback(SE_SEC_UDP_RECV_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v6->UdpRecvCallback = callback;
	v6->UdpRecvCallbackParam = callback_param;
}

// ESP パケット受信コールバック設定
void SeVpn6ClientSetRecvEspCallback(SE_SEC_ESP_RECV_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v6->EspRecvCallback = callback;
	v6->EspRecvCallbackParam = callback_param;
}

// 仮想 IP パケット受信コールバック設定
void SeVpn6ClientSetRecvVirtualIpCallback(SE_SEC_VIRTUAL_IP_RECV_CALLBACK *callback, void *callback_param, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v6->VirtualIpRecvCallback = callback;
	v6->VirtualIpRecvCallbackParam = callback_param;
}

// UDP パケット送信
void SeVpn6ClientSendUdp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	SE_VPN *v;
	SE_IPV6 *ip;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;
	ip = v->IPv6_Physical;

	Se6SendUdp(ip, SeIkeGetIPv6Address(dest_addr), dest_port, SeIkeGetIPv6Address(src_addr), src_port, data, size, NULL);
}

// ESP パケット送信
void SeVpn6ClientSendEsp(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	SE_VPN *v;
	SE_IPV6 *ip;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;
	ip = v->IPv6_Physical;

	Se6SendIp(ip, SeIkeGetIPv6Address(dest_addr), SeIkeGetIPv6Address(src_addr),
		SE_IP_PROTO_ESP, 0, data, size, NULL);
}

// 仮想 IP パケット送信
void SeVpn6ClientSendVirtualIp(void *data, UINT size, void *param)
{
	SE_VPN6 *v6 = (SE_VPN6 *)param;
	SE_VPN *v;
	SE_IPV6 *ip;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;
	ip = v->IPv6_Virtual;

	// IPv6 パケットである
	Se6SendRawIp(ip, SeClone(data, size), size, NULL);
}

// IPsec 解放
void SeVpn6FreeIPsec(SE_VPN6 *v6)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;

	SeSecFree(v6->Sec);
}

// メインプロシージャ
void SeVpn6MainProc(SE_VPN6 *v6)
{
	SE_VPN *v;
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	v = v6->Vpn;

	if (v->Config->Mode == SE_CONFIG_MODE_L3IPSEC)
	{
		// IPsec メインプロシージャ
		SeVpn6IPsecMainProc(v6);
	}
}

// インターバル実行
bool SeVpn6DoInterval(SE_VPN6 *v6, UINT64 *var, UINT interval)
{
	// 引数チェック
	if (v6 == NULL)
	{
		return false;
	}

	return SeVpnDoInterval(v6->Vpn, var, interval);
}

// 現在時刻の取得
UINT64 SeVpn6Tick(SE_VPN6 *v6)
{
	// 引数チェック
	if (v6 == NULL)
	{
		return 0;
	}

	return SeVpnTick(v6->Vpn);
}

// タイマのセット
void SeVpn6SetTimer(SE_VPN6 *v6, UINT interval)
{
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	SeVpnAddTimer(v6->Vpn, interval);
}

// 物理 ETH からの IP パケットの受信
void SeVpn6RecvPacketPhysical(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v6 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;

	if (info->UnicastForMe == false)
	{
		return;
	}

	if (info->HopLimit == SE_IPV6_HOP_MAX)
	{
		// ルーティング禁止
		return;
	}

	if (c->Mode == SE_CONFIG_MODE_L3TRANS)
	{
		// 仮想ネットワークに転送する
		Se6SendIp(v->IPv6_Virtual, info->DestIpAddress, info->SrcIpAddress,
			info->Protocol, info->HopLimit, data, size, NULL);
	}
	else
	{
		// IPsec モジュールに解読してもらう
		SeVpn6IPsecInputFromPhysical(v6, p, info, data, size);
	}
}

// 仮想 ETH からの IP パケットの受信
void SeVpn6RecvPacketVirtual(SE_VPN6 *v6, SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size)
{
	SE_VPN *v;
	SE_VPN_CONFIG *c;
	bool b = false;
	UINT type;
	// 引数チェック
	if (v6 == NULL || p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = v6->Vpn;
	c = v->Config;

	type = Se6GetIPAddrType(info->SrcIpAddress);

	if ((type & SE_IPV6_ADDR_GLOBAL_UNICAST) == 0)
	{
		// 送信元がグローバルユニキャストアドレス以外の場合は転送してはならない
		return;
	}

	if (info->HopLimit == SE_IPV6_HOP_MAX)
	{
		// ルーティング禁止
		return;
	}

	// 送信元 IP アドレスがゲスト OS が使用すべき IP アドレスであるかどうか確認する
	if (Se6IsGuestNode(p, info->SrcIpAddress))
	{
		// ゲスト OS の IP アドレスである
		b = true;
	}

	if (b == false)
	{
		// パケットを破棄する
		return;
	}

	// 通常のパケットの処理
	if (c->Mode == SE_CONFIG_MODE_L3TRANS)
	{
		// 透過モード
		if (info->IsRawIpPacket == false)
		{
			// 物理ネットワークの IP アドレスを仮想ネットワークと同一にする
			v->IPv6_Physical->GlobalIpAddress = info->SrcIpAddress;

			// 物理ネットワークに転送する
			Se6SendIp(v->IPv6_Physical, info->DestIpAddress, info->SrcIpAddress, info->Protocol,
				info->HopLimit, data, size, NULL);
		}
	}
	else
	{
		// ゲスト OS が使用していると推定される IP アドレスを更新する
		SeVpn6UpdateGuestOsIpAddress(v6, info->SrcIpAddress);

		// VPN モード
		if (info->IsRawIpPacket)
		{
			// IPsec モジュールを用いて送信する
			SeVpn6IPsecInputFromVirtual(v6, p, info, data, size);
		}
	}
}

// VPN の初期化
SE_VPN6 *SeVpn6Init(SE_VPN *vpn)
{
	SE_VPN6 *v6;
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (vpn == NULL)
	{
		return NULL;
	}

	// 初期化
	v6 = SeZeroMalloc(sizeof(SE_VPN6));
	v6->Vpn = vpn;
	v6->icmp_ip_packet_id = 0x7fffffff;

	c = vpn->Config;

	// 仮想 LAN 側でプロキシ NDP を有効にする
	vpn->IPv6_Virtual->UseProxyNdp = true;

	// 仮想 LAN 側で Raw IP を有効にする
	vpn->IPv6_Virtual->UseRawIp = true;

	if (c->RaV6)
	{
		// 仮想 LAN 側で RA 送信を有効にする
		vpn->IPv6_Virtual->SendRa = true;
	}

	// IPsec 初期化
	if (c->Mode == SE_CONFIG_MODE_L3IPSEC)
	{
		SeVpn6InitIPsec(v6);
	}

	// メイン関数を 1 回だけ呼び出す
	SeVpn6MainProc(v6);

	return v6;
}

// VPN の解放
void SeVpn6Free(SE_VPN6 *v6)
{
	// 引数チェック
	if (v6 == NULL)
	{
		return;
	}

	// IPsec 解放
	if (v6->Vpn->Config->Mode == SE_CONFIG_MODE_L3IPSEC)
	{
		SeVpn6FreeIPsec(v6);
	}

	SeFree(v6);
}

