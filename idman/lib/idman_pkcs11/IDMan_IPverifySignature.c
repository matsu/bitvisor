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
 * 電子署名検証プログラム
 * \file IDMan_IPverifySignature.c
 */

/* 作成ライブラリ関数名 */
#include "IDMan_IPCommon.h"

int IDMan_IPvSParamChk ( char*, char*, unsigned char*, unsigned short int, unsigned char*, unsigned short int, unsigned char*, unsigned short int, int );
int IDMan_IPvSIdxParamChk ( int , unsigned char*, unsigned short int, unsigned char*, unsigned short int, unsigned char*, unsigned short int, int );
int IDMan_IPMkPublicKeyObj( CK_SESSION_HANDLE, void*, long, CK_OBJECT_HANDLE* );
int IDMan_IPCheckSignature( CK_SESSION_HANDLE, int, unsigned char*, unsigned short int, unsigned char*, unsigned short int, unsigned char*, unsigned short int, CK_OBJECT_HANDLE );


/**
* 電子署名検証の内部関数でパラメータチェック関数
* @author University of Tsukuba
* @param ptrSubjectDN  取得する公開鍵証明書のsubjectDN
* @param ptrIssuerDN  取得する公開鍵証明書のissuerDN
* @param strData 署名対象データ
* @param iDataLen 署名対象データのレングス
* @param ptrSignature 署名データ
* @param iSignatureLen 署名データのレングス
* @param ptrPKC DER形式のX.509公開鍵証明書のバイナリデータ
* @param iPKCLen DER形式のX.509公開鍵証明書のバイナリデータのレングス
* @param iAlgorithm ハッシュアルゴリズム
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPvSParamChk ( char* ptrSubjectDN,
						 char* ptrIssuerDN,
						 unsigned char* strData,
						 unsigned short int iDataLen,
						 unsigned char* ptrSignature,
						 unsigned short int iSignatureLen,
						 unsigned char* ptrPKC,
						 unsigned short int iPKCLen,
						 int iAlgorithm )
{
	char				buff[1024];


	memset(buff,0x00, sizeof(buff));

	/**パラメータチェックを行う。 */
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

	/**引数署名データのチェックを行う。 */
	/**引数の署名データが0x00ポインタの場合、 */
	if(ptrSignature == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数署名データレングスのチェックを行う。 */
	/**引数の署名データレングスが0以下の場合、 */
	if(iSignatureLen <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数DER形式のX.509公開鍵証明書のバイナリデータのチェックを行う。 */
	/**引数のDER形式のX.509公開鍵証明書のバイナリデータが0x00ポインタの場合、 */
	if(ptrPKC == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数DER形式のX.509公開鍵証明書のバイナリデータレングスのチェックを行う。 */
	/**−引数のDER形式のX.509公開鍵証明書のバイナリデータレングスが0以下の場合、 */
	if(iPKCLen <= 0)
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
* 電子署名検証Index指定版の内部関数でパラメータチェック関数
* @author University of Tsukuba
* @param iPkcxIndex  取得する公開鍵証明書のiPkcxIndex
* @param strData 署名対象データ
* @param iDataLen 署名対象データのレングス
* @param ptrSignature 署名データ
* @param iSignatureLen 署名データのレングス
* @param ptrPKC DER形式のX.509公開鍵証明書のバイナリデータ
* @param iPKCLen DER形式のX.509公開鍵証明書のバイナリデータのレングス
* @param iAlgorithm ハッシュアルゴリズム
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPvSIdxParamChk ( int iPkczIndex,
						 unsigned char* strData,
						 unsigned short int iDataLen,
						 unsigned char* ptrSignature,
						 unsigned short int iSignatureLen,
						 unsigned char* ptrPKC,
						 unsigned short int iPKCLen,
						 int iAlgorithm )
{
	char				buff[1024];

	memset(buff,0x00, sizeof(buff));

	/**パラメータチェックを行う。 */
	/**引数iPkcxIndexのチェックを行う。 */
	/**引数のiPkcxIndexが0以下の場合、 */
	if(iPkczIndex <= 0)
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

	/**引数署名データのチェックを行う。 */
	/**引数の署名データが0x00ポインタの場合、 */
	if(ptrSignature == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数署名データレングスのチェックを行う。 */
	/**引数の署名データレングスが0以下の場合、 */
	if(iSignatureLen <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数DER形式のX.509公開鍵証明書のバイナリデータのチェックを行う。 */
	/**引数のDER形式のX.509公開鍵証明書のバイナリデータが0x00ポインタの場合、 */
	if(ptrPKC == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	/**引数DER形式のX.509公開鍵証明書のバイナリデータレングスのチェックを行う。 */
	/**−引数のDER形式のX.509公開鍵証明書のバイナリデータレングスが0以下の場合、 */
	if(iPKCLen <= 0)
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
* 電子署名検証関数の内部関数で公開鍵証明書PKC(z)の公開鍵オブジェクト作成関数
* （公開鍵証明書PKC(z)の公開鍵オブジェクトを作成する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param ptrCert 証明書確保領域ポインタ
* @param lCert 証明書レングス
* @param ObjectHandle オブジェクトハンドル
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPMkPublicKeyObj(	 CK_SESSION_HANDLE Sessinhandle,
							 void* ptrCert,
							 long lCert,
							 CK_OBJECT_HANDLE* ObjectHandle)
{
	long					lret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	unsigned char			PublicKey[1024];					//公開鍵データ
	unsigned long				lPublicKeyLen;						//公開鍵データレングス
	CK_ATTRIBUTE		ObjAttribute[3];					//
	long					class;								//
	char					Label[256];

	class=0;
	iReturn = RET_IPOK;

	/**公開鍵証明書PKC(Z)より公開鍵を取得する。 */
	memset(PublicKey,0x00,sizeof(PublicKey));
	lret = IDMan_CmGetPublicKey ( ptrCert, lCert, PublicKey, &lPublicKeyLen);
	/**公開鍵証明書PKC(Z)の公開鍵取得エラーの場合、 */
	if (lret != RET_IPOK) 
	{
		iReturn = RET_IPNG;
		/**−異常リターンする。 */
		return iReturn;
	}

	/**公開鍵オブジェクトの作成を行う。 */
	/**−CKA_CLASSにCKO_PUBLIC_KEYを指定する。 */
	/**−CKA_LABELにTrustCertificateを指定する。 */
	/**−CKA_VALUEに取得した公開鍵を指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_PUBLIC_KEY;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	memset(Label,0x00, sizeof(Label));
	ObjAttribute[1].type = CKA_LABEL;
	strcpy(Label,CK_LABEL_TRUST_CERT);
	ObjAttribute[1].pValue = &Label;
	ObjAttribute[1].ulValueLen = strlen(Label);

	ObjAttribute[2].type = CKA_VALUE;
	ObjAttribute[2].pValue = &PublicKey;
	ObjAttribute[2].ulValueLen = lPublicKeyLen;

	rv = C_CreateObject(Sessinhandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 3,(CK_OBJECT_HANDLE_PTR)ObjectHandle);
	/**公開鍵オブジェクトの作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**返り値をリターンする。 */
	return iReturn;
}


/**
* 電子署名検証関数の内部関数で署名データ検証関数
* （パラメータの署名データを検証する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param Algorithm ハッシュアルゴリズム
* @param data 署名対象データ
* @param dataLen 署名対象データのレングス
* @param signature 署名データ
* @param signatureLen 署名データのレングス
* @param pPKC DER形式のX.509公開鍵証明書のバイナリデータ
* @param PKCLen DER形式のX.509公開鍵証明書のバイナリデータのレングス
* @param pKeyObjectHandle 公開鍵オブジェクトハンドル
* @return int 0:正常 
*            -1:不正ICカードが挿入されている場合
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -4:PIN番号が間違っている場合
*            -5:PINがロックされている場合
*            -6:異常
* @since 2008.02
* @version 1.0
*/
int IDMan_IPCheckSignature(	CK_SESSION_HANDLE Sessinhandle,
							int Algorithm,
							unsigned char* data,
							unsigned short int dataLen,
							unsigned char* signature,
							unsigned short int signatureLen,
							unsigned char* pPKC,
							unsigned short int PKCLen,
							CK_OBJECT_HANDLE pKeyObjectHandle )
{
	int						iReturn;							//返り値
	int						iret;								//返り値
	CK_RV					rv;									//返り値
	CK_MECHANISM		strMechanismDigest;					//メカニズム情報（Digest）
	CK_BYTE_PTR				pHash;								//ハッシュデータ
	CK_ULONG				lHashLen;							//ハッシュデータ長
	CK_MECHANISM		strMechanismVerify;					//メカニズム情報（Verify）
	unsigned char			SignatureData[2048];				//パディング後署名対象データ
	long					SignatureDataLen;					//パディング後署名対象データレングス

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
	rv = C_DigestUpdate( Sessinhandle, (CK_BYTE_PTR)data,(CK_ULONG)dataLen);
	/**ダイジェスト作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**ダイジェストレングス取得を行う。 */
	rv = C_DigestFinal( Sessinhandle, 0x00,&lHashLen);
	/**ダイジェストレングス取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**ハッシュ値格納領域を確保する。 */
	pHash = IDMan_StMalloc(lHashLen);
	/**ハッシュ値格納領域確保エラーの場合、 */
	if (pHash == 0x00) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}
	memset(pHash,0x00,lHashLen);

	/**ダイジェスト作成終了を行う。 */
	rv = C_DigestFinal( Sessinhandle, pHash,&lHashLen);
	/**ダイジェスト作成終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		IDMan_StFree (pHash);
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名対象データ（ハッシュ値）にパディング追加を行う。 */
	memset(SignatureData,0x00, sizeof(SignatureData));
	SignatureDataLen = CERSIG_PADDING_SIZE;
	iret = IDMan_CmPadding( Algorithm ,SignatureDataLen, pHash, lHashLen ,SignatureData );
	/**署名対象データ（ハッシュ値）パディング追加エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		IDMan_StFree (pHash);
		return iReturn;
	}
	IDMan_StFree (pHash);

	/**署名検証の開始を行う。 */
	strMechanismVerify.mechanism = Algorithm;
	rv = C_VerifyInit( Sessinhandle, &strMechanismVerify,pKeyObjectHandle);
	/**−署名検証の開始エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名を検証する。 */
	rv =  C_Verify( Sessinhandle, SignatureData, SignatureDataLen, signature, signatureLen);
	if (rv != CKR_OK) 
	{
		/**−署名データ不正か確認する。 */
		/**−署名データ不正の場合、 */
		if(rv == CKR_SIGNATURE_INVALID)
		{
			/**−−署名の検証に失敗（１）をリターンする。 */
			iReturn = RET_IPOK_NO_VERIFY_SIGNATURE;
		}
		/**−署名検証エラーの場合、 */
		else
		{
			/**−−返り値に異常リターンする。 */
			iReturn = IDMan_IPRetJudgment(rv);
		}
		return iReturn;
	}

	/**返り値をリターンする。 */
	return iReturn;
}

/**
* 電子署名検証関数
* （署名対象データのハッシュ値を求め、ICカード内の秘密鍵で署名をする。署名に利用する鍵ペアは、公開鍵証明書のsubjectDNとissuerDNで指定する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param subjectDN 署名に使用する鍵ペアに対する公開鍵証明書のsubjectDN
* @param issuerDN 署名に使用する鍵ペアに対する公開鍵証明書のissuerDN
* @param data 署名対象データ
* @param dataLen 署名対象データのレングス
* @param signature 署名データ
* @param signatureLen 署名データのレングス
* @param PKC DER形式のX.509公開鍵証明書のバイナリデータ
* @param PKCLen DER形式のX.509公開鍵証明書のバイナリデータのレングス
* @param algorithm ハッシュアルゴリズム
* @return int 0:正常終了（署名の検証に成功）
*             1:正常終了（署名の検証に失敗）
*             2:公開鍵証明書の検証に失敗の場合
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたsubjectDNとissuerDNが同様の証明書はありません）
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_verifySignature(	 unsigned long int SessionHandle,
							 char* subjectDN,
							 char* issuerDN,
							 unsigned char* data,
							 unsigned short int dataLen,
							 unsigned char* signature,
							 unsigned short int signatureLen,
							 unsigned char* PKC,
							 unsigned short int PKCLen,
							 int algorithm )
{

	int						iret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	void*					ptrCert;							//証明書確保領域ポインタ
	long					lCert;								//証明書レングス
	void*					ptrKeyId;							//鍵ペアID確保領域ポインタ
	long					lKeyId;								//鍵ペアIDレングス
	CK_OBJECT_HANDLE		PublicKeyObjHandle;					//公開鍵オブジェクトハンドル

	DEBUG_OutPut("IDMan_verifySignature start\n");

	//初期処理
	iReturn = RET_IPOK;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPvSParamChk( subjectDN, issuerDN, data, dataLen, signature, signatureLen, PKC, PKCLen, algorithm );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignature 入力パラメータのチェック エラー IDMan_IPvSParamChk \n");
		return iReturn;
	}

	/**SubjectDN、IssuerDNに対応する公開鍵証明書PKC(z)を取得する。 */
	iret = IDMan_IPgetCertPKCS( SessionHandle, subjectDN, issuerDN,&ptrCert,&lCert, &ptrKeyId, &lKeyId,1);
	/**SubjectDN、IssuerDNに対応する公開鍵証明書PKC(z)取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignature SubjectDN、IssuerDNに対応する公開鍵証明書PKC(z)取得 エラー IDMan_IPgetCertPKCS \n");
		return iReturn;
	}

	/**PKC(Z)の公開鍵オブジェクトを作成する。 */
	iret = IDMan_IPMkPublicKeyObj( SessionHandle, ptrCert, lCert, &PublicKeyObjHandle );
	/**−PKC(Z)の公開鍵オブジェクト作成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (ptrCert);
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignature PKC(Z)の公開鍵オブジェクト作成 エラー IDMan_IPMkPublicKeyObj \n");
		return iReturn;
	}
	IDMan_StFree (ptrCert);

	/**パラメータの公開鍵証明書（PKC）の正当性チェックを行う。 */
	iret = IDMan_IPCheckPKC( SessionHandle, PKC, PKCLen,CERSIG_ALGORITHM,&PublicKeyObjHandle);
	/**パラメータの公開鍵証明書（PKC）の正当性チェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		if(iret == RET_IPOK_NO_VERIFY)
		{
			iReturn = RET_IPOK_NO_VERIFY_CERT;
		}
		else
		{
			iReturn = iret;
		}
		C_DestroyObject( SessionHandle, PublicKeyObjHandle);
		DEBUG_OutPut("IDMan_verifySignature パラメータの公開鍵証明書（PKC）の正当性チェック エラー IDMan_IPCheckPKC \n");
		return iReturn;
	}

	/**公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, PublicKeyObjHandle);
	/**公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_verifySignature 公開鍵オブジェクト破棄 エラー C_DestroyObject \n");
		return iReturn;
	}

	/**PKCの公開鍵オブジェクトを作成する。 */
	iret = IDMan_IPMkPublicKeyObj( SessionHandle, PKC, PKCLen, &PublicKeyObjHandle );
	/**PKCの公開鍵オブジェクト作成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignature PKCの公開鍵オブジェクト作成 エラー IDMan_IPMkPublicKeyObj \n");
		return iReturn;
	}

	/**署名データの検証する。 */
	iret = IDMan_IPCheckSignature( SessionHandle,algorithm, data, dataLen,signature,signatureLen,PKC,PKCLen,PublicKeyObjHandle);
	/**署名データの検証エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		C_DestroyObject( SessionHandle, PublicKeyObjHandle);
		DEBUG_OutPut("IDMan_verifySignature 署名データの検証 エラー IDMan_IPCheckSignature \n");
		return iReturn;
	}

	/**公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, PublicKeyObjHandle);
	/**公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_verifySignature 公開鍵オブジェクト破棄2 エラー C_DestroyObject \n");
		return iReturn;
	}

	DEBUG_OutPut("IDMan_verifySignature end \n");

	/**返り値をリターンする。 */
	return iReturn;
}

/**
* 電子署名検証関数
* （署名対象データのハッシュ値を求め、ICカード内の秘密鍵で署名をする。署名に利用する鍵ペアは、PkcxIndexで公開鍵証明書を指定する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param PkczIndex 署名に使用する鍵ペアに対する公開鍵証明書のインデックス（1,2,3）
* @param data 署名対象データ
* @param dataLen 署名対象データのレングス
* @param signature 署名データ
* @param signatureLen 署名データのレングス
* @param PKC DER形式のX.509公開鍵証明書のバイナリデータ
* @param PKCLen DER形式のX.509公開鍵証明書のバイナリデータのレングス
* @param algorithm ハッシュアルゴリズム
* @return int 0:正常終了（署名の検証に成功）
*             1:正常終了（署名の検証に失敗）
*             2:公開鍵証明書の検証に失敗の場合
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたPkczIndexに対応する証明書はありません）
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_verifySignatureByIndex( unsigned long int SessionHandle,
							 int PkczIndex,
							 unsigned char* data,
							 unsigned short int dataLen,
							 unsigned char* signature,
							 unsigned short int signatureLen,
							 unsigned char* PKC,
							 unsigned short int PKCLen,
							 int algorithm )
{

	int						iret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	void*					ptrCert;							//証明書確保領域ポインタ
	long					lCert;								//証明書レングス
	void*					ptrKeyId;							//鍵ペアID確保領域ポインタ
	long					lKeyId;								//鍵ペアIDレングス
	CK_OBJECT_HANDLE		PublicKeyObjHandle;					//公開鍵オブジェクトハンドル

	DEBUG_OutPut("IDMan_verifySignatureByIndex start \n");

	//初期処理
	iReturn = RET_IPOK;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPvSIdxParamChk( PkczIndex, data, dataLen, signature, signatureLen, PKC, PKCLen, algorithm );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignatureByIndex 入力パラメータのチェック エラー IDMan_IPvSIdxParamChk \n");
		return iReturn;
	}

	/**PkczIndexに対応する公開鍵証明書PKC(z)を取得する。 */
	iret = IDMan_IPgetCertIdxPKCS( SessionHandle, PkczIndex,&ptrCert,&lCert, &ptrKeyId, &lKeyId,1);
	/**PkczIndexに対応する公開鍵証明書PKC(z)取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignatureByIndex PkczIndexに対応する公開鍵証明書PKC(z)取得 エラー IDMan_IPgetCertIdxPKCS \n");
		return iReturn;
	}

	/**PKC(Z)の公開鍵オブジェクトを作成する。 */
	iret = IDMan_IPMkPublicKeyObj( SessionHandle, ptrCert, lCert, &PublicKeyObjHandle );
	/**−PKC(Z)の公開鍵オブジェクト作成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (ptrCert);
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignatureByIndex PKC(Z)の公開鍵オブジェクト作成 エラー IDMan_IPMkPublicKeyObj \n");
		return iReturn;
	}
	IDMan_StFree (ptrCert);

	/**パラメータの公開鍵証明書（PKC）の正当性チェックを行う。 */
	iret = IDMan_IPCheckPKC( SessionHandle, PKC, PKCLen,CERSIG_ALGORITHM,&PublicKeyObjHandle);
	/**パラメータの公開鍵証明書（PKC）の正当性チェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		if(iret == RET_IPOK_NO_VERIFY)
		{
			iReturn = RET_IPOK_NO_VERIFY_CERT;
		}
		else
		{
			iReturn = iret;
		}
		C_DestroyObject( SessionHandle, PublicKeyObjHandle);
		DEBUG_OutPut("IDMan_verifySignatureByIndex パラメータの公開鍵証明書（PKC）の正当性チェック エラー IDMan_IPCheckPKC \n");
		return iReturn;
	}

	/**公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, PublicKeyObjHandle);
	/**公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_verifySignatureByIndex 公開鍵オブジェクト破棄 エラー C_DestroyObject \n");
		return iReturn;
	}


	/**PKCの公開鍵オブジェクトを作成する。 */
	iret = IDMan_IPMkPublicKeyObj( SessionHandle, PKC, PKCLen, &PublicKeyObjHandle );
	/**PKCの公開鍵オブジェクト作成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_verifySignatureByIndex PKCの公開鍵オブジェクト作成 エラー IDMan_IPMkPublicKeyObj \n");
		return iReturn;
	}

	/**署名データの検証する。 */
	iret = IDMan_IPCheckSignature( SessionHandle,algorithm, data, dataLen,signature,signatureLen,PKC,PKCLen,PublicKeyObjHandle);
	/**署名データの検証エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		C_DestroyObject( SessionHandle, PublicKeyObjHandle);
		DEBUG_OutPut("IDMan_verifySignatureByIndex 署名データの検証 エラー IDMan_IPCheckSignature \n");
		return iReturn;
	}

	/**公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, PublicKeyObjHandle);
	/**公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_verifySignatureByIndex 公開鍵オブジェクト破棄2 エラー C_DestroyObject \n");
		return iReturn;
	}

	DEBUG_OutPut("IDMan_verifySignatureByIndex end \n");

	/**返り値をリターンする。 */
	return iReturn;
}
