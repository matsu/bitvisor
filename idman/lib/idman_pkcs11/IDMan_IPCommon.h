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

#include "IDMan_PKPkcs11.h"
#include "IDMan_StandardIo.h"

#define  IDMAN_IPMAIN
#include "IDMan_IPLibrary.h"

#include <common/string.h>		/* merge */
#define  SLOT_CNT				1				//スロット件数

#define  CERSIG_FORMAT_START	0x6A			//証明書署名フォーマット先頭バイト
#define  CERSIG_FORMAT_END		0xBC			//証明書署名フォーマット末尾バイト
#define  CERSIG_FORMAT_LEN		106				//証明書署名フォーマット証明書レングス
#define  CERSIG_ALGORITHM		0x220			//証明書署名時のハッシュアルゴリズム
#define  CERSIG_HASH_SIZE		20				//証明書署名時のハッシュサイズ
#define  CERSIG_PADDING_SIZE	128				//証明書署名のパディングサイズ

//戻り値
#define  RET_IPOK							0	//正常
#define  RET_IPNG_TOKEN_NOT_RECOGNIZED		-1	//不正ICカードが挿入されている場合
#define  RET_IPNG_TOKEN_NOT_PRESENT			-2	//ICカードリーダにICカードが挿入されてない場合
#define  RET_IPNG_DEVICE_REMOVED			-3	//ICカードリーダが途中で差し替えられた場合
#define  RET_IPNG_PIN_INCORREC				-4	//PIN番号が間違っている場合
#define  RET_IPNG_PIN_LOCKED				-5	//PINがロックされている場合
#define  RET_IPNG							-6	//異常
#define  RET_IPNG_NO_MATCHING				-7	//該当データなし
#define  RET_IPNG_INITIALIZE_REPEAT			-8	//初期化済みの場合
#define  RET_IPNG_NO_INITIALIZE				-9	//初期化処理未実施の場合
#define  RET_IPNG_NO_READER					-10	//リーダ未接続の場合
#define  RET_IPOK_NO_VERIFY_SIGNATURE		1	//正常終了（署名の検証に失敗）
#define  RET_IPOK_NO_VERIFY_CERT			2	//公開鍵証明書の検証に失敗
#define  RET_IPOK_NO_VERIFY_CERTX			1	//正常終了（公開鍵証明書の検証に失敗）
#define  RET_IPOK_NO_CRL_ERR				2	//正常終了（公開鍵証明書が失効している）
#define  RET_IPOK_NO_VERIFY_CERTXKEY		3	//正常終了（チャレンジ＆レスポンスの認証に失敗）
#define  RET_IPOK_NO_VERIFY					1	//正常終了（検証失敗）

//MAX値
#define  OBJECT_HANDLE_MAX					1024
#define  SUBJECT_DN_MAX						1024
#define  ISSUER_DN_MAX						1024


//パディング変換関数のハッシュアルゴリズムID
#define	ALGORITHM_ID_SHA1		"\x30\x21\x30\x09\x06\x05\x2b\x0e\x03\x02\x1a\x05\x00\x04\x14"
#define	ALGORITHM_ID_SHA256		"\x30\x31\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01\x05\x00\x04\x20"
#define	ALGORITHM_ID_SHA512		"\x30\x51\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x03\x05\x00\x04\x40"

//パディング変換関数のハッシュアルゴリズムIDレングス
#define	ALGORITHM_IDLEN_SHA1	15
#define	ALGORITHM_IDLEN_SHA256	19
#define	ALGORITHM_IDLEN_SHA512	19

#ifdef DEBUG
#define DEBUG_OutPut(message); IDMan_CmDebugOutPut(message);
#else
#define DEBUG_OutPut(message);
#endif

int IDMan_IPRetJudgment (CK_RV prv);
int IDMan_IPgetCertPKCS (CK_SESSION_HANDLE Sessinhandle, char *subjectDN,
			 char *issuerDN, void **ptrCert, long *lCert,
			 void **ptrKeyID, long *lKeyID, int getCert);
int IDMan_IPgetCertIdxPKCS (CK_SESSION_HANDLE Sessinhandle, int iPkcIndex,
			    void **ptrCert, long *lCert, void **ptrKeyID,
			    long *lKeyID, int getCert);
int IDMan_IPIdPassListMalloc (idPasswordList **list, unsigned long int IDLen,
			      unsigned long int passwordLen);
int IDMan_IPCheckPKC (CK_SESSION_HANDLE Sessinhandle, unsigned char *pPKC,
		      unsigned short int PKCLen, int Algorithm,
		      CK_OBJECT_HANDLE *pKeyObjectHandle);
