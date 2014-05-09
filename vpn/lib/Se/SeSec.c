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

// SeSec.c
// 概要: IPsec 処理

#define SE_INTERNAL
#include <Se/Se.h>

//Main mode
bool SeSecSendMain1(SE_SEC *s)
{
	SE_IKE_SA *sa;
	SE_SEC_CONFIG *config;
	char tmp1[MAX_SIZE], tmp2[MAX_SIZE];
	UINT vpn_connect_timeout_interval;
	// 引数チェック
	if (s == NULL)
	{
		return false;
	}

	config = &s->Config;

	if (config->VpnAuthMethod != SE_SEC_AUTH_METHOD_PASSWORD)
	{
		SeError("IKE: main mode: VpnAuthMethod is not Password");
		return false;
	}

	if (s->SendStrictIdV6 && s->IPv6)
	{
		if (SeIkeIsZeroIP(&config->MyVirtualIpAddress))
		{
			// 自分の使用すべき仮想 IP アドレスが指定されていない
			return false;
		}
	}

	SeIkeIpAddressToStr(tmp1, &s->Config.VpnGatewayAddress);
	SeIkeIpAddressToStr(tmp2, &s->Config.MyIpAddress);
	SeInfo("IKE: VPN Connect Start: %s -> %s", tmp2, tmp1);

	// IKE SA の作成
	sa = SeSecNewIkeSa(s, s->Config.MyIpAddress, s->Config.VpnGatewayAddress,
		SE_SEC_IKE_UDP_PORT, SE_SEC_IKE_UDP_PORT, SeSecGenIkeSaInitCookie(s));
	sa->ResponderCookie = 0;

	SeInfo("IKE: SA #%u Created", sa->Id);

	SeBinToStr(tmp1, sizeof(tmp1), &sa->InitiatorCookie, sizeof(sa->InitiatorCookie));
	SeInfo("IKE: SA #%u: Initiator Cookie: 0x%s", sa->Id, tmp1);

	// DH 作成
	sa->Dh = SeDhNewGroup2();

	// 乱数生成
	sa->Phase1MyRand = SeRandBuf(SE_SHA1_HASH_SIZE);

	vpn_connect_timeout_interval = s->Config.VpnConnectTimeout * 1000;
	sa->ConnectTimeoutTick = SeSecTick(s) + (UINT64)(vpn_connect_timeout_interval);

	SeSecAddTimer(s, vpn_connect_timeout_interval);

	// パケットの構築
	if (true)
	{
		SE_LIST *payload_list;
		SE_LIST *transform_value_list;
		SE_IKE_PACKET_PAYLOAD *transform_payload;
		SE_LIST *transform_payload_list;
		SE_LIST *proposal_payload_list;
		SE_IKE_PACKET_PAYLOAD *proposal_payload;
		SE_BUF *packet_buf;
		SE_IKE_PACKET *packet;
		SE_IKE_PACKET_PAYLOAD *sa_payload;

		payload_list = SeNewList(NULL);

		// トランスフォーム値リストの作成
		transform_value_list = SeNewList(NULL);
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_CRYPTO, config->VpnPhase1Crypto));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_HASH, config->VpnPhase1Hash));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_AUTH_METHOD, SE_IKE_P1_AUTH_METHOD_PRESHAREDKEY));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_DH_GROUP, SE_IKE_P1_DH_GROUP_1024_MODP));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_TYPE, SE_IKE_P1_LIFE_TYPE_SECONDS));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_VALUE, config->VpnPhase1LifeSeconds));
		if (config->VpnPhase1LifeKilobytes != 0)
		{
			SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_TYPE, SE_IKE_P1_LIFE_TYPE_KILOBYTES));
			SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_VALUE, config->VpnPhase1LifeKilobytes));
		}

		// トランスフォームペイロードの作成
		transform_payload = SeIkeNewTransformPayload(0, SE_IKE_TRANSFORM_ID_P1_KEY_IKE, transform_value_list);

		// トランスフォームペイロードリストの作成
		transform_payload_list = SeNewList(NULL);
		SeAdd(transform_payload_list, transform_payload);

		// プロポーザルペイロードの作成
		proposal_payload = SeIkeNewProposalPayload(0, SE_IKE_PROTOCOL_ID_IKE, NULL, 0, transform_payload_list);

		// プロポーザルペイロードリストの作成
		proposal_payload_list = SeNewList(NULL);
		SeAdd(proposal_payload_list, proposal_payload);

		// SA ペイロードの追加
		sa_payload = SeIkeNewSaPayload(proposal_payload_list);
		SeAdd(payload_list, sa_payload);

		// ベンダ ID ペイロードの追加
		SeAdd(payload_list, SeIkeNewDataPayload(SE_IKE_PAYLOAD_VENDOR_ID,
			SE_SEC_VENDOR_ID_STR, SeStrLen(SE_SEC_VENDOR_ID_STR)));

		// IKE パケットのビルド
		packet = SeIkeNew(sa->InitiatorCookie, 0, SE_IKE_EXCHANGE_TYPE_MAIN,
			false, false, false, 0,
			payload_list);

		packet_buf = SeIkeBuild(packet, NULL);

		// ペイロードのコピーをとっておく
		sa->SAi_b = SeCloneBuf(sa_payload->BitArray);

		sa->TransferBytes += packet->DecryptedPayload->Size;

		// 送信
		SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

		sa->Status = 1;

		SeFreeBuf(packet_buf);
		SeIkeFree(packet);
	}

	return true;
}

void SeSecRecvMain2(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size)
{
	SE_IKE_PACKET *packet = NULL;
	SE_LIST *payload_list = NULL;
	SE_IKE_PACKET_PAYLOAD *sa_payload = NULL;

	// 引数チェック
	if (s == NULL || sa == NULL || data == NULL)
	{
		return;
	}

	packet = SeIkeParse(data, size, NULL);
	if (packet == NULL)
	{
		return;
	}

	sa->ResponderCookie = packet->ResponderCookie;

	payload_list = packet->PayloadList;
	if (payload_list == NULL)
	{
		SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);
		goto exit;
	}

	// SA ペイロードの取得
	sa_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_SA, 0);

	if (sa_payload == NULL)
	{
		SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);
		goto exit;
	}

	// Here we must proceed SA payload if we have sent multiple proposals in packet 1

	sa->TransferBytes += packet->DecryptedPayload->Size;

	sa->Status = 2;

	SeSecSendMain3(s, sa);

exit:
	SeIkeFree(packet);
}

void SeSecSendMain3(SE_SEC *s, SE_IKE_SA *sa)
{
	SE_LIST *payload_list = NULL;
	SE_IKE_PACKET *packet = NULL;
	SE_BUF *packet_buf = NULL;

	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return;
	}

	payload_list = SeNewList(NULL);

	// 鍵交換ペイロードの追加
	SeAdd(payload_list, SeIkeNewDataPayload(SE_IKE_PAYLOAD_KEY_EXCHANGE,
		sa->Dh->MyPublicKey->Buf, sa->Dh->MyPublicKey->Size));

	// 乱数ペイロードの追加
	SeAdd(payload_list, SeIkeNewDataPayload(SE_IKE_PAYLOAD_RAND,
		sa->Phase1MyRand->Buf, sa->Phase1MyRand->Size));

	packet = SeIkeNew(sa->InitiatorCookie, sa->ResponderCookie, SE_IKE_EXCHANGE_TYPE_MAIN,
		false, false, false, 0,
		payload_list);

	packet_buf = SeIkeBuild(packet, NULL);

	// 送信
	SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

	sa->TransferBytes += packet->DecryptedPayload->Size;

	sa->Status = 3;

	SeFreeBuf(packet_buf);
	SeIkeFree(packet);
}

void SeSecRecvMain4(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size)
{
	SE_SEC_CONFIG *config = NULL;

	SE_IKE_PACKET *packet = NULL;
	SE_LIST *payload_list = NULL;
	SE_IKE_PACKET_PAYLOAD *key_payload = NULL, *rand_payload = NULL;

	// 引数チェック
	if (s == NULL || sa == NULL || data == NULL)
	{
		return;
	}

	config = &s->Config;

	packet = SeIkeParse(data, size, NULL);
	if (packet == NULL)
	{
		return;
	}

	payload_list = packet->PayloadList;
	if (payload_list == NULL)
	{
		SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);
		goto exit;
	}

	// 鍵交換ペイロードの取得
	key_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_KEY_EXCHANGE, 0);

	// 乱数ペイロードの取得
	rand_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_RAND, 0);

	if (key_payload == NULL || rand_payload == NULL)
	{
		SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);
		goto exit;
	}
	 // パスワードバッファの作成
	sa->Phase1Password = SeIkeStrToPassword(config->VpnPassword);

	// DH 共有鍵の計算
	sa->Dh->YourPublicKey = SeCloneBuf(key_payload->Payload.KeyExchange.Data);

	if (sa->Dh->YourPublicKey->Size != SE_DH_KEY_SIZE)
	{
		// 鍵サイズ不正
		SeError("IKE: SA #%u: Invalid DH Key Size: %u", sa->Id, sa->Dh->YourPublicKey->Size);
		goto exit;
	}

	sa->TransferBytes += packet->DecryptedPayload->Size;

	// Phase one keyset computing
	{
		UCHAR private_key[SE_DH_KEY_SIZE];
		SE_BUF *tmp_buf = NULL;

		if (SeDhCompute(sa->Dh, private_key, sa->Dh->YourPublicKey->Buf, sa->Dh->YourPublicKey->Size) == false)
		{
			SeError("IKE: SA #%u: DH Computing Key Failed", sa->Id);
			goto exit;
		}

		// パスワード認証
		tmp_buf = SeNewBuf();
		SeWriteBufBuf(tmp_buf, sa->Phase1MyRand);
		SeWriteBufBuf(tmp_buf, rand_payload->Payload.Rand.Data);
		SeMacSha1(sa->SKEYID, sa->Phase1Password->Buf, sa->Phase1Password->Size,
			tmp_buf->Buf, tmp_buf->Size);

		SeFreeBuf(tmp_buf);

		// 鍵セットの計算
		SeSecCalcP1KeySet(&sa->P1KeySet, sa->SKEYID, sizeof(sa->SKEYID), private_key, sizeof(private_key),
			sa->InitiatorCookie, sa->ResponderCookie,
			SeIkePhase1CryptIdToKeySize(config->VpnPhase1Crypto));
	}

	sa->Status = 4;

	SeSecSendMain5(s, sa);

exit:
	SeIkeFree(packet);

}

void SeSecSendMain5(SE_SEC *s, SE_IKE_SA *sa)
{
	SE_SEC_CONFIG *config = NULL;

	SE_LIST *payload_list = NULL;
	SE_IKE_PACKET_PAYLOAD *id_payload = NULL, *hash_payload = NULL;
	SE_IKE_PACKET *packet = NULL;
	SE_BUF *packet_buf = NULL;

	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return;
	}

	config = &s->Config;

	payload_list = SeNewList(NULL);

	// Initiator IP address is sent as ID
	if (s->IPv6 == false)
	{
		UINT myip = Se4IPToUINT(SeIkeGetIPv4Address(&config->MyIpAddress));
		id_payload = SeIkeNewIdPayload(SE_IKE_ID_IPV4_ADDR, 0, 0, &myip, sizeof(myip));
	}
	else
	{
		id_payload = SeIkeNewIdPayload(SE_IKE_ID_IPV6_ADDR, 0, 0,
					&config->MyIpAddress.Address.Ipv6, sizeof(SE_IPV6_ADDR));
	}

	sa->IDii_b = SeIkeBuildIdPayload(&id_payload->Payload.Id);

	SeAdd(payload_list, id_payload);

	// Initiator hash computing
	{
		UCHAR hash_init[SE_SHA1_HASH_SIZE];
		SE_BUF *b = SeNewBuf();

		SeWriteBufBuf(b, sa->Dh->MyPublicKey);
		SeWriteBufBuf(b, sa->Dh->YourPublicKey);
		SeWriteBuf(b, &sa->InitiatorCookie, sizeof(sa->InitiatorCookie));
		SeWriteBuf(b, &sa->ResponderCookie, sizeof(sa->ResponderCookie));
		SeWriteBufBuf(b, sa->SAi_b);
		SeWriteBufBuf(b, sa->IDii_b);
		SeMacSha1(hash_init, sa->SKEYID, sizeof(sa->SKEYID), b->Buf, b->Size);
		SeFreeBuf(b);

		hash_payload = SeIkeNewDataPayload(SE_IKE_PAYLOAD_HASH, hash_init, sizeof(hash_init));
	}

	SeAdd(payload_list, hash_payload);

	// Computation of the phase one IV
	SeSecCalcP1Iv(sa->Phase1Iv,
		sa->Dh->MyPublicKey->Buf,
		sa->Dh->MyPublicKey->Size,
		sa->Dh->YourPublicKey->Buf,
		sa->Dh->YourPublicKey->Size,
		sizeof(sa->Phase1Iv));

	// Computation of key, packets 5 and 6 of phase one and everything in phase two are encrypted with
	if (config->VpnPhase1Crypto == SE_IKE_P1_CRYPTO_DES_CBC)
	{
		// DES
		UCHAR *p_key = (UCHAR *)sa->P1KeySet.SKEYID_e->Buf;

		sa->Phase2DesKey = SeDesNewKey(p_key);
	}
	else
	{
		// 3DES
		UCHAR *p_key = (UCHAR *)sa->P1KeySet.SKEYID_e->Buf;

		sa->Phase2DesKey = SeDes3NewKey(
			p_key + SE_DES_KEY_SIZE * 0,
			p_key + SE_DES_KEY_SIZE * 1,
			p_key + SE_DES_KEY_SIZE * 2);
	}

	packet = SeIkeNew(sa->InitiatorCookie, sa->ResponderCookie, SE_IKE_EXCHANGE_TYPE_MAIN,
		true, false, false, 0,
		payload_list);

	//Building encrypted packet
	{
		SE_IKE_CRYPTO_PARAM cparam;
		SeZero(&cparam, sizeof(cparam));
		SeCopy(cparam.Iv, sa->Phase1Iv, sizeof(cparam.Iv));
		cparam.DesKey = sa->Phase2DesKey;

		packet_buf = SeIkeBuild(packet, &cparam);
		SeCopy(sa->Phase1Iv, cparam.NextIv, SE_DES_IV_SIZE);
	}

	// 送信
	SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

	sa->TransferBytes += packet->DecryptedPayload->Size;

	sa->Status = 5;

	SeFreeBuf(packet_buf);
	SeIkeFree(packet);
}

void SeSecRecvMain6(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size)
{
	SE_SEC_CONFIG *config = NULL;

	SE_IKE_PACKET *packet = NULL;
	SE_LIST *payload_list = NULL;
	SE_IKE_PACKET_PAYLOAD *id_payload = NULL, *hash_payload = NULL;

	// 引数チェック
	if (s == NULL || sa == NULL || data == NULL)
	{
		return;
	}

	config = &s->Config;

	// Parsing encrypted packet
	{
		SE_IKE_CRYPTO_PARAM cparam;
		SeZero(&cparam, sizeof(cparam));
		cparam.DesKey = sa->Phase2DesKey;

		SeCopy(cparam.Iv, sa->Phase1Iv, SE_DES_IV_SIZE);

		packet = SeIkeParse(data, size, &cparam);
		if (packet == NULL)
		{
			return;
		}
		SeCopy(sa->Phase1Iv, cparam.NextIv, SE_DES_IV_SIZE);
	}

	payload_list = packet->PayloadList;
	if (payload_list == NULL)
	{
		SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);
		goto exit;
	}

	// ID ペイロードの取得
	id_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_ID, 0);

	// ハッシュペイロードの取得
	hash_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_HASH, 0);

	if (id_payload == NULL || hash_payload == NULL)
	{
		SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);

		goto exit;
	}

	// Responder hash computing
	{
		UCHAR hash_resp[SE_SHA1_HASH_SIZE];
		SE_BUF *b = SeNewBuf();

		SeWriteBufBuf(b, sa->Dh->YourPublicKey);
		SeWriteBufBuf(b, sa->Dh->MyPublicKey);
		SeWriteBuf(b, &sa->ResponderCookie, sizeof(sa->ResponderCookie));
		SeWriteBuf(b, &sa->InitiatorCookie, sizeof(sa->InitiatorCookie));
		SeWriteBufBuf(b, sa->SAi_b);
		SeWriteBufBuf(b, id_payload->BitArray);
		SeMacSha1(hash_resp, sa->SKEYID, sizeof(sa->SKEYID), b->Buf, b->Size);
		SeFreeBuf(b);

		if ((SeCmpEx(hash_resp, sizeof(hash_resp),
			hash_payload->Payload.Hash.Data->Buf,
			hash_payload->Payload.Hash.Data->Size) == false))
		{
			// レスポンダハッシュ値の比較
			SeError("IKE: SA #%u: Phase 1 Auth Failed (Invalid Responder Hash)", sa->Id);

			goto exit;
		}
	}

	sa->TransferBytes += packet->DecryptedPayload->Size;

	sa->Status = 6;

	sa->Phase = 1;
	sa->Phase1EstablishedTick = SeSecTick(s);

	if (config->VpnPhase1LifeSeconds != 0)
	{
		SeSecAddTimer(s, (UINT)SeSecLifeSeconds64bit(config->VpnPhase1LifeSeconds));
	}

	if (true)
	{
		// フェーズ 2 の開始を準備
		UINT interval = config->VpnWaitPhase2BlankSpan;
		UINT64 start_tick = SeSecTick(s) + (UINT64)interval;

		sa->Phase2StartTick = start_tick;
		SeSecAddTimer(s, interval);
	}

exit:
	SeIkeFree(packet);
}

// IKE SA の削除メッセージ送信
void SeSecSendIkeSaDeleteMsg(SE_SEC *s, SE_IKE_SA *sa)
{
	SE_IKE_PACKET_PAYLOAD *payload;
	SE_LIST *spi_list;
	UCHAR spi[sizeof(UINT64) * 2];
	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return;
	}

	spi_list = SeNewList(NULL);
	SeCopy(spi, &sa->InitiatorCookie, sizeof(UINT64));
	SeCopy(spi + sizeof(UINT64), &sa->ResponderCookie, sizeof(UINT64));
	SeAdd(spi_list, SeMemToBuf(spi, sizeof(spi)));

	payload = SeIkeNewDeletePayload(SE_IKE_PROTOCOL_ID_IKE, spi_list);

	SeSecSendIkeSaMsgInfoSimple(s, sa, payload);
}

// IPsec SA の削除メッセージ送信
void SeSecSendIPsecSaDeleteMsg(SE_SEC *s, SE_IPSEC_SA *sa, SE_IKE_SA *ike_sa)
{
	SE_IKE_PACKET_PAYLOAD *payload;
	SE_LIST *spi_list;
	// 引数チェック
	if (s == NULL || sa == NULL || ike_sa == NULL)
	{
		return;
	}

	spi_list = SeNewList(NULL);
	SeAdd(spi_list, SeMemToBuf(&sa->Spi, sizeof(UINT)));
	
	payload = SeIkeNewDeletePayload(SE_IKE_PROTOCOL_ID_IPSEC_ESP, spi_list);

	SeSecSendIkeSaMsgInfoSimple(s, ike_sa, payload);
}

// IKE SA 情報交換メッセージ送信処理 (シンプル)
void SeSecSendIkeSaMsgInfoSimple(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET_PAYLOAD *payload)
{
	SE_LIST *payload_list;
	// 引数チェック
	if (s == NULL || sa == NULL || payload == NULL)
	{
		return;
	}

	payload_list = SeNewList(NULL);
	SeAdd(payload_list, payload);

	SeSecSendIkeSaMsgInfo(s, sa, payload_list);
}

// IKE SA 情報交換メッセージ送信処理
void SeSecSendIkeSaMsgInfo(SE_SEC *s, SE_IKE_SA *sa, SE_LIST *payload_list_src)
{
	SE_SEC_CONFIG *config;
	SE_IKE_CRYPTO_PARAM cparam;
	UINT msg_id;
	UINT msg_id_be;

	// 引数チェック
	if (s == NULL || sa == NULL || payload_list_src == NULL)
	{
		return;
	}

	config = &s->Config;
	msg_id = SeSecGenIkeSaMessageId(s);
	msg_id_be = SeEndian32(msg_id);

	// パケットの構築
	if (true)
	{
		UCHAR hash_dummy[SE_SHA1_HASH_SIZE];
		UCHAR hash1[SE_SHA1_HASH_SIZE];
		SE_LIST *payload_list;
		SE_BUF *packet_buf;
		SE_IKE_PACKET *packet;
		SE_IKE_PACKET_PAYLOAD *hash_payload;
		UINT i;
		SE_BUF *buf_after_hash;
		UINT after_hash_offset;
		UINT after_hash_size;
		SE_BUF *tmp;

		payload_list = SeNewList(NULL);

		// ハッシュペイロードの追加
		SeZero(hash_dummy, sizeof(hash_dummy));
		hash_payload = SeIkeNewDataPayload(SE_IKE_PAYLOAD_HASH, hash_dummy, sizeof(hash_dummy));
		SeAdd(payload_list, hash_payload);

		// その他のペイロードの追加
		for (i = 0;i < SE_LIST_NUM(payload_list_src);i++)
		{
			SE_IKE_PACKET_PAYLOAD *pay = SE_LIST_DATA(payload_list_src, i);

			SeAdd(payload_list, pay);
		}
		SeFreeList(payload_list_src);

		// ハッシュ計算用の一時的なパケットの作成
		packet = SeIkeNew(sa->InitiatorCookie, sa->ResponderCookie,
			SE_IKE_EXCHANGE_TYPE_INFORMATION, false, false, false, msg_id,
			payload_list);

		packet_buf = SeIkeBuild(packet, NULL);

		// ハッシュ以降のペイロードを取得する
		after_hash_offset = sizeof(SE_IKE_HEADER) + hash_payload->BitArray->Size + sizeof(SE_IKE_COMMON_HEADER);
		after_hash_size = ((packet_buf->Size > after_hash_offset) ?
			(packet_buf->Size - after_hash_offset) : 0);

		buf_after_hash = SeMemToBuf(((UCHAR *)packet_buf->Buf) + after_hash_offset, after_hash_size);

		SeFreeBuf(packet_buf);

		// ハッシュを計算する
		tmp = SeNewBuf();
		SeWriteBuf(tmp, &msg_id_be, sizeof(msg_id_be));
		SeWriteBufBuf(tmp, buf_after_hash);
		SeMacSha1(hash1, sa->P1KeySet.SKEYID_a->Buf, sa->P1KeySet.SKEYID_a->Size,
			tmp->Buf, tmp->Size);
		SeFreeBuf(tmp);
		SeFreeBuf(buf_after_hash);

		// ハッシュを上書きする
		SeCopy(hash_payload->Payload.Hash.Data->Buf, hash1, sizeof(hash1));

		// 暗号化パケットのビルド
		SeZero(&cparam, sizeof(cparam));
		SeSecCalcP2Iv(cparam.Iv, sa->Phase1Iv, sizeof(sa->Phase1Iv), msg_id, SE_DES_BLOCK_SIZE);
		cparam.DesKey = sa->Phase2DesKey;
		packet->FlagEncrypted = true;
		packet_buf = SeIkeBuild(packet, &cparam);

		sa->TransferBytes += packet->DecryptedPayload->Size;

		SeCopy(sa->Phase2Iv, cparam.NextIv, SE_DES_BLOCK_SIZE);

		// 送信
		SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

		SeFreeBuf(packet_buf);

		SeIkeFree(packet);
	}
}

// IKE SA 情報交換メッセージ受信処理
void SeSecProcessIkeSaMsgInfo(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size)
{
	SE_IKE_PACKET *packet;
	SE_SEC_CONFIG *config;
	SE_IKE_CRYPTO_PARAM cparam;
	// 引数チェック
	if (s == NULL || sa == NULL || packet_header == NULL || data == NULL)
	{
		return;
	}

	config = &s->Config;

	SeZero(&cparam, sizeof(cparam));
	cparam.DesKey = sa->Phase2DesKey;
	SeSecCalcP2Iv(cparam.Iv, sa->Phase1Iv, sizeof(sa->Phase1Iv),
		packet_header->MessageId, sizeof(cparam.Iv));

	packet = SeIkeParse(data, size, &cparam);
	if (packet == NULL)
	{
		return;
	}
	else
	{
		SE_LIST *payload_list;
		SE_IKE_PACKET_PAYLOAD *hash_payload;
		UCHAR *hashed_data;
		UINT hashed_data_size = 0;
		UCHAR hash[SE_SHA1_HASH_SIZE];
		SE_BUF *hash_buf;
		UINT msg_id_be = SeEndian32(packet->MessageId);

		// ペイロードリストの取得
		payload_list = packet->PayloadList;

		// ハッシュペイロードの取得
		hash_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_HASH, 0);
		if (hash_payload != NULL)
		{
			// ハッシュ値を検査
			hashed_data = ((UCHAR *)packet->DecryptedPayload->Buf)
				+ sizeof(SE_IKE_COMMON_HEADER) + hash_payload->BitArray->Size;

			if (packet->DecryptedPayload->Size >= (sizeof(SE_IKE_COMMON_HEADER) + hash_payload->BitArray->Size))
			{
				hashed_data_size = packet->DecryptedPayload->Size
					- (sizeof(SE_IKE_COMMON_HEADER) + hash_payload->BitArray->Size);
			}

			hash_buf = SeNewBuf();
			SeWriteBuf(hash_buf, &msg_id_be, sizeof(msg_id_be));
			SeWriteBuf(hash_buf, hashed_data, hashed_data_size);
			SeMacSha1(hash,
				sa->P1KeySet.SKEYID_a->Buf,
				sa->P1KeySet.SKEYID_a->Size,
				hash_buf->Buf,
				hash_buf->Size);
			SeFreeBuf(hash_buf);

			// ハッシュ値を比較
			if (SeCmpEx(hash, sizeof(hash), hash_payload->Payload.Hash.Data->Buf, sizeof(hash)) == false)
			{
				SeError("IKE: SA #%u: Invalid Message Exchange Packet Hash");
			}
			else
			{
				UINT n;
				sa->TransferBytes += packet->DecryptedPayload->Size;
				sa->LastCommTick = SeSecTick(s);

				// 削除ペイロードの検索
				for (n = 0;;n++)
				{
					SE_IKE_PACKET_PAYLOAD *delete_payload =
						SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_DELETE, n);
					SE_IKE_PACKET_DELETE_PAYLOAD *del;

					if (delete_payload == NULL)
					{
						break;
					}

					del = &delete_payload->Payload.Delete;

					if (del->ProtocolId == SE_IKE_PROTOCOL_ID_IKE)
					{
						// IKE SA の削除
						UINT i;

						for (i = 0;i < SE_LIST_NUM(del->SpiList);i++)
						{
							SE_BUF *spi_buf = SE_LIST_DATA(del->SpiList, i);

							if (spi_buf->Size == sizeof(UINT64) * 2)
							{
								SE_IKE_SA *target_sa = SeSecSearchIkeSaBySpi(s,
									sa->SrcAddr, sa->DestAddr, sa->SrcPort, sa->DestPort,
									spi_buf->Buf);

								if (target_sa != NULL)
								{
									target_sa->DeleteNow = true;
									s->StatusChanged = true;
								}
							}
						}
					}
					else if (del->ProtocolId == SE_IKE_PROTOCOL_ID_IPSEC_ESP)
					{
						// IPsec SA の削除
						UINT i;

						for (i = 0;i < SE_LIST_NUM(del->SpiList);i++)
						{
							SE_BUF *spi_buf = SE_LIST_DATA(del->SpiList, i);

							if (spi_buf->Size == sizeof(UINT))
							{
								UINT spi = *((UINT *)spi_buf->Buf);
								SE_IPSEC_SA *target_sa = SeSecSearchIPsecSaBySpi(s,
									sa->SrcAddr, sa->DestAddr, spi);

								if (target_sa != NULL)
								{
									SeSecFreeIPsecSa(s, target_sa);
								}
							}
						}
					}
				}
			}
		}
	}

	SeIkeFree(packet);
}

// IKE SA フェーズ 2 クイックモード 最終ハッシュメッセージ送信処理
void SeSecSendIkeSaMsgPhase2FinalHash(SE_SEC *s, SE_IKE_SA *sa)
{
	SE_SEC_CONFIG *config;
	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return;
	}

	config = &s->Config;

	// パケットの構築
	if (true)
	{
		SE_LIST *payload_list;
		SE_IKE_PACKET_PAYLOAD *hash_payload;
		SE_BUF *tmp;
		UCHAR c;
		UINT msg_id = SeEndian32(sa->Phase2MessageId);
		UCHAR hash[SE_SHA1_HASH_SIZE];
		SE_IKE_CRYPTO_PARAM cparam;
		SE_IKE_PACKET *packet;
		SE_BUF *packet_buf;

		payload_list = SeNewList(NULL);

		// ハッシュの計算
		tmp = SeNewBuf();
		c = 0;
		SeWriteBuf(tmp, &c, sizeof(c));
		SeWriteBuf(tmp, &msg_id, sizeof(msg_id));
		SeWriteBufBuf(tmp, sa->Phase2MyRand);
		SeWriteBufBuf(tmp, sa->Phase2YourRand);
		SeMacSha1(hash, sa->P1KeySet.SKEYID_a->Buf, sa->P1KeySet.SKEYID_a->Size,
			tmp->Buf, tmp->Size);
		SeFreeBuf(tmp);

		hash_payload = SeIkeNewDataPayload(SE_IKE_PAYLOAD_HASH, hash, sizeof(hash));
		SeAdd(payload_list, hash_payload);

		// パケットの生成
		packet = SeIkeNew(sa->InitiatorCookie, sa->ResponderCookie,
			SE_IKE_EXCHANGE_TYPE_QUICK, true, false, false, sa->Phase2MessageId, payload_list);

		// 暗号化パケットのビルド
		SeZero(&cparam, sizeof(cparam));
		SeCopy(cparam.Iv, sa->Phase2Iv, SE_DES_BLOCK_SIZE);
		cparam.DesKey = sa->Phase2DesKey;
		packet_buf = SeIkeBuild(packet, &cparam);

		// 送信
		sa->TransferBytes += packet->DecryptedPayload->Size;
		SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

		SeFreeBuf(packet_buf);
		SeIkeFree(packet);
	}
}

// IKE SA フェーズ 2 クイックモード 応答受信処理
void SeSecProcessIkeSaMsgPhase2(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size)
{
	SE_IKE_PACKET *packet;
	SE_SEC_CONFIG *config;
	SE_IKE_CRYPTO_PARAM cparam;
	// 引数チェック
	if (s == NULL || sa == NULL || packet_header == NULL || data == NULL)
	{
		return;
	}

	config = &s->Config;

	SeZero(&cparam, sizeof(cparam));
	cparam.DesKey = sa->Phase2DesKey;
	SeCopy(cparam.Iv, sa->Phase2Iv, SE_DES_BLOCK_SIZE);

	packet = SeIkeParse(data, size, &cparam);
	if (packet == NULL)
	{
		return;
	}
	else
	{
		if (sa->Phase2MessageId == packet->MessageId)
		{
			SE_LIST *payload_list;
			SE_IKE_PACKET_PAYLOAD *hash_payload, *sa_payload, *rand_payload,
				*proposal_payload = NULL;

			SeInfo("IKE: SA #%u: Phase 2 Response Received", sa->Id);

			// ペイロードリストの取得
			payload_list = packet->PayloadList;

			// ハッシュペイロードの取得
			hash_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_HASH, 0);

			// SA ペイロードの取得
			sa_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_SA, 0);

			// プロポーザルペイロードの取得
			if (sa_payload != NULL)
			{
				proposal_payload = SeIkeGetPayload(sa_payload->Payload.Sa.PayloadList, SE_IKE_PAYLOAD_PROPOSAL, 0);
			}

			// 乱数ペイロードの取得
			rand_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_RAND, 0);

			if (hash_payload == NULL || sa_payload == NULL || rand_payload == NULL || proposal_payload == NULL)
			{
				SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);
			}
			else
			{
				SE_IKE_PACKET_PROPOSAL_PAYLOAD *proposal = &proposal_payload->Payload.Proposal;
				SE_BUF *spi_buf = proposal->Spi;

				// ゲートウェイから取得した SPI 値をチェック
				if (spi_buf->Size != sizeof(UINT))
				{
					SeError("IKE: SA #%u: Invalid SPI Value", sa->Id);
				}
				else
				{
					UINT your_spi = *((UINT *)spi_buf->Buf);
					char tmp1[MAX_SIZE];
					UCHAR *hashed_data;
					UINT hashed_data_size = 0;
					UCHAR hash[SE_SHA1_HASH_SIZE];
					SE_BUF *hash_buf;
					UINT msg_id_be = SeEndian32(packet->MessageId);

					SeBinToStr(tmp1, sizeof(tmp1), &your_spi, sizeof(your_spi));
					SeInfo("IKE: SA #%u: Responder SPI: 0x%s", sa->Id, tmp1);

					// ハッシュ値を検査
					hashed_data = ((UCHAR *)packet->DecryptedPayload->Buf)
						+ sizeof(SE_IKE_COMMON_HEADER) + hash_payload->BitArray->Size;

					if (packet->DecryptedPayload->Size >= (sizeof(SE_IKE_COMMON_HEADER) + hash_payload->BitArray->Size))
					{
						hashed_data_size = packet->DecryptedPayload->Size
							- (sizeof(SE_IKE_COMMON_HEADER) + hash_payload->BitArray->Size);
					}

					hash_buf = SeNewBuf();
					SeWriteBuf(hash_buf, &msg_id_be, sizeof(msg_id_be));
					SeWriteBufBuf(hash_buf, sa->Phase2MyRand);
					SeWriteBuf(hash_buf, hashed_data, hashed_data_size);
					SeMacSha1(hash,
						sa->P1KeySet.SKEYID_a->Buf,
						sa->P1KeySet.SKEYID_a->Size,
						hash_buf->Buf,
						hash_buf->Size);
					SeFreeBuf(hash_buf);

					// ハッシュ値を比較
					if (SeCmpEx(hash, sizeof(hash), hash_payload->Payload.Hash.Data->Buf, sizeof(hash)) == false)
					{
						SeError("IKE: SA #%u: Invalid Responder Hash");
					}
					else
					{
						SE_IPSEC_SA *ipsec_sa_in, *ipsec_sa_out;

						sa->TransferBytes += packet->DecryptedPayload->Size;

						// KEYMAT の計算
						sa->YourSpi = your_spi;
						sa->Phase2YourRand = SeCloneBuf(rand_payload->Payload.Rand.Data);

						sa->MyKEYMAT = SeSecCalcKEYMAT(sa->P1KeySet.SKEYID_d,
							SE_IKE_PROTOCOL_ID_IPSEC_ESP,
							sa->MySpi,
							sa->Phase2MyRand,
							sa->Phase2YourRand,
							SeIkePhase2CryptIdToKeySize(config->VpnPhase2Crypto) + SE_HMAC_SHA1_96_KEY_SIZE);

						sa->YourKEYMAT = SeSecCalcKEYMAT(sa->P1KeySet.SKEYID_d,
							SE_IKE_PROTOCOL_ID_IPSEC_ESP,
							sa->YourSpi,
							sa->Phase2MyRand,
							sa->Phase2YourRand,
							SeIkePhase2CryptIdToKeySize(config->VpnPhase2Crypto) + SE_HMAC_SHA1_96_KEY_SIZE);

						SeCopy(sa->Phase2Iv, cparam.NextIv, SE_DES_BLOCK_SIZE);

						// IKE SA フェーズ 2 クイックモード 最終ハッシュメッセージ送信処理
						SeSecSendIkeSaMsgPhase2FinalHash(s, sa);

						// IPsec SA の確立
						ipsec_sa_in = SeSecNewIPsecSa(s, sa, false, sa->MySpi,
							sa->SrcAddr, sa->DestAddr, sa->MyKEYMAT);
						ipsec_sa_out =SeSecNewIPsecSa(s, sa, true, sa->YourSpi,
							sa->SrcAddr, sa->DestAddr, sa->YourKEYMAT);

						// VPN 接続の確立完了
						SeInfo("IKE: SA #%u: VPN Connection Established", sa->Id);
						sa->Established = true;
						sa->LastCommTick = SeSecTick(s);
					}
				}
			}
		}
	}

	SeIkeFree(packet);
}

// IPsec SA の解放
void SeSecFreeIPsecSa(SE_SEC *s, SE_IPSEC_SA *sa)
{
	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return;
	}

	SeSecSendIPsecSaDeleteMsg(s, sa, sa->IkeSa);

	sa->IkeSa->DeleteNow = true;
	s->StatusChanged = true;

	SeFreeBuf(sa->EncryptionKey);
	SeFreeBuf(sa->HashKey);
	SeDes3FreeKey(sa->DesKey);

	SeDelete(s->IPsecSaList, sa);

	SeFree(sa);
}

// IPsec SA の確立
SE_IPSEC_SA *SeSecNewIPsecSa(SE_SEC *s, SE_IKE_SA *ike_sa, bool outgoing, UINT spi,
							 SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
							 SE_BUF *keymat)
{
	SE_SEC_CONFIG *config;
	SE_IPSEC_SA *sa;
	// 引数チェック
	if (s == NULL || ike_sa == NULL || keymat == NULL)
	{
		return NULL;
	}

	config = &s->Config;

	sa = SeZeroMalloc(sizeof(SE_IPSEC_SA));
	sa->Outgoing = outgoing;
	sa->Spi = spi;
	SeRand(sa->NextIv, sizeof(sa->NextIv));
	sa->IkeSa = ike_sa;

	sa->SrcAddr = src_addr;
	sa->DestAddr = dest_addr;

	sa->EncryptionKey = SeMemToBuf(((UCHAR *)keymat->Buf), SeIkePhase2CryptIdToKeySize(config->VpnPhase2Crypto));
	sa->HashKey = SeMemToBuf(((UCHAR *)keymat->Buf) + SeIkePhase2CryptIdToKeySize(config->VpnPhase2Crypto), SE_HMAC_SHA1_96_KEY_SIZE);

	if (config->VpnPhase2Crypto == SE_IKE_TRANSFORM_ID_P2_ESP_3DES)
	{
		// 3DES
		sa->DesKey = SeDes3NewKey(
			((UCHAR *)sa->EncryptionKey->Buf) + SE_DES_KEY_SIZE * 0,
			((UCHAR *)sa->EncryptionKey->Buf) + SE_DES_KEY_SIZE * 1,
			((UCHAR *)sa->EncryptionKey->Buf) + SE_DES_KEY_SIZE * 2);
	}
	else
	{
		// DES
		sa->DesKey = SeDesNewKey(sa->EncryptionKey->Buf);
	}

	sa->EstablishedTick = SeSecTick(s);
	if (config->VpnPhase2LifeSeconds != 0)
	{
		SeSecAddTimer(s, (UINT)SeSecLifeSeconds64bit(config->VpnPhase2LifeSeconds));
	}

	SeInsert(s->IPsecSaList, sa);

	return sa;
}

// IKE SA フェーズ 2 クイックモード 要求送信処理
void SeSecSendIkeSaMsgPhase2(SE_SEC *s, SE_IKE_SA *sa)
{
	SE_SEC_CONFIG *config;
	UINT msg_id_buf;
	char tmp1[MAX_SIZE];
	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return;
	}

	config = &s->Config;

	// 新しいメッセージ ID の生成
	sa->Phase2MessageId = SeSecGenIkeSaMessageId(s);
	msg_id_buf = SeEndian32(sa->Phase2MessageId);

	SeInfo("IKE: SA #%u: Phase 2 Started", sa->Id);

	sa->Phase2Started = true;

	// パケットの構築
	if (true)
	{
		UCHAR hash_dummy[SE_SHA1_HASH_SIZE];
		UCHAR hash1[SE_SHA1_HASH_SIZE];
		SE_LIST *payload_list;
		SE_LIST *transform_value_list;
		SE_IKE_PACKET_PAYLOAD *transform_payload;
		SE_LIST *transform_payload_list;
		SE_IKE_PACKET_PAYLOAD *proposal_payload;
		SE_LIST *proposal_payload_list;
		SE_BUF *packet_buf;
		SE_IKE_PACKET *packet;
		SE_IKE_PACKET_PAYLOAD *hash_payload;
		SE_IKE_PACKET_PAYLOAD *sa_payload;
		SE_IKE_PACKET_PAYLOAD *rand_payload;
		SE_IKE_PACKET_PAYLOAD *id_init_payload;
		SE_IKE_PACKET_PAYLOAD *id_resp_payload;
		UINT new_spi;
		SE_BUF *buf_after_hash;
		UINT after_hash_offset;
		UINT after_hash_size;
		SE_BUF *tmp;
		SE_IKE_CRYPTO_PARAM cparam;

		payload_list = SeNewList(NULL);

		// ハッシュペイロードの追加
		SeZero(hash_dummy, sizeof(hash_dummy));
		hash_payload = SeIkeNewDataPayload(SE_IKE_PAYLOAD_HASH, hash_dummy, sizeof(hash_dummy));
		SeAdd(payload_list, hash_payload);

		// トランスフォーム値リストの作成
		transform_value_list = SeNewList(NULL);
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P2_HMAC, config->VpnPhase2Hash));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P2_LIFE_TYPE, SE_IKE_P1_LIFE_TYPE_SECONDS));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P2_LIFE, config->VpnPhase2LifeSeconds));
		if (config->VpnPhase2LifeKilobytes != 0)
		{
			SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P2_LIFE_TYPE, SE_IKE_P1_LIFE_TYPE_KILOBYTES));
			SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P2_LIFE, config->VpnPhase2LifeKilobytes));
		}
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P2_CAPSULE, SE_IKE_P2_CAPSULE_TUNNEL));

		// トランスフォームペイロードの作成
		transform_payload = SeIkeNewTransformPayload(0, config->VpnPhase2Crypto, transform_value_list);

		// トランスフォームペイロードリストの作成
		transform_payload_list = SeNewList(NULL);
		SeAdd(transform_payload_list, transform_payload);

		// プロポーザルペイロードの作成
		new_spi = SeSecGenIPsecSASpi(s);
		proposal_payload = SeIkeNewProposalPayload(0, SE_IKE_PROTOCOL_ID_IPSEC_ESP, &new_spi, sizeof(UINT), transform_payload_list);

		// 自分の SPI をコピーしておく
		sa->MySpi = new_spi;

		SeBinToStr(tmp1, sizeof(tmp1), &new_spi, sizeof(new_spi));
		SeInfo("IKE: SA #%u: Initiator SPI: 0x%s", sa->Id, tmp1);

		// プロポーザルペイロードリストの作成
		proposal_payload_list = SeNewList(NULL);
		SeAdd(proposal_payload_list, proposal_payload);

		// SA ペイロードの作成
		sa_payload = SeIkeNewSaPayload(proposal_payload_list);
		SeAdd(payload_list, sa_payload);

		// 乱数ペイロードの作成
		sa->Phase2MyRand = SeRandBuf(SE_SHA1_HASH_SIZE);
		rand_payload = SeIkeNewDataPayload(SE_IKE_PAYLOAD_RAND, sa->Phase2MyRand->Buf, sa->Phase2MyRand->Size);
		SeAdd(payload_list, rand_payload);

		// ID ペイロードの作成
		if (s->IPv6 == false)
		{
			// IPv4
			UINT myip_32 = 0;
			UCHAR zero_buffer[8];

			// 接続元ホストが送出する VPN 内での IP アドレス
			myip_32 = Se4IPToUINT(SeIkeGetIPv4Address(&config->MyVirtualIpAddress));
			id_init_payload = SeIkeNewIdPayload(SE_IKE_ID_IPV4_ADDR, 0, 0, &myip_32, sizeof(myip_32));

			// 接続先 VPN ゲートウェイから受け取る IP アドレス (制限しない)
			SeZero(zero_buffer, sizeof(zero_buffer));
			id_resp_payload = SeIkeNewIdPayload(SE_IKE_ID_IPV4_ADDR_SUBNET, 0, 0, zero_buffer, sizeof(zero_buffer));
		}
		else
		{
			// IPv6
			UCHAR zero_buffer[sizeof(SE_IPV6_ADDR) * 2];

			// 接続元ホストが送出する VPN 内での IP アドレス
			id_init_payload = SeIkeNewIdPayload(SE_IKE_ID_IPV6_ADDR, 0, 0,
				&config->MyVirtualIpAddress.Address.Ipv6, sizeof(SE_IPV6_ADDR));

			// 接続先 VPN ゲートウェイから受け取る IP アドレス (制限しない)
			SeZero(zero_buffer, sizeof(zero_buffer));
			id_resp_payload = SeIkeNewIdPayload(SE_IKE_ID_IPV6_ADDR_SUBNET, 0, 0, zero_buffer, sizeof(zero_buffer));
		}
		SeAdd(payload_list, id_init_payload);
		SeAdd(payload_list, id_resp_payload);

		// ハッシュ計算用の一時的なパケットを作成する
		packet = SeIkeNew(sa->InitiatorCookie, sa->ResponderCookie,
			SE_IKE_EXCHANGE_TYPE_QUICK, false, false, false,
			sa->Phase2MessageId, payload_list);

		packet_buf = SeIkeBuild(packet, NULL);

		// ハッシュ以降のペイロードを取得する
		after_hash_offset = sizeof(SE_IKE_HEADER) + hash_payload->BitArray->Size + sizeof(SE_IKE_COMMON_HEADER);
		after_hash_size = ((packet_buf->Size > after_hash_offset) ?
			(packet_buf->Size - after_hash_offset) : 0);

		buf_after_hash = SeMemToBuf(((UCHAR *)packet_buf->Buf) + after_hash_offset, after_hash_size);

		SeFreeBuf(packet_buf);

		// ハッシュを計算する
		tmp = SeNewBuf();
		SeWriteBuf(tmp, &msg_id_buf, sizeof(msg_id_buf));
		SeWriteBufBuf(tmp, buf_after_hash);
		SeMacSha1(hash1, sa->P1KeySet.SKEYID_a->Buf, sa->P1KeySet.SKEYID_a->Size,
			tmp->Buf, tmp->Size);
		SeFreeBuf(tmp);
		SeFreeBuf(buf_after_hash);

		// ハッシュを上書きする
		SeCopy(hash_payload->Payload.Hash.Data->Buf, hash1, sizeof(hash1));

		// 暗号化パケットのビルド
		SeZero(&cparam, sizeof(cparam));
		SeSecCalcP2Iv(cparam.Iv, sa->Phase1Iv, sizeof(sa->Phase1Iv), sa->Phase2MessageId, SE_DES_BLOCK_SIZE);
		cparam.DesKey = sa->Phase2DesKey;
		packet->FlagEncrypted = true;
		packet_buf = SeIkeBuild(packet, &cparam);

		sa->TransferBytes += packet->DecryptedPayload->Size;

		SeCopy(sa->Phase2Iv, cparam.NextIv, SE_DES_BLOCK_SIZE);

		// 送信
		SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

		SeFreeBuf(packet_buf);

		SeIkeFree(packet);
	}
}

// IKE SA フェーズ 1 アグレッシブモード 応答受信処理
void SeSecRecvAggr(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size)
{
	SE_IKE_PACKET *packet;
	SE_SEC_CONFIG *config;
	// 引数チェック
	if (s == NULL || sa == NULL || data == NULL)
	{
		return;
	}

	config = &s->Config;

	packet = SeIkeParse(data, size, NULL);
	if (packet == NULL)
	{
		return;
	}
	else
	{
		char tmp[MAX_PATH];
		SE_LIST *payload_list;
		SE_IKE_PACKET_PAYLOAD *sa_payload, *key_payload, *rand_payload,
			*id_payload, *hash_payload = NULL, *sign_payload = NULL;

		sa->TransferBytes += packet->DecryptedPayload->Size;

		SeInfo("IKE: SA #%u: Phase 1 Response Recvied", sa->Id);

		SeBinToStr(tmp, sizeof(tmp), &packet->ResponderCookie, sizeof(packet->ResponderCookie));
		SeInfo("IKE: SA #%u: Responser Cookie: 0x%s", sa->Id, tmp);

		// ペイロードリストの取得
		payload_list = packet->PayloadList;

		// SA ペイロードの取得
		sa_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_SA, 0);

		// 鍵交換ペイロードの取得
		key_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_KEY_EXCHANGE, 0);

		// 乱数ペイロードの取得
		rand_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_RAND, 0);

		// ID ペイロードの取得
		id_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_ID, 0);

		if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_PASSWORD)
		{
			// ハッシュペイロードの取得
			sign_payload = hash_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_HASH, 0);
		}
		else
		{
			// 署名ペイロードの取得
			sign_payload = hash_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_SIGN, 0);
		}

		// ペイロードが揃っているかどうか取得
		if (payload_list == NULL || sa_payload == NULL || key_payload == NULL ||
			rand_payload == NULL || id_payload == NULL || hash_payload == NULL)
		{
			SeError("IKE: SA #%u: Invalid Response Payload", sa->Id);
		}
		else
		{
			// DH 共有鍵の計算
			SE_BUF *key_buf = key_payload->Payload.KeyExchange.Data;

			if (key_buf->Size != SE_DH_KEY_SIZE)
			{
				// 鍵サイズ不正
				SeError("IKE: SA #%u: Invalid DH Key Size: %u", sa->Id, key_buf->Size);
			}
			else
			{
				UCHAR private_key[SE_DH_KEY_SIZE];

				if (SeDhCompute(sa->Dh, private_key, key_buf->Buf, key_buf->Size) == false)
				{
					SeError("IKE: SA #%u: DH Computing Key Failed", sa->Id);
				}
				else
				{
					// SKEYID の計算
					UCHAR skeyid[SE_SHA1_HASH_SIZE];
					SE_IKE_P1_KEYSET key_set;
					SE_BUF *idir_b;
					UCHAR hash_init[SE_SHA1_HASH_SIZE], hash_resp[SE_SHA1_HASH_SIZE];
					SE_BUF *b;
					bool ok = true;

					if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_PASSWORD)
					{
						// パスワード認証
						SE_BUF *tmp_buf = SeNewBuf();

						SeWriteBufBuf(tmp_buf, sa->Phase1MyRand);
						SeWriteBufBuf(tmp_buf, rand_payload->Payload.Rand.Data);

						SeMacSha1(skeyid, sa->Phase1Password->Buf, sa->Phase1Password->Size,
							tmp_buf->Buf, tmp_buf->Size);

						SeFreeBuf(tmp_buf);
					}
					else if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_CERT)
					{
						// 証明書認証
						SE_BUF *tmp_buf = SeNewBuf();

						SeWriteBufBuf(tmp_buf, sa->Phase1MyRand);
						SeWriteBufBuf(tmp_buf, rand_payload->Payload.Rand.Data);

						SeMacSha1(skeyid, tmp_buf->Buf, tmp_buf->Size,
							private_key, sizeof(private_key));

						SeFreeBuf(tmp_buf);
					}

					// 鍵セットの計算
					SeSecCalcP1KeySet(&key_set, skeyid, sizeof(skeyid), private_key, sizeof(private_key),
						sa->InitiatorCookie, packet->ResponderCookie,
						SeIkePhase1CryptIdToKeySize(config->VpnPhase1Crypto));

					// IDir_b の取得
					idir_b = id_payload->BitArray;

					// イニシエータハッシュ値の計算
					b = SeNewBuf();
					SeWriteBufBuf(b, sa->Dh->MyPublicKey);
					SeWriteBufBuf(b, key_buf);
					SeWriteBuf(b, &sa->InitiatorCookie, sizeof(sa->InitiatorCookie));
					SeWriteBuf(b, &packet->ResponderCookie, sizeof(packet->ResponderCookie));
					SeWriteBufBuf(b, sa->SAi_b);
					SeWriteBufBuf(b, sa->IDii_b);
					SeMacSha1(hash_init, skeyid, sizeof(skeyid), b->Buf, b->Size);
					SeFreeBuf(b);

					// レスポンダハッシュ値の計算
					b = SeNewBuf();
					SeWriteBufBuf(b, key_buf);
					SeWriteBufBuf(b, sa->Dh->MyPublicKey);
					SeWriteBuf(b, &packet->ResponderCookie, sizeof(packet->ResponderCookie));
					SeWriteBuf(b, &sa->InitiatorCookie, sizeof(sa->InitiatorCookie));
					SeWriteBufBuf(b, sa->SAi_b);
					SeWriteBufBuf(b, idir_b);
					SeMacSha1(hash_resp, skeyid, sizeof(skeyid), b->Buf, b->Size);
					SeFreeBuf(b);

					if ((config->VpnAuthMethod == SE_SEC_AUTH_METHOD_PASSWORD) &&
						(SeCmpEx(hash_resp, sizeof(hash_resp),
						hash_payload->Payload.Hash.Data->Buf,
						hash_payload->Payload.Hash.Data->Size) == false))
					{
						// レスポンダハッシュ値の比較
						SeError("IKE: SA #%u: Phase 1 Auth Failed (Invalid Responder Hash)", sa->Id);

						SeSecFreeP1KeySet(&key_set);

						ok = false;
					}

					if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_CERT)
					{
						SE_IKE_PACKET_PAYLOAD *cert_payload;

						// 接続先が提示した証明書の取得
						cert_payload = SeIkeGetPayload(payload_list, SE_IKE_PAYLOAD_CERT, 0);

						if (cert_payload == NULL || cert_payload->Payload.Cert.CertType != SE_IKE_CERT_TYPE_X509)
						{
							// 証明書ペイロードが無い
							SeError("IKE: SA #%u: Responder Cert Payload Not Found", sa->Id);
							SeSecFreeP1KeySet(&key_set);
							ok = false;
						}
						else
						{
							SE_CERT *cert;
							bool is_trusted = true;

							// 証明書を取り出す
							cert = SeBufToCert(cert_payload->Payload.Cert.CertData, false);

							if (false)
							{
								// デバッグ用
								SE_BUF *buf;
								buf = SeCertToBuf(cert, true);
								SeSeekBuf(buf, buf->Size, 0);
								SeWriteBufInt(buf, 0);
								SeError("%s\n", buf->Buf);
								SeFreeBuf(buf);
							}

							if (cert == NULL)
							{
								// 証明書が不正
								SeError("IKE: SA #%u: Invalid Responder Cert", sa->Id);
								SeSecFreeP1KeySet(&key_set);
								ok = false;
							}
							else
							{
								if (sa->CaCert != NULL)
								{
									// 接続先 VPN サーバーの証明書を検証するための CA 証明書が
									// 指定されている場合は、CA 証明書を用いて VPN サーバーの証明書
									// が信頼できるかどうかを検証する
									is_trusted = SeIsCertSignedByCert(cert, sa->CaCert);
								}

								if (is_trusted == false)
								{
									// 接続先の証明書は信頼できない
									SeError("IKE: SA #%u: Untrusted Responder Cert", sa->Id);
									SeSecFreeP1KeySet(&key_set);
									ok = false;
								}
								else
								{
									// 公開鍵の取得
									SE_KEY *key = SeGetKeyFromCert(cert);

									if (key == NULL)
									{
										// 公開鍵が不正
										SeError("IKE: SA #%u: Invalid Responder Cert", sa->Id);
										SeSecFreeP1KeySet(&key_set);
										ok = false;
									}
									else
									{
										if (SeRsaVerifyWithPadding(hash_resp, sizeof(hash_resp),
											sign_payload->Payload.Rand.Data->Buf,
											sign_payload->Payload.Rand.Data->Size,
											key) == false)
										{
											// 署名が不正
											SeError("IKE: SA #%u: Invalid Responder RSA Signature", sa->Id);
											SeSecFreeP1KeySet(&key_set);
											ok = false;
										}
									}
									SeFreeKey(key);
								}
								SeFreeCert(cert);
							}
						}
					}

					if (ok)
					{
						// 返送パケットの作成
						SE_LIST *payload_list;
						SE_IKE_PACKET_PAYLOAD *hash_payload = NULL;
						SE_IKE_PACKET *resp_packet;
						SE_BUF *packet_buf;

						payload_list = SeNewList(NULL);

						if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_PASSWORD)
						{
							// パスワード認証
							hash_payload = SeIkeNewDataPayload(SE_IKE_PAYLOAD_HASH,
								hash_init, sizeof(hash_init));
							SeAdd(payload_list, hash_payload);
						}
						else if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_CERT)
						{
							// 証明書認証
							SE_IKE_PACKET_PAYLOAD *cert_payload;
							SE_BUF *cert_buf;
							SE_BUF *sign_buf;

							cert_buf = SeCertToBuf(sa->MyCert, false);

							// 証明書ペイロード
							cert_payload = SeIkeNewCertPayload(SE_IKE_CERT_TYPE_X509,
								cert_buf->Buf, cert_buf->Size);

							SeFreeBuf(cert_buf);

							SeAdd(payload_list, cert_payload);

							// 署名の実施
							sign_buf = SeRsaSign(config->VpnRsaKeyName, hash_init, sizeof(hash_init));

							if (sign_buf == NULL)
							{
								// 署名に失敗
								SeError("IKE: SA #%u: RSA Sign Failed", sa->Id);
								ok = false;
							}
							else
							{
								SE_IKE_PACKET_PAYLOAD *sign_payload;

								// 署名ペイロードの追加
								sign_payload = SeIkeNewDataPayload(SE_IKE_PAYLOAD_SIGN,
									sign_buf->Buf, sign_buf->Size);
								SeAdd(payload_list, sign_payload);

								SeFreeBuf(sign_buf);
							}
						}

						if (ok == false)
						{
							SeSecFreeP1KeySet(&key_set);
							SeIkeFreePayloadList(payload_list);
						}
						else
						{
							// 状態遷移
							sa->ResponderCookie = packet->ResponderCookie;
							sa->P1KeySet = key_set;
							sa->Phase = 1;
							sa->Phase1EstablishedTick = SeSecTick(s);

							if (config->VpnPhase1LifeSeconds != 0)
							{
								SeSecAddTimer(s, (UINT)SeSecLifeSeconds64bit(config->VpnPhase1LifeSeconds));
							}

							resp_packet = SeIkeNew(sa->InitiatorCookie, sa->ResponderCookie,
								SE_IKE_EXCHANGE_TYPE_AGGRESSIVE, false, false, false, 0,
								payload_list);

							// パケット構築
							packet_buf = SeIkeBuild(resp_packet, NULL);

							// 送信
							SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

							SeFreeBuf(packet_buf);
							SeIkeFree(resp_packet);

							// フェーズ 1 IV の計算
							SeSecCalcP1Iv(sa->Phase1Iv,
								sa->Dh->MyPublicKey->Buf,
								sa->Dh->MyPublicKey->Size,
								key_buf->Buf,
								key_buf->Size,
								sizeof(sa->Phase1Iv));

							// フェーズ 2 で使用する DES 鍵の計算
							if (config->VpnPhase1Crypto == SE_IKE_P1_CRYPTO_DES_CBC)
							{
								// DES
								UCHAR *p_key = (UCHAR *)key_set.SKEYID_e->Buf;

								sa->Phase2DesKey = SeDesNewKey(p_key);
							}
							else
							{
								// 3DES
								UCHAR *p_key = (UCHAR *)key_set.SKEYID_e->Buf;

								sa->Phase2DesKey = SeDes3NewKey(
									p_key + SE_DES_KEY_SIZE * 0,
									p_key + SE_DES_KEY_SIZE * 1,
									p_key + SE_DES_KEY_SIZE * 2);
							}

							if (true)
							{
								// フェーズ 2 の開始を準備
								UINT interval = config->VpnWaitPhase2BlankSpan;
								UINT64 start_tick = SeSecTick(s) + (UINT64)interval;

								sa->Phase2StartTick = start_tick;
								SeSecAddTimer(s, interval);
							}
						}
					}
				}
			}
		}
	}

	SeIkeFree(packet);
}

void SeSecProcessIkeSaMsgPhase1(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size)
{
	if (s->Config.VpnPhase1Mode == SE_IKE_EXCHANGE_TYPE_MAIN)
	{
		if (sa->Status == 1)
		{
			SeSecRecvMain2(s, sa, data, size);
		}
		else if (sa->Status == 3)
		{
			SeSecRecvMain4(s, sa, data, size);
		}
		else if (sa->Status == 5)
		{
			SeSecRecvMain6(s, sa, data, size);
		}
	}
	else if ((s->Config.VpnPhase1Mode == SE_IKE_EXCHANGE_TYPE_AGGRESSIVE))
	{
		SeSecRecvAggr(s, sa, data, size);
	}
}

// フェーズ 2 用 IV の計算
void SeSecCalcP2Iv(void *iv, void *p1_iv, UINT p1_iv_size, UINT msg_id, UINT request_size)
{
	SE_BUF *b;
	UCHAR hash[SE_SHA1_HASH_SIZE];
	// 引数チェック
	if (iv == NULL || p1_iv == NULL)
	{
		return;
	}

	msg_id = SeEndian32(msg_id);

	b = SeNewBuf();
	SeWriteBuf(b, p1_iv, p1_iv_size);
	SeWriteBuf(b, &msg_id, sizeof(msg_id));

	SeSha1(hash, b->Buf, b->Size);
	SeFreeBuf(b);

	SeCopy(iv, hash, MIN(sizeof(hash), request_size));
}

// フェーズ 1 用 IV の計算
void SeSecCalcP1Iv(void *iv, void *g_xi, UINT g_xi_size, void *g_xr, UINT g_xr_size, UINT request_size)
{
	SE_BUF *b;
	UCHAR hash[SE_SHA1_HASH_SIZE];
	// 引数チェック
	if (iv == NULL || g_xi == NULL || g_xr == NULL)
	{
		return;
	}

	b = SeNewBuf();
	SeWriteBuf(b, g_xi, g_xi_size);
	SeWriteBuf(b, g_xr, g_xr_size);

	SeSha1(hash, b->Buf, b->Size);
	SeFreeBuf(b);

	SeCopy(iv, hash, MIN(sizeof(hash), request_size));
}

// フェーズ 1 用鍵セットの計算
void SeSecCalcP1KeySet(SE_IKE_P1_KEYSET *set, void *skeyid, UINT skeyid_size, void *dh_key, UINT dh_key_size, UINT64 init_cookie, UINT64 resp_cookie,
					   UINT request_e_key_size)
{
	UCHAR hash_d[SE_SHA1_HASH_SIZE];
	UCHAR hash_a[SE_SHA1_HASH_SIZE];
	UCHAR hash_e[SE_SHA1_HASH_SIZE];
	SE_BUF *buf;
	UCHAR c;
	// 引数チェック
	if (set == NULL || dh_key == NULL || skeyid == NULL)
	{
		return;
	}

	SeZero(set, sizeof(SE_IKE_P1_KEYSET));

	// SKEYID_d の計算
	buf = SeNewBuf();
	SeWriteBuf(buf, dh_key, dh_key_size);
	SeWriteBuf(buf, &init_cookie, sizeof(init_cookie));
	SeWriteBuf(buf, &resp_cookie, sizeof(resp_cookie));
	c = 0;
	SeWriteBuf(buf, &c, sizeof(c));
	SeMacSha1(hash_d, skeyid, skeyid_size, buf->Buf, buf->Size);
	SeFreeBuf(buf);

	// SKEYID_a の計算
	buf = SeNewBuf();
	SeWriteBuf(buf, hash_d, sizeof(hash_d));
	SeWriteBuf(buf, dh_key, dh_key_size);
	SeWriteBuf(buf, &init_cookie, sizeof(init_cookie));
	SeWriteBuf(buf, &resp_cookie, sizeof(resp_cookie));
	c = 1;
	SeWriteBuf(buf, &c, sizeof(c));
	SeMacSha1(hash_a, skeyid, skeyid_size, buf->Buf, buf->Size);
	SeFreeBuf(buf);

	// SKEYID_e の計算
	buf = SeNewBuf();
	SeWriteBuf(buf, hash_a, sizeof(hash_d));
	SeWriteBuf(buf, dh_key, dh_key_size);
	SeWriteBuf(buf, &init_cookie, sizeof(init_cookie));
	SeWriteBuf(buf, &resp_cookie, sizeof(resp_cookie));
	c = 2;
	SeWriteBuf(buf, &c, sizeof(c));
	SeMacSha1(hash_e, skeyid, skeyid_size, buf->Buf, buf->Size);
	SeFreeBuf(buf);

	// 結果のまとめ
	set->SKEYID_d = SeMemToBuf(hash_d, sizeof(hash_d));
	set->SKEYID_a = SeMemToBuf(hash_a, sizeof(hash_a));
	set->SKEYID_e = SeSecCalcKa(hash_e, sizeof(hash_e), request_e_key_size);
}

// KEYMAT の計算
SE_BUF *SeSecCalcKEYMAT(SE_BUF *skeyid_d, UCHAR protocol, UINT spi,
						SE_BUF *my_rand, SE_BUF *your_rand, UINT request_size)
{
	SE_BUF *b;
	SE_BUF *ret;
	// 引数チェック
	if (skeyid_d == NULL || my_rand == NULL || your_rand == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();
	SeWriteBuf(b, &protocol, sizeof(protocol));
	SeWriteBuf(b, &spi, sizeof(spi));
	SeWriteBufBuf(b, my_rand);
	SeWriteBufBuf(b, your_rand);

	ret = SeSecCalcKEYMATFull(skeyid_d, b->Buf, b->Size, request_size);

	SeFreeBuf(b);

	return ret;
}

// KEYMAT の計算 (サイズの拡張)
SE_BUF *SeSecCalcKEYMATFull(SE_BUF *skeyid_d, void *keymat_src, UINT keymat_src_size, UINT request_size)
{
	UCHAR k[SE_SHA1_HASH_SIZE];
	UINT i;
	SE_BUF *b, *ret;
	// 引数チェック
	if (keymat_src == NULL || skeyid_d == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();

	i = 0;

	while (b->Size < request_size)
	{
		if (i == 0)
		{
			SeMacSha1(k, skeyid_d->Buf, skeyid_d->Size, keymat_src, keymat_src_size);
		}
		else
		{
			SE_BUF *b2 = SeNewBuf();

			SeWriteBuf(b2, k, sizeof(k));
			SeWriteBuf(b2, keymat_src, keymat_src_size);

			SeMacSha1(k, skeyid_d->Buf, skeyid_d->Size, b2->Buf, b2->Size);

			SeFreeBuf(b2);
		}

		i++;

		SeWriteBuf(b, k, sizeof(k));
	}

	ret = SeMemToBuf(b->Buf, request_size);

	SeFreeBuf(b);

	return ret;
}

// Ka の計算
SE_BUF *SeSecCalcKa(void *skeyid_e, UINT skeyid_e_size, UINT request_size)
{
	UCHAR k[SE_SHA1_HASH_SIZE];
	UCHAR c = 0;
	UINT i;
	SE_BUF *b, *ret;
	// 引数チェック
	if (skeyid_e == NULL)
	{
		return NULL;
	}

	if (skeyid_e_size >= request_size)
	{
		b = SeMemToBuf(skeyid_e, request_size);

		return b;
	}

	b = SeNewBuf();

	i = 0;
	while (b->Size < request_size)
	{
		if (i == 0)
		{
			SeMacSha1(k, skeyid_e, skeyid_e_size, &c, sizeof(c));
		}
		else
		{
			SeMacSha1(k, skeyid_e, skeyid_e_size, k, sizeof(k));
		}

		i++;

		SeWriteBuf(b, k, sizeof(k));
	}

	ret = SeMemToBuf(b->Buf, request_size);

	SeFreeBuf(b);

	return ret;
}

// IKE SA メッセージ処理
void SeSecProcessIkeSaMsg(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size)
{
	// 引数チェック
	if (s == NULL || sa == NULL || packet_header == NULL || data == NULL)
	{
		return;
	}

	if (sa->Phase == 0)
	{
		// フェーズ 1 アグレッシブモード or main mode 応答受信処理
		SeSecProcessIkeSaMsgPhase1(s, sa, packet_header, data, size);
	}
	else if (sa->Phase == 1)
	{
		if (sa->Established == false)
		{
			if (packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_QUICK)
			{
				// フェーズ 2 クイックモード応答受信処理
				SeSecProcessIkeSaMsgPhase2(s, sa, packet_header, data, size);
			}
		}
		else
		{
			if (packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_INFORMATION)
			{
				// 情報交換受信処理
				SeSecProcessIkeSaMsgInfo(s, sa, packet_header, data, size);
			}
		}
	}
}

//Aggressive mode
bool SeSecSendAggr(SE_SEC *s)
{
	SE_IKE_SA *sa;
	SE_SEC_CONFIG *config;
	char tmp1[MAX_SIZE], tmp2[MAX_SIZE];
	bool ok = true;
	UINT vpn_connect_timeout_interval;
	// 引数チェック
	if (s == NULL)
	{
		return false;
	}

	config = &s->Config;

	if (s->SendStrictIdV6 && s->IPv6)
	{
		if (SeIkeIsZeroIP(&config->MyVirtualIpAddress))
		{
			// 自分の使用すべき仮想 IP アドレスが指定されていない
			return false;
		}
	}

	SeIkeIpAddressToStr(tmp1, &s->Config.VpnGatewayAddress);
	SeIkeIpAddressToStr(tmp2, &s->Config.MyIpAddress);
	SeInfo("IKE: VPN Connect Start: %s -> %s", tmp2, tmp1);

	// IKE SA の作成
	sa = SeSecNewIkeSa(s, s->Config.MyIpAddress, s->Config.VpnGatewayAddress,
		SE_SEC_IKE_UDP_PORT, SE_SEC_IKE_UDP_PORT, SeSecGenIkeSaInitCookie(s));

	SeInfo("IKE: SA #%u Created", sa->Id);

	SeBinToStr(tmp1, sizeof(tmp1), &sa->InitiatorCookie, sizeof(sa->InitiatorCookie));
	SeInfo("IKE: SA #%u: Initiator Cookie: 0x%s", sa->Id, tmp1);

	// DH 作成
	sa->Dh = SeDhNewGroup2();

	// 乱数生成
	sa->Phase1MyRand = SeRandBuf(SE_SHA1_HASH_SIZE);

	vpn_connect_timeout_interval = s->Config.VpnConnectTimeout * 1000;
	sa->ConnectTimeoutTick = SeSecTick(s) + (UINT64)(vpn_connect_timeout_interval);

	SeSecAddTimer(s, vpn_connect_timeout_interval);

	// パケットの構築
	if (true)
	{
		SE_LIST *payload_list;
		SE_LIST *transform_value_list;
		SE_IKE_PACKET_PAYLOAD *transform_payload;
		SE_LIST *transform_payload_list;
		SE_LIST *proposal_payload_list;
		SE_IKE_PACKET_PAYLOAD *proposal_payload;
		SE_BUF *packet_buf;
		SE_IKE_PACKET *packet;
		SE_IKE_PACKET_PAYLOAD *id_payload;
		SE_IKE_PACKET_PAYLOAD *sa_payload;
		UINT auth_method = 0;

		payload_list = SeNewList(NULL);

		// 認証方法
		switch (config->VpnAuthMethod)
		{
		case SE_SEC_AUTH_METHOD_PASSWORD:
			// パスワード認証
			auth_method = SE_IKE_P1_AUTH_METHOD_PRESHAREDKEY;
			break;

		case SE_SEC_AUTH_METHOD_CERT:
			// 証明書認証
			auth_method = SE_IKE_P1_AUTH_METHOD_RSA_SIGN;
			break;
		}

		// トランスフォーム値リストの作成
		transform_value_list = SeNewList(NULL);
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_CRYPTO, config->VpnPhase1Crypto));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_HASH, config->VpnPhase1Hash));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_AUTH_METHOD, auth_method));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_DH_GROUP, SE_IKE_P1_DH_GROUP_1024_MODP));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_TYPE, SE_IKE_P1_LIFE_TYPE_SECONDS));
		SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_VALUE, config->VpnPhase1LifeSeconds));
		if (config->VpnPhase1LifeKilobytes != 0)
		{
			SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_TYPE, SE_IKE_P1_LIFE_TYPE_KILOBYTES));
			SeAdd(transform_value_list, SeIkeNewTransformValue(SE_IKE_TRANSFORM_VALUE_P1_LIFE_VALUE, config->VpnPhase1LifeKilobytes));
		}

		// トランスフォームペイロードの作成
		transform_payload = SeIkeNewTransformPayload(0, SE_IKE_TRANSFORM_ID_P1_KEY_IKE, transform_value_list);

		// トランスフォームペイロードリストの作成
		transform_payload_list = SeNewList(NULL);
		SeAdd(transform_payload_list, transform_payload);

		// プロポーザルペイロードの作成
		proposal_payload = SeIkeNewProposalPayload(0, SE_IKE_PROTOCOL_ID_IKE, NULL, 0, transform_payload_list);

		// プロポーザルペイロードリストの作成
		proposal_payload_list = SeNewList(NULL);
		SeAdd(proposal_payload_list, proposal_payload);

		// SA ペイロードの追加
		sa_payload = SeIkeNewSaPayload(proposal_payload_list);
		SeAdd(payload_list, sa_payload);

		// 鍵交換ペイロードの追加
		SeAdd(payload_list, SeIkeNewDataPayload(SE_IKE_PAYLOAD_KEY_EXCHANGE,
			sa->Dh->MyPublicKey->Buf, sa->Dh->MyPublicKey->Size));

		// 乱数ペイロードの追加
		SeAdd(payload_list, SeIkeNewDataPayload(SE_IKE_PAYLOAD_RAND,
			sa->Phase1MyRand->Buf, sa->Phase1MyRand->Size));

		if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_PASSWORD)
		{
			// パスワード認証の場合

			// ID ペイロードの追加
			id_payload = SeIkeNewIdPayload(SE_IKE_ID_USER_FQDN, SE_IKE_ID_PROTOCOL_UDP,
				SE_SEC_IKE_UDP_PORT, config->VpnIdString, SeStrLen(config->VpnIdString));

			// パスワードバッファの作成
			sa->Phase1Password = SeIkeStrToPassword(config->VpnPassword);

			SeAdd(payload_list, id_payload);
		}
		else if (config->VpnAuthMethod == SE_SEC_AUTH_METHOD_CERT)
		{
			// 証明書認証の場合
			SE_BUF *cert_subject;
			SE_IKE_PACKET_PAYLOAD *cert_request_payload;

			// 証明書の読み込み
			sa->MyCert = SeLoadCert(config->VpnCertName);
			if (sa->MyCert == NULL)
			{
				// 証明書の読み込みに失敗
				SeError("IKE: SA #%u: Load Cert File \"%s\" Failed", sa->Id, config->VpnCertName);
				SeIkeFreePayloadList(payload_list);

				goto LABEL_ERROR;
			}

			if (SeIsEmptyStr(config->VpnCaCertName) == false)
			{
				sa->CaCert = SeLoadCert(config->VpnCaCertName);
				if (sa->CaCert == NULL)
				{
					// 証明書の読み込みに失敗
					SeError("IKE: SA #%u: Load Cert File \"%s\" Failed", sa->Id, config->VpnCaCertName);
					SeIkeFreePayloadList(payload_list);
					SeFreeCert(sa->MyCert);
					sa->MyCert = NULL;

					goto LABEL_ERROR;
				}
			}
			else
			{
				sa->CaCert = NULL;
			}

			cert_subject = SeGetCertSubjectName(sa->MyCert);

			// ID ペイロードの追加
			id_payload = SeIkeNewIdPayload(SE_IKE_ID_DER_ASN1_DN, SE_IKE_ID_PROTOCOL_UDP,
				SE_SEC_IKE_UDP_PORT, cert_subject->Buf, cert_subject->Size);

			SeAdd(payload_list, id_payload);

			SeFreeBuf(cert_subject);

			// 証明書要求ペイロードの追加
			if (s->Config.VpnSpecifyIssuer == false)
			{
				cert_request_payload = SeIkeNewCertRequestPayload(SE_IKE_CERT_TYPE_X509, NULL, 0);
			}
			else
			{
				SE_BUF *issuer = SeGetCertIssuerName(sa->MyCert);

				cert_request_payload = SeIkeNewCertRequestPayload(SE_IKE_CERT_TYPE_X509, issuer->Buf, issuer->Size);

				SeFreeBuf(issuer);
			}

			SeAdd(payload_list, cert_request_payload);
		}

		// ベンダ ID ペイロードの追加
		SeAdd(payload_list, SeIkeNewDataPayload(SE_IKE_PAYLOAD_VENDOR_ID,
			SE_SEC_VENDOR_ID_STR, SeStrLen(SE_SEC_VENDOR_ID_STR)));

		// IKE パケットのビルド
		packet = SeIkeNew(sa->InitiatorCookie, 0, SE_IKE_EXCHANGE_TYPE_AGGRESSIVE,
			false, false, false, 0,
			payload_list);

		packet_buf = SeIkeBuild(packet, NULL);

		// ペイロードのコピーをとっておく
		sa->IDii_b = SeCloneBuf(id_payload->BitArray);
		sa->SAi_b = SeCloneBuf(sa_payload->BitArray);

		sa->TransferBytes += packet->DecryptedPayload->Size;

		// 送信
		SeSecSendUdp(s, &sa->DestAddr, &sa->SrcAddr, sa->DestPort, sa->SrcPort, packet_buf->Buf, packet_buf->Size);

		SeFreeBuf(packet_buf);
		SeIkeFree(packet);
	}

	if (ok == false)
	{
		UINT interval;

LABEL_ERROR:

		interval = s->Config.VpnConnectTimeout * 1000;

		// 中断
		SeSecFreeIkeSa(s, sa);

		s->NextConnectStartTick = SeSecTick(s) + (UINT64)interval;
		SeSecAddTimer(s, interval);
	}

	return ok;
}

// VPN 接続の開始
bool SeSecStartVpnConnect(SE_SEC *s)
{
	if (s->Config.VpnPhase1Mode == SE_IKE_EXCHANGE_TYPE_MAIN)
	{
		return SeSecSendMain1(s);
	}
	else if (s->Config.VpnPhase1Mode == SE_IKE_EXCHANGE_TYPE_AGGRESSIVE)
	{
		return SeSecSendAggr(s);
	}
	return false;
}

// 強制再接続する
void SeSecForceReconnect(SE_SEC *s)
{
	UINT i;
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
	{
		SE_IKE_SA *sa = SE_LIST_DATA(s->IkeSaList, i);

		sa->DeleteNow = true;
	}
}

// メイン処理
void SeSecProcessMain(SE_SEC *s)
{
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	// 定期的ポーリングの発生
	SeSecDoInterval(s, &s->PoolingVar, SE_SEC_POLLING_INTERVAL);

	do
	{
		s->StatusChanged = false;

		// 有効期限が切れた IPsec SA を削除する
		SeSecDeleteExpiredIPsecSa(s);

		// 有効期限が切れた IKE SA を削除する
		SeSecDeleteExpiredIkeSa(s);

		if (SE_LIST_NUM(s->IkeSaList) == 0)
		{
			// 現在 IKE SA が 1 つも無い場合は VPN 接続を開始する
			if (s->NextConnectStartTick <= SeSecTick(s))
			{
				SeSecStartVpnConnect(s);
			}
		}
		else
		{
			UINT i;
			for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
			{
				SE_IKE_SA *sa = SE_LIST_DATA(s->IkeSaList, i);

				if (sa->Phase == 1 && sa->Phase2StartTick != 0 &&
					SeSecTick(s) >= sa->Phase2StartTick &&
					sa->Phase2Started == false)
				{
					SeSecSendIkeSaMsgPhase2(s, sa);
				}
			}
		}
	}
	while (s->StatusChanged);
}

// Life Seconds 値の調整
UINT64 SeSecLifeSeconds64bit(UINT value)
{
	UINT64 ret = (UINT64)value * 1000ULL;

	if (ret >= 1000ULL)
	{
		ret -= 800ULL;
	}

	return ret;
}

// IKE SA の有効期限のチェック
bool SeSecCheckIkeSaExpires(SE_SEC *s, SE_IKE_SA *sa)
{
	SE_SEC_CONFIG *config;
	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return false;
	}

	config = &s->Config;

	if (config->VpnPhase1LifeKilobytes != 0)
	{
		UINT64 value = config->VpnPhase1LifeKilobytes * 1024;

		if (sa->TransferBytes >= value)
		{
			// 転送量超過
			SeInfo("IKE: SA #%u: VpnPhase1LifeKilobytes Over", sa->Id);
			return false;
		}
	}

	if (config->VpnPhase1LifeSeconds != 0)
	{
		if (sa->Phase1EstablishedTick != 0)
		{
			UINT64 value = SeSecLifeSeconds64bit(config->VpnPhase1LifeSeconds) +
				sa->Phase1EstablishedTick;

			if (value <= SeSecTick(s))
			{
				// 時間超過
				SeInfo("IKE: SA #%u: VpnPhase1LifeSeconds Expired", sa->Id);
				return false;
			}
		}
	}

	return true;
}

// IPsec SA の有効期限のチェック
bool SeSecCheckIPsecSaExpires(SE_SEC *s, SE_IPSEC_SA *sa)
{
	SE_SEC_CONFIG *config;
	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return false;
	}

	config = &s->Config;

	if (config->VpnPhase2LifeKilobytes != 0)
	{
		UINT64 value = config->VpnPhase2LifeKilobytes * 1024;

		if (sa->TransferBytes >= value)
		{
			// 転送量超過
			SeInfo("IKE: SA #%u: VpnPhase2LifeKilobytes Over", sa->IkeSa->Id);
			return false;
		}
	}

	if (config->VpnPhase2LifeSeconds != 0)
	{
		if (sa->EstablishedTick != 0)
		{
			UINT64 value = SeSecLifeSeconds64bit(config->VpnPhase2LifeSeconds) +
				sa->EstablishedTick;

			if (value <= SeSecTick(s))
			{
				// 時間超過
				SeInfo("IKE: SA #%u: VpnPhase2LifeSeconds Expired", sa->IkeSa->Id);
				return false;
			}
		}
	}

	return true;
}

// 有効期限が切れた IPsec SA を削除する
void SeSecDeleteExpiredIPsecSa(SE_SEC *s)
{
	UINT i;
	SE_SEC_CONFIG *config;
	SE_LIST *o = NULL;
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	config = &s->Config;

	for (i = 0;i < SE_LIST_NUM(s->IPsecSaList);i++)
	{
		SE_IPSEC_SA *sa = SE_LIST_DATA(s->IPsecSaList, i);
		bool b = false;

		if (SeSecCheckIPsecSaExpires(s, sa) == false)
		{
			b = true;
		}

		if (b)
		{
			if (o == NULL)
			{
				o = SeNewList(NULL);
			}

			SeAdd(o, sa);
		}
	}

	if (o != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(o);i++)
		{
			SE_IPSEC_SA *sa = SE_LIST_DATA(o, i);

			// IPsec SA 削除
			SeSecFreeIPsecSa(s, sa);
		}

		SeFreeList(o);
	}
}

// 有効期限が切れた IKE SA を削除する
void SeSecDeleteExpiredIkeSa(SE_SEC *s)
{
	UINT i;
	SE_SEC_CONFIG *config;
	SE_LIST *o = NULL;
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	config = &s->Config;

	for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
	{
		SE_IKE_SA *sa = SE_LIST_DATA(s->IkeSaList, i);
		bool b = false;

		if (sa->DeleteNow)
		{
			// すぐに削除
			b = true;
		}
		else if (sa->Established == false)
		{
			// VPN 接続未完了の SA
			if (sa->ConnectTimeoutTick <= SeSecTick(s))
			{
				// VPN 接続タイムアウト発生
				SeInfo("IKE: SA #%u: VPN Connection Timeout Occurred", sa->Id);
				b = true;
			}
		}
		else
		{
			if (sa->Established)
			{
				if (config->VpnIdleTimeout != 0)
				{
					UINT64 value = (UINT64)(config->VpnIdleTimeout * 1000) +
						sa->LastCommTick;

					if (value <= SeSecTick(s))
					{
						// アイドルタイムアウト発生
						SeInfo("IKE: SA #%u: Idle Timeout Occurred", sa->Id);
						b = true;
					}
				}
			}

			// VPN 接続完了済みの SA
			if (SeSecCheckIkeSaExpires(s, sa) == false)
			{
				// 有効期限到来
				SeInfo("IKE: SA #%u: Expired", sa->Id);
				b = true;
			}
		}

		if (b)
		{
			if (o == NULL)
			{
				o = SeNewList(NULL);
			}

			SeAdd(o, sa);
		}
	}

	if (o != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(o);i++)
		{
			SE_IKE_SA *sa = SE_LIST_DATA(o, i);

			// IKE SA 削除
			SeSecFreeIkeSa(s, sa);
		}

		SeFreeList(o);
	}
}

// UDP 受信コールバック
void SeSecUdpRecvCallback(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param)
{
	SE_SEC *s = (SE_SEC *)param;
	SE_IKE_PACKET *packet_header;
	SE_IKE_SA *sa = NULL;
	// 引数チェック
	if (s == NULL || dest_addr == NULL || src_addr == NULL || data == NULL)
	{
		return;
	}

	if (src_port != SE_SEC_IKE_UDP_PORT || dest_port != SE_SEC_IKE_UDP_PORT ||
		SeCmp(dest_addr, &s->Config.MyIpAddress, sizeof(SE_IKE_IP_ADDR)) != 0)
	{
		return;
	}

	// IKE パース
	packet_header = SeIkeParseHeader(data, size, NULL);
	if (packet_header == NULL)
	{
		return;
	}

	// 対応する SA の検索
	if (packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_AGGRESSIVE || (packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_MAIN))
	{
		UINT i;
		// フェーズ 1 アグレッシブモード or main mode 応答パケット
		for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
		{
			SE_IKE_SA *sa2 = SE_LIST_DATA(s->IkeSaList, i);

			if (sa2->InitiatorCookie == packet_header->InitiatorCookie &&
				((packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_AGGRESSIVE &&
				sa2->ResponderCookie == 0) ||
				(packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_MAIN &&
				((sa2->ResponderCookie == 0 && sa2->Status == 1) ||
				sa2->ResponderCookie == packet_header->ResponderCookie))) &&
				sa2->DestPort == src_port &&
				sa2->SrcPort == dest_port &&
				SeCmp(&sa2->DestAddr, src_addr, sizeof(SE_IKE_IP_ADDR)) == 0 &&
				SeCmp(&sa2->SrcAddr, dest_addr, sizeof(SE_IKE_IP_ADDR)) == 0 &&
				sa2->Phase == 0)
			{
				sa = sa2;
				break;
			}
		}
	}
	else if (packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_QUICK)
	{
		// フェーズ 2 クイックモード応答パケット
		sa = SeSecSearchIkeSa(s, *dest_addr, *src_addr, dest_port, src_port,
			packet_header->InitiatorCookie, packet_header->ResponderCookie);

		if (sa != NULL && sa->Established)
		{
			// 既に確立済みの場合は除外する
			sa = NULL;
		}
	}
	else if (packet_header->ExchangeType == SE_IKE_EXCHANGE_TYPE_INFORMATION)
	{
		// 情報伝送パケット
		sa = SeSecSearchIkeSa(s, *dest_addr, *src_addr, dest_port, src_port,
			packet_header->InitiatorCookie, packet_header->ResponderCookie);

		if (sa != NULL && sa->Established == false)
		{
			// 未確立の場合は除外する
			sa = NULL;
		}
	}

	if (sa != NULL)
	{
		// IKE SA メッセージ処理
		SeSecProcessIkeSaMsg(s, sa, packet_header, data, size);
	}

	SeIkeFree(packet_header);
}

// ESP 受信コールバック
void SeSecEspRecvCallback(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param)
{
	SE_SEC *s = (SE_SEC *)param;
	SE_SEC_CONFIG *config;
	SE_IPSEC_SA *sa;
	// 引数チェック
	if (s == NULL || data == NULL)
	{
		return;
	}

	config = &s->Config;
	sa = SeSecGetIPsecSa(s, false);
	if (sa == NULL)
	{
		return;
	}

	// ESP パケットの解析
	if (SeCmp(&sa->DestAddr, src_addr, sizeof(SE_IKE_IP_ADDR)) == 0 &&
		SeCmp(&sa->SrcAddr, dest_addr, sizeof(SE_IKE_IP_ADDR)) == 0)
	{
		UCHAR *esp = (UCHAR *)data;
		UINT esp_size = size;
		UINT enc_block_size = SE_DES_BLOCK_SIZE;
		UINT enc_iv_size = SE_DES_IV_SIZE;
		UINT hash_size = SE_HMAC_SHA1_96_HASH_SIZE;

		if (esp_size >= sizeof(UINT) + sizeof(UINT) + enc_iv_size + enc_block_size + hash_size)
		{
			// SPI の検査
			UINT *spi = (UINT *)((UCHAR *)esp);

			if (*spi == sa->Spi)
			{
				// シーケンス番号
				UINT *seq = (UINT *)(((UCHAR *)esp) + sizeof(UINT));

				// IV
				UCHAR *iv = (UCHAR *)(((UCHAR *)esp) + sizeof(UINT) + sizeof(UINT));

				// データブロック
				UCHAR *data_block = (UCHAR *)(((UCHAR *)esp) + sizeof(UINT) + sizeof(UINT) + enc_iv_size);

				// データブロックサイズの計算
				UINT data_block_size = esp_size - (sizeof(UINT) + sizeof(UINT) + enc_iv_size + hash_size);

				if (data_block_size > 0 && ((data_block_size % enc_block_size) == 0))
				{
					// 認証データ
					UCHAR *hash = (UCHAR *)(((UCHAR *)esp) + sizeof(UINT) + sizeof(UINT) + enc_iv_size + data_block_size);
					UCHAR hash2[SE_HMAC_SHA1_96_HASH_SIZE];

					// ハッシュの計算
					SeMacSha196(hash2, sa->HashKey->Buf, esp, esp_size - hash_size);

					// ハッシュの比較
					if (SeCmp(hash, hash2, SE_HMAC_SHA1_96_HASH_SIZE) == 0)
					{
						// データ本体の解読
						UCHAR *payload_data = SeMalloc(data_block_size);

						UINT payload_size;

						UCHAR *padding_size = payload_data + data_block_size - sizeof(UCHAR) * 2;

						UCHAR *next_header = padding_size + 1;

						UCHAR next_header_2 = s->IPv6 ? 41 : 4;

						SeDes3Decrypt(payload_data, data_block, data_block_size,
							sa->DesKey, iv);

						if (data_block_size >= (sizeof(UCHAR) * 2 + *padding_size))
						{
							// ペイロードサイズの計算
							payload_size = data_block_size - (sizeof(UCHAR) * 2 + *padding_size);

							// IP ヘッダ番号の確認
							if (*next_header == next_header_2)
							{
								// 送信
								SeSecSendVirtualIp(s, payload_data, payload_size);

								sa->TransferBytes += payload_size;
								sa->IkeSa->LastCommTick = SeSecTick(s);
							}
						}

						SeFree(payload_data);
					}
				}
			}
		}
	}
}

// 仮想 IP 受信コールバック
void SeSecVirtualIpRecvCallback(void *data, UINT size, void *param)
{
	SE_SEC *s = (SE_SEC *)param;
	SE_SEC_CONFIG *config;
	SE_IPSEC_SA *sa;
	// 引数チェック
	if (s == NULL || data == NULL)
	{
		return;
	}

	config = &s->Config;
	sa = SeSecGetIPsecSa(s, true);
	if (sa == NULL)
	{
		return;
	}

	// ESP パケットの構築
	if (true)
	{
		UINT enc_block_size = SE_DES_BLOCK_SIZE;
		UINT enc_iv_size = SE_DES_IV_SIZE;
		UINT data_block_size;
		UINT esp_size;
		UINT hash_size = SE_HMAC_SHA1_96_HASH_SIZE;
		UINT padding_size;
		UCHAR padding_size_char;
		UCHAR *esp;
		UINT i;
		UCHAR n;
		UCHAR next_header = (s->IPv6 ? 41 : 4);
		UINT seq_be;

		// 暗号化の対象となるデータブロックサイズを取得する
		data_block_size = size + sizeof(UCHAR) * 2;
		if ((data_block_size % enc_block_size) != 0)
		{
			data_block_size = ((data_block_size / enc_block_size) + 1) * enc_block_size;
		}

		// ESP パケットサイズを計算する
		esp_size = sizeof(UINT) + sizeof(UINT) + enc_iv_size + data_block_size + hash_size;

		// ESP パケットを構築する
		esp = SeMalloc(esp_size);

		// SPI
		SeCopy(esp, &sa->Spi, sizeof(UINT));

		// シーケンス番号
		seq_be = SeEndian32(++sa->Seq);
		SeCopy(esp + sizeof(UINT), &seq_be, sizeof(UINT));

		// IV
		SeCopy(esp + sizeof(UINT) + sizeof(UINT), sa->NextIv, enc_iv_size);

		// ペイロードデータ
		SeCopy(esp + sizeof(UINT) + sizeof(UINT) + enc_iv_size, data, size);

		// パディング長
		padding_size = data_block_size - (size + sizeof(UCHAR) * 2);
		padding_size_char = (UCHAR)padding_size;
		SeCopy(esp + sizeof(UINT) + sizeof(UINT) + enc_iv_size + size + padding_size,
			&padding_size_char, sizeof(UCHAR));

		// 次ヘッダ番号
		SeCopy(esp + sizeof(UINT) + sizeof(UINT) + enc_iv_size + size + padding_size + sizeof(UCHAR),
			&next_header, sizeof(UCHAR));

		// パディング
		n = 0;
		for (i = sizeof(UINT) + sizeof(UINT) + enc_iv_size + size;
			i < sizeof(UINT) + sizeof(UINT) + enc_iv_size + size + padding_size;
			i++)
		{
			esp[i] = ++n;
		}

		// 暗号化
		SeDes3Encrypt(esp + sizeof(UINT) + sizeof(UINT) + enc_iv_size,
			esp + sizeof(UINT) + sizeof(UINT) + enc_iv_size,
			data_block_size,
			sa->DesKey,
			sa->NextIv);

		// 認証
		SeMacSha196(esp + sizeof(UINT) + sizeof(UINT) + enc_iv_size + data_block_size,
			sa->HashKey->Buf,
			esp,
			sizeof(UINT) + sizeof(UINT) + enc_iv_size + data_block_size);

		// 送信
		SeSecSendEsp(s, &sa->DestAddr, &sa->SrcAddr, esp, esp_size);

		// 最終ブロックを次の IV として保持
		SeCopy(sa->NextIv, esp + sizeof(UINT) + sizeof(UINT) + enc_iv_size
			+ data_block_size - enc_block_size, enc_block_size);

		SeFree(esp);

		sa->TransferBytes += size;

		if (sa->Seq == 0xffffffff)
		{
			// シーケンス番号超過
			SeDebug("IKE: SA #%u: Sequence Number Overflow", sa->IkeSa->Id);

			SeSecFreeIPsecSa(s, sa);
		}
	}
}

// 使用可能な IPsec SA の取得
SE_IPSEC_SA *SeSecGetIPsecSa(SE_SEC *s, bool outgoing)
{
	UINT i;
	// 引数チェック
	if (s == NULL)
	{
		return NULL;
	}

	for (i = 0;i < SE_LIST_NUM(s->IPsecSaList);i++)
	{
		UINT j = SE_LIST_NUM(s->IPsecSaList) - i - 1;
		SE_IPSEC_SA *sa = SE_LIST_DATA(s->IPsecSaList, j);

		if (sa->Outgoing == outgoing)
		{
			return sa;
		}
	}

	return NULL;
}

// 初期化メイン
void SeSecInitMain(SE_SEC *s)
{
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	// SA リストの初期化
	SeSecInitSaList(s);

	// メインルーチンを 1 度だけ呼び出す
	SeSecProcessMain(s);
}

// 解放メイン
void SeSecFreeMain(SE_SEC *s)
{
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	// SA リストの解放
	SeSecFreeSaList(s);
}

// IKE SA の作成
SE_IKE_SA *SeSecNewIkeSa(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
					  UINT src_port, UINT dest_port, UINT64 init_cookie)
{
	SE_IKE_SA *sa;
	static UINT ike_sa_id_seed = 0;
	// 引数チェック
	if (s == NULL)
	{
		return NULL;
	}

	sa = SeZeroMalloc(sizeof(SE_IKE_SA));

	sa->SrcAddr = src_addr;
	sa->DestAddr = dest_addr;
	sa->SrcPort = src_port;
	sa->DestPort = dest_port;
	sa->InitiatorCookie = init_cookie;
	sa->Id = ++ike_sa_id_seed;

	SeInsert(s->IkeSaList, sa);

	return sa;
}

// P1 用鍵セットの解放
void SeSecFreeP1KeySet(SE_IKE_P1_KEYSET *set)
{
	// 引数チェック
	if (set == NULL)
	{
		return;
	}

	if (set->SKEYID_d != NULL)
	{
		SeFreeBuf(set->SKEYID_d);
	}

	if (set->SKEYID_a != NULL)
	{
		SeFreeBuf(set->SKEYID_a);
	}

	if (set->SKEYID_e != NULL)
	{
		SeFreeBuf(set->SKEYID_e);
	}

	SeZero(set, sizeof(SE_IKE_P1_KEYSET));
}

// IKE SA の解放
void SeSecFreeIkeSa(SE_SEC *s, SE_IKE_SA *sa)
{
	UINT i;
	SE_LIST *o;
	// 引数チェック
	if (s == NULL || sa == NULL)
	{
		return;
	}

	o = NULL;

	for (i = 0;i < SE_LIST_NUM(s->IPsecSaList);i++)
	{
		// この IKE SA を参照しているすべての IPsec SA を削除
		SE_IPSEC_SA *ipsec_sa = SE_LIST_DATA(s->IPsecSaList, i);

		if (ipsec_sa->IkeSa == sa)
		{
			if (o == NULL)
			{
				o = SeNewList(NULL);
			}

			SeAdd(o, ipsec_sa);
		}
	}

	if (o != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(o);i++)
		{
			SE_IPSEC_SA *ipsec_sa = SE_LIST_DATA(o, i);

			SeSecFreeIPsecSa(s, ipsec_sa);
		}

		SeFreeList(o);
	}

	if (sa->Phase >= 1)
	{
		SeSecSendIkeSaDeleteMsg(s, sa);
	}

	SeInfo("IKE: SA #%u Deleted", sa->Id);

	if (sa->Phase2DesKey != NULL)
	{
		SeDes3FreeKey(sa->Phase2DesKey);
	}

	if (sa->Dh != NULL)
	{
		SeDhFree(sa->Dh);
	}

	if (sa->Phase1MyRand != NULL)
	{
		SeFreeBuf(sa->Phase1MyRand);
	}

	if (sa->Phase2MyRand != NULL)
	{
		SeFreeBuf(sa->Phase2MyRand);
	}

	if (sa->Phase2YourRand != NULL)
	{
		SeFreeBuf(sa->Phase2YourRand);
	}

	if (sa->Phase1Password != NULL)
	{
		SeFreeBuf(sa->Phase1Password);
	}

	if (sa->SAi_b != NULL)
	{
		SeFreeBuf(sa->SAi_b);
	}

	if (sa->IDii_b != NULL)
	{
		SeFreeBuf(sa->IDii_b);
	}

	if (sa->MyKEYMAT != NULL)
	{
		SeFreeBuf(sa->MyKEYMAT);
	}

	if (sa->YourKEYMAT != NULL)
	{
		SeFreeBuf(sa->YourKEYMAT);
	}

	if (sa->MyCert != NULL)
	{
		SeFreeCert(sa->MyCert);
	}

	if (sa->CaCert != NULL)
	{
		SeFreeCert(sa->CaCert);
	}

	SeSecFreeP1KeySet(&sa->P1KeySet);

	SeDelete(s->IkeSaList, sa);

	SeFree(sa);
}

// IPsec SA のための新しい SPI 値の作成
UINT SeSecGenIPsecSASpi(SE_SEC *s)
{
	UINT c;
	bool b;
	// 引数チェック
	if (s == NULL)
	{
		return 0;
	}

	do
	{
		// IPsec SA に対する重複検査を実施
		UINT i;
		do
		{
			c = SeSecNewSpi();
		} while (c == 0 || c == 0xffffffff);

		b = false;

		for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
		{
			SE_IKE_SA *sa = SE_LIST_DATA(s->IkeSaList, i);

			if (sa->MySpi == c)
			{
				b = true;
			}
		}
	}
	while (b);

	return c;
}

// IKE SA のための新しいメッセージ ID の作成
UINT SeSecGenIkeSaMessageId(SE_SEC *s)
{
	UINT c;
	bool b;
	// 引数チェック
	if (s == NULL)
	{
		return 0;
	}

	do
	{
		UINT i;
		do
		{
			c = SeRand32();
		} while (c == 0 || c == 0xffffffff);

		b = false;

		for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
		{
			SE_IKE_SA *sa = SE_LIST_DATA(s->IkeSaList, i);

			if (sa->Phase2MessageId == c)
			{
				b = true;
			}
		}
	}
	while (b);

	return c;
}

// IKE SA のための新しいイニシエータ Cookie の作成
UINT64 SeSecGenIkeSaInitCookie(SE_SEC *s)
{
	UINT64 c;
	bool b;
	// 引数チェック
	if (s == NULL)
	{
		return 0;
	}

	do
	{
		UINT i;
		do
		{
			c = SeRand64();
		} while (c == 0ULL || c == 0xffffffffffffffffULL);

		b = false;

		for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
		{
			SE_IKE_SA *sa = SE_LIST_DATA(s->IkeSaList, i);

			if (sa->InitiatorCookie == c)
			{
				b = true;
			}
		}
	}
	while (b);

	return c;
}

// SA リストの初期化
void SeSecInitSaList(SE_SEC *s)
{
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	s->IkeSaList = SeNewList(SeSecCmpIkeSa);
	s->IPsecSaList = SeNewList(NULL);
}

// SA リストの解放
void SeSecFreeSaList(SE_SEC *s)
{
	UINT i;
	SE_IKE_SA **sa_list;
//	SE_IPSEC_SA **ipsec_list;
	UINT num_sa_list;
	// 引数チェック
	if (s == NULL)
	{
		return;
	}
/*
	// IPsec SA リストの削除
	ipsec_list = SeToArray(s->IPsecSaList);
	num_sa_list = SE_LIST_NUM(s->IPsecSaList);

	for (i = 0;i < num_sa_list;i++)
	{
		SE_IPSEC_SA *sa = ipsec_list[i];

		SeSecFreeIPsecSa(s, sa);
	}

	SeFree(ipsec_list);*/

	// IKE SA リストの削除
	sa_list = SeToArray(s->IkeSaList);
	num_sa_list = SE_LIST_NUM(s->IkeSaList);

	for (i = 0;i < num_sa_list;i++)
	{
		SE_IKE_SA *sa = sa_list[i];

		SeSecFreeIkeSa(s, sa);
	}

	SeFreeList(s->IkeSaList);
	SeFree(sa_list);

	SeFreeList(s->IPsecSaList);
}

// IKE SA の比較
int SeSecCmpIkeSa(void *p1, void *p2)
{
	SE_IKE_SA *s1, *s2;
	int i;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	s1 = *(SE_IKE_SA **)p1;
	s2 = *(SE_IKE_SA **)p2;
	if (s1 == NULL || s2 == NULL)
	{
		return 0;
	}

	i = SE_COMPARE(s1->InitiatorCookie, s2->InitiatorCookie);
	if (i != 0)
	{
		return i;
	}

	i = SE_COMPARE(s1->ResponderCookie, s2->ResponderCookie);
	if (i != 0)
	{
		return i;
	}

	i = SeCmp(&s1->SrcAddr, &s2->SrcAddr, sizeof(SE_IKE_IP_ADDR));
	if (i != 0)
	{
		return i;
	}

	i = SeCmp(&s1->DestAddr, &s2->DestAddr, sizeof(SE_IKE_IP_ADDR));
	if (i != 0)
	{
		return i;
	}

	i = SE_COMPARE(s1->SrcPort, s2->SrcPort);
	if (i != 0)
	{
		return i;
	}

	i = SE_COMPARE(s1->DestPort, s2->DestPort);
	if (i != 0)
	{
		return i;
	}

	return 0;
}

// SPI をキーとして IPsec SA の検索
SE_IPSEC_SA *SeSecSearchIPsecSaBySpi(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
									 UINT spi)
{
	UINT i;
	// 引数チェック
	if (s == NULL)
	{
		return NULL;
	}

	for (i = 0;i < SE_LIST_NUM(s->IPsecSaList);i++)
	{
		SE_IPSEC_SA *sa = SE_LIST_DATA(s->IPsecSaList, i);

		if (SeCmp(&src_addr, &sa->DestAddr, sizeof(SE_IKE_IP_ADDR)) &&
			SeCmp(&dest_addr, &sa->SrcAddr, sizeof(SE_IKE_IP_ADDR)))
		{
			if (spi == sa->Spi)
			{
				return sa;
			}
		}
	}

	return NULL;
}

// SPI をキーとして IKE SA の検索
SE_IKE_SA *SeSecSearchIkeSaBySpi(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
								 UINT src_port, UINT dest_port, void *spi_buf)
{
	UINT i;
	// 引数チェック
	if (s == NULL || spi_buf == NULL)
	{
		return NULL;
	}

	for (i = 0;i < SE_LIST_NUM(s->IkeSaList);i++)
	{
		SE_IKE_SA *sa = SE_LIST_DATA(s->IkeSaList, i);

		if (SeCmp(&src_addr, &sa->DestAddr, sizeof(SE_IKE_IP_ADDR)) &&
			SeCmp(&dest_addr, &sa->SrcAddr, sizeof(SE_IKE_IP_ADDR)))
		{
			if (sa->SrcPort == dest_port && sa->DestPort == src_port)
			{
				UINT64 a = ((UINT64 *)spi_buf)[0];
				UINT64 b = ((UINT64 *)spi_buf)[1];

				if (a == sa->InitiatorCookie && b == sa->ResponderCookie)
				{
					return sa;
				}
			}
		}
	}

	return NULL;
}

// IKE SA の検索
SE_IKE_SA *SeSecSearchIkeSa(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
							UINT src_port, UINT dest_port, UINT64 init_cookie, UINT64 resp_cookie)
{
	SE_IKE_SA t;
	// 引数チェック
	if (s == NULL)
	{
		return NULL;
	}

	SeZero(&t, sizeof(t));
	t.SrcAddr = src_addr;
	t.DestAddr = dest_addr;
	t.SrcPort = src_port;
	t.DestPort = dest_port;
	t.InitiatorCookie = init_cookie;
	t.ResponderCookie = resp_cookie;

	return (SE_IKE_SA *)SeSearch(s->IkeSaList, &t);
}

// 新しい SPI 値の作成
UINT SeSecNewSpi()
{
	while (true)
	{
		UINT i = SeRand32();

		if (i >= 4096)
		{
			return i;
		}
	}
}

// タイマコールバック (メインプロシージャ)
void SeSecTimerCallback(UINT64 tick, void *param)
{
	SE_SEC *s = (SE_SEC *)param;
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	SeSecProcessMain(s);
}

// 定期的処理の実行
bool SeSecDoInterval(SE_SEC *s, UINT64 *var, UINT interval)
{
	UINT64 tick;
	// 引数チェック
	if (s == NULL || var == NULL)
	{
		return false;
	}

	tick = SeSecTick(s);

	if (*var == 0 || (*var + (UINT64)interval) <= tick)
	{
		*var = tick;

		SeSecAddTimer(s, interval);

		return true;
	}
	else
	{
		return false;
	}
}

// 初期化
SE_SEC *SeSecInit(SE_VPN *vpn, bool ipv6, SE_SEC_CONFIG *config, SE_SEC_CLIENT_FUNCTION_TABLE *function_table, void *param, bool send_strict_id)
{
	SE_SEC *s;
	// 引数チェック
	if (vpn == NULL || function_table == NULL || config == NULL)
	{
		return NULL;
	}

	s = SeZeroMalloc(sizeof(SE_SEC));

	s->Vpn = vpn;
	s->ClientFunctions = *function_table;
	s->Param = param;
	s->Config = *config;
	s->IPv6 = ipv6;
	s->SendStrictIdV6 = send_strict_id;

	SeSecSetTimerCallback(s, SeSecTimerCallback);
	SeSecSetRecvUdpCallback(s, SeSecUdpRecvCallback);
	SeSecSetRecvEspCallback(s, SeSecEspRecvCallback);
	SeSecSetRecvVirtualIpCallback(s, SeSecVirtualIpRecvCallback);

	// 初期化メイン
	SeSecInitMain(s);

	return s;
}

// 解放
void SeSecFree(SE_SEC *s)
{
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	s->Halting = true;

	// 解放メイン
	SeSecFreeMain(s);

	SeFree(s);
}

// 文字列から認証メソッドを取得
UINT SeSecStrToAuthMethod(char *str)
{
	if (SeStartWith(str, "Password") || SeStartWith("Password", str))
	{
		return SE_SEC_AUTH_METHOD_PASSWORD;
	}
	else if (SeStartWith(str, "Cert") || SeStartWith("Cert", str))
	{
		return SE_SEC_AUTH_METHOD_CERT;
	}

	return 0;
}

// 現在時刻を取得してもらう
UINT64 SeSecTick(SE_SEC *s)
{
	// 引数チェック
	if (s == NULL)
	{
		return 0;
	}

	return s->ClientFunctions.ClientGetTick(s->Param);
}

// タイマコールバックを設定してもらう
void SeSecSetTimerCallback(SE_SEC *s, SE_SEC_TIMER_CALLBACK *callback)
{
	// 引数チェック
	if (s == NULL || callback == NULL)
	{
		return;
	}

	s->ClientFunctions.ClientSetTimerCallback(callback, s, s->Param);
}

// タイマを追加してもらう
void SeSecAddTimer(SE_SEC *s, UINT interval)
{
	// 引数チェック
	if (s == NULL)
	{
		return;
	}
	if (s->Halting)
	{
		return;
	}

	s->ClientFunctions.ClientAddTimer(interval, s->Param);
}

// UDP 受信コールバックを設定してもらう
void SeSecSetRecvUdpCallback(SE_SEC *s, SE_SEC_UDP_RECV_CALLBACK *callback)
{
	// 引数チェック
	if (s == NULL || callback == NULL)
	{
		return;
	}

	s->ClientFunctions.ClientSetRecvUdpCallback(callback, s, s->Param);
}

// ESP 受信コールバックを設定してもらう
void SeSecSetRecvEspCallback(SE_SEC *s, SE_SEC_ESP_RECV_CALLBACK *callback)
{
	// 引数チェック
	if (s == NULL || callback == NULL)
	{
		return;
	}

	s->ClientFunctions.ClientSetRecvEspCallback(callback, s, s->Param);
}

// 仮想 IP 受信コールバックを設定してもらう
void SeSecSetRecvVirtualIpCallback(SE_SEC *s, SE_SEC_VIRTUAL_IP_RECV_CALLBACK *callback)
{
	// 引数チェック
	if (s == NULL || callback == NULL)
	{
		return;
	}

	s->ClientFunctions.ClientSetRecvVirtualIpCallback(callback, s, s->Param);
}

// UDP パケットを送信してもらう
void SeSecSendUdp(SE_SEC *s, SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size)
{
	// 引数チェック
	if (s == NULL || dest_addr == NULL || src_addr == NULL || data == NULL)
	{
		return;
	}
	if (s->Halting)
	{
		return;
	}

	s->ClientFunctions.ClientSendUdp(dest_addr, src_addr, dest_port, src_port, data, size, s->Param);
}

// ESP パケットを送信してもらう
void SeSecSendEsp(SE_SEC *s, SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size)
{
	// 引数チェック
	if (s == NULL || dest_addr == NULL || src_addr == NULL || data == NULL)
	{
		return;
	}
	if (s->Halting)
	{
		return;
	}

	s->ClientFunctions.ClientSendEsp(dest_addr, src_addr, data, size, s->Param);
}

// 仮想 IP パケットを送信してもらう
void SeSecSendVirtualIp(SE_SEC *s, void *data, UINT size)
{
	// 引数チェック
	if (s == NULL || data == NULL)
	{
		return;
	}
	if (s->Halting)
	{
		return;
	}

	s->ClientFunctions.ClientSendVirtualIp(data, size, s->Param);
}




