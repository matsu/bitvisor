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

// SeIke.h
// 概要: SeIke.c のヘッダ

#ifndef	SEIKE_H
#define SEIKE_H

#ifdef	SE_WIN32
#pragma pack(push, 1)
#endif	// SE_WIN32

// IKE ヘッダ
struct SE_IKE_HEADER
{
	UINT64 InitiatorCookie;						// イニシエータクッキー
	UINT64 ResponderCookie;						// レスポンダクッキー
	UCHAR NextPayload;							// 次のペイロード
	UCHAR Version;								// バージョン
	UCHAR ExchangeType;							// 交換種類
	UCHAR Flag;									// フラグ
	UINT MessageId;								// メッセージ ID
	UINT MessageSize;							// メッセージサイズ
} SE_STRUCT_PACKED;

// IKE バージョン
#define SE_IKE_VERSION					0x10	// 1.0

// IKE ペイロード種類
#define	SE_IKE_PAYLOAD_NONE				0		// ペイロード無し
#define SE_IKE_PAYLOAD_SA				1		// SA ペイロード
#define SE_IKE_PAYLOAD_PROPOSAL			2		// プロポーザルペイロード
#define SE_IKE_PAYLOAD_TRANSFORM		3		// トランスフォームペイロード
#define SE_IKE_PAYLOAD_KEY_EXCHANGE		4		// 鍵交換ペイロード
#define SE_IKE_PAYLOAD_ID				5		// ID ペイロード
#define SE_IKE_PAYLOAD_CERT				6		// 証明書ペイロード
#define SE_IKE_PAYLOAD_CERT_REQUEST		7		// 証明書要求ペイロード
#define SE_IKE_PAYLOAD_HASH				8		// ハッシュペイロード
#define SE_IKE_PAYLOAD_SIGN				9		// 署名ペイロード
#define SE_IKE_PAYLOAD_RAND				10		// 乱数ペイロード
#define SE_IKE_PAYLOAD_NOTICE			11		// 通知ペイロード
#define SE_IKE_PAYLOAD_DELETE			12		// 削除ペイロード
#define SE_IKE_PAYLOAD_VENDOR_ID		13		// ベンダ ID ペイロード

// サポートされているペイロード種類かどうか確認するマクロ
#define SE_IKE_IS_SUPPORTED_PAYLOAD_TYPE(i) (((i) >= SE_IKE_PAYLOAD_SA) && ((i) <= SE_IKE_PAYLOAD_VENDOR_ID))

// IKE 交換種類
#define SE_IKE_EXCHANGE_TYPE_MAIN		2		// Main mode
#define SE_IKE_EXCHANGE_TYPE_AGGRESSIVE	4		// アグレッシブモード
#define SE_IKE_EXCHANGE_TYPE_INFORMATION	5	// 情報交換
#define SE_IKE_EXCHANGE_TYPE_QUICK		32		// クイックモード

// IKE ヘッダフラグ
#define SE_IKE_HEADER_FLAG_ENCRYPTED	1	// 暗号化
#define SE_IKE_HEADER_FLAG_COMMIT		2	// コミット
#define SE_IKE_HEADER_FLAG_AUTH_ONLY	4	// 認証のみ

// IKE ペイロード共通ヘッダ
struct SE_IKE_COMMON_HEADER
{
	UCHAR NextPayload;
	UCHAR Reserved;
	USHORT PayloadSize;
} SE_STRUCT_PACKED;

// IKE SA ペイロードヘッダ
struct SE_IKE_SA_HEADER
{
	UINT DoI;									// DOI 値
	UINT Situation;								// シチュエーション値
} SE_STRUCT_PACKED;

// IKE SA ペイロードにおける DOI 値
#define SE_IKE_SA_DOI_IPSEC				1		// IPsec

// IKE SA ペイロードにおけるシチュエーション値
#define SE_IKE_SA_SITUATION_IDENTITY	1		// 認証のみ

// IKE プロポーザルペイロードヘッダ
struct SE_IKE_PROPOSAL_HEADER
{
	UCHAR Number;								// 番号
	UCHAR ProtocolId;							// プロトコル ID
	UCHAR SpiSize;								// SPI の長さ
	UCHAR NumTransforms;						// トランスフォーム数
} SE_STRUCT_PACKED;

// IKE プロポーザルペイロードヘッダにおけるプロトコル ID
#define SE_IKE_PROTOCOL_ID_IKE			1		// IKE
#define SE_IKE_PROTOCOL_ID_IPSEC_AH		2		// AH
#define SE_IKE_PROTOCOL_ID_IPSEC_ESP	3		// ESP

// IKE トランスフォームペイロードヘッダ
struct SE_IKE_TRANSFORM_HEADER
{
	UCHAR Number;								// 番号
	UCHAR TransformId;							// トランスフォーム ID
	USHORT Reserved;							// 予約
} SE_STRUCT_PACKED;

// IKE トランスフォームペイロードヘッダにおけるトランスフォーム ID (フェーズ 1)
#define SE_IKE_TRANSFORM_ID_P1_KEY_IKE			1	// IKE

// IKE トランスフォームペイロードヘッダにおけるトランスフォーム ID (フェーズ 2)
#define SE_IKE_TRANSFORM_ID_P2_ESP_DES			2	// DES-CBC
#define SE_IKE_TRANSFORM_ID_P2_ESP_3DES			3	// 3DES-CBC

// IKE トランスフォーム値 (固定長)
struct SE_IKE_TRANSFORM_VALUE
{
	UCHAR AfBit;								// AF ビット (1: 固定長, 0: 可変長)
	UCHAR Type;									// 種類
	USHORT Value;								// 値データ (16bit)
} SE_STRUCT_PACKED;

// IKE トランスフォーム値における Type の値 (フェーズ 1)
#define SE_IKE_TRANSFORM_VALUE_P1_CRYPTO		1	// 暗号化アルゴリズム
#define SE_IKE_TRANSFORM_VALUE_P1_HASH			2	// ハッシュアルゴリズム
#define SE_IKE_TRANSFORM_VALUE_P1_AUTH_METHOD	3	// 認証方法
#define SE_IKE_TRANSFORM_VALUE_P1_DH_GROUP		4	// DH グループ番号
#define SE_IKE_TRANSFORM_VALUE_P1_LIFE_TYPE		11	// 有効期限タイプ
#define SE_IKE_TRANSFORM_VALUE_P1_LIFE_VALUE	12	// 有効期限
#define SE_IKE_TRANSFORM_VALUE_P1_KET_SIZE		14	// 鍵サイズ

// IKE トランスフォーム値における Type の値 (フェーズ 2)
#define SE_IKE_TRANSFORM_VALUE_P2_LIFE_TYPE		1	// 有効期限タイプ
#define SE_IKE_TRANSFORM_VALUE_P2_LIFE			2	// 有効期限
#define SE_IKE_TRANSFORM_VALUE_P2_DH_GROUP		3	// DH グループ番号
#define SE_IKE_TRANSFORM_VALUE_P2_CAPSULE		4	// カプセル化モード
#define SE_IKE_TRANSFORM_VALUE_P2_HMAC			5	// HMAC アルゴリズム
#define SE_IKE_TRANSFORM_VALUE_P2_KEY_SIZE		6	// 鍵サイズ

// フェーズ 1: IKE トランスフォーム値における暗号化アルゴリズム
#define SE_IKE_P1_CRYPTO_DES_CBC				1
#define SE_IKE_P1_CRYPTO_3DES_CBC				5

// フェーズ 1: IKE トランスフォーム値におけるハッシュアルゴリズム
#define SE_IKE_P1_HASH_SHA1						2

// フェーズ 1: IKE トランスフォーム値における認証方法
#define SE_IKE_P1_AUTH_METHOD_PRESHAREDKEY		1
#define SE_IKE_P1_AUTH_METHOD_RSA_SIGN			3

// フェーズ 1: IKE トランスフォーム値における DH グループ番号
#define SE_IKE_P1_DH_GROUP_1024_MODP			2

// フェーズ 1: IKE トランスフォーム値における有効期限タイプ
#define SE_IKE_P1_LIFE_TYPE_SECONDS				1
#define SE_IKE_P1_LIFE_TYPE_KILOBYTES			2

// フェーズ 2: IKE トランスフォーム値における HMAC アルゴリズム
#define SE_IKE_P2_HMAC_SHA1						2

// フェーズ 2: IKE トランスフォーム値における DH グループ番号
#define SE_IKE_P2_DH_GROUP_1024_MODP			2

// フェーズ 2: IKE トランスフォーム値におけるカプセル化モード
#define SE_IKE_P2_CAPSULE_TUNNEL				1


// IKE ID ペイロードヘッダ
struct SE_IKE_ID_HEADER
{
	UCHAR IdType;								// ID の種類
	UCHAR ProtocolId;							// プロトコル ID
	USHORT Port;								// ポート
} SE_STRUCT_PACKED;

// IKE ID ペイロードヘッダにおける ID の種類
#define SE_IKE_ID_IPV4_ADDR				1		// IPv4 アドレス (32 bit)
#define SE_IKE_ID_FQDN					2		// FQDN
#define SE_IKE_ID_USER_FQDN				3		// ユーザー FQDN
#define SE_IKE_ID_IPV4_ADDR_SUBNET		4		// IPv4 + サブネット (64 bit)
#define SE_IKE_ID_IPV6_ADDR				5		// IPv6 アドレス (128 bit)
#define SE_IKE_ID_IPV6_ADDR_SUBNET		6		// IPv6 + サブネット (256 bit)
#define SE_IKE_ID_DER_ASN1_DN			9		// X.500 Distinguished Name
#define SE_IKE_ID_DER_ASN1_GN			10		// X.500 General Name

// IKE ID ペイロードにおけるプロトコル ID
#define SE_IKE_ID_PROTOCOL_UDP			SE_IP_PROTO_UDP	// UDP

// IKE 証明書ペイロードヘッダ
struct SE_IKE_CERT_HEADER
{
	UCHAR CertType;								// 証明書タイプ
} SE_STRUCT_PACKED;

// IKE 証明書ペイロードヘッダにおける証明書タイプ
#define SE_IKE_CERT_TYPE_X509			4		// X.509 証明書 (デジタル署名用)

// IKE 証明書ペイロードヘッダ
struct SE_IKE_CERT_REQUEST_HEADER
{
	UCHAR CertType;								// 証明書タイプ
} SE_STRUCT_PACKED;

// IKE 通知ペイロードヘッダ
struct SE_IKE_NOTICE_HEADER
{
	UINT DoI;									// DOI 値
	UCHAR ProtocolId;							// プロトコル ID
												// IKE プロポーザルペイロードヘッダにおけるプロトコル IDと同一
	UCHAR SpiSize;								// SPI サイズ
	USHORT MessageType;							// メッセージタイプ
} SE_STRUCT_PACKED;

// IKE 削除ペイロードヘッダ
struct SE_IKE_DELETE_HEADER
{
	UINT DoI;									// DOI 値
	UCHAR ProtocolId;							// プロトコル ID
	// IKE プロポーザルペイロードヘッダにおけるプロトコル IDと同一
	UCHAR SpiSize;								// SPI サイズ
	USHORT NumSpis;								// SPI 数
} SE_STRUCT_PACKED;


#ifdef	SE_WIN32
#pragma pack(pop)
#endif	// SE_WIN32



//
// IKE 内部データ構造
//

// IKE パケット SA ペイロード
struct SE_IKE_PACKET_SA_PAYLOAD
{
	SE_LIST *PayloadList;						// プロポーザルペイロードリスト
};

// IKE パケットプロポーザルペイロード
struct SE_IKE_PACKET_PROPOSAL_PAYLOAD
{
	UCHAR Number;								// 番号
	UCHAR ProtocolId;							// プロトコル ID
	SE_BUF *Spi;								// SPI データ

	SE_LIST *PayloadList;						// ペイロードリスト
};

// IKE パケットトランスフォームペイロード
struct SE_IKE_PACKET_TRANSFORM_PAYLOAD
{
	UCHAR Number;								// 番号
	UCHAR TransformId;							// トランスフォーム ID

	SE_LIST *ValueList;							// 値リスト
};

// IKE パケットトランスフォーム値
struct SE_IKE_PACKET_TRANSFORM_VALUE
{
	UCHAR Type;									// 種類
	UINT Value;									// 値
};

// IKE 汎用データペイロード
struct SE_IKE_PACKET_DATA_PAYLOAD
{
	SE_BUF *Data;								// 汎用データ
};

// IKE パケット ID ペイロード
struct SE_IKE_PACKET_ID_PAYLOAD
{
	UCHAR Type;									// 種類
	UCHAR ProtocolId;							// プロトコル ID
	USHORT Port;								// ポート番号
	SE_BUF *IdData;								// ID データ
};

// IKE パケット証明書ペイロード
struct SE_IKE_PACKET_CERT_PAYLOAD
{
	UCHAR CertType;								// 証明書種類
	SE_BUF *CertData;							// 証明書データ
};

// IKE パケット証明書要求ペイロード
struct SE_IKE_PACKET_CERT_REQUEST_PAYLOAD
{
	UCHAR CertType;								// 証明書種類
	SE_BUF *Data;								// 要求データ
};

// IKE パケット通知ペイロード
struct SE_IKE_PACKET_NOTICE_PAYLOAD
{
	UCHAR ProtocolId;							// プロトコル ID
	USHORT MessageType;							// メッセージタイプ
	SE_BUF *Spi;								// SPI データ
	SE_BUF *MessageData;						// メッセージデータ
};

// IKE パケット削除ペイロード
struct SE_IKE_PACKET_DELETE_PAYLOAD
{
	UCHAR ProtocolId;							// プロトコル ID
	SE_LIST *SpiList;							// SPI リスト
};

// IKE パケットペイロード
struct SE_IKE_PACKET_PAYLOAD
{
	UCHAR PayloadType;							// ペイロード種類
	UCHAR Padding[3];
	SE_BUF *BitArray;							// ビット列

	union
	{
		SE_IKE_PACKET_SA_PAYLOAD Sa;			// SA ペイロード
		SE_IKE_PACKET_PROPOSAL_PAYLOAD Proposal;	// プロポーザルペイロード
		SE_IKE_PACKET_TRANSFORM_PAYLOAD Transform;	// トランスフォームペイロード
		SE_IKE_PACKET_DATA_PAYLOAD KeyExchange;	// 鍵交換ペイロード
		SE_IKE_PACKET_ID_PAYLOAD Id;			// ID ペイロード
		SE_IKE_PACKET_CERT_PAYLOAD Cert;		// 証明書ペイロード
		SE_IKE_PACKET_CERT_REQUEST_PAYLOAD CertRequest;	// 証明書要求ペイロード
		SE_IKE_PACKET_DATA_PAYLOAD Hash;		// ハッシュペイロード
		SE_IKE_PACKET_DATA_PAYLOAD Sign;		// 署名ペイロード
		SE_IKE_PACKET_DATA_PAYLOAD Rand;		// 乱数ペイロード
		SE_IKE_PACKET_NOTICE_PAYLOAD Notice;	// 通知ペイロード
		SE_IKE_PACKET_DELETE_PAYLOAD Delete;	// 削除ペイロード
		SE_IKE_PACKET_DATA_PAYLOAD VendorId;	// ベンダ ID ペイロード
		SE_IKE_PACKET_DATA_PAYLOAD GeneralData;	// 汎用データペイロード
	} Payload;
};

// IKE パケットデータ構造
struct SE_IKE_PACKET
{
	UINT64 InitiatorCookie;						// イニシエータクッキー
	UINT64 ResponderCookie;						// レスポンダクッキー
	UCHAR ExchangeType;							// 交換種類
	bool FlagEncrypted;							// 暗号化フラグ
	bool FlagCommit;							// コミットフラグ
	bool FlagAuthOnly;							// 認証のみフラグ
	UINT MessageId;								// メッセージ ID
	SE_LIST *PayloadList;						// ペイロードリスト
	SE_BUF *DecryptedPayload;					// 解読されたペイロード
};

// IKE 暗号化パラメータ
struct SE_IKE_CRYPTO_PARAM
{
	SE_DES_KEY *DesKey;							// DES 鍵
	UCHAR Iv[SE_DES_IV_SIZE];					// IV
	UCHAR NextIv[SE_DES_IV_SIZE];				// 次に使用すべき IV
};

// IP アドレス
struct SE_IKE_IP_ADDR
{
	union
	{
		SE_IPV4_ADDR Ipv4;						// IPv4 アドレス
		SE_IPV6_ADDR Ipv6;						// IPv6 アドレス
	} Address;
	bool IsIPv6;								// IPv6 フラグ
};


// 関数プロトタイプ
SE_IKE_PACKET *SeIkeParseHeader(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam);
SE_IKE_PACKET *SeIkeParse(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam);
SE_IKE_PACKET *SeIkeParseEx(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam, bool header_only);
void SeIkeFree(SE_IKE_PACKET *p);
SE_IKE_PACKET *SeIkeNew(UINT64 init_cookie, UINT64 resp_cookie, UCHAR exchange_type,
						bool encrypted, bool commit, bool auth_only, UINT msg_id,
						SE_LIST *payload_list);

SE_BUF *SeIkeEncrypt(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam);
SE_BUF *SeIkeEncryptWithPadding(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam);
SE_BUF *SeIkeDecrypt(void *data, UINT size, SE_IKE_CRYPTO_PARAM *cparam);

SE_LIST *SeIkeParsePayloadList(void *data, UINT size, UCHAR first_payload);
SE_LIST *SeIkeParsePayloadListEx(void *data, UINT size, UCHAR first_payload, UINT *total_read_size);
void SeIkeFreePayloadList(SE_LIST *o);
UINT SeIkeGetPayloadNum(SE_LIST *o, UINT payload_type);
SE_IKE_PACKET_PAYLOAD *SeIkeGetPayload(SE_LIST *o, UINT payload_type, UINT index);

SE_IKE_PACKET_PAYLOAD *SeIkeParsePayload(UINT payload_type, SE_BUF *b);
void SeIkeFreePayload(SE_IKE_PACKET_PAYLOAD *p);
bool SeIkeParseDataPayload(SE_IKE_PACKET_DATA_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeDataPayload(SE_IKE_PACKET_DATA_PAYLOAD *t);
bool SeIkeParseSaPayload(SE_IKE_PACKET_SA_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeSaPayload(SE_IKE_PACKET_SA_PAYLOAD *t);
bool SeIkeParseProposalPayload(SE_IKE_PACKET_PROPOSAL_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeProposalPayload(SE_IKE_PACKET_PROPOSAL_PAYLOAD *t);
bool SeIkeParseTransformPayload(SE_IKE_PACKET_TRANSFORM_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeTransformPayload(SE_IKE_PACKET_TRANSFORM_PAYLOAD *t);
SE_LIST *SeIkeParseTransformValueList(SE_BUF *b);
void SeIkeFreeTransformValueList(SE_LIST *o);
bool SeIkeParseIdPayload(SE_IKE_PACKET_ID_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeIdPayload(SE_IKE_PACKET_ID_PAYLOAD *t);
bool SeIkeParseCertPayload(SE_IKE_PACKET_CERT_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeCertPayload(SE_IKE_PACKET_CERT_PAYLOAD *t);
bool SeIkeParseCertRequestPayload(SE_IKE_PACKET_CERT_REQUEST_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeCertRequestPayload(SE_IKE_PACKET_CERT_REQUEST_PAYLOAD *t);
bool SeIkeParseNoticePayload(SE_IKE_PACKET_NOTICE_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeNoticePayload(SE_IKE_PACKET_NOTICE_PAYLOAD *t);
bool SeIkeParseDeletePayload(SE_IKE_PACKET_DELETE_PAYLOAD *t, SE_BUF *b);
void SeIkeFreeDeletePayload(SE_IKE_PACKET_DELETE_PAYLOAD *t);

SE_IKE_PACKET_PAYLOAD *SeIkeNewPayload(UINT payload_type);
SE_IKE_PACKET_PAYLOAD *SeIkeNewDataPayload(UCHAR payload_type, void *data, UINT size);
SE_IKE_PACKET_PAYLOAD *SeIkeNewSaPayload(SE_LIST *payload_list);
SE_IKE_PACKET_PAYLOAD *SeIkeNewProposalPayload(UCHAR number, UCHAR protocol_id, void *spi, UINT spi_size, SE_LIST *payload_list);
SE_IKE_PACKET_PAYLOAD *SeIkeNewTransformPayload(UCHAR number, UCHAR transform_id, SE_LIST *value_list);
SE_IKE_PACKET_TRANSFORM_VALUE *SeIkeNewTransformValue(UCHAR type, UINT value);
SE_IKE_PACKET_PAYLOAD *SeIkeNewIdPayload(UCHAR id_type, UCHAR protocol_id, USHORT port, void *id_data, UINT id_size);
SE_IKE_PACKET_PAYLOAD *SeIkeNewCertPayload(UCHAR cert_type, void *cert_data, UINT cert_size);
SE_IKE_PACKET_PAYLOAD *SeIkeNewCertRequestPayload(UCHAR cert_type, void *data, UINT size);
SE_IKE_PACKET_PAYLOAD *SeIkeNewNoticePayload(UCHAR protocol_id, USHORT message_type,
											 void *spi, UINT spi_size,
											 void *message, UINT message_size);
SE_IKE_PACKET_PAYLOAD *SeIkeNewDeletePayload(UCHAR protocol_id, SE_LIST *spi_list);

UCHAR SeIkeGetFirstPayloadType(SE_LIST *o);
SE_BUF *SeIkeBuild(SE_IKE_PACKET *p, SE_IKE_CRYPTO_PARAM *cparam);
SE_BUF *SeIkeBuildPayloadList(SE_LIST *o);
SE_BUF *SeIkeBuildPayload(SE_IKE_PACKET_PAYLOAD *p);
SE_BUF *SeIkeBuildDataPayload(SE_IKE_PACKET_DATA_PAYLOAD *t);
SE_BUF *SeIkeBuildSaPayload(SE_IKE_PACKET_SA_PAYLOAD *t);
SE_BUF *SeIkeBuildProposalPayload(SE_IKE_PACKET_PROPOSAL_PAYLOAD *t);
SE_BUF *SeIkeBuildTransformPayload(SE_IKE_PACKET_TRANSFORM_PAYLOAD *t);
SE_BUF *SeIkeBuildTransformValue(SE_IKE_PACKET_TRANSFORM_VALUE *v);
SE_BUF *SeIkeBuildTransformValueList(SE_LIST *o);
SE_BUF *SeIkeBuildIdPayload(SE_IKE_PACKET_ID_PAYLOAD *t);
SE_BUF *SeIkeBuildCertPayload(SE_IKE_PACKET_CERT_PAYLOAD *t);
SE_BUF *SeIkeBuildCertRequestPayload(SE_IKE_PACKET_CERT_REQUEST_PAYLOAD *t);
SE_BUF *SeIkeBuildNoticePayload(SE_IKE_PACKET_NOTICE_PAYLOAD *t);
SE_BUF *SeIkeBuildDeletePayload(SE_IKE_PACKET_DELETE_PAYLOAD *t);

void SeIkeInitIPv4Address(SE_IKE_IP_ADDR *a, SE_IPV4_ADDR *addr);
void SeIkeInitIPv6Address(SE_IKE_IP_ADDR *a, SE_IPV6_ADDR *addr);
SE_IPV4_ADDR SeIkeGetIPv4Address(SE_IKE_IP_ADDR *a);
SE_IPV6_ADDR SeIkeGetIPv6Address(SE_IKE_IP_ADDR *a);
void SeIkeIpAddressToStr(char *str, SE_IKE_IP_ADDR *a);
bool SeIkeIsZeroIP(SE_IKE_IP_ADDR *a);

UINT SeIkeStrToPhase1Mode(char *name);
UCHAR SeIkeStrToPhase1CryptId(char *name);
UCHAR SeIkeStrToPhase1HashId(char *name);
UCHAR SeIkeStrToPhase2CryptId(char *name);
UCHAR SeIkeStrToPhase2HashId(char *name);
SE_BUF *SeIkeStrToPassword(char *str);
UINT SeIkePhase1CryptIdToKeySize(UCHAR id);
UINT SeIkePhase2CryptIdToKeySize(UCHAR id);


#endif	// SEIKE_H

