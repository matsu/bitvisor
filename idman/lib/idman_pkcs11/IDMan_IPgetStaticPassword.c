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
 * ID／固定パスワード取得プログラム
 * \file IDMan_IPgetStaticPassword.c
 */

/* 作成ライブラリ関数名 */
#include "IDMan_IPCommon.h"

/**
* ID／固定パスワード取得関数
* （PINを用いてICカードに接続し、ICカードに格納されている全てのID／固定パスワードのペアを取得する。）
* @author University of Tsukuba
* @param SessionHandle  セッションハンドル
* @param list 取得したＩＤ／固定パスワードのリスト
* @return int 0:正常終了
*            -1:不正ICカードが挿入されている場合
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -4:PIN番号が間違っている場合
*            -5:PINがロックされている場合
*            -6:異常
*            -9:初期化処理未実施
* @since 2008.02
* @version 1.0
*/
int IDMan_getStaticPassword( unsigned long int SessionHandle,
							 idPasswordList** list )
{
	int						iret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	idPasswordList *		pidPassList;						//リストポインタ
	idPasswordList *		pidPassFreeList;					//リストポインタ
	idPasswordList *		pidPassNextList;					//リストポインタ
	unsigned long int 		LoopCnt;
	CK_OBJECT_HANDLE		ObjectHandle[OBJECT_HANDLE_MAX];	//オブジェクトハンドル
	CK_ULONG				ObjectHandleCnt;					//オブジェクトハンドル件数
	CK_ATTRIBUTE		ObjAttribute[3];					//
	CK_ATTRIBUTE		GetAttributeval[1024];				//
	unsigned long int 		pIdLen;
	unsigned long int 		pPassLen;
	char * 					pId;
	unsigned char * 		pPass;
	unsigned long int 		class;								//
	char					Label[256];

	DEBUG_OutPut("IDMan_getStaticPassword start\n");

	class=0;
	iReturn = RET_IPOK;
	pidPassFreeList = 0x00;
	(*list) = 0x00;

	/**ID/パスワードの検索操作初期化を行う。 */
	/**−CKA_CLASSにCKO_DATAを指定する。 */
	/**−CKA_LABELにCK_LABEL_ID_PASSを指定する。 */
	ObjAttribute[0].type = CKA_CLASS;
	class = CKO_DATA;
	ObjAttribute[0].pValue = &class;
	ObjAttribute[0].ulValueLen = sizeof(class);

	memset(Label,0x00, sizeof(Label));
	ObjAttribute[1].type = CKA_LABEL;
	strcpy(Label,CK_LABEL_ID_PASS);
	ObjAttribute[1].pValue = &Label;
	ObjAttribute[1].ulValueLen = strlen(Label);

	rv = C_FindObjectsInit(SessionHandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 2);
	/**ID/パスワードの検索操作初期化エラーの場合は、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		DEBUG_OutPut("IDMan_getStaticPassword ID/パスワードの検索操作初期化 エラー C_FindObjectsInit\n");
		return iReturn;
	}

	/**ID/パスワードの件数取得を行う。 */
	rv = C_FindObjects(SessionHandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	/**ID/パスワードの件数取得エラーの場合、 */
	if (rv != CKR_OK)
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getStaticPassword ID/パスワードの件数取得 エラー C_FindObjects\n");
		return iReturn;
	}

	/**取得した件数分処理を繰り返しID/パスワードのサイズを取得する。 */
	for(LoopCnt=0;LoopCnt < ObjectHandleCnt*2;LoopCnt+=2)
	{
		GetAttributeval[LoopCnt].type = CKA_VALUE_ID;
		GetAttributeval[LoopCnt].pValue = 0x00;
		GetAttributeval[LoopCnt].ulValueLen = 0;

		GetAttributeval[LoopCnt+1].type = CKA_VALUE_PASSWORD;
		GetAttributeval[LoopCnt+1].pValue = 0x00;
		GetAttributeval[LoopCnt+1].ulValueLen = 0;
	}
	/**ID/パスワードのサイズ取得を行う。 */
	rv = C_GetAttributeValue(SessionHandle,ObjectHandle[0],(CK_ATTRIBUTE_PTR)&GetAttributeval,ObjectHandleCnt*2);
	/**ID/パスワードのサイズ取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常を設定して取得処理を終了する。 */
		iReturn = IDMan_IPRetJudgment(rv);
		C_FindObjectsFinal( SessionHandle );
		DEBUG_OutPut("IDMan_getStaticPassword ID/パスワードのサイズ取得 エラー C_GetAttributeValue\n");
		return iReturn;
	}

	/**取得したID/パスワードを保存するリスト作成を行う。 */
	/**取得した件数分処理を繰り返しID/パスワード保存リストを作成する。 */
	for(LoopCnt=0;LoopCnt < ObjectHandleCnt*2;LoopCnt+=2)
	{
		pIdLen = GetAttributeval[LoopCnt].ulValueLen;
		pPassLen = GetAttributeval[LoopCnt+1].ulValueLen;

		/**−1件か確認する。 */
		/**−1件目の場合、 */
		if(LoopCnt == 0)
		{
			/**−−ID/パスワードを保存するリストの領域を確保する。 */
			iret = IDMan_IPIdPassListMalloc( &pidPassList,pIdLen+1,pPassLen);
			/**−−ID/パスワードを保存するリストの領域確保エラーの場合、 */
			if (iret != RET_IPOK) 
			{
				/**−−−返り値に異常を設定してリスト作成処理を終了する。 */
				iReturn = RET_IPNG;
				DEBUG_OutPut("IDMan_getStaticPassword ID/パスワードを保存するリストの領域確保1件目 エラー IDMan_IPIdPassListMalloc\n");
				break;
			}
			(*list) = pidPassList;
			pidPassFreeList = pidPassList;
		}
		/**−1件目でない場合、 */
		else
		{
			/**−−ID/パスワードを保存するリストの領域を確保する。 */
			iret = IDMan_IPIdPassListMalloc( &pidPassNextList, pIdLen+1,pPassLen );
			/**−−ID/パスワードを保存するリストの領域確保エラーの場合、 */
			if (iret != RET_IPOK) 
			{
				/**−−−返り値に異常を設定してリスト作成処理を終了する。 */
				iReturn = RET_IPNG;
				DEBUG_OutPut("IDMan_getStaticPassword ID/パスワードを保存するリストの領域確保1件目 エラー IDMan_IPIdPassListMalloc\n");
				break;
			}
			/**−−ID/パスワードを保存するリストの領域を以前確保してあったリスト領域につなげる。 */
			pidPassList->next = pidPassNextList;
			pidPassList = pidPassNextList;
		}

		pId = pidPassList->ID;
		GetAttributeval[LoopCnt].type = CKA_VALUE_ID;
		GetAttributeval[LoopCnt].pValue = pId;
		GetAttributeval[LoopCnt].ulValueLen = pIdLen;

		pPass = pidPassList->password;
		pidPassList->passwordLen = pPassLen;
		GetAttributeval[LoopCnt+1].type = CKA_VALUE_PASSWORD;
		GetAttributeval[LoopCnt+1].pValue = pPass;
		GetAttributeval[LoopCnt+1].ulValueLen = pPassLen;
	}
	/**領域確保エラーが発生していないか確認する。*/
	/**領域確保エラーが発生していない場合、 */
	if(iReturn == RET_IPOK)
	{
		/**−ID/パスワードの取得を行う。 */
		rv = C_GetAttributeValue(SessionHandle,ObjectHandle[0],(CK_ATTRIBUTE_PTR)&GetAttributeval,ObjectHandleCnt*2);
		/**−ID/パスワードの取得エラーの場合、 */
		if (rv != CKR_OK) 
		{
			/**−−返り値に異常を設定する。 */
			iReturn = IDMan_IPRetJudgment(rv);
			DEBUG_OutPut("IDMan_getStaticPassword ID/パスワードの取得 エラー C_GetAttributeValue\n");
		}
	}

	/**ID/パスワードの検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( SessionHandle );
	/**ID/パスワードの検索操作終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常を設定する。 */
		if(iReturn == RET_IPOK)
		{
			iReturn = IDMan_IPRetJudgment(rv);
		}
		DEBUG_OutPut("IDMan_getStaticPassword ID/パスワードの検索操作終了 エラー C_FindObjectsFinal\n");
	}

	/**返り値とリスト領域の確認を行う。 */
	/**返り値が異常でID/パスワードリスト領域が確保済みの場合、 */
	if(iReturn != RET_IPOK && pidPassFreeList != 0x00)
	{
		/**−ID/パスワードリスト領域を解放する。 */
		IDMan_getMemoryFree ( 0x00, (idPasswordList **) &pidPassFreeList );
		pidPassFreeList = 0x00;
	}

	DEBUG_OutPut("IDMan_getStaticPassword end\n");

	/**返り値をリターンする。 */
	return iReturn;

}
