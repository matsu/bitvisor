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

// SeVpn.c
// 概要: VPN 処理 (IPv4 / IPv6 共通部分)

#define SE_INTERNAL
#include <Se/Se.h>

// IPv4 パケットの受信コールバック関数
void SeVpnRecvIpV4PacketCallback(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size, void *param)
{
	SE_VPN *v;
	// 引数チェック
	if (p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = p->Vpn;

	if (p->Physical)
	{
		SeVpn4RecvPacketPhysical(v->Vpn4, p, info, data, size);
	}
	else
	{
		SeVpn4RecvPacketVirtual(v->Vpn4, p, info, data, size);
	}
}

// IPv6 パケットの受信コールバック関数
void SeVpnRecvIpV6PacketCallback(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size, void *param)
{
	SE_VPN *v;
	// 引数チェック
	if (p == NULL || info == NULL || data == NULL)
	{
		return;
	}

	v = p->Vpn;

	if (p->Physical)
	{
		SeVpn6RecvPacketPhysical(v->Vpn6, p, info, data, size);
	}
	else
	{
		SeVpn6RecvPacketVirtual(v->Vpn6, p, info, data, size);
	}
}

// IP スタック初期化
void SeVpnInitIpStack(SE_VPN *v)
{
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	c = v->Config;

	if (c->Mode != SE_CONFIG_MODE_L2TRANS)
	{
		// L2 透過モード以外では IP スタックを有効にする
		if (c->BindV4)	// IPv4
		{
			// 物理 ETH 側
			v->IPv4_Physical = Se4Init(v, v->PhysicalEth, true,
				c->HostIpAddressV4, c->HostIpSubnetV4, c->HostIpDefaultGatewayV4,
				c->HostMtuV4, SeVpnRecvIpV4PacketCallback, NULL);

			// 仮想 ETH 側
			v->IPv4_Virtual = Se4Init(v, v->VirtualEth, false,
				c->GuestVirtualGatewayIpAddressV4, c->GuestIpSubnetV4, Se4ZeroIP(),
				c->GuestMtuV4, SeVpnRecvIpV4PacketCallback, NULL);

			// VPN モジュール初期化
			v->Vpn4 = SeVpn4Init(v);
		}

		if (c->BindV6)	// IPv6
		{
			char tmp[MAX_PATH];

			// 物理 ETH 側
			v->IPv6_Physical = Se6Init(v, v->PhysicalEth, true,
				c->HostIpAddressV6, Se6GenerateSubnetMask(c->HostIpAddressSubnetV6), c->HostIpDefaultGatewayV6,
				c->HostMtuV6, SeVpnRecvIpV6PacketCallback, NULL);

			Se6IPToStr(tmp, v->IPv6_Physical->GlobalIpAddress);
			SeInfo("Physical IPv6 Global: %s", tmp);
			Se6IPToStr(tmp, v->IPv6_Physical->LocalIpAddress);
			SeInfo("Physical IPv6 Local : %s", tmp);

			// 仮想 ETH 側
			v->IPv6_Virtual = Se6Init(v, v->VirtualEth, false,
				Se6GenerateEui64GlobalAddress(c->GuestIpAddressPrefixV6, Se6GenerateSubnetMask(c->GuestIpAddressSubnetV6), v->VirtualEth->MyMacAddress),
				Se6GenerateSubnetMask(c->GuestIpAddressSubnetV6), Se6ZeroIP(),
				c->GuestMtuV6, SeVpnRecvIpV6PacketCallback, NULL);

			if (Se6IsZeroIP(c->GuestVirtualGatewayIpAddressV6) == false)
			{
				v->IPv6_Virtual->LocalIpAddress = c->GuestVirtualGatewayIpAddressV6;
			}

			Se6IPToStr(tmp, v->IPv6_Virtual->GlobalIpAddress);
			SeInfo("V_Router IPv6 Global: %s", tmp);
			Se6IPToStr(tmp, v->IPv6_Virtual->LocalIpAddress);
			SeInfo("V_Router IPv6 Local : %s", tmp);

			// VPN モジュール初期化
			v->Vpn6 = SeVpn6Init(v);
		}
	}
}

// IP スタック解放
void SeVpnFreeIpStack(SE_VPN *v)
{
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	if (v->Vpn6 != NULL)
	{
		SeVpn6Free(v->Vpn6);
		v->Vpn6 = NULL;
	}

	if (v->IPv6_Physical != NULL)
	{
		Se6Free(v->IPv6_Physical);
		v->IPv6_Physical = NULL;
	}

	if (v->IPv6_Virtual != NULL)
	{
		Se6Free(v->IPv6_Virtual);
		v->IPv6_Virtual = NULL;
	}

	if (v->Vpn4 != NULL)
	{
		SeVpn4Free(v->Vpn4);
		v->Vpn4 = NULL;
	}

	if (v->IPv4_Physical != NULL)
	{
		Se4Free(v->IPv4_Physical);
		v->IPv4_Physical = NULL;
	}

	if (v->IPv4_Virtual != NULL)
	{
		Se4Free(v->IPv4_Virtual);
		v->IPv4_Virtual = NULL;
	}
}

// 受信した Ethernet パケットの処理
void SeVpnMainProcRecvEtherPacket(SE_VPN *v, bool physical, void *packet, UINT packet_size)
{
	SE_VPN_CONFIG *c;
	// 引数チェック
	if (v == NULL || packet == NULL)
	{
		return;
	}

	c = v->Config;

	if (c->Mode == SE_CONFIG_MODE_L2TRANS)
	{
		// L2 透過モード
		SeVpnSendEtherPacket(v, (physical ? v->VirtualEth : v->PhysicalEth),
			packet, packet_size);
	}
	else
	{
		// L3 透過モード or L3 IPsec モード
		if (physical)
		{
			// IPv4
			if (v->IPv4_Physical != NULL)
			{
				Se4RecvEthPacket(v->IPv4_Physical, packet, packet_size);
			}

			// IPv6
			if (v->IPv6_Physical != NULL)
			{
				Se6RecvEthPacket(v->IPv6_Physical, packet, packet_size);
			}
		}
		else
		{
			// IPv4
			if (v->IPv4_Virtual != NULL)
			{
				Se4RecvEthPacket(v->IPv4_Virtual, packet, packet_size);
			}

			// IPv6
			if (v->IPv6_Virtual != NULL)
			{
				Se6RecvEthPacket(v->IPv6_Virtual, packet, packet_size);
			}
		}
	}
}

// インターバル実行
bool SeVpnDoInterval(SE_VPN *v, UINT64 *var, UINT interval)
{
	UINT64 tick;
	// 引数チェック
	if (v == NULL || var == NULL)
	{
		return false;
	}

	tick = SeVpnTick(v);

	if (*var == 0 || (*var + (UINT64)interval) <= tick)
	{
		*var = tick;

		SeVpnAddTimer(v, interval);

		return true;
	}
	else
	{
		return false;
	}
}

// メインプロセス
void SeVpnMainProcess(SE_VPN *v)
{
	UINT num_pe, num_ve;
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	if (v->IPv4_Physical != NULL)
	{
		Se4MainProc(v->IPv4_Physical);
	}
	if (v->IPv4_Virtual != NULL)
	{
		Se4MainProc(v->IPv4_Virtual);
	}
	if (v->Vpn4 != NULL)
	{
		SeVpn4MainProc(v->Vpn4);
	}

	if (v->IPv6_Physical != NULL)
	{
		Se6MainProc(v->IPv6_Physical);
	}
	if (v->IPv6_Virtual != NULL)
	{
		Se6MainProc(v->IPv6_Virtual);
	}
	if (v->Vpn6 != NULL)
	{
		SeVpn6MainProc(v->Vpn6);
	}

	// 仮想 NIC から Ethernet パケットを受信
	while (true)
	{
		void *packet = SeVpnRecvEtherPacket(v, v->VirtualEth);
		if (packet == NULL)
		{
			break;
		}

		SeVpnMainProcRecvEtherPacket(v, false, packet, SeMemSize(packet));

		SeFree(packet);
	}

	// 物理 NIC から Ethernet パケットを受信
	while (true)
	{
		void *packet = SeVpnRecvEtherPacket(v, v->PhysicalEth);
		if (packet == NULL)
		{
			break;
		}

		SeVpnMainProcRecvEtherPacket(v, true, packet, SeMemSize(packet));

		SeFree(packet);
	}

	// 送信キューに入れた Ethernet パケットの一括送信
	num_pe = SeEthSendAll(v->PhysicalEth);
	num_ve = SeEthSendAll(v->VirtualEth);

	if (num_pe != 0 || num_ve != 0)
	{
		SeVpnStatusChanged(v);
	}
}

// メインプロセス初期化
void SeVpnMainInit(SE_VPN *v)
{
	SE_VPN_CONFIG *c;
	char tmp[MAX_PATH] = {0};
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	c = v->Config;

	v->Tick64 = SeTick64();

	if (SeIsZero(c->VirtualGatewayMacAddress, SE_ETHERNET_MAC_ADDR_SIZE) == false)
	{
		SeCopy(v->VirtualEth->MyMacAddress, c->VirtualGatewayMacAddress, SE_ETHERNET_MAC_ADDR_SIZE);
	}

	SeCopy(v->PhysicalEth->MyMacAddress, v->PhysicalEth->Info.MacAddress, SE_ETHERNET_MAC_ADDR_SIZE);

	//SeMacToStr(tmp, sizeof(tmp), v->VirtualEth->MyMacAddress);
	//SeLog(SE_LOG_INFO, "Virtual MAC Address  : %s", tmp);

	//SeMacToStr(tmp, sizeof(tmp), v->PhysicalEth->MyMacAddress);
	//SeLog(SE_LOG_INFO, "Physical MAC Address : %s", tmp);

	if (c->Mode == SE_CONFIG_MODE_L2TRANS)
	{
		SeLog(SE_LOG_INFO, "Mode: L2Trans");

		v->PhysicalEth->IsPromiscusMode = true;
		v->VirtualEth->IsPromiscusMode = true;
	}
	else if (c->Mode == SE_CONFIG_MODE_L3TRANS)
	{
		SeLog(SE_LOG_INFO, "Mode: L3Trans");

		v->PhysicalEth->IsPromiscusMode = false;
		v->VirtualEth->IsPromiscusMode = false;
	}
	else
	{
		SeLog(SE_LOG_INFO, "Mode: L3IPsec");

		v->PhysicalEth->IsPromiscusMode = false;
		v->VirtualEth->IsPromiscusMode = false;
	}

	v->Inited = true;

	// IP スタックの初期化
	SeVpnInitIpStack(v);
}

// メインプロセス終了
void SeVpnMainFree(SE_VPN *v)
{
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	// IP スタックの解放
	SeVpnFreeIpStack(v);

	v->Inited = false;
}

// Ethernet パケットの受信
void *SeVpnRecvEtherPacket(SE_VPN *v, SE_ETH *e)
{
	void *packet;
	UINT type;
	bool b = false;
	// 引数チェック
	if (v == NULL || e == NULL)
	{
		return NULL;
	}

	// 次のパケットを取得
	packet = SeEthGetNextRecvPacket(e);
	if (packet == NULL)
	{
		return NULL;
	}

	// パケットを分析
	type = SeEthParseEthernetPacket(packet, SeMemSize(packet), e->MyMacAddress);

	if (type & SE_ETHER_PACKET_TYPE_VALID)
	{
		if (e->IsPromiscusMode)
		{
			b = true;
		}
		else
		{
			if (type & SE_ETHER_PACKET_TYPE_FOR_ME)
			{
				b = true;
			}
		}
	}

	if (b == false)
	{
		SeFree(packet);
		return NULL;
	}
	else
	{
		return packet;
	}
}

// Ethernet パケットの送信
void SeVpnSendEtherPacket(SE_VPN *v, SE_ETH *e, void *packet, UINT packet_size)
{
	// 引数チェック
	if (v == NULL || e == NULL || packet == NULL)
	{
		return;
	}

	SeEthSendAdd(e, packet, packet_size);
}

// メインプロセス内で状態が変化した場合に呼び出す関数
void SeVpnStatusChanged(SE_VPN *v)
{
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	v->StatusChanged = true;
}

// メインハンドラ
void SeVpnMainHandler(SE_VPN *v)
{
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	SeLock(v->MainLock);
	{
		// 現在の時刻を取得する
		v->Tick64 = SeTick64();

		// メインプロセスをループで回す
		do
		{
			v->StatusChanged = false;

			SeVpnMainProcess(v);
		}
		while (v->StatusChanged);	// 状態変化が収束したらループを抜ける
	}
	SeUnlock(v->MainLock);
}

// 現在の Tick 値を取得する
UINT64 SeVpnTick(SE_VPN *v)
{
	// 引数チェック
	if (v == NULL)
	{
		return 0;
	}

	return v->Tick64;
}

// タイマを追加する
void SeVpnAddTimer(SE_VPN *v, UINT interval)
{
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	SeTimerSet(v->Timer, interval);
}

// タイマハンドラ
void SeVpnTimerCallback(SE_TIMER *t, UINT64 current_tick, void *param)
{
	SE_VPN *v = (SE_VPN *)param;
	// 引数チェック
	if (t == NULL || v == NULL)
	{
		return;
	}

	SeVpnMainHandler(v);
}

// Ethernet 受信ハンドラ
void SeVpnEthRecvCallback(SE_ETH *eth, void *param)
{
	SE_VPN *v = (SE_VPN *)param;
	// 引数チェック
	if (eth == NULL || v == NULL)
	{
		return;
	}

	// メインハンドラを呼び出す
	SeVpnMainHandler(v);
}

// VPN 初期化
SE_VPN *SeVpnInit(SE_HANDLE physical_nic_handle, SE_HANDLE virtual_nic_handle, char *config_name)
{
	SE_VPN_CONFIG *c;
	SE_LIST *o;
	SE_VPN *v;
	// 引数チェック
	if (physical_nic_handle == NULL || virtual_nic_handle == NULL || config_name == NULL)
	{
		return NULL;
	}

	// 設定の読み込み
	o = SeLoadConfigEntryList(config_name);
	if (o == NULL)
	{
		// 読み込みに失敗
		SeLog(SE_LOG_FATAL, "Could not load config file \"%s\".", config_name);
		return NULL;
	}

	c = SeVpnLoadConfig(o);

	SeFreeConfigEntryList(o);

	if (c == NULL)
	{
		// パースに失敗
		return NULL;
	}

	v = SeZeroMalloc(sizeof(SE_VPN));

	v->MainLock = SeNewLock();

	v->Config = c;

	// ETH の作成
	v->PhysicalEth = SeEthNew(physical_nic_handle, SE_NIC_PHYSICAL,
		SeVpnEthRecvCallback, v);
	v->VirtualEth = SeEthNew(virtual_nic_handle, SE_NIC_VIRTUAL,
		SeVpnEthRecvCallback, v);

	// タイマの作成
	v->Timer = SeTimerNew(SeVpnTimerCallback, v);

	// メインプロセス初期化
	SeVpnMainInit(v);

	// 1 回だけメインハンドラを呼び出す
	SeVpnMainHandler(v);

	return v;
}

// VPN 解放
void SeVpnFree(SE_VPN *v)
{
	// 引数チェック
	if (v == NULL)
	{
		return;
	}

	// メインプロセス終了
	SeVpnMainFree(v);

	SeTimerFree(v->Timer);

	SeEthFree(v->PhysicalEth);
	SeEthFree(v->VirtualEth);

	SeVpnFreeConfig(v->Config);

	SeDeleteLock(v->MainLock);

	SeFree(v);
}

// 設定データの解放
void SeVpnFreeConfig(SE_VPN_CONFIG *c)
{
	// 引数チェック
	if (c == NULL)
	{
		return;
	}

	SeFree(c);
}

// 設定の読み込み
SE_VPN_CONFIG *SeVpnLoadConfig(SE_LIST *o)
{
	SE_VPN_CONFIG c;
	SE_VPN_CONFIG *ret = NULL;
	char *s;
	char error_str[MAX_PATH];
	SE_BUF *b;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	SeZero(&c, sizeof(c));
	SeStrCpy(error_str, sizeof(error_str), "");

	// 動作モード
	s = SeGetConfigStr(o, "Mode");
	if (SeStrCmpi(s, "L2Trans") == 0)
	{
		c.Mode = SE_CONFIG_MODE_L2TRANS;
	}
	else if (SeStrCmpi(s, "L3Trans") == 0)
	{
		c.Mode = SE_CONFIG_MODE_L3TRANS;
	}
	else if (SeStrCmpi(s, "L3IPsec") == 0)
	{
		c.Mode = SE_CONFIG_MODE_L3IPSEC;
	}
	else
	{
		SeFormat(error_str, sizeof(error_str), "Mode: Invalid Value: \"%s\"", s);
		goto LABEL_ERROR;
	}

	b = SeGetConfigBin(o, "VirtualGatewayMacAddress");
	if (b->Size == SE_ETHERNET_MAC_ADDR_SIZE)
	{
		SeCopy(c.VirtualGatewayMacAddress, b->Buf, SE_ETHERNET_MAC_ADDR_SIZE);
	}
	else
	{
		SeFormat(error_str, sizeof(error_str), "Mode: Invalid Value: VirtualGatewayMacAddress");
		SeFreeBuf(b);
		goto LABEL_ERROR;
	}
	SeFreeBuf(b);

	// IPv4
	c.BindV4 = SeGetConfigBool(o, "BindV4");
	if (c.BindV4)
	{
		// ゲスト OS が使用する IPv4 アドレスおよびサブネットマスク
		c.GuestIpAddressV4 = Se4StrToIP(SeGetConfigStr(o, "GuestIpAddressV4"));
		c.GuestIpSubnetV4 = Se4StrToIP(SeGetConfigStr(o, "GuestIpSubnetV4"));
		// ゲスト OS が IPv4 で使用する MTU (Ethernet ペイロードサイズ)
		c.GuestMtuV4 = MAKESURE(SE_DEFAULT_VALUE(SeGetConfigInt(o, "GuestMtuV4"), SE_V4_MTU_IN_VPN_DEFAULT), SE_V4_MTU_MIN, SE_V4_MTU_MAX);
		// ゲスト OS に対して通信サービスを提供する仮想ゲートウェイの IPv4 アドレス
		c.GuestVirtualGatewayIpAddressV4 = Se4StrToIP(SeGetConfigStr(o, "GuestVirtualGatewayIpAddressV4"));
		if (Se4IsZeroIP(c.GuestVirtualGatewayIpAddressV4))
		{
			UINT i = Se4IPToUINT(Se4GetBroadcastAddress(c.GuestIpAddressV4, c.GuestIpSubnetV4));

			i = SeEndian32(SeEndian32(i) - 1);

			c.GuestVirtualGatewayIpAddressV4 = Se4UINTToIP(i);
		}
		// ゲスト OS に対して DHCPv4 サービスを提供するかどうか
		c.DhcpV4 = SeGetConfigBool(o, "DhcpV4");
		// DHCPv4 サービスで割り当てる IPv4 アドレスの有効期限 (秒)
		c.DhcpLeaseExpiresV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "DhcpLeaseExpiresV4"), SE_IPV4_DHCP_EXPIRES_DEFAULT);
		// ゲスト OS に対して DHCP で指定する DNS サーバーアドレス
		c.DhcpDnsV4 = Se4StrToIP(SeGetConfigStr(o, "DhcpDnsV4"));
		// ゲスト OS に対して DHCP で指定するドメイン名
		SeStrCpy(c.DhcpDomainV4, sizeof(c.DhcpDomainV4), SeGetConfigStr(o, "DhcpDomainV4"));

		// ゲスト OS が送信する TCPv4 SYN パケットの MSS の強制書き換え
		c.AdjustTcpMssV4 = SeGetConfigInt(o, "AdjustTcpMssV4");

		// ホスト OS が使用する IPv4 アドレスおよびサブネットマスク
		if (c.Mode == SE_CONFIG_MODE_L3TRANS)
		{
			c.HostIpAddressV4 = c.GuestIpAddressV4;
			c.HostIpSubnetV4 = c.GuestIpSubnetV4;
		}
		else
		{
			c.HostIpAddressV4 = Se4StrToIP(SeGetConfigStr(o, "HostIpAddressV4"));
			c.HostIpSubnetV4 = Se4StrToIP(SeGetConfigStr(o, "HostIpSubnetV4"));
		}
		// ホスト OS が IPv4 で使用する MTU (Ethernet ペイロードサイズ)
		c.HostMtuV4 = MAKESURE(SE_DEFAULT_VALUE(SeGetConfigInt(o, "HostMtuV4"), SE_V4_MTU_DEFAULT), SE_V4_MTU_MIN, SE_V4_MTU_MAX);

		// デフォルトゲートウェイの IPv4 アドレス
		c.HostIpDefaultGatewayV4 = Se4StrToIP(SeGetConfigStr(o, "HostIpDefaultGatewayV4"));
		if (Se4IsZeroIP(c.HostIpDefaultGatewayV4) == false)
		{
			c.UseDefaultGatewayV4 = true;
		}

		// オプション値
		c.OptionV4ArpExpires = SE_DEFAULT_VALUE(SeGetConfigInt(o, "OptionV4ArpExpires"), SE_IPV4_ARP_EXPIRES_DEFAULT);
		c.OptionV4ArpDontUpdateExpires = SeGetConfigBool(o, "OptionV4ArpDontUpdateExpires");

		// 値のチェック (ゲスト OS)
		if (Se4IsSubnetMask(c.GuestIpSubnetV4) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "GuestIpSubnetV4: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (Se4CheckUnicastAddress(c.GuestIpAddressV4, c.GuestIpSubnetV4) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "GuestIpAddressV4: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (Se4IsInSameNetwork(c.GuestVirtualGatewayIpAddressV4, c.GuestIpAddressV4, c.GuestIpSubnetV4) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "GuestVirtualGatewayIpAddressV4: Not In Same Network.");
			goto LABEL_ERROR;
		}

		// 値のチェック (ホスト OS)
		if (Se4IsSubnetMask(c.HostIpSubnetV4) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "HostIpSubnetV4: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (Se4CheckUnicastAddress(c.HostIpAddressV4, c.HostIpSubnetV4) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "HostIpAddressV4: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (c.UseDefaultGatewayV4)
		{
			if (Se4Cmp(Se4GetNetworkAddress(c.HostIpAddressV4, c.HostIpSubnetV4),
				Se4GetNetworkAddress(c.HostIpDefaultGatewayV4, c.HostIpSubnetV4)) != 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "HostIpDefaultGatewayV4: Invalid Value (Bad Network Address).");
				goto LABEL_ERROR;
			}

			if (Se4CheckUnicastAddress(c.HostIpDefaultGatewayV4, c.HostIpSubnetV4) == false)
			{
				SeStrCpy(error_str, sizeof(error_str), "HostIpDefaultGatewayV4: Invalid Value (Not Unicast Address).");
				goto LABEL_ERROR;
			}
		}

		if (c.Mode == SE_CONFIG_MODE_L3IPSEC)
		{
			// IPsec パラメータの読み込み
			c.VpnGatewayAddressV4 = Se4StrToIP(SeGetConfigStr(o, "VpnGatewayAddressV4"));

			s = SeGetConfigStr(o, "VpnAuthMethodV4");
			c.VpnAuthMethodV4 = SeSecStrToAuthMethod(s);

			SeStrCpy(c.VpnPasswordV4, sizeof(c.VpnPasswordV4), SeGetConfigStr(o, "VpnPasswordV4"));
			SeStrCpy(c.VpnIdStringV4, sizeof(c.VpnIdStringV4), SeGetConfigStr(o, "VpnIdStringV4"));
			SeStrCpy(c.VpnCertNameV4, sizeof(c.VpnCertNameV4), SeGetConfigStr(o, "VpnCertNameV4"));
			SeStrCpy(c.VpnCaCertNameV4, sizeof(c.VpnCaCertNameV4), SeGetConfigStr(o, "VpnCaCertNameV4"));
			SeStrCpy(c.VpnRsaKeyNameV4, sizeof(c.VpnRsaKeyNameV4), SeGetConfigStr(o, "VpnRsaKeyNameV4"));

			c.VpnSpecifyIssuerV4 = SeGetConfigBool(o, "VpnSpecifyIssuerV4");

			c.VpnPhase1ModeV4 = SeIkeStrToPhase1Mode(SeGetConfigStr(o, "VpnPhase1ModeV4"));
			c.VpnPhase1CryptoV4 = SeIkeStrToPhase1CryptId(SeGetConfigStr(o, "VpnPhase1CryptoV4"));
			c.VpnPhase1HashV4 = SeIkeStrToPhase1HashId(SeGetConfigStr(o, "VpnPhase1HashV4"));
			c.VpnPhase1LifeKilobytesV4 = SeGetConfigInt(o, "VpnPhase1LifeKilobytesV4");
			c.VpnPhase1LifeSecondsV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPhase1LifeSecondsV4"), SE_SEC_DEFAULT_P1_LIFE_SECONDS);
			c.VpnWaitPhase2BlankSpanV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnWaitPhase2BlankSpanV4"), SE_SEC_DEFAULT_WAIT_P2_BLANK_SPAN);
			c.VpnPhase2CryptoV4 = SeIkeStrToPhase2CryptId(SeGetConfigStr(o, "VpnPhase2CryptoV4"));
			c.VpnPhase2HashV4 = SeIkeStrToPhase2HashId(SeGetConfigStr(o, "VpnPhase2HashV4"));
			c.VpnPhase2LifeKilobytesV4 = SeGetConfigInt(o, "VpnPhase2LifeKilobytesV4");
			c.VpnPhase2LifeSecondsV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPhase2LifeSecondsV4"), SE_SEC_DEFAULT_P2_LIFE_SECONDS);
			c.VpnConnectTimeoutV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnConnectTimeoutV4"), SE_SEC_DEFAULT_CONNECT_TIMEOUT);
			c.VpnIdleTimeoutV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnIdleTimeoutV4"), SE_SEC_DEFAULT_IDLE_TIMEOUT);
			c.VpnPingTargetV4 = Se4StrToIP(SeGetConfigStr(o, "VpnPingTargetV4"));
			c.VpnPingIntervalV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPingIntervalV4"), SE_SEC_DEFAULT_PING_INTERVAL);
			c.VpnPingMsgSizeV4 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPingMsgSizeV4"), SE_SEC_DEFAULT_PING_MSG_SIZE);

			// IPsec パラメータの検査
			if (Se4IsZeroIP(c.VpnGatewayAddressV4))
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnGatewayAddressV4: Invalid Host Address.");
				goto LABEL_ERROR;
			}
			if (c.VpnAuthMethodV4 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnAuthMethodV4: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnAuthMethodV4 == SE_SEC_AUTH_METHOD_PASSWORD)
			{
				if (SeIsEmptyStr(c.VpnIdStringV4))
				{
					SeStrCpy(error_str, sizeof(error_str), "VpnIdStringV4: Invalid Value.");
					goto LABEL_ERROR;
				}
			}
			else if (c.VpnAuthMethodV4 == SE_SEC_AUTH_METHOD_CERT)
			{
				if (SeIsEmptyStr(c.VpnCertNameV4))
				{
					SeStrCpy(error_str, sizeof(error_str), "VpnCertNameV4: Invalid Value.");
					goto LABEL_ERROR;
				}
				if (SeIsEmptyStr(c.VpnRsaKeyNameV4))
				{
					SeStrCpy(error_str, sizeof(error_str), "VpnRsaKeyNameV4: Invalid Value.");
					goto LABEL_ERROR;
				}
			}
			if (c.VpnPhase1ModeV4 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase1ModeV4: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase1CryptoV4 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase1CryptoV4: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase1HashV4 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase1HashV4: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase2CryptoV4 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase2CryptoV4: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase2HashV4 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase2HashV4: Invalid Value.");
				goto LABEL_ERROR;
			}
		}
	}

	// IPv6
	c.BindV6 = SeGetConfigBool(o, "BindV6");
	if (c.BindV6)
	{
		// ゲスト OS が使用する IPv6 アドレスプレフィックス
		c.GuestIpAddressPrefixV6 = Se6StrToIP(SeGetConfigStr(o, "GuestIpAddressPrefixV6"));
		// ゲスト OS が使用する IPv6 アドレスプレフィックスのサブネット長
		c.GuestIpAddressSubnetV6 = SeGetConfigInt(o, "GuestIpAddressSubnetV6");
		// ゲスト OS が IPv6 で使用する MTU (Ethernet ペイロードサイズ)
		c.GuestMtuV6 = MAKESURE(SE_DEFAULT_VALUE(SeGetConfigInt(o, "GuestMtuV6"), SE_V6_MTU_IN_VPN_DEFAULT), SE_V6_MTU_MIN, SE_V6_MTU_MAX);
		// ゲスト OS に対して通信サービスを提供する仮想ゲートウェイの IPv6 アドレス
		c.GuestVirtualGatewayIpAddressV6 = Se6StrToIP(SeGetConfigStr(o, "GuestVirtualGatewayIpAddressV6"));
		if (Se6IsZeroIP(c.GuestVirtualGatewayIpAddressV6))
		{
			c.GuestVirtualGatewayIpAddressV6 = Se6GenerateEui64LocalAddress(c.VirtualGatewayMacAddress);
		}
		// ゲスト OS に対して Router Advertisement を行うかどうか
		c.RaV6 = SeGetConfigBool(o, "RaV6");
		// Router Advertisement で指定するプレフィックスの有効期限 (秒)
		c.RaLifetimeV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "RaLifetimeV6"), SE_IPV6_RA_EXPIRES_DEFAULT);
		// ゲスト OS が使用すべき DNSv6 サーバーアドレス
		c.RaDnsV6 = Se6StrToIP(SeGetConfigStr(o, "RaDnsV6"));

		// ホスト OS が使用する IPv6 アドレスおよびサブネットマスク
		if (c.Mode == SE_CONFIG_MODE_L3TRANS)
		{
			c.HostIpAddressV6 = c.GuestIpAddressPrefixV6;
			c.HostIpAddressV6.Value[15] = 3;
			c.HostIpAddressSubnetV6 = c.GuestIpAddressSubnetV6;
		}
		else
		{
			c.HostIpAddressV6 = Se6StrToIP(SeGetConfigStr(o, "HostIpAddressV6"));
			c.HostIpAddressSubnetV6 = SeGetConfigInt(o, "HostIpAddressSubnetV6");
		}

		// ホスト OS が IPv6 で使用する MTU (Ethernet ペイロードサイズ)
		c.HostMtuV6 = MAKESURE(SE_DEFAULT_VALUE(SeGetConfigInt(o, "HostMtuV6"), SE_V6_MTU_DEFAULT), SE_V6_MTU_MIN, SE_V6_MTU_MAX);
		// ホスト OS が使用するデフォルトゲートウェイの IPv6 アドレス
		c.HostIpDefaultGatewayV6 = Se6StrToIP(SeGetConfigStr(o, "HostIpDefaultGatewayV6"));
		// 近隣エントリキャッシュの有効期限 (秒)
		c.OptionV6NeighborExpires = SE_DEFAULT_VALUE(SeGetConfigInt(o, "OptionV6NeighborExpires"), SE_IPV6_NEIGHBOR_EXPIRES_DEFAULT);

		// パラメータチェック (ゲスト OS)
		if (Se6CheckUnicastAddress(c.GuestIpAddressPrefixV6) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "GuestIpAddressPrefixV6: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (Se6CheckSubnetLength(c.GuestIpAddressSubnetV6) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "GuestIpAddressSubnetV6: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (Se6IsNetworkPrefixAddress(c.GuestIpAddressPrefixV6, Se6GenerateSubnetMask(c.GuestIpAddressSubnetV6)) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "GuestIpAddressPrefixV6: Not Prefix Address.");
			goto LABEL_ERROR;
		}
		if ((Se6GetIPAddrType(c.GuestVirtualGatewayIpAddressV6) & SE_IPV6_ADDR_LOCAL_UNICAST) == 0)
		{
			SeStrCpy(error_str, sizeof(error_str), "GuestVirtualGatewayIpAddressV6: Not Local Unicast Address.");
			goto LABEL_ERROR;
		}

		// パラメータチェック (ホスト OS)
		if (Se6CheckUnicastAddress(c.HostIpAddressV6) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "HostIpAddressV6: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (Se6CheckSubnetLength(c.HostIpAddressSubnetV6) == false)
		{
			SeStrCpy(error_str, sizeof(error_str), "HostIpAddressSubnetV6: Invalid Value.");
			goto LABEL_ERROR;
		}
		if (Se6IsZeroIP(c.HostIpDefaultGatewayV6) == false)
		{
			if ((Se6GetIPAddrType(c.HostIpDefaultGatewayV6) & SE_IPV6_ADDR_UNICAST) == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "HostIpDefaultGatewayV6: Not Unicast Address.");
				goto LABEL_ERROR;
			}

			c.UseDefaultGatewayV6 = true;
		}

		if (c.Mode == SE_CONFIG_MODE_L3IPSEC)
		{
			// IPsec パラメータの読み込み
			c.VpnGatewayAddressV6 = Se6StrToIP(SeGetConfigStr(o, "VpnGatewayAddressV6"));

			s = SeGetConfigStr(o, "VpnAuthMethodV6");
			c.VpnAuthMethodV6 = SeSecStrToAuthMethod(s);

			SeStrCpy(c.VpnPasswordV6, sizeof(c.VpnPasswordV6), SeGetConfigStr(o, "VpnPasswordV6"));
			SeStrCpy(c.VpnIdStringV6, sizeof(c.VpnIdStringV6), SeGetConfigStr(o, "VpnIdStringV6"));
			SeStrCpy(c.VpnCertNameV6, sizeof(c.VpnCertNameV6), SeGetConfigStr(o, "VpnCertNameV6"));
			SeStrCpy(c.VpnCaCertNameV6, sizeof(c.VpnCaCertNameV6), SeGetConfigStr(o, "VpnCaCertNameV6"));
			SeStrCpy(c.VpnRsaKeyNameV6, sizeof(c.VpnRsaKeyNameV6), SeGetConfigStr(o, "VpnRsaKeyNameV6"));

			c.VpnSpecifyIssuerV6 = SeGetConfigBool(o, "VpnSpecifyIssuerV6");

			c.VpnPhase1ModeV6 = SeIkeStrToPhase1Mode(SeGetConfigStr(o, "VpnPhase1ModeV4"));
			c.VpnPhase1CryptoV6 = SeIkeStrToPhase1CryptId(SeGetConfigStr(o, "VpnPhase1CryptoV6"));
			c.VpnPhase1HashV6 = SeIkeStrToPhase1HashId(SeGetConfigStr(o, "VpnPhase1HashV6"));
			c.VpnPhase1LifeKilobytesV6 = SeGetConfigInt(o, "VpnPhase1LifeKilobytesV6");
			c.VpnPhase1LifeSecondsV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPhase1LifeSecondsV6"), SE_SEC_DEFAULT_P1_LIFE_SECONDS);
			c.VpnWaitPhase2BlankSpanV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnWaitPhase2BlankSpanV6"), SE_SEC_DEFAULT_WAIT_P2_BLANK_SPAN);
			c.VpnPhase2CryptoV6 = SeIkeStrToPhase2CryptId(SeGetConfigStr(o, "VpnPhase2CryptoV6"));
			c.VpnPhase2HashV6 = SeIkeStrToPhase2HashId(SeGetConfigStr(o, "VpnPhase2HashV6"));
			c.VpnPhase2LifeKilobytesV6 = SeGetConfigInt(o, "VpnPhase2LifeKilobytesV6");
			c.VpnPhase2LifeSecondsV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPhase2LifeSecondsV6"), SE_SEC_DEFAULT_P2_LIFE_SECONDS);
			c.VpnPhase2StrictIdV6 = SeGetConfigBool(o, "VpnPhase2StrictIdV6");
			c.VpnConnectTimeoutV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnConnectTimeoutV6"), SE_SEC_DEFAULT_CONNECT_TIMEOUT);
			c.VpnIdleTimeoutV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnIdleTimeoutV6"), SE_SEC_DEFAULT_IDLE_TIMEOUT);
			c.VpnPingTargetV6 = Se6StrToIP(SeGetConfigStr(o, "VpnPingTargetV6"));
			c.VpnPingIntervalV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPingIntervalV6"), SE_SEC_DEFAULT_PING_INTERVAL);
			c.VpnPingMsgSizeV6 = SE_DEFAULT_VALUE(SeGetConfigInt(o, "VpnPingMsgSizeV6"), SE_SEC_DEFAULT_PING_MSG_SIZE);

			// IPsec パラメータの検査
			if (Se6IsZeroIP(c.VpnGatewayAddressV6))
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnGatewayAddressV6: Invalid Host Address.");
				goto LABEL_ERROR;
			}
			if (c.VpnAuthMethodV6 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnAuthMethodV6: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnAuthMethodV6 == SE_SEC_AUTH_METHOD_PASSWORD)
			{
				if (SeIsEmptyStr(c.VpnIdStringV6))
				{
					SeStrCpy(error_str, sizeof(error_str), "VpnIdStringV6: Invalid Value.");
					goto LABEL_ERROR;
				}
			}
			else if (c.VpnAuthMethodV6 == SE_SEC_AUTH_METHOD_CERT)
			{
				if (SeIsEmptyStr(c.VpnCertNameV6))
				{
					SeStrCpy(error_str, sizeof(error_str), "VpnCertNameV6: Invalid Value.");
					goto LABEL_ERROR;
				}
				if (SeIsEmptyStr(c.VpnRsaKeyNameV6))
				{
					SeStrCpy(error_str, sizeof(error_str), "VpnRsaKeyNameV6: Invalid Value.");
					goto LABEL_ERROR;
				}
			}
			if (c.VpnPhase1ModeV6 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase1ModeV6: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase1CryptoV6 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase1CryptoV6: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase1HashV6 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase1HashV6: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase2CryptoV6 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase2CryptoV6: Invalid Value.");
				goto LABEL_ERROR;
			}
			if (c.VpnPhase2HashV6 == 0)
			{
				SeStrCpy(error_str, sizeof(error_str), "VpnPhase2HashV6: Invalid Value.");
				goto LABEL_ERROR;
			}
		}
	}

	ret = SeClone(&c, sizeof(SE_VPN_CONFIG));

	return ret;

LABEL_ERROR:
	// エラー発生
	SeLog(SE_LOG_FATAL, "Config Error: %s", error_str);
	return NULL;
}
