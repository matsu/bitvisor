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

// SeVpn.h
// 概要: SeVpn.c のヘッダ

#ifndef	SEVPN_H
#define	SEVPN_H

// VPN 設定データ
struct SE_VPN_CONFIG
{
	UINT Mode;						// 動作モード
	UCHAR VirtualGatewayMacAddress[SE_ETHERNET_MAC_ADDR_SIZE];	// 仮想ネットワーク側の仮想ゲートウェイの持つ MAC アドレス

	// IPv4
	bool BindV4;					// IPv4 を使用するかどうか
	SE_IPV4_ADDR GuestIpAddressV4;	// ゲスト OS が使用する IPv4 アドレス
	SE_IPV4_ADDR GuestIpSubnetV4;	// ゲスト OS が使用するサブネットマスク
	UINT GuestMtuV4;				// ゲスト OS が IPv4 で使用する MTU (Ethernet ペイロードサイズ)
	SE_IPV4_ADDR GuestVirtualGatewayIpAddressV4;	// ゲスト OS に対して通信サービスを提供する仮想ゲートウェイの IPv4 アドレス
	bool DhcpV4;					// ゲスト OS に対して DHCPv4 サービスを提供するかどうか
	UINT DhcpLeaseExpiresV4;		// DHCPv4 サービスで割り当てる IPv4 アドレスの有効期限 (秒)
	SE_IPV4_ADDR DhcpDnsV4;			// ゲスト OS に対して DHCP で指定する DNS サーバーアドレス
	char DhcpDomainV4[MAX_SIZE];	// ゲスト OS に対して DHCP で指定するドメイン名
	UINT AdjustTcpMssV4;			// ゲスト OS が送信する TCPv4 SYN パケットの MSS の強制書き換え
	SE_IPV4_ADDR HostIpAddressV4;	// ホスト OS が使用する IPv4 アドレス
	SE_IPV4_ADDR HostIpSubnetV4;	// ホスト OS が使用するサブネットマスク
	bool UseDefaultGatewayV4;		// デフォルトゲートウェイを使用するかどうか
	SE_IPV4_ADDR HostIpDefaultGatewayV4;	// デフォルトゲートウェイの IPv4 アドレス
	UINT HostMtuV4;					// ホスト OS が IPv4 で使用する MTU (Ethernet ペイロードサイズ)
	UINT OptionV4ArpExpires;		// ARP エントリキャッシュの有効期限 (秒)
	bool OptionV4ArpDontUpdateExpires;	// ARP エントリキャッシュの有効期限を参照される度に増加させないかどうか

	// IPsec v4
	SE_IPV4_ADDR VpnGatewayAddressV4;	// 接続先 IPsec VPN ゲートウェイの IP アドレス
	UINT VpnAuthMethodV4;			// ユーザー認証方法
	char VpnPasswordV4[MAX_SIZE];	// パスワード認証を用いる場合の事前共有鍵
	char VpnIdStringV4[MAX_SIZE];	// パスワード認証を用いる場合に送出する ID 情報
	char VpnCertNameV4[MAX_SIZE];	// 証明書認証を用いる場合に使用する X.509 証明書ファイルの名前
	char VpnCaCertNameV4[MAX_SIZE];	// 接続先 VPN サーバーから返された X.509 証明書を検証する CA 証明書のファイル名
	char VpnRsaKeyNameV4[MAX_SIZE];	// VpnCertNameV4 で指定した X.509 証明書に対応した RSA 秘密鍵の名前
	bool VpnSpecifyIssuerV4;		// 証明書認証を用いる場合に証明書要求フィールドに自分の証明書の発行者名を明記するかどうか

	UINT VpnPhase1ModeV4;			// IKE Phase one mode (Main/Aggressive)
	UCHAR VpnPhase1CryptoV4;		// フェーズ 1 における暗号化アルゴリズム
	UCHAR VpnPhase1HashV4;			// フェーズ 1 における署名アルゴリズム
	UINT VpnPhase1LifeKilobytesV4;	// ISAKMP SA の有効期限の値 (単位: キロバイト, 0 の場合は無効)
	UINT VpnPhase1LifeSecondsV4;	// ISAKMP SA の有効期限の値 (単位: 秒, 0 の場合は無効)
	UINT VpnWaitPhase2BlankSpanV4;	// フェーズ 1 完了からフェーズ 2 開始までの間にあける時間 (単位: ミリ秒)
	UCHAR VpnPhase2CryptoV4;		// フェーズ 2 における暗号化アルゴリズム
	UCHAR VpnPhase2HashV4;			// フェーズ 2 における署名アルゴリズム
	UINT VpnPhase2LifeKilobytesV4;	// ISAKMP SA の有効期限の値 (単位: キロバイト, 0 の場合は無効)
	UINT VpnPhase2LifeSecondsV4;	// ISAKMP SA の有効期限の値 (単位: 秒, 0 の場合は無効)
	UINT VpnConnectTimeoutV4;		// VPN の接続処理を開始してから接続失敗とみなすまでのタイムアウト (秒)
	UINT VpnIdleTimeoutV4;			// VPN 接続完了後、一定時間無通信の場合にトンネルが消えたと仮定して再接続するまでのアイドルタイムアウト (単位: 秒)
	SE_IPV4_ADDR VpnPingTargetV4;	// VPN 接続完了後、定期的に ping を送信する宛先ホストの IP アドレス
	UINT VpnPingIntervalV4;			// VpnPingTargetV4 で指定した宛先に ping を送信する間隔 (秒)
	UINT VpnPingMsgSizeV4;			// VpnPingTargetV4 で指定した宛先に ping を送信する際の ICMP メッセージサイズ (バイト数)

	// IPv6
	bool BindV6;					// IPv6 を使用するかどうか
	SE_IPV6_ADDR GuestIpAddressPrefixV6;	// ゲスト OS が使用する IPv6 アドレスプレフィックス
	UINT GuestIpAddressSubnetV6;	// ゲスト OS が使用する IPv6 アドレスプレフィックスのサブネット長
	UINT GuestMtuV6;				// ゲスト OS が IPv6 で使用する MTU (Ethernet ペイロードサイズ)
	SE_IPV6_ADDR GuestVirtualGatewayIpAddressV6;	// ゲスト OS に対して通信サービスを提供する仮想ゲートウェイの IPv6 アドレス
	bool RaV6;						// ゲスト OS に対して Router Advertisement を行うかどうか
	UINT RaLifetimeV6;				// Router Advertisement で指定するプレフィックスの有効期限 (秒)
	SE_IPV6_ADDR RaDnsV6;			// ゲスト OS が使用すべき DNSv6 サーバーアドレス
	SE_IPV6_ADDR HostIpAddressV6;	// ホスト OS が使用する IPv6 アドレス (L3IPsec モードの場合のみ指定)
	UINT HostIpAddressSubnetV6;		// ホスト OS が使用する IPv6 アドレスのサブネット長 (L3IPsec モードの場合のみ指定)
	UINT HostMtuV6;					// ホスト OS が IPv6 で使用する MTU (Ethernet ペイロードサイズ)
	bool UseDefaultGatewayV6;		// デフォルトゲートウェイを使用するかどうか
	SE_IPV6_ADDR HostIpDefaultGatewayV6;	// ホスト OS が使用するデフォルトゲートウェイの IPv6 アドレス
	UINT OptionV6NeighborExpires;	// ホスト OS が使用するデフォルトゲートウェイの IPv6 アドレス

	// IPsec v6
	SE_IPV6_ADDR VpnGatewayAddressV6;	// 接続先 IPsec VPN ゲートウェイの IP アドレス
	UINT VpnAuthMethodV6;			// ユーザー認証方法
	char VpnPasswordV6[MAX_SIZE];	// パスワード認証を用いる場合の事前共有鍵
	char VpnIdStringV6[MAX_SIZE];	// パスワード認証を用いる場合に送出する ID 情報
	char VpnCertNameV6[MAX_SIZE];	// 証明書認証を用いる場合に使用する X.509 証明書ファイルの名前
	char VpnCaCertNameV6[MAX_SIZE];	// 接続先 VPN サーバーから返された X.509 証明書を検証する CA 証明書のファイル名
	char VpnRsaKeyNameV6[MAX_SIZE];	// VpnCertNameV6 で指定した X.509 証明書に対応した RSA 秘密鍵の名前
	bool VpnSpecifyIssuerV6;		// 証明書認証を用いる場合に証明書要求フィールドに自分の証明書の発行者名を明記するかどうか
	UINT VpnPhase1ModeV6;			// IKE Phase one (Main/Aggressive)
	UCHAR VpnPhase1CryptoV6;		// フェーズ 1 における暗号化アルゴリズム
	UCHAR VpnPhase1HashV6;			// フェーズ 1 における署名アルゴリズム
	UINT VpnPhase1LifeKilobytesV6;	// ISAKMP SA の有効期限の値 (単位: キロバイト, 0 の場合は無効)
	UINT VpnPhase1LifeSecondsV6;	// ISAKMP SA の有効期限の値 (単位: 秒, 0 の場合は無効)
	UINT VpnWaitPhase2BlankSpanV6;	// フェーズ 1 完了からフェーズ 2 開始までの間にあける時間 (単位: ミリ秒)
	UCHAR VpnPhase2CryptoV6;		// フェーズ 2 における暗号化アルゴリズム
	UCHAR VpnPhase2HashV6;			// フェーズ 2 における署名アルゴリズム
	UINT VpnPhase2LifeKilobytesV6;	// ISAKMP SA の有効期限の値 (単位: キロバイト, 0 の場合は無効)
	UINT VpnPhase2LifeSecondsV6;	// ISAKMP SA の有効期限の値 (単位: 秒, 0 の場合は無効)
	bool VpnPhase2StrictIdV6;		// フェーズ 2 においてゲスト OS の持つ仮想 IPv6 アドレスを ID として正しく送信するかどうか
	UINT VpnConnectTimeoutV6;		// VPN の接続処理を開始してから接続失敗とみなすまでのタイムアウト (秒)
	UINT VpnIdleTimeoutV6;			// VPN 接続完了後、一定時間無通信の場合にトンネルが消えたと仮定して再接続するまでのアイドルタイムアウト (単位: 秒)
	SE_IPV6_ADDR VpnPingTargetV6;	// VPN 接続完了後、定期的に ping を送信する宛先ホストの IP アドレス
	UINT VpnPingIntervalV6;			// VpnPingTargetV4 で指定した宛先に ping を送信する間隔 (秒)
	UINT VpnPingMsgSizeV6;			// VpnPingTargetV4 で指定した宛先に ping を送信する際の ICMP メッセージサイズ (バイト数)
};

// 動作モード
#define	SE_CONFIG_MODE_L2TRANS		0	// Layer-2 透過 (ブリッジ)
#define SE_CONFIG_MODE_L3TRANS		1	// Layer-3 透過 (透過的ルーティング)
#define SE_CONFIG_MODE_L3IPSEC		2	// IPsec 暗号化 (VPN)

// VPN
struct SE_VPN
{
	SE_VPN_CONFIG *Config;			// 設定データ
	SE_TIMER *Timer;				// タイマ
	SE_ETH *PhysicalEth;			// 物理 ETH
	SE_ETH *VirtualEth;				// 仮想 ETH
	bool StatusChanged;				// 状態が変化したフラグ
	SE_LOCK *MainLock;				// ロック
	bool Inited;					// 初期化完了
	UINT64 Tick64;					// 現在の Tick 値

	// IPv4
	SE_VPN4 *Vpn4;					// IPv4 VPN
	SE_IPV4 *IPv4_Physical;			// IPv4 (物理 ETH)
	SE_IPV4 *IPv4_Virtual;			// IPv4 (仮想 ETH)

	// IPv6
	SE_VPN6 *Vpn6;					// IPv6 VPN
	SE_IPV6 *IPv6_Physical;			// IPv6 (物理 ETH)
	SE_IPV6 *IPv6_Virtual;			// IPv6 (仮想 ETH)
};

// 関数プロトタイプ
SE_VPN_CONFIG *SeVpnLoadConfig(SE_LIST *o);
void SeVpnFreeConfig(SE_VPN_CONFIG *c);

SE_VPN *SeVpnInit(SE_HANDLE physical_nic_handle, SE_HANDLE virtual_nic_handle, char *config_name);
void SeVpnFree(SE_VPN *v);
void SeVpnEthRecvCallback(SE_ETH *eth, void *param);
void SeVpnTimerCallback(SE_TIMER *t, UINT64 current_tick, void *param);
void SeVpnMainInit(SE_VPN *v);
void SeVpnMainFree(SE_VPN *v);
void SeVpnMainHandler(SE_VPN *v);
void SeVpnMainProcess(SE_VPN *v);
void SeVpnAddTimer(SE_VPN *v, UINT interval);
void SeVpnStatusChanged(SE_VPN *v);
void SeVpnSendEtherPacket(SE_VPN *v, SE_ETH *e, void *packet, UINT packet_size);
void *SeVpnRecvEtherPacket(SE_VPN *v, SE_ETH *e);
void SeVpnMainProcRecvEtherPacket(SE_VPN *v, bool physical, void *packet, UINT packet_size);
UINT64 SeVpnTick(SE_VPN *v);
void SeVpnInitIpStack(SE_VPN *v);
void SeVpnFreeIpStack(SE_VPN *v);
bool SeVpnDoInterval(SE_VPN *v, UINT64 *var, UINT interval);

void SeVpnRecvIpV4PacketCallback(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size, void *param);

void SeVpnRecvIpV6PacketCallback(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size, void *param);



#endif	// SEVPN_H

