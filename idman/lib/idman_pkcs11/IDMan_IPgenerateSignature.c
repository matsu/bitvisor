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
 * 電子署名生成プログラム
 * \file IDMan_IPgenerateSignature.c
 */

/* 作成ライブラリ関数名 */
#include "IDMan_IPCommon.h"

int IDMan_IPgSParamChk( char*, char*, unsigned char*, unsigned short int, int );
int IDMan_IPgIdxSParamChk ( int , unsigned char* , unsigned short int , int  );
int IDMan_IPgetPrivateKey( CK_SESSION_HANDLE , char*, long, CK_OBJECT_HANDLE* );
int IDMan_IPgetHash( CK_SESSION_HANDLE , int, unsigned char*, unsigned short int, CK_BYTE_PTR*, CK_ULONG* );
int IDMan_IPCrSignature ( CK_SESSION_HANDLE, CK_OBJECT_HANDLE, int, CK_BYTE_PTR, CK_ULONG ,int, unsigned char*, unsigned short int* );


/**
* 電子署名生成の内部関数でパラメータチェック関数
* @author University of Tsukuba
* @param ptrSubjectDN  取得する公開鍵証明書のsubjectDN
* @param ptrIssuerDN  取得する公開鍵証明書のissuerDN
* @param strData 署名対象データ
* @param iDataLen 署名対象データのレングス
* @param iAlgorithm ハッシュアルゴリズム
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgSParamChk ( char* ptrSubjectDN,
						 char* ptrIssuerDN,
						 unsigned char* strData,
						 unsigned short int iDataLen,
						 int iAlgorithm )
{
	char				buff[1024];

	memset(buff,0x00, sizeof(buff));

	/**引数SubjectDNのチェックを行う。 */
	/**引数のSubjectDNが0x00ポインタの場合、 */
	if(ptrSubjectDN == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数のSubjectDNが0x00値の場合、 */
	if(strcmp(ptrSubjectDN , buff) == 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数のSubjectDNが半角スペースの場合、 */
	if(strcmp(ptrSubjectDN , " ") == 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数のSubjectDNが全角スペースの場合、 */
	if(strcmp(ptrSubjectDN , "　") == 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数IssuerDNのチェックを行う。 */
	/**引数のIssuerDNが0x00ポインタの場合、 */
	if(ptrIssuerDN == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数のIssuerDNが0x00値の場合、 */
	if(strcmp(ptrIssuerDN , buff) == 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数のIssuerDNが半角スペースの場合、 */
	if(strcmp(ptrIssuerDN , " ") == 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数のIssuerDNが全角スペースの場合、 */
	if(strcmp(ptrIssuerDN , "　") == 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数署名対象データのチェックを行う。 */
	/**引数の署名対象データが0x00ポインタの場合、 */
	if(strData == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数署名対象データレングスのチェックを行う。 */
	/**引数の署名対象データレングスが0以下の場合、 */
	if(iDataLen <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数ハッシュアルゴリズムのチェックを行う。 */
	/**引数のハッシュアルゴリズムが0以下の場合、 */
	if(iAlgorithm <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**正常リターンする。 */
	return RET_IPOK;

}

/**
* 電子署名生成Index指定版の内部関数でパラメータチェック関数
* @author University of Tsukuba
* @param iPkcxIndex  取得する公開鍵証明書のiPkcxIndex
* @param strData 署名対象データ
* @param iDataLen 署名対象データのレングス
* @param iAlgorithm ハッシュアルゴリズム
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgIdxSParamChk ( int iPkcxIndex,
						 unsigned char* strData,
						 unsigned short int iDataLen,
						 int iAlgorithm )
{
	char				buff[1024];

	memset(buff,0x00, sizeof(buff));

	/**パラメータチェックを行う。 */
	/**引数iPkcxIndexのチェックを行う。 */
	/**引数のiPkcxIndexが0以下の場合、 */
	if(iPkcxIndex <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数署名対象データのチェックを行う。 */
	/**引数の署名対象データが0x00ポインタの場合、 */
	if(strData == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数署名対象データレングスのチェックを行う。 */
	/**引数の署名対象データレングスが0以下の場合、 */
	if(iDataLen <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数ハッシュアルゴリズムのチェックを行う。 */
	/**引数のハッシュアルゴリズムが0以下の場合、 */
	if(iAlgorithm <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**正常リターンする。 */
	return RET_IPOK;

}

/**
* 電子署名生成関数の内部関数で秘密鍵オブジェクトハンドル取得関数
* （公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドルを取得する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param KeyId 鍵ペアID確保領域ポインタ
* @param lKeyId 鍵ペアIDレングス
* @param pKeyObjectHandle 秘密鍵オブジェクトハンドル
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgetPrivateKey(	CK_SESSION_HANDLE Sessinhandle,
							char* KeyId,
							long lKeyId,
							CK_OBJECT_HANDLE* pKeyObjectHandle )
{

	/*int						iret;*/								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	CK_ATTRIBUTE		Obj2Attribute[4];					//オブジェクトハンドル
	CK_OBJECT_HANDLE		Object2Handle[OBJECT_HANDLE_MAX];	
	CK_ULONG				Object2HandleCnt;
	long					class;
	unsigned char			token;
	char					Label[256];

	iReturn = RET_IPOK;
	class=0;
	token=0;

	/**秘密鍵の検索操作初期化を行う。 */
	/**−CKA_CLASSにPrivateKeyを指定する。 */
	/**−CKA_TOKENにTRUEを指定する。 */
	/**−CKA_LABELにSecretKeyを指定する。 */
	/**−CKA_IDに証明書検索で取得した鍵ペアIDを指定する。（秘密鍵IDと同じ） */
	Obj2Attribute[0].type = CKA_CLASS;
	class = CKO_PRIVATE_KEY;
	Obj2Attribute[0].pValue = &class;
	Obj2Attribute[0].ulValueLen = sizeof(class);

	Obj2Attribute[1].type = CKA_TOKEN;
	token = CK_TRUE;
	Obj2Attribute[1].pValue = &token;
	Obj2Attribute[1].ulValueLen = sizeof(token);

	Obj2Attribute[2].type = CKA_LABEL;
	memset(Label,0x00, sizeof(Label));
	strcpy(Label,CK_LABEL_PRIVATE_KEY);
	Obj2Attribute[2].pValue = Label;
	Obj2Attribute[2].ulValueLen = strlen(Label);

	Obj2Attribute[3].type = CKA_ID;
	Obj2Attribute[3].pValue = KeyId;
	Obj2Attribute[3].ulValueLen = lKeyId;

	rv = C_FindObjectsInit(Sessinhandle,(CK_ATTRIBUTE_PTR)&Obj2Attribute, 4);
	/**秘密鍵の検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**秘密鍵のオブジェクトハンドル取得を行う。 */
	rv = C_FindObjects(Sessinhandle,(CK_OBJECT_HANDLE_PTR)&Object2Handle,OBJECT_HANDLE_MAX,&Object2HandleCnt);
	/**秘密鍵のオブジェクトハンドル取得エラーの場合、 */
	if (rv != CKR_OK)
	{
		/**−返り値に異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}
	/**秘密鍵のオブジェクトハンドル件数が1件でないエラーの場合、 */
	if (Object2HandleCnt != 1)
	{
		/**−返り値に異常リターンする。 */
		iReturn = RET_IPNG;
		C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}
	memcpy(pKeyObjectHandle,&Object2Handle,sizeof(CK_OBJECT_HANDLE));

	/**秘密鍵の検索操作終了を行う。 */
	rv = C_FindObjectsFinal( Sessinhandle );
	/**秘密鍵の検索操作終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**返り値をリターンする。 */
	return iReturn;

}


/**
* 電子署名生成関数の内部関数で署名対象データハッシュ値生成関数
* （署名対象データのハッシュ値を求める）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param Algorithm ハッシュアルゴリズム
* @param Data 署名対象データ
* @param DataLen 署名対象データのレングス
* @param pHash ハッシュ値領域ポインタ
* @param lHashLen ハッシュ値レングス
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgetHash( CK_SESSION_HANDLE Sessinhandle,
					 int Algorithm ,
					 unsigned char* Data,
					 unsigned short int DataLen,
					 CK_BYTE_PTR* pHash,
					 CK_ULONG* lHashLen)
{
	/*int						iret;*/								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	CK_MECHANISM		strMechanismDigest;					//メカニズム情報（Digest）

	iReturn = RET_IPOK;

	/**ダイジェスト作成開始を行う。 */
	strMechanismDigest.mechanism = Algorithm;
	rv = C_DigestInit( Sessinhandle, &strMechanismDigest);
	/**ダイジェスト作成開始エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**ダイジェスト作成を行う。 */
	rv = C_DigestUpdate( Sessinhandle, (CK_BYTE_PTR)Data,(CK_ULONG)DataLen);
	/**ダイジェスト作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**ダイジェストレングス取得を行う。 */
	rv = C_DigestFinal( Sessinhandle, 0x00,lHashLen);
	/**−ダイジェストレングス取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**ハッシュ値格納領域を確保する。 */
	(*pHash) = IDMan_StMalloc((*lHashLen));
	/**ハッシュ値格納領域確保エラーの場合、 */
	if ((*pHash) == 0x00) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}
	memset((*pHash),0x00,(*lHashLen));

	/**ダイジェスト作成終了を行う。 */
	rv = C_DigestFinal( Sessinhandle, (*pHash),lHashLen);
	/**ダイジェスト作成終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		IDMan_StFree ((*pHash));
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**返り値をリターンする。 */
	return iReturn;

}

/**
* 電子署名生成関数の内部関数で署名生成関数
* （秘密鍵を使用して署名を生成する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param KeyObjectHandle  秘密鍵オブジェクトハンドル
* @param Algorithm ハッシュアルゴリズム
* @param pHash ハッシュ値領域ポインタ
* @param lHashLen ハッシュ値レングス
* @param iPaddingFlg パディングフラグ 0:パディング実施 1:パディング未実施
* @param Signature 署名データ
* @param pSignatureLen 署名データのレングス
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPCrSignature (	CK_SESSION_HANDLE Sessinhandle,
							CK_OBJECT_HANDLE KeyObjectHandle,
							int Algorithm ,
							CK_BYTE_PTR pHash,
							CK_ULONG lHashLen,
							int iPaddingFlg,
							unsigned char* Signature,
							unsigned short int* pSignatureLen )
{

	int						iret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	CK_MECHANISM		strMechanismSign;					//メカニズム情報（Sign）
	/*CK_BYTE_PTR				pDigest;*/							//ダイジェストデータポインタ
	/*CK_ULONG				lDigestLen;*/							//ダイジェストデータ長
	CK_BYTE_PTR				pSignature;							//署名データポインタ
	unsigned char			SignatureData[2048];				//パディング後署名対象データ
	long					SignatureDataLen;					//パディング後署名対象データレングス

	iReturn = RET_IPOK;

	/**パディングフラグの確認を行う。 */
	if(iPaddingFlg==0)
	{
		/**−パディング実施の場合、署名対象データ（ハッシュ値）にパディング追加を行う。 */
		memset(SignatureData,0x00, sizeof(SignatureData));
		SignatureDataLen = CERSIG_PADDING_SIZE;
		iret = IDMan_CmPadding( Algorithm ,SignatureDataLen, pHash, lHashLen ,SignatureData );
		/**−署名対象データ（ハッシュ値）パディング追加エラーの場合、 */
		if (iret != RET_IPOK) 
		{
			/**−−異常リターンする。 */
			iReturn = RET_IPNG;
			return iReturn;
		}
	}
	else
	{
		/**−パディング未実施の場合、署名対象データを設定する。 */
		memcpy(SignatureData ,pHash,lHashLen);
		SignatureDataLen=lHashLen;
	}

	strMechanismSign.mechanism = Algorithm;
	/**署名処理の初期化を行う。 */
	rv = C_SignInit( Sessinhandle, &strMechanismSign,KeyObjectHandle);
	/**署名処理の初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名レングス取得を行う。 */
	rv = C_Sign( Sessinhandle, SignatureData,SignatureDataLen,0x00,(CK_ULONG_PTR)pSignatureLen);
	/**署名レングス取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名データ領域を確保する。 */
	pSignature = IDMan_StMalloc((*pSignatureLen));
	/**署名データ領域確保エラーの場合、 */
	if (pSignature == 0x00) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}
	memset(pSignature,0x00,(*pSignatureLen));

	/**署名処理を行う。 */
	rv = C_Sign( Sessinhandle, SignatureData,SignatureDataLen,pSignature,(CK_ULONG_PTR)pSignatureLen);
	/**署名処理エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (pSignature);
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}
	memcpy(Signature,pSignature,(*pSignatureLen));
	IDMan_StFree (pSignature);

	/**返り値をリターンする。 */
	return iReturn;

}

/**
* 電子署名生成関数
* （署名対象データのハッシュ値を求め、ICカード内の秘密鍵で署名をする。署名に利用する鍵ペアは、公開鍵証明書のsubjectDNとissuerDNで指定する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param subjectDN 署名に使用する鍵ペアに対する公開鍵証明書のsubjectDN
* @param issuerDN 署名に使用する鍵ペアに対する公開鍵証明書のissuerDN
* @param data 署名対象データ
* @param dataLen 署名対象データのレングス
* @param signature 署名データ
* @param signatureLen 署名データのレングス
* @param algorithm ハッシュアルゴリズム
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたsubjectDNとissuerDNが同様の証明書はありません）
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
 int  IDMan_generateSignature (	 unsigned long int SessionHandle,
								 char* subjectDN,
								 char* issuerDN,
								 unsigned char* data,
								 unsigned short int dataLen,
								 unsigned char* signature,
								 unsigned short int* signatureLen,
								 int algorithm )
{
	int						iret;								//返り値
	int						iReturn;							//返り値
	/*CK_RV					rv;*/									//返り値
	/*CK_MECHANISM		strMechanismSign;*/					//メカニズム情報（Sign）
	void					*ptrCert;							//証明書確保領域ポインタ
	long					lCert;								//証明書レングス
	void					*KeyId;								//鍵ペアID確保領域ポインタ
	long					lKeyId;								//鍵ペアIDレングス
	CK_OBJECT_HANDLE		KeyObjectHandle;	
	CK_BYTE_PTR				pDigest;							//ダイジェストデータポインタ
	CK_ULONG				lDigestLen;							//ダイジェストデータ長
	/*CK_BYTE_PTR				pSignature;*/							//署名データポインタ

	DEBUG_OutPut("IDMan_generateSignature start\n");

	iReturn = RET_IPOK;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPgSParamChk( subjectDN, issuerDN, data, dataLen, algorithm );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignature 入力パラメータのチェック エラー IDMan_IPgSParamChk\n");
		return iReturn;
	}

	/**SubjectDN、IssuerDNに対応する公開鍵証明書PKC(ｘ)の鍵ペアIDを取得する。 */
	iret = IDMan_IPgetCertPKCS( SessionHandle, subjectDN, issuerDN,&ptrCert, &lCert, &KeyId, &lKeyId,0);
	/**SubjectDN、IssuerDNに対応する公開鍵証明書PKC(ｘ)の鍵ペアID取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignature SubjectDN、IssuerDNに対応する公開鍵証明書PKC(ｘ)の鍵ペアID取得 エラー IDMan_IPgetCertPKCS\n");
		return iReturn;
	}

	/**公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドルを取得する。 */
	iret = IDMan_IPgetPrivateKey( SessionHandle, (char *)KeyId, lKeyId, &KeyObjectHandle );
	/**−公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドル取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (KeyId);
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignature 公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドル取得 エラー IDMan_IPgetPrivateKey\n");
		return iReturn;
	}
	IDMan_StFree (KeyId);

	/**署名対象データのハッシュ値を求める。 */
	iret = IDMan_IPgetHash( SessionHandle, algorithm, data, dataLen, &pDigest, &lDigestLen);
	/**署名対象データのハッシュ値取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignature 署名対象データのハッシュ値取得 エラー IDMan_IPgetHash\n");
		return iReturn;
	}

	/**署名生成を行う。 */
	iret = IDMan_IPCrSignature( SessionHandle,KeyObjectHandle, algorithm, pDigest, lDigestLen, 0, signature, signatureLen);
	/**−署名生成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (pDigest);
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignature 署名生成 エラー IDMan_IPCrSignature\n");
		return iReturn;
	}
	IDMan_StFree (pDigest);

	DEBUG_OutPut("IDMan_generateSignature end\n");

	/**返り値をリターンする。 */
	return iReturn;

}
/**
* 電子署名生成関数Index指定版
* （署名対象データのハッシュ値を求め、ICカード内の秘密鍵で署名をする。署名に利用する鍵ペアは、PkcxIndexで公開鍵証明書を指定する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param PkcxIndex 署名に使用する鍵ペアに対する公開鍵証明書のインデックス（1,2,3）
* @param data 署名対象データ
* @param dataLen 署名対象データのレングス
* @param signature 署名データ
* @param signatureLen 署名データのレングス
* @param algorithm ハッシュアルゴリズム
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたPkcxIndexに対応する証明書はありません）
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
 int  IDMan_generateSignatureByIndex (	 unsigned long int SessionHandle,
								 int PkcxIndex,
								 unsigned char* data,
								 unsigned short int dataLen,
								 unsigned char* signature,
								 unsigned short int* signatureLen,
								 int algorithm )
{
	int						iret;								//返り値
	int						iReturn;							//返り値
	/*CK_RV					rv;*/									//返り値
	/*CK_MECHANISM		strMechanismSign;*/					//メカニズム情報（Sign）
	void					*KeyId;								//鍵ペアID確保領域ポインタ
	long					lKeyId;								//鍵ペアIDレングス
	void					*Cert;								//証明書確保領域ポインタ
	long					lCert;								//証明書レングス
	CK_OBJECT_HANDLE		KeyObjectHandle;	
	CK_BYTE_PTR				pDigest;							//ダイジェストデータポインタ
	CK_ULONG				lDigestLen;							//ダイジェストデータ長
	/*CK_BYTE_PTR				pSignature;*/							//署名データポインタ

	DEBUG_OutPut("IDMan_generateSignatureByIndex start\n");

	iReturn = RET_IPOK;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPgIdxSParamChk(  PkcxIndex, data, dataLen, algorithm );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignatureByIndex 入力パラメータのチェック エラー IDMan_IPgIdxSParamChk\n");
		return iReturn;
	}

	/**PkcxIndexに対応する公開鍵証明書PKC(ｘ)の鍵ペアIDを取得する。 */
	iret = IDMan_IPgetCertIdxPKCS( SessionHandle, PkcxIndex, &Cert,&lCert,&KeyId, &lKeyId,0);
	/**PkcxIndexに対応する公開鍵証明書PKC(ｘ)の鍵ペアID取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignatureByIndex PkcxIndexに対応する公開鍵証明書PKC(ｘ)の鍵ペアID取得 エラー IDMan_IPgetCertIdxPKCS\n");
		return iReturn;
	}

	/**公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドルを取得する。 */
	iret = IDMan_IPgetPrivateKey( SessionHandle, KeyId, lKeyId, &KeyObjectHandle );
	/**−公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドル取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (KeyId);
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignatureByIndex 公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドル取得 エラー IDMan_IPgetPrivateKey\n");
		return iReturn;
	}
	IDMan_StFree (KeyId);

	/**署名対象データのハッシュ値を求める。 */
	iret = IDMan_IPgetHash( SessionHandle, algorithm, data, dataLen, &pDigest, &lDigestLen);
	/**署名対象データのハッシュ値取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignatureByIndex 署名対象データのハッシュ値取得 エラー IDMan_IPgetHash\n");
		return iReturn;
	}

	/**署名生成を行う。 */
	iret = IDMan_IPCrSignature( SessionHandle,KeyObjectHandle, algorithm, pDigest, lDigestLen, 0, signature, signatureLen);
	/**−署名生成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (pDigest);
		iReturn = iret;
		DEBUG_OutPut("IDMan_generateSignatureByIndex 署名生成 エラー IDMan_IPCrSignature\n");
		return iReturn;
	}
	IDMan_StFree (pDigest);

	DEBUG_OutPut("IDMan_generateSignatureByIndex end\n");

	/**返り値をリターンする。 */
	return iReturn;

}


/**
* 電子署名生成（未ハッシュ版）関数Index指定版
* （署名対象データをICカード内の秘密鍵で署名をする。署名に利用する鍵ペアは、PkcxIndexで公開鍵証明書を指定する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param PkcxIndex 署名に使用する鍵ペアに対する公開鍵証明書のインデックス（1,2,3）
* @param data 署名対象データ
* @param dataLen 署名対象データのレングス
* @param signature 署名データ
* @param signatureLen 署名データのレングス
* @param algorithm アルゴリズム
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたPkcxIndexに対応する証明書はありません）
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
 int  IDMan_EncryptByIndex (	 unsigned long int SessionHandle,
								 int PkcxIndex,
								 unsigned char* data,
								 unsigned short int dataLen,
								 unsigned char* signature,
								 unsigned short int* signatureLen,
								 int algorithm )
{
	int						iret;								//返り値
	int						iReturn;							//返り値
	/*CK_RV					rv;*/									//返り値
	/*CK_MECHANISM		strMechanismSign;*/					//メカニズム情報（Sign）
	void					*KeyId;								//鍵ペアID確保領域ポインタ
	long					lKeyId;								//鍵ペアIDレングス
	void					*Cert;								//証明書確保領域ポインタ
	long					lCert;								//証明書レングス
	CK_OBJECT_HANDLE		KeyObjectHandle;	
	/*CK_BYTE_PTR				pSignature;*/							//署名データポインタ
	unsigned char			SignatureData[2048];				//パディング後署名対象データ
	long					SignatureDataLen;					//パディング後署名対象データレングス

	DEBUG_OutPut("IDMan_EncryptByIndex start\n");

	iReturn = RET_IPOK;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPgIdxSParamChk( PkcxIndex, data, dataLen, algorithm );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_EncryptByIndex 入力パラメータのチェック エラー IDMan_IPgIdxSParamChk\n");
		return iReturn;
	}

	/**PkcxIndexに対応する公開鍵証明書PKC(ｘ)の鍵ペアIDを取得する。 */
	iret = IDMan_IPgetCertIdxPKCS( SessionHandle, PkcxIndex,&Cert,&lCert, &KeyId, &lKeyId,0);
	/**PkcxIndexに対応する公開鍵証明書PKC(ｘ)の鍵ペアID取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_EncryptByIndex PkcxIndexに対応する公開鍵証明書PKC(ｘ)の鍵ペアID取得 エラー IDMan_IPgetCertIdxPKCS\n");
		return iReturn;
	}

	/**公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドルを取得する。 */
	iret = IDMan_IPgetPrivateKey( SessionHandle, KeyId, lKeyId, &KeyObjectHandle );
	/**−公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドル取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (KeyId);
		iReturn = iret;
		DEBUG_OutPut("IDMan_EncryptByIndex 公開鍵証明書PKC(ｘ)に対応する秘密鍵オブジェクトハンドル取得 エラー IDMan_IPgetPrivateKey\n");
		return iReturn;
	}
	IDMan_StFree (KeyId);

	/**−パディング実施の場合、署名対象データ（ハッシュ値）にパディング追加を行う。 */
	memset(SignatureData,0x00, sizeof(SignatureData));
	SignatureDataLen = CERSIG_PADDING_SIZE;
	iret = IDMan_CmPadding2( algorithm ,SignatureDataLen, data, dataLen ,SignatureData );
	/**−署名対象データ（ハッシュ値）パディング追加エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−−異常リターンする。 */
		iReturn = RET_IPNG;
		DEBUG_OutPut("IDMan_EncryptByIndex 署名生成 エラー IDMan_CmPadding2\n");
		return iReturn;
	}
	/**署名生成を行う。 */
	iret = IDMan_IPCrSignature( SessionHandle,KeyObjectHandle, algorithm, SignatureData, SignatureDataLen, 1, signature, signatureLen);
	/**−署名生成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_EncryptByIndex 署名生成 エラー IDMan_IPCrSignature\n");
		return iReturn;
	}

	DEBUG_OutPut("IDMan_EncryptByIndex end\n");

	/**返り値をリターンする。 */
	return iReturn;

}

