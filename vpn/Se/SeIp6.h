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

// SeIp6.h
// 概要: SeIp6.c のヘッダ

#ifndef	SEIP6_H
#define SEIP6_H

// 定数
#define SE_IPV6_RA_EXPIRES_DEFAULT	3600		// Router Advertisement で指定するプレフィックスの有効期限 (秒)
#define SE_IPV6_NEIGHBOR_EXPIRES_DEFAULT	60	// 近隣エントリキャッシュの有効期限 (秒)
#define SE_IPV6_NDP_SEND_INTERVAL	1			// NDP 送信間隔 (秒)
#define SE_IPV6_NDP_SEND_COUNT		5			// NDP 送信回数
#define SE_IPV6_COMBINE_INITIAL_BUF_SIZE	(4096)	// IP パケット結合のための初期バッファサイズ
#define SE_IPV6_COMBINE_QUEUE_SIZE_QUOTA	(1 * 1024 * 1024)	// IP パケットの結合のために使用することができるメモリサイズの上限
#define SE_IPV6_COMBINE_TIMEOUT		60			// IP パケット結合タイムアウト (秒)
#define SE_IPV6_COMBINE_MAX_COUNT	256			// IP パケット結合エントリ最大数
#define SE_IPV6_SEND_HOP_LIMIT		128			// 送信 IP パケットの Hop Limit 値
#define SE_IPV6_MAX_GUEST_NODES		16			// ゲストノードの数
#define SE_IPV6_RA_SEND_INTERVAL	30			// RA を送信する間隔

// 近隣エントリ
struct SE_IPV6_NEIGHBOR_ENTRY
{
	SE_IPV6_ADDR IpAddress;			// IP アドレス
	UCHAR MacAddress[6];			// MAC アドレス
	UCHAR Padding[2];
	UINT64 Created;					// 作成日時
	UINT64 Expire;					// 有効期限
};

// NDP 待機リスト
struct SE_NDPV6_WAIT
{
	SE_IPV6_ADDR IpAddress;			// 解決しようとしている IP アドレス
	UINT64 IntervalValue;			// インターバル値
	UINT SendCounter;				// 送信回数カウンタ
};

// IPv6 待機リスト
struct SE_IPV6_WAIT
{
	SE_IPV6_ADDR SrcIP;				// 送信元 IP アドレス
	SE_IPV6_ADDR DestIPLocal;		// 同一セグメント上での宛先 IP アドレス
	UINT64 Expire;					// 保管期限
	void *Data;						// データ
	UINT Size;						// サイズ
};

// IPv6 フラグメントリスト
struct SE_IPV6_FRAGMENT
{
	UINT Offset;					// オフセット
	UINT Size;						// サイズ
};

// IPv6 結合リスト
struct SE_IPV6_COMBINE
{
	SE_IPV6_ADDR DestIpAddress;		// 宛先 IP アドレス
	SE_IPV6_ADDR SrcIpAddress;		// 送信元 IP アドレス
	UINT Id;						// IP パケット ID
	UINT64 Expire;					// 保管期限
	void *Data;						// パケットデータ
	UINT DataReserved;				// データ用に確保された領域
	UINT Size;						// パケットサイズ (トータル)
	SE_LIST *IpFragmentList;		// IP フラグメントリスト
	UCHAR Protocol;					// プロトコル番号
	UCHAR HopLimit;					// Hop Limit
	UCHAR SrcMacAddress[6];			// 送信元 MAC アドレス
	int combine_id;				// 結合 ID
};

// ICMPv6 ヘッダ情報
struct SE_ICMPV6_HEADER_INFO
{
	UCHAR Type;
	UCHAR Code;
	USHORT DataSize;
	void *Data;
	SE_ICMP_ECHO EchoHeader;
	void *EchoData;
	UINT EchoDataSize;

	union
	{
		SE_ICMPV6_ROUTER_SOLICIATION_HEADER *RouterSoliciationHeader;
		SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER *RouterAdvertisementHeader;
		SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER *NeighborSoliciationHeader;
		SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER *NeighborAdvertisementHeader;
		void *HeaderPointer;
	} Headers;

	SE_ICMPV6_OPTION_LIST OptionList;
};

// UDPv6 ヘッダ情報
struct SE_UDPV6_HEADER_INFO
{
	USHORT SrcPort;
	USHORT DstPort;
	USHORT DataSize;
	void *Data;
};

// IPv6 ヘッダ情報
struct SE_IPV6_HEADER_INFO
{
	bool IsRawIpPacket;
	USHORT Size;
	UINT Id;
	UCHAR Protocol;
	UCHAR HopLimit;
	SE_IPV6_ADDR SrcIpAddress;			// ※ Raw IP でも有効
	SE_IPV6_ADDR DestIpAddress;			// ※ Raw IP でも有効
	bool UnicastForMe;
	bool UnicastForRouting;
	bool UnicastForRoutingWithProxyNdp;
	bool IsBroadcast;
	UINT TypeL4;
	SE_UDPV6_HEADER_INFO *UDPv6Info;
	SE_ICMPV6_HEADER_INFO *ICMPv6Info;
};

// IPv6 インターフェイス
struct SE_IPV6
{
	SE_VPN *Vpn;
	SE_ETH *Eth;
	bool Physical;
	SE_IPV6_RECV_CALLBACK *RecvCallback;	// 受信コールバック
	void *RecvCallbackParam;		// 受信コールバックパラメータ
	SE_IPV6_ADDR GlobalIpAddress;	// グローバル IP アドレス
	SE_IPV6_ADDR SubnetMask;		// サブネットマスク
	SE_IPV6_ADDR LocalIpAddress;	// ローカル IP アドレス
	SE_IPV6_ADDR PrefixAddress;		// プレフィックスアドレス
	bool UseDefaultGateway;			// デフォルトゲートウェイを使用するかどうか
	bool UseProxyNdp;				// プロキシ NDP を使用するかどうか
	bool UseRawIp;					// Raw IP を使用するかどうか
	bool SendRa;					// RA を送信するかどうか
	SE_IPV6_ADDR DefaultGateway;	// デフォルトゲートウェイ
	UINT Mtu;						// MTU
	SE_LIST *NeighborEntryList;		// 近隣エントリリスト
	SE_LIST *NdpWaitList;			// 近隣待機リスト
	SE_LIST *IpWaitList;			// IP 待機リスト
	SE_LIST *IpCombineList;			// IP 復元リスト
	int combine_current_id;			// 現在の結合 ID
	UINT CurrentIpQuota;			// IP 復元に使用できるメモリ使用量
	UINT IdSeed;					// ID 生成用の値
	SE_IPV6_ADDR GuestNodes[SE_IPV6_MAX_GUEST_NODES];	// ゲストノード一覧
	UINT GuestNodeIndexSeed;		// ゲストノード書き込み用インデックス
	UINT64 VarRaSendInterval;		// RA 送信用間隔変数
};

// アドレスの種類
#define SE_IPV6_ADDR_UNICAST				1	// ユニキャスト
#define SE_IPV6_ADDR_LOCAL_UNICAST			2	// ローカルユニキャスト
#define SE_IPV6_ADDR_GLOBAL_UNICAST			4	// グローバルユニキャスト
#define SE_IPV6_ADDR_MULTICAST				8	// マルチキャスト
#define SE_IPV6_ADDR_ALL_NODE_MULTICAST		16	// 全ノードマルチキャスト
#define SE_IPV6_ADDR_ALL_ROUTER_MULTICAST	32	// 全ルータマルチキャスト
#define SE_IPV6_ADDR_SOLICIATION_MULTICAST	64	// 要請ノードマルチキャスト



// 関数プロトタイプ
SE_IPV6_ADDR Se6ZeroIP();
SE_IPV6_ADDR Se6StrToIP(char *str);
bool Se6CheckSubnetLength(UINT i);
void Se6IPToStr(char *str, SE_IPV6_ADDR a);
bool Se6CheckIPItemStr(char *str);
void Se6IPItemStrToChars(UCHAR *chars, char *str);
bool Se6IsZeroIP(SE_IPV6_ADDR a);
int Se6Cmp(SE_IPV6_ADDR a, SE_IPV6_ADDR b);
SE_IPV6_ADDR Se6GenerateSubnetMask(UINT i);
SE_IPV6_ADDR Se6And(SE_IPV6_ADDR a, SE_IPV6_ADDR b);
SE_IPV6_ADDR Se6Or(SE_IPV6_ADDR a, SE_IPV6_ADDR b);
SE_IPV6_ADDR Se6Not(SE_IPV6_ADDR a);
UINT Se6GetIPAddrType(SE_IPV6_ADDR a);
void Se6GenerateMulticastMacAddress(UCHAR *mac, SE_IPV6_ADDR a);
SE_IPV6_ADDR Se6GetAllNodeMulticastAddr();
SE_IPV6_ADDR Se6GetAllRouterMulticastAddr();
SE_IPV6_ADDR Se6GetSoliciationMulticastAddr(SE_IPV6_ADDR a);
bool Se6CheckUnicastAddress(SE_IPV6_ADDR a);
bool Se6IsNetworkPrefixAddress(SE_IPV6_ADDR a, SE_IPV6_ADDR subnet);
SE_IPV6_ADDR Se6GetPrefixAddress(SE_IPV6_ADDR a, SE_IPV6_ADDR subnet);
SE_IPV6_ADDR Se6GetHostAddress(SE_IPV6_ADDR a, SE_IPV6_ADDR subnet);
bool Se6IsInSameNetwork(SE_IPV6_ADDR a1, SE_IPV6_ADDR a2, SE_IPV6_ADDR subnet);
void Se6GenerateEui64Address(UCHAR *dst, UCHAR *mac);
SE_IPV6_ADDR Se6GenerateEui64LocalAddress(UCHAR *mac);
SE_IPV6_ADDR Se6GenerateEui64GlobalAddress(SE_IPV6_ADDR prefix, SE_IPV6_ADDR subnet, UCHAR *mac);
bool Se6IsSubnetMask(SE_IPV6_ADDR a);
UINT Se6SubnetMaskToInt(SE_IPV6_ADDR a);
SE_IPV6_ADDR Se6LocalSubnet();

SE_IPV6 *Se6Init(SE_VPN *vpn, SE_ETH *eth, bool physical, SE_IPV6_ADDR global_ip, SE_IPV6_ADDR subnet,
				 SE_IPV6_ADDR gateway, UINT mtu, SE_IPV6_RECV_CALLBACK *recv_callback, void *recv_callback_param);
void Se6Free(SE_IPV6 *p);
UINT64 Se6Tick(SE_IPV6 *p);
void Se6MainProc(SE_IPV6 *p);

SE_LIST *Se6InitIpFragmentList();
void Se6FreeIpFragmentList(SE_LIST *o);

SE_LIST *Se6InitIpCombineList();
SE_IPV6_COMBINE *Se6InsertIpCombine(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip,
									UINT id, UCHAR protocol, UCHAR hop_limit, UCHAR *src_mac);
void Se6FreeIpCombine(SE_IPV6 *p, SE_IPV6_COMBINE *c);
void Se6FlushIpCombineList(SE_IPV6 *p, SE_LIST *o);
void Se6CombineIp(SE_IPV6 *p, SE_IPV6_COMBINE *c, UINT offset, void *data, UINT size, bool last_packet);
void Se6FreeIpCombineList(SE_IPV6 *p, SE_LIST *o);
int Se6CmpIpCombineList(void *p1, void *p2);
SE_IPV6_COMBINE *Se6SearchIpCombineList(SE_LIST *o, SE_IPV6_ADDR dest, SE_IPV6_ADDR src, UINT id, UCHAR protocol);

SE_LIST *Se6InitIpWaitList();
void Se6InsertIpWait(SE_LIST *o, UINT64 tick, SE_IPV6_ADDR dest_ip_local, SE_IPV6_ADDR src_ip, void *data, UINT size);
void Se6FlushIpWaitList(SE_IPV6 *p);
void Se6FreeIpWait(SE_IPV6_WAIT *w);
void Se6FreeIpWaitList(SE_LIST *o);
void Se6SendWaitingIpWait(SE_IPV6 *p, SE_IPV6_ADDR ip_addr_local, UCHAR *mac_addr);

SE_LIST *Se6InitNdpWaitList();
int Se6CmpNdpWaitEntry(void *p1, void *p2);
SE_NDPV6_WAIT *Se6SearchNdpWaitList(SE_LIST *o, SE_IPV6_ADDR a);
void Se6FreeNdpWaitList(SE_LIST *o);

SE_LIST *Se6InitNeighborEntryList();
int Se6CmpNeighborEntry(void *p1, void *p2);
void Se6AddNeighborEntryList(SE_LIST *o, UINT64 tick, UINT expire_span, SE_IPV6_ADDR ip_address, UCHAR *mac_address);
SE_IPV6_NEIGHBOR_ENTRY *Se6SearchNeighborEntryList(SE_LIST *o, SE_IPV6_ADDR a, UINT64 tick);
void Se6FlushNeighborEntryList(SE_LIST *o, UINT64 tick);
void Se6FreeNeighborEntryList(SE_LIST *o);

void Se6RecvEthPacket(SE_IPV6 *p, void *packet, UINT packet_size);
void Se6SendEthPacket(SE_IPV6 *p, UCHAR *dest_mac, USHORT protocol, void *data, UINT data_size);
UCHAR *Se6BroadcastMacAddress();

void Se6MacIpRelationKnown(SE_IPV6 *p, SE_IPV6_ADDR ip_addr, UCHAR *mac_addr);

USHORT Se6IpChecksum(void *buf, UINT size);

void Se6SendNeighborSoliciation(SE_IPV6 *p, SE_IPV6_ADDR target_ip);
bool Se6SendNeighborSoliciationDoProcess(SE_IPV6 *p, SE_NDPV6_WAIT *w);
void Se6ProcessNdpWaitList(SE_IPV6 *p);

void Se6RecvIp(SE_IPV6 *p, SE_PACKET *pkt);

void Se6FreeIpHeaderInfo(SE_IPV6_HEADER_INFO *info);
void Se6RecvIpComplete(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UINT id, UCHAR protocol,
					   UCHAR hop_limit, void *data, UINT size, UCHAR *src_mac);
bool Se6ParseIpPacket(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip,
					  UINT id, UCHAR protocol, UCHAR hop_limit, void *data, UINT size);
bool Se6ParseIpPacketICMPv6(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size);
bool Se6ParseIpPacketUDPv6(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size);

USHORT Se6CalcChecksum(SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR protocol, void *data, UINT size);

void Se6SendIp(SE_IPV6 *p, SE_IPV6_ADDR dest_ip, SE_IPV6_ADDR src_ip, UCHAR protocol, UCHAR hop_limit, void *data,
			   UINT size, UCHAR *dest_mac);
void Se6SendIpFragment(SE_IPV6 *p, void *data, UINT size, UCHAR *dest_mac);
void Se6SendRawIp(SE_IPV6 *p, void *data, UINT size, UCHAR *dest_mac);
void Se6SendIpFragmentNow(SE_IPV6 *p, UCHAR *dest_mac, void *data, UINT size);
void Se6SendUdp(SE_IPV6 *p, SE_IPV6_ADDR dest_ip, UINT dest_port, SE_IPV6_ADDR src_ip, UINT src_port,
				void *data, UINT size, UCHAR *dest_mac);
void Se6SendIcmp(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR hop_limit, UCHAR type, UCHAR code, void *data, UINT size, UCHAR *dest_mac);
void Se6SendIcmpEchoRequest(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, USHORT id,
							USHORT seq_no, void *data, UINT size);
void Se6SendIcmpEchoResponse(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, USHORT id,
							 USHORT seq_no, void *data, UINT size);
SE_BUF *Se6BuildIcmpEchoPacket(SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR type, UCHAR code, USHORT id, USHORT seq_no, void *data, UINT size);
void Se6SendIcmpNeighborSoliciation(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR target_ip);
void Se6SendIcmpNeighborAdvertisement(SE_IPV6 *p, bool router, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR *mac, UCHAR *dest_mac);
void Se6SendIcmpRouterAdvertisement(SE_IPV6 *p);

bool Se6IsGuestNode(SE_IPV6 *p, SE_IPV6_ADDR a);
void Se6AddGuestNode(SE_IPV6 *p, SE_IPV6_ADDR a);

#endif	// SEIP6_H


