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
 * ユーザ認証プログラム
 * \file IDMan_IPuserAuthPKCS11.c
 */

/* 作成ライブラリ関数名 */
#include "IDMan_IPCommon.h"


#define  PKCY_FOPEN_MODE			"r"							//PKC(Y)ファイルオープンモード

int IDMan_IPuAPKCS11ParamChk ( char*, char*, int );
int IDMan_IPuAPKCS11IdxParamChk ( int, int );
int IDMan_IPgetCertPkeyPKCSx( CK_SESSION_HANDLE, char*, char*, void**, unsigned long int *, void**, unsigned long int *, unsigned char*, unsigned long int *);
int IDMan_IPgetCertPkeyIdxPKCSx( CK_SESSION_HANDLE, int, void**, unsigned long int *, void**, unsigned long int *, unsigned char*, unsigned long int *);
int IDMan_IPgetCertPkeyPKCSy( char* keyid,unsigned char* ptrPublicKeyY,unsigned long int  * lPublicKeyLenY);
int IDMan_IPCheckCrlPkcX( char*,unsigned char*,long);
int IDMan_IPgetMakeRandData( unsigned char*,long* );
int IDMan_IPMkRandHash( CK_SESSION_HANDLE, int, unsigned char*, long, unsigned char*, unsigned long* );
int IDMan_IPgetPrivateKeyPKCSx( CK_SESSION_HANDLE,char*,CK_OBJECT_HANDLE* );
int IDMan_IPSignatureVerify( CK_SESSION_HANDLE,int, CK_OBJECT_HANDLE, unsigned char*, long, CK_OBJECT_HANDLE );


/**
* ユーザ認証の内部関数でパラメータチェック関数
* @author University of Tsukuba
* @param ptrSubjectDN  取得する公開鍵証明書のsubjectDN
* @param ptrIssuerDN  取得する公開鍵証明書のissuerDN
* @param iAlgorithm ハッシュアルゴリズム
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPuAPKCS11ParamChk ( char* ptrSubjectDN,
							   char* ptrIssuerDN,
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
	/**−引数のIssuerDNが全角スペースの場合、 */
	if(strcmp(ptrIssuerDN , "　") == 0)
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
* ユーザ認証Index指定版の内部関数でパラメータチェック関数
* @author University of Tsukuba
* @param iPkcxIndex  取得する公開鍵証明書のiPkcxIndex
* @param iAlgorithm ハッシュアルゴリズム
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPuAPKCS11IdxParamChk ( int iPkcxIndex,
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
* ユーザ認証関数の内部関数で公開鍵証明書PKC(X)の公開鍵取得関数
* （SubjectDN、IssuerDNに対応するICカード所有者（鍵ペアに対応する）公開鍵証明書PKC(X)から公開鍵を取得する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param subjectDN 鍵ペアに対する公開鍵証明書のsubjectDN
* @param issuerDN 鍵ペアに対する公開鍵証明書のissuerDN
* @param ptrCert 証明書確保領域ポインタ
* @param lCert 証明書レングス
* @param ptrKeyID 鍵ペアID確保領域ポインタ
* @param lKeyID 鍵ペアIDレングス
* @param ptrPublicKeyX 公開鍵データ
* @param lPublicKeyLenX 公開鍵データレングス
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたsubjectDNとissuerDNが同様の証明書はありません）
*            -9:初期化処理未実施
*           -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgetCertPkeyPKCSx( CK_SESSION_HANDLE Sessinhandle,
						  char* subjectDN,
						  char* issuerDN,
						  void ** ptrCert,
						  unsigned long int * lCert,
						  void ** ptrKeyID,
						  unsigned long int * lKeyID,
						  unsigned char* ptrPublicKeyX,
						  unsigned long int * lPublicKeyLenX)
{
	long					lret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	int						MatchingFlg;						//マッチングフラグ
	CK_ATTRIBUTE		ObjAttribute[3];					//
	CK_ATTRIBUTE		Attributeval[2];					//
	CK_OBJECT_HANDLE		ObjectHandle[OBJECT_HANDLE_MAX];	//オブジェクトハンドル
	CK_ULONG				ObjectHandleCnt;					//オブジェクトハンドル件数
	unsigned long int		LoopCnt;							//ループカウンタ
	char					SubjectDN[SUBJECT_DN_MAX];			//SubjectDN取得領域
	char					IssuerDN[ISSUER_DN_MAX];			//IssuerDN取得領域
	long					class;								//
	unsigned char			token;								//
	char					Label[1024];

	class=0;
	token=0;
	iReturn = RET_IPOK;
	MatchingFlg = 0;
	(*ptrCert) = 0x00;
	(*ptrKeyID) = 0x00;

	/**公開鍵証明書の検索操作初期化を行う。 */
	/**−CKA_CLASSにCERTIFICATEを指定する。 */
	/**−CKA_TOKENにTRUEを指定する。 */
	/**−CKA_LABELにPublicCertificateを指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_CERTIFICATE;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	ObjAttribute[1].type = CKA_TOKEN;
	token = CK_TRUE;
	ObjAttribute[1].pValue = &token;
	ObjAttribute[1].ulValueLen = sizeof(token);

	memset(Label,0x00, sizeof(Label));
	ObjAttribute[2].type = CKA_LABEL;
	strcpy(Label,CK_LABEL_PUBLIC_CERT);
	ObjAttribute[2].pValue = &Label;
	ObjAttribute[2].ulValueLen = strlen(Label);

	rv = C_FindObjectsInit(Sessinhandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 3);
	/**公開鍵証明書の検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**公開鍵証明書の件数取得を行う。 */
	/**公開鍵証明書の件数取得エラーの場合、 */
	rv = C_FindObjects(Sessinhandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	if (rv != CKR_OK)
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}
	/**取得した公開鍵証明書の件数を確認する。 */
	/**公開鍵証明書の件数が0件以下だった場合、 */
	if(ObjectHandleCnt <= 0)
	{
		/**−異常リターンする。 */
		C_FindObjectsFinal( Sessinhandle );
		return RET_IPNG;
	}

	/**取得した件数分処理を繰り返し引数のSubjectDN、IssuerDNに対応する公開鍵証明書PCK(X)を取得する。 */
	for(LoopCnt=0;LoopCnt < ObjectHandleCnt;LoopCnt++)
	{
		Attributeval[0].type = CKA_VALUE;
		Attributeval[0].pValue = 0x00;
		Attributeval[0].ulValueLen = 0;

		Attributeval[1].type = CKA_ID;
		Attributeval[1].pValue = 0x00;
		Attributeval[1].ulValueLen = 0;

		/**−証明書本体と鍵ペアIDのサイズ取得を行う。 */
		rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[LoopCnt],(CK_ATTRIBUTE_PTR)&Attributeval,2);
		/**−実データ無しの場合、 */
		if (rv == CKR_TOKEN_NOT_RECOGNIZED) 
		{
			/**−−公開鍵証明書取得処理を終了する。 */
			break;
		}
		/**−証明書本体と鍵ペアIDのサイズ取得エラーの場合、 */
		if (rv != CKR_OK) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = IDMan_IPRetJudgment(rv);
			break;
		}

		/**−−証明書本体の領域を確保する。 */
		(*ptrCert) = IDMan_StMalloc(Attributeval[0].ulValueLen+1);
		/**−証明書本体の領域確保エラーの場合、 */
		if ((*ptrCert) == 0x00) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			break;
		}
		memset((*ptrCert),0x00,Attributeval[0].ulValueLen+1);

		/**−鍵ペアIDの領域を確保する。 */
		(*ptrKeyID) = IDMan_StMalloc(Attributeval[1].ulValueLen+1);
		/**−−鍵ペアIDの領域確保エラーの場合、 */
		if ((*ptrKeyID) == 0x00) 
		{
			/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			break;
		}
		memset((*ptrKeyID),0x00,Attributeval[1].ulValueLen+1);


		Attributeval[0].type = CKA_VALUE;
		Attributeval[0].pValue = (*ptrCert);

		Attributeval[1].type = CKA_ID;
		Attributeval[1].pValue = (*ptrKeyID);

		/**−証明書本体と鍵ペアIDの取得を行う。 */
		rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[LoopCnt],(CK_ATTRIBUTE_PTR)&Attributeval,2);
		/**−証明書本体と鍵ペアIDの取得エラーの場合、 */
		if (rv != CKR_OK) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = IDMan_IPRetJudgment(rv);
			break;
		}
		
		/**−証明書よりSubjectDNとIssuerDNを取得する。 */
		memset(SubjectDN,0x00, sizeof(SubjectDN));
		memset(IssuerDN,0x00, sizeof(IssuerDN));
		lret = IDMan_CmGetDN((*ptrCert),Attributeval[0].ulValueLen,SubjectDN,IssuerDN);
		/**−証明書のSubjectDNとIssuerDN取得エラーの場合、 */
		if (lret != RET_IPOK) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			break;
		}

		/**−引数のsubjectDN・issuerDNと証明書より取得したSubjectDN・IssuerDNを比較する。 */
		/**−引数のsubjectDN・issuerDNと証明書より取得したSubjectDN・IssuerDNが同じ場合 */
		if(strcmp(subjectDN , SubjectDN) == 0 && strcmp(issuerDN , IssuerDN) == 0)
		{
			MatchingFlg = 1;
			/**−−証明書データのレングス値を保持する。 */
			(*lCert) = Attributeval[0].ulValueLen;
			/**−−鍵ペアIDのレングス値を保持する。 */
			(*lKeyID) = Attributeval[1].ulValueLen;
			/**−−公開鍵証明書取得処理を終了する。 */
			break;
		}
		IDMan_StFree ((*ptrCert));
		(*ptrCert) = 0x00;
		IDMan_StFree ((*ptrKeyID));
		(*ptrKeyID) = 0x00;
	}

	/**証明書の検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( Sessinhandle );
	/**証明書の検索操作終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常を設定する。 */
		if(iReturn == RET_IPOK)
		{
			iReturn = IDMan_IPRetJudgment(rv);
		}
	}

	/**返り値とマッチング検索結果（指定されたsubjectDN・issuerDNが同様の証明書があるか）の確認を行う。 */
	/**返り値が正常で引数で指定されたsubjectDN・issuerDNが同様の証明書がない場合、 */
	if(iReturn == RET_IPOK && MatchingFlg != 1)
	{
		/**−返り値に異常を設定する。 */
		iReturn = RET_IPNG_NO_MATCHING;
	}

	/**返り値の確認を行う。 */
	if( iReturn == RET_IPOK )
	{
		/**−正常の場合、 */
		/**−公開鍵証明書から公開鍵を取得する。*/
		lret = IDMan_CmGetPublicKey( (*ptrCert),(*lCert) ,(void*)ptrPublicKeyX,lPublicKeyLenX);
		/**−証明書の公開鍵取得エラーの場合、 */
		if (lret != RET_IPOK) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
		}
	}
	else
	{
		/**−異常の場合、確保領域を解放する */
		if((*ptrCert) != 0x00)
		{
			IDMan_StFree ((*ptrCert));
			(*ptrCert) = 0x00;
		}
		if((*ptrKeyID) != 0x00)
		{
			IDMan_StFree ((*ptrKeyID));
			(*ptrKeyID) = 0x00;
		}
	}

	/**返り値をリターンする。 */
	return iReturn;
}

/**
* ユーザ認証関数Index指定版の内部関数で公開鍵証明書PKC(X)の公開鍵取得関数
* （iPkcxIndexに対応するICカード所有者（鍵ペアに対応する）公開鍵証明書PKC(X)から公開鍵を取得する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param iPkcxIndex  取得する公開鍵証明書のiPkcxIndex
* @param ptrCert 証明書確保領域ポインタ
* @param lCert 証明書レングス
* @param ptrKeyID 鍵ペアID確保領域ポインタ
* @param lKeyID 鍵ペアIDレングス
* @param ptrPublicKeyX 公開鍵データ
* @param lPublicKeyLenX 公開鍵データレングス
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたPkcxIndexに対応する証明書はありません）
*            -9:初期化処理未実施
*           -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgetCertPkeyIdxPKCSx( CK_SESSION_HANDLE Sessinhandle,
						  int iPkcxIndex,
						  void ** ptrCert,
						  unsigned long int * lCert,
						  void ** ptrKeyID,
						  unsigned long int * lKeyID,
						  unsigned char* ptrPublicKeyX,
						  unsigned long int * lPublicKeyLenX)
{
	long					lret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	int						MatchingFlg;						//マッチングフラグ
	CK_ATTRIBUTE		ObjAttribute[32];					//
	CK_ATTRIBUTE		Attributeval[2];					//
	CK_OBJECT_HANDLE		ObjectHandle[OBJECT_HANDLE_MAX];	//オブジェクトハンドル
	CK_ULONG				ObjectHandleCnt;					//オブジェクトハンドル件数
	/*char					SubjectDN[SUBJECT_DN_MAX];*/			//SubjectDN取得領域
	/*char					IssuerDN[ISSUER_DN_MAX];*/			//IssuerDN取得領域
	long					class;								//
	unsigned char			token;								//
	char					Label[1024];
	unsigned long int		lindex;

	class=0;
	token=0;
	iReturn = RET_IPOK;
	MatchingFlg = 0;

	/**公開鍵証明書の検索操作初期化を行う。 */
	/**−CKA_CLASSにCERTIFICATEを指定する。 */
	/**−CKA_TOKENにTRUEを指定する。 */
	/**−CKA_LABELにPublicCertificateを指定する。 */
	/**−CKA_INDEXにパラメータiPkcxIndexを指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_CERTIFICATE;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	ObjAttribute[1].type = CKA_TOKEN;
	token = CK_TRUE;
	ObjAttribute[1].pValue = &token;
	ObjAttribute[1].ulValueLen = sizeof(token);

	memset(Label,0x00, sizeof(Label));
	ObjAttribute[2].type = CKA_LABEL;
	strcpy(Label,CK_LABEL_PUBLIC_CERT);
	ObjAttribute[2].pValue = &Label;
	ObjAttribute[2].ulValueLen = strlen(Label);

	ObjAttribute[3].type = CKA_INDEX;
	lindex = (unsigned long int)iPkcxIndex;
	ObjAttribute[3].pValue = &lindex;
	ObjAttribute[3].ulValueLen = sizeof(lindex);

	rv = C_FindObjectsInit(Sessinhandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 4);
	/**公開鍵証明書の検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**公開鍵証明書の件数取得を行う。 */
	/**公開鍵証明書の件数取得エラーの場合、 */
	rv = C_FindObjects(Sessinhandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	if (rv != CKR_OK)
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}
	/**取得した公開鍵証明書の件数を確認する。 */
	/**公開鍵証明書の件数が0件だった場合、 */
	if(ObjectHandleCnt == 0)
	{
		/**−異常リターンする。 */
		C_FindObjectsFinal( Sessinhandle );
		return RET_IPNG_NO_MATCHING;
	}

	Attributeval[0].type = CKA_VALUE;
	Attributeval[0].pValue = 0x00;
	Attributeval[0].ulValueLen = 0;

	Attributeval[1].type = CKA_ID;
	Attributeval[1].pValue = 0x00;
	Attributeval[1].ulValueLen = 0;

	/**証明書本体と鍵ペアIDのサイズ取得を行う。 */
	rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[0],(CK_ATTRIBUTE_PTR)&Attributeval,2);
	/**−実データ無しの場合、 */
	if (rv == CKR_TOKEN_NOT_RECOGNIZED) 
	{
		/**−異常リターンする。 */
		C_FindObjectsFinal( Sessinhandle );
		return RET_IPNG_NO_MATCHING;
	}
	/**証明書本体と鍵ペアIDのサイズ取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}

	/**−−証明書本体の領域を確保する。 */
	(*ptrCert) = IDMan_StMalloc(Attributeval[0].ulValueLen+1);
	/**−証明書本体の領域確保エラーの場合、 */
	if ((*ptrCert) == 0x00) 
	{
		/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
		iReturn = RET_IPNG;
		C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}
	memset((*ptrCert),0x00,Attributeval[0].ulValueLen+1);

	/**−鍵ペアIDの領域を確保する。 */
	(*ptrKeyID) = IDMan_StMalloc(Attributeval[1].ulValueLen+1);
	/**−−鍵ペアIDの領域確保エラーの場合、 */
	if ((*ptrKeyID) == 0x00) 
	{
		/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
		iReturn = RET_IPNG;
		C_FindObjectsFinal( Sessinhandle );
		IDMan_StFree ((*ptrCert));
		return iReturn;
	}
	memset((*ptrKeyID),0x00,Attributeval[1].ulValueLen+1);

	Attributeval[0].type = CKA_VALUE;
	Attributeval[0].pValue = (*ptrCert);

	Attributeval[1].type = CKA_ID;
	Attributeval[1].pValue = (*ptrKeyID);

	/**証明書本体と鍵ペアIDの取得を行う。 */
	rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[0],(CK_ATTRIBUTE_PTR)&Attributeval,2);
	/**証明書本体と鍵ペアIDの取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( Sessinhandle );
		IDMan_StFree ((*ptrCert));
		IDMan_StFree ((*ptrKeyID));
		return iReturn;
	}

	/**証明書データのレングス値を保持する。 */
	(*lCert) = Attributeval[0].ulValueLen;
	/**鍵ペアIDのレングス値を保持する。 */
	(*lKeyID) = Attributeval[1].ulValueLen;

	/**証明書の検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( Sessinhandle );
	/**証明書の検索操作終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		IDMan_StFree ((*ptrCert));
		IDMan_StFree ((*ptrKeyID));
		return iReturn;
	}

	/**−公開鍵証明書から公開鍵を取得する。*/
	lret = IDMan_CmGetPublicKey( (*ptrCert),(*lCert) ,(void*)ptrPublicKeyX,lPublicKeyLenX);
	/**−証明書の公開鍵取得エラーの場合、 */
	if (lret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		IDMan_StFree ((*ptrCert));
		IDMan_StFree ((*ptrKeyID));
		return iReturn;
	}

	/**返り値をリターンする。 */
	return iReturn;
}

/**
* ユーザ認証関数の内部関数で公開鍵証明書PKC(Y)の公開鍵取得関数
* （設定情報から公開鍵証明書PKC(Y)のファイルパスを取得し、ファイルよりPEM形式の公開鍵証明書PKC(Y)を取得する。
* 　取得した公開鍵証明書PKC(Y)をDER形式に変換した後、DER形式の公開鍵証明書PKC(Y)から公開鍵を取得する）
* @author University of Tsukuba
* @param keyid 鍵ペアID
* @param ptrPublicKeyY 公開鍵データ
* @param lPublicKeyLenY 公開鍵データレングス
* @return int 0:正常  -6:異常
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgetCertPkeyPKCSy( char* keyid,unsigned char* ptrPublicKeyY,unsigned long int * lPublicKeyLenY)
{
	long				lret;				//戻り値long
	int					iret;				//戻り値int
	/*char*				pret;*/				//戻り値char*
	int					iReturn;			//戻り値
	char				mem1Buff[1024];		//メンババッファ
	char				PkcYFdirBuff[1024];	//PKC(Y)ファイルパス
	/*unsigned char		ReadBuff[1024];*/
	/*long				lReadBuffLen;*/
	/*long				lFileSize;*/
	/*unsigned char*		ptrFile;*/
	/*unsigned char		PemFile[2048];*/
	char			DerFile[2048];
	unsigned long int	lDerFileLen;
	/*char				cKeyIdBuff[1024];*/	//キーIDバッファ
	unsigned char		uKeyIdBuff;			//キーIDバッファ


	iReturn = RET_IPOK;
	memset(mem1Buff,0x00, sizeof(mem1Buff));

	memcpy(&uKeyIdBuff,keyid+7,1);
	strcpy(mem1Buff,TRUSTANCHORCERT);
	if(uKeyIdBuff == 1 )
	{
		strcat(mem1Buff,"01");
	}
	else if(uKeyIdBuff == 2)
	{
		strcat(mem1Buff,"02");
	}
	else if(uKeyIdBuff == 3)
	{
		strcat(mem1Buff,"03");
	}

	memset(PkcYFdirBuff,0x00, sizeof(PkcYFdirBuff));
	/**設定情報から公開鍵証明書PKC(Y)取得を行う。 */
	memset(DerFile,0x00, sizeof(DerFile));
	iret = IDMan_StReadSetData(mem1Buff, DerFile,&lDerFileLen);
	/**設定情報から公開鍵証明書PKC(Y)取得エラーの場合、 */
	if(iret < RET_IPOK)
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}

	/**公開鍵証明書PKC(Y)DER形式から公開鍵を取得する。 */
	lret = IDMan_CmGetPublicKey(DerFile,lDerFileLen ,ptrPublicKeyY,lPublicKeyLenY);
	/**公開鍵証明書PKC(Y)DER形式から公開鍵取得エラーの場合、 */
	if (lret != 0) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}

	/**正常リターンする。 */
	return iReturn;
}

/**
* ユーザ認証関数の内部関数で失効リストチェック関数
* （設定情報から失効リストのファイルパスを取得し、失効リストチェックを行う）
* @author University of Tsukuba
* @param keyid 鍵ペアID
* @param ptrCertdata 公開鍵証明書PKC(x)データ
* @param lCertLen 公開鍵証明書PKC(x)データレングス
* @return int 0:正常 -6:異常 2:公開鍵証明書が失効しているの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPCheckCrlPkcX( char* keyid,unsigned char* ptrCertdata,long lCertLen)
{
	long				lret;				//戻り値long
	int					iret;				//戻り値int
	/*char*				pret;*/				//戻り値char*
	int					iReturn;			//戻り値
	char				mem1Buff[1024];		//メンババッファ
	/*char				mem2Buff[1024];*/		//メンババッファ
	char				CRLFdirBuff[4096];	//CRLデータ
	unsigned long int	CRLFdirLen;			//CRLデータレングス
	int					iCRLFdirLen;		//CRLデータレングス
	unsigned char		lKeyIdBuff;			//キーIDバッファ


	iReturn = RET_IPOK;
	memset(mem1Buff,0x00, sizeof(mem1Buff));

	memcpy(&lKeyIdBuff,keyid+7,1);
	strcpy(mem1Buff,CRL);
	if(lKeyIdBuff == 1)
	{
		strcat(mem1Buff,"01");
	}
	else if(lKeyIdBuff == 2)
	{
		strcat(mem1Buff,"02");
	}
	else if(lKeyIdBuff == 3)
	{
		strcat(mem1Buff,"03");
	}

	memset(CRLFdirBuff,0x00, sizeof(CRLFdirBuff));
	/**設定情報から失効リスト取得を行う。 */
	iret = IDMan_StReadSetData(mem1Buff, CRLFdirBuff,&CRLFdirLen);
	/**設定情報から失効リスト取得エラーの場合、 */
	if(iret < RET_IPOK)
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}

	/**失効リストチェックを行う。 */
	iCRLFdirLen = CRLFdirLen;
	lret = IDMan_CmCheckCRL(ptrCertdata,lCertLen,CRLFdirBuff,iCRLFdirLen);
	/**失効リストチェックエラーの場合、 */
	if(lret != RET_IPOK)
	{
		/**−異常リターンする。 */
		iReturn = RET_IPOK_NO_CRL_ERR;
		return iReturn;
	}

	/**正常リターンする。 */
	return iReturn;
}

/**
* ユーザ認証関数の内部関数で乱数生成関数
* （設定情報より乱数サイズを取得し、乱数を生成する）
* @author University of Tsukuba
* @param ptrRandData 乱数データ
* @param lRandData 乱数データレングス
* @return int 0:正常  -6:異常 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgetMakeRandData( unsigned char* ptrRandData,long* lRandData)
{
	long				lret;				//戻り値long
	int					iret;				//戻り値int
	int					iReturn;			//戻り値
	char				RandSizeBuff[1024];	//乱数サイズ
	unsigned long int	RandSizeBuffLen;	//乱数サイズレングス

	iReturn = RET_IPOK;

	memset(RandSizeBuff,0x00, sizeof(RandSizeBuff));
	/**設定情報から乱数サイズ取得を行う。 */
	iret = IDMan_StReadSetData(RANDOMSEEDSIZE,RandSizeBuff,&RandSizeBuffLen);
	/**設定情報から乱数サイズ取得エラーの場合、 */
	if(iret < RET_IPOK)
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}

	(*lRandData) = atol(RandSizeBuff);
	/**乱数生成を行う。 */
	lret = IDMan_CmMakeRand( (*lRandData), ptrRandData);
	/**−乱数生成エラーの場合、 */
	if(lret != RET_IPOK)
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}

	/**正常リターンする。 */
	return iReturn;

}

/**
* ユーザ認証関数の内部関数で乱数ハッシュ値生成関数
* （乱数データをハッシュ化する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param Algorithm ハッシュアルゴリズム
* @param ptrRandData 乱数データ
* @param RandDataLen 乱数データレングス
* @param pHashData ハッシュデータ
* @param iHashDataLen ハッシュデータレングス
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPMkRandHash(	CK_SESSION_HANDLE Sessinhandle,
						int Algorithm ,
						unsigned char* ptrRandData,
						long RandDataLen,
						unsigned char* pHashData,
						unsigned long* iHashDataLen )
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
	rv = C_DigestUpdate( Sessinhandle, ptrRandData,RandDataLen);
	/**ダイジェスト作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**ダイジェストレングス取得を行う。 */
	rv = C_DigestFinal( Sessinhandle, 0x00,iHashDataLen);
	/**ダイジェストレングス取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**ダイジェスト作成終了を行う。 */
	rv = C_DigestFinal( Sessinhandle, pHashData,iHashDataLen);
	/**ダイジェスト作成終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**正常リターンする。 */
	return iReturn;
}

/**
* ユーザ認証関数の内部関数でPKC（X）の秘密鍵オブジェクトハンドル取得関数
* （鍵ぺアIDに該当するPKC（X）の秘密鍵オブジェクトハンドルを取得する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param KeyId 鍵ペアID
* @param ptrObjectHandle 秘密鍵オブジェクトハンドル
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgetPrivateKeyPKCSx( CK_SESSION_HANDLE Sessinhandle,char* KeyId,CK_OBJECT_HANDLE* ptrObjectHandle)
{
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	CK_ATTRIBUTE		ObjAttribute[4];					//
	CK_OBJECT_HANDLE		ObjectHandle[OBJECT_HANDLE_MAX];	//オブジェクトハンドル
	CK_ULONG				ObjectHandleCnt;					//オブジェクトハンドル件数
	long					class;								//
	unsigned char			token;								//
	char					Label[1024];

	class=0;
	token=0;
	iReturn = RET_IPOK;

	/**公開鍵証明書の秘密鍵検索操作初期化を行う。 */
	/**−CKA_CLASSにCK_LABEL_PRIVATE_KEYを指定する。 */
	/**−CKA_TOKENにTRUEを指定する。 */
	/**−CKA_LABELにSecretkeyを指定する。 */
	/**−CKA_IDに鍵ペアID格納アドレスを指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_PRIVATE_KEY;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	ObjAttribute[1].type = CKA_TOKEN;
	token = CK_TRUE;
	ObjAttribute[1].pValue = &token;
	ObjAttribute[1].ulValueLen = sizeof(token);

	memset(Label,0x00, sizeof(Label));
	ObjAttribute[2].type = CKA_LABEL;
	strcpy(Label,CK_LABEL_PRIVATE_KEY);
	ObjAttribute[2].pValue = &Label;
	ObjAttribute[2].ulValueLen = strlen(Label);

	ObjAttribute[3].type = CKA_ID;
	ObjAttribute[3].pValue = KeyId;
	ObjAttribute[3].ulValueLen = 8;

	rv = C_FindObjectsInit(Sessinhandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 4);
	/**公開鍵証明書の秘密鍵検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**公開鍵証明書秘密鍵の件数取得を行う。 */
	rv = C_FindObjects(Sessinhandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	/**公開鍵証明書秘密鍵の件数取得エラーの場合、 */
	if (rv != CKR_OK)
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}
	/**取得した公開鍵証明書秘密鍵の件数を確認する。 */
	/**公開鍵証明書秘密鍵の件数が1件でない場合、 */
	if(ObjectHandleCnt != 1)
	{
		/**−異常リターンする。 */
		C_FindObjectsFinal( Sessinhandle );
		return RET_IPNG;
	}

	/**証明書秘密鍵の検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( Sessinhandle );
	/**公開鍵証明書秘密鍵の件数が0件以下だった場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	(*ptrObjectHandle) = ObjectHandle[0];

	/**正常リターンする。 */
	return iReturn;
}

/**
* ユーザ認証関数の内部関数でユーザの正当性検証関数
* （ハッシュ値を秘密鍵で署名しPKC（X）の公開鍵で複合化した値とハッシュ値を検証する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param Algorithm アルゴリズム
* @param PriKeyObjectHandle  秘密鍵オブジェクトハンドル
* @param pHash ハッシュ値領域ポインタ
* @param lHashLen ハッシュ値レングス
* @param XPubKeyObjectHandle 公開鍵オブジェクトハンドル
* @return int 0:正常 
*             3:チャレンジ＆レスポンスの認証に失敗の場合
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPSignatureVerify( CK_SESSION_HANDLE Sessinhandle,
							 int Algorithm ,
							 CK_OBJECT_HANDLE PriKeyObjectHandle,
							 unsigned char* pHash,
							 long lHashLen,
							 CK_OBJECT_HANDLE XPubKeyObjectHandle)
{
	int						iret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	CK_MECHANISM		strMechanismSign;					//メカニズム情報（Sign）
	unsigned char			Signature[1024];
	CK_ULONG				SignatureLen ;
	CK_MECHANISM		strMechanismVerify;					//メカニズム情報（Verify）
	unsigned char			SignatureData[2048];				//パディング後署名対象データ
	long					SignatureDataLen;					//パディング後署名対象データレングス

	iReturn = RET_IPOK;

	/**署名対象データ（ハッシュ値）にパディング追加を行う。 */
	memset(SignatureData,0x00, sizeof(SignatureData));
	SignatureDataLen = CERSIG_PADDING_SIZE;
	iret = IDMan_CmPadding( Algorithm ,SignatureDataLen, pHash, lHashLen ,SignatureData );
	/**署名対象データ（ハッシュ値）パディング追加エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		return iReturn;
	}

	strMechanismSign.mechanism = CKM_RSA_PKCS;
	/**署名処理の初期化を行う。 */
	rv = C_SignInit( Sessinhandle, &strMechanismSign,PriKeyObjectHandle);
	/**署名処理の初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名レングス取得を行う。 */
	rv = C_Sign( Sessinhandle, SignatureData,SignatureDataLen,0x00,(CK_ULONG_PTR)&SignatureLen);
	/**署名レングス取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	memset(Signature,0x00, sizeof(Signature));
	/**署名処理を行う。 */
	rv = C_Sign( Sessinhandle, SignatureData,SignatureDataLen,Signature,(CK_ULONG_PTR)&SignatureLen);
	/**署名処理エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名検証の開始を行う。 */
	strMechanismVerify.mechanism = CKM_RSA_PKCS;
	rv = C_VerifyInit( Sessinhandle, &strMechanismVerify,XPubKeyObjectHandle);
	/**署名検証の開始エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名を検証する。 */
	rv =  C_Verify( Sessinhandle, SignatureData, SignatureDataLen, Signature, SignatureLen);
	if (rv != CKR_OK) 
	{
		/**−署名データ不正か確認する。 */
		/**−署名データ不正の場合、 */
		if(rv == CKR_SIGNATURE_INVALID)
		{
			/**−−チャレンジ＆レスポンスの認証に失敗（３）をリターンする。 */
			iReturn = RET_IPOK_NO_VERIFY_CERTXKEY;
		}
		/**−署名検証エラーの場合、 */
		else
		{
			/**−−返り値に異常リターンする。 */
			iReturn = IDMan_IPRetJudgment(rv);
		}
		return iReturn;
	}

	/**正常リターンする。 */
	return iReturn;
}

/**
* ユーザ認証関数
* （署名対象データのハッシュ値を求め、ICカード内の秘密鍵で署名をする。署名に利用する鍵ペアは、公開鍵証明書のsubjectDNとissuerDNで指定する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param subjectDN 署名に使用する鍵ペアに対する公開鍵証明書のsubjectDN
* @param issuerDN 署名に使用する鍵ペアに対する公開鍵証明書のissuerDN
* @param algorithm ハッシュアルゴリズム
* @return int 0:正常終了（認証に成功）
*             1:公開鍵証明書の検証に失敗の場合
*             2:公開鍵証明書が失効しているの場合
*             3:チャレンジ＆レスポンスの認証に失敗の場合
*            -2:ICカードリーダにICカードが挿入されてない
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたsubjectDNとissuerDNが同様の証明書はありません）
*            -9:初期化処理未実施
*           -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_userAuthPKCS11(	 unsigned long int SessionHandle,
							 char* subjectDN,
							 char* issuerDN,
							 int algorithm )
{
	int						iret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	void 					*CertX;								//PKC（X）証明書確保領域
	unsigned long int 		lCertX;								//PKC（X）証明書レングス
	void 					*KeyId;								//鍵ペアID確保領域
	unsigned long int 		lKeyId;								//鍵ペアIDレングス
	unsigned char			PublickeyX[1024];					//PKC（X）の公開鍵確保領域
	unsigned long int 		lPublickeyX;						//PKC（X）公開鍵レングス
	unsigned char			PublickeyY[1024];					//PKC（Y）の公開鍵確保領域
	unsigned long int 		lPublickeyY;						//PKC（Y）公開鍵レングス
	unsigned char			RandData[1024];						//乱数データ
	long					RandDataLen;						//乱数データレングス
	unsigned char			HashData[1024];						//乱数ハッシュデータ
	unsigned long				HashDataLen;						//乱数ハッシュデータレングス
	CK_ATTRIBUTE		ObjAttribute[3];					//
	long					class;								//
	char					Label[1024];						//
	CK_OBJECT_HANDLE		YPublicKeyObjHandle;				//公開鍵オブジェクトハンドルPKC(Y)
	CK_OBJECT_HANDLE		XPublicKeyObjHandle;				//公開鍵オブジェクトハンドルPKC(X)
	CK_OBJECT_HANDLE		PkeyObjectHandle;					//秘密鍵オブジェクトハンドル

	DEBUG_OutPut("IDMan_userAuthPKCS11 start\n");

	iReturn = RET_IPOK;
	class=0;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPuAPKCS11ParamChk(  subjectDN, issuerDN, algorithm );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11 入力パラメータのチェック エラー IDMan_IPuAPKCS11ParamChk\n");
		return iReturn;
	}

	/**SubjectDN、IssuerDNに対応する公開鍵証明書PKC(x)と鍵ペアIDと公開鍵を取得する。 */
	memset(PublickeyX,0x00, sizeof(PublickeyX));
	iret = IDMan_IPgetCertPkeyPKCSx( SessionHandle, subjectDN, issuerDN, &CertX, &lCertX, &KeyId, &lKeyId, PublickeyX, &lPublickeyX);
	/**−SubjectDN、IssuerDNに対応する公開鍵証明書PKC(x)と鍵ペアIDと公開鍵取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11 SubjectDN、IssuerDNに対応する公開鍵証明書PKC(x)と鍵ペアIDと公開鍵取得 エラー IDMan_IPgetCertPkeyPKCSx\n");
		return iReturn;
	}

	/**公開鍵証明書PKC(Y)の公開鍵を取得する。 */
	iret = IDMan_IPgetCertPkeyPKCSy( (char*) KeyId,PublickeyY, &lPublickeyY);
	/**公開鍵証明書PKC(Y)の公開鍵取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11 公開鍵証明書PKC(Y)の公開鍵取得 エラー IDMan_IPgetCertPkeyPKCSy\n");
		return iReturn;
	}

	/**PKC(Y)公開鍵オブジェクトの作成を行う。 */
	/**−CKA_CLASSにCKO_PUBLIC_KEYを指定する。 */
	/**−CKA_LABELにPKC(Y)を指定する。 */
	/**−VALUEに上記で取得したPKC(Y)の公開鍵を指定する。 */
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
	ObjAttribute[2].pValue = &PublickeyY;
	ObjAttribute[2].ulValueLen = lPublickeyY;

	rv = C_CreateObject(SessionHandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 3,&YPublicKeyObjHandle);
	/**PKC(Y)公開鍵オブジェクトの作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11 PKC(Y)公開鍵オブジェクトの作成 エラー C_CreateObject\n");
		return iReturn;
	}

	/**PKC(X）の署名検証を行う。 */
	iret = IDMan_IPCheckPKC(  SessionHandle, CertX, lCertX, CERSIG_ALGORITHM , &YPublicKeyObjHandle );
	/**PKC(X）の署名検証エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		if(iret == RET_IPOK_NO_VERIFY)
		{
			iReturn = RET_IPOK_NO_VERIFY_CERTX;
		}
		else
		{
			iReturn = iret;
		}
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		C_DestroyObject( SessionHandle, YPublicKeyObjHandle);
		DEBUG_OutPut("IDMan_userAuthPKCS11 PKC(X）の署名検証 エラー IDMan_IPCheckPKC \n");
		return iReturn;
	}

	/**PKC(Y)公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, YPublicKeyObjHandle);
	/**PKC(Y)公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11 PKC(Y)公開鍵オブジェクト破棄 エラー C_DestroyObject \n");
		return iReturn;
	}

	/**PKC(X）の失効リストチェックを行う。 */
	iret = IDMan_IPCheckCrlPkcX( KeyId,CertX, lCertX);
	/**PKC(X）の失効リストチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11 PKC(X）の失効リストチェック エラー IDMan_IPCheckCrlPkcX \n");
		return iReturn;
	}
	IDMan_StFree ((CertX));

	/**設定情報より乱数サイズを取得し乱数を生成する。 */
	memset(RandData,0x00, sizeof(RandData));
	iret = IDMan_IPgetMakeRandData( RandData, &RandDataLen);
	/**設定情報より乱数サイズを取得と乱数生成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11 設定情報より乱数サイズを取得と乱数生成 エラー IDMan_IPgetMakeRandData \n");
		return iReturn;
	}

	/**乱数のハッシュ化を行う。 */
	memset(HashData,0x00, sizeof(HashData));
	iret = IDMan_IPMkRandHash(SessionHandle,algorithm, RandData, RandDataLen,HashData,&HashDataLen);
	/**乱数のハッシュ化エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11 乱数のハッシュ化 エラー IDMan_IPMkRandHash \n");
		return iReturn;
	}

	/**秘密鍵のオブジェクト取得を行う。 */
	iret = IDMan_IPgetPrivateKeyPKCSx(SessionHandle,KeyId,&PkeyObjectHandle);
	/**秘密鍵のオブジェクト取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11 秘密鍵のオブジェクト取得エラー IDMan_IPgetPrivateKeyPKCSx \n");
		return iReturn;
	}
	IDMan_StFree ((KeyId));

	/**PKC(X)公開鍵オブジェクトの作成を行う。 */
	/**−CKA_CLASSにCKO_PUBLIC_KEYを指定する。 */
	/**−CKA_LABELにPKC(X)を指定する。 */
	/**−VALUEに上記で取得したPKC(X)の公開鍵を指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_PUBLIC_KEY;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	memset(Label,0x00, sizeof(Label));
	ObjAttribute[1].type = CKA_LABEL;
	strcpy(Label,CK_LABEL_PUBLIC_KEY);
	ObjAttribute[1].pValue = &Label;
	ObjAttribute[1].ulValueLen = strlen(Label);

	ObjAttribute[2].type = CKA_VALUE;
	ObjAttribute[2].pValue = &PublickeyX;
	ObjAttribute[2].ulValueLen = lPublickeyX;

	rv = C_CreateObject(SessionHandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 3,&XPublicKeyObjHandle);
	/**PKC(X)公開鍵オブジェクトの作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11 PKC(X)公開鍵オブジェクトの作成 エラー C_CreateObject \n");
		return iReturn;
	}

	/**署名生成と認証処理を行う。 */
	iret = IDMan_IPSignatureVerify(SessionHandle,algorithm,PkeyObjectHandle,HashData,HashDataLen,XPublicKeyObjHandle);
	/**署名生成と認証処理エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		C_DestroyObject( SessionHandle, XPublicKeyObjHandle);
		DEBUG_OutPut("IDMan_userAuthPKCS11 署名生成と認証処理エラー IDMan_IPSignatureVerify \n");
		return iReturn;
	}

	/**PKC(X)公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, XPublicKeyObjHandle);
	/**PKC(X)公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11 PKC(X)公開鍵オブジェクト破棄 エラー C_DestroyObject \n");
		return iReturn;
	}

	/**正常リターンする。 */
	DEBUG_OutPut("IDMan_userAuthPKCS11 end \n");
	return iReturn;
}

/**
* ユーザ認証関数Index指定版
* （署名対象データのハッシュ値を求め、ICカード内の秘密鍵で署名をする。署名に利用する鍵ペアは、PkczIndexで公開鍵証明書を指定する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param PkczIndex 署名に使用する鍵ペアに対する公開鍵証明書のインデックス（1,2,3）
* @param algorithm ハッシュアルゴリズム
* @return int 0:正常終了（認証に成功）
*             1:公開鍵証明書の検証に失敗の場合
*             2:公開鍵証明書が失効しているの場合
*             3:チャレンジ＆レスポンスの認証に失敗の場合
*            -2:ICカードリーダにICカードが挿入されてない
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたPkcxIndexに対応する証明書はありません）
*            -9:初期化処理未実施
*           -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_userAuthPKCS11ByIndex( unsigned long int SessionHandle,
							 int PkcxIndex,
							 int algorithm )
{
	int						iret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	void					*CertX;							//PKC（X）証明書確保領域
	unsigned long int 		lCertX;								//PKC（X）証明書レングス
	void					*KeyId;							//鍵ペアID確保領域
	unsigned long int 		lKeyId;								//鍵ペアIDレングス
	unsigned char			PublickeyX[1024];					//PKC（X）の公開鍵確保領域
	unsigned long int 		lPublickeyX;						//PKC（X）公開鍵レングス
	unsigned char			PublickeyY[1024];					//PKC（Y）の公開鍵確保領域
	unsigned long int 		lPublickeyY;						//PKC（Y）公開鍵レングス
	unsigned char			RandData[1024];						//乱数データ
	long					RandDataLen;						//乱数データレングス
	unsigned char			HashData[1024];						//乱数ハッシュデータ
	unsigned long				HashDataLen;						//乱数ハッシュデータレングス
	CK_ATTRIBUTE		ObjAttribute[3];					//
	long					class;								//
	char					Label[1024];						//
	CK_OBJECT_HANDLE		YPublicKeyObjHandle;				//公開鍵オブジェクトハンドルPKC(Y)
	CK_OBJECT_HANDLE		XPublicKeyObjHandle;				//公開鍵オブジェクトハンドルPKC(X)
	CK_OBJECT_HANDLE		PkeyObjectHandle;					//秘密鍵オブジェクトハンドル

	DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex start \n");

	iReturn = RET_IPOK;
	class=0;
	CertX = 0x00;
	KeyId = 0x00;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPuAPKCS11IdxParamChk(  PkcxIndex, algorithm );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex 入力パラメータのチェック エラー IDMan_IPuAPKCS11IdxParamChk \n");
		return iReturn;
	}

	/**PkcxIndexに対応する公開鍵証明書PKC(x)と鍵ペアIDと公開鍵を取得する。 */
	memset(PublickeyX,0x00, sizeof(PublickeyX));
	iret = IDMan_IPgetCertPkeyIdxPKCSx( SessionHandle, PkcxIndex, &CertX, &lCertX, &KeyId, &lKeyId, PublickeyX, &lPublickeyX);
	/**−PkcxIndexに対応する公開鍵証明書PKC(x)と鍵ペアIDと公開鍵取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex PkcxIndexに対応する公開鍵証明書PKC(x)と鍵ペアIDと公開鍵取得 エラー IDMan_IPgetCertPkeyIdxPKCSx \n");
		return iReturn;
	}

	/**公開鍵証明書PKC(Y)の公開鍵を取得する。 */
	iret = IDMan_IPgetCertPkeyPKCSy( (char*) KeyId,PublickeyY, &lPublickeyY);
	/**公開鍵証明書PKC(Y)の公開鍵取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex 公開鍵証明書PKC(Y)の公開鍵取得 エラー IDMan_IPgetCertPkeyPKCSy \n");
		return iReturn;
	}

	/**PKC(Y)公開鍵オブジェクトの作成を行う。 */
	/**−CKA_CLASSにCKO_PUBLIC_KEYを指定する。 */
	/**−CKA_LABELにPKC(Y)を指定する。 */
	/**−VALUEに上記で取得したPKC(Y)の公開鍵を指定する。 */
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
	ObjAttribute[2].pValue = &PublickeyY;
	ObjAttribute[2].ulValueLen = lPublickeyY;

	rv = C_CreateObject(SessionHandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 3,&YPublicKeyObjHandle);
	/**PKC(Y)公開鍵オブジェクトの作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex PKC(Y)公開鍵オブジェクトの作成 エラー C_CreateObject \n");
		return iReturn;
	}

	/**PKC(X）の署名検証を行う。 */
	iret = IDMan_IPCheckPKC(  SessionHandle , CertX, lCertX, CERSIG_ALGORITHM, &YPublicKeyObjHandle );
	/**PKC(X）の署名検証エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		if(iret == RET_IPOK_NO_VERIFY)
		{
			iReturn = RET_IPOK_NO_VERIFY_CERTX;
		}
		else
		{
			iReturn = iret;
		}
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		C_DestroyObject( SessionHandle, YPublicKeyObjHandle);
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex PKC(X）の署名検証 エラー IDMan_IPCheckPKC \n");
		return iReturn;
	}

	/**PKC(Y)公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, YPublicKeyObjHandle);
	/**PKC(Y)公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex PKC(Y)公開鍵オブジェクト破棄 エラー C_DestroyObject \n");
		return iReturn;
	}

	/**PKC(X）の失効リストチェックを行う。 */
	iret = IDMan_IPCheckCrlPkcX( (char*) KeyId, (unsigned char*) CertX, lCertX);
	/**PKC(X）の失効リストチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		IDMan_StFree ((CertX));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex PKC(X）の失効リストチェック エラー IDMan_IPCheckCrlPkcX \n");
		return iReturn;
	}
	IDMan_StFree ((CertX));

	/**設定情報より乱数サイズを取得し乱数を生成する。 */
	memset(RandData,0x00, sizeof(RandData));
	iret = IDMan_IPgetMakeRandData( RandData, &RandDataLen);
	/**設定情報より乱数サイズを取得と乱数生成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex 設定情報より乱数サイズを取得と乱数生成 エラー IDMan_IPgetMakeRandData \n");
		return iReturn;
	}

	/**乱数のハッシュ化を行う。 */
	memset(HashData,0x00, sizeof(HashData));
	iret = IDMan_IPMkRandHash(SessionHandle,algorithm, RandData, RandDataLen,HashData,&HashDataLen);
	/**乱数のハッシュ化エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex 乱数のハッシュ化 エラー IDMan_IPMkRandHash \n");
		return iReturn;
	}

	/**秘密鍵のオブジェクト取得を行う。 */
	iret = IDMan_IPgetPrivateKeyPKCSx(SessionHandle,(char*)KeyId,&PkeyObjectHandle);
	/**秘密鍵のオブジェクト取得エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		IDMan_StFree ((KeyId));
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex 秘密鍵のオブジェクト取得エラー IDMan_IPgetPrivateKeyPKCSx \n");
		return iReturn;
	}
	IDMan_StFree ((KeyId));

	/**PKC(X)公開鍵オブジェクトの作成を行う。 */
	/**−CKA_CLASSにCKO_PUBLIC_KEYを指定する。 */
	/**−CKA_LABELにPKC(X)を指定する。 */
	/**−VALUEに上記で取得したPKC(X)の公開鍵を指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_PUBLIC_KEY;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	memset(Label,0x00, sizeof(Label));
	ObjAttribute[1].type = CKA_LABEL;
	strcpy(Label,CK_LABEL_PUBLIC_KEY);
	ObjAttribute[1].pValue = &Label;
	ObjAttribute[1].ulValueLen = strlen(Label);

	ObjAttribute[2].type = CKA_VALUE;
	ObjAttribute[2].pValue = &PublickeyX;
	ObjAttribute[2].ulValueLen = lPublickeyX;

	rv = C_CreateObject(SessionHandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 3,&XPublicKeyObjHandle);
	/**PKC(X)公開鍵オブジェクトの作成エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex PKC(X)公開鍵オブジェクトの作成 エラー C_CreateObject \n");
		return iReturn;
	}

	/**署名生成と認証処理を行う。 */
	iret = IDMan_IPSignatureVerify(SessionHandle,algorithm,PkeyObjectHandle,HashData,HashDataLen,XPublicKeyObjHandle);
	/**署名生成と認証処理エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		C_DestroyObject( SessionHandle, XPublicKeyObjHandle);
		iReturn = iret;
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex 署名生成と認証処理エラー IDMan_IPSignatureVerify  \n");
		return iReturn;
	}

	/**PKC(X)公開鍵オブジェクトを破棄する。 */
	rv = C_DestroyObject( SessionHandle, XPublicKeyObjHandle);
	/**PKC(X)公開鍵オブジェクト破棄エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex PKC(X)公開鍵オブジェクト破棄 エラー C_DestroyObject  \n");
		return iReturn;
	}


	/**正常リターンする。 */
	DEBUG_OutPut("IDMan_userAuthPKCS11ByIndex end \n");
	return iReturn;
}
