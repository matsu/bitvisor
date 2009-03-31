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
 * 公開鍵証明書を取得するプログラム
 * \file IDMan_IPgetCertificate.c
 */


/* 作成ライブラリ関数名 */
#include "IDMan_IPCommon.h"

#define  PKC_TYPE_SMALL_X						"x"
#define  PKC_TYPE_BIG_X							"X"
#define  PKC_TYPE_SMALL_Z						"z"
#define  PKC_TYPE_BIG_Z							"Z"

int IDMan_IPgCParamChk(  char* , char* );
int IDMan_IPgCIndexParamChk( char , int );


/**
* 公開鍵証明書の取得の内部関数でパラメータチェックを行う
* @author University of Tsukuba
* @param ptrSubjectDN  取得する公開鍵証明書のsubjectDN
* @param ptrIssuerDN  取得する公開鍵証明書のissuerDN
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgCParamChk (  char* ptrSubjectDN, char* ptrIssuerDN )
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

	/**正常リターンする。 */
	return RET_IPOK;

}

/**
* 公開鍵証明書の取得関数
* （引数で指定されたsubjectDNとissuuerDNに対応する公開鍵証明書をIDカードより取得する）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param subjectDN 取得する公開鍵証明書のsubjectDN
* @param issuerDN 取得する公開鍵証明書のissuerDN
* @param Cert 取得したX.509公開鍵証明書データ
* @param CertLen 取得したX.509公開鍵証明書データのレングス
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
int IDMan_getCertificate ( unsigned long int SessionHandle,
						   char* subjectDN,
						   char* issuerDN,
						   unsigned char* Cert,
						   unsigned short int* CertLen)
{

	long					lret;
	int						iret;
	int						iReturn;
	CK_RV					rv;
	int						MatchingFlg;
	CK_ATTRIBUTE		ObjAttribute[2];					//オブジェクトハンドル
	CK_ATTRIBUTE		Attribute;
	void*					ptrCert;
	CK_OBJECT_HANDLE		ObjectHandle[OBJECT_HANDLE_MAX];	
	CK_ULONG				ObjectHandleCnt;
	unsigned long int		LoopCnt;
	char					SubjectDN[SUBJECT_DN_MAX];
	char					IssuerDN[ISSUER_DN_MAX];
	long					class;
	unsigned char			token;

	DEBUG_OutPut("IDMan_getCertificate start\n");

	iReturn = RET_IPOK;
	MatchingFlg = 0;
	class=0;
	token=0;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPgCParamChk( subjectDN,issuerDN );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_getCertificate 入力パラメータのチェック エラー IDMan_IPgCParamChk\n");
		return iReturn;
	}

	/**公開鍵証明書の検索操作初期化を行う。 */
	/**−CKA_CLASSにCERTIFICATEを指定する。 */
	/**−CKA_TOKENにTRUEを指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_CERTIFICATE;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	ObjAttribute[1].type = CKA_TOKEN;
	token = CK_TRUE;
	ObjAttribute[1].pValue = &token;
	ObjAttribute[1].ulValueLen = sizeof(token);

	rv = C_FindObjectsInit(SessionHandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 2);
	/**公開鍵証明書の検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_getCertificate 公開鍵証明書の検索操作初期化 エラー C_FindObjectsInit\n");
		return iReturn;
	}

	/**公開鍵証明書の件数取得を行う。 */
	/**公開鍵証明書の件数取得エラーの場合、 */
	rv = C_FindObjects(SessionHandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	if (rv != CKR_OK)
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificate 公開鍵証明書の件数取得 エラー C_FindObjects\n");
		return iReturn;
	}
	/**取得した公開鍵証明書の件数を確認する。 */
	/**公開鍵証明書の件数が0件以下だった場合、 */
	if(ObjectHandleCnt <= 0)
	{
		/**−異常リターンする。 */
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificate 公開鍵証明書の件数 エラー 証明書の件数\n");
		return RET_IPNG;
	}

	/**取得した件数分処理を繰り返し引数のSubjectDN、IssuerDNに対応する公開鍵証明書を取得する。 */
	for(LoopCnt=0;LoopCnt < ObjectHandleCnt;LoopCnt++)
	{
		Attribute.type = CKA_VALUE;
		Attribute.pValue = 0x00;
		Attribute.ulValueLen = 0;

		/**−証明書本体のサイズ取得を行う。 */
		rv = C_GetAttributeValue(SessionHandle,ObjectHandle[LoopCnt],&Attribute,1);
		/**−実データ無しの場合、 */
		if (rv == CKR_TOKEN_NOT_RECOGNIZED) 
		{
			/**−−公開鍵証明書取得処理の先頭にもどる。 */
			continue;
		}
		/**−証明書本体のサイズ取得エラーの場合、 */
		if (rv != CKR_OK) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = IDMan_IPRetJudgment(rv);
			DEBUG_OutPut("IDMan_getCertificate 証明書本体のサイズ取得 エラー C_GetAttributeValue\n");
			break;
		}

		/**−証明書本体の領域を確保する。 */
		ptrCert = IDMan_StMalloc(Attribute.ulValueLen);
		/**−証明書本体の領域確保エラーの場合、 */
		if (ptrCert == 0x00) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			DEBUG_OutPut("IDMan_getCertificate 証明書本体の領域確保 エラー IDMan_StMalloc\n");
			break;
		}
		memset(ptrCert,0x00,Attribute.ulValueLen);

		Attribute.type = CKA_VALUE;
		Attribute.pValue = ptrCert;

		/**−証明書本体の取得を行う。 */
		rv = C_GetAttributeValue(SessionHandle,ObjectHandle[LoopCnt],&Attribute,1);
		/**−証明書本体の取得エラーの場合、 */
		if (rv != CKR_OK) 
		{
			/**−−証明書本体領域の解放する。 */
			IDMan_StFree (ptrCert);
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = IDMan_IPRetJudgment(rv);
			DEBUG_OutPut("IDMan_getCertificate 証明書本体の取得 エラー C_GetAttributeValue\n");
			break;
		}

		/**−証明書よりSubjectDNとIssuerDNを取得する。 */
		memset(SubjectDN,0x00, sizeof(SubjectDN));
		memset(IssuerDN,0x00, sizeof(IssuerDN));
		lret = IDMan_CmGetDN(ptrCert,Attribute.ulValueLen,SubjectDN,IssuerDN);
		/**−証明書のSubjectDNとIssuerDN取得エラーの場合、 */
		if (lret != RET_IPOK) 
		{
			/**−−証明書本体領域の解放する。 */
			IDMan_StFree (ptrCert);
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			DEBUG_OutPut("IDMan_getCertificate 証明書のSubjectDNとIssuerDN取得 エラー IDMan_CmGetDN\n");
			break;
		}

		/**−引数のsubjectDN・issuerDNと証明書より取得したSubjectDN・IssuerDNを比較する。 */
		/**−引数のsubjectDN・issuerDNと証明書より取得したSubjectDN・IssuerDNが同じ場合、 */
		if(strcmp(subjectDN , SubjectDN) == 0 && strcmp(issuerDN , IssuerDN) == 0)
		{
			MatchingFlg = 1;
			/**−−引数に公開鍵証明書データとレングス値を設定する。 */
			memcpy(Cert,ptrCert,Attribute.ulValueLen);
			(*CertLen) = Attribute.ulValueLen;
			/**−−証明書本体領域を解放する。 */
			IDMan_StFree (ptrCert);
			/**−−公開鍵証明書取得処理を終了する。 */
			break;
		}

		/**−証明書本体領域を解放する。 */
		IDMan_StFree (ptrCert);
		ptrCert=0x00;
	}

	/**証明書の検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( SessionHandle );
	/**証明書の検索操作の終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常を設定する。 */
		if(iReturn == RET_IPOK)
		{
			iReturn = IDMan_IPRetJudgment(rv);
		}
		DEBUG_OutPut("IDMan_getCertificate 証明書の検索操作の終了 エラー C_FindObjectsFinal\n");
	}

	/**返り値とマッチング検索結果（指定されたsubjectDN・issuerDNが同様の証明書があるか）の確認を行う。 */
	/**返り値が正常で引数で指定されたsubjectDN・issuerDNが同様の証明書がない場合、 */
	if(iReturn == RET_IPOK && MatchingFlg != 1)
	{
		/**−返り値に異常を設定する。 */
		iReturn = RET_IPNG_NO_MATCHING;
	}

	DEBUG_OutPut("IDMan_getCertificate end\n");

	/**返り値をリターンする。 */
	return iReturn;

}

/**
* 公開鍵証明書の取得Index指定版の内部関数でパラメータチェックを行う
* @author University of Tsukuba
* @param strPkcType 取得する公開鍵証明書のタイプ（x,z）
* @param iPkcIndex 取得する公開鍵証明書のインデックス（1,2,3）
* @return int 0:正常  -7:該当データなし
* @since 2008.02
* @version 1.0
*/
int IDMan_IPgCIndexParamChk (  char strPkcType, int iPkcIndex )
{
	char				buff[1024];

	memset(buff,0x00, sizeof(buff));

	/**パラメータチェックを行う。 */
	/**引数strPkcTypeのチェックを行う。 */
	/**引数のstrPkcTypeが"x","X","z","Z"以外の場合、 */
	if(strncmp(&strPkcType , PKC_TYPE_SMALL_X,1) != 0 && strncmp(&strPkcType, PKC_TYPE_BIG_X,1) != 0 && 
	   strncmp(&strPkcType , PKC_TYPE_SMALL_Z,1) != 0 && strncmp(&strPkcType, PKC_TYPE_BIG_Z,1) != 0 )
	{
		/**−異常リターンする。 */
		return RET_IPNG_NO_MATCHING;
	}

	/**引数iPkcIndexのチェックを行う。 */
	/**引数のiPkcIndexが0以下の場合、 */
	if(iPkcIndex <= 0)
	{
		/**−異常リターンする。 */
		return RET_IPNG_NO_MATCHING;
	}

	/**正常リターンする。 */
	return RET_IPOK;

}

/**
* 公開鍵証明書の取得関数Index指定版
* （引数で指定されたPkcTypeとPkcIndexに対応する公開鍵証明書をICカードより取得する）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param PkcType 取得する公開鍵証明書のタイプ（x,z）
* @param PkcIndex 取得する公開鍵証明書のインデックス（1,2,3）
* @param Cert 取得したX.509公開鍵証明書データ
* @param CertLen 取得したX.509公開鍵証明書データのレングス
* @return int 0:正常 
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -7:該当データなし（指定されたPkcTypeとPkcIndexに対応する証明書はありません）
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_getCertificateByIndex ( unsigned long int SessionHandle,
								  char PkcType,
								  int PkcIndex,
								  unsigned char* Cert,
								  unsigned short int* CertLen)
{

	/*long					lret;*/
	int						iret;
	int						iReturn;
	CK_RV					rv;
	CK_ATTRIBUTE		ObjAttribute[32];					//オブジェクトハンドル
	CK_ATTRIBUTE		Attribute;
	void*					ptrCert;
	CK_OBJECT_HANDLE		ObjectHandle[OBJECT_HANDLE_MAX];	
	CK_ULONG				ObjectHandleCnt;
	/*unsigned long int		LoopCnt;*/
	/*char					SubjectDN[SUBJECT_DN_MAX];*/
	/*char					IssuerDN[ISSUER_DN_MAX];*/
	long					class;
	unsigned char			token;
	char					Label[1024];
	unsigned long int		lindex;

	DEBUG_OutPut("IDMan_getCertificateByIndex start\n");

	iReturn = RET_IPOK;
	class=0;
	token=0;

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_IPgCIndexParamChk( PkcType,PkcIndex );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_getCertificateByIndex 入力パラメータのチェック エラー IDMan_IPgCIndexParamChk\n");
		return iReturn;
	}

	/**公開鍵証明書の検索操作初期化を行う。 */
	/**−CKA_CLASSにCERTIFICATEを指定する。 */
	/**−CKA_TOKENにTRUEを指定する。 */
	/**−CKA_INDEXにパラメータPkcIndexを指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_CERTIFICATE;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	ObjAttribute[1].type = CKA_TOKEN;
	token = CK_TRUE;
	ObjAttribute[1].pValue = &token;
	ObjAttribute[1].ulValueLen = sizeof(token);

	ObjAttribute[2].type = CKA_INDEX;
	lindex =  (unsigned long int)PkcIndex;
	ObjAttribute[2].pValue = &lindex;
	ObjAttribute[2].ulValueLen = sizeof(lindex);

	/**公開鍵証明書のタイプがPKCXの場合、 */
	memset(Label,0x00, sizeof(Label));
	ObjAttribute[3].type = CKA_LABEL;
	if( strncmp(&PkcType, PKC_TYPE_SMALL_X,1) == 0 || 
		strncmp(&PkcType, PKC_TYPE_BIG_X,1) == 0 )
	{
		/**−CKA_LABELにPublicCertificateを指定する。 */
		strcpy(Label,CK_LABEL_PUBLIC_CERT);
	}
	/**公開鍵証明書のタイプがPKCZの場合、 */
	else
	{
		/**−CKA_LABELにTrustCertificateを指定する。 */
		strcpy(Label,CK_LABEL_TRUST_CERT);
	}
	ObjAttribute[3].pValue = &Label;
	ObjAttribute[3].ulValueLen = strlen(Label);


	rv = C_FindObjectsInit(SessionHandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 4);
	/**公開鍵証明書の検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_getCertificateByIndex 公開鍵証明書の検索操作初期化 エラー C_FindObjectsInit\n");
		return iReturn;
	}

	/**公開鍵証明書の件数取得を行う。 */
	/**公開鍵証明書の件数取得エラーの場合、 */
	rv = C_FindObjects(SessionHandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	if (rv != CKR_OK)
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificateByIndex 公開鍵証明書の件数取得 エラー C_FindObjects\n");
		return iReturn;
	}
	/**取得した公開鍵証明書の件数を確認する。 */
	/**公開鍵証明書の件数が0件だった場合、 */
	if(ObjectHandleCnt == 0)
	{
		/**−異常リターンする。 */
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificateByIndex 公開鍵証明書の件数 エラー 証明書の件数\n");
		return RET_IPNG_NO_MATCHING;
	}

	Attribute.type = CKA_VALUE;
	Attribute.pValue = 0x00;
	Attribute.ulValueLen = 0;

	/**証明書本体のサイズ取得を行う。 */
	rv = C_GetAttributeValue(SessionHandle,ObjectHandle[0],&Attribute,1);
	/**実データ無しの場合、 */
	if (rv == CKR_TOKEN_NOT_RECOGNIZED) 
	{
		/**−異常リターンする。 */
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificateByIndex 証明書本体のサイズ取得 実データ無し C_GetAttributeValue\n");
		return RET_IPNG_NO_MATCHING;
	}
	/**証明書本体のサイズ取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificateByIndex 証明書本体のサイズ取得エラー C_GetAttributeValue\n");
		return iReturn;
	}

	/**証明書本体の領域を確保する。 */
	ptrCert = IDMan_StMalloc(Attribute.ulValueLen);
	/**証明書本体の領域確保エラーの場合、 */
	if (ptrCert == 0x00) 
	{
		/**−異常リターンする。 */
		iReturn = RET_IPNG;
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificateByIndex  証明書本体の領域確保 エラー IDMan_StMalloc\n");
		return iReturn;
	}
	memset(ptrCert,0x00,Attribute.ulValueLen);

	Attribute.type = CKA_VALUE;
	Attribute.pValue = ptrCert;

	/**証明書本体の取得を行う。 */
	rv = C_GetAttributeValue(SessionHandle,ObjectHandle[0],&Attribute,1);
	/**証明書本体の取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−証明書本体領域の解放する。 */
		IDMan_StFree (ptrCert);
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getCertificateByIndex  証明書本体の取得 エラー C_GetAttributeValue\n");
		return iReturn;
	}

	/**引数に公開鍵証明書データとレングス値を設定する。 */
	memcpy(Cert,ptrCert,Attribute.ulValueLen);
	(*CertLen) = Attribute.ulValueLen;

	/**証明書本体領域を解放する。 */
	IDMan_StFree (ptrCert);
	ptrCert=0x00;

	/**証明書の検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( SessionHandle );
	/**証明書の検索操作の終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_getCertificateByIndex  証明書の検索操作の終了 エラー C_FindObjectsFinal\n");
		return iReturn;
	}

	DEBUG_OutPut("IDMan_getCertificateByIndex  end\n");

	/**返り値をリターンする。 */
	return iReturn;

}
