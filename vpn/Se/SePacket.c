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

// SePacket.c
// 概要: パケット解析モジュール

#define SE_INTERNAL
#include <Se/Se.h>

// ICMPv6 パケットのオプション値のビルド
void SeBuildICMPv6OptionValue(SE_BUF *b, UCHAR type, void *header_pointer, UINT total_size)
{
	UINT packet_size;
	UCHAR *packet;
	SE_ICMPV6_OPTION *opt;
	// 引数チェック
	if (b == NULL || header_pointer == NULL)
	{
		return;
	}

	packet_size = ((total_size + 7) / 8) * 8;
	packet = SeZeroMalloc(packet_size);

	SeCopy(packet, header_pointer, total_size);
	opt = (SE_ICMPV6_OPTION *)packet;
	opt->Length = (UCHAR)(packet_size / 8);
	opt->Type = type;

	SeWriteBuf(b, packet, packet_size);

	SeFree(packet);
}

// ICMPv6 パケットのオプションのビルド
SE_BUF *SeBuildICMPv6Options(SE_ICMPV6_OPTION_LIST *o)
{
	SE_BUF *b;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();

	if (o->SourceLinkLayer != NULL)
	{
		SeBuildICMPv6OptionValue(b, SE_ICMPV6_OPTION_TYPE_SOURCE_LINK_LAYER, o->SourceLinkLayer, sizeof(SE_ICMPV6_OPTION_LINK_LAYER));
	}
	if (o->TargetLinkLayer != NULL)
	{
		SeBuildICMPv6OptionValue(b, SE_ICMPV6_OPTION_TYPE_TARGET_LINK_LAYER, o->TargetLinkLayer, sizeof(SE_ICMPV6_OPTION_LINK_LAYER));
	}
	if (o->Prefix != NULL)
	{
		SeBuildICMPv6OptionValue(b, SE_ICMPV6_OPTION_TYPE_PREFIX, o->Prefix, sizeof(SE_ICMPV6_OPTION_PREFIX));
	}
	if (o->Mtu != NULL)
	{
		SeBuildICMPv6OptionValue(b, SE_ICMPV6_OPTION_TYPE_MTU, o->Mtu, sizeof(SE_ICMPV6_OPTION_MTU));
	}

	SeSeekBuf(b, 0, 0);

	return b;
}

// ICMPv6 パケットのオプションの解析
bool SeParseICMPv6Options(SE_ICMPV6_OPTION_LIST *o, UCHAR *buf, UINT size)
{
	// 引数チェック
	if (o == NULL || buf == NULL)
	{
		return false;
	}

	SeZero(o, sizeof(SE_ICMPV6_OPTION_LIST));

	// ヘッダ部分の読み込み
	while (true)
	{
		SE_ICMPV6_OPTION *option_header;
		UINT header_total_size;
		UCHAR *header_pointer;
		if (size < sizeof(SE_ICMPV6_OPTION))
		{
			// サイズ不足
			return true;
		}

		option_header = (SE_ICMPV6_OPTION *)buf;
		// ヘッダ全体サイズの計算
		header_total_size = option_header->Length * 8;
		if (size < header_total_size)
		{
			// サイズ不足
			return true;
		}

		header_pointer = buf;
		buf += header_total_size;
		size -= header_total_size;

		switch (option_header->Type)
		{
		case SE_ICMPV6_OPTION_TYPE_SOURCE_LINK_LAYER:
		case SE_ICMPV6_OPTION_TYPE_TARGET_LINK_LAYER:
			// ソース or ターゲットリンクレイヤオプション
			if (header_total_size >= sizeof(SE_ICMPV6_OPTION_LINK_LAYER))
			{
				if (option_header->Type == SE_ICMPV6_OPTION_TYPE_SOURCE_LINK_LAYER)
				{
					o->SourceLinkLayer = (SE_ICMPV6_OPTION_LINK_LAYER *)header_pointer;
				}
				else
				{
					o->TargetLinkLayer = (SE_ICMPV6_OPTION_LINK_LAYER *)header_pointer;
				}
			}
			else
			{
				// ICMPv6 パケット破損?
				return false;
			}
			break;

		case SE_ICMPV6_OPTION_TYPE_PREFIX:
			// プレフィックス情報
			if (header_total_size >= sizeof(SE_ICMPV6_OPTION_PREFIX))
			{
				o->Prefix = (SE_ICMPV6_OPTION_PREFIX *)header_pointer;
			}
			else
			{
				// ICMPv6 パケット破損?
			}
			break;

		case SE_ICMPV6_OPTION_TYPE_MTU:
			// MTU
			if (header_total_size >= sizeof(SE_ICMPV6_OPTION_MTU))
			{
				o->Mtu = (SE_ICMPV6_OPTION_MTU *)header_pointer;
			}
			else
			{
				// ICMPv6 パケット破損?
			}
			break;
		}
	}
}

// IPv6 パケットをフラグメント化を自動的に有効にしてビルドする
SE_LIST *SeBuildIPv6PacketWithFragment(SE_IPV6_HEADER_PACKET_INFO *info, UINT id, UINT mtu)
{
	SE_BUF *b;
	UINT size_for_headers;
	UINT one_fragment_payload_size;
	UINT i;
	SE_LIST *o;
	// 引数チェック
	if (info == NULL)
	{
		return NULL;
	}

	mtu = MAX(SE_V4_MTU_MIN, mtu);

	if (info->FragmentHeader != NULL)
	{
		info->FragmentHeader->Identification = SeEndian32(id);
	}

	// まず単純にビルドする
	b = SeBuildIPv6Packet(info, &size_for_headers);
	if (b == NULL)
	{
		return NULL;
	}

	// サイズが MTU 以下の場合はそのまま送信
	if (b->Size <= mtu)
	{
		SE_LIST *o = SeNewList(NULL);

		SeAdd(o, b);

		return o;
	}

	SeFreeBuf(b);

	// mtu - ヘッダサイズを計算して 8 の倍数にする
	if (mtu <= size_for_headers)
	{
		// ヘッダ部が長すぎる
		return NULL;
	}

	one_fragment_payload_size = mtu - size_for_headers;
	one_fragment_payload_size = (one_fragment_payload_size / 8) * 8;
	if (one_fragment_payload_size == 0)
	{
		// フラグメント部がない
		return NULL;
	}

	o = SeNewList(NULL);

	// フラグメンテーション処理
	for (i = 0;i < info->PayloadSize;i += one_fragment_payload_size)
	{
		SE_IPV6_HEADER_PACKET_INFO info2;
		SE_IPV6_FRAGMENT_HEADER frag;
		SE_BUF *b;

		SeCopy(&info2, info, sizeof(SE_IPV6_HEADER_PACKET_INFO));

		if (info2.FragmentHeader == NULL)
		{
			info2.FragmentHeader = &frag;
		}

		SeZero(info2.FragmentHeader, sizeof(SE_IPV6_FRAGMENT_HEADER));

		info2.Payload = ((UCHAR *)info->Payload) + i;
		info2.PayloadSize = MIN(info->PayloadSize - i, one_fragment_payload_size);

		SE_IPV6_SET_FRAGMENT_OFFSET(info2.FragmentHeader, ((USHORT)(i / 8)));
		SE_IPV6_SET_FLAGS(info2.FragmentHeader,
			((info->PayloadSize - i) > one_fragment_payload_size) ? SE_IPV6_FRAGMENT_HEADER_FLAG_MORE_FRAGMENTS : 0);
		info2.FragmentHeader->Identification = SeEndian32(id);

		b = SeBuildIPv6Packet(&info2, NULL);

		SeAdd(o, b);
	}

	return o;
}

// パケットの配列を解放する
void SeFreePacketList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_BUF *b = SE_LIST_DATA(o, i);

		SeFreeBuf(b);
	}

	SeFreeList(o);
}

// パケットの配列を解放する (バッファ部分は解放しない)
void SeFreePacketListWithoutBuffer(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_BUF *b = SE_LIST_DATA(o, i);

		SeFreeBufWithoutBuffer(b);
	}

	SeFreeList(o);
}

// キューから次のヘッダ番号を取得
UCHAR SeGetNextHeaderFromQueue(SE_QUEUE *q)
{
	UINT *p;
	UCHAR v;
	// 引数チェック
	if (q == NULL)
	{
		return SE_IPV6_HEADER_NONE;
	}

	p = (UINT *)SeGetNext(q);
	v = (UCHAR)(*p);
	SeFree(p);

	return v;
}

// IPv6 拡張オプションヘッダ (可変長) の追加
void SeBuildAndAddIPv6PacketOptionHeader(SE_BUF *b, SE_IPV6_OPTION_HEADER *opt, UCHAR next_header, UINT size)
{
	SE_IPV6_OPTION_HEADER *h;
	UINT total_size;
	// 引数チェック
	if (b == NULL || opt == NULL)
	{
		return;
	}

	total_size = size;
	if ((total_size % 8) != 0)
	{
		total_size = ((total_size / 8) + 1) * 8;
	}

	h = SeZeroMalloc(total_size);
	SeCopy(h, opt, size);
	h->Size = (total_size / 8) - 1;
	h->NextHeader = next_header;

	SeWriteBuf(b, h, total_size);

	SeFree(h);
}

// IPv6 パケットのビルド
SE_BUF *SeBuildIPv6Packet(SE_IPV6_HEADER_PACKET_INFO *info, UINT *bytes_before_payload)
{
	SE_BUF *b;
	SE_QUEUE *q;
	UINT bbp = 0;
	// 引数チェック
	if (info == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();
	q = SeNewQueue();

	// オプションヘッダの一覧を作成
	if (info->HopHeader != NULL)
	{
		SeInsertQueueInt(q, SE_IPV6_HEADER_HOP);
	}
	if (info->EndPointHeader != NULL)
	{
		SeInsertQueueInt(q, SE_IPV6_HEADER_ENDPOINT);
	}
	if (info->RoutingHeader != NULL)
	{
		SeInsertQueueInt(q, SE_IPV6_HEADER_ROUTING);
	}
	if (info->FragmentHeader != NULL)
	{
		SeInsertQueueInt(q, SE_IPV6_HEADER_FRAGMENT);
	}
	SeInsertQueueInt(q, info->Protocol);

	// IPv6 ヘッダ
	info->IPv6Header->NextHeader = SeGetNextHeaderFromQueue(q);
	SeWriteBuf(b, info->IPv6Header, sizeof(SE_IPV6_HEADER));

	// ホップバイホップオプションヘッダ
	if (info->HopHeader != NULL)
	{
		SeBuildAndAddIPv6PacketOptionHeader(b, info->HopHeader,
			SeGetNextHeaderFromQueue(q), info->HopHeaderSize);
	}

	// 終点オプションヘッダ
	if (info->EndPointHeader != NULL)
	{
		SeBuildAndAddIPv6PacketOptionHeader(b, info->EndPointHeader,
			SeGetNextHeaderFromQueue(q), info->EndPointHeaderSize);
	}

	// ルーティングヘッダ
	if (info->RoutingHeader != NULL)
	{
		SeBuildAndAddIPv6PacketOptionHeader(b, info->RoutingHeader,
			SeGetNextHeaderFromQueue(q), info->RoutingHeaderSize);
	}

	// フラグメントヘッダ
	if (info->FragmentHeader != NULL)
	{
		info->FragmentHeader->NextHeader = SeGetNextHeaderFromQueue(q);
		SeWriteBuf(b, info->FragmentHeader, sizeof(SE_IPV6_FRAGMENT_HEADER));
	}

	bbp = b->Size;
	if (info->FragmentHeader == NULL)
	{
		bbp += sizeof(SE_IPV6_FRAGMENT_HEADER);
	}

	// ペイロード
	if (info->Protocol != SE_IPV6_HEADER_NONE)
	{
		SeWriteBuf(b, info->Payload, info->PayloadSize);
	}

	SeFreeQueue(q);

	SeSeekBuf(b, 0, 0);

	// ペイロード長さ
	((SE_IPV6_HEADER *)b->Buf)->PayloadLength = SeEndian16(b->Size - (USHORT)sizeof(SE_IPV6_HEADER));

	if (bytes_before_payload != NULL)
	{
		// ペイロードの直前までの長さ (ただしフラグメントヘッダは必ず含まれると仮定)
		// を計算する
		*bytes_before_payload = bbp;
	}

	return b;
}

// IPv6 拡張ヘッダ解析
bool SeParseIPv6ExtHeader(SE_IPV6_HEADER_PACKET_INFO *info, UCHAR next_header, UCHAR *buf, UINT size)
{
	bool ret = false;
	SE_IPV6_OPTION_HEADER *option_header;
	UINT option_header_size;
	UCHAR next_header_2 = SE_IPV6_HEADER_NONE;
	// 引数チェック
	if (info == NULL || buf == NULL)
	{
		return false;
	}

	while (true)
	{
		if (size > 8)
		{
			next_header_2 = *((UCHAR *)buf);
		}

		switch (next_header)
		{
		case SE_IPV6_HEADER_HOP:
		case SE_IPV6_HEADER_ENDPOINT:
		case SE_IPV6_HEADER_ROUTING:
			// 可変長ヘッダ
			if (size < 8)
			{
				return false;
			}

			option_header = (SE_IPV6_OPTION_HEADER *)buf;
			option_header_size = (option_header->Size + 1) * 8;
			if (size < option_header_size)
			{
				return false;
			}

			switch (next_header)
			{
			case SE_IPV6_HEADER_HOP:
				info->HopHeader = (SE_IPV6_OPTION_HEADER *)buf;
				info->HopHeaderSize = option_header_size;
				break;

			case SE_IPV6_HEADER_ENDPOINT:
				info->EndPointHeader = (SE_IPV6_OPTION_HEADER *)buf;
				info->EndPointHeaderSize = option_header_size;
				break;

			case SE_IPV6_HEADER_ROUTING:
				info->RoutingHeader = (SE_IPV6_OPTION_HEADER *)buf;
				info->RoutingHeaderSize = option_header_size;
				break;
			}

			buf += option_header_size;
			size -= option_header_size;
			break;

		case SE_IPV6_HEADER_FRAGMENT:
			// フラグメントヘッダ (固定長)
			if (size < sizeof(SE_IPV6_FRAGMENT_HEADER))
			{
				return false;
			}

			info->FragmentHeader = (SE_IPV6_FRAGMENT_HEADER *)buf;

			buf += sizeof(SE_IPV6_FRAGMENT_HEADER);
			size -= sizeof(SE_IPV6_FRAGMENT_HEADER);
			break;

		default:
			// 後にペイロードが続くとみなす
			if (next_header != SE_IPV6_HEADER_NONE)
			{
				info->Payload = buf;
				info->PayloadSize = size;
			}
			else
			{
				info->Payload = NULL;
				info->PayloadSize = 0;
			}
			info->Protocol = next_header;
			return true;
		}

		next_header = next_header_2;
	}
}

// IPv6 ヘッダ解析
bool SeParseIPv6Packet(SE_IPV6_HEADER_PACKET_INFO *info, UCHAR *buf, UINT size)
{
	// 引数チェック
	if (info == NULL || buf == NULL)
	{
		return false;
	}

	SeZero(info, sizeof(SE_IPV6_HEADER_PACKET_INFO));

	// IPv6 ヘッダ
	if (size < sizeof(SE_IPV6_HEADER))
	{
		// サイズ不正
		return false;
	}

	info->IPv6Header = (SE_IPV6_HEADER *)buf;
	buf += sizeof(SE_IPV6_HEADER);
	size -= sizeof(SE_IPV6_HEADER);

	if (SE_IPV6_GET_VERSION(info->IPv6Header) != 6)
	{
		// バージョン不正
		return false;
	}

	// 拡張ヘッダの解析
	return SeParseIPv6ExtHeader(info, info->IPv6Header->NextHeader, buf, size);
}

// L3 IPv6 パース
bool SeParsePacketL3IPv6(SE_PACKET *p, UCHAR *buf, UINT size)
{
	// 引数チェック
	if (p == NULL || buf == NULL)
	{
		return false;
	}

	if (SeParseIPv6Packet(&p->IPv6HeaderPacketInfo, buf, size) == false)
	{
		return false;
	}

	p->TypeL3 = SE_L3_IPV6;

	return true;
}

// L3 IPv4 パース
bool SeParsePacketL3IPv4(SE_PACKET *p, UCHAR *buf, UINT size)
{
	UINT header_size;
	// 引数チェック
	if (p == NULL || buf == NULL)
	{
		return false;
	}

	// サイズチェック
	if (size < sizeof(SE_IPV4_HEADER))
	{
		return false;
	}

	// バージョンチェック
	if (SE_IPV4_GET_VERSION(p->L3.IPv4Header) != 4)
	{
		// バージョン不正
		return false;
	}

	// ヘッダチェック
	header_size = SE_IPV4_GET_HEADER_LEN(p->L3.IPv4Header) * 4;
	if (header_size < sizeof(SE_IPV4_HEADER) || size < header_size)
	{
		// ヘッダサイズ不正
		return false;
	}

	p->TypeL3 = SE_L3_IPV4;

	buf += header_size;
	size -= header_size;

	p->L4.PointerL4 = buf;

	switch (p->L3.IPv4Header->Protocol)
	{
	case SE_IP_PROTO_ICMPV4: // ICMPv4
		p->TypeL4 = SE_L4_ICMPV4;
		break;

	case SE_IP_PROTO_UDP:	// UDPv4
		p->TypeL4 = SE_L4_UDPV4;
		break;

	case SE_IP_PROTO_ESP:	// ESPv4
		p->TypeL4 = SE_L4_ESPV4;
		break;

	default:				// 不明
		p->TypeL4 = SE_L4_UNKNOWN;
		break;
	}

	return true;
}

// L3 ARPv4 パース
bool SeParsePacketL3ARPv4(SE_PACKET *p, UCHAR *buf, UINT size)
{
	// 引数チェック
	if (p == NULL || buf == NULL)
	{
		return false;
	}

	// サイズチェック
	if (size < sizeof(SE_ARPV4_HEADER))
	{
		return false;
	}

	p->TypeL3 = SE_L3_ARPV4;

	return true;
}

// L2 パース
bool SeParsePacketL2(SE_PACKET *p, UCHAR *buf, UINT size)
{
	// 引数チェック
	if (p == NULL || buf == NULL)
	{
		return false;
	}

	// ヘッダサイズをチェック
	if (size < sizeof(SE_MAC_HEADER))
	{
		return false;
	}

	// MAC ヘッダ
	p->MacHeader = (SE_MAC_HEADER *)buf;

	buf += sizeof(SE_MAC_HEADER);
	size -= sizeof(SE_MAC_HEADER);

	p->L3.PointerL3 = buf;

	// L3 パース
	switch (SeEndian16(p->MacHeader->Protocol))
	{
	case SE_MAC_PROTO_ARPV4:	// ARPv4
		return SeParsePacketL3ARPv4(p, buf, size);

	case SE_MAC_PROTO_IPV4:		// IPv4
		return SeParsePacketL3IPv4(p, buf, size);

	case SE_MAC_PROTO_IPV6:		// IPv6
		return SeParsePacketL3IPv6(p, buf, size);

	default:					// 不明
		return true;
	}
}

// パケットのパース
SE_PACKET *SeParsePacket(void *data, UINT size)
{
	SE_PACKET *p;
	UINT packet_type;
	// 引数チェック
	if (data == NULL || size == 0)
	{
		return NULL;
	}

	// パケットの検査
	packet_type = SeEthParseEthernetPacket(data, size, NULL);

	if ((packet_type & SE_ETHER_PACKET_TYPE_VALID) == 0)
	{
		return NULL;
	}

	p = SeZeroMalloc(sizeof(SE_PACKET));
	p->PacketData = data;
	p->PacketSize = size;
	p->IsBroadcast = (packet_type & SE_ETHER_PACKET_TYPE_BROADCAST) ? true : false;

	if (SeParsePacketL2(p, p->PacketData, p->PacketSize) == false)
	{
		SeFreePacket(p);
		return NULL;
	}

	return p;
}

// パケットの解放
void SeFreePacket(SE_PACKET *p)
{
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	SeFree(p);
}
