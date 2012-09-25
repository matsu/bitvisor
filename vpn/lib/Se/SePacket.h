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

// SePacket.h
// 概要: SePacket.c のヘッダ

#ifndef	SEPACKET_H
#define SEPACKET_H

#ifdef	SE_WIN32
#pragma pack(push, 1)
#endif	// SE_WIN32

// MAC ヘッダ
struct SE_MAC_HEADER
{
	UCHAR	DestAddress[6];			// 宛先 MAC アドレス
	UCHAR	SrcAddress[6];			// 送信元 MAC アドレス
	USHORT	Protocol;				// プロトコル
} SE_STRUCT_PACKED;

// MAC プロトコル
#define	SE_MAC_PROTO_ARPV4		0x0806				// ARPv4 パケット
#define	SE_MAC_PROTO_IPV4		0x0800				// IPv4 パケット
#define	SE_MAC_PROTO_IPV6		0x86dd				// IPv6 パケット

// ARPv4 ヘッダ
struct SE_ARPV4_HEADER
{
	USHORT	HardwareType;			// ハードウェアタイプ
	USHORT	ProtocolType;			// プロトコルタイプ
	UCHAR	HardwareSize;			// ハードウェアサイズ
	UCHAR	ProtocolSize;			// プロトコルサイズ
	USHORT	Operation;				// オペレーション
	UCHAR	SrcAddress[6];			// 送信元 MAC アドレス
	UINT	SrcIP;					// 送信元 IP アドレス
	UCHAR	TargetAddress[6];		// ターゲット MAC アドレス
	UINT	TargetIP;				// ターゲット IP アドレス
} SE_STRUCT_PACKED;

// ARPv4 ハードウェア種類
#define	SE_ARPV4_HARDWARE_TYPE_ETHERNET		0x0001

// ARPv4 オペレーション種類
#define	SE_ARPV4_OPERATION_REQUEST			1
#define	SE_ARPV4_OPERATION_RESPONSE			2

// IPv4 アドレス
struct SE_IPV4_ADDR
{
	UCHAR Value[4];					// 値
} SE_STRUCT_PACKED;

// IPv4 ヘッダ
struct SE_IPV4_HEADER
{
	UCHAR	VersionAndHeaderLength;		// バージョンとヘッダサイズ
	UCHAR	TypeOfService;				// サービスタイプ
	USHORT	TotalLength;				// 合計サイズ
	USHORT	Identification;				// 識別子
	UCHAR	FlagsAndFlagmentOffset[2];	// フラグとフラグメントオフセット
	UCHAR	TimeToLive;					// TTL
	UCHAR	Protocol;					// プロトコル
	USHORT	Checksum;					// チェックサム
	UINT	SrcIP;						// 送信元 IP アドレス
	UINT	DstIP;						// 宛先 IP アドレス
} SE_STRUCT_PACKED;

// IPv4 ヘッダ操作用マクロ
#define	SE_IPV4_GET_VERSION(h)			(((h)->VersionAndHeaderLength >> 4 & 0x0f))
#define	SE_IPV4_SET_VERSION(h, v)		((h)->VersionAndHeaderLength |= (((v) & 0x0f) << 4))
#define	SE_IPV4_GET_HEADER_LEN(h)		((h)->VersionAndHeaderLength & 0x0f)
#define	SE_IPV4_SET_HEADER_LEN(h, v)	((h)->VersionAndHeaderLength |= ((v) & 0x0f))

// IPv4 フラグメント関係操作用マクロ
#define	SE_IPV4_GET_FLAGS(h)			(((h)->FlagsAndFlagmentOffset[0] >> 5) & 0x07)
#define	SE_IPV4_SET_FLAGS(h, v)		((h)->FlagsAndFlagmentOffset[0] |= (((v) & 0x07) << 5))
#define	SE_IPV4_GET_OFFSET(h)			(((h)->FlagsAndFlagmentOffset[0] & 0x1f) * 256 + ((h)->FlagsAndFlagmentOffset[1]))
#define	SE_IPV4_SET_OFFSET(h, v)		{(h)->FlagsAndFlagmentOffset[0] |= (UCHAR)((v) / 256); (h)->FlagsAndFlagmentOffset[1] = (UCHAR)((v) % 256);}

// IP プロトコル番号
#define	SE_IP_PROTO_ICMPV4			0x01	// ICMPv4 プロトコル
#define SE_IP_PROTO_ICMPV6			0x3a	// ICMPv6 プロトコル
#define	SE_IP_PROTO_UDP				0x11	// UDP プロトコル
#define	SE_IP_PROTO_ESP				0x32	// ESP プロトコル
#define SE_IP_PROTO_TCP				0x06	// TCP プロトコル

// UDP ヘッダ
struct SE_UDP_HEADER
{
	USHORT	SrcPort;				// 送信元ポート番号
	USHORT	DstPort;				// 宛先ポート番号
	USHORT	PacketLength;			// データ長
	USHORT	Checksum;				// チェックサム
} SE_STRUCT_PACKED;

// UDPv4 擬似ヘッダ
struct SE_UDPV4_PSEUDO_HEADER
{
	UINT	SrcIP;					// 送信元 IP アドレス
	UINT	DstIP;					// 宛先 IP アドレス
	UCHAR	Reserved;				// 未使用
	UCHAR	Protocol;				// プロトコル番号
	USHORT	PacketLength1;			// UDP データ長 1
	USHORT	SrcPort;				// 送信元ポート番号
	USHORT	DstPort;				// 宛先ポート番号
	USHORT	PacketLength2;			// UDP データ長 2
	USHORT	Checksum;				// チェックサム
} SE_STRUCT_PACKED;

// ICMP ヘッダ
struct SE_ICMP_HEADER
{
	UCHAR	Type;					// タイプ
	UCHAR	Code;					// コード
	USHORT	Checksum;				// チェックサム
} SE_STRUCT_PACKED;

#define	SE_ICMPV4_TYPE_ECHO_REQUEST				8		// ICMPv4 Echo 要求
#define	SE_ICMPV4_TYPE_ECHO_RESPONSE			0		// ICMPv4 Echo 応答

// ICMP Echo データ
struct SE_ICMP_ECHO
{
	USHORT	Identifier;						// ID
	USHORT	SeqNo;							// シーケンス番号
} SE_STRUCT_PACKED;

// TCPv4 擬似ヘッダ
struct SE_TCPV4_PSEUDO_HEADER
{
	UINT	SrcIP;					// 送信元 IP アドレス
	UINT	DstIP;					// 宛先 IP アドレス
	UCHAR	Reserved;				// 未使用
	UCHAR	Protocol;				// プロトコル番号
	USHORT	PacketLength;			// TCP データ長 1
} SE_STRUCT_PACKED;

// TCP ヘッダ
struct SE_TCP_HEADER
{
	USHORT	SrcPort;				// 送信元ポート番号
	USHORT	DstPort;				// 宛先ポート番号
	UINT	SeqNumber;				// シーケンス番号
	UINT	AckNumber;				// 確認応答番号
	UCHAR	HeaderSizeAndReserved;	// ヘッダサイズと予約領域
	UCHAR	Flag;					// フラグ
	USHORT	WindowSize;				// ウインドウサイズ
	USHORT	Checksum;				// チェックサム
	USHORT	UrgentPointer;			// 緊急ポインタ
} SE_STRUCT_PACKED;

// TCP マクロ
#define	SE_TCP_GET_HEADER_SIZE(h)		(((h)->HeaderSizeAndReserved >> 4) & 0x0f)
#define	SE_TCP_SET_HEADER_SIZE(h, v)	((h)->HeaderSizeAndReserved = (((v) & 0x0f) << 4))

// TCP フラグ
#define	SE_TCP_FIN					1
#define	SE_TCP_SYN					2
#define	SE_TCP_RST					4
#define	SE_TCP_PSH					8
#define	SE_TCP_ACK					16
#define	SE_TCP_URG					32

// DHCPv4 ヘッダ
struct SE_DHCPV4_HEADER
{
	UCHAR	OpCode;					// オペコード
	UCHAR	HardwareType;			// ハードウェア種類
	UCHAR	HardwareAddressSize;	// ハードウェアアドレスサイズ
	UCHAR	Hops;					// ホップ数
	UINT	TransactionId;			// トランザクション ID
	USHORT	Seconds;				// 秒数
	USHORT	Flags;					// フラグ
	UINT	ClientIP;				// クライアント IP アドレス
	UINT	YourIP;					// 割り当て IP アドレス
	UINT	ServerIP;				// サーバー IP アドレス
	UINT	RelayIP;				// リレー IP アドレス
	UCHAR	ClientMacAddress[6];	// クライアント MAC アドレス
	UCHAR	Padding[10];			// Ethernet 以外のためにパディング
} SE_STRUCT_PACKED;

#define	SE_DHCPV4_MAGIC_COOKIE	0x63825363	// Magic Cookie (固定)

// IPv6 アドレス
struct SE_IPV6_ADDR
{
	UCHAR Value[16];					// 値
} SE_STRUCT_PACKED;

// IPv6 ヘッダパケット情報
struct SE_IPV6_HEADER_PACKET_INFO
{
	SE_IPV6_HEADER *IPv6Header;					// IPv6 ヘッダ
	SE_IPV6_OPTION_HEADER *HopHeader;			// ホップバイホップオプションヘッダ
	UINT HopHeaderSize;							// ホップバイホップオプションヘッダサイズ
	SE_IPV6_OPTION_HEADER *EndPointHeader;		// 終点オプションヘッダ
	UINT EndPointHeaderSize;					// 終点オプションヘッダサイズ
	SE_IPV6_OPTION_HEADER *RoutingHeader;		// ルーティングヘッダ
	UINT RoutingHeaderSize;						// ルーティングヘッダサイズ
	SE_IPV6_FRAGMENT_HEADER *FragmentHeader;	// フラグメントヘッダ
	void *Payload;								// ペイロード
	UINT PayloadSize;							// ペイロードサイズ
	UCHAR Protocol;								// ペイロードプロトコル
};

// IPv6 ヘッダ
struct SE_IPV6_HEADER
{
	UCHAR VersionAndTrafficClass1;		// バージョン番号 (4 bit) とトラフィッククラス 1 (4 bit)
	UCHAR TrafficClass2AndFlowLabel1;	// トラフィッククラス 2 (4 bit) とフローラベル 1 (4 bit)
	UCHAR FlowLabel2;					// フローラベル 2 (8 bit)
	UCHAR FlowLabel3;					// フローラベル 3 (8 bit)
	USHORT PayloadLength;				// ペイロードの長さ (拡張ヘッダを含む)
	UCHAR NextHeader;					// 次のヘッダ
	UCHAR HopLimit;						// ホップリミット
	SE_IPV6_ADDR SrcAddress;			// ソースアドレス
	SE_IPV6_ADDR DestAddress;			// 宛先アドレス
} SE_STRUCT_PACKED;

// IPv6 ヘッダ操作用マクロ
#define SE_IPV6_GET_VERSION(h)			(((h)->VersionAndTrafficClass1 >> 4) & 0x0f)
#define SE_IPV6_SET_VERSION(h, v)		((h)->VersionAndTrafficClass1 = ((h)->VersionAndTrafficClass1 & 0x0f) | (((v) << 4) & 0xf0))
#define SE_IPV6_GET_TRAFFIC_CLASS(h)	((((h)->VersionAndTrafficClass1 << 4) & 0xf0) | (((h)->TrafficClass2AndFlowLabel1 >> 4) & 0x0f))
#define	SE_IPV6_SET_TRAFFIC_CLASS(h, v)	((h)->VersionAndTrafficClass1 = ((h)->VersionAndTrafficClass1 & 0xf0) | (((v) >> 4) & 0x0f),\
										(h)->TrafficClass2AndFlowLabel1 = ((h)->TrafficClass2AndFlowLabel1 & 0x0f) | (((v) << 4) & 0xf0))
#define	SE_IPV6_GET_FLOW_LABEL(h)		((((h)->TrafficClass2AndFlowLabel1 << 16) & 0xf0000) | (((h)->FlowLabel2 << 8) & 0xff00) |\
										(((h)->FlowLabel3) & 0xff))
#define SE_IPV6_SET_FLOW_LABEL(h, v)	((h)->TrafficClass2AndFlowLabel1 = ((h)->TrafficClass2AndFlowLabel1 & 0xf0) | (((v) >> 16) & 0x0f), \
										(h)->FlowLabel2 = ((v) >> 8) & 0xff,\
										(h)->FlowLabel3 = (v) & 0xff)

// IPv6 ホップ最大値 (ルーティングしない)
#define SE_IPV6_HOP_MAX					255

// IPv6 ホップ標準数
#define SE_IPV6_HOP_DEFAULT				127

// IPv6 ヘッダ番号
#define SE_IPV6_HEADER_HOP				0	// ホップバイホップオプションヘッダ
#define SE_IPV6_HEADER_ENDPOINT			60	// 終点オプションヘッダ
#define SE_IPV6_HEADER_ROUTING			43	// ルーティングヘッダ
#define SE_IPV6_HEADER_FRAGMENT			44	// フラグメントヘッダ
#define SE_IPV6_HEADER_NONE				59	// 次ヘッダ無し

// IPv6 オプションヘッダ
// (ホップオプションヘッダ、終点オプションヘッダ、ルーティングヘッダで使用される)
struct SE_IPV6_OPTION_HEADER
{
	UCHAR NextHeader;					// 次のヘッダ
	UCHAR Size;							// ヘッダサイズ (/8)
} SE_STRUCT_PACKED;

// IPv6 フラグメントヘッダ
// (フラグメント不可能部分は、ルーティングヘッダがある場合はその直前まで、
//  ホップバイホップオプションヘッダがある場合はその直前まで、
//  それ以外の場合は最初の拡張ヘッダもしくはペイロードの直前まで)
struct SE_IPV6_FRAGMENT_HEADER
{
	UCHAR NextHeader;					// 次のヘッダ
	UCHAR Reserved;						// 予約
	UCHAR FlagmentOffset1;				// フラグメントオフセット 1 (/8, 8 bit)
	UCHAR FlagmentOffset2AndFlags;		// フラグメントオフセット 2 (/8, 5 bit) + 予約 (2 bit) + More フラグ (1 bit)
	UINT Identification;				// ID
} SE_STRUCT_PACKED;

// IPv6 フラグメントヘッダ操作用マクロ
#define SE_IPV6_GET_FRAGMENT_OFFSET(h)		(((((h)->FlagmentOffset1) << 5) & 0x1fe0) | (((h)->FlagmentOffset2AndFlags >> 3) & 0x1f))
#define SE_IPV6_SET_FRAGMENT_OFFSET(h, v)	((h)->FlagmentOffset1 = (v / 32) & 0xff,	\
											((h)->FlagmentOffset2AndFlags = ((v % 256) << 3) & 0xf8) | ((h)->FlagmentOffset2AndFlags & 0x07))
#define SE_IPV6_GET_FLAGS(h)				((h)->FlagmentOffset2AndFlags & 0x0f)
#define SE_IPV6_SET_FLAGS(h, v)				((h)->FlagmentOffset2AndFlags = (((h)->FlagmentOffset2AndFlags & 0xf8) | (v & 0x07)))

// フラグ
#define SE_IPV6_FRAGMENT_HEADER_FLAG_MORE_FRAGMENTS		0x01	// 次のフラグメントがある

// IPv6 仮想ヘッダ
struct SE_IPV6_PSEUDO_HEADER
{
	SE_IPV6_ADDR SrcAddress;			// ソースアドレス
	SE_IPV6_ADDR DestAddress;			// 宛先アドレス
	UINT UpperLayerPacketSize;			// 上位レイヤのパケットサイズ
	UCHAR Padding[3];					// パディング
	UCHAR NextHeader;					// 次ヘッダ (TCP / UDP)
} SE_STRUCT_PACKED;

// ICMPv6
#define SE_ICMPV6_TYPE_ECHO_REQUEST				128		// ICMPv6 Echo 要求
#define SE_ICMPV6_TYPE_ECHO_RESPONSE			129		// ICMPv6 Echo 応答
#define SE_ICMPV6_TYPE_ROUTER_SOLICIATION		133		// ルータ要請
#define SE_ICMPV6_TYPE_ROUTER_ADVERTISEMENT		134		// ルータ広告
#define SE_ICMPV6_TYPE_NEIGHBOR_SOLICIATION		135		// 近隣要請
#define SE_ICMPV6_TYPE_NEIGHBOR_ADVERTISEMENT	136		// 近隣広告

// ICMPv6 ルータ要請ヘッダ
struct SE_ICMPV6_ROUTER_SOLICIATION_HEADER
{
	UINT Reserved;							// 予約
	// + オプション (ソースリンクレイヤアドレス[任意])
} SE_STRUCT_PACKED;

// ICMPv6 ルータ広告ヘッダ
struct SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER
{
	UCHAR CurHopLimit;						// デフォルトのホップリミット数
	UCHAR Flags;							// フラグ (0)
	USHORT Lifetime;						// 寿命
	UINT ReachableTime;						// 0
	UINT RetransTimer;						// 0
	// + オプション (プレフィックス情報[必須], MTU[任意])
} SE_STRUCT_PACKED;

// ICMPv6 近隣要請ヘッダ
struct SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER
{
	UINT Reserved;							// 予約
	SE_IPV6_ADDR TargetAddress;				// ターゲットアドレス
	// + オプション (ソースリンクレイヤアドレス[必須])
} SE_STRUCT_PACKED;

// ICMPv6 近隣広告ヘッダ
struct SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER
{
	UCHAR Flags;							// フラグ
	UCHAR Reserved[3];						// 予約
	SE_IPV6_ADDR TargetAddress;				// ターゲットアドレス
	// + オプション (ターゲットリンクレイヤアドレス)
} SE_STRUCT_PACKED;

#define SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_FLAG_ROUTER	0x80	// ルータ
#define SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_FLAG_SOLICITED	0x40	// 要請フラグ
#define SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_FLAG_OVERWRITE	0x20	// 上書きフラグ

// ICMPv6 オプションリスト
struct SE_ICMPV6_OPTION_LIST
{
	SE_ICMPV6_OPTION_LINK_LAYER *SourceLinkLayer;		// ソースリンクレイヤアドレス
	SE_ICMPV6_OPTION_LINK_LAYER *TargetLinkLayer;		// ターゲットリンクレイヤアドレス
	SE_ICMPV6_OPTION_PREFIX *Prefix;					// プレフィックス情報
	SE_ICMPV6_OPTION_MTU *Mtu;							// MTU
} SE_STRUCT_PACKED;

// ICMPv6 オプション
struct SE_ICMPV6_OPTION
{
	UCHAR Type;								// タイプ
	UCHAR Length;							// 長さ (/8, タイプと長さを含める)
} SE_STRUCT_PACKED;

#define	SE_ICMPV6_OPTION_TYPE_SOURCE_LINK_LAYER	1		// ソースリンクレイヤアドレス
#define SE_ICMPV6_OPTION_TYPE_TARGET_LINK_LAYER	2		// ターゲットリンクレイヤアドレス
#define SE_ICMPV6_OPTION_TYPE_PREFIX			3		// プレフィックス情報
#define SE_ICMPV6_OPTION_TYPE_MTU				5		// MTU

// ICMPv6 リンクレイヤオプション
struct SE_ICMPV6_OPTION_LINK_LAYER
{
	SE_ICMPV6_OPTION IcmpOptionHeader;		// オプションヘッダ
	UCHAR Address[6];						// MAC アドレス
} SE_STRUCT_PACKED;

// ICMPv6 プレフィックス情報オプション
struct SE_ICMPV6_OPTION_PREFIX
{
	SE_ICMPV6_OPTION IcmpOptionHeader;		// オプションヘッダ
	UCHAR SubnetLength;						// サブネット長
	UCHAR Flags;							// フラグ
	UINT ValidLifetime;						// 正式な寿命
	UINT PreferredLifetime;					// 望ましい寿命
	UINT Reserved;							// 予約
	SE_IPV6_ADDR Prefix;					// プレフィックスアドレス
} SE_STRUCT_PACKED;

#define SE_ICMPV6_OPTION_PREFIX_FLAG_ONLINK		0x80	// リンク上
#define SE_ICMPV6_OPTION_PREFIX_FLAG_AUTO		0x40	// 自動

// ICMPv6 MTU オプション
struct SE_ICMPV6_OPTION_MTU
{
	SE_ICMPV6_OPTION IcmpOptionHeader;		// オプションヘッダ
	USHORT Reserved;						// 予約
	UINT Mtu;								// MTU 値
} SE_STRUCT_PACKED;

// パケット
struct SE_PACKET
{
	UCHAR *PacketData;						// パケットデータ
	UINT PacketSize;						// パケットサイズ
	SE_MAC_HEADER *MacHeader;				// MAC ヘッダ
	bool IsBroadcast;						// ブロードキャストパケットかどうか
	UINT TypeL3;							// Layer-3 パケットの種類
	SE_IPV6_HEADER_PACKET_INFO IPv6HeaderPacketInfo;	// IPv6 ヘッダパケット情報
	union
	{
		SE_IPV4_HEADER *IPv4Header;			// IPv4 ヘッダ
		SE_ARPV4_HEADER *ARPv4Header;		// ARPv4 ヘッダ
		SE_IPV6_HEADER *IPv6Header;			// IPv6 ヘッダ
		void *PointerL3;					// ポインタ
	} L3;
	union
	{
		void *PointerL4;					// ポインタ
	} L4;
	UINT TypeL4;							// Layer-4 パケットの種類
} SE_STRUCT_PACKED;

// Layer-3 パケット分類
#define	SE_L3_UNKNOWN			0		// 不明
#define	SE_L3_ARPV4				1		// ARPv4 パケット
#define	SE_L3_IPV4				2		// IPv4 パケット
#define SE_L3_IPV6				3		// IPv6 パケット

// Layer-4 パケット分類
#define	SE_L4_UNKNOWN			0		// 不明
#define	SE_L4_UDPV4				1		// UDPv4 パケット
#define	SE_L4_ICMPV4			2		// ICMPv4 パケット
#define SE_L4_ESPV4				3		// ESPv4 パケット
#define SE_L4_UDPV6				4		// UDPv6 パケット
#define SE_L4_ICMPV6			5		// ICMPv6 パケット
#define SE_L4_ESPV6				6		// ESPv6 パケット


// IP パケットの結合用のキューで許容される合計サイズ クォータ
#define SE_IP_COMBINE_WAIT_QUEUE_SIZE_QUOTA	(50 * 1024 * 1024)

// データ最大サイズ定数
#define	SE_MAX_L3_DATA_SIZE				(1500)
#define	SE_MAX_IPV4_DATA_SIZE			(SE_MAX_L3_DATA_SIZE - SE_IPV4_HEADER_SIZE)
#define	SE_MAX_UDPV4_DATA_SIZE			(SE_MAX_IPV4_DATA_SIZE - SE_UDP_HEADER_SIZE)
#define	SE_MAX_IPV4_DATA_SIZE_TOTAL		(65535)

// IPv4 MTU 最大 / 最小値
#define SE_V4_MTU_MAX					1500			// 最大値
#define SE_V4_MTU_DEFAULT				1500			// デフォルト値
#define SE_V4_MTU_IN_VPN_DEFAULT		1400			// VPN 内での MTU 値
#define SE_V4_MTU_MIN					1280			// 最小値

// IPv6 MTU 最大 / 最小値
#define SE_V6_MTU_MAX					1500			// 最大値
#define SE_V6_MTU_DEFAULT				1500			// デフォルト値
#define SE_V6_MTU_IN_VPN_DEFAULT		1400			// VPN 内での MTU 値
#define SE_V6_MTU_MIN					1280			// 最小値

// IPv4 パケットオプション定数
#define	SE_DEFAULT_IPV4_TOS				0				// IP ヘッダの TOS
#define	SE_DEFAULT_IPV4_TTL				128				// IP ヘッダの TTL

// DHCPv4 クライアント動作
#define	SE_DHCPV4_DISCOVER		1
#define	SE_DHCPV4_REQUEST		3

// DHCPv4 サーバー動作
#define	SE_DHCPV4_OFFER			2
#define	SE_DHCPV4_ACK			5
#define	SE_DHCPV4_NACK			6

// DHCPv4 関係の定数
#define	SE_DHCPV4_ID_MESSAGE_TYPE		0x35
#define	SE_DHCPV4_ID_REQUEST_IP_ADDRESS	0x32
#define	SE_DHCPV4_ID_HOST_NAME			0x0c
#define	SE_DHCPV4_ID_SERVER_ADDRESS		0x36
#define	SE_DHCPV4_ID_LEASE_TIME			0x33
#define	SE_DHCPV4_ID_DOMAIN_NAME		0x0f
#define	SE_DHCPV4_ID_SUBNET_MASK		0x01
#define	SE_DHCPV4_ID_GATEWAY_ADDR		0x03
#define	SE_DHCPV4_ID_DNS_ADDR			0x06
#define SE_DHCPV4_ID_MTU				0x1a

#ifdef	SE_WIN32
#pragma pack(pop)
#endif	// SE_WIN32

// 関数プロトタイプ
SE_PACKET *SeParsePacket(void *data, UINT size);
void SeFreePacket(SE_PACKET *p);
bool SeParsePacketL2(SE_PACKET *p, UCHAR *buf, UINT size);
bool SeParsePacketL3ARPv4(SE_PACKET *p, UCHAR *buf, UINT size);
bool SeParsePacketL3IPv4(SE_PACKET *p, UCHAR *buf, UINT size);
bool SeParsePacketL3IPv6(SE_PACKET *p, UCHAR *buf, UINT size);

bool SeParseIPv6Packet(SE_IPV6_HEADER_PACKET_INFO *info, UCHAR *buf, UINT size);
bool SeParseIPv6ExtHeader(SE_IPV6_HEADER_PACKET_INFO *info, UCHAR next_header, UCHAR *buf, UINT size);
UCHAR SeGetNextHeaderFromQueue(SE_QUEUE *q);
void SeBuildAndAddIPv6PacketOptionHeader(SE_BUF *b, SE_IPV6_OPTION_HEADER *opt, UCHAR next_header, UINT size);
SE_BUF *SeBuildIPv6Packet(SE_IPV6_HEADER_PACKET_INFO *info, UINT *bytes_before_payload);
SE_LIST *SeBuildIPv6PacketWithFragment(SE_IPV6_HEADER_PACKET_INFO *info, UINT id, UINT mtu);
void SeFreePacketList(SE_LIST *o);
void SeFreePacketListWithoutBuffer(SE_LIST *o);
bool SeParseICMPv6Options(SE_ICMPV6_OPTION_LIST *o, UCHAR *buf, UINT size);
SE_BUF *SeBuildICMPv6Options(SE_ICMPV6_OPTION_LIST *o);
void SeBuildICMPv6OptionValue(SE_BUF *b, UCHAR type, void *header_pointer, UINT total_size);


#endif	// SEPACKET_H


