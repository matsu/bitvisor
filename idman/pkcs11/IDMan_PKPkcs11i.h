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

/**
 * PKCS#11ライブラリ内部定義ヘッダファイル
 * \file IDMan_PKPkcs11i.h
 */



/* 標準ライブラリ */


/* 外部ライブラリ */


/* 作成ライブラリ */


#ifndef _IDMAN_PKPKCS11I_H_
#define _IDMAN_PKPKCS11I_H_



#define CK_INFO_CK_VER_MAJ 2		/* PKCS#11のバージョン（整数） */
#define CK_INFO_CK_VER_MIN 20		/* PKCS#11のバージョン（小数） */
#define CK_INFO_MAN_ID "TOYOTA"		/* ライブラリの製造者ID（32バイト空白パディング） */
#define CK_INFO_FLAG 0			/* フラグ（固定値0） */
#define CK_INFO_LIB_DCR "TOYOTA PKCS#11"		/* ライブラリの記述（32バイト空白パディング） */
#define CK_INFO_LIB_VER_MAJ 1		/* ライブラリのバージョン（整数） */
#define CK_INFO_LIB_VER_MIN 0		/* ライブラリのバージョン（小数） */


/* VAL_TYPE定義 */
#define	VAL_TYPE_DEFAULT		0x00000000
#define	VAL_TYPE_HEAD			0x00000001
#define	VAL_TYPE_SESSION_DATA	0x00000002
#define	VAL_TYPE_SLOT_DATA		0x00000003
#define	VAL_TYPE_TOKEN_DATA		0x00000004
#define	VAL_TYPE_ATTRIBUTE		0x00000005
#define	VAL_TYPE_TLV_DATA		0x00000006


/* ATTR_VAL_TYPE定義 */
#define	ATTR_VAL_TYPE_NUM			0x00000000
#define	ATTR_VAL_TYPE_BYTE			0x00000001
#define	ATTR_VAL_TYPE_BYTE_ARRAY	0x00000002
#define	ATTR_VAL_TYPE_INVALID		0xFFFFFFFF



/* リスト要素部 */
typedef struct CK_I_ELEM
{
	CK_ULONG key;
	CK_VOID_PTR val;
	CK_ULONG valType;
	struct CK_I_ELEM *next;
} CK_I_ELEM;

typedef CK_I_ELEM *CK_I_ELEM_PTR;


/* リストヘッダ部 */
typedef struct CK_I_HEAD
{
	CK_ULONG num;	//要素数
	CK_I_ELEM_PTR elem;
} CK_I_HEAD;

typedef CK_I_HEAD *CK_I_HEAD_PTR;
typedef CK_I_HEAD_PTR *CK_I_HEAD_PTR_PTR;



/* TLVデータ */
typedef struct CK_I_TLV_DATA
{
	CK_BYTE tag;		//Tag
	CK_ULONG len;		//Length
	CK_BYTE val[256];	//Value
} CK_I_TLV_DATA;

typedef CK_I_TLV_DATA *CK_I_TLV_DATA_PTR;



/* DIRデータ */
typedef struct CK_I_DIR_DATA
{
	CK_BYTE pkcs11DfAid[256];
	CK_ULONG pkcs11DfAidLen;
	CK_BYTE idFuncDfAid[256];
	CK_ULONG idFuncDfAidLen;
} CK_I_DIR_DATA;

typedef CK_I_DIR_DATA *CK_I_DIR_DATA_PTR;



/* ODFデータ */
typedef struct CK_I_ODF_DATA
{
	CK_BYTE prkEfid[4];
	CK_BYTE pukEfid[4];
	CK_BYTE sekEfid[4];
	CK_BYTE trcEfid[4];
	CK_BYTE pucEfid[4];
	CK_BYTE aoEfid[4];
	CK_BYTE doEfid[4];
} CK_I_ODF_DATA;

typedef CK_I_ODF_DATA *CK_I_ODF_DATA_PTR;



/* トークンデータ */
typedef struct CK_I_TOKEN_DATA
{
	CK_I_DIR_DATA_PTR dirData;			//DIRデータ
	CK_I_ODF_DATA_PTR odfData;			//ODFデータ
	CK_TOKEN_INFO_PTR tokenInfo;	//トークン情報
	CK_I_HEAD_PTR mechanismTable;	//メカニズムテーブル
	CK_I_HEAD_PTR objectTable;		//オブジェクトテーブル
} CK_I_TOKEN_DATA;

typedef CK_I_TOKEN_DATA *CK_I_TOKEN_DATA_PTR;



/* スロットデータ */
typedef struct CK_I_SLOT_DATA
{
	CK_SLOT_ID slotID;
	CK_SLOT_INFO_PTR slotInfo;			//スロット情報
	CK_I_TOKEN_DATA_PTR tokenData;		//トークンデータ
	CK_CHAR reader[256];
	CK_ULONG dwReaderLen;
	CK_ULONG hCard;						//カードハンドル
	CK_ULONG dwActiveProtocol;			//カード接続プロトコル
} CK_I_SLOT_DATA;

typedef CK_I_SLOT_DATA *CK_I_SLOT_DATA_PTR;



/* セッションデータ */
typedef struct CK_I_SESSION_DATA
{
	CK_SESSION_INFO_PTR sessionInfo;	//セッション情報
	CK_BYTE pin[256];					//PIN
	CK_ULONG ulPinLen;					//PINの長さ
	CK_BBOOL srchFlg;					//検索中フラグ
	CK_I_HEAD_PTR srchObject;			//検索条件オブジェクト
	CK_MECHANISM_TYPE signMechanism;	//Signメカニズム
	CK_I_HEAD_PTR signObject;			//Sign鍵オブジェクト
	CK_BYTE signData[2048];				//Signデータ
	CK_ULONG signDataLen;				//Signデータ長
	CK_BYTE signSignature[2048];		//Sign署名データ
	CK_ULONG signSignatureLen;			//Sign署名データ長
	CK_MECHANISM_TYPE digestMechanism;	//Digestメカニズム
	CK_BYTE digestInData[2048];			//Digest入力データ
	CK_ULONG digestInDataLen;			//Digest入力データ長
	CK_MECHANISM_TYPE verifyMechanism;	//Verifyメカニズム
	CK_I_HEAD_PTR verifyObject;			//Verify鍵オブジェクト
} CK_I_SESSION_DATA;

typedef CK_I_SESSION_DATA *CK_I_SESSION_DATA_PTR;



#endif
