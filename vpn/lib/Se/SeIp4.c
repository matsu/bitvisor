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

// SeIp4.c
// 概要: IPv4 プロトコルスタック

#define SE_INTERNAL
#include <Se/Se.h>

// TCP の MSS の値を調整する
bool Se4AdjustTcpMss(void *src_data, UINT src_size, UINT mss)
{
	UCHAR *p;
	UINT p_size;
	SE_IPV4_HEADER *ip_header;
	UINT ip_header_size;
	SE_TCP_HEADER *tcp_header;
	UINT tcp_header_size;
	UINT tcp_header_and_payload_size;
	UCHAR *options;
	UINT options_size;
	// 引数チェック
	if (src_data == NULL)
	{
		return false;
	}

	p = (UCHAR *)src_data;
	p_size = src_size;

	// IP ヘッダ解析
	if (p_size < sizeof(SE_IPV4_HEADER))
	{
		return false;
	}
	ip_header = (SE_IPV4_HEADER *)p;

	if (SE_IPV4_GET_VERSION(ip_header) != 4)
	{
		// IPv4 でない
		return false;
	}

	if (ip_header->Protocol != SE_IP_PROTO_TCP)
	{
		// TCP でない
		return false;
	}

	ip_header_size = SE_IPV4_GET_HEADER_LEN(ip_header) * 4;
	if (p_size < ip_header_size)
	{
		return false;
	}

	p_size -= ip_header_size;
	p += ip_header_size;

	// TCP ヘッダ解析
	if (p_size < sizeof(SE_TCP_HEADER))
	{
		return false;
	}
	tcp_header = (SE_TCP_HEADER *)p;
	tcp_header_size = SE_TCP_GET_HEADER_SIZE(tcp_header) * 4;
	if (tcp_header_size < sizeof(SE_TCP_HEADER))
	{
		return false;
	}

	if (((tcp_header->Flag & SE_TCP_SYN) == false) ||
		((tcp_header->Flag & SE_TCP_RST) ||
		(tcp_header->Flag & SE_TCP_PSH) ||
		(tcp_header->Flag & SE_TCP_ACK) ||
		(tcp_header->Flag & SE_TCP_URG)))
	{
		// SYN パケットでない
		return false;
	}

	if (p_size < tcp_header_size)
	{
		return false;
	}

	tcp_header_and_payload_size = p_size;

	// オプションフィールドを取得
	options = ((UCHAR *)tcp_header) + sizeof(SE_TCP_HEADER);
	options_size = tcp_header_size - sizeof(SE_TCP_HEADER);

	if (options_size >= 4 && options[0] == 0x02 && options[1] == 0x04)
	{
		// TCP の MSS オプションが付加されている
		USHORT current_mss;

		SeCopy(&current_mss, options + 2, sizeof(USHORT));

		current_mss = SeEndian16(current_mss);

		if (current_mss <= mss)
		{
			// MSS の値が指定サイズよりも最初から小さい場合は
			// 書き換える必要がない
			return false;
		}

		// MSS の値の書き換え
		current_mss = SeEndian16((USHORT)mss);

		SeCopy(options + 2, &current_mss, sizeof(USHORT));
	}
	else
	{
		// TCP の MSS オプションが付加されていない
		return false;
	}

	if (true)
	{
		// TCP のチェックサムの再計算
		SE_TCPV4_PSEUDO_HEADER *vh;

		// TCP ヘッダのチェックサムをゼロクリア
		tcp_header->Checksum = 0;

		// 擬似ヘッダ付き TCP の作成
		vh = SeZeroMalloc(sizeof(SE_TCPV4_PSEUDO_HEADER) + tcp_header_and_payload_size);
		vh->SrcIP = ip_header->SrcIP;
		vh->DstIP = ip_header->DstIP;
		vh->Protocol = ip_header->Protocol;
		vh->PacketLength = SeEndian16(tcp_header_and_payload_size);

		SeCopy(((UCHAR *)vh) + sizeof(SE_TCPV4_PSEUDO_HEADER),
			tcp_header, tcp_header_and_payload_size);

		// チェックサム計算
		tcp_header->Checksum = Se4IpChecksum(vh, sizeof(SE_TCPV4_PSEUDO_HEADER) + tcp_header_and_payload_size);

		SeFree(vh);
	}

	return true;
}

// メインプロシージャ
void Se4MainProc(SE_IPV4 *p)
{
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	// ARP 待機リストの実行
	Se4ProcessArpWaitList(p);

	// 古くなった IP 結合リストの削除
	Se4FlushIpCombineList(p, p->IpCombineList);

	// 古くなった IP 待機リストの削除
	Se4FlushIpWaitList(p);
}

// ICMP Echo パケットのビルド
SE_BUF *Se4BuildIcmpEchoPacket(UCHAR type, UCHAR code, USHORT id, USHORT seq_no, void *data, UINT size)
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

	icmp->Checksum = Se4IpChecksum(icmp, sizeof(SE_ICMP_HEADER) + sizeof(SE_ICMP_ECHO) + size);

	ret = SeMemToBuf(icmp, sizeof(SE_ICMP_HEADER) + sizeof(SE_ICMP_ECHO) + size);

	SeFree(icmp);

	return ret;
}

// ICMP Echo Request パケットの送信
void Se4SendIcmpEchoRequest(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, USHORT seq_no, void *data, UINT size)
{
	SE_BUF *buf;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}

	buf = Se4BuildIcmpEchoPacket(SE_ICMPV4_TYPE_ECHO_REQUEST, 0, id, seq_no, data, size);

	Se4SendIp(p, dest_ip, src_ip, SE_IP_PROTO_ICMPV4, 0,
		buf->Buf, buf->Size, NULL);

	SeFreeBuf(buf);
}

// ICMP Echo Response パケットの送信
void Se4SendIcmpEchoResponse(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, USHORT seq_no, void *data, UINT size)
{
	SE_BUF *buf;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}

	buf = Se4BuildIcmpEchoPacket(SE_ICMPV4_TYPE_ECHO_RESPONSE, 0, id, seq_no, data, size);

	Se4SendIp(p, dest_ip, src_ip, SE_IP_PROTO_ICMPV4, 0,
		buf->Buf, buf->Size, NULL);

	SeFreeBuf(buf);
}

// ICMP パケットの送信
void Se4SendIcmp(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, UCHAR type, UCHAR code, void *data, UINT size)
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
	icmp->Checksum = Se4IpChecksum(icmp, sizeof(SE_ICMP_HEADER) + size);

	Se4SendIp(p, dest_ip, src_ip, SE_IP_PROTO_ICMPV4, 0, icmp, sizeof(SE_ICMP_HEADER) + size, NULL);

	SeFree(icmp);
}

// UDP パケットの送信
void Se4SendUdp(SE_IPV4 *p, SE_IPV4_ADDR dest_ip, UINT dest_port, SE_IPV4_ADDR src_ip, UINT src_port, void *data, UINT size, UCHAR *dest_mac)
{
	SE_UDPV4_PSEUDO_HEADER *vh;
	SE_UDP_HEADER *udp;
	UINT udp_packet_length = sizeof(SE_UDP_HEADER) + size;
	USHORT checksum;
	// 引数チェック
	if (p == NULL || data == NULL)
	{
		return;
	}
	if (udp_packet_length > SE_IP4_MAX_PAYLOAD_SIZE)
	{
		return;
	}

	// 仮想ヘッダの作成
	vh = SeZeroMalloc(sizeof(SE_UDPV4_PSEUDO_HEADER) + size);
	udp = (SE_UDP_HEADER *)(((UCHAR *)vh) + 12);

	vh->SrcIP = Se4IPToUINT(src_ip);
	vh->DstIP = Se4IPToUINT(dest_ip);
	vh->Protocol = SE_IP_PROTO_UDP;
	vh->PacketLength1 = SeEndian16((USHORT)udp_packet_length);

	udp->SrcPort = SeEndian16((USHORT)src_port);
	udp->DstPort = SeEndian16((USHORT)dest_port);
	udp->PacketLength = SeEndian16((USHORT)udp_packet_length);

	// データコピー
	SeCopy(((UCHAR *)udp) + sizeof(SE_UDP_HEADER), data, size);

	// チェックサム計算
	checksum = Se4IpChecksum(vh, udp_packet_length + 12);
	if (checksum == 0x0000)
	{
		checksum = 0xffff;
	}

	udp->Checksum = checksum;

	// パケット送信
	Se4SendIp(p, dest_ip, src_ip, SE_IP_PROTO_UDP, 0, udp, udp_packet_length, dest_mac);

	SeFree(vh);
}

// IP フラグメントパケットの送信処理の実行
void Se4SendIpFragmentNow(SE_IPV4 *p, UCHAR *dest_mac, void *data, UINT size)
{
	// 引数チェック
	if (p == NULL || dest_mac == NULL || data == NULL || size == 0)
	{
		return;
	}

	// 送信
	Se4SendEthPacket(p, dest_mac, SE_MAC_PROTO_IPV4, data, size);
}

// Raw IP パケットの送信
void Se4SendRawIp(SE_IPV4 *p, void *data, UINT size, UCHAR *dest_mac)
{
	UCHAR *buf;
	SE_IPV4_HEADER *ip;
	SE_IPV4_ADDR dest_ip_local, dest_ip, src_ip;
	// 引数チェック
	if (p == NULL || data == NULL || size == 0)
	{
		return;
	}

	if (size <= sizeof(SE_IPV4_HEADER))
	{
		return;
	}

	buf = (UCHAR *)data;

	// IP ヘッダ解釈
	ip = (SE_IPV4_HEADER *)data;

	dest_ip_local = dest_ip = Se4UINTToIP(ip->DstIP);
	src_ip = Se4UINTToIP(ip->SrcIP);

	// MAC アドレスの解決
	if (dest_mac == NULL)
	{
		if (Se4IsBroadcastAddress(dest_ip) ||
			Se4Cmp(Se4GetBroadcastAddress(p->IpAddress, p->SubnetMask), dest_ip) == 0)
		{
			// 宛先 IP アドレスはブロードキャストアドレス
			dest_mac = Se4BroadcastMacAddress();
		}
		else
		{
			SE_ARPV4_ENTRY *e;

			if (Se4IsInSameNetwork(p->IpAddress, dest_ip, p->SubnetMask) == false)
			{
				if (p->UseDefaultGateway)
				{
					// ルーティングが必要である。ルータの IP アドレスを解決する
					dest_ip_local = p->DefaultGateway;
				}
				else
				{
					// ルーティングが必要であるがデフォルトゲートウェイが存在しない
					// のでパケットを破棄する
					SeFree(buf);
					return;
				}
			}

			// ARP テーブルを検索
			e = Se4SearchArpEntryList(p->ArpEntryList, dest_ip_local, Se4Tick(p),
				p->Vpn->Config->OptionV4ArpExpires,
				p->Vpn->Config->OptionV4ArpDontUpdateExpires);

			if (e != NULL)
			{
				dest_mac = e->MacAddress;
			}
		}
	}

	if (dest_mac != NULL)
	{
		// パケットの送信処理を実行
		Se4SendIpFragmentNow(p, dest_mac, buf, size);

		// バッファ解放
		SeFree(buf);
	}
	else
	{
		// IP 送信待ちテーブルに格納
		Se4InsertIpWait(p->IpWaitList, Se4Tick(p), dest_ip_local, src_ip, buf, size);

		// ARP の送信
		Se4SendArpRequest(p, dest_ip_local);
	}
}

// IP フラグメントパケットの送信
void Se4SendIpFragment(SE_IPV4 *p, SE_IPV4_ADDR dest_ip, SE_IPV4_ADDR src_ip,
					   USHORT id, USHORT total_size, USHORT offset, UCHAR protocol, UCHAR ttl,
					   void *data, UINT size, UCHAR *dest_mac)
{
	UCHAR *buf;
	SE_IPV4_HEADER *ip;
	// 引数チェック
	if (p == NULL || data == NULL || size == 0)
	{
		return;
	}

	// メモリ確保
	buf = SeZeroMalloc(size + sizeof(SE_IPV4_HEADER));
	ip = (SE_IPV4_HEADER *)buf;

	// IP ヘッダの構築
	SE_IPV4_SET_VERSION(ip, 4);
	SE_IPV4_SET_HEADER_LEN(ip, (sizeof(SE_IPV4_HEADER) / 4));
	ip->TotalLength = SeEndian16((USHORT)(size + sizeof(SE_IPV4_HEADER)));
	ip->Identification = SeEndian16(id);
	SE_IPV4_SET_OFFSET(ip, (offset / 8));
	if ((offset + size) >= total_size)
	{
		SE_IPV4_SET_FLAGS(ip, 0x00);
	}
	else
	{
		SE_IPV4_SET_FLAGS(ip, 0x01);
	}
	ip->TimeToLive = (ttl == 0 ? SE_IPV4_SEND_TTL : ttl);
	ip->Protocol = protocol;
	ip->SrcIP = Se4IPToUINT(src_ip);
	ip->DstIP = Se4IPToUINT(dest_ip);

	// チェックサムの計算
	ip->Checksum = Se4IpChecksum(ip, sizeof(SE_IPV4_HEADER));

	// データコピー
	SeCopy(buf + sizeof(SE_IPV4_HEADER), data, size);

	// Raw IP 送信
	Se4SendRawIp(p, buf, size + sizeof(SE_IPV4_HEADER), dest_mac);
}

// IP パケットの送信
void Se4SendIp(SE_IPV4 *p, SE_IPV4_ADDR dest_ip, SE_IPV4_ADDR src_ip, UCHAR protocol, UCHAR ttl, void *data, UINT size, UCHAR *dest_mac)
{
	UINT mss;
	UCHAR *buf;
	USHORT offset;
	USHORT id;
	USHORT total_size;
	UINT size_of_this_packet;
	// 引数チェック
	if (p == NULL || data == NULL || size > SE_IP4_MAX_PAYLOAD_SIZE)
	{
		return;
	}

	// MSS の計算
	mss = p->Mtu - sizeof(SE_IPV4_HEADER);

	// バッファ
	buf = (UCHAR *)data;

	// ID
	id = p->IdSeed++;

	// 合計サイズ
	total_size = (USHORT)size;

	// 分割
	offset = 0;

	while (true)
	{
		bool last_packet = false;
		// このパケットのサイズを取得
		size_of_this_packet = MIN((USHORT)mss, (total_size - offset));
		if ((offset + (USHORT)size_of_this_packet) == total_size)
		{
			last_packet = true;
		}

		// 分割されたパケットの送信処理
		Se4SendIpFragment(p, dest_ip, src_ip, id, total_size, offset, protocol, ttl,
			buf + offset, size_of_this_packet, dest_mac);

		if (last_packet)
		{
			break;
		}

		offset += (USHORT)size_of_this_packet;
	}
}

// UDP パケットの解析
bool Se4ParseIpPacketUDPv4(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size)
{
	SE_UDPV4_HEADER_INFO udp_info;
	SE_UDP_HEADER *udp;
	UINT packet_length;
	void *buf;
	UINT buf_size;
	USHORT src_port, dest_port;
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
	if (packet_length > size)
	{
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

	info->TypeL4 = SE_L4_UDPV4;
	info->UDPv4Info = SeClone(&udp_info, sizeof(udp_info));

	return true;
}

// ICMP パケットの解析
bool Se4ParseIpPacketICMPv4(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, void *data, UINT size)
{
	SE_ICMPV4_HEADER_INFO icmp_info;
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
	checksum_calc = Se4IpChecksum(data, size);
	icmp->Checksum = checksum_original;

	if (checksum_calc != checksum_original)
	{
		// チェックサムが一致しない
		return false;
	}

	icmp_info.Type = icmp->Type;
	icmp_info.Code = icmp->Code;
	icmp_info.Data = ((UCHAR *)data) + sizeof(SE_ICMP_HEADER);
	icmp_info.DataSize = msg_size;

	switch (icmp_info.Type)
	{
	case SE_ICMPV4_TYPE_ECHO_REQUEST:
	case SE_ICMPV4_TYPE_ECHO_RESPONSE:
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
	}

	info->TypeL4 = SE_L4_ICMPV4;
	info->ICMPv4Info = SeClone(&icmp_info, sizeof(icmp_info));

	return true;
}

// IP パケットの解析
bool Se4ParseIpPacket(SE_IPV4 *p, SE_IPV4_HEADER_INFO *info, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, UCHAR procotol, UCHAR ttl, void *data, UINT size, bool is_broadcast)
{
	// 引数チェック
	if (p == NULL || info == NULL || data == NULL || size == 0)
	{
		return false;
	}

	SeZero(info, sizeof(SE_IPV4_HEADER_INFO));

	info->Size = size;
	info->Id = id;
	info->Protocol = procotol;
	info->SrcIpAddress = src_ip;
	info->DestIpAddress = dest_ip;
	info->IsBroadcast = is_broadcast;
	info->Ttl = ttl;
	info->UnicastForMe = (Se4Cmp(dest_ip, p->IpAddress) == 0 ? true : false);
	if (info->IsBroadcast == false && info->UnicastForMe == false)
	{
		info->UnicastForRouting = true;

		if (Se4IsInSameNetwork(dest_ip, p->IpAddress, p->SubnetMask))
		{
			info->UnicastForRoutingWithProxyArp = true;
		}
	}

	switch (info->Protocol)
	{
	case SE_IP_PROTO_ICMPV4:	// ICMPv4
		if (Se4ParseIpPacketICMPv4(p, info, data, size) == false)
		{
			return false;
		}
		break;

	case SE_IP_PROTO_UDP:		// UDPv4
		if (Se4ParseIpPacketUDPv4(p, info, data, size) == false)
		{
			return false;
		}
		break;

	case SE_IP_PROTO_ESP:		// ESPv4
		info->TypeL4 = SE_L4_ESPV4;
		break;
	}

	return true;
}

// 完全な IP パケットの受信
void Se4RecvIpComplete(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip, USHORT id, UCHAR procotol, UCHAR ttl, void *data, UINT size, bool is_broadcast)
{
	SE_IPV4_HEADER_INFO info;
	// 引数チェック
	if (p == NULL || data == NULL || size == 0)
	{
		return;
	}

	if (Se4ParseIpPacket(p, &info, src_ip, dest_ip, id, procotol, ttl, data, size, is_broadcast))
	{
		if (info.UnicastForMe)
		{
			if (info.TypeL4 == SE_L4_ICMPV4)
			{
				if (info.ICMPv4Info->Type == SE_ICMPV4_TYPE_ECHO_REQUEST)
				{
					// 自分自身を宛先とした ICMP Echo Request パケットが届いたので返信する
					Se4SendIcmpEchoResponse(p, p->IpAddress, info.SrcIpAddress,
						info.ICMPv4Info->EchoHeader.Identifier,
						info.ICMPv4Info->EchoHeader.SeqNo,
						info.ICMPv4Info->EchoData,
						info.ICMPv4Info->EchoDataSize);
				}
			}
		}

		p->RecvCallback(p, &info, data, size, p->RecvCallbackParam);

		Se4FreeIpHeaderInfo(&info);
	}
}

// IP ヘッダ情報構造体の使用したメモリの解放
void Se4FreeIpHeaderInfo(SE_IPV4_HEADER_INFO *info)
{
	// 引数チェック
	if (info == NULL)
	{
		return;
	}

	if (info->ICMPv4Info != NULL)
	{
		SeFree(info->ICMPv4Info);
	}

	if (info->UDPv4Info != NULL)
	{
		SeFree(info->UDPv4Info);
	}
}

// IP パケットの受信
void Se4RecvIp(SE_IPV4 *p, SE_PACKET *pkt)
{
	SE_IPV4_HEADER *ip;
	UINT ipv4_header_size;
	void *data;
	UINT size, data_size_recved;
	// 引数チェック
	if (p == NULL || pkt == NULL)
	{
		return;
	}

	ip = pkt->L3.IPv4Header;

	// IPv4 ヘッダのサイズを取得する
	ipv4_header_size = SE_IPV4_GET_HEADER_LEN(ip) * 4;

	// チェックサム計算
	if (Se4IpCheckChecksum(ip) == false)
	{
		return;
	}

	// データへのポインタを取得
	data = ((UCHAR *)pkt->L3.PointerL3) + ipv4_header_size;

	// IP アドレスと MAC アドレスの関連付け
	Se4MacIpRelationKnown(p, Se4UINTToIP(ip->SrcIP), pkt->MacHeader->SrcAddress);

	// データサイズ取得
	size = SeEndian16(ip->TotalLength);
	if (size <= ipv4_header_size)
	{
		// データ無し (パケットが短すぎる)
		return;
	}

	size -= ipv4_header_size;
	data_size_recved = pkt->PacketSize - (ipv4_header_size + sizeof(SE_MAC_HEADER));
	if (data_size_recved < size)
	{
		// データ無し (パケットが短すぎる)
		return;
	}

	if (p->UseRawIp)
	{
		// Raw パケットを受信処理する
		SE_IPV4_HEADER_INFO info;

		SeZero(&info, sizeof(info));
		info.IsRawIpPacket = true;
		info.SrcIpAddress = Se4UINTToIP(ip->SrcIP);
		info.DestIpAddress = Se4UINTToIP(ip->DstIP);

		p->RecvCallback(p, &info, ip, pkt->PacketSize - sizeof(SE_MAC_HEADER), p->RecvCallbackParam);
	}

	if (SE_IPV4_GET_OFFSET(ip) == 0 && (SE_IPV4_GET_FLAGS(ip) & 0x01) == 0)
	{
		// 分割されていない IP パケットを受信
		Se4RecvIpComplete(p, Se4UINTToIP(ip->SrcIP), Se4UINTToIP(ip->DstIP),
			SeEndian16(ip->Identification), ip->Protocol, ip->TimeToLive, data, size, pkt->IsBroadcast);
	}
	else
	{
		// 分割された IP パケットを受信
		SE_IPV4_COMBINE *c;
		UINT offset;
		bool is_last_packet;

		offset = SE_IPV4_GET_OFFSET(ip) * 8;
		c = Se4SearchIpCombineList(p->IpCombineList, Se4UINTToIP(ip->DstIP), Se4UINTToIP(ip->SrcIP),
			SeEndian16(ip->Identification), ip->Protocol);
		is_last_packet = ((SE_IPV4_GET_FLAGS(ip) & 0x01) == 0 ? true : false);

		if (c == NULL)
		{
			// 最初のパケット
			c = Se4InsertIpCombine(p,
				Se4UINTToIP(ip->SrcIP), Se4UINTToIP(ip->DstIP), SeEndian16(ip->Identification), ip->Protocol, ip->TimeToLive,
				pkt->IsBroadcast);

			if (c != NULL)
			{
				Se4CombineIp(p, c, offset, data, size, is_last_packet);
			}
		}
		else
		{
			// 2 個目以降のパケット
			Se4CombineIp(p, c, offset, data, size, is_last_packet);
		}
	}
}

// IP ヘッダのチェックサムを確認する
bool Se4IpCheckChecksum(SE_IPV4_HEADER *ip)
{
	UINT header_size;
	USHORT checksum_original, checksum_calc;
	// 引数チェック
	if (ip == NULL)
	{
		return false;
	}

	header_size = SE_IPV4_GET_HEADER_LEN(ip) * 4;
	checksum_original = ip->Checksum;
	ip->Checksum = 0;
	checksum_calc = Se4IpChecksum(ip, header_size);
	ip->Checksum = checksum_original;

	if (checksum_original == checksum_calc)
	{
		return true;
	}
	else
	{
		return false;
	}
}

// チェックサムを計算する
USHORT Se4IpChecksum(void *buf, UINT size)
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
void Se4MacIpRelationKnown(SE_IPV4 *p, SE_IPV4_ADDR ip_addr, UCHAR *mac_addr)
{
	SE_ARPV4_WAIT *w;
	// 引数チェック
	if (p == NULL || mac_addr == NULL)
	{
		return;
	}

	// 自分の所属している IP ネットワークのアドレスかどうか判定
	if (Se4IsInSameNetwork(p->IpAddress, ip_addr, p->SubnetMask) == false)
	{
		// 異なる IP ネットワークのアドレス (ルーティングされてきたアドレス)
		// は ARP テーブルに登録しない
		return;
	}

	// ARP 待機リストを検索
	w = Se4SearchArpWaitList(p->ArpWaitList, ip_addr);
	if (w != NULL)
	{
		// ARP 待機リストを削除
		SeDelete(p->ArpWaitList, w);

		SeFree(w);
	}

	// ARP テーブルに登録
	Se4AddArpEntryList(p->ArpEntryList, Se4Tick(p), p->Vpn->Config->OptionV4ArpExpires,
		ip_addr, mac_addr);

	// IP 待機リストで待機している IP パケットがあればすべて送信する
	Se4SendWaitingIpWait(p, ip_addr, mac_addr);
}

// 現在の Tick 値を取得する
UINT64 Se4Tick(SE_IPV4 *p)
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

// ARP リクエストの送信
void Se4SendArpRequest(SE_IPV4 *p, SE_IPV4_ADDR ip)
{
	SE_ARPV4_WAIT *w;
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	w = Se4SearchArpWaitList(p->ArpWaitList, ip);
	if (w != NULL)
	{
		w->SendCounter = 0;
	}
	else
	{
		w = SeZeroMalloc(sizeof(SE_ARPV4_WAIT));

		w->IpAddress = ip;
		w->SendCounter = 0;

		SeInsert(p->ArpWaitList, w);
	}

	Se4SendArpDoProcess(p, w);
}

// ARP 待機リストの実行
void Se4ProcessArpWaitList(SE_IPV4 *p)
{
	UINT i;
	SE_LIST *o;
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	o = NULL;

	for (i = 0;i < SE_LIST_NUM(p->ArpWaitList);i++)
	{
		SE_ARPV4_WAIT *w = SE_LIST_DATA(p->ArpWaitList, i);

		if (Se4SendArpDoProcess(p, w))
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
			SE_ARPV4_WAIT *w = SE_LIST_DATA(o, i);

			SeDelete(p->ArpWaitList, w);
			SeFree(w);
		}

		SeFreeList(o);
	}
}

// ARP リクエスト送信プロセスを実行する
bool Se4SendArpDoProcess(SE_IPV4 *p, SE_ARPV4_WAIT *w)
{
	// 引数チェック
	if (p == NULL || w == NULL)
	{
		return false;
	}

	if (SeVpnDoInterval(p->Vpn, &w->IntervalValue, SE_IPV4_ARP_SEND_INTERVAL * 1000))
	{
		SE_ARPV4_HEADER arp;

		if ((w->SendCounter++) >= SE_IPV4_ARP_SEND_COUNT)
		{
			return true;
		}

		SeZero(&arp, sizeof(arp));
		arp.HardwareType = SeEndian16(SE_ARPV4_HARDWARE_TYPE_ETHERNET);
		arp.ProtocolType = SeEndian16(SE_MAC_PROTO_IPV4);
		arp.HardwareSize = 6;
		arp.ProtocolSize = 4;
		arp.Operation = SeEndian16(SE_ARPV4_OPERATION_REQUEST);
		SeCopy(arp.SrcAddress, p->Eth->MyMacAddress, SE_ETHERNET_MAC_ADDR_SIZE);
		arp.SrcIP = Se4IPToUINT(p->IpAddress);
		arp.TargetIP = Se4IPToUINT(w->IpAddress);

		Se4SendEthPacket(p, NULL, SE_MAC_PROTO_ARPV4, &arp, sizeof(arp));
	}

	return false;
}

// ARP レスポンスの送信
void Se4SendArpResponse(SE_IPV4 *p, UCHAR *dest_mac, SE_IPV4_ADDR dest_ip, SE_IPV4_ADDR src_ip)
{
	SE_ARPV4_HEADER arp;
	// 引数チェック
	if (p == NULL || dest_mac == NULL)
	{
		return;
	}

	SeZero(&arp, sizeof(arp));
	arp.HardwareType = SeEndian16(SE_ARPV4_HARDWARE_TYPE_ETHERNET);
	arp.ProtocolType = SeEndian16(SE_MAC_PROTO_IPV4);
	arp.HardwareSize = 6;
	arp.ProtocolSize = 4;
	arp.Operation = SeEndian16(SE_ARPV4_OPERATION_RESPONSE);
	SeCopy(arp.SrcAddress, p->Eth->MyMacAddress, SE_ETHERNET_MAC_ADDR_SIZE);
	SeCopy(arp.TargetAddress, dest_mac, SE_ETHERNET_MAC_ADDR_SIZE);
	arp.SrcIP = Se4IPToUINT(src_ip);
	arp.TargetIP = Se4IPToUINT(dest_ip);

	Se4SendEthPacket(p, dest_mac, SE_MAC_PROTO_ARPV4, &arp, sizeof(arp));
}

// ARP リクエストパケットの受信
void Se4RecvArpRequest(SE_IPV4 *p, SE_PACKET *pkt, SE_ARPV4_HEADER *arp)
{
	// 引数チェック
	if (p == NULL || pkt == NULL || arp == NULL)
	{
		return;
	}

	// 関連付け
	Se4MacIpRelationKnown(p, Se4UINTToIP(arp->SrcIP), arp->SrcAddress);

	// 自ホスト宛かどうか調査
	if (Se4Cmp(Se4UINTToIP(arp->TargetIP), p->IpAddress) == 0)
	{
		// ARP レスポンスの送信
		Se4SendArpResponse(p, arp->SrcAddress, Se4UINTToIP(arp->SrcIP), p->IpAddress);
	}
	else
	{
		if (p->UseProxyArp)
		{
			SE_IPV4_ADDR target_ip = Se4UINTToIP(arp->TargetIP);

			// プロキシ ARP 使用時
			if (Se4Cmp(p->ProxyArpExceptionAddress, target_ip) != 0)
			{
				if (Se4IsInSameNetwork(target_ip, p->IpAddress, p->SubnetMask))
				{
					if (Se4IsBroadcastAddress(target_ip) == false)
					{
						if (Se4Cmp(Se4GetBroadcastAddress(p->IpAddress, p->SubnetMask), target_ip) != 0 &&
							Se4Cmp(Se4GetNetworkAddress(p->IpAddress, p->SubnetMask), target_ip) != 0)
						{
							// ホストの所属しているネットワークのユニキャストアドレス宛の ARP 要求
							// が届いたのでプロキシ ARP 応答を行う
							Se4SendArpResponse(p, arp->SrcAddress, Se4UINTToIP(arp->SrcIP),
								target_ip);
						}
					}
				}
			}
		}
	}
}

// ARP レスポンスパケットの受信
void Se4RecvArpResponse(SE_IPV4 *p, SE_PACKET *pkt, SE_ARPV4_HEADER *arp)
{
	// 引数チェック
	if (p == NULL || pkt == NULL || arp == NULL)
	{
		return;
	}

	// 関連付け
	Se4MacIpRelationKnown(p, Se4UINTToIP(arp->SrcIP), arp->SrcAddress);
}

// ARP パケットの受信
void Se4RecvArp(SE_IPV4 *p, SE_PACKET *pkt)
{
	SE_ARPV4_HEADER *arp;
	// 引数チェック
	if (p == NULL || pkt == NULL)
	{
		return;
	}

	arp = pkt->L3.ARPv4Header;

	if (SeEndian16(arp->HardwareType) == SE_ARPV4_HARDWARE_TYPE_ETHERNET)
	{
		if (SeEndian16(arp->ProtocolType) == SE_MAC_PROTO_IPV4)
		{
			if (arp->HardwareSize == 6 && arp->ProtocolSize == 4)
			{
				if (SeCmp(arp->SrcAddress, pkt->MacHeader->SrcAddress, SE_ETHERNET_MAC_ADDR_SIZE) == 0)
				{
					switch (SeEndian16(arp->Operation))
					{
					case SE_ARPV4_OPERATION_REQUEST:
						// ARP リクエスト
						Se4RecvArpRequest(p, pkt, arp);
						break;

					case SE_ARPV4_OPERATION_RESPONSE:
						// ARP レスポンス
						Se4RecvArpResponse(p, pkt, arp);
						break;
					}
				}
			}
		}
	}
}

// プロードキャスト MAC アドレス
UCHAR *Se4BroadcastMacAddress()
{
	static UCHAR mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, };

	return mac;
}

// Ethernet パケットの送信
void Se4SendEthPacket(SE_IPV4 *p, UCHAR *dest_mac, USHORT protocol, void *data, UINT data_size)
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
		dest_mac = Se4BroadcastMacAddress();
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
void Se4RecvEthPacket(SE_IPV4 *p, void *packet, UINT packet_size)
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
		case SE_L3_UNKNOWN:	// 不明なパケット
			// 無視
			break;

		case SE_L3_ARPV4:	// ARPv4 パケット
			Se4RecvArp(p, pkt);
			break;

		case SE_L3_IPV4:	// IPv4 パケット
			Se4RecvIp(p, pkt);
			break;
		}
	}

	SeFreePacket(pkt);
}

// ARP エントリリストの比較関数
int Se4CmpArpEntry(void *p1, void *p2)
{
	SE_ARPV4_ENTRY *a1, *a2;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	a1 = *(SE_ARPV4_ENTRY **)p1;
	a2 = *(SE_ARPV4_ENTRY **)p2;
	if (a1 == NULL || a2 == NULL)
	{
		return 0;
	}

	return Se4Cmp(a1->IpAddress, a2->IpAddress);
}

// ARP エントリリストの初期化
SE_LIST *Se4InitArpEntryList()
{
	SE_LIST *o = SeNewList(Se4CmpArpEntry);

	return o;
}

// ARP エントリリストの解放
void Se4FreeArpEntryList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_ARPV4_ENTRY *a = SE_LIST_DATA(o, i);

		SeFree(a);
	}

	SeFreeList(o);
}

// ARP エントリリストのフラッシュ
void Se4FlushArpEntryList(SE_LIST *o, UINT64 tick)
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
		SE_ARPV4_ENTRY *e = SE_LIST_DATA(o, i);

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
			SE_ARPV4_ENTRY *e = SE_LIST_DATA(t, i);

			SeDelete(o, e);
			SeFree(e);
		}

		SeFreeList(t);
	}
}

// ARP エントリリストの検索
SE_ARPV4_ENTRY *Se4SearchArpEntryList(SE_LIST *o, SE_IPV4_ADDR a, UINT64 tick, UINT expire_span, bool dont_update_expire)
{
	SE_ARPV4_ENTRY t, *r;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	Se4FlushArpEntryList(o, tick);

	SeZero(&t, sizeof(t));
	t.IpAddress = a;

	r = SeSearch(o, &t);
	if (r == NULL)
	{
		return NULL;
	}

	if (dont_update_expire == false)
	{
		r->Expire = tick + (UINT64)expire_span * 1000ULL;
	}

	return r;
}

// ARP エントリの追加
void Se4AddArpEntryList(SE_LIST *o, UINT64 tick, UINT expire_span, SE_IPV4_ADDR ip_address, UCHAR *mac_address)
{
	SE_ARPV4_ENTRY *t;
	// 引数チェック
	if (o == NULL || mac_address == NULL)
	{
		return;
	}

	t = Se4SearchArpEntryList(o, ip_address, tick, expire_span, false);
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
		t = SeZeroMalloc(sizeof(SE_ARPV4_ENTRY));

		t->IpAddress = ip_address;
		SeCopy(t->MacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE);
		t->Created = tick;
		t->Expire = tick + (UINT64)expire_span * 1000ULL;

		SeInsert(o, t);
	}
}

// ARP 待機リストの検索
SE_ARPV4_WAIT *Se4SearchArpWaitList(SE_LIST *o, SE_IPV4_ADDR a)
{
	SE_ARPV4_WAIT t, *w;
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

// ARP 待機リストの比較関数
int Se4CmpArpWaitEntry(void *p1, void *p2)
{
	SE_ARPV4_WAIT *w1, *w2;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	w1 = *(SE_ARPV4_WAIT **)p1;
	w2 = *(SE_ARPV4_WAIT **)p2;
	if (w1 == NULL || w2 == NULL)
	{
		return 0;
	}

	return Se4Cmp(w1->IpAddress, w2->IpAddress);
}

// ARP 待機リストの初期化
SE_LIST *Se4InitArpWaitList()
{
	SE_LIST *o = SeNewList(Se4CmpArpWaitEntry);

	return o;
}

// ARP 待機リストの解放
void Se4FreeArpWaitList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_ARPV4_WAIT *w = SE_LIST_DATA(o, i);

		SeFree(w);
	}

	SeFreeList(o);
}

// 指定した IP アドレス宛の待機している IP パケットを一斉に送信する
void Se4SendWaitingIpWait(SE_IPV4 *p, SE_IPV4_ADDR ip_addr_local, UCHAR *mac_addr)
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
		SE_IPV4_WAIT *w = SE_LIST_DATA(p->IpWaitList, i);

		if (Se4Cmp(w->DestIPLocal, ip_addr_local) == 0)
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
			SE_IPV4_WAIT *w = SE_LIST_DATA(o, i);

			// 送信処理
			Se4SendIpFragmentNow(p, mac_addr, w->Data, w->Size);

			// リストから削除
			SeDelete(p->IpWaitList, w);

			// 解放
			Se4FreeIpWait(w);
		}

		SeFreeList(o);
	}
}

// 古い IP 待機リストの削除
void Se4FlushIpWaitList(SE_IPV4 *p)
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
		SE_IPV4_WAIT *w = SE_LIST_DATA(p->IpWaitList, i);

		if (w->Expire <= Se4Tick(p))
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
			SE_IPV4_WAIT *w = SE_LIST_DATA(o, i);

			Se4FreeIpWait(w);

			SeDelete(p->IpWaitList, w);
		}

		SeFreeList(o);
	}
}

// IP 待機リストを挿入
void Se4InsertIpWait(SE_LIST *o, UINT64 tick, SE_IPV4_ADDR dest_ip_local, SE_IPV4_ADDR src_ip, void *data, UINT size)
{
	SE_IPV4_WAIT *w;
	// 引数チェック
	if (o == NULL || data == NULL)
	{
		return;
	}

	w = SeZeroMalloc(sizeof(SE_IPV4_WAIT));
	w->Data = data;
	w->Size = size;
	w->SrcIP = src_ip;
	w->DestIPLocal = dest_ip_local;
	w->Expire = tick + (UINT64)(SE_IPV4_ARP_SEND_INTERVAL * SE_IPV4_ARP_SEND_COUNT) * 1000ULL;

	SeAdd(o, w);
}

// IP 待機リストの初期化
SE_LIST *Se4InitIpWaitList()
{
	SE_LIST *o = SeNewList(NULL);

	return o;
}

// IP 待機エントリの解放
void Se4FreeIpWait(SE_IPV4_WAIT *w)
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
void Se4FreeIpWaitList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV4_WAIT *w = SE_LIST_DATA(o, i);

		Se4FreeIpWait(w);
	}

	SeFreeList(o);
}

// IP 結合リストの比較
int Se4CmpIpCombineList(void *p1, void *p2)
{
	SE_IPV4_COMBINE *c1, *c2;
	UINT i;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	c1 = *(SE_IPV4_COMBINE **)p1;
	c2 = *(SE_IPV4_COMBINE **)p2;
	if (c1 == NULL || c2 == NULL)
	{
		return 0;
	}

	i = Se4Cmp(c1->DestIpAddress, c2->DestIpAddress);
	if (i != 0)
	{
		return i;
	}

	i = Se4Cmp(c1->SrcIpAddress, c2->SrcIpAddress);
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

	if (c1->Protocol > c2->Protocol)
	{
		return 1;
	}
	else if (c1->Protocol < c2->Protocol)
	{
		return -1;
	}

	return 0;
}

// IP 結合リストの初期化
SE_LIST *Se4InitIpCombineList()
{
	SE_LIST *o = SeNewList(Se4CmpIpCombineList);

	return o;
}

// IP 結合リストの検索
SE_IPV4_COMBINE *Se4SearchIpCombineList(SE_LIST *o, SE_IPV4_ADDR dest, SE_IPV4_ADDR src, USHORT id, UCHAR protocol)
{
	SE_IPV4_COMBINE c, *ret;
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
void Se4FreeIpCombineList(SE_IPV4 *p, SE_LIST *o)
{
	UINT i;
	// 結合チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV4_COMBINE *c = SE_LIST_DATA(o, i);

		Se4FreeIpCombine(p, c);
	}

	SeFreeList(o);
}

// IP 結合処理
void Se4CombineIp(SE_IPV4 *p, SE_IPV4_COMBINE *c, UINT offset, void *data, UINT size, bool last_packet)
{
	UINT i;
	UINT need_size;
	UINT data_size_delta;
	SE_IPV4_FRAGMENT *f;
	bool status_changed = false;
	// 引数チェック
	if (c == NULL || data == NULL)
	{
		return;
	}

	// オフセットとサイズをチェック
	if ((offset + size) > SE_IP4_MAX_PAYLOAD_SIZE)
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
		SE_IPV4_FRAGMENT *f = SE_LIST_DATA(c->IpFragmentList, i);

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
		f = SeZeroMalloc(sizeof(SE_IPV4_FRAGMENT));

		f->Offset = offset;
		f->Size = size;

		SeAdd(c->IpFragmentList, f);

		status_changed = true;
	}

	if (status_changed)
	{
		c->Expire = Se4Tick(p) + SE_IPV4_COMBINE_TIMEOUT * 1000ULL;
	}

	if (c->Size != 0)
	{
		// すでに受信したデータ部分リストの合計サイズを取得する
		UINT total_size = 0;
		UINT i;

		for (i = 0;i < SE_LIST_NUM(c->IpFragmentList);i++)
		{
			SE_IPV4_FRAGMENT *f = SE_LIST_DATA(c->IpFragmentList, i);

			total_size += f->Size;
		}

		if (total_size == c->Size)
		{
			// IP パケットをすべて受信した
			Se4RecvIpComplete(p, c->SrcIpAddress, c->DestIpAddress, c->Id,
				c->Protocol, c->Ttl, c->Data, c->Size, c->IsBroadcast);

			// 結合オブジェクトの解放
			Se4FreeIpCombine(p, c);

			// 結合オブジェクトをリストから削除
			SeDelete(p->IpCombineList, c);
		}
	}
}

// 古くなった IP 結合リストの削除
void Se4FlushIpCombineList(SE_IPV4 *p, SE_LIST *o)
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
		SE_IPV4_COMBINE *c = SE_LIST_DATA(o, i);

		if (c->Expire <= Se4Tick(p) ||
		    p->combine_current_id - c->combine_id > SE_IPV4_COMBINE_MAX_COUNT)
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
			SE_IPV4_COMBINE *c = SE_LIST_DATA(t, i);

			Se4FreeIpCombine(p, c);

			SeDelete(o, c);
		}

		SeFreeList(t);
	}
}

// IP 結合エントリの挿入
SE_IPV4_COMBINE *Se4InsertIpCombine(SE_IPV4 *p, SE_IPV4_ADDR src_ip, SE_IPV4_ADDR dest_ip,
									USHORT id, UCHAR protocol, UCHAR ttl, bool is_broadcast)
{
	SE_IPV4_COMBINE *c;
	// 引数チェック
	if (p == NULL)
	{
		return NULL;
	}

	// クォータの調査
	if ((p->CurrentIpQuota + SE_IPV4_COMBINE_INITIAL_BUF_SIZE) > SE_IPV4_COMBINE_QUEUE_SIZE_QUOTA)
	{
		// メモリ不足
		return NULL;
	}

	c = SeZeroMalloc(sizeof(SE_IPV4_COMBINE));
	c->DestIpAddress = dest_ip;
	c->SrcIpAddress = src_ip;
	c->Id = id;
	c->Expire = Se4Tick(p) + (UINT64)SE_IPV4_COMBINE_TIMEOUT * 1000ULL;
	c->Size = 0;
	c->IpFragmentList = Se4InitIpFragmentList();
	c->Protocol = protocol;
	c->Ttl = ttl;
	c->IsBroadcast = is_broadcast;
	c->combine_id = p->combine_current_id++;

	c->DataReserved = SE_IPV4_COMBINE_INITIAL_BUF_SIZE;
	c->Data = SeMalloc(c->DataReserved);

	SeInsert(p->IpCombineList, c);
	p->CurrentIpQuota += c->DataReserved;

	return c;
}

// IP 結合エントリの解放
void Se4FreeIpCombine(SE_IPV4 *p, SE_IPV4_COMBINE *c)
{
	// 引数チェック
	if (c == NULL)
	{
		return;
	}

	p->CurrentIpQuota -= c->DataReserved;
	SeFree(c->Data);
	Se4FreeIpFragmentList(c->IpFragmentList);

	SeFree(c);
}

// IP フラグメントリストの初期化
SE_LIST *Se4InitIpFragmentList()
{
	SE_LIST *o = SeNewList(NULL);

	return o;
}

// IP フラグメントリストの解放
void Se4FreeIpFragmentList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IPV4_FRAGMENT *f = SE_LIST_DATA(o, i);

		SeFree(f);
	}

	SeFreeList(o);
}

// IPv4 バインド (初期化)
SE_IPV4 *Se4Init(SE_VPN *vpn, SE_ETH *eth, bool physical, SE_IPV4_ADDR ip, SE_IPV4_ADDR subnet,
				 SE_IPV4_ADDR gateway, UINT mtu, SE_IPV4_RECV_CALLBACK *recv_callback, void *recv_callback_param)
{
	SE_IPV4 *p;
	// 引数チェック
	if (vpn == NULL || eth == NULL || recv_callback == NULL)
	{
		return NULL;
	}

	p = SeZeroMalloc(sizeof(SE_IPV4));
	p->Vpn = vpn;
	p->Eth = eth;
	p->Physical = physical;
	p->RecvCallback = recv_callback;
	p->RecvCallbackParam = recv_callback_param;
	p->IpAddress = ip;
	p->SubnetMask = subnet;
	p->NetworkAddress = Se4GetNetworkAddress(ip, subnet);
	p->BroadcastAddress = Se4GetBroadcastAddress(ip, subnet);
	if (Se4IsZeroIP(gateway) == false)
	{
		p->DefaultGateway = gateway;
		p->UseDefaultGateway = true;
	}
	p->Mtu = mtu;

	p->ArpEntryList = Se4InitArpEntryList();
	p->ArpWaitList = Se4InitArpWaitList();
	p->IpWaitList = Se4InitIpWaitList();
	p->IpCombineList = Se4InitIpCombineList();

	return p;
}

// IPv4 バインド解除 (解放)
void Se4Free(SE_IPV4 *p)
{
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	Se4FreeIpCombineList(p, p->IpCombineList);
	Se4FreeIpWaitList(p->IpWaitList);
	Se4FreeArpWaitList(p->ArpWaitList);
	Se4FreeArpEntryList(p->ArpEntryList);

	SeFree(p);
}

// 同一のネットワークかどうか調べる
bool Se4IsInSameNetwork(SE_IPV4_ADDR a1, SE_IPV4_ADDR a2, SE_IPV4_ADDR subnet)
{
	if (Se4Cmp(Se4GetNetworkAddress(a1, subnet), Se4GetNetworkAddress(a2, subnet)) == 0)
	{
		return true;
	}

	return false;
}

// ユニキャストアドレスが有効かどうか取得する
bool Se4CheckUnicastAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet)
{
	if (Se4IsZeroIP(Se4GetHostAddress(addr, subnet)))
	{
		return false;
	}
	if (Se4Cmp(addr, Se4GetBroadcastAddress(addr, subnet)) == 0)
	{
		return false;
	}

	return true;
}

// ホストアドレスの取得
SE_IPV4_ADDR Se4GetHostAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet)
{
	return Se4And(addr, Se4Not(subnet));
}

// ネットワークアドレスの取得
SE_IPV4_ADDR Se4GetNetworkAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet)
{
	return Se4And(addr, subnet);
}

// ブロードキャストアドレスかどうか判定
bool Se4IsBroadcastAddress(SE_IPV4_ADDR addr)
{
	UINT i = Se4IPToUINT(addr);

	if (i == 0xffffffff)
	{
		return true;
	}

	return false;
}

// ブロードキャストアドレスの取得
SE_IPV4_ADDR Se4GetBroadcastAddress(SE_IPV4_ADDR addr, SE_IPV4_ADDR subnet)
{
	return Se4Or(Se4GetNetworkAddress(addr, subnet), Se4Not(subnet));
}

// IP アドレスの論理演算
SE_IPV4_ADDR Se4And(SE_IPV4_ADDR a, SE_IPV4_ADDR b)
{
	SE_IPV4_ADDR ret;
	UINT i;

	for (i = 0;i < 4;i++)
	{
		ret.Value[i] = a.Value[i] & b.Value[i];
	}

	return ret;
}
SE_IPV4_ADDR Se4Or(SE_IPV4_ADDR a, SE_IPV4_ADDR b)
{
	SE_IPV4_ADDR ret;
	UINT i;

	for (i = 0;i < 4;i++)
	{
		ret.Value[i] = a.Value[i] | b.Value[i];
	}

	return ret;
}
SE_IPV4_ADDR Se4Not(SE_IPV4_ADDR a)
{
	SE_IPV4_ADDR ret;
	UINT i;

	for (i = 0;i < 4;i++)
	{
		ret.Value[i] = ~(a.Value[i]);
	}

	return ret;
}

// 指定したアドレスがサブネットマスクかどうかチェック
bool Se4IsSubnetMask(SE_IPV4_ADDR a)
{
	UINT i;
	for (i = 0;i <= 32;i++)
	{
		if (Se4Cmp(a, Se4GenerateSubnetMask(i)) == 0)
		{
			return true;
		}
	}

	return false;
}

// IP アドレスの比較
int Se4Cmp(SE_IPV4_ADDR a, SE_IPV4_ADDR b)
{
	UINT u1 = Se4IPToUINT(a);
	UINT u2 = Se4IPToUINT(b);

	if (u1 > u2)
	{
		return 1;
	}
	else if (u1 < u2)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

// ゼロかどうかチェック
bool Se4IsZeroIP(SE_IPV4_ADDR a)
{
	return (Se4IPToUINT(a) == 0 ? true : false);
}

// サブネットマスクの作成
SE_IPV4_ADDR Se4GenerateSubnetMask(UINT i)
{
	return Se4UINTToIP(Se4GenerateSubnetMask32(i));
}
UINT Se4GenerateSubnetMask32(UINT i)
{
	return SeEndian32((0xffffffff << (32 - MAKESURE(i, 0, 32))));
}

// IP アドレスを文字列に変換
void Se4IPToStr(char *str, SE_IPV4_ADDR a)
{
	// 引数チェック
	if (str == NULL)
	{
		return;
	}

	SeFormat(str, 0, "%u.%u.%u.%u", a.Value[0], a.Value[1], a.Value[2], a.Value[3]);
}
void Se4IPToStr32(char *str, UINT v)
{
	Se4IPToStr(str, Se4UINTToIP(v));
}

// 文字列を IP アドレスに変換
SE_IPV4_ADDR Se4StrToIP(char *str)
{
	SE_TOKEN_LIST *t;
	SE_IPV4_ADDR a;
	char tmp[MAX_SIZE];
	// 引数チェック
	if (str == NULL)
	{
		return Se4ZeroIP();
	}

	SeZero(&a, sizeof(a));

	SeStrCpy(tmp, sizeof(tmp), str);
	SeTrim(tmp);
	t = SeParseToken(tmp, ".");

	if (t->NumTokens == 4)
	{
		UINT i;

		for (i = 0;i < 4;i++)
		{
			a.Value[i] = SeToInt(t->Token[i]);
		}
	}

	SeFreeToken(t);

	return a;
}
UINT Se4StrToIP32(char *str)
{
	return Se4IPToUINT(Se4StrToIP(str));
}

// 0
SE_IPV4_ADDR Se4ZeroIP()
{
	SE_IPV4_ADDR a;

	SeZero(&a, sizeof(a));

	return a;
}

// IP アドレスを整数に変換
UINT Se4IPToUINT(SE_IPV4_ADDR a)
{
	UINT v;

	SeCopy(&v, &a, sizeof(UINT));

	return v;
}

// 整数を IP アドレスに変換
SE_IPV4_ADDR Se4UINTToIP(UINT v)
{
	SE_IPV4_ADDR a;

	SeCopy(&a, &v, sizeof(UINT));

	return a;
}
