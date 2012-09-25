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

// SeIp4.h
// 概要: SeIp4.c のヘッダ

#ifndef	SEIP4_H
#define SEIP4_H

// 定数
#define SE_IPV4_DHCP_EXPIRES_DEFAULT	3600	// DHCPv4 サービスで割り当てる IPv4 アドレスの有効期限 (秒)
#define SE_IPV4_ARP_EXPIRES_DEFAULT	60			// ARP エントリキャッシュの有効期限 (秒)
#define SE_IPV4_ARP_SEND_INTERVAL	1			// ARP 送信間隔 (秒)
#define SE_IPV4_ARP_SEND_COUNT		5			// ARP 送信回数
#define SE_IPV4_COMBINE_INITIAL_BUF_SIZE	(4096)	// IP パケット結合のための初期バッファサイズ
#define SE_IPV4_COMBINE_QUEUE_SIZE_QUOTA	(1 * 1024 * 1024)	// IP パケットの結合のために使用することができるメモリサイズの上限
#define SE_IPV4_COMBINE_TIMEOUT		60			// IP パケット結合タイムアウト (秒)
#define SE_IPV4_COMBINE_MAX_COUNT	256			// IP パケット結合エントリ最大数
#define SE_IPV4_SEND_TTL			128			// 送信 IP パケットの TTL 値
#define SE_IPV4_DHCP_SERVER_PORT	67			// DHCP サーバーポート
#define	SE_IPV4_DHCP_CLIENT_PORT	68			// DHCP クライアントポート


// ARPv4 エントリ
struct SE_ARPV4_ENTRY
{
	SE_IPV4_ADDR IpAddress;			// IP アドレス
	UCHAR MacAddress[6];			// MAC アドレス
	UCHAR Padding[2];
	UINT64 Created;					// 作成日時
	UINT64 Expire;					// 有効期限
};

// ARPv4 待機リスト
struct SE_ARPV4_WAIT
{
	SE_IPV4_ADDR IpAddress;			// 解決しようとしている IP アドレス
	UINT64 IntervalValue;			// インターバル値
	UINT SendCounter;				// 送信回数カウンタ
};

// IPv4 待機リスト
struct SE_IPV4_WAIT
{
	SE_IPV4_ADDR SrcIP;				// 送信元 IP アドレス
	SE_IPV4_ADDR DestIPLocal;		// 同一セグメント上での宛先 IP アドレス
	UINT64 Expire;					// 保管期限
	void *Data;						// データ
	UINT Size;						// サイズ
};

// IPv4 フラグメントリスト
struct SE_IPV4_FRAGMENT
{
	UINT Offset;					// オフセット
	UINT Size;						// サイズ
};

// IPv4 結合リスト
struct SE_IPV4_COMBINE
{
	SE_IPV4_ADDR DestIpAddress;		// 宛先 IP アドレス
	SE_IPV4_ADDR SrcIpAddress;		// 送信元 IP アドレス
	USHORT Id;						// IP パケット ID
	UCHAR Padding1[2];
	UINT64 Expire;					// 保管期限
	void *Data;						// パケットデータ
	UINT DataReserved;				// データ用に確保された領域
	UINT Size;						// パケットサイズ (トータル)
	SE_LIST *IpFragmentList;		// IP フラグメントリスト
	UCHAR Protocol;					// プロトコル番号
	UCHAR Ttl;						// TTL
	UCHAR Padding2[3];
	bool IsBroadcast;				// ブロードキャストパケット
	int combine_id;				// 結合 ID
};

// DHCPv4 オプション
struct SE_DHCPV4_OPTION
{
	UINT Id;						// ID
	UINT Size;						// サイズ
	void *Data;						// データ
};

// DHCPv4 オプションリスト
struct SE_DHCPV4_OPTION_LIST
{
	// 共通項目
	UINT Opcode;					// DHCP オペコード

	// クライアント要求
	SE_IPV4_ADDR RequestedIp;		// 要求された IP アドレス
	char Hostname[MAX_SIZE];		// ホスト名

	// サーバー応答
	SE_IPV4_ADDR ServerAddress;		// DHCP サーバーアドレス
	UINT LeaseTime;					// リース時間
	char DomainName[MAX_SIZE];		// ドメイン名
	SE_IPV4_ADDR SubnetMask;		// サブネットマスク
	SE_IPV4_ADDR Gateway;			// ゲートウェイアドレス
	SE_IPV4_ADDR DnsServer;			// DNS サーバーアドレス
	UINT Mtu;						// MTU の値
};

// ICMPv4 ヘッダ情報
struct SE_ICMPV4_HEADER_INFO
{
	UCHAR Type;
	UCHAR Code;
	USHORT DataSize;
	void *Data;
	SE_ICMP_ECHO EchoHeader;
	void *EchoData;
	UINT EchoDataSize;
};

// UDPv4 ヘッダ情報
struct SE_UDPV4_HEADER_INFO
{
	USHORT SrcPort;
	USHORT DstPort;
	USHORT DataSize;
	void *Data;
};

// IPv4 ヘッダ情報
struct SE_IPV4_HEADER_INFO
{
	bool IsRawIpPacket;
	USHORT Size;
	USHORT Id;
	UCHAR Protocol;
	UCHAR Ttl;
	SE_IPV4_ADDR SrcIpAddress;			// ※ Raw IP でも有効
	SE_IPV4_ADDR DestIpAddress;			// ※ Raw IP でも有効
	bool UnicastForMe;
	bool UnicastForRouting;
	bool UnicastForRoutingWithProxyArp;
	bool IsBroadcast;
	UINT TypeL4;
	SE_UDPV4_HEADER_INFO *UDPv4Info;
	SE_ICMPV4_HEADER_INFO *ICMPv4Info;
};

// IPv4 インターフェイス
struct SE_IPV4
{
	SE_VPN *Vpn;
	SE_ETH *Eth;
	bool Physical;
	SE_IPV4_RECV_CALLBACK *RecvCallback;	// 受信コールバック
	void *RecvCallbackParam;		// 受信コールバックパラメータ
	SE_IPV4_ADDR IpAddress;			// IP アドレス
	SE_IPV4_ADDR SubnetMask;		// サブネットマスク
	SE_IPV4_ADDR NetworkAddress;	// ネットワークアドレス
	SE_IPV4_ADDR BroadcastAddress;	// ブロードキャストアドレス
	bool UseDefaultGateway;			// デフォルトゲートウェイを使用するかどうか
	bool UseProxyArp;				// プロキシ ARP を使用するかどうか
	bool UseRawIp;					// Raw IP を使用するかどうか
	SE_IPV4_ADDR ProxyArpExceptionAddress;	// プロキシ ARP 応答を返答しない IP アドレス
	SE_IPV4_ADDR DefaultGateway;	// デフォルトゲートウェイ
	UINT Mtu;						// MTU
	SE_LIST *ArpEntryList;			// ARP エントリリスト
	SE_LIST *ArpWaitList;			// ARP 待機リスト
	SE_LIST *IpWaitList;			// IP 待機リスト
	SE_LIST *IpCombineList;			// IP 復元リスト
	int combine_current_id;			// 現在の結合 ID
	UINT CurrentIpQuota;			// IP 復元に使用できるメモリ使用量
	USHORT IdSeed;					// ID 生成用の値
};

// 関数プロトタイプ
SE_IPV4_ADDR Se4ZeroIP();
UINT Se4IPToUINT(SE_IPV4_ADDR a);
SE_IPV4_ADDR Se4UINTToIP(UINT v);
SE_IPV4_ADDR Se4StrToIP(char *str);
UINT Se4StrToIP32(char *str);
void Se4IPToStr(char *str, SE_IPV4_ADDR a);
void Se4IPToStr32(char *str, UINT v);
UINT Se4GenerateSubnetMask32(UINT i);
SE_IPV4_ADDR Se4GenerateSubnetMask(UINT i);
bool Se4IsZeroIP(SE_IPV4_ADDR a);
bool Se4IsSubnetMask(SE_IPV4_ADDR a);
int Se4Cmp(SE_IPV4_ADDR a, SE_IPV4_ADDR b);
SE_IPV4_ADDR Se4GetNetworkAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet);
SE_IPV4_ADDR Se4GetHostAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet);
bool Se4CheckUnicastAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet);
bool Se4IsInSameNetwork(SE_IPV4_ADDR a1, SE_IPV4_ADDR a2, SE_IPV4_ADDR subnet);
SE_IPV4_ADDR Se4GetBroadcastAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet);
bool Se4IsBroadcastAddress(SE_IPV4_ADDR addr);
SE_IPV4_ADDR Se4And(SE_IPV4_ADDR a, SE_IPV4_ADDR b);
SE_IPV4_ADDR Se4Or(SE_IPV4_ADDR a, SE_IPV4_ADDR b);
SE_IPV4_ADDR Se4Not(SE_IPV4_ADDR a);
USHORT Se4IpChecksum(void *buf, UINT size);
bool Se4IpCheckChecksum(SE_IPV4_HEADER *ip);

int Se4CmpArpEntry(void *p1, void *p2);
SE_LIST *Se4InitArpEntryList();
void Se4FreeArpEntryList(SE_LIST *o);
SE_ARPV4_ENTRY *Se4SearchArpEntryList(SE_LIST *o, SE_IPV4_ADDR a, UINT64 tick, UINT expire_span, bool dont_update_expire);
void Se4FlushArpEntryList(SE_LIST *o, UINT64 tick);
void Se4AddArpEntryList(SE_LIST *o, UINT64 tick, UINT expire_span, SE_IPV4_ADDR ip_address, UCHAR *mac_address);

int Se4CmpArpWaitEntry(void *p1, void *p2);
SE_LIST *Se4InitArpWaitList();
void Se4FreeArpWaitList(SE_LIST *o);
SE_ARPV4_WAIT *Se4SearchArpWaitList(SE_LIST *o, SE_IPV4_ADDR a);

SE_LIST *Se4InitIpWaitList();
void Se4FreeIpWaitList(SE_LIST *o);
void Se4FreeIpWait(SE_IPV4_WAIT *w);
void Se4InsertIpWait(SE_LIST *o, UINT64 tick, SE_IPV4_ADDR dest_ip_local, SE_IPV4_ADDR src_ip, void *data, UINT size);
void Se4FlushIpWaitList(SE_IPV4 *p);
void Se4SendWaitingIpWait(SE_IPV4 *p, SE_IPV4_ADDR ip_addr_local, UCHAR *mac_addr);

SE_LIST *Se4InitIpCombineList();
UINT64 Se4Tick(SE_IPV4 *p);
void Se4FreeIpCombineList(SE_IPV4 *p, SE_LIST *o);
SE_IPV4_COMBINE *Se4SearchIpCombineList(SE_LIST *o, SE_IPV4_ADDR dest, SE_IPV4_ADDR src, USHORT id, UCHAR protocol);
void Se4FreeIpCombine(SE_IPV4 *p, SE_IPV4_COMBINE *c);
SE_LIST *Se4InitIpFragmentList();
void Se4FreeIpFragmentList(SE_LIST *o);
SE_IPV4_COMBINE *Se4InsertIpCombine(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip,
									USHORT id, UCHAR protocol, UCHAR ttl, bool is_broadcast);
void Se4CombineIp(SE_IPV4 *p, SE_IPV4_COMBINE *c, UINT offset, void *data, UINT size, bool last_packet);
void Se4FlushIpCombineList(SE_IPV4 *p, SE_LIST *o);

SE_IPV4 *Se4Init(SE_VPN *vpn, SE_ETH *eth, bool physical, SE_IPV4_ADDR ip, SE_IPV4_ADDR subnet,
				 SE_IPV4_ADDR gateway, UINT mtu, SE_IPV4_RECV_CALLBACK *recv_callback, void *recv_callback_param);
void Se4Free(SE_IPV4 *p);
UINT64 Se4Tick(SE_IPV4 *p);
void Se4MainProc(SE_IPV4 *p);

void Se4MacIpRelationKnown(SE_IPV4 *p, SE_IPV4_ADDR ip_addr, UCHAR *mac_addr);

void Se4RecvEthPacket(SE_IPV4 *p, void *packet, UINT packet_size);
void Se4SendEthPacket(SE_IPV4 *p, UCHAR *dest_mac, USHORT protocol, void *data, UINT data_size);
UCHAR *Se4BroadcastMacAddress();

void Se4RecvArp(SE_IPV4 *p, SE_PACKET *pkt);
void Se4RecvArpRequest(SE_IPV4 *p, SE_PACKET *pkt, SE_ARPV4_HEADER *arp);
void Se4RecvArpResponse(SE_IPV4 *p, SE_PACKET *pkt, SE_ARPV4_HEADER *arp);
void Se4SendArpResponse(SE_IPV4 *p, UCHAR *dest_mac, SE_IPV4_ADDR dest_ip, SE_IPV4_ADDR src_ip);
void Se4SendArpRequest(SE_IPV4 *p, SE_IPV4_ADDR ip);
bool Se4SendArpDoProcess(SE_IPV4 *p, SE_ARPV4_WAIT *w);
void Se4ProcessArpWaitList(SE_IPV4 *p);

void Se4RecvIp(SE_IPV4 *p, SE_PACKET *pkt);
void Se4RecvIpComplete(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, UCHAR procotol, UCHAR ttl, void *data, UINT size, bool is_broadcast);
bool Se4ParseIpPacket(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, UCHAR procotol, UCHAR ttl, void *data, UINT size, bool is_broadcast);
bool Se4ParseIpPacketICMPv4(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size);
bool Se4ParseIpPacketUDPv4(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size);
void Se4FreeIpHeaderInfo(SE_IPV4_HEADER_INFO *info);
void Se4SendIp(SE_IPV4 *p, SE_IPV4_ADDR dest_ip, SE_IPV4_ADDR src_ip, UCHAR protocol, UCHAR ttl, void *data, UINT size, UCHAR *dest_mac);
void Se4SendIpFragment(SE_IPV4 *p, SE_IPV4_ADDR dest_ip, SE_IPV4_ADDR src_ip,
					   USHORT id, USHORT total_size, USHORT offset, UCHAR protocol, UCHAR ttl,
					   void *data, UINT size, UCHAR *dest_mac);
void Se4SendRawIp(SE_IPV4 *p, void *data, UINT size, UCHAR *dest_mac);
void Se4SendIpFragmentNow(SE_IPV4 *p, UCHAR *dest_mac, void *data, UINT size);
void Se4SendIcmp(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, UCHAR type, UCHAR code, void *data, UINT size);
SE_BUF *Se4BuildIcmpEchoPacket(UCHAR type, UCHAR code, USHORT id, USHORT seq_no, void *data, UINT size);
void Se4SendIcmpEchoResponse(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, USHORT seq_no, void *data, UINT size);
void Se4SendIcmpEchoRequest(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, USHORT seq_no, void *data, UINT size);
void Se4SendUdp(SE_IPV4 *p, SE_IPV4_ADDR dest_ip, UINT dest_port, SE_IPV4_ADDR src_ip, UINT src_port, void *data, UINT size, UCHAR *dest_mac);

bool Se4AdjustTcpMss(void *src_data, UINT src_size, UINT mss);

#endif	// SEIP4_H


