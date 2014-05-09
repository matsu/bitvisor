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

// SeSec.h
// 概要: SeSec.c のヘッダ

#ifndef	SESEC_H
#define SESEC_H
//
// 定数
//

// ベンダ ID 文字列
#define SE_SEC_VENDOR_ID_STR					"UNIVERSITYOF TSUKUBA"

// ポート番号
#define SE_SEC_IKE_UDP_PORT						500		// ポート番号

// 認証メソッド
#define SE_SEC_AUTH_METHOD_PASSWORD				1		// パスワード認証
#define SE_SEC_AUTH_METHOD_CERT					2		// 証明書認証

// VPN 設定のデフォルト値
#define SE_SEC_DEFAULT_P1_LIFE_SECONDS			7200
#define SE_SEC_DEFAULT_P2_LIFE_SECONDS			7200
#define	SE_SEC_DEFAULT_CONNECT_TIMEOUT			5
#define SE_SEC_DEFAULT_IDLE_TIMEOUT				300
#define SE_SEC_DEFAULT_PING_INTERVAL			12
#define SE_SEC_DEFAULT_PING_MSG_SIZE			32
#define SE_SEC_DEFAULT_WAIT_P2_BLANK_SPAN		1

// 定期的ポーリング間隔
#define SE_SEC_POLLING_INTERVAL					500


//
// データ構造
//

// VPN 設定
struct SE_SEC_CONFIG
{
	SE_IKE_IP_ADDR MyIpAddress;		// 自分の IP アドレス
	SE_IKE_IP_ADDR MyVirtualIpAddress;	// 自分の仮想 IP アドレス
	SE_IKE_IP_ADDR VpnGatewayAddress;	// 接続先 IPsec VPN ゲートウェイの IP アドレス
	UINT VpnAuthMethod;				// ユーザー認証方法
	char VpnPassword[MAX_SIZE];		// パスワード認証を用いる場合の事前共有鍵
	char VpnIdString[MAX_SIZE];		// パスワード認証を用いる場合に送出する ID 情報
	char VpnCertName[MAX_SIZE];		// 証明書認証を用いる場合に使用する X.509 証明書ファイルの名前
	char VpnCaCertName[MAX_SIZE];	// 接続先 VPN サーバーから返された X.509 証明書を検証する CA 証明書のファイル名
	char VpnRsaKeyName[MAX_SIZE];	// VpnCertName で指定した X.509 証明書に対応した RSA 秘密鍵の名前
	UINT VpnPhase1Mode;				// VPN phase one mode (Main or Aggressive)
	UCHAR VpnPhase1Crypto;			// フェーズ 1 における暗号化アルゴリズム
	UCHAR VpnPhase1Hash;			// フェーズ 1 における署名アルゴリズム
	UINT VpnPhase1LifeKilobytes;	// ISAKMP SA の有効期限の値 (単位: キロバイト, 0 の場合は無効)
	UINT VpnPhase1LifeSeconds;		// ISAKMP SA の有効期限の値 (単位: 秒, 0 の場合は無効)
	UINT VpnWaitPhase2BlankSpan;	// フェーズ 1 完了からフェーズ 2 開始までの間にあける時間 (単位: ミリ秒)
	UCHAR VpnPhase2Crypto;			// フェーズ 2 における暗号化アルゴリズム
	UCHAR VpnPhase2Hash;			// フェーズ 2 における署名アルゴリズム
	UINT VpnPhase2LifeKilobytes;	// ISAKMP SA の有効期限の値 (単位: キロバイト, 0 の場合は無効)
	UINT VpnPhase2LifeSeconds;		// ISAKMP SA の有効期限の値 (単位: 秒, 0 の場合は無効)
	UINT VpnConnectTimeout;			// VPN の接続処理を開始してから接続失敗とみなすまでのタイムアウト (秒)
	UINT VpnIdleTimeout;			// VPN 接続完了後、一定時間無通信の場合にトンネルが消えたと仮定して再接続するまでのアイドルタイムアウト (単位: 秒)
	SE_IKE_IP_ADDR VpnPingTarget;	// VPN 接続完了後、定期的に ping を送信する宛先ホストの IP アドレス
	UINT VpnPingInterval;			// VpnPingTarget で指定した宛先に ping を送信する間隔 (秒)
	UINT VpnPingMsgSize;			// VpnPingTarget で指定した宛先に ping を送信する際の ICMP メッセージサイズ (バイト数)
	bool VpnSpecifyIssuer;			// 証明書認証を用いる場合に証明書要求フィールドに自分の証明書の発行者名を明記するかどうか
};

// クライアント提供関数テーブル
struct SE_SEC_CLIENT_FUNCTION_TABLE
{
	// 現在時刻取得関数
	UINT64 (*ClientGetTick)(void *param);
	// タイマコールバック設定関数
	void (*ClientSetTimerCallback)(SE_SEC_TIMER_CALLBACK *callback, void *callback_param, void *param);
	// タイマ追加関数
	void (*ClientAddTimer)(UINT interval, void *param);
	// UDP パケット受信コールバック設定関数
	void (*ClientSetRecvUdpCallback)(SE_SEC_UDP_RECV_CALLBACK *callback, void *callback_param, void *param);
	// ESP パケット受信コールバック設定関数
	void (*ClientSetRecvEspCallback)(SE_SEC_ESP_RECV_CALLBACK *callback, void *callback_param, void *param);
	// 仮想 IP パケット受信コールバック設定関数
	void (*ClientSetRecvVirtualIpCallback)(SE_SEC_VIRTUAL_IP_RECV_CALLBACK *callback, void *callback_param, void *param);
	// UDP パケット送信関数
	void (*ClientSendUdp)(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param);
	// ESP パケット送信関数
	void (*ClientSendEsp)(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param);
	// 仮想 IP パケット送信関数
	void (*ClientSendVirtualIp)(void *data, UINT size, void *param);
};

// IKE P1 鍵セット
struct SE_IKE_P1_KEYSET
{
	SE_BUF *SKEYID_d;									// IPsec SA 用鍵
	SE_BUF *SKEYID_a;									// IKE SA 認証用鍵
	SE_BUF *SKEYID_e;									// IKE SA 暗号化用鍵
};

// IKE SA
struct SE_IKE_SA
{
	UINT Id;
	SE_IKE_IP_ADDR SrcAddr, DestAddr;
	UINT SrcPort, DestPort;
	UINT64 InitiatorCookie;
	UINT64 ResponderCookie;
	UINT64 TransferBytes;								// 転送バイト数
	UINT Phase;											// フェーズ番号
	UINT Status;										// Phase one status (number of packet being sent/proceed)
	SE_DH *Dh;											// DH
	SE_BUF *Phase1MyRand;								// P1 での自分の乱数
	SE_BUF *Phase1Password;								// P1 でのパスワード
	SE_BUF *Phase2MyRand;								// P2 での自分の乱数
	SE_BUF *Phase2YourRand;								// P2 での相手の乱数
	SE_BUF *SAi_b, *IDii_b;
	UINT64 ConnectTimeoutTick;							// 接続タイムアウトが発生する時刻
	UCHAR SKEYID[SE_SHA1_HASH_SIZE];					// Key, all other keys are derived from
	SE_IKE_P1_KEYSET P1KeySet;							// フェーズ 1 用鍵セット
	UCHAR Phase1Iv[SE_DES_BLOCK_SIZE];					// フェーズ 1 用 IV
	UINT Phase2MessageId;								// フェーズ 2 用最終メッセージ ID
	SE_DES_KEY *Phase2DesKey;							// フェーズ 2 用 DES 鍵
	UINT64 Phase2StartTick;								// フェーズ 2 を開始する時刻
	bool Phase2Started;									// フェーズ 2 開始フラグ
	UINT MySpi;											// 自分の SPI
	bool Established;									// VPN 接続が確立されているかどうか
	UCHAR Phase2Iv[SE_DES_BLOCK_SIZE];					// フェーズ 2 用 IV
	UINT YourSpi;										// 相手の SPI
	SE_BUF *MyKEYMAT, *YourKEYMAT;						// 自分と相手の KEYMAT
	UINT64 LastCommTick;								// 最終通信日時
	UINT64 Phase1EstablishedTick;						// フェーズ 1 確立完了時刻
	bool DeleteNow;										// 削除フラグ
	SE_CERT *MyCert;									// 自分の証明書
	SE_CERT *CaCert;									// CA の証明書
};

// IPsec SA
struct SE_IPSEC_SA
{
	SE_IKE_IP_ADDR SrcAddr, DestAddr;
	bool Outgoing;										// true のとき送信方向, false のとき受信方向
	UINT Spi;											// SPI
	UCHAR NextIv[SE_DES_BLOCK_SIZE];					// 次の IV
	SE_IKE_SA *IkeSa;									// IKE SA へのポインタ
	UINT64 EstablishedTick;								// 確立完了時刻
	UINT64 TransferBytes;								// 転送バイト数
	UINT Seq;											// シーケンス番号
	SE_BUF *EncryptionKey;								// 暗号化鍵
	SE_BUF *HashKey;									// ハッシュ鍵
	SE_DES_KEY *DesKey;									// DES 鍵
};

// IPsec 処理構造体
struct SE_SEC
{
	SE_VPN *Vpn;										// VPN オブジェクト
	SE_SEC_CLIENT_FUNCTION_TABLE ClientFunctions;		// クライアント提供関数テーブル
	void *Param;										// クライアント指定パラメータ
	SE_SEC_CONFIG Config;								// 設定データ
	bool IPv6;											// IPv6 モード
	SE_LIST *IkeSaList;									// IKE SA リスト
	UINT64 NextConnectStartTick;						// 次の接続開始時刻
	SE_LIST *IPsecSaList;								// IPsec SA リスト
	bool Halting;										// 停止中
	UINT64 PoolingVar;									// ポーリング用変数
	bool StatusChanged;									// 状態変化
	bool SendStrictIdV6;								// IPv6 において厳密に ID を送信する
};

// 関数プロトタイプ
UINT64 SeSecTick(SE_SEC *s);
void SeSecSetTimerCallback(SE_SEC *s, SE_SEC_TIMER_CALLBACK *callback);
void SeSecAddTimer(SE_SEC *s, UINT interval);
void SeSecSetRecvUdpCallback(SE_SEC *s, SE_SEC_UDP_RECV_CALLBACK *callback);
void SeSecSetRecvEspCallback(SE_SEC *s, SE_SEC_ESP_RECV_CALLBACK *callback);
void SeSecSetRecvVirtualIpCallback(SE_SEC *s, SE_SEC_VIRTUAL_IP_RECV_CALLBACK *callback);
void SeSecSendUdp(SE_SEC *s, SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size);
void SeSecSendEsp(SE_SEC *s, SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size);
void SeSecSendVirtualIp(SE_SEC *s, void *data, UINT size);

UINT SeSecStrToAuthMethod(char *str);

SE_SEC *SeSecInit(SE_VPN *vpn, bool ipv6, SE_SEC_CONFIG *config, SE_SEC_CLIENT_FUNCTION_TABLE *function_table, void *param, bool send_strict_id);
void SeSecFree(SE_SEC *s);

void SeSecTimerCallback(UINT64 tick, void *param);

UINT SeSecNewSpi();

void SeSecUdpRecvCallback(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, UINT dest_port, UINT src_port, void *data, UINT size, void *param);
void SeSecEspRecvCallback(SE_IKE_IP_ADDR *dest_addr, SE_IKE_IP_ADDR *src_addr, void *data, UINT size, void *param);
void SeSecVirtualIpRecvCallback(void *data, UINT size, void *param);
void SeSecInitMain(SE_SEC *s);
void SeSecFreeMain(SE_SEC *s);
void SeSecProcessMain(SE_SEC *s);
void SeSecForceReconnect(SE_SEC *s);
bool SeSecDoInterval(SE_SEC *s, UINT64 *var, UINT interval);
void SeSecDeleteExpiredIPsecSa(SE_SEC *s);
void SeSecDeleteExpiredIkeSa(SE_SEC *s);
bool SeSecCheckIkeSaExpires(SE_SEC *s, SE_IKE_SA *sa);
bool SeSecCheckIPsecSaExpires(SE_SEC *s, SE_IPSEC_SA *sa);

void SeSecInitSaList(SE_SEC *s);
void SeSecFreeSaList(SE_SEC *s);
int SeSecCmpIkeSa(void *p1, void *p2);
SE_IKE_SA *SeSecSearchIkeSa(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
							UINT src_port, UINT dest_port, UINT64 init_cookie, UINT64 resp_cookie);
SE_IKE_SA *SeSecSearchIkeSaBySpi(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
								 UINT src_port, UINT dest_port, void *spi_buf);
SE_IPSEC_SA *SeSecSearchIPsecSaBySpi(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
									 UINT spi);
SE_IKE_SA *SeSecNewIkeSa(SE_SEC *s, SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
					  UINT src_port, UINT dest_port, UINT64 init_cookie);
void SeSecFreeIkeSa(SE_SEC *s, SE_IKE_SA *sa);
UINT64 SeSecGenIkeSaInitCookie(SE_SEC *s);
UINT SeSecGenIkeSaMessageId(SE_SEC *s);
UINT SeSecGenIPsecSASpi(SE_SEC *s);

void SeSecFreeP1KeySet(SE_IKE_P1_KEYSET *set);

bool SeSecSendMain1(SE_SEC *s);
void SeSecRecvMain2(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size);
void SeSecSendMain3(SE_SEC *s, SE_IKE_SA *sa);
void SeSecRecvMain4(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size);
void SeSecSendMain5(SE_SEC *s, SE_IKE_SA *sa);
void SeSecRecvMain6(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size);
bool SeSecSendAggr(SE_SEC *s);
void SeSecRecvAggr(SE_SEC *s, SE_IKE_SA *sa, void *data, UINT size);

bool SeSecStartVpnConnect(SE_SEC *s);
void SeSecProcessIkeSaMsg(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size);
void SeSecProcessIkeSaMsgPhase1(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size);
void SeSecCalcP1KeySet(SE_IKE_P1_KEYSET *set, void *skeyid, UINT skeyid_size, void *dh_key, UINT dh_key_size, UINT64 init_cookie, UINT64 resp_cookie,
					   UINT request_e_key_size);
SE_BUF *SeSecCalcKa(void *skeyid_e, UINT skeyid_e_size, UINT request_size);
void SeSecSendIkeSaMsgPhase2(SE_SEC *s, SE_IKE_SA *sa);
void SeSecCalcP1Iv(void *iv, void *g_xi, UINT g_xi_size, void *g_xr, UINT g_xr_size, UINT request_size);
void SeSecCalcP2Iv(void *iv, void *p1_iv, UINT p1_iv_size, UINT msg_id, UINT request_size);
void SeSecProcessIkeSaMsgPhase2(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size);
void SeSecProcessIkeSaMsgInfo(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET *packet_header, void *data, UINT size);
void SeSecSendIkeSaMsgPhase2FinalHash(SE_SEC *s, SE_IKE_SA *sa);
void SeSecSendIkeSaMsgInfo(SE_SEC *s, SE_IKE_SA *sa, SE_LIST *payload_list_src);
void SeSecSendIkeSaMsgInfoSimple(SE_SEC *s, SE_IKE_SA *sa, SE_IKE_PACKET_PAYLOAD *payload);
void SeSecSendIkeSaDeleteMsg(SE_SEC *s, SE_IKE_SA *sa);
void SeSecSendIPsecSaDeleteMsg(SE_SEC *s, SE_IPSEC_SA *sa, SE_IKE_SA *ike_sa);

SE_BUF *SeSecCalcKEYMAT(SE_BUF *skeyid_d, UCHAR protocol, UINT spi,
						SE_BUF *my_rand, SE_BUF *your_rand, UINT request_size);
SE_BUF *SeSecCalcKEYMATFull(SE_BUF *skeyid_d, void *keymat_src, UINT keymat_src_size, UINT request_size);

SE_IPSEC_SA *SeSecNewIPsecSa(SE_SEC *s, SE_IKE_SA *ike_sa, bool outgoing, UINT spi,
							 SE_IKE_IP_ADDR src_addr, SE_IKE_IP_ADDR dest_addr,
							 SE_BUF *keymat);
void SeSecFreeIPsecSa(SE_SEC *s, SE_IPSEC_SA *sa);
UINT64 SeSecLifeSeconds64bit(UINT value);

SE_IPSEC_SA *SeSecGetIPsecSa(SE_SEC *s, bool outgoing);

#endif	// SESEC_H

