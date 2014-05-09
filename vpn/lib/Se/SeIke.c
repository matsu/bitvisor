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

// SeIke.c
// 概要: IKE (ISAKMP) プロトコルスタック

#define SE_INTERNAL
#include <Se/Se.h>

// 文字列をパスワードに変換
SE_BUF *SeIkeStrToPassword(char *str)
{
	SE_BUF *b;
	// 引数チェック
	if (str == NULL)
	{
		return SeNewBuf();
	}

	if (SeStartWith(str, "0x") == false)
	{
		// 文字列をそのまま使用
		b = SeNewBuf();
		SeWriteBuf(b, str, SeStrLen(str));
	}
	else
	{
		// 16 進として解釈
		b = SeStrToBin(str + 2);
	}

	return b;
}

// フェーズ 1 暗号化アルゴリズム名を鍵サイズに変換
UINT SeIkePhase1CryptIdToKeySize(UCHAR id)
{
	switch (id)
	{
	case SE_IKE_P1_CRYPTO_3DES_CBC:
		return SE_3DES_KEY_SIZE;

	case SE_IKE_P1_CRYPTO_DES_CBC:
		return SE_DES_KEY_SIZE;
	}

	return 0;
}

// フェーズ 2 暗号化アルゴリズム名を鍵サイズに変換
UINT SeIkePhase2CryptIdToKeySize(UCHAR id)
{
	switch (id)
	{
	case SE_IKE_TRANSFORM_ID_P2_ESP_3DES:
		return SE_3DES_KEY_SIZE;

	case SE_IKE_TRANSFORM_ID_P2_ESP_DES:
		return SE_DES_KEY_SIZE;
	}

	return 0;
}

// Phase one mode selection
UINT SeIkeStrToPhase1Mode(char *name)
{
	if (SeStrCmp(name, "Main") == 0)
	{
		return SE_IKE_EXCHANGE_TYPE_MAIN;
	}
	else if (SeStrCmp(name, "Aggressive") == 0)
	{
		return SE_IKE_EXCHANGE_TYPE_AGGRESSIVE;
	}
	else
	{
		return 0;
	}
}

// 文字列をアルゴリズム名に変換
UCHAR SeIkeStrToPhase1CryptId(char *name)
{
	if (SeStartWith(name, "3DES") || SeStartWith("3DES", name))
	{
		return SE_IKE_P1_CRYPTO_3DES_CBC;
	}
	else if (SeStartWith(name, "DES") || SeStartWith("DES", name))
	{
		return SE_IKE_P1_CRYPTO_DES_CBC;
	}
	else
	{
		return 0;
	}
}
UCHAR SeIkeStrToPhase1HashId(char *name)
{
	if (SeStartWith(name, "SHA-1") || SeStartWith("SHA-1", name))
	{
		return SE_IKE_P1_HASH_SHA1;
	}

	return 0;
}
UCHAR SeIkeStrToPhase2CryptId(char *name)
{
	if (SeStartWith(name, "3DES") || SeStartWith("3DES", name))
	{
		return SE_IKE_TRANSFORM_ID_P2_ESP_3DES;
	}
	else if (SeStartWith(name, "DES") || SeStartWith("DES", name))
	{
		return SE_IKE_TRANSFORM_ID_P2_ESP_DES;
	}
	else
	{
		return 0;
	}
}
UCHAR SeIkeStrToPhase2HashId(char *name)
{
	if (SeStartWith(name, "SHA-1") || SeStartWith("SHA-1", name))
	{
		return SE_IKE_P2_HMAC_SHA1;
	}

	return 0;
}

// IP アドレスを文字列に変換する
void SeIkeIpAddressToStr(char *str, SE_IKE_IP_ADDR *a)
{
	// 引数チェック
	if (str == NULL || a == NULL)
	{
		return;
	}

	if (a->IsIPv6 == false)
	{
		// IPv4
		Se4IPToStr(str, SeIkeGetIPv4Address(a));
	}
	else
	{
		// IPv6
		Se6IPToStr(str, SeIkeGetIPv6Address(a));
	}
}

// アドレスがゼロアドレスかどうか取得する
bool SeIkeIsZeroIP(SE_IKE_IP_ADDR *a)
{
	bool ret = true;
	// 引数チェック
	if (a == NULL)
	{
		return false;
	}

	if (a->IsIPv6 == false)
	{
		ret = Se4IsZeroIP(a->Address.Ipv4);
	}
	else
	{
		ret = Se6IsZeroIP(a->Address.Ipv6);
	}

	return ret;
}

// IPv6 アドレスの初期化
void SeIkeInitIPv6Address(SE_IKE_IP_ADDR *a, SE_IPV6_ADDR *addr)
{
	// 引数チェック
	if (a == NULL || addr == NULL)
	{
		return;
	}

	SeZero(a, sizeof(SE_IKE_IP_ADDR));

	a->Address.Ipv6 = *addr;
	a->IsIPv6 = true;
}

// IPv4 アドレスの初期化
void SeIkeInitIPv4Address(SE_IKE_IP_ADDR *a, SE_IPV4_ADDR *addr)
{
	// 引数チェック
	if (a == NULL || addr == NULL)
	{
		return;
	}

	SeZero(a, sizeof(SE_IKE_IP_ADDR));

	a->Address.Ipv4 = *addr;
	a->IsIPv6 = false;
}

// IPv6 アドレスの取得
SE_IPV6_ADDR SeIkeGetIPv6Address(SE_IKE_IP_ADDR *a)
{
	// 引数チェック
	if (a == NULL)
	{
		return Se6ZeroIP();
	}

	return a->Address.Ipv6;
}

// IPv4 アドレスの取得
SE_IPV4_ADDR SeIkeGetIPv4Address(SE_IKE_IP_ADDR *a)
{
	// 引数チェック
	if (a == NULL)
	{
		return Se4ZeroIP();
	}

	return a->Address.Ipv4;
}

// データペイロードの構築
SE_BUF *SeIkeBuildDataPayload(SE_IKE_PACKET_DATA_PAYLOAD *t)
{
	SE_BUF *b;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();
	SeWriteBuf(b, t->Data->Buf, t->Data->Size);

	return b;
}

// SA ペイロードの構築
SE_BUF *SeIkeBuildSaPayload(SE_IKE_PACKET_SA_PAYLOAD *t)
{
	SE_IKE_SA_HEADER h;
	SE_BUF *ret;
	SE_BUF *b;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.DoI = SeEndian32(SE_IKE_SA_DOI_IPSEC);
	h.Situation = SeEndian32(SE_IKE_SA_SITUATION_IDENTITY);

	ret = SeNewBuf();

	SeWriteBuf(ret, &h, sizeof(h));

	b = SeIkeBuildPayloadList(t->PayloadList);
	SeWriteBufBuf(ret, b);

	SeFreeBuf(b);

	return ret;
}

// プロポーザルペイロードの構築
SE_BUF *SeIkeBuildProposalPayload(SE_IKE_PACKET_PROPOSAL_PAYLOAD *t)
{
	SE_IKE_PROPOSAL_HEADER h;
	SE_BUF *ret, *b;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.Number = t->Number;
	h.NumTransforms = SE_LIST_NUM(t->PayloadList);
	h.ProtocolId = t->ProtocolId;
	h.SpiSize = t->Spi->Size;

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));
	SeWriteBufBuf(ret, t->Spi);

	b = SeIkeBuildPayloadList(t->PayloadList);
	SeWriteBufBuf(ret, b);

	SeFreeBuf(b);

	return ret;
}

// トランスフォーム値リストの構築
SE_BUF *SeIkeBuildTransformValueList(SE_LIST *o)
{
	SE_BUF *b;
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IKE_PACKET_TRANSFORM_VALUE *v = SE_LIST_DATA(o, i);
		SE_BUF *tmp = SeIkeBuildTransformValue(v);

		SeWriteBufBuf(b, tmp);

		SeFreeBuf(tmp);
	}

	return b;
}

// トランスフォーム値の構築
SE_BUF *SeIkeBuildTransformValue(SE_IKE_PACKET_TRANSFORM_VALUE *v)
{
	SE_BUF *b;
	UCHAR af_bit, type;
	USHORT size_or_value;
	// 引数チェック
	if (v == NULL)
	{
		return NULL;
	}

	type = v->Type;

	if (v->Value >= 65536)
	{
		// 32 bit
		af_bit = 0;
		size_or_value = sizeof(UINT);
	}
	else
	{
		// 16 bit
		af_bit = 0x80;
		size_or_value = SeEndian16((USHORT)v->Value);
	}

	b = SeNewBuf();
	SeWriteBuf(b, &af_bit, sizeof(af_bit));
	SeWriteBuf(b, &type, sizeof(type));
	SeWriteBuf(b, &size_or_value, sizeof(size_or_value));

	if (af_bit == 0)
	{
		UINT value = SeEndian32(v->Value);
		SeWriteBuf(b, &value, sizeof(UINT));
	}

	return b;
}

// トランスフォームペイロードの構築
SE_BUF *SeIkeBuildTransformPayload(SE_IKE_PACKET_TRANSFORM_PAYLOAD *t)
{
	SE_IKE_TRANSFORM_HEADER h;
	SE_BUF *ret, *b;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.Number = t->Number;
	h.TransformId = t->TransformId;

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));

	b = SeIkeBuildTransformValueList(t->ValueList);
	SeWriteBufBuf(ret, b);

	SeFreeBuf(b);

	return ret;
}

// ID ペイロードの構築
SE_BUF *SeIkeBuildIdPayload(SE_IKE_PACKET_ID_PAYLOAD *t)
{
	SE_IKE_ID_HEADER h;
	SE_BUF *ret;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.IdType = t->Type;
	h.Port = SeEndian16(t->Port);
	h.ProtocolId = t->ProtocolId;

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));

	SeWriteBufBuf(ret, t->IdData);

	return ret;
}

// 証明書ペイロードの構築
SE_BUF *SeIkeBuildCertPayload(SE_IKE_PACKET_CERT_PAYLOAD *t)
{
	SE_IKE_CERT_HEADER h;
	SE_BUF *ret;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.CertType = t->CertType;

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));
	SeWriteBufBuf(ret, t->CertData);

	return ret;
}

// 証明書要求ペイロードの構築
SE_BUF *SeIkeBuildCertRequestPayload(SE_IKE_PACKET_CERT_REQUEST_PAYLOAD *t)
{
	SE_IKE_CERT_REQUEST_HEADER h;
	SE_BUF *ret;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.CertType = t->CertType;

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));
	SeWriteBufBuf(ret, t->Data);

	return ret;
}

// 通知ペイロードの構築
SE_BUF *SeIkeBuildNoticePayload(SE_IKE_PACKET_NOTICE_PAYLOAD *t)
{
	SE_IKE_NOTICE_HEADER h;
	SE_BUF *ret;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.DoI = SeEndian32(SE_IKE_SA_DOI_IPSEC);
	h.MessageType = SeEndian16(t->MessageType);
	h.ProtocolId = t->ProtocolId;
	h.SpiSize = t->Spi->Size;

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));
	SeWriteBuf(ret, t->Spi->Buf, t->Spi->Size);

	return ret;
}

// 削除ペイロードの構築
SE_BUF *SeIkeBuildDeletePayload(SE_IKE_PACKET_DELETE_PAYLOAD *t)
{
	SE_IKE_DELETE_HEADER h;
	SE_BUF *ret;
	UINT i;
	// 引数チェック
	if (t == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.DoI = SeEndian32(SE_IKE_SA_DOI_IPSEC);
	h.NumSpis = SE_LIST_NUM(t->SpiList);
	h.ProtocolId = t->ProtocolId;

	if (SE_LIST_NUM(t->SpiList) >= 1)
	{
		SE_BUF *b = SE_LIST_DATA(t->SpiList, 0);

		h.SpiSize = b->Size;
	}

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));

	for (i = 0;i < SE_LIST_NUM(t->SpiList);i++)
	{
		SE_BUF *b = SE_LIST_DATA(t->SpiList, i);

		SeWriteBuf(ret, b->Buf, b->Size);
	}

	return ret;
}

// ペイロードからビット列を構築
SE_BUF *SeIkeBuildPayload(SE_IKE_PACKET_PAYLOAD *p)
{
	SE_BUF *b = NULL;
	// 引数チェック
	if (p == NULL)
	{
		return NULL;
	}

	switch (p->PayloadType)
	{
	case SE_IKE_PAYLOAD_SA:					// SA ペイロード
		b = SeIkeBuildSaPayload(&p->Payload.Sa);
		break;

	case SE_IKE_PAYLOAD_PROPOSAL:			// プロポーザルペイロード
		b = SeIkeBuildProposalPayload(&p->Payload.Proposal);
		break;

	case SE_IKE_PAYLOAD_TRANSFORM:			// トランスフォームペイロード
		b = SeIkeBuildTransformPayload(&p->Payload.Transform);
		break;

	case SE_IKE_PAYLOAD_ID:					// ID ペイロード
		b = SeIkeBuildIdPayload(&p->Payload.Id);
		break;

	case SE_IKE_PAYLOAD_CERT:				// 証明書ペイロード
		b = SeIkeBuildCertPayload(&p->Payload.Cert);
		break;

	case SE_IKE_PAYLOAD_CERT_REQUEST:		// 証明書要求ペイロード
		b = SeIkeBuildCertRequestPayload(&p->Payload.CertRequest);
		break;

	case SE_IKE_PAYLOAD_NOTICE:				// 通知ペイロード
		b = SeIkeBuildNoticePayload(&p->Payload.Notice);
		break;

	case SE_IKE_PAYLOAD_DELETE:				// 削除ペイロード
		b = SeIkeBuildDeletePayload(&p->Payload.Delete);
		break;

	case SE_IKE_PAYLOAD_KEY_EXCHANGE:		// 鍵交換ペイロード
	case SE_IKE_PAYLOAD_HASH:				// ハッシュペイロード
	case SE_IKE_PAYLOAD_SIGN:				// 署名ペイロード
	case SE_IKE_PAYLOAD_RAND:				// 乱数ペイロード
	case SE_IKE_PAYLOAD_VENDOR_ID:			// ベンダ ID ペイロード
		b = SeIkeBuildDataPayload(&p->Payload.GeneralData);
		break;
	}

	if (b != NULL)
	{
		if (p->BitArray != NULL)
		{
			SeFreeBuf(p->BitArray);
		}
		p->BitArray = SeCloneBuf(b);
	}

	return b;
}

// 1 個目のペイロードの種類を取得
UCHAR SeIkeGetFirstPayloadType(SE_LIST *o)
{
	SE_IKE_PACKET_PAYLOAD *p;
	// 引数チェック
	if (o == NULL)
	{
		return SE_IKE_PAYLOAD_NONE;
	}

	if (SE_LIST_NUM(o) == 0)
	{
		return SE_IKE_PAYLOAD_NONE;
	}

	p = (SE_IKE_PACKET_PAYLOAD *)SE_LIST_DATA(o, 0);

	return p->PayloadType;
}

// ペイロードリストからビット列を構築
SE_BUF *SeIkeBuildPayloadList(SE_LIST *o)
{
	SE_BUF *b;
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	b = SeNewBuf();

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IKE_PACKET_PAYLOAD *p = SE_LIST_DATA(o, i);
		SE_IKE_PACKET_PAYLOAD *next = NULL;
		SE_IKE_COMMON_HEADER h;
		SE_BUF *tmp;

		if (i < (SE_LIST_NUM(o) - 1))
		{
			next = SE_LIST_DATA(o, i + 1);
		}

		SeZero(&h, sizeof(h));
		if (next != NULL)
		{
			h.NextPayload = next->PayloadType;
		}
		else
		{
			h.NextPayload = SE_IKE_PAYLOAD_NONE;
		}

		tmp = SeIkeBuildPayload(p);
		if (tmp != NULL)
		{
			h.PayloadSize = SeEndian16(tmp->Size + (USHORT)sizeof(h));

			SeWriteBuf(b, &h, sizeof(h));
			SeWriteBuf(b, tmp->Buf, tmp->Size);

			SeFreeBuf(tmp);
		}
	}

	SeSeekBuf(b, 0, 0);

	return b;
}

// 指定したペイロードを取得
SE_IKE_PACKET_PAYLOAD *SeIkeGetPayload(SE_LIST *o, UINT payload_type, UINT index)
{
	UINT i, num;
	SE_IKE_PACKET_PAYLOAD *ret = NULL;
	// 引数チェック
	if (o == NULL)
	{
		return 0;
	}

	num = 0;

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IKE_PACKET_PAYLOAD *p = SE_LIST_DATA(o, i);

		if (p->PayloadType == payload_type)
		{
			if (num == index)
			{
				ret = p;
				break;
			}

			num++;
		}
	}

	return ret;
}

// 指定した種類のペイロードの個数を取得
UINT SeIkeGetPayloadNum(SE_LIST *o, UINT payload_type)
{
	UINT i, num;
	// 引数チェック
	if (o == NULL)
	{
		return 0;
	}

	num = 0;

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IKE_PACKET_PAYLOAD *p = SE_LIST_DATA(o, i);

		if (p->PayloadType == payload_type)
		{
			num++;
		}
	}

	return num;
}

// 削除ペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewDeletePayload(UCHAR protocol_id, SE_LIST *spi_list)
{
	SE_IKE_PACKET_PAYLOAD *p;
	if (spi_list == NULL)
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_DELETE);
	p->Payload.Delete.ProtocolId = protocol_id;
	p->Payload.Delete.SpiList = spi_list;

	return p;
}

// 通知ペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewNoticePayload(UCHAR protocol_id, USHORT message_type,
											 void *spi, UINT spi_size,
											 void *message, UINT message_size)
{
	SE_IKE_PACKET_PAYLOAD *p;
	if (spi == NULL && spi_size != 0)
	{
		return NULL;
	}
	if (message == NULL && message_size != 0)
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_NOTICE);
	p->Payload.Notice.MessageType = message_type;
	p->Payload.Notice.MessageData = SeMemToBuf(message, message_size);
	p->Payload.Notice.Spi = SeMemToBuf(spi, spi_size);
	p->Payload.Notice.ProtocolId = protocol_id;

	return p;
}

// 証明書要求ペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewCertRequestPayload(UCHAR cert_type, void *data, UINT size)
{
	SE_IKE_PACKET_PAYLOAD *p;
	if (data == NULL && size != 0)
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_CERT_REQUEST);
	p->Payload.CertRequest.CertType = cert_type;
	p->Payload.CertRequest.Data = SeMemToBuf(data, size);

	return p;
}

// 証明書ペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewCertPayload(UCHAR cert_type, void *cert_data, UINT cert_size)
{
	SE_IKE_PACKET_PAYLOAD *p;
	if (cert_data == NULL && cert_size != 0)
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_CERT);
	p->Payload.Cert.CertType = cert_type;
	p->Payload.Cert.CertData = SeMemToBuf(cert_data, cert_size);

	return p;
}

// ID ペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewIdPayload(UCHAR id_type, UCHAR protocol_id, USHORT port, void *id_data, UINT id_size)
{
	SE_IKE_PACKET_PAYLOAD *p;
	if (id_data == NULL && id_size != 0)
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_ID);
	p->Payload.Id.IdData = SeMemToBuf(id_data, id_size);
	p->Payload.Id.Port = port;
	p->Payload.Id.ProtocolId = protocol_id;
	p->Payload.Id.Type = id_type;

	return p;
}

// トランスフォームペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewTransformPayload(UCHAR number, UCHAR transform_id, SE_LIST *value_list)
{
	SE_IKE_PACKET_PAYLOAD *p;
	if (value_list == NULL)
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_TRANSFORM);
	p->Payload.Transform.Number = number;
	p->Payload.Transform.TransformId = transform_id;
	p->Payload.Transform.ValueList = value_list;

	return p;
}

// プロポーザルペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewProposalPayload(UCHAR number, UCHAR protocol_id, void *spi, UINT spi_size, SE_LIST *payload_list)
{
	SE_IKE_PACKET_PAYLOAD *p;
	if (payload_list == NULL || (spi == NULL && spi_size != 0))
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_PROPOSAL);
	p->Payload.Proposal.Number = number;
	p->Payload.Proposal.ProtocolId = protocol_id;
	p->Payload.Proposal.Spi = SeMemToBuf(spi, spi_size);
	p->Payload.Proposal.PayloadList = payload_list;

	return p;
}

// SA ペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewSaPayload(SE_LIST *payload_list)
{
	SE_IKE_PACKET_PAYLOAD *p;
	// 引数チェック
	if (payload_list == NULL)
	{
		return NULL;
	}

	p = SeIkeNewPayload(SE_IKE_PAYLOAD_SA);
	p->Payload.Sa.PayloadList = payload_list;

	return p;
}

// データペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewDataPayload(UCHAR payload_type, void *data, UINT size)
{
	SE_IKE_PACKET_PAYLOAD *p;
	// 引数チェック
	if (data == NULL)
	{
		return NULL;
	}

	p = SeIkeNewPayload(payload_type);
	p->Payload.GeneralData.Data = SeMemToBuf(data, size);

	return p;
}

// 新しいペイロードの作成
SE_IKE_PACKET_PAYLOAD *SeIkeNewPayload(UINT payload_type)
{
	SE_IKE_PACKET_PAYLOAD *p;

	p = SeZeroMalloc(sizeof(SE_IKE_PACKET_PAYLOAD));

	p->PayloadType = payload_type;

	return p;
}

// IKE ペイロード本体の解析
SE_IKE_PACKET_PAYLOAD *SeIkeParsePayload(UINT payload_type, SE_BUF *b)
{
	SE_IKE_PACKET_PAYLOAD *p = NULL;
	bool ok = true;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	p = SeZeroMalloc(sizeof(SE_IKE_PACKET_PAYLOAD));
	p->PayloadType = payload_type;

	switch (p->PayloadType)
	{
	case SE_IKE_PAYLOAD_SA:					// SA ペイロード
		ok = SeIkeParseSaPayload(&p->Payload.Sa, b);
		break;

	case SE_IKE_PAYLOAD_PROPOSAL:			// プロポーザルペイロード
		ok = SeIkeParseProposalPayload(&p->Payload.Proposal, b);
		break;

	case SE_IKE_PAYLOAD_TRANSFORM:			// プロポーザルペイロード
		ok = SeIkeParseTransformPayload(&p->Payload.Transform, b);
		break;

	case SE_IKE_PAYLOAD_ID:					// ID ペイロード
		ok = SeIkeParseIdPayload(&p->Payload.Id, b);
		break;

	case SE_IKE_PAYLOAD_CERT:				// 証明書ペイロード
		ok = SeIkeParseCertPayload(&p->Payload.Cert, b);
		break;

	case SE_IKE_PAYLOAD_CERT_REQUEST:		// 証明書要求ペイロード
		ok = SeIkeParseCertRequestPayload(&p->Payload.CertRequest, b);
		break;

	case SE_IKE_PAYLOAD_NOTICE:				// 通知ペイロード
		ok = SeIkeParseNoticePayload(&p->Payload.Notice, b);
		break;

	case SE_IKE_PAYLOAD_DELETE:				// 削除ペイロード
		ok = SeIkeParseDeletePayload(&p->Payload.Delete, b);
		break;

	case SE_IKE_PAYLOAD_KEY_EXCHANGE:		// 鍵交換ペイロード
	case SE_IKE_PAYLOAD_HASH:				// ハッシュペイロード
	case SE_IKE_PAYLOAD_SIGN:				// 署名ペイロード
	case SE_IKE_PAYLOAD_RAND:				// 乱数ペイロード
	case SE_IKE_PAYLOAD_VENDOR_ID:			// ベンダ ID ペイロード
		ok = SeIkeParseDataPayload(&p->Payload.GeneralData, b);
		break;
	}

	if (ok == false)
	{
		SeFree(p);
		p = NULL;
	}
	else
	{
		p->BitArray = SeCloneBuf(b);
	}

	return p;
}

// SA ペイロードのパース
bool SeIkeParseSaPayload(SE_IKE_PACKET_SA_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_SA_HEADER *h;
	UCHAR *buf;
	UINT size;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (b->Size < sizeof(SE_IKE_SA_HEADER))
	{
		return false;
	}

	h = (SE_IKE_SA_HEADER *)b->Buf;
	buf = (UCHAR *)b->Buf;
	buf += sizeof(SE_IKE_SA_HEADER);
	size = b->Size - sizeof(SE_IKE_SA_HEADER);

	if (SeEndian32(h->DoI) != SE_IKE_SA_DOI_IPSEC)
	{
		SeError("ISAKMP: Invalid DoI Value: 0x%x", SeEndian32(h->DoI));
		return false;
	}

	if (SeEndian32(h->Situation) != SE_IKE_SA_SITUATION_IDENTITY)
	{
		SeError("ISAKMP: Invalid Situation Value: 0x%x", SeEndian32(h->Situation));
		return false;
	}

	t->PayloadList = SeIkeParsePayloadList(buf, size, SE_IKE_PAYLOAD_PROPOSAL);

	return true;
}

// SA ペイロードの解放
void SeIkeFreeSaPayload(SE_IKE_PACKET_SA_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->PayloadList != NULL)
	{
		SeIkeFreePayloadList(t->PayloadList);
		t->PayloadList = NULL;
	}
}

// プロポーザルペイロードのパース
bool SeIkeParseProposalPayload(SE_IKE_PACKET_PROPOSAL_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_PROPOSAL_HEADER *h;
	UCHAR *buf;
	UINT size;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (b->Size < sizeof(SE_IKE_PROPOSAL_HEADER))
	{
		return false;
	}

	h = (SE_IKE_PROPOSAL_HEADER *)b->Buf;

	t->Number = h->Number;
	t->ProtocolId = h->ProtocolId;

	buf = (UCHAR *)b->Buf;
	buf += sizeof(SE_IKE_PROPOSAL_HEADER);
	size = b->Size - sizeof(SE_IKE_PROPOSAL_HEADER);

	if (size < (UINT)h->SpiSize)
	{
		return false;
	}

	t->Spi = SeMemToBuf(buf, h->SpiSize);

	buf += h->SpiSize;
	size -= h->SpiSize;

	t->PayloadList = SeIkeParsePayloadList(buf, size, SE_IKE_PAYLOAD_TRANSFORM);

	return true;
}

// プロポーザルペイロードの解放
void SeIkeFreeProposalPayload(SE_IKE_PACKET_PROPOSAL_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->Spi != NULL)
	{
		SeFreeBuf(t->Spi);
		t->Spi = NULL;
	}

	if (t->PayloadList != NULL)
	{
		SeIkeFreePayloadList(t->PayloadList);
		t->PayloadList = NULL;
	}
}

// トランスフォームペイロードのパース
bool SeIkeParseTransformPayload(SE_IKE_PACKET_TRANSFORM_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_TRANSFORM_HEADER h;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (SeReadBuf(b, &h, sizeof(h)) != sizeof(h))
	{
		return false;
	}

	t->Number = h.Number;
	t->TransformId = h.TransformId;
	t->ValueList = SeIkeParseTransformValueList(b);

	return true;
}

// 新しいトランスフォーム値の作成
SE_IKE_PACKET_TRANSFORM_VALUE *SeIkeNewTransformValue(UCHAR type, UINT value)
{
	SE_IKE_PACKET_TRANSFORM_VALUE *v = SeZeroMalloc(sizeof(SE_IKE_PACKET_TRANSFORM_VALUE));

	v->Type = type;
	v->Value = value;

	return v;
}

// トランスフォーム値リストのパース
SE_LIST *SeIkeParseTransformValueList(SE_BUF *b)
{
	SE_LIST *o;
	bool ok = true;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	o = SeNewList(NULL);

	while (b->Current < b->Size)
	{
		UCHAR af_bit, type;
		USHORT size;
		UINT value = 0;
		SE_IKE_PACKET_TRANSFORM_VALUE *v;

		if (SeReadBuf(b, &af_bit, sizeof(af_bit)) != sizeof(af_bit))
		{
			ok = false;
			break;
		}

		if (SeReadBuf(b, &type, sizeof(type)) != sizeof(type))
		{
			ok = false;
			break;
		}

		if (SeReadBuf(b, &size, sizeof(size)) != sizeof(size))
		{
			ok = false;
		}

		size = SeEndian16(size);

		if (af_bit == 0)
		{
			UCHAR *tmp = SeMalloc(size);

			if (SeReadBuf(b, tmp, size) != size)
			{
				ok = false;
				SeFree(tmp);
				break;
			}

			switch (size)
			{
			case sizeof(UINT):
				value = SeEndian32(*((UINT *)tmp));
				break;

			case sizeof(USHORT):
				value = SeEndian16(*((USHORT *)tmp));
				break;

			case sizeof(UCHAR):
				value = *((UCHAR *)tmp);
				break;
			}

			SeFree(tmp);
		}
		else
		{
			value = (UINT)size;
		}

		v = SeZeroMalloc(sizeof(SE_IKE_PACKET_TRANSFORM_VALUE));
		v->Type = type;
		v->Value = value;

		SeAdd(o, v);
	}

	if (ok == false)
	{
		SeIkeFreeTransformValueList(o);
		o = NULL;
	}

	return o;
}

// トランスフォーム値リストの解放
void SeIkeFreeTransformValueList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IKE_PACKET_TRANSFORM_VALUE *v = SE_LIST_DATA(o, i);

		SeFree(v);
	}

	SeFreeList(o);
}

// トランスフォームペイロードの解放
void SeIkeFreeTransformPayload(SE_IKE_PACKET_TRANSFORM_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->ValueList != NULL)
	{
		SeIkeFreeTransformValueList(t->ValueList);
		t->ValueList = NULL;
	}
}

// ID ペイロードのパース
bool SeIkeParseIdPayload(SE_IKE_PACKET_ID_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_ID_HEADER h;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (SeReadBuf(b, &h, sizeof(h)) != sizeof(h))
	{
		return false;
	}

	t->Type = h.IdType;
	t->ProtocolId = h.ProtocolId;
	t->Port = SeEndian16(h.Port);
	t->IdData = SeReadRemainBuf(b);
	if (t->IdData == NULL)
	{
		return false;
	}

	return true;
}

// ID ペイロードの解放
void SeIkeFreeIdPayload(SE_IKE_PACKET_ID_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->IdData != NULL)
	{
		SeFreeBuf(t->IdData);
		t->IdData = NULL;
	}
}

// 証明書ペイロードのパース
bool SeIkeParseCertPayload(SE_IKE_PACKET_CERT_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_CERT_HEADER h;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (SeReadBuf(b, &h, sizeof(h)) != sizeof(h))
	{
		return false;
	}

	t->CertType = h.CertType;
	t->CertData = SeReadRemainBuf(b);
	if (t->CertData == NULL)
	{
		return false;
	}

	return true;
}

// 証明書ペイロードの解放
void SeIkeFreeCertPayload(SE_IKE_PACKET_CERT_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->CertData != NULL)
	{
		SeFreeBuf(t->CertData);
		t->CertData = NULL;
	}
}

// 証明書要求ペイロードのパース
bool SeIkeParseCertRequestPayload(SE_IKE_PACKET_CERT_REQUEST_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_CERT_REQUEST_HEADER h;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (SeReadBuf(b, &h, sizeof(h)) != sizeof(h))
	{
		return false;
	}

	t->CertType = h.CertType;
	t->Data = SeReadRemainBuf(b);
	if (t->Data == NULL)
	{
		return false;
	}

	return true;
}

// 証明書要求ペイロードの解放
void SeIkeFreeCertRequestPayload(SE_IKE_PACKET_CERT_REQUEST_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->Data != NULL)
	{
		SeFreeBuf(t->Data);
		t->Data = NULL;
	}
}

// 通知ペイロードのパース
bool SeIkeParseNoticePayload(SE_IKE_PACKET_NOTICE_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_NOTICE_HEADER h;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (SeReadBuf(b, &h, sizeof(h)) != sizeof(h))
	{
		return false;
	}

	if (SeEndian32(h.DoI) != SE_IKE_SA_DOI_IPSEC)
	{
		SeError("ISAKMP: Invalid DoI Value: 0x%x", SeEndian32(h.DoI));
		return false;
	}

	t->MessageType = SeEndian16(h.MessageType);
	t->ProtocolId = h.ProtocolId;
	t->Spi = SeReadBufFromBuf(b, h.SpiSize);
	if (t->Spi == NULL)
	{
		return false;
	}
	t->MessageData = SeReadRemainBuf(b);

	return true;
}

// 通知ペイロードの解放
void SeIkeFreeNoticePayload(SE_IKE_PACKET_NOTICE_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->MessageData != NULL)
	{
		SeFreeBuf(t->MessageData);
		t->MessageData = NULL;
	}

	if (t->Spi != NULL)
	{
		SeFreeBuf(t->Spi);
		t->Spi = NULL;
	}
}

// 削除ペイロードのパース
bool SeIkeParseDeletePayload(SE_IKE_PACKET_DELETE_PAYLOAD *t, SE_BUF *b)
{
	SE_IKE_DELETE_HEADER h;
	UINT num_spi;
	UINT spi_size;
	UINT i;
	bool ok = true;
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	if (SeReadBuf(b, &h, sizeof(h)) != sizeof(h))
	{
		return false;
	}

	if (SeEndian32(h.DoI) != SE_IKE_SA_DOI_IPSEC)
	{
		SeError("ISAKMP: Invalid DoI Value: 0x%x", SeEndian32(h.DoI));
		return false;
	}

	t->ProtocolId = h.ProtocolId;
	t->SpiList = SeNewList(NULL);
	num_spi = SeEndian16(h.NumSpis);
	spi_size = h.SpiSize;

	for (i = 0;i < num_spi;i++)
	{
		SE_BUF *spi = SeReadBufFromBuf(b, spi_size);

		if (spi == NULL)
		{
			ok = false;
			break;
		}

		SeAdd(t->SpiList, spi);
	}

	if (ok == false)
	{
		SeIkeFreeDeletePayload(t);
		return false;
	}

	return true;
}

// 削除ペイロードの解放
void SeIkeFreeDeletePayload(SE_IKE_PACKET_DELETE_PAYLOAD *t)
{
	UINT i;
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	if (t->SpiList != NULL)
	{
		for (i = 0;i < SE_LIST_NUM(t->SpiList);i++)
		{
			SE_BUF *spi = SE_LIST_DATA(t->SpiList, i);

			SeFreeBuf(spi);
		}

		SeFreeList(t->SpiList);

		t->SpiList = NULL;
	}
}

// データペイロードのパース
bool SeIkeParseDataPayload(SE_IKE_PACKET_DATA_PAYLOAD *t, SE_BUF *b)
{
	// 引数チェック
	if (t == NULL || b == NULL)
	{
		return false;
	}

	t->Data = SeMemToBuf(b->Buf, b->Size);

	return true;
}

// データペイロードの解放
void SeIkeFreeDataPayload(SE_IKE_PACKET_DATA_PAYLOAD *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	SeFreeBuf(t->Data);
}

// IKE ペイロード本体の解放
void SeIkeFreePayload(SE_IKE_PACKET_PAYLOAD *p)
{
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	switch (p->PayloadType)
	{
	case SE_IKE_PAYLOAD_SA:					// SA ペイロード
		SeIkeFreeSaPayload(&p->Payload.Sa);
		break;

	case SE_IKE_PAYLOAD_PROPOSAL:			// プロポーザルペイロード
		SeIkeFreeProposalPayload(&p->Payload.Proposal);
		break;

	case SE_IKE_PAYLOAD_TRANSFORM:			// プロポーザルペイロード
		SeIkeFreeTransformPayload(&p->Payload.Transform);
		break;

	case SE_IKE_PAYLOAD_ID:					// ID ペイロード
		SeIkeFreeIdPayload(&p->Payload.Id);
		break;

	case SE_IKE_PAYLOAD_CERT:				// 証明書ペイロード
		SeIkeFreeCertPayload(&p->Payload.Cert);
		break;

	case SE_IKE_PAYLOAD_CERT_REQUEST:		// 証明書要求ペイロード
		SeIkeFreeCertRequestPayload(&p->Payload.CertRequest);
		break;

	case SE_IKE_PAYLOAD_NOTICE:				// 通知ペイロード
		SeIkeFreeNoticePayload(&p->Payload.Notice);
		break;

	case SE_IKE_PAYLOAD_DELETE:				// 削除ペイロード
		SeIkeFreeDeletePayload(&p->Payload.Delete);
		break;

	case SE_IKE_PAYLOAD_KEY_EXCHANGE:		// 鍵交換ペイロード
	case SE_IKE_PAYLOAD_HASH:				// ハッシュペイロード
	case SE_IKE_PAYLOAD_SIGN:				// 署名ペイロード
	case SE_IKE_PAYLOAD_RAND:				// 乱数ペイロード
	case SE_IKE_PAYLOAD_VENDOR_ID:			// ベンダ ID ペイロード
		SeIkeFreeDataPayload(&p->Payload.GeneralData);
		break;
	}

	if (p->BitArray != NULL)
	{
		SeFreeBuf(p->BitArray);
	}

	SeFree(p);
}

// IKE ペイロードリストの解析
SE_LIST *SeIkeParsePayloadList(void *data, UINT size, UCHAR first_payload)
{
	return SeIkeParsePayloadListEx(data, size, first_payload, NULL);
}
SE_LIST *SeIkeParsePayloadListEx(void *data, UINT size, UCHAR first_payload, UINT *total_read_size)
{
	SE_LIST *o;
	SE_BUF *b;
	UCHAR payload_type = first_payload;
	UINT total = 0;
	// 引数チェック
	if (data == NULL)
	{
		return NULL;
	}

	o = SeNewList(NULL);
	b = SeMemToBuf(data, size);

	while (payload_type != SE_IKE_PAYLOAD_NONE)
	{
		// 共通ヘッダを読み込み
		SE_IKE_COMMON_HEADER header;
		USHORT payload_size;
		SE_BUF *payload_data;
		SE_IKE_PACKET_PAYLOAD *pay;

		if (SeReadBuf(b, &header, sizeof(header)) != sizeof(header))
		{
			SeError("ISAKMP: Broken Packet (Invalid Payload Size)");

LABEL_ERROR:
			// ヘッダ読み込み失敗
			SeIkeFreePayloadList(o);
			o = NULL;

			break;
		}

		total += sizeof(header);

		// ペイロードサイズを取得
		payload_size = SeEndian16(header.PayloadSize);

		if (payload_size < sizeof(header))
		{
			SeError("ISAKMP: Broken Packet (Invalid Payload Size)");
			goto LABEL_ERROR;
		}

		payload_size -= sizeof(header);

		// ペイロードデータを読み込み
		payload_data = SeReadBufFromBuf(b, payload_size);
		if (payload_data == NULL)
		{
			// データ読み込み失敗
			SeError("ISAKMP: Broken Packet (Invalid Payload Data)");
			goto LABEL_ERROR;
		}

		total += payload_size;

		// ペイロード本体を解析
		if (SE_IKE_IS_SUPPORTED_PAYLOAD_TYPE(payload_type))
		{
			// 対応しているペイロードタイプ
			pay = SeIkeParsePayload(payload_type, payload_data);

			if (pay == NULL)
			{
				SeFreeBuf(payload_data);
				SeError("ISAKMP: Broken Packet (Payload Data Parse Failed)");
				goto LABEL_ERROR;
			}

			SeAdd(o, pay);
		}
		else
		{
			// 対応していないペイロードタイプ
			SeWarning("ISAKMP: Ignored Payload Type: %u", payload_type);
		}

		payload_type = header.NextPayload;

		SeFreeBuf(payload_data);
	}

	SeFreeBuf(b);

	if (total_read_size != NULL)
	{
		*total_read_size = total;
	}

	return o;
}

// IKE ペイロードリストの解放
void SeIkeFreePayloadList(SE_LIST *o)
{
	UINT i;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		SE_IKE_PACKET_PAYLOAD *p = SE_LIST_DATA(o, i);

		SeIkeFreePayload(p);
	}

	SeFreeList(o);
}

// IKE パケットの構築
SE_BUF *SeIkeBuild(SE_IKE_PACKET *p, SE_IKE_CRYPTO_PARAM *cparam)
{
	SE_IKE_HEADER h;
	SE_BUF *msg_buf;
	SE_BUF *ret;
	// 引数チェック
	if (p == NULL)
	{
		return NULL;
	}

	SeZero(&h, sizeof(h));
	h.InitiatorCookie = p->InitiatorCookie;
	h.ResponderCookie = p->ResponderCookie;
	h.NextPayload = SeIkeGetFirstPayloadType(p->PayloadList);
	h.Version = SE_IKE_VERSION;
	h.ExchangeType = p->ExchangeType;
	h.Flag = (p->FlagEncrypted ? SE_IKE_HEADER_FLAG_ENCRYPTED : 0) |
		(p->FlagCommit ? SE_IKE_HEADER_FLAG_COMMIT : 0) |
		(p->FlagAuthOnly ? SE_IKE_HEADER_FLAG_AUTH_ONLY : 0);
	h.MessageId = SeEndian32(p->MessageId);

	msg_buf = SeIkeBuildPayloadList(p->PayloadList);

	if (p->DecryptedPayload != NULL)
	{
		SeFreeBuf(p->DecryptedPayload);
	}

	p->DecryptedPayload = SeCloneBuf(msg_buf);

	if (p->FlagEncrypted)
	{
		SE_BUF *b;
		// 暗号化
		b = SeIkeEncryptWithPadding(msg_buf->Buf, msg_buf->Size, cparam);

		if (b == NULL)
		{
			SeError("ISAKMP: Packet Encrypt Failed");
			SeFreeBuf(msg_buf);
			return NULL;
		}

		SeFreeBuf(msg_buf);

		msg_buf = b;
	}

	h.MessageSize = SeEndian32(msg_buf->Size + sizeof(h));

	ret = SeNewBuf();
	SeWriteBuf(ret, &h, sizeof(h));
	SeWriteBufBuf(ret, msg_buf);

	SeFreeBuf(msg_buf);

	SeSeekBuf(ret, 0, 0);

	return ret;
}

// IKE パケットの解析
SE_IKE_PACKET *SeIkeParseEx(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam, bool header_only)
{
	SE_IKE_PACKET *p = NULL;
	SE_BUF *b;
	// 引数チェック
	if (data == NULL)
	{
		return NULL;
	}

	b = SeMemToBuf(data, size);

	if (b->Size < sizeof(SE_IKE_HEADER))
	{
		SeError("ISAKMP: Invalid Packet Size");
	}
	else
	{
		// ヘッダ解析
		SE_IKE_HEADER *h = (SE_IKE_HEADER *)b->Buf;

		p = SeZeroMalloc(sizeof(SE_IKE_PACKET));

		p->InitiatorCookie = h->InitiatorCookie;
		p->ResponderCookie = h->ResponderCookie;
		p->ExchangeType = h->ExchangeType;
		p->FlagEncrypted = (h->Flag & SE_IKE_HEADER_FLAG_ENCRYPTED) ? true : false;
		p->FlagCommit = (h->Flag & SE_IKE_HEADER_FLAG_COMMIT) ? true : false;
		p->FlagAuthOnly = (h->Flag & SE_IKE_HEADER_FLAG_AUTH_ONLY) ? true : false;
		p->MessageId = SeEndian32(h->MessageId);

		if (b->Size < SeEndian32(h->MessageSize) ||
			SeEndian32(h->MessageSize) < sizeof(SE_IKE_HEADER))
		{
			SeError("ISAKMP: Invalid Packet Size");

			SeIkeFree(p);
			p = NULL;
		}
		else
		{
			if (header_only == false)
			{
				bool ok = false;
				UCHAR *payload_data;
				UINT payload_size;
				SE_BUF *buf = NULL;

				payload_data = ((UCHAR *)h) + sizeof(SE_IKE_HEADER);
				payload_size = SeEndian32(h->MessageSize) - sizeof(SE_IKE_HEADER);

				// 暗号化されている場合は解読
				if (p->FlagEncrypted)
				{
					buf = SeIkeDecrypt(payload_data, payload_size, cparam);

					if (buf != NULL)
					{
						ok = true;

						payload_data = buf->Buf;
						payload_size = buf->Size;

						p->DecryptedPayload = SeCloneBuf(buf);
					}
				}
				else
				{
					ok = true;
				}

				if (ok == false)
				{
					SeError("ISAKMP: Decrypt Failed");

					SeIkeFree(p);
					p = NULL;
				}
				else
				{
					UINT total_read_size;

					// ペイロード解析
					p->PayloadList = SeIkeParsePayloadListEx(payload_data,
						payload_size,
						h->NextPayload,
						&total_read_size);

					if (p->DecryptedPayload != NULL)
					{
						p->DecryptedPayload->Size = MIN(p->DecryptedPayload->Size, total_read_size);
					}
					else
					{
						p->DecryptedPayload = SeMemToBuf(payload_data, payload_size);
					}
				}

				if (buf != NULL)
				{
					SeFreeBuf(buf);
				}
			}
		}
	}

	SeFreeBuf(b);

	return p;
}
SE_IKE_PACKET *SeIkeParseHeader(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam)
{
	return SeIkeParseEx(data, size, cparam, true);
}
SE_IKE_PACKET *SeIkeParse(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam)
{
	return SeIkeParseEx(data, size, cparam, false);
}

// 暗号化 (パディングも実行する)
SE_BUF *SeIkeEncryptWithPadding(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam)
{
	UINT total_size;
	UINT i;
	UCHAR n = 0;
	UCHAR *tmp;
	SE_BUF *ret;
	// 引数チェック
	if (data == NULL || cparam == NULL)
	{
		return NULL;
	}

	total_size = ((size / SE_DES_BLOCK_SIZE) + ((size % SE_DES_BLOCK_SIZE) == 0 ? 0 : 1))
		* SE_DES_BLOCK_SIZE;
	if (total_size == 0)
	{
		total_size = SE_DES_BLOCK_SIZE;
	}

	tmp = SeMalloc(total_size);
	SeCopy(tmp, data, size);

	for (i = size;i < total_size;i++)
	{
		tmp[i] = ++n;
	}

	ret = SeIkeEncrypt(tmp, total_size, cparam);

	SeFree(tmp);

	return ret;
}

// 暗号化
SE_BUF *SeIkeEncrypt(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam)
{
	void *tmp;
	SE_BUF *b;
	// 引数チェック
	if (data == NULL || cparam == NULL)
	{
		return NULL;
	}

	if ((size % SE_DES_BLOCK_SIZE) != 0)
	{
		// ブロックサイズの整数倍でない
		return NULL;
	}

	tmp = SeMalloc(size);
	SeDes3Encrypt(tmp, data, size, cparam->DesKey, cparam->Iv);

	if (size >= SE_DES_BLOCK_SIZE)
	{
		SeCopy(cparam->NextIv, ((UCHAR *)tmp) + (size - SE_DES_BLOCK_SIZE), SE_DES_BLOCK_SIZE);
	}
	else
	{
		SeZero(cparam->NextIv, SE_DES_BLOCK_SIZE);
	}

	b = SeMemToBuf(tmp, size);
	SeFree(tmp);

	return b;
}

// 解読
SE_BUF *SeIkeDecrypt(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam)
{
	void *tmp;
	SE_BUF *b;
	// 引数チェック
	if (data == NULL || cparam == NULL)
	{
		return NULL;
	}

	if ((size % SE_DES_BLOCK_SIZE) != 0)
	{
		// ブロックサイズの整数倍でない
		return NULL;
	}

	tmp = SeMalloc(size);
	SeDes3Decrypt(tmp, data, size, cparam->DesKey, cparam->Iv);

	if (size >= SE_DES_BLOCK_SIZE)
	{
		SeCopy(cparam->NextIv, ((UCHAR *)data) + (size - SE_DES_BLOCK_SIZE), SE_DES_BLOCK_SIZE);
	}
	else
	{
		SeZero(cparam->NextIv, SE_DES_BLOCK_SIZE);
	}

	b = SeMemToBuf(tmp, size);
	SeFree(tmp);

	return b;
}

// IKE パケットの解放
void SeIkeFree(SE_IKE_PACKET *p)
{
	// 引数チェック
	if (p == NULL)
	{
		return;
	}

	if (p->PayloadList != NULL)
	{
		SeIkeFreePayloadList(p->PayloadList);
	}

	if (p->DecryptedPayload != NULL)
	{
		SeFreeBuf(p->DecryptedPayload);
	}

	SeFree(p);
}

// IKE パケットの作成
SE_IKE_PACKET *SeIkeNew(UINT64 init_cookie, UINT64 resp_cookie, UCHAR exchange_type,
						bool encrypted, bool commit, bool auth_only, UINT msg_id,
						SE_LIST *payload_list)
{
	SE_IKE_PACKET *p = SeZeroMalloc(sizeof(SE_IKE_PACKET));

	p->InitiatorCookie = init_cookie;
	p->ResponderCookie = resp_cookie;
	p->ExchangeType = exchange_type;
	p->FlagEncrypted = encrypted;
	p->FlagCommit = commit;
	p->FlagAuthOnly = auth_only;
	p->MessageId = msg_id;
	p->PayloadList = payload_list;

	return p;
}


