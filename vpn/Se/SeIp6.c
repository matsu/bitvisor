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

// SeIp6.c
// 概要: IPv6 プロトコルスタック

#define SE_INTERNAL
#include <Se/Se.h>

// メインプロシージャ
void Se6MainProc(SE_IPV6 *p)
{
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	// NDP 待機リストの実行
	Se6ProcessNdpWaitList(p);

	// 古くなった IP 結合リストの削除
	Se6FlushIpCombineList(p, p->IpCombineList);

	// 古くなった IP 待機リストの削除
	Se6FlushIpWaitList(p);

	if (p->SendRa)
	{
		if (SeVpnDoInterval(p->Vpn, &p->VarRaSendInterval, SE_IPV6_RA_SEND_INTERVAL * 1000))
		{
			// 定期的な RA の送信
			Se6SendIcmpRouterAdvertisement(p);
		}
	}
}

// NDP 待機リストの実行
void Se6ProcessNdpWaitList(SE_IPV6 *p)
{
	UINT i;
	SE_LIST *o;
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	o = NULL;

	for (i = 0;i < SE_LIST_NUM(p->NdpWaitList);i++)
	{
		SE_NDPV6_WAIT *w = SE_LIST_DATA(p->NdpWaitList, i);

		if (Se6SendNeighborSoliciationDoProcess(p, w))
		{
			if (o == NULL)
			{
				o = SeNewList(NULL);
			}

			SeAdd(o, w);
		}
	}

	if (o != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(o);i++)
		{
			SE_NDPV6_WAIT *w = SE_LIST_DATA(o, i);

			SeDelete(p->NdpWaitList, w);
			SeFree(w);
		}

		SeFreeList(o);
	}
}

// 近隣要請パケットの送信の処理の実行
bool Se6SendNeighborSoliciationDoProcess(SE_IPV6 *p, SE_NDPV6_WAIT *w)
{
	// 引数チェック
	if (p == NULL || w == NULL)
	{
		return false;
	}

	if (SeVpnDoInterval(p->Vpn, &w->IntervalValue, SE_IPV6_NDP_SEND_INTERVAL * 1000))
	{
		if ((w->SendCounter++) >= SE_IPV6_NDP_SEND_COUNT)
		{
			return true;
		}
		else
		{
			UINT type = Se6GetIPAddrType(w->IpAddress);
			SE_IPV6_ADDR src_ip;

			if (type & SE_IPV6_ADDR_LOCAL_UNICAST)
			{
				src_ip = p->LocalIpAddress;
			}
			else
			{
				src_ip = p->GlobalIpAddress;
			}

			Se6SendIcmpNeighborSoliciation(p, src_ip, w->IpAddress);
		}
	}

	return false;
}

// 近隣要請パケットの送信
void Se6SendNeighborSoliciation(SE_IPV6 *p, SE_IPV6_ADDR target_ip)
{
	SE_NDPV6_WAIT *w;
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	w = Se6SearchNdpWaitList(p->NdpWaitList, target_ip);
	if (w != NULL)
	{
		w->SendCounter = 0;
	}
	else
	{
		w = SeZeroMalloc(sizeof(SE_NDPV6_WAIT));

		w->IpAddress = target_ip;
		w->SendCounter = 0;

		SeInsert(p->NdpWaitList, w);
	}

	Se6SendNeighborSoliciationDoProcess(p, w);
}

// ICMP ルータ広告パケットの送信
void Se6SendIcmpRouterAdvertisement(SE_IPV6 *p)
{
	SE_ICMPV6_OPTION_LIST opt;
	SE_ICMPV6_OPTION_LINK_LAYER link;
	SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER header;
	SE_ICMPV6_OPTION_PREFIX prefix;
	SE_ICMPV6_OPTION_MTU mtu;
	SE_BUF *b, *b2;
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	SeZero(&link, sizeof(link));
	SeZero(&opt, sizeof(opt));
	SeZero(&header, sizeof(header));
	SeZero(&prefix, sizeof(prefix));
	SeZero(&mtu, sizeof(mtu));

	// ソースリンクレイヤアドレス
	SeCopy(link.Address, p->Eth->MyMacAddress, 6);

	// ヘッダ
	header.CurHopLimit = SE_IPV6_SEND_HOP_LIMIT;
	header.Lifetime = SeEndian16(p->Vpn->Config->RaLifetimeV6);

	// プレフィックス
	prefix.SubnetLength = (UCHAR)Se6SubnetMaskToInt(p->SubnetMask);
	prefix.Flags = SE_ICMPV6_OPTION_PREFIX_FLAG_ONLINK | SE_ICMPV6_OPTION_PREFIX_FLAG_AUTO;
	prefix.ValidLifetime = prefix.PreferredLifetime = SeEndian32(p->Vpn->Config->RaLifetimeV6);
	prefix.Prefix = Se6GetPrefixAddress(p->GlobalIpAddress, p->SubnetMask);

	// MTU
	mtu.Mtu = SeEndian32(p->Mtu);

	// オプションリスト
	opt.Mtu = &mtu;
	opt.Prefix = &prefix;
	opt.SourceLinkLayer = &link;

	b = SeBuildICMPv6Options(&opt);
	b2 = SeNewBuf();

	SeWriteBuf(b2, &header, sizeof(header));
	SeWriteBufBuf(b2, b);

	Se6SendIcmp(p, p->LocalIpAddress, Se6GetAllNodeMulticastAddr(), SE_IPV6_HOP_MAX,
		SE_ICMPV6_TYPE_ROUTER_ADVERTISEMENT, 0, b2->Buf, b2->Size, NULL);

	SeFreeBuf(b2);
	SeFreeBuf(b);
}

// ICMP 近隣広告パケットの送信
void Se6SendIcmpNeighborAdvertisement(SE_IPV6 *p, bool router, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR *mac, UCHAR *dest_mac)
{
	SE_ICMPV6_OPTION_LIST opt;
	SE_ICMPV6_OPTION_LINK_LAYER link;
	SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER header;
	SE_BUF *b;
	SE_BUF *b2;
	// 引数チェック
	if (p == NULL || mac == NULL)
	{
		return;
	}

	SeZero(&link, sizeof(link));
	SeCopy(link.Address, mac, 6);

	SeZero(&opt, sizeof(opt));
	opt.TargetLinkLayer = &link;

	SeZero(&header, sizeof(header));
	header.Flags = (router ? SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_FLAG_ROUTER : 0) |
		SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_FLAG_SOLICITED | SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_FLAG_OVERWRITE;
	header.TargetAddress = src_ip;

	b = SeBuildICMPv6Options(&opt);

	b2 = SeNewBuf();

	SeWriteBuf(b2, &header, sizeof(header));
	SeWriteBufBuf(b2, b);

	Se6SendIcmp(p, src_ip, dest_ip, SE_IPV6_HOP_MAX,
		SE_ICMPV6_TYPE_NEIGHBOR_ADVERTISEMENT, 0, b2->Buf, b2->Size, dest_mac);

	SeFreeBuf(b2);
	SeFreeBuf(b);
}

// ICMP 近隣要請パケットの送信
void Se6SendIcmpNeighborSoliciation(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR target_ip)
{
	SE_ICMPV6_OPTION_LIST opt;
	SE_ICMPV6_OPTION_LINK_LAYER link;
	SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER header;
	SE_BUF *b;
	SE_BUF *b2;
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	SeZero(&link, sizeof(link));
	SeCopy(link.Address, p->Eth->MyMacAddress, 6);

	SeZero(&opt, sizeof(opt));
	opt.SourceLinkLayer = &link;

	b = SeBuildICMPv6Options(&opt);

	SeZero(&header, sizeof(header));
	header.TargetAddress = target_ip;

	b2 = SeNewBuf();

	SeWriteBuf(b2, &header, sizeof(header));
	SeWriteBufBuf(b2, b);

	Se6SendIcmp(p, src_ip, Se6GetSoliciationMulticastAddr(target_ip), SE_IPV6_HOP_MAX,
		SE_ICMPV6_TYPE_NEIGHBOR_SOLICIATION, 0, b2->Buf, b2->Size, NULL);

	SeFreeBuf(b2);
	SeFreeBuf(b);
}

// ICMP Echo パケットのビルド
SE_BUF *Se6BuildIcmpEchoPacket(SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR type, UCHAR code, USHORT id, USHORT seq_no, void *data, UINT size)
{
	SE_ICMP_HEADER *icmp;
	SE_ICMP_ECHO *e;
	SE_BUF *ret;
	// 引数チェック
	if (data == NULL)
	{
		return NULL;
	}

	// ICMP ヘッダ
	icmp = SeZeroMalloc(sizeof(SE_ICMP_HEADER) + sizeof(SE_ICMP_ECHO) + size);

	icmp->Type = type;
	icmp->Code = code;

	// Echo データ
	e = (SE_ICMP_ECHO *)(((UCHAR *)icmp) + sizeof(SE_ICMP_HEADER));
	e->Identifier = SeEndian16(id);
	e->SeqNo = SeEndian16(seq_no);

	SeCopy(((UCHAR *)e) + sizeof(SE_ICMP_ECHO), data, size);

	icmp->Checksum = Se6CalcChecksum(src_ip, dest_ip, SE_IP_PROTO_ICMPV6, icmp,
		sizeof(SE_ICMP_HEADER) + sizeof(SE_ICMP_ECHO) + size);

	ret = SeMemToBuf(icmp, sizeof(SE_ICMP_HEADER) + sizeof(SE_ICMP_ECHO) + size);

	SeFree(icmp);

	return ret;
}

// ICMP Echo Request パケットの送信
void Se6SendIcmpEchoRequest(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, USHORT id,
							USHORT seq_no, void *data, UINT size)
{
	SE_BUF *buf;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}

	buf = Se6BuildIcmpEchoPacket(src_ip, dest_ip, SE_ICMPV6_TYPE_ECHO_REQUEST, 0,
		id, seq_no, data, size);

	Se6SendIp(p, dest_ip, src_ip, SE_IP_PROTO_ICMPV6, 0,
		buf->Buf, buf->Size, NULL);

	SeFreeBuf(buf);
}

// ICMP Echo Response パケットの送信
void Se6SendIcmpEchoResponse(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, USHORT id,
							USHORT seq_no, void *data, UINT size)
{
	SE_BUF *buf;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}

	buf = Se6BuildIcmpEchoPacket(src_ip, dest_ip, SE_ICMPV6_TYPE_ECHO_RESPONSE, 0,
		id, seq_no, data, size);

	Se6SendIp(p, dest_ip, src_ip, SE_IP_PROTO_ICMPV6, 0,
		buf->Buf, buf->Size, NULL);

	SeFreeBuf(buf);
}

// ICMP パケットの送信
void Se6SendIcmp(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR hop_limit, UCHAR type, UCHAR code, void *data, UINT size, UCHAR *dest_mac)
{
	SE_ICMP_HEADER *icmp;
	void *data_buf;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}

	// ヘッダの組み立て
	icmp = SeZeroMalloc(sizeof(SE_ICMP_HEADER) + size);
	data_buf = ((UCHAR *)icmp) + sizeof(SE_ICMP_HEADER);
	SeCopy(data_buf, data, size);

	icmp->Type = type;
	icmp->Code = code;
	icmp->Checksum = Se6CalcChecksum(src_ip, dest_ip, SE_IP_PROTO_ICMPV6, icmp,
		sizeof(SE_ICMP_HEADER) + size);

	Se6SendIp(p, dest_ip, src_ip, SE_IP_PROTO_ICMPV6, hop_limit, icmp, sizeof(SE_ICMP_HEADER) + size, dest_mac);

	SeFree(icmp);
}

// UDP パケットの送信
void Se6SendUdp(SE_IPV6 *p, SE_IPV6_ADDR dest_ip, UINT dest_port, SE_IPV6_ADDR src_ip, UINT src_port,
				void *data, UINT size, UCHAR *dest_mac)
{
	SE_UDP_HEADER *udp;
	UINT udp_packet_length = sizeof(SE_UDP_HEADER) + size;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}
	if (udp_packet_length > SE_IP6_MAX_PAYLOAD_SIZE)
	{
		return;
	}

	// UDP ヘッダの作成
	udp = SeZeroMalloc(udp_packet_length);
	udp->SrcPort = SeEndian16((USHORT)src_port);
	udp->DstPort = SeEndian16((USHORT)dest_port);
	udp->PacketLength = SeEndian16((USHORT)udp_packet_length);

	// データ本体のコピー
	SeCopy(((UCHAR *)udp) + sizeof(SE_UDP_HEADER), data, size);

	// チェックサム計算
	udp->Checksum = Se6CalcChecksum(src_ip, dest_ip, SE_IP_PROTO_UDP, udp, udp_packet_length);
	if (udp->Checksum == 0x0000)
	{
		udp->Checksum = 0xffff;
	}

	// 送信
	Se6SendIp(p, dest_ip, src_ip, SE_IP_PROTO_UDP, 0, udp, udp_packet_length, dest_mac);

	SeFree(udp);
}

// IP フラグメントの送信処理の実行
void Se6SendIpFragmentNow(SE_IPV6 *p, UCHAR *dest_mac, void *data, UINT size)
{
	// 引数チェック
	if (p == NULL || dest_mac == NULL || data == NULL || size == 0)
	{
		return;
	}

	// 送信
	Se6SendEthPacket(p, dest_mac, SE_MAC_PROTO_IPV6, data, size);
}

// Raw IP パケットの送信
void Se6SendRawIp(SE_IPV6 *p, void *data, UINT size, UCHAR *dest_mac)
{
	UCHAR *buf;
	SE_IPV6_HEADER *ip;
	SE_IPV6_ADDR dest_ip_local, dest_ip, src_ip;
	char dest_mac_tmp[6];
	// 引数チェック
	if (p == NULL || data == NULL || size == 0)
	{
		return;
	}

	if (size <= sizeof(SE_IPV6_HEADER))
	{
		return;
	}

	buf = (UCHAR *)data;

	// IP ヘッダ解釈
	ip = (SE_IPV6_HEADER *)data;

	dest_ip_local = dest_ip = ip->DestAddress;
	src_ip = ip->SrcAddress;

	// MAC アドレスの解決
	if (dest_mac == NULL)
	{
		// 宛先 IP アドレスの種類を確認
		UINT type = Se6GetIPAddrType(dest_ip);

		if (type & SE_IPV6_ADDR_UNICAST)
		{
			// ユニキャストアドレス
			SE_IPV6_NEIGHBOR_ENTRY *e;

			if ((type & SE_IPV6_ADDR_GLOBAL_UNICAST) &&
				(Se6IsInSameNetwork(p->GlobalIpAddress, dest_ip, p->SubnetMask)) == false)
			{
				// ルーティングが必要な IP アドレスである
				if (p->UseDefaultGateway)
				{
					dest_ip_local = p->DefaultGateway;
				}
				else
				{
					// デフォルトゲートウェイが存在しないのでパケットを破棄
					return;
				}
			}

			// 近隣テーブルの検索
			e = Se6SearchNeighborEntryList(p->NeighborEntryList, dest_ip_local, Se6Tick(p));

			if (e != NULL)
			{
				dest_mac = e->MacAddress;
			}
		}
		else
		{
			// マルチキャストアドレスなので宛先 MAC アドレスを生成する
			Se6GenerateMulticastMacAddress(dest_mac_tmp, dest_ip);
			dest_mac = dest_mac_tmp;
		}
	}

	if (dest_mac != NULL)
	{
		// パケットの送信処理を実行
		Se6SendIpFragmentNow(p, dest_mac, buf, size);

		// バッファ解放
		SeFree(buf);
	}
	else
	{
		// IP 送信待ちテーブルに格納
		Se6InsertIpWait(p->IpWaitList, Se6Tick(p), dest_ip_local, src_ip, buf, size);

		// 近隣要請の送信
		Se6SendNeighborSoliciation(p, dest_ip_local);
	}
}

// IP フラグメントパケットの送信
void Se6SendIpFragment(SE_IPV6 *p, void *data, UINT size, UCHAR *dest_mac)
{
	// 引数チェック
	if (p == NULL || data == NULL || size == 0)
	{
		return;
	}

	Se6SendRawIp(p, data, size, dest_mac);
}

// IP パケットの送信
void Se6SendIp(SE_IPV6 *p, SE_IPV6_ADDR dest_ip, SE_IPV6_ADDR src_ip, UCHAR protocol, UCHAR hop_limit, void *data,
			   UINT size, UCHAR *dest_mac)
{
	UINT id;
	SE_LIST *o;
	SE_IPV6_HEADER_PACKET_INFO info;
	SE_IPV6_HEADER ip_header;
	UINT i;
	// 引数チェック
	if (p == NULL || data == NULL || size > SE_IP6_MAX_PAYLOAD_SIZE)
	{
		return;
	}
	if (hop_limit == 0)
	{
		hop_limit = SE_IPV6_SEND_HOP_LIMIT;
	}

	id = p->IdSeed++;

	// IPv6 ヘッダ
	SeZero(&ip_header, sizeof(ip_header));
	SE_IPV6_SET_VERSION(&ip_header, 6);
	ip_header.HopLimit = hop_limit;
	ip_header.SrcAddress = src_ip;
	ip_header.DestAddress = dest_ip;

	// ヘッダパケット情報の整理
	SeZero(&info, sizeof(info));
	info.IPv6Header = &ip_header;
	info.Protocol = protocol;
	info.Payload = data;
	info.PayloadSize = size;

	// 分割の実行
	o = SeBuildIPv6PacketWithFragment(&info, id, p->Mtu);

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_BUF *b = SE_LIST_DATA(o, i);

		Se6SendIpFragment(p, b->Buf, b->Size, dest_mac);
	}

	SeFreePacketListWithoutBuffer(o);
}

// UDP パケットの解析
bool Se6ParseIpPacketUDPv6(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size)
{
	SE_UDPV6_HEADER_INFO udp_info;
	SE_UDP_HEADER *udp;
	UINT packet_length;
	void *buf;
	UINT buf_size;
	USHORT src_port, dest_port;
	USHORT checksum_original, checksum_calc;
	// 引数チェック
	if (p == NULL || info == NULL || data == NULL || size == 0)
	{
		return false;
	}

	udp = (SE_UDP_HEADER *)data;
	if (size < sizeof(SE_UDP_HEADER))
	{
		return false;
	}
	packet_length = SeEndian16(udp->PacketLength);
	if (packet_length < size)
	{
		return false;
	}

	// チェックサムの計算
	checksum_original = udp->Checksum;
	udp->Checksum = 0;
	checksum_calc = Se6CalcChecksum(info->SrcIpAddress, info->DestIpAddress,
		info->Protocol, udp, packet_length);
	if (checksum_calc == 0)
	{
		checksum_calc = 0xffff;
	}
	udp->Checksum = checksum_original;

	if (checksum_original != checksum_calc)
	{
		// チェックサム不一致
		return false;
	}

	buf = ((UCHAR *)data) + sizeof(SE_UDP_HEADER);
	buf_size = packet_length - sizeof(SE_UDP_HEADER);
	src_port = SeEndian16(udp->SrcPort);
	dest_port = SeEndian16(udp->DstPort);
	if (dest_port == 0)
	{
		return false;
	}

	SeZero(&udp_info, sizeof(udp_info));

	udp_info.SrcPort = src_port;
	udp_info.DstPort = dest_port;
	udp_info.DataSize = buf_size;
	udp_info.Data = buf;

	info->TypeL4 = SE_L4_UDPV6;
	info->UDPv6Info = SeClone(&udp_info, sizeof(udp_info));

	return true;
}

// ICMP パケットの解析
bool Se6ParseIpPacketICMPv6(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, void *data, UINT size)
{
	SE_ICMPV6_HEADER_INFO icmp_info;
	SE_ICMP_HEADER *icmp;
	SE_ICMP_ECHO *echo;
	UINT msg_size;
	USHORT checksum_calc, checksum_original;
	// 引数チェック
	if (p == NULL || info == NULL || data == NULL || size == 0)
	{
		return false;
	}

	SeZero(&icmp_info, sizeof(icmp_info));

	if (size < sizeof(SE_ICMP_HEADER))
	{
		return false;
	}

	icmp = (SE_ICMP_HEADER *)data;

	// メッセージサイズ
	msg_size = size - sizeof(SE_ICMP_HEADER);

	// チェックサムの計算
	checksum_original = icmp->Checksum;
	icmp->Checksum = 0;
	checksum_calc = Se6CalcChecksum(info->SrcIpAddress, info->DestIpAddress, info->Protocol, data, size);
	icmp->Checksum = checksum_original;

	if (checksum_calc != checksum_original)
	{
		// チェックサム不一致
		return false;
	}

	icmp_info.Type = icmp->Type;
	icmp_info.Code = icmp->Code;
	icmp_info.Data = ((UCHAR *)data) + sizeof(SE_ICMP_HEADER);
	icmp_info.DataSize = msg_size;

	switch (icmp_info.Type)
	{
	case SE_ICMPV6_TYPE_ECHO_REQUEST:
	case SE_ICMPV6_TYPE_ECHO_RESPONSE:
		// ICMP Echo Request / Response
		if (icmp_info.DataSize < sizeof(SE_ICMP_ECHO))
		{
			return false;
		}

		echo = (SE_ICMP_ECHO *)icmp_info.Data;

		icmp_info.EchoHeader.Identifier = SeEndian16(echo->Identifier);
		icmp_info.EchoHeader.SeqNo = SeEndian16(echo->SeqNo);
		icmp_info.EchoData = (UCHAR *)echo + sizeof(SE_ICMP_ECHO);
		icmp_info.EchoDataSize = icmp_info.DataSize - sizeof(SE_ICMP_ECHO);

		break;

	case SE_ICMPV6_TYPE_ROUTER_SOLICIATION:
		// ルータ要請
		if (icmp_info.DataSize < sizeof(SE_ICMPV6_ROUTER_SOLICIATION_HEADER))
		{
			return false;
		}

		icmp_info.Headers.RouterSoliciationHeader =
			(SE_ICMPV6_ROUTER_SOLICIATION_HEADER *)(((UCHAR *)icmp_info.Data));

		if (SeParseICMPv6Options(&icmp_info.OptionList, ((UCHAR *)icmp_info.Headers.HeaderPointer) + sizeof(SE_ICMPV6_ROUTER_SOLICIATION_HEADER),
			icmp_info.DataSize - sizeof(SE_ICMPV6_ROUTER_SOLICIATION_HEADER)) == false)
		{
			return false;
		}

		break;

	case SE_ICMPV6_TYPE_ROUTER_ADVERTISEMENT:
		// ルータ広告
		if (icmp_info.DataSize < sizeof(SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER))
		{
			return false;
		}

		icmp_info.Headers.RouterAdvertisementHeader =
			(SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER *)(((UCHAR *)icmp_info.Data));

		if (SeParseICMPv6Options(&icmp_info.OptionList, ((UCHAR *)icmp_info.Headers.HeaderPointer) + sizeof(SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER),
			icmp_info.DataSize - sizeof(SE_ICMPV6_ROUTER_ADVERTISEMENT_HEADER)) == false)
		{
			return false;
		}

		break;

	case SE_ICMPV6_TYPE_NEIGHBOR_SOLICIATION:
		// 近隣要請
		if (icmp_info.DataSize < sizeof(SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER))
		{
			return false;
		}

		icmp_info.Headers.NeighborSoliciationHeader =
			(SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER *)(((UCHAR *)icmp_info.Data));

		if (SeParseICMPv6Options(&icmp_info.OptionList, ((UCHAR *)icmp_info.Headers.HeaderPointer) + sizeof(SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER),
			icmp_info.DataSize - sizeof(SE_ICMPV6_NEIGHBOR_SOLICIATION_HEADER)) == false)
		{
			return false;
		}

		break;

	case SE_ICMPV6_TYPE_NEIGHBOR_ADVERTISEMENT:
		// 近隣広告
		if (icmp_info.DataSize < sizeof(SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER))
		{
			return false;
		}

		icmp_info.Headers.NeighborAdvertisementHeader =
			(SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER *)(((UCHAR *)icmp_info.Data));

		if (SeParseICMPv6Options(&icmp_info.OptionList, ((UCHAR *)icmp_info.Headers.HeaderPointer) + sizeof(SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER),
			icmp_info.DataSize - sizeof(SE_ICMPV6_NEIGHBOR_ADVERTISEMENT_HEADER)) == false)
		{
			return false;
		}

		break;
	}

	info->TypeL4 = SE_L4_ICMPV6;
	info->ICMPv6Info = SeClone(&icmp_info, sizeof(icmp_info));

	return true;
}

// ICMP, TCP, UDP 等のためのチェックサム計算
USHORT Se6CalcChecksum(SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UCHAR protocol, void *data, UINT size)
{
	UCHAR *tmp;
	UINT tmp_size;
	SE_IPV6_PSEUDO_HEADER *ph;
	USHORT ret;
	// 引数チェック
	if (data == NULL && size != 0)
	{
		return 0;
	}

	tmp_size = size + sizeof(SE_IPV6_PSEUDO_HEADER);
	tmp = SeZeroMalloc(tmp_size);

	ph = (SE_IPV6_PSEUDO_HEADER *)tmp;
	ph->SrcAddress = src_ip;
	ph->DestAddress = dest_ip;
	ph->UpperLayerPacketSize = SeEndian32(size);
	ph->NextHeader = protocol;

	SeCopy(((UCHAR *)tmp) + sizeof(SE_IPV6_PSEUDO_HEADER), data, size);

	ret = Se6IpChecksum(tmp, tmp_size);

	SeFree(tmp);

	return ret;
}

// IP パケットの解析
bool Se6ParseIpPacket(SE_IPV6 *p, SE_IPV6_HEADER_INFO *info, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip,
					  UINT id, UCHAR protocol, UCHAR hop_limit, void *data, UINT size)
{
	UINT type;
	// 引数チェック
	if (p == NULL || info == NULL || data == NULL || size == 0)
	{
		return false;
	}

	SeZero(info, sizeof(SE_IPV6_HEADER_INFO));

	info->Size = size;
	info->Id = id;
	info->Protocol = protocol;
	info->SrcIpAddress = src_ip;
	info->DestIpAddress = dest_ip;
	info->HopLimit = hop_limit;

	type = Se6GetIPAddrType(dest_ip);
	if (type & SE_IPV6_ADDR_UNICAST)
	{
		if (Se6Cmp(dest_ip, p->LocalIpAddress) == 0 || Se6Cmp(dest_ip, p->GlobalIpAddress) == 0)
		{
			info->UnicastForMe = true;
		}
		else
		{
			info->UnicastForRouting = true;

			if (Se6IsInSameNetwork(dest_ip, p->GlobalIpAddress, p->SubnetMask))
			{
				info->UnicastForRoutingWithProxyNdp = true;
			}
		}
	}

	switch (info->Protocol)
	{
	case SE_IP_PROTO_ICMPV6:	// ICMP
		if (Se6ParseIpPacketICMPv6(p, info, data, size) == false)
		{
			return false;
		}
		break;

	case SE_IP_PROTO_UDP:	// UDP
		if (Se6ParseIpPacketUDPv6(p, info, data, size) == false)
		{
			return false;
		}
		break;

	case SE_IP_PROTO_ESP:	// ESP
		info->TypeL4 = SE_L4_ESPV6;
		break;
	}

	return true;
}

// ゲストノードとして登録されているかどうか検査
bool Se6IsGuestNode(SE_IPV6 *p, SE_IPV6_ADDR a)
{
	UINT i;
	// 引数チェック
	if (p == NULL || Se6IsZeroIP(a))
	{
		return false;
	}

	for (i = 0;i < SE_IPV6_MAX_GUEST_NODES;i++)
	{
		if (Se6IsZeroIP(p->GuestNodes[i]))
		{
			return false;
		}
		if (Se6Cmp(p->GuestNodes[i], a) == 0)
		{
			return true;
		}
	}

	return false;
}

// ゲストノードとして登録
void Se6AddGuestNode(SE_IPV6 *p, SE_IPV6_ADDR a)
{
	// 引数チェック
	if (p == NULL || Se6IsZeroIP(a))
	{
		return;
	}

	if (Se6IsGuestNode(p, a))
	{
		return;
	}

	if (Se6IsInSameNetwork(a, p->GlobalIpAddress, p->SubnetMask) == false &&
		Se6IsInSameNetwork(a, p->LocalIpAddress, Se6LocalSubnet()) == false)
	{
		return;
	}

	p->GuestNodes[(p->GuestNodeIndexSeed++) % SE_IPV6_MAX_GUEST_NODES] = a;
}

// 完全な IP パケットの受信
void Se6RecvIpComplete(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip, UINT id, UCHAR protocol,
					   UCHAR hop_limit, void *data, UINT size, UCHAR *src_mac)
{
	SE_IPV6_HEADER_INFO info;
	// 引数チェック
	if (p == NULL || data == NULL || size == 0)
	{
		return;
	}

	if (Se6ParseIpPacket(p, &info, src_ip, dest_ip, id, protocol, hop_limit, data, size))
	{
		if (info.UnicastForMe)
		{
			if (info.TypeL4 == SE_L4_ICMPV6)
			{
				if (info.ICMPv6Info->Type == SE_ICMPV6_TYPE_ECHO_REQUEST)
				{
					// 自分宛の ping が届いたので応答する
					Se6SendIcmpEchoResponse(p, dest_ip, src_ip,
						info.ICMPv6Info->EchoHeader.Identifier,
						info.ICMPv6Info->EchoHeader.SeqNo,
						info.ICMPv6Info->EchoData,
						info.ICMPv6Info->EchoDataSize);
				}
			}
		}

		if (p->UseProxyNdp)
		{
			// 届いた IP パケットの送信元 IP アドレスをゲストノードとして登録する
			Se6AddGuestNode(p, src_ip);
		}

		if (info.TypeL4 == SE_L4_ICMPV6)
		{
			SE_ICMPV6_HEADER_INFO *icmp_info = info.ICMPv6Info;

			if (icmp_info->Type == SE_ICMPV6_TYPE_ROUTER_SOLICIATION)
			{
				if (hop_limit == SE_IPV6_HOP_MAX)
				{
					// RA の送信
					if (p->SendRa)
					{
						Se6SendIcmpRouterAdvertisement(p);
					}
				}
			}

			if (icmp_info->Type == SE_ICMPV6_TYPE_NEIGHBOR_SOLICIATION)
			{
				if (hop_limit == SE_IPV6_HOP_MAX)
				{
					if (icmp_info->OptionList.SourceLinkLayer != NULL)
					{
						bool b = false;
						bool router = false;

						if (Se6Cmp(icmp_info->Headers.NeighborSoliciationHeader->TargetAddress, p->GlobalIpAddress) == 0)
						{
							// 自分をターゲットとした近隣要請が届いた
							b = true;
						}

						if (Se6Cmp(icmp_info->Headers.NeighborSoliciationHeader->TargetAddress, p->LocalIpAddress) == 0)
						{
							// 自分をターゲットとした近隣要請が届いた
							b = true;
							// 自分はルータである
							router = true;
						}

						if (p->UseProxyNdp)
						{
							if (Se6GetIPAddrType(icmp_info->Headers.NeighborSoliciationHeader->TargetAddress) & SE_IPV6_ADDR_GLOBAL_UNICAST)
							{
								if (Se6IsZeroIP(src_ip) == false)
								{
									if (Se6IsGuestNode(p, dest_ip) == false)
									{
										// プロキシ NDP として対応すべき近隣要請が届いた
										b = true;
									}
								}
								else
								{
									// ゲストノードと思われるノードからの DAD パケットが届いた
									if (b == false)
									{
										Se6AddGuestNode(p, icmp_info->Headers.NeighborSoliciationHeader->TargetAddress);
									}
								}
							}
						}

						if (b)
						{
							// 近隣応答を返信する
							if (icmp_info->OptionList.SourceLinkLayer != NULL)
							{
								Se6SendIcmpNeighborAdvertisement(p, router,
									icmp_info->Headers.NeighborSoliciationHeader->TargetAddress,
									info.SrcIpAddress,
									p->Eth->MyMacAddress,
									icmp_info->OptionList.SourceLinkLayer->Address);
							}
						}
					}
				}
			}
		}

		// 受信コールバック
		p->RecvCallback(p, &info, data, size, p->RecvCallbackParam);

		Se6FreeIpHeaderInfo(&info);
	}
}

// IP ヘッダ情報構造体の使用したメモリの解放
void Se6FreeIpHeaderInfo(SE_IPV6_HEADER_INFO *info)
{
	// 引数チェック
	if (info == NULL)
	{
		return;
	}

	if (info->ICMPv6Info != NULL)
	{
		SeFree(info->ICMPv6Info);
	}

	if (info->UDPv6Info != NULL)
	{
		SeFree(info->UDPv6Info);
	}
}

// IP パケットの受信
void Se6RecvIp(SE_IPV6 *p, SE_PACKET *pkt)
{
	SE_IPV6_HEADER *ip;
	void *data;
	SE_IPV6_HEADER_PACKET_INFO *info;
	// 引数チェック
	if (p == NULL || pkt == NULL)
	{
		return;
	}

	ip = pkt->L3.IPv6Header;

	// データへのポインタを取得
	data = ((UCHAR *)pkt->L3.PointerL3) + sizeof(SE_IPV6_HEADER);

	// IP アドレスと MAC アドレスとの関連付け
	Se6MacIpRelationKnown(p, pkt->L3.IPv6Header->SrcAddress, pkt->MacHeader->SrcAddress);
	info = &pkt->IPv6HeaderPacketInfo;

	if (p->UseRawIp)
	{
		// Raw パケットの受信処理をする
		SE_IPV6_HEADER_INFO head_info;

		SeZero(&head_info, sizeof(head_info));
		head_info.IsRawIpPacket = true;
		head_info.SrcIpAddress = ip->SrcAddress;
		head_info.DestIpAddress = ip->DestAddress;

		p->RecvCallback(p, &head_info, ip, pkt->PacketSize - sizeof(SE_MAC_HEADER), p->RecvCallbackParam);
	}

	if (info->FragmentHeader == NULL ||
		(SE_IPV6_GET_FRAGMENT_OFFSET(info->FragmentHeader) == 0 &&
		(SE_IPV6_GET_FLAGS(info->FragmentHeader) & SE_IPV6_FRAGMENT_HEADER_FLAG_MORE_FRAGMENTS) == 0))
	{
		// 分割されていない IP パケットを受信
		Se6RecvIpComplete(p, ip->SrcAddress, ip->DestAddress, 0, info->Protocol, ip->HopLimit,
			info->Payload, info->PayloadSize, pkt->MacHeader->SrcAddress);
	}
	else
	{
		// 分割された IP パケットの一部を受信
		SE_IPV6_COMBINE *c;
		UINT offset;
		bool is_last_packet;

		offset = SE_IPV6_GET_FRAGMENT_OFFSET(info->FragmentHeader) * 8;
		c = Se6SearchIpCombineList(p->IpCombineList, ip->DestAddress, ip->SrcAddress,
			SeEndian32(info->FragmentHeader->Identification), info->Protocol);
		is_last_packet = ((SE_IPV6_GET_FLAGS(info->FragmentHeader) & SE_IPV6_FRAGMENT_HEADER_FLAG_MORE_FRAGMENTS) == 0 ? true : false);

		if (c == NULL)
		{
			// 最初のパケット
			c = Se6InsertIpCombine(p, ip->SrcAddress, ip->DestAddress, SeEndian32(info->FragmentHeader->Identification),
				info->Protocol, ip->HopLimit, pkt->MacHeader->SrcAddress);

			if (c != NULL)
			{
				Se6CombineIp(p, c, offset, info->Payload, info->PayloadSize, is_last_packet);
			}
		}
		else
		{
			// 2 個目以降のパケット
			Se6CombineIp(p, c, offset, info->Payload, info->PayloadSize, is_last_packet);
		}
	}
}

// チェックサムを計算する
USHORT Se6IpChecksum(void *buf, UINT size)
{
	int sum = 0;
	USHORT *addr = (USHORT *)buf;
	int len = (int)size;
	USHORT *w = addr;
	int nleft = len;
	USHORT answer = 0;

	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1)
	{
		*(UCHAR *)(&answer) = *(UCHAR *)w;
		sum += answer;
	}

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	answer = ~sum;

	return answer;
}

// IP アドレスと MAC アドレスの関連付けが判明した
void Se6MacIpRelationKnown(SE_IPV6 *p, SE_IPV6_ADDR ip_addr, UCHAR *mac_addr)
{
	SE_NDPV6_WAIT *w;
	UINT type;
	// 引数チェック
	if (p == NULL || mac_addr == NULL)
	{
		return;
	}

	// アドレスタイプの検査
	type = Se6GetIPAddrType(ip_addr);
	if ((type & SE_IPV6_ADDR_UNICAST) == 0)
	{
		// ユニキャストアドレス以外は登録しない
		return;
	}

	// 自分の所属している IP ネットワークのアドレスかどうか判定
	if (Se6IsInSameNetwork(p->GlobalIpAddress, ip_addr, p->SubnetMask) == false &&
		Se6IsInSameNetwork(p->LocalIpAddress, ip_addr, Se6LocalSubnet()) == false)
	{
		// 異なる IP ネットワークのアドレス (ルーティングされてきたアドレス)
		// は近隣テーブルに登録しない
		return;
	}

	// NDP 待機リストを検索
	w = Se6SearchNdpWaitList(p->NdpWaitList, ip_addr);
	if (w != NULL)
	{
		// エントリを削除
		SeDelete(p->NdpWaitList, w);

		SeFree(w);
	}

	// 近隣テーブルに登録
	Se6AddNeighborEntryList(p->NeighborEntryList, Se6Tick(p), p->Vpn->Config->OptionV6NeighborExpires,
		ip_addr, mac_addr);

	// IP 待機リストで待機している IP パケットがあればすべて送信する
	Se6SendWaitingIpWait(p, ip_addr, mac_addr);
}

// プロードキャスト MAC アドレス
UCHAR *Se6BroadcastMacAddress()
{
	static UCHAR mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, };

	return mac;
}

// Ethernet パケットの送信
void Se6SendEthPacket(SE_IPV6 *p, UCHAR *dest_mac, USHORT protocol, void *data, UINT data_size)
{
	SE_MAC_HEADER *mac_header;
	UCHAR *buf;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}
	if (dest_mac == NULL)
	{
		dest_mac = Se6BroadcastMacAddress();
	}

	buf = SeZeroMalloc(sizeof(SE_MAC_HEADER) + data_size);

	// MAC ヘッダの構築
	mac_header = (SE_MAC_HEADER *)buf;
	SeCopy(mac_header->DestAddress, dest_mac, SE_ETHERNET_MAC_ADDR_SIZE);
	SeCopy(mac_header->SrcAddress, p->Eth->MyMacAddress, SE_ETHERNET_MAC_ADDR_SIZE);
	mac_header->Protocol = SeEndian16(protocol);

	// データ
	SeCopy(buf + sizeof(SE_MAC_HEADER), data, data_size);

	// 送信
	SeVpnSendEtherPacket(p->Vpn, p->Eth, buf, sizeof(SE_MAC_HEADER) + data_size);

	SeFree(buf);
}

// Ethernet パケットの受信
void Se6RecvEthPacket(SE_IPV6 *p, void *packet, UINT packet_size)
{
	SE_PACKET *pkt;
	// 引数チェック
	if (p == NULL || packet == NULL)
	{
		return;
	}

	pkt = SeParsePacket(packet, packet_size);

	// NULL ポインタチェック
	if (pkt == NULL)
	{
		return;		/* FIXME */
	}

	// 送信者が自分自身であるパケットは必ず無視する
	if (SeCmp(p->Eth->MyMacAddress, pkt->MacHeader->SrcAddress, SE_ETHERNET_MAC_ADDR_SIZE) != 0)
	{
		switch (pkt->TypeL3)
		{
		case SE_L3_IPV6:	// IPv6 パケット
			Se6RecvIp(p, pkt);
			break;
		}
	}

	SeFreePacket(pkt);
}

// 近隣エントリリストの比較関数
int Se6CmpNeighborEntry(void *p1, void *p2)
{
	SE_IPV6_NEIGHBOR_ENTRY *a1, *a2;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	a1 = *(SE_IPV6_NEIGHBOR_ENTRY **)p1;
	a2 = *(SE_IPV6_NEIGHBOR_ENTRY **)p2;
	if (a1 == NULL || a2 == NULL)
	{
		return 0;
	}

	return Se6Cmp(a1->IpAddress, a2->IpAddress);
}

// 近隣エントリリストの初期化
SE_LIST *Se6InitNeighborEntryList()
{
	return SeNewList(Se6CmpNeighborEntry);
}

// 近隣エントリリストの解放
void Se6FreeNeighborEntryList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV6_NEIGHBOR_ENTRY *e = SE_LIST_DATA(o, i);

		SeFree(e);
	}

	SeFreeList(o);
}

// 近隣エントリリストのフラッシュ
void Se6FlushNeighborEntryList(SE_LIST *o, UINT64 tick)
{
	SE_LIST *t;
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	t = NULL;

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV6_NEIGHBOR_ENTRY *e = SE_LIST_DATA(o, i);

		if (tick > e->Expire)
		{
			if (t == NULL)
			{
				t = SeNewList(NULL);
			}

			SeAdd(t, e);
		}
	}

	if (t != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(t);i++)
		{
			SE_IPV6_NEIGHBOR_ENTRY *e = SE_LIST_DATA(t, i);

			SeDelete(o, e);
			SeFree(e);
		}

		SeFreeList(t);
	}
}

// 近隣エントリリストの検索
SE_IPV6_NEIGHBOR_ENTRY *Se6SearchNeighborEntryList(SE_LIST *o, SE_IPV6_ADDR a, UINT64 tick)
{
	SE_IPV6_NEIGHBOR_ENTRY t, *r;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	Se6FlushNeighborEntryList(o, tick);

	SeZero(&t, sizeof(t));
	t.IpAddress = a;

	r = SeSearch(o, &t);
	if (r == NULL)
	{
		return NULL;
	}

	return r;
}

// 近隣エントリの追加
void Se6AddNeighborEntryList(SE_LIST *o, UINT64 tick, UINT expire_span, SE_IPV6_ADDR ip_address, UCHAR *mac_address)
{
	SE_IPV6_NEIGHBOR_ENTRY *t;
	// 引数チェック
	if (o == NULL || mac_address == NULL)
	{
		return;
	}

	t = Se6SearchNeighborEntryList(o, ip_address, tick);
	if (t != NULL)
	{
		if (SeCmp(t->MacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE) != 0)
		{
			SeCopy(t->MacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE);
			t->Created = tick;
		}
	}
	else
	{
		t = SeZeroMalloc(sizeof(SE_IPV6_NEIGHBOR_ENTRY));

		t->IpAddress = ip_address;
		SeCopy(t->MacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE);
		t->Created = tick;
		t->Expire = tick + (UINT64)expire_span * 1000ULL;

		SeInsert(o, t);
	}
}

// NDP 待機リストの検索
SE_NDPV6_WAIT *Se6SearchNdpWaitList(SE_LIST *o, SE_IPV6_ADDR a)
{
	SE_NDPV6_WAIT t, *w;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	SeZero(&t, sizeof(t));
	t.IpAddress = a;

	w = SeSearch(o, &t);

	return w;
}

// NDP 待機リストの比較関数
int Se6CmpNdpWaitEntry(void *p1, void *p2)
{
	SE_NDPV6_WAIT *w1, *w2;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	w1 = *(SE_NDPV6_WAIT **)p1;
	w2 = *(SE_NDPV6_WAIT **)p2;
	if (w1 == NULL || w2 == NULL)
	{
		return 0;
	}

	return Se6Cmp(w1->IpAddress, w2->IpAddress);
}

// NDP 待機リストの初期化
SE_LIST *Se6InitNdpWaitList()
{
	return SeNewList(Se6CmpNdpWaitEntry);
}

// NDP 待機リストの解放
void Se6FreeNdpWaitList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_NDPV6_WAIT *w = SE_LIST_DATA(o, i);

		SeFree(w);
	}

	SeFreeList(o);
}

// 指定した IP アドレス宛の待機している IP パケットを一斉に送信する
void Se6SendWaitingIpWait(SE_IPV6 *p, SE_IPV6_ADDR ip_addr_local, UCHAR *mac_addr)
{
	UINT i;
	SE_LIST *o;
	// 引数チェック
	if (p == NULL || mac_addr == NULL)
	{
		return;
	}

	o = NULL;

	for (i = 0;i < SE_LIST_NUM(p->IpWaitList);i++)
	{
		SE_IPV6_WAIT *w = SE_LIST_DATA(p->IpWaitList, i);

		if (Se6Cmp(w->DestIPLocal, ip_addr_local) == 0)
		{
			if (o == NULL)
			{
				o = SeNewList(NULL);
			}

			SeAdd(o, w);
		}
	}

	if (o != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(o);i++)
		{
			SE_IPV6_WAIT *w = SE_LIST_DATA(o, i);

			// 送信処理
			Se6SendIpFragmentNow(p, mac_addr, w->Data, w->Size);

			// リストから削除
			SeDelete(p->IpWaitList, w);

			// 解放
			Se6FreeIpWait(w);
		}

		SeFreeList(o);
	}
}

// 古い IP 待機リストの削除
void Se6FlushIpWaitList(SE_IPV6 *p)
{
	SE_LIST *o;
	UINT i;
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	o = NULL;

	for (i = 0;i < SE_LIST_NUM(p->IpWaitList);i++)
	{
		SE_IPV6_WAIT *w = SE_LIST_DATA(p->IpWaitList, i);

		if (w->Expire <= Se6Tick(p))
		{
			if (o == NULL)
			{
				o = SeNewList(NULL);
			}

			SeAdd(o, w);
		}
	}

	if (o != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(o);i++)
		{
			SE_IPV6_WAIT *w = SE_LIST_DATA(o, i);

			Se6FreeIpWait(w);

			SeDelete(p->IpWaitList, w);
		}

		SeFreeList(o);
	}
}

// IP 待機リストを挿入
void Se6InsertIpWait(SE_LIST *o, UINT64 tick, SE_IPV6_ADDR dest_ip_local, SE_IPV6_ADDR src_ip, void *data, UINT size)
{
	SE_IPV6_WAIT *w;
	// 引数チェック
	if (o == NULL || data == NULL)
	{
		return;
	}

	w = SeZeroMalloc(sizeof(SE_IPV6_WAIT));
	w->Data = data;
	w->Size = size;
	w->SrcIP = src_ip;
	w->DestIPLocal = dest_ip_local;
	w->Expire = tick + (UINT64)(SE_IPV6_NDP_SEND_INTERVAL * SE_IPV6_NDP_SEND_COUNT) * 1000ULL;

	SeAdd(o, w);
}

// IP 待機リストの初期化
SE_LIST *Se6InitIpWaitList()
{
	return SeNewList(NULL);
}

// IP 待機エントリの解放
void Se6FreeIpWait(SE_IPV6_WAIT *w)
{
	// 引数チェック
	if (w == NULL)
	{
		return;
	}

	SeFree(w->Data);
	SeFree(w);
}

// IP 待機リストの解放
void Se6FreeIpWaitList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV6_WAIT *w = SE_LIST_DATA(o, i);

		Se6FreeIpWait(w);
	}

	SeFreeList(o);
}

// IP 結合リストの比較
int Se6CmpIpCombineList(void *p1, void *p2)
{
	SE_IPV6_COMBINE *c1, *c2;
	UINT i;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	c1 = *(SE_IPV6_COMBINE **)p1;
	c2 = *(SE_IPV6_COMBINE **)p2;
	if (c1 == NULL || c2 == NULL)
	{
		return 0;
	}

	i = Se6Cmp(c1->DestIpAddress, c2->DestIpAddress);
	if (i != 0)
	{
		return i;
	}

	i = Se6Cmp(c1->SrcIpAddress, c2->SrcIpAddress);
	if (i != 0)
	{
		return i;
	}

	if (c1->Id > c2->Id)
	{
		return 1;
	}
	else if (c1->Id < c2->Id)
	{
		return -1;
	}

	return 0;
}

// IP 結合リストの初期化
SE_LIST *Se6InitIpCombineList()
{
	return SeNewList(Se6CmpIpCombineList);
}

// IP 結合リストの検索
SE_IPV6_COMBINE *Se6SearchIpCombineList(SE_LIST *o, SE_IPV6_ADDR dest, SE_IPV6_ADDR src, UINT id, UCHAR protocol)
{
	SE_IPV6_COMBINE c, *ret;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	SeZero(&c, sizeof(c));
	c.DestIpAddress = dest;
	c.SrcIpAddress = src;
	c.Id = id;
	c.Protocol = protocol;

	ret = SeSearch(o, &c);

	return ret;
}

// IP 結合リストの解放
void Se6FreeIpCombineList(SE_IPV6 *p, SE_LIST *o)
{
	UINT i;
	// 結合チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV6_COMBINE *c = SE_LIST_DATA(o, i);

		Se6FreeIpCombine(p, c);
	}

	SeFreeList(o);
}

// IP 結合処理
void Se6CombineIp(SE_IPV6 *p, SE_IPV6_COMBINE *c, UINT offset, void *data, UINT size, bool last_packet)
{
	UINT i;
	UINT need_size;
	UINT data_size_delta;
	SE_IPV6_FRAGMENT *f;
	bool status_changed = false;
	// 引数チェック
	if (c == NULL || data == NULL)
	{
		return;
	}

	// オフセットとサイズをチェック
	if ((offset + size) > SE_IP6_MAX_PAYLOAD_SIZE)
	{
		// 大きすぎるので無視する
		return;
	}

	if (last_packet == false && c->Size != 0)
	{
		if ((offset + size) > c->Size)
		{
			// パケットサイズより大きいパケットは処理しない
			return;
		}
	}

	need_size = offset + size;
	data_size_delta = c->DataReserved;

	// バッファが不足している場合は拡張する
	while (c->DataReserved < need_size)
	{
		c->DataReserved *= 4;
		c->Data = SeReAlloc(c->Data, c->DataReserved);
	}
	data_size_delta = c->DataReserved - data_size_delta;
	p->CurrentIpQuota += data_size_delta;

	// データをバッファに上書きする
	SeCopy(((UCHAR *)c->Data) + offset, data, size);

	if (last_packet)
	{
		// No More Flagment パケットが届いた場合、このデータグラムのサイズが確定する
		c->Size = offset + size;
	}

	// オフセットとサイズによって表現されている領域と既存の受信済みリストの
	// オフセットとサイズによって表現されている領域との間の重複をチェックする
	for (i = 0;i < SE_LIST_NUM(c->IpFragmentList);i++)
	{
		UINT moving_size;
		SE_IPV6_FRAGMENT *f = SE_LIST_DATA(c->IpFragmentList, i);

		// 先頭領域と既存領域との重複をチェック
		if ((f->Offset <= offset) && ((f->Offset + f->Size) > offset))
		{
			// このパケットと既存パケットとの間で先頭部分に重複が見つかったので
			// このパケットのオフセットを後方に圧縮する

			if ((offset + size) <= (f->Offset + f->Size))
			{
				// このパケットは既存のパケットの中に埋もれている
				size = 0;
			}
			else
			{
				// 後方領域は重なっていない
				moving_size = f->Offset + f->Size - offset;
				offset += moving_size;
				size -= moving_size;
			}
		}
		if ((f->Offset < (offset + size)) && ((f->Offset + f->Size) >= (offset + size)))
		{
			// このパケットと既存パケットとの間で後方部分に重複が見つかったので
			// このパケットのサイズを前方に圧縮する

			moving_size = f->Offset + f->Size - offset - size;
			size -= moving_size;
		}

		if ((f->Offset >= offset) && ((f->Offset + f->Size) <= (offset + size)))
		{
			// このパケットが既存のパケットを完全に覆いかぶさるように上書きされた
			f->Size = 0;
		}
	}

	if (size != 0)
	{
		// このパケットを登録する
		f = SeZeroMalloc(sizeof(SE_IPV6_FRAGMENT));

		f->Offset = offset;
		f->Size = size;

		SeAdd(c->IpFragmentList, f);

		status_changed = true;
	}

	if (status_changed)
	{
		c->Expire = Se6Tick(p) + SE_IPV6_COMBINE_TIMEOUT * 1000ULL;
	}

	if (c->Size != 0)
	{
		// すでに受信したデータ部分リストの合計サイズを取得する
		UINT total_size = 0;
		UINT i;

		for (i = 0;i < SE_LIST_NUM(c->IpFragmentList);i++)
		{
			SE_IPV6_FRAGMENT *f = SE_LIST_DATA(c->IpFragmentList, i);

			total_size += f->Size;
		}

		if (total_size == c->Size)
		{
			// IP パケットをすべて受信した
			Se6RecvIpComplete(p, c->SrcIpAddress, c->DestIpAddress, c->Id,
				c->Protocol, c->HopLimit, c->Data, c->Size, c->SrcMacAddress);

			// 結合オブジェクトの解放
			Se6FreeIpCombine(p, c);

			// 結合オブジェクトをリストから削除
			SeDelete(p->IpCombineList, c);
		}
	}
}

// 古くなった IP 結合リストの削除
void Se6FlushIpCombineList(SE_IPV6 *p, SE_LIST *o)
{
	SE_LIST *t;
	UINT i;
	// 引数チェック
	if (p == NULL || o == NULL)
	{
		return;
	}

	t = NULL;

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV6_COMBINE *c = SE_LIST_DATA(o, i);

		if (c->Expire <= Se6Tick(p) ||
		    p->combine_current_id - c->combine_id > SE_IPV6_COMBINE_MAX_COUNT)
		{
			if (t == NULL)
			{
				t = SeNewList(NULL);
			}

			SeAdd(t, c);
		}
	}

	if (t != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(t);i++)
		{
			SE_IPV6_COMBINE *c = SE_LIST_DATA(t, i);

			Se6FreeIpCombine(p, c);

			SeDelete(o, c);
		}

		SeFreeList(t);
	}
}

// IP 結合エントリの挿入
SE_IPV6_COMBINE *Se6InsertIpCombine(SE_IPV6 *p, SE_IPV6_ADDR src_ip, SE_IPV6_ADDR dest_ip,
									UINT id, UCHAR protocol, UCHAR hop_limit, UCHAR *src_mac)
{
	SE_IPV6_COMBINE *c;
	// 引数チェック
	if (p == NULL)
	{
		return NULL;
	}

	// クォータの調査
	if ((p->CurrentIpQuota + SE_IPV6_COMBINE_INITIAL_BUF_SIZE) > SE_IPV6_COMBINE_QUEUE_SIZE_QUOTA)
	{
		// メモリ不足
		return NULL;
	}

	c = SeZeroMalloc(sizeof(SE_IPV6_COMBINE));
	c->DestIpAddress = dest_ip;
	c->SrcIpAddress = src_ip;
	c->Id = id;
	c->Expire = Se6Tick(p) + (UINT64)SE_IPV6_COMBINE_TIMEOUT * 1000ULL;
	c->Size = 0;
	c->IpFragmentList = Se6InitIpFragmentList();
	c->Protocol = protocol;
	c->HopLimit = hop_limit;
	c->combine_id = p->combine_current_id++;

	c->DataReserved = SE_IPV6_COMBINE_INITIAL_BUF_SIZE;
	c->Data = SeMalloc(c->DataReserved);

	SeInsert(p->IpCombineList, c);
	p->CurrentIpQuota += c->DataReserved;

	SeCopy(c->SrcMacAddress, src_mac, 6);

	return c;
}

// IP 結合エントリの解放
void Se6FreeIpCombine(SE_IPV6 *p, SE_IPV6_COMBINE *c)
{
	// 引数チェック
	if (c == NULL)
	{
		return;
	}

	p->CurrentIpQuota -= c->DataReserved;
	SeFree(c->Data);
	Se6FreeIpFragmentList(c->IpFragmentList);

	SeFree(c);
}

// IP フラグメントリストの初期化
SE_LIST *Se6InitIpFragmentList()
{
	return SeNewList(NULL);
}

// IP フラグメントリストの解放
void Se6FreeIpFragmentList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV6_FRAGMENT *f = SE_LIST_DATA(o, i);

		SeFree(f);
	}

	SeFreeList(o);
}

// IPv6 バインド (初期化)
SE_IPV6 *Se6Init(SE_VPN *vpn, SE_ETH *eth, bool physical, SE_IPV6_ADDR global_ip, SE_IPV6_ADDR subnet,
				 SE_IPV6_ADDR gateway, UINT mtu, SE_IPV6_RECV_CALLBACK *recv_callback, void *recv_callback_param)
{
	SE_IPV6 *p;
	// 引数チェック
	if (vpn == NULL || eth == NULL || recv_callback == NULL)
	{
		return NULL;
	}

	p = SeZeroMalloc(sizeof(SE_IPV6));
	p->Vpn = vpn;
	p->Eth = eth;
	p->Physical = physical;
	p->RecvCallback = recv_callback;
	p->RecvCallbackParam = recv_callback_param;
	p->GlobalIpAddress = global_ip;
	p->SubnetMask = subnet;
	p->PrefixAddress = Se6GetPrefixAddress(p->GlobalIpAddress, p->SubnetMask);
	p->LocalIpAddress = Se6GenerateEui64LocalAddress(eth->MyMacAddress);
	if (Se6IsZeroIP(gateway) == false)
	{
		p->DefaultGateway = gateway;
		p->UseDefaultGateway = true;
	}
	p->Mtu = MAX(mtu, SE_V6_MTU_MIN);

	p->NeighborEntryList = Se6InitNeighborEntryList();
	p->NdpWaitList = Se6InitNdpWaitList();
	p->IpWaitList = Se6InitIpWaitList();
	p->IpCombineList = Se6InitIpCombineList();

	return p;
}

// IPv6 バインド解除 (解放)
void Se6Free(SE_IPV6 *p)
{
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	Se6FreeIpCombineList(p, p->IpCombineList);
	Se6FreeIpWaitList(p->IpWaitList);
	Se6FreeNdpWaitList(p->NdpWaitList);
	Se6FreeNeighborEntryList(p->NeighborEntryList);

	SeFree(p);
}

// 現在の Tick 値を取得する
UINT64 Se6Tick(SE_IPV6 *p)
{
	SE_VPN *v;
	// 引数チェック
	if (p == NULL)
	{
		return 0;
	}

	v = p->Vpn;

	return SeVpnTick(v);
}

// ローカル IP アドレスのサブネットマスク
SE_IPV6_ADDR Se6LocalSubnet()
{
	return Se6GenerateSubnetMask(64);
}

// サブネットマスクを整数に変換する
UINT Se6SubnetMaskToInt(SE_IPV6_ADDR a)
{
	UINT i;

	for (i = 0;i <= 128;i++)
	{
		if (Se6Cmp(a, Se6GenerateSubnetMask(i)) == 0)
		{
			return i;
		}
	}

	return 0;
}

// 指定した IP アドレスがサブネットマスクかどうか調べる
bool Se6IsSubnetMask(SE_IPV6_ADDR a)
{
	UINT i;

	for (i = 0;i <= 128;i++)
	{
		if (Se6Cmp(a, Se6GenerateSubnetMask(i)) == 0)
		{
			return true;
		}
	}

	return false;
}

// MAC アドレスからグローバルアドレスを生成する
SE_IPV6_ADDR Se6GenerateEui64GlobalAddress(SE_IPV6_ADDR prefix, SE_IPV6_ADDR subnet, UCHAR *mac)
{
	SE_IPV6_ADDR a;
	UCHAR tmp[8];
	// 引数チェック
	if (mac == NULL)
	{
		return Se6ZeroIP();
	}

	Se6GenerateEui64Address(tmp, mac);

	SeZero(&a, sizeof(a));
	SeCopy(&a.Value[8], tmp, 8);

	return Se6Or(Se6And(a, Se6Not(subnet)), Se6And(prefix, subnet));
}

// MAC アドレスからローカルアドレスを生成する
SE_IPV6_ADDR Se6GenerateEui64LocalAddress(UCHAR *mac)
{
	SE_IPV6_ADDR a;
	UCHAR tmp[8];
	// 引数チェック
	if (mac == NULL)
	{
		return Se6ZeroIP();
	}

	Se6GenerateEui64Address(tmp, mac);

	SeZero(&a, sizeof(a));
	a.Value[0] = 0xfe;
	a.Value[1] = 0x80;
	SeCopy(&a.Value[8], tmp, 8);

	return a;
}

// MAC アドレスから EUI-64 アドレスを生成する
void Se6GenerateEui64Address(UCHAR *dst, UCHAR *mac)
{
	// 引数チェック
	if (dst == NULL || mac == NULL)
	{
		return;
	}

	SeCopy(dst, mac, 3);
	SeCopy(dst + 5, mac, 3);

	dst[3] = 0xff;
	dst[4] = 0xfe;
	dst[0] = ((~(dst[0] & 0x02)) & 0x02) | (dst[0] & 0xfd);
}

// 同一のネットワークかどうか調べる
bool Se6IsInSameNetwork(SE_IPV6_ADDR a1, SE_IPV6_ADDR a2, SE_IPV6_ADDR subnet)
{
	if (Se6Cmp(Se6GetPrefixAddress(a1, subnet), Se6GetPrefixAddress(a2, subnet)) == 0)
	{
		return true;
	}

	return false;
}

// ネットワークプレフィックスアドレスかどうか検査する
bool Se6IsNetworkPrefixAddress(SE_IPV6_ADDR a, SE_IPV6_ADDR subnet)
{
	if (Se6IsZeroIP(Se6GetHostAddress(a, subnet)))
	{
		return true;
	}

	return false;
}

// ユニキャストアドレスが有効かどうかチェックする
bool Se6CheckUnicastAddress(SE_IPV6_ADDR a)
{
	if (Se6IsZeroIP(a))
	{
		return false;
	}

	if ((Se6GetIPAddrType(a) & SE_IPV6_ADDR_UNICAST) == 0)
	{
		return false;
	}

	return true;
}

// ホストアドレスの取得
SE_IPV6_ADDR Se6GetHostAddress(SE_IPV6_ADDR a, SE_IPV6_ADDR subnet)
{
	return Se6And(a, Se6Not(subnet));
}

// プレフィックスアドレスの取得
SE_IPV6_ADDR Se6GetPrefixAddress(SE_IPV6_ADDR a, SE_IPV6_ADDR subnet)
{
	return Se6And(a, subnet);
}

// 要請ノードマルチキャストアドレスを取得
SE_IPV6_ADDR Se6GetSoliciationMulticastAddr(SE_IPV6_ADDR a)
{
	SE_IPV6_ADDR prefix;

	SeZero(&prefix, sizeof(prefix));
	prefix.Value[0] = 0xff;
	prefix.Value[1] = 0x02;
	prefix.Value[11] = 0x01;
	prefix.Value[12] = 0xff;

	return Se6Or(Se6And(prefix, Se6GenerateSubnetMask(104)), Se6And(a, Se6Not(Se6GenerateSubnetMask(104))));
}

// マルチキャストアドレスに対応した MAC アドレスの生成
void Se6GenerateMulticastMacAddress(UCHAR *mac, SE_IPV6_ADDR a)
{
	// 引数チェック
	if (mac == NULL)
	{
		return;
	}

	mac[0] = 0x33;
	mac[1] = 0x33;
	mac[2] = a.Value[12];
	mac[3] = a.Value[13];
	mac[4] = a.Value[14];
	mac[5] = a.Value[15];
}

// IP アドレスのタイプの取得
UINT Se6GetIPAddrType(SE_IPV6_ADDR a)
{
	UINT ret = 0;

	if (a.Value[0] == 0xff)
	{
		ret |= SE_IPV6_ADDR_MULTICAST;

		if (Se6Cmp(a, Se6GetAllNodeMulticastAddr()) == 0)
		{
			ret |= SE_IPV6_ADDR_ALL_NODE_MULTICAST;
		}
		else if (Se6Cmp(a, Se6GetAllRouterMulticastAddr()) == 0)
		{
			ret |= SE_IPV6_ADDR_ALL_ROUTER_MULTICAST;
		}
		else
		{
			if (a.Value[1] == 0x02 && a.Value[2] == 0 && a.Value[3] == 0 &&
				a.Value[4] == 0 && a.Value[5] == 0 && a.Value[6] == 0 &&
				a.Value[7] == 0 && a.Value[8] == 0 && a.Value[9] == 0 &&
				a.Value[10] == 0 && a.Value[11] == 0x01 && a.Value[12] == 0xff)
			{
				ret |= SE_IPV6_ADDR_SOLICIATION_MULTICAST;
			}
		}
	}
	else
	{
		ret |= SE_IPV6_ADDR_UNICAST;

		if (a.Value[0] == 0xfe && (a.Value[1] & 0xc0) == 0x80)
		{
			ret |= SE_IPV6_ADDR_LOCAL_UNICAST;
		}
		else
		{
			ret |= SE_IPV6_ADDR_GLOBAL_UNICAST;
		}
	}

	return ret;
}

// 全ノードマルチキャストアドレスを取得する
SE_IPV6_ADDR Se6GetAllNodeMulticastAddr()
{
	SE_IPV6_ADDR a;

	SeZero(&a, sizeof(a));

	a.Value[0] = 0xff;
	a.Value[1] = 0x02;
	a.Value[15] = 0x01;

	return a;
}

// 全ルータマルチキャストアドレスを取得する
SE_IPV6_ADDR Se6GetAllRouterMulticastAddr()
{
	SE_IPV6_ADDR a;

	SeZero(&a, sizeof(a));

	a.Value[0] = 0xff;
	a.Value[1] = 0x02;
	a.Value[15] = 0x02;

	return a;
}

// IP アドレスの論理演算
SE_IPV6_ADDR Se6And(SE_IPV6_ADDR a, SE_IPV6_ADDR b)
{
	UINT i;
	SE_IPV6_ADDR ret;

	for (i = 0;i < 16;i++)
	{
		ret.Value[i] = a.Value[i] & b.Value[i];
	}

	return ret;
}
SE_IPV6_ADDR Se6Or(SE_IPV6_ADDR a, SE_IPV6_ADDR b)
{
	UINT i;
	SE_IPV6_ADDR ret;

	for (i = 0;i < 16;i++)
	{
		ret.Value[i] = a.Value[i] | b.Value[i];
	}

	return ret;
}
SE_IPV6_ADDR Se6Not(SE_IPV6_ADDR a)
{
	UINT i;
	SE_IPV6_ADDR ret;

	for (i = 0;i < 16;i++)
	{
		ret.Value[i] = ~a.Value[i];
	}

	return ret;
}

// サブネットマスクの作成
SE_IPV6_ADDR Se6GenerateSubnetMask(UINT i)
{
	UINT j = i / 8;
	UINT k = i % 8;
	UINT z;
	SE_IPV6_ADDR a;

	SeZero(&a, sizeof(a));

	for (z = 0;z < 16;z++)
	{
		if (z < j)
		{
			a.Value[z] = 0xff;
		}
		else if (z == j)
		{
			a.Value[z] = ~(0xff >> k);
		}
	}

	return a;
}

// IP アドレスの比較
int Se6Cmp(SE_IPV6_ADDR a, SE_IPV6_ADDR b)
{
	return SeCmp(&a, &b, sizeof(SE_IPV6_ADDR));
}

// ゼロかどうかチェック
bool Se6IsZeroIP(SE_IPV6_ADDR a)
{
	return SeIsZero(a.Value, sizeof(a.Value));
}

// IP アドレスを文字列に変換
void Se6IPToStr(char *str, SE_IPV6_ADDR a)
{
	UINT i;
	USHORT values[8];
	UINT zero_started_index;
	UINT max_zero_len;
	UINT max_zero_start;
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	for (i = 0;i < 8;i++)
	{
		SeCopy(&values[i], &a.Value[i * 2], sizeof(USHORT));
		values[i] = SeEndian16(values[i]);
	}

	// 省略できる場所があるかどうか検索
	zero_started_index = INFINITE;
	max_zero_len = 0;
	max_zero_start = INFINITE;
	for (i = 0;i < 9;i++)
	{
		USHORT v = (i != 8 ? values[i] : 1);

		if (values[i] == 0)
		{
			if (zero_started_index == INFINITE)
			{
				zero_started_index = i;
			}
		}
		else
		{
			UINT zero_len;

			if (zero_started_index != INFINITE)
			{
				zero_len = i - zero_started_index;
				if (zero_len >= 2)
				{
					if (max_zero_len < zero_len)
					{
						max_zero_start = zero_started_index;
						max_zero_len = zero_len;
					}
				}

				zero_started_index = INFINITE;
			}
		}
	}

	// 文字列を形成
	SeStrCpy(str, 0, "");
	for (i = 0;i < 8;i++)
	{
		char tmp[16];

		SeToHex(tmp, values[i]);
		SeStrLower(tmp);

		if (i == max_zero_start)
		{
			if (i == 0)
			{
				SeStrCat(str, 0, "::");
			}
			else
			{
				SeStrCat(str, 0, ":");
			}
			i += max_zero_len - 1;
		}
		else
		{
			SeStrCat(str, 0, tmp);
			if (i != 7)
			{
				SeStrCat(str, 0, ":");
			}
		}
	}
}

// 文字列を IP アドレスに変換
SE_IPV6_ADDR Se6StrToIP(char *str)
{
	SE_TOKEN_LIST *t;
	char tmp[MAX_PATH];
	SE_IPV6_ADDR a;
	// 引数チェック
	if (str == NULL)
	{
		return Se6ZeroIP();
	}

	a = Se6ZeroIP();

	SeStrCpy(tmp, sizeof(tmp), str);
	SeTrim(tmp);

	// トークン分割
	t = SeParseTokenWithNullStr(tmp, ":");
	if (t->NumTokens >= 3 && t->NumTokens <= 8)
	{
		UINT i, n;
		bool b = true;
		UINT k = 0;

		n = 0;

		for (i = 0;i < t->NumTokens;i++)
		{
			char *str = t->Token[i];

			if (i != 0 && i != (t->NumTokens - 1) && SeStrLen(str) == 0)
			{
				n++;
				if (n == 1)
				{
					k += 2 * (8 - t->NumTokens + 1);
				}
				else
				{
					b = false;
					break;
				}
			}
			else
			{
				UCHAR chars[2];

				if (Se6CheckIPItemStr(str) == false)
				{
					b = false;
					break;
				}

				Se6IPItemStrToChars(chars, str);

				a.Value[k++] = chars[0];
				a.Value[k++] = chars[1];
			}
		}

		if (n != 0 && n != 1)
		{
			b = false;
		}
		else if (n == 0 && t->NumTokens != 8)
		{
			b = false;
		}

		if (b == false)
		{
			a = Se6ZeroIP();
		}
	}

	SeFreeToken(t);

	return a;
}

// IP アドレスの文字から UCHAR 型に変換
void Se6IPItemStrToChars(UCHAR *chars, char *str)
{
	char tmp[5];
	SE_BUF *b;
	UINT len;
	// 引数チェック
	if (chars == NULL)
	{
		return;
	}

	SeZero(tmp, sizeof(tmp));

	len = SeStrLen(str);
	switch (len)
	{
	case 0:
		tmp[0] = tmp[1] = tmp[2] = tmp[3] = '0';
		break;

	case 1:
		tmp[0] = tmp[1] = tmp[2] = '0';
		tmp[3] = str[0];
		break;

	case 2:
		tmp[0] = tmp[1] = '0';
		tmp[2] = str[0];
		tmp[3] = str[1];
		break;

	case 3:
		tmp[0] = '0';
		tmp[1] = str[0];
		tmp[2] = str[1];
		tmp[3] = str[2];
		break;

	case 4:
		tmp[0] = str[0];
		tmp[1] = str[1];
		tmp[2] = str[2];
		tmp[3] = str[3];
		break;
	}

	b = SeStrToBin(tmp);

	chars[0] = ((UCHAR *)b->Buf)[0];
	chars[1] = ((UCHAR *)b->Buf)[1];

	SeFreeBuf(b);
}

// IP アドレスの要素文字列の中に不正な文字が含まれていないかどうかチェックする
bool Se6CheckIPItemStr(char *str)
{
	UINT i, len;
	// 引数チェック
	if (str == NULL)
	{
		return false;
	}

	len = SeStrLen(str);
	if (len >= 5)
	{
		// 長さ不正
		return false;
	}

	for (i = 0;i < len;i++)
	{
		char c = str[i];

		if ((c >= 'a' && c <= 'f') ||
			(c >= 'A' && c <= 'F') ||
			(c >= '0' && c <= '9'))
		{
		}
		else
		{
			return false;
		}
	}

	return true;
}

// 0
SE_IPV6_ADDR Se6ZeroIP()
{
	SE_IPV6_ADDR a;

	SeZero(&a, sizeof(SE_IPV6_ADDR));

	return a;
}

// サブネット長の検査
bool Se6CheckSubnetLength(UINT i)
{
	if (i >= 1 && i <= 127)
	{
		return true;
	}

	return false;
}

