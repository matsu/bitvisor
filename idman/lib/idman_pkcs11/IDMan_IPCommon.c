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
 * ID管理（PKCS）の共通機能プログラム
 * \file IDMan_IPCommon.c
 */


/* 作成ライブラリ関数名 */
#include "IDMan_IPCommon.h"


/**
* ID/パスワードを保存するリストの初期領域確保関数
* @author University of Tsukuba
* @param list ID/パスワードを保存するリストのアドレス
* @param IDLen ID領域サイズ
* @param passwordLen password領域サイズ
* @return int 0:正常 
*            -6:異常
* @since 2008.02
* @version 1.0
*/
int IDMan_IPIdPassListMalloc( idPasswordList** list ,unsigned long int IDLen,unsigned long int passwordLen)
{

	idPasswordList *		pidPasswordList;	//リストポインタ

	/**ID/パスワードを保存するリストの領域を確保する。 */
	pidPasswordList =(idPasswordList*) IDMan_StMalloc(sizeof(idPasswordList));
	/**ID/パスワードを保存するリストの領域確保エラーの場合、 */
	if (pidPasswordList == 0x00) 
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}
	memset(pidPasswordList,0x00,sizeof(idPasswordList));

	/**ID領域を確保する。 */
	pidPasswordList->ID =(char*) IDMan_StMalloc(IDLen);
	/**ID領域確保エラーの場合、 */
	if (pidPasswordList->ID == 0x00) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (pidPasswordList);
		return RET_IPNG;
	}
	memset(pidPasswordList->ID,0x00,IDLen);

	/**password領域を確保する。 */
	pidPasswordList->password =(unsigned char*) IDMan_StMalloc(passwordLen);
	/**password領域確保エラーの場合、 */
	if (pidPasswordList->password == 0x00) 
	{
		/**−異常リターンする。 */
		IDMan_StFree (pidPasswordList->ID);
		IDMan_StFree (pidPasswordList);
		return RET_IPNG;
	}
	memset(pidPasswordList->password,0x00,passwordLen);

	/**passwordLenに引数のpasswordLenを設定する。 */
	pidPasswordList->passwordLen = passwordLen;
	/**Nextポインタに0x00を設定する。 */
	pidPasswordList->next =0x00;
	(*list) = pidPasswordList;

	/**正常リターンする。 */
	return RET_IPOK;

}

/**
* ICカードリーダ接続初期処理関数
* @author University of Tsukuba
* @return int 0:正常 
*            -6:異常
*            -8:初期化済みの場合
*           -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPInitializeReader(void)
{
	CK_RV				rv;					//戻り値
	int					ret;				

	DEBUG_OutPut("IDMan_IPInitializeReader start\n");

	/**PKCS#11ライブラリ初期化する。 */
	rv = C_Initialize(0x00);
	/**PKCS#11ライブラリ初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPInitializeReader PKCS#11ライブラリ初期化 エラー C_Initialize\n");
		ret = IDMan_IPRetJudgment(rv);
		if (ret != RET_IPNG_INITIALIZE_REPEAT)
		{
			return RET_IPNG_NO_READER;
		}
		return ret;
	}

	/**正常リターンする。 */
	DEBUG_OutPut("IDMan_IPInitializeReader end\n");
	return RET_IPOK;

}

/**
* ICカード接続初期処理関数
* @author University of Tsukuba
* @param PIN ICカードPIN情報
* @param SessionHandle セッションハンドル
* @return int 0:正常 -1:不正ICカードが挿入されている -2:ICカードリーダにICカードが挿入されてない -3:ICカードリーダが途中で差し替えられた -4:PIN番号が間違っている -5:PINがロックされている -6:異常 -9:初期化処理未実施 -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPInitialize( char* PIN,unsigned long int* SessionHandle)
{

	CK_RV				rv;					//戻り値
	CK_TOKEN_INFO		tokenInfo;			
	CK_ULONG			PINLen;				
	int					ret;				
	CK_BBOOL			tokenPresent;		
	CK_ULONG			lSlotCnt;			
	CK_SLOT_ID			slotID[12];			//スロットID

	DEBUG_OutPut("IDMan_IPInitialize start\n");

	tokenPresent = CK_TRUE;

	/**カード有りのスロット件数を取得する。 */
	rv = C_GetSlotList( tokenPresent,0x00,&lSlotCnt);
	/**カード有りのスロット件数を取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPInitialize カード有りのスロット件数を取得 エラー C_GetSlotList\n");
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	/**カード有りのスロットリストを取得する。 */
	rv = C_GetSlotList( tokenPresent,(CK_SLOT_ID_PTR)&slotID[0],&lSlotCnt);
	/**カード有りのスロットリスト取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPInitialize カード有りのスロットリストを取得 エラー C_GetSlotList\n");
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	/**トークン情報を取得する。 */
	rv = C_GetTokenInfo(slotID[0],&tokenInfo);
	/**トークン情報取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPInitialize トークン情報を取得 エラー C_GetTokenInfo\n");
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	/**セッションの確立を行う。 */
	rv = C_OpenSession(slotID[0],CKS_RW_USER_FUNCTIONS,0x00,0x00,(CK_SESSION_HANDLE_PTR)SessionHandle);
	/**セッション確立エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPInitialize セッションの確立 エラー C_OpenSession\n");
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	/**カードにログインを行う。 */
	PINLen = (CK_ULONG) strlen((char*)PIN);
	rv = C_Login((*SessionHandle),CKU_USER,(CK_UTF8CHAR_PTR)PIN,PINLen);
	/**カードログインエラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPInitialize カードログイン エラー C_Login\n");
		C_CloseSession((*SessionHandle));
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	/**正常リターンする。 */
	DEBUG_OutPut("IDMan_IPInitialize end\n");
	return RET_IPOK;

}

/**
* ICカード接続終了処理関数
* @author University of Tsukuba
* @param SessionHandle セッションハンドル
* @return int  0:正常
*             -6:異常
*             -9:初期化処理未実施
* @since 2008.02
* @version 1.0
*/
int IDMan_IPFinalize(unsigned long int SessionHandle)
{

	CK_RV				rv;
	int					ret;

	DEBUG_OutPut("IDMan_IPFinalize start\n");

	/**カードからログアウトを行う。 */
	rv = C_Logout(SessionHandle);
	/**カードログアウトエラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPFinalize カードログアウト エラー C_Logout\n");
		C_CloseSession(SessionHandle);
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	/**セッションを切断を行う。 */
	rv = C_CloseSession(SessionHandle);
	/**セッション切断エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPFinalize セッション切断 エラー C_CloseSession\n");
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	DEBUG_OutPut("IDMan_IPFinalize end\n");

	/**正常リターンする。 */
	return RET_IPOK;

}

/**
* ICカードリーダ接続終了処理関数
* @author University of Tsukuba
* @param SessionHandle セッションハンドル
* @return int 0:正常  -6:異常 -9:初期化処理未実施
* @since 2008.02
* @version 1.0
*/
int IDMan_IPFinalizeReader(void)
{

	CK_RV				rv;
	int					ret;

	DEBUG_OutPut("IDMan_IPFinalizeReader start\n");

	/**PKCS#11ライブラリを終了する。 */
	rv = C_Finalize((CK_VOID_PTR)0x00);
	/**PKCS#11ライブラリ終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		DEBUG_OutPut("IDMan_IPFinalizeReader PKCS#11ライブラリ終了 エラー C_Finalize\n");
		ret = IDMan_IPRetJudgment(rv);
		return ret;
	}

	DEBUG_OutPut("IDMan_IPFinalizeReader end\n");

	/**正常リターンする。 */
	return RET_IPOK;

}

/**
* 返り値判定処理関数
* @author University of Tsukuba
* @param prv PKCS11関数の返り値
* @return int 0:正常
*            -1:不正ICカードが挿入されている
*            -2:ICカードリーダにICカードが挿入されてない
*            -3:ICカードリーダが途中で差し替えられた
*            -4:PIN番号が間違っている
*            -5:PINがロックされている
*            -6:異常
*            -8:初期化済みの場合
*            -9:初期化処理未実施
*           -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPRetJudgment(CK_RV prv)
{

	int					ireturn;

	/**引数の値を確認して状態に対応する値を返り値に設定する。 */
	switch(prv)
	{
		/**−不正なICカードを検出した場合、 */
		case CKR_TOKEN_NOT_RECOGNIZED :
			/**−−不正ICカードが挿入されていることを示す値を返り値に設定する。 */
			ireturn = RET_IPNG_TOKEN_NOT_RECOGNIZED;	//-1:不正ICカードが挿入されている場合
			break;
		/**−カードが挿入されていない場合、 */
		case CKR_TOKEN_NOT_PRESENT :
			/**−−ICカードリーダにICカードが挿入されてないことを示す値を返り値に設定する。 */
			ireturn = RET_IPNG_TOKEN_NOT_PRESENT;		//-2:ICカードリーダにICカードが挿入されてない場合
			break;
		/**−カードが抜かれた場合、 */
		case CKR_DEVICE_REMOVED :
			/**−−ICカードリーダが途中で差し替えられたことを示す値を返り値に設定する。 */
			ireturn = RET_IPNG_DEVICE_REMOVED;			//-3:ICカードリーダが途中で差し替えられた場合
			break;
		/**−パスワード指定誤りの場合、 */
		case CKR_PIN_INCORRECT :
			/**−−PIN番号が間違っていることを示す値を返り値に設定する。 */
			ireturn = RET_IPNG_PIN_INCORREC;			//-4:PIN番号が間違っている場合
			break;
		/**−パスワードがロックされている場合、 */
		case CKR_PIN_LOCKED :
			/**−−PINがロックされていることを示す値を返り値に設定する。 */
			ireturn = RET_IPNG_PIN_LOCKED;				//-5:PINがロックされている場合
			break;
		/**−初期化済みの場合、 */
		case CKR_CRYPTOKI_ALREADY_INITIALIZED:
			ireturn = RET_IPNG_INITIALIZE_REPEAT;		//-8:初期化済みの場合
			break;
		/**−cryptoki未初期化の場合、 */
		case CKR_CRYPTOKI_NOT_INITIALIZED:
			ireturn = RET_IPNG_NO_INITIALIZE;			//-9:初期化処理未実施の場合
			break;
		/**−デバイスエラーの場合、 */
		case CKR_DEVICE_ERROR:
			ireturn = RET_IPNG_NO_READER;				//-10:リーダ接続エラーの場合
			break;
		/**−上記以外の場合、 */
		case CKR_SIGNATURE_INVALID :
		default  :
			/**−−異常を示す値を返り値に設定する。 */
			ireturn = RET_IPNG;							//-6:異常
			break;
	}

	/**返り値をリターンする。 */
	return ireturn;

}

/**
* 情報リストやX.509公開鍵証明書データの領域を解放する関数
* @author University of Tsukuba
* @param data データアドレス
* @param list 情報リスト
* @return void なし 
* @since 2008.02
* @version 1.0
*/
void IDMan_getMemoryFree (  unsigned char* data,  idPasswordList ** list )
{

	idPasswordList *			pList;
	idPasswordList *			pFreeList;

	DEBUG_OutPut("IDMan_getMemoryFree start\n");

	/**引数のデータアドレス領域に0x00が指定されているか確認する。 */
	/**引数のデータアドレス領域が0x00でない場合、 */
	if( data != 0x00 )
	{
		/**−引数のデータアドレス領域を解放する。 */
		IDMan_StFree (data);
	}

	/**引数の情報リスト領域に0x00が指定されているか確認する。 */
	/**引数の情報リスト領域が0x00でない場合、 */
	if( list != 0x00 )
	{
		pList = (*list);
		pFreeList = (*list);

		/**−引数の情報リスト領域が示すチェーン構造である全ての領域を解放する。 */
		while(1)
		{
			/**−−IDの領域を解放する。 */
			IDMan_StFree (pFreeList->ID);
			/**−−パスワードの領域を解放する。 */
			IDMan_StFree (pFreeList->password);
			/**−−次の情報リスト領域アドレスをワーク領域に確保する。 */
			pList = pFreeList->next;
			/**−−情報リスト領域を解放する。 */
			IDMan_StFree (pFreeList);
			/**−−次の情報リスト領域アドレスが存在するか確認する。 */
			/**−−次の情報リスト領域アドレスが存在しない場合、 */
			if(pList == 0x00)
			{
				/**−−−処理を終了する。 */
				break;
			}
			/**−−次の情報リスト領域アドレスを解放する領域アドレスに設定する。 */
			pFreeList = pList;
		}
	}

	DEBUG_OutPut("IDMan_getMemoryFree end\n");

	/**リターンする。 */
	return ;
}


/**
* パディング変換関数の内部関数でパラメータチェックを行う
* @author University of Tsukuba
* @param algorithm アルゴリズム 0x220：SHA1 0x250：SHA256 0x270：SHA512
* @param size パディングレングス
* @param data 対象データ
* @param len 対象データのレングス
* @return int 0:正常  -6:パラメータエラー 
* @since 2008.02
* @version 1.0
*/
int IDMan_PaddingParamChk ( int  algorithm,
					  unsigned long int size,
					  void * data,
					  unsigned long int len )
{

	/**パラメータチェックを行う。 */
	/**引数algorithmのチェックを行う。 */
	/**引数のalgorithmが対応するタイプ(SHA1,SHA256,SHA512)以外の場合、 */
	if(algorithm != ALGORITHM_SHA1 && algorithm != ALGORITHM_SHA256 && algorithm != ALGORITHM_SHA512  )
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数sizeのチェックを行う。 */
	/**引数のalgorithm値を確認してsizeのサイズを確認する。 */
	switch(algorithm)
	{
		/**−algorithmがSHA1の場合、 */
		case ALGORITHM_SHA1 :
			/**−−引数sizeのサイズ確認する。 */
			if(size < 2+1+1+ALGORITHM_IDLEN_SHA1+HASH_LEN_SHA1)
			{
				/**−−−引数sizeが論理矛盾サイズSHA1（2+1+1+15+20=39未満）の場合、 */
				/**−−−異常リターンする。 */
				return RET_IPNG;
			}
			break;
		/**−algorithmがSHA256の場合、 */
		case ALGORITHM_SHA256 :
			/**−−引数sizeのサイズ確認する。 */
			if(size < 2+1+1+ALGORITHM_IDLEN_SHA256+HASH_LEN_SHA256)
			{
				/**−−−引数sizeが論理矛盾サイズSHA256（2+1+1+19+32=55未満）の場合、 */
				/**−−−異常リターンする。 */
				return RET_IPNG;
			}
			break;
		/**−algorithmがSHA512の場合、 */
		case ALGORITHM_SHA512 :
			/**−−引数sizeのサイズ確認する。 */
			if(size < 2+1+1+ALGORITHM_IDLEN_SHA512+HASH_LEN_SHA512)
			{
				/**−−−引数sizeが論理矛盾サイズSHA512（2+1+1+19+64=87未満）の場合、 */
				/**−−−異常リターンする。 */
				return RET_IPNG;
			}
			break;
	}

	/**引数dataのチェックを行う。 */
	/**引数のdataが0x00ポインタの場合、 */
	if(data == 0x00)
	{
		/**−異常リターンする。 */
		return RET_IPNG;
	}

	/**引数lenのチェックを行う。 */
	/**引数のalgorithm値を確認してlenのサイズを確認する。 */
	switch(algorithm)
	{
		/**−algorithmがSHA1の場合、 */
		case ALGORITHM_SHA1 :
			/**−−引数lenのサイズ確認する。 */
			if(len != HASH_LEN_SHA1)
			{
				/**−−−引数lenがハッシュサイズ以外の場合、 */
				/**−−−異常リターンする。 */
				return RET_IPNG;
			}
			break;
		/**−algorithmがSHA256の場合、 */
		case ALGORITHM_SHA256 :
			/**−−引数lenのサイズ確認する。 */
			if(len != HASH_LEN_SHA256)
			{
				/**−−−引数lenがハッシュサイズ以外の場合、 */
				/**−−−異常リターンする。 */
				return RET_IPNG;
			}
			break;
		/**−algorithmがSHA512の場合、 */
		case ALGORITHM_SHA512 :
			/**−−引数lenのサイズ確認する。 */
			if(len != HASH_LEN_SHA512)
			{
				/**−−−引数lenがハッシュサイズ以外の場合、 */
				/**−−−異常リターンする。 */
				return RET_IPNG;
			}
			break;
	}

	/**正常リターンする。 */
	return RET_IPOK;

}


/**
*パディング変換関数
* @author University of Tsukuba
* @param algorithm アルゴリズム 0x220：SHA1 0x250：SHA256 0x270：SHA512
* @param size パディングレングス
* @param data 対象データ
* @param len 対象データのレングス
* @param PaddingData パディング後データ
* @return   long    0：正常 -6：異常
* @since 2008.02
* @version 1.0
*/
int IDMan_CmPadding( int  algorithm,
					  unsigned long int size,
					  void * data,
					  unsigned long int len,
					  void * PaddingData  )
{
	int					iret=0;
	int					iReturn=RET_IPOK;
	char				HashAlgorithmIdData[128];	//ハッシュアルゴリズムID
	unsigned long int 	HashAlgorithmIdDataLen=0;	//ハッシュアルゴリズムIDレングス
	long				lBytePosition=0;			//バイト位置

	DEBUG_OutPut("IDMan_CmPadding start\n");

	memset(HashAlgorithmIdData,0x00,sizeof(HashAlgorithmIdData));

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_PaddingParamChk( algorithm, size,  data,  len );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_CmPadding 入力パラメータのチェック エラー IDMan_PaddingParamChk\n");
		return iReturn;
	}

	/**ハッシュアルゴリズムIDを決定する。 */
	/**引数のalgorithm値を確認してハッシュアルゴリズムIDとレングスを取得する。 */
	switch(algorithm)
	{
		/**−algorithmがSHA1の場合、 */
		case ALGORITHM_SHA1 :
			/**−−SHA1のハッシュアルゴリズムIDとレングスを設定する。 */
			memcpy(HashAlgorithmIdData,ALGORITHM_ID_SHA1,ALGORITHM_IDLEN_SHA1);
			HashAlgorithmIdDataLen=ALGORITHM_IDLEN_SHA1;
			break;
		/**−algorithmがSHA256の場合、 */
		case ALGORITHM_SHA256 :
			/**−−SHA256のハッシュアルゴリズムIDとレングスを設定する。 */
			memcpy(HashAlgorithmIdData,ALGORITHM_ID_SHA256,ALGORITHM_IDLEN_SHA256);
			HashAlgorithmIdDataLen=ALGORITHM_IDLEN_SHA256;
			break;
		/**−algorithmがSHA512の場合、 */
		case ALGORITHM_SHA512 :
			/**−−SHA512のハッシュアルゴリズムIDとレングスを設定する。 */
			memcpy(HashAlgorithmIdData,ALGORITHM_ID_SHA512,ALGORITHM_IDLEN_SHA512);
			HashAlgorithmIdDataLen=ALGORITHM_IDLEN_SHA512;
			break;
	}


	/**パディング後データの先頭から0x00 0x01を設定する。 */
	memcpy(PaddingData,"\x00\x01",2);
	lBytePosition=2;

	/**パディング後データの次バイト位置よりパディングレングス−（1+ハッシュアルゴリズムIDレングス＋対象データのレングスサイズ）の計算結果バイトサイズ分、0xffを設定する。 */
	memset(PaddingData+lBytePosition,0xff,size-(2+1+HashAlgorithmIdDataLen+len));
	lBytePosition+=size-(2+1+HashAlgorithmIdDataLen+len);

	/**パディング後データの次バイト位置より0x00を設定する */
	memcpy(PaddingData+lBytePosition,"\x00",1);
	lBytePosition+=1;

	/**パディング後データの次バイト位置よりアッシュアルゴリズムIDを設定する */
	memcpy(PaddingData+lBytePosition,HashAlgorithmIdData,HashAlgorithmIdDataLen);
	lBytePosition+=HashAlgorithmIdDataLen;

	/**パディング後データの次バイト位置より署名データを設定する */
	memcpy(PaddingData+lBytePosition,data,len);
	lBytePosition+=len;

	DEBUG_OutPut("IDMan_CmPadding end\n");

	return iReturn;

}
/**
*パディング変換関数 for IPsec
* @author University of Tsukuba
* @param algorithm アルゴリズム 0x220：SHA1 0x250：SHA256 0x270：SHA512
* @param size パディングレングス
* @param data 対象データ
* @param len 対象データのレングス
* @param PaddingData パディング後データ
* @return   long    0：正常 -6：異常
* @since 2008.02
* @version 1.0
*/
int IDMan_CmPadding2( int  algorithm,
					  unsigned long int size,
					  void * data,
					  unsigned long int len,
					  void * PaddingData  )
{
	int					iret=0;
	int					iReturn=RET_IPOK;
	char				HashAlgorithmIdData[128];	//ハッシュアルゴリズムID
	unsigned long int 	HashAlgorithmIdDataLen=0;	//ハッシュアルゴリズムIDレングス
	long				lBytePosition=0;			//バイト位置

	DEBUG_OutPut("IDMan_CmPadding start\n");

	memset(HashAlgorithmIdData,0x00,sizeof(HashAlgorithmIdData));

	/**入力パラメータのチェックを行う。 */
	iret = IDMan_PaddingParamChk( algorithm, size,  data,  len );
	/**入力パラメータのチェックエラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−異常リターンする。 */
		iReturn = iret;
		DEBUG_OutPut("IDMan_CmPadding 入力パラメータのチェック エラー IDMan_PaddingParamChk\n");
		return iReturn;
	}

	/**ハッシュアルゴリズムIDを決定する。 */
	/**引数のalgorithm値を確認してハッシュアルゴリズムIDとレングスを取得する。 */
	switch(algorithm)
	{
		/**−algorithmがSHA1の場合、 */
		case ALGORITHM_SHA1 :
			/**−−SHA1のハッシュアルゴリズムIDとレングスを設定する。 */
			memcpy(HashAlgorithmIdData,ALGORITHM_ID_SHA1,ALGORITHM_IDLEN_SHA1);
			HashAlgorithmIdDataLen=ALGORITHM_IDLEN_SHA1;
			break;
		/**−algorithmがSHA256の場合、 */
		case ALGORITHM_SHA256 :
			/**−−SHA256のハッシュアルゴリズムIDとレングスを設定する。 */
			memcpy(HashAlgorithmIdData,ALGORITHM_ID_SHA256,ALGORITHM_IDLEN_SHA256);
			HashAlgorithmIdDataLen=ALGORITHM_IDLEN_SHA256;
			break;
		/**−algorithmがSHA512の場合、 */
		case ALGORITHM_SHA512 :
			/**−−SHA512のハッシュアルゴリズムIDとレングスを設定する。 */
			memcpy(HashAlgorithmIdData,ALGORITHM_ID_SHA512,ALGORITHM_IDLEN_SHA512);
			HashAlgorithmIdDataLen=ALGORITHM_IDLEN_SHA512;
			break;
	}


	/**パディング後データの先頭から0x00 0x01を設定する。 */
	memcpy(PaddingData,"\x00\x01",2);
	lBytePosition=2;

	/**パディング後データの次バイト位置よりパディングレングス−（1+ハッシュアルゴリズムIDレングス＋対象データのレングスサイズ）の計算結果バイトサイズ分、0xffを設定する。 */
	memset(PaddingData+lBytePosition,0xff,size-(2+1+len));
	lBytePosition+=size-(2+1+len);

	/**パディング後データの次バイト位置より0x00を設定する */
	memcpy(PaddingData+lBytePosition,"\x00",1);
	lBytePosition+=1;

	/**パディング後データの次バイト位置より署名データを設定する */
	memcpy(PaddingData+lBytePosition,data,len);
	lBytePosition+=len;

	DEBUG_OutPut("IDMan_CmPadding end\n");

	return iReturn;

}
/**
* 公開鍵証明書PKC(x)またはPKC(ｚ)と鍵ペアID取得関数
* （SubjectDN、IssuerDNに対応する公開鍵証明書PKC(z)または鍵ペアIDを取得する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param subjectDN 鍵ペアに対する公開鍵証明書のsubjectDN
* @param issuerDN 鍵ペアに対する公開鍵証明書のissuerDN
* @param ptrCert 証明書確保領域ポインタ
* @param lCert 証明書レングス
* @param ptrKeyID 鍵ペアID確保領域ポインタ
* @param lKeyID 鍵ペアIDレングス
* @param getCert 証明書取得フラグ 0:PKC(x) 1:PKC(ｚ)
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
int IDMan_IPgetCertPKCS( CK_SESSION_HANDLE Sessinhandle,
						 char* subjectDN,
						 char* issuerDN,
						 void** ptrCert,
						 long* lCert,
						 void** ptrKeyID,
						 long* lKeyID,
						 int getCert)
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
	char					Label[256];

	class=0;
	token=0;
	iReturn = RET_IPOK;
	MatchingFlg = 0;
	(*ptrCert) = 0x00;
	(*ptrKeyID) = 0x00;

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

	/**証明書取得フラグを確認する。 */
	if(getCert == 0)
	{
		/**−証明書取得フラグがPKC(x)の場合、CKA_LABELにPublicCertificateを指定する。 */
		memset(Label,0x00, sizeof(Label));
		ObjAttribute[2].type = CKA_LABEL;
		strcpy(Label,CK_LABEL_PUBLIC_CERT);
		ObjAttribute[2].pValue = &Label;
		ObjAttribute[2].ulValueLen = strlen(Label);
	}
	else
	{
		/**−証明書取得フラグがPKC(z)の場合、CKA_LABELにTrustCertificateを指定する。 */
		memset(Label,0x00, sizeof(Label));
		ObjAttribute[2].type = CKA_LABEL;
		strcpy(Label,CK_LABEL_TRUST_CERT);
		ObjAttribute[2].pValue = &Label;
		ObjAttribute[2].ulValueLen = strlen(Label);
	}

	rv = C_FindObjectsInit(Sessinhandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 3);
	/**公開鍵証明書の検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**公開鍵証明書の件数取得を行う。 */
	rv = C_FindObjects(Sessinhandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	/**公開鍵証明書の件数取得エラーの場合、 */
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

	/**取得した件数分処理を繰り返し引数のSubjectDN、IssuerDNに対応する公開鍵証明書PKCを取得する。 */
	for(LoopCnt=0;LoopCnt < ObjectHandleCnt;LoopCnt++)
	{
		Attributeval[0].type = CKA_VALUE;
		Attributeval[0].pValue = 0x00;
		Attributeval[0].ulValueLen = 0;

		/**−証明書取得フラグを確認する。 */
		if(getCert == 0)
		{
			/**−−証明書取得フラグがPKC(x)の場合、証明書と鍵ペアIDを取得する。 */
			Attributeval[1].type = CKA_ID;
			Attributeval[1].pValue = 0x00;
			Attributeval[1].ulValueLen = 0;

			/**−−証明書本体と鍵ペアIDのサイズ取得を行う。 */
			rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[LoopCnt],(CK_ATTRIBUTE_PTR)&Attributeval,2);
			/**−−実データ無しの場合、 */
			if (rv == CKR_TOKEN_NOT_RECOGNIZED) 
			{
				/**−−−公開鍵証明書取得処理を終了する。 */
				break;
			}
			/**−−証明書本体と鍵ペアIDのサイズ取得エラーの場合、 */
			if (rv != CKR_OK) 
			{
				/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
				iReturn = IDMan_IPRetJudgment(rv);
				break;
			}
	
			/**−−証明書本体の領域を確保する。 */
			(*ptrCert) = IDMan_StMalloc(Attributeval[0].ulValueLen);
			/**−証明書本体の領域確保エラーの場合、 */
			if ((*ptrCert) == 0x00) 
			{
				/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
				iReturn = RET_IPNG;
				break;
			}
			memset((*ptrCert),0x00,Attributeval[0].ulValueLen);
	
			/**−−鍵ペアIDの領域を確保する。 */
			(*ptrKeyID) = IDMan_StMalloc(Attributeval[1].ulValueLen+1);
			/**−−鍵ペアIDの領域確保エラーの場合、 */
			if ((*ptrKeyID) == 0x00) 
			{
				/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
				/**−−−証明書本体領域の解放する。 */
				IDMan_StFree ((*ptrCert));
				iReturn = RET_IPNG;
				break;
			}
			memset((*ptrKeyID),0x00,Attributeval[1].ulValueLen+1);
	
			Attributeval[0].type = CKA_VALUE;
			Attributeval[0].pValue = (*ptrCert);
	
			Attributeval[1].type = CKA_ID;
			Attributeval[1].pValue = (*ptrKeyID);
	
			/**−−証明書本体と鍵ペアIDの取得を行う。 */
			rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[LoopCnt],(CK_ATTRIBUTE_PTR)&Attributeval,2);
			/**−−証明書本体と鍵ペアIDの取得エラーの場合、 */
			if (rv != CKR_OK) 
			{
				/**−−−証明書本体領域の解放する。 */
				IDMan_StFree ((*ptrCert));
				/**−−−鍵ペアID領域の解放する。 */
				IDMan_StFree ((*ptrKeyID));
				/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
				iReturn = IDMan_IPRetJudgment(rv);
				break;
			}
		}
		else
		{
			/**−−証明書取得フラグがPKC(z)の場合、証明書を取得する */
			/**−−証明書本体のサイズ取得を行う。 */
			rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[LoopCnt],(CK_ATTRIBUTE_PTR)&Attributeval,1);
			/**−−実データ無しの場合、 */
			if (rv == CKR_TOKEN_NOT_RECOGNIZED) 
			{
				/**−−−公開鍵証明書取得処理を終了する。 */
				break;
			}
			/**−−証明書本体のサイズ取得エラーの場合、 */
			if (rv != CKR_OK) 
			{
				/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
				iReturn = IDMan_IPRetJudgment(rv);
				break;
			}
	
			/**−−証明書本体の領域を確保する。 */
			(*ptrCert) = IDMan_StMalloc(Attributeval[0].ulValueLen);
			/**−証明書本体の領域確保エラーの場合、 */
			if ((*ptrCert) == 0x00) 
			{
				/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
				iReturn = RET_IPNG;
				break;
			}
			memset((*ptrCert),0x00,Attributeval[0].ulValueLen);
	
			Attributeval[0].type = CKA_VALUE;
			Attributeval[0].pValue = (*ptrCert);

			/**−−証明書本体の取得を行う。 */
			rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[LoopCnt],(CK_ATTRIBUTE_PTR)&Attributeval,1);
			/**−−証明書本体と鍵ペアIDの取得エラーの場合、 */
			if (rv != CKR_OK) 
			{
				/**−−−証明書本体領域の解放する。 */
				IDMan_StFree ((*ptrCert));
				/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
				iReturn = IDMan_IPRetJudgment(rv);
				break;
			}
		}

		/**−証明書よりSubjectDNとIssuerDNを取得する。 */
		memset(SubjectDN,0x00, sizeof(SubjectDN));
		memset(IssuerDN,0x00, sizeof(IssuerDN));
		lret = IDMan_CmGetDN((*ptrCert),Attributeval[0].ulValueLen,SubjectDN,IssuerDN);
		/**−証明書のSubjectDNとIssuerDN取得エラーの場合、 */
		if (lret != RET_IPOK) 
		{
			/**−−証明書本体領域の解放する。 */
			IDMan_StFree ((*ptrCert));
			/**−−証明書取得フラグを確認する。 */
			if(getCert == 0)
			{
				/**−−−証明書取得フラグがPKC(x)の場合、 */
				/**−−−鍵ペアID領域の解放する。 */
				IDMan_StFree ((*ptrKeyID));
			}
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			break;
		}

		/**−引数のsubjectDN・issuerDNと証明書より取得したSubjectDN・IssuerDNを比較する。 */
		/**−引数のsubjectDN・issuerDNと証明書より取得したSubjectDN・IssuerDNが同じ場合、 */
		if(strcmp(subjectDN , SubjectDN) == 0 && strcmp(issuerDN , IssuerDN) == 0)
		{
			MatchingFlg = 1;
			/**−−証明書データのレングス値を保持する。 */
			(*lCert) = Attributeval[0].ulValueLen;
			/**−−証明書取得フラグを確認する。 */
			if(getCert == 0)
			{
				/**−−−証明書取得フラグがPKC(x)の場合、 */
				/**−−−鍵ペアIDのレングス値を保持する。 */
				(*lKeyID) = Attributeval[1].ulValueLen;
			}
			/**−−公開鍵証明書取得処理を終了する。 */
			break;
		}

		/**−証明書本体領域を解放する。 */
		IDMan_StFree ((*ptrCert));
		(*ptrCert)=0x00;
		/**−証明書取得フラグを確認する。 */
		if(getCert == 0)
		{
			/**−−証明書取得フラグがPKC(x)の場合、 */
			/**−−鍵ペアID領域の解放する。 */
			IDMan_StFree ((*ptrKeyID));
			(*ptrKeyID)=0x00;
		}
	}

	/**証明書の検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( Sessinhandle );
	/**証明書の検索操作終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常を設定する。 */
		/**−証明書の検索操作終了以外が正常な場合、 */
		if(iReturn == RET_IPOK)
		{
			/**−−マッチング検索結果（指定されたsubjectDN・issuerDNが同様の証明書があるか）の確認を行う。 */
			/**−−指定されたsubjectDN・issuerDNが同様の証明書がある場合、 */
			if(MatchingFlg==1)
			{
				/**−−−証明書本体領域の解放する。 */
				IDMan_StFree ((*ptrCert));
				/**−−−証明書取得フラグを確認する。 */
				if(getCert == 0)
				{
					/**−−−−証明書取得フラグがPKC(x)の場合、 */
					/**−−−−鍵ペアID領域の解放する。 */
					IDMan_StFree ((*ptrKeyID));
				}
			}
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

	/**証明書取得フラグを確認する。 */
	if(getCert == 0)
	{
		/**−証明書取得フラグがPKC(x)の場合、 */
		/**−証明書領域の解放する。 */
		IDMan_StFree ((*ptrCert));
	}

	/**返り値をリターンする。 */
	return iReturn;
}

/**
* 公開鍵証明書PKC(x)またはPKC(ｚ)と鍵ペアID取得関数
* （iPkcIndexに対応する公開鍵証明書PKC(z)または鍵ペアIDを取得する）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param iPkcIndex  取得する公開鍵証明書のiPkcIndex
* @param ptrCert 証明書確保領域ポインタ
* @param lCert 証明書レングス
* @param ptrKeyID 鍵ペアID確保領域ポインタ
* @param lKeyID 鍵ペアIDレングス
* @param getCert 証明書取得フラグ 0:PKC(x) 1:PKC(ｚ)
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
int IDMan_IPgetCertIdxPKCS( CK_SESSION_HANDLE Sessinhandle,
						 int iPkcIndex,
						 void** ptrCert,
						 long* lCert,
						 void** ptrKeyID,
						 long* lKeyID,
						 int getCert)
{
	/*long					lret;*/								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	int						MatchingFlg;						//マッチングフラグ
	CK_ATTRIBUTE		ObjAttribute[4];					//
	CK_ATTRIBUTE		Attributeval[2];					//
	CK_OBJECT_HANDLE		ObjectHandle[OBJECT_HANDLE_MAX];	//オブジェクトハンドル
	CK_ULONG				ObjectHandleCnt;					//オブジェクトハンドル件数
	long					class;								//
	unsigned char			token;								//
	char					Label[256];
	unsigned long int		lindex;

	class=0;
	token=0;
	iReturn = RET_IPOK;
	MatchingFlg = 0;
	(*ptrCert) = 0x00;
	(*ptrKeyID) = 0x00;
	*lCert=0;
	*lKeyID = 0;

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

	/**証明書取得フラグを確認する。 */
	if(getCert == 0)
	{
		/**−証明書取得フラグがPKC(x)の場合、CKA_LABELにPublicCertificateを指定する。 */
		memset(Label,0x00, sizeof(Label));
		ObjAttribute[2].type = CKA_LABEL;
		strcpy(Label,CK_LABEL_PUBLIC_CERT);
		ObjAttribute[2].pValue = &Label;
		ObjAttribute[2].ulValueLen = strlen(Label);
	}
	else
	{
		/**−証明書取得フラグがPKC(z)の場合、CKA_LABELにTrustCertificateを指定する。 */
		memset(Label,0x00, sizeof(Label));
		ObjAttribute[2].type = CKA_LABEL;
		strcpy(Label,CK_LABEL_TRUST_CERT);
		ObjAttribute[2].pValue = &Label;
		ObjAttribute[2].ulValueLen = strlen(Label);
	}
	ObjAttribute[3].type = CKA_INDEX;
	lindex = (unsigned long int)iPkcIndex;
	ObjAttribute[3].pValue = &lindex;
	ObjAttribute[3].ulValueLen = sizeof(lindex);

	/**公開鍵証明書の検索操作初期化を行う。 */
	rv = C_FindObjectsInit(Sessinhandle,(CK_ATTRIBUTE_PTR)&ObjAttribute, 4);
	/**公開鍵証明書の検索操作初期化エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**公開鍵証明書の件数取得を行う。 */
	rv = C_FindObjects(Sessinhandle,(CK_OBJECT_HANDLE_PTR)&ObjectHandle,OBJECT_HANDLE_MAX,&ObjectHandleCnt);
	/**公開鍵証明書の件数取得エラーの場合、 */
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

	/**証明書取得フラグを確認する。 */
	if(getCert == 0)
	{
		/**−証明書取得フラグがPKC(x)の場合、鍵ペアIDを取得する。 */
		Attributeval[0].type = CKA_ID;
		Attributeval[0].pValue = 0x00;
		Attributeval[0].ulValueLen = 0;
	}
	else
	{
		/**−証明書取得フラグがPKC(z)の場合、証明書を取得する。 */
		Attributeval[0].type = CKA_VALUE;
		Attributeval[0].pValue = 0x00;
		Attributeval[0].ulValueLen = 0;
	}
	/**証明書本体か鍵ペアIDのサイズ取得を行う。 */
	rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[0],(CK_ATTRIBUTE_PTR)&Attributeval,1);
	/**実データ無しの場合、 */
	if (rv == CKR_TOKEN_NOT_RECOGNIZED) 
	{
		/**−公開鍵証明書取得処理を終了する。 */
		rv = C_FindObjectsFinal( Sessinhandle );
		return RET_IPNG_NO_MATCHING;
	}
	/**−−証明書本体か鍵ペアIDのサイズ取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
		iReturn = IDMan_IPRetJudgment(rv);
		rv = C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}

	/**−証明書取得フラグを確認する。 */
	if(getCert == 0)
	{
		/**−証明書取得フラグがPKC(x)の場合、鍵ペアIDを取得する。 */
		/**−−鍵ペアIDの領域を確保する。 */
		(*ptrKeyID) = IDMan_StMalloc(Attributeval[1].ulValueLen+1);
		/**−−鍵ペアIDの領域確保エラーの場合、 */
		if ((*ptrKeyID) == 0x00) 
		{
			/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			rv = C_FindObjectsFinal( Sessinhandle );
			return iReturn;
		}
		memset((*ptrKeyID),0x00,Attributeval[1].ulValueLen+1);
		Attributeval[0].type = CKA_ID;
		Attributeval[0].pValue = (*ptrKeyID);
	}
	else
	{
		/**−証明書取得フラグがPKC(z)の場合、証明書を取得する。 */
		/**−−証明書本体の領域を確保する。 */
		(*ptrCert) = IDMan_StMalloc(Attributeval[0].ulValueLen);
		/**−証明書本体の領域確保エラーの場合、 */
		if ((*ptrCert) == 0x00) 
		{
			/**−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
			iReturn = RET_IPNG;
			rv = C_FindObjectsFinal( Sessinhandle );
			return iReturn;
		}
		memset((*ptrCert),0x00,Attributeval[0].ulValueLen);
		Attributeval[0].type = CKA_VALUE;
		Attributeval[0].pValue = (*ptrCert);
	}


	/**−−証明書本体か鍵ペアIDの取得を行う。 */
	rv = C_GetAttributeValue(Sessinhandle,ObjectHandle[0],(CK_ATTRIBUTE_PTR)&Attributeval,1);
	/**−−証明書本体と鍵ペアIDの取得エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−−−証明書本体領域または鍵ペアID領域の解放する。 */
		if((*ptrCert) != 0x00)
		{
			IDMan_StFree ((*ptrCert));
		}
		if((*ptrKeyID) != 0x00)
		{
			IDMan_StFree ((*ptrKeyID));
		}
		/**−−−返り値に異常を設定して公開鍵証明書取得処理を終了する。 */
		iReturn = IDMan_IPRetJudgment(rv);
		rv = C_FindObjectsFinal( Sessinhandle );
		return iReturn;
	}

	/**−証明書取得フラグを確認する。 */
	if(getCert == 0)
	{
		/**−証明書取得フラグがPKC(x)の場合、鍵ペアIDのレングス値を保持する。 */
		(*lKeyID) = Attributeval[0].ulValueLen;
	}
	else
	{
		/**−証明書取得フラグがPKC(z)の場合、証明書のレングス値を保持する。 */
		(*lCert) = Attributeval[0].ulValueLen;
	}


	/**証明書または鍵ペアIDの検索操作の終了を行う。 */
	rv = C_FindObjectsFinal( Sessinhandle );
	/**証明書または鍵ペアIDの検索操作終了エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−−−返り値に異常を設定して証明書または鍵ペアIDの検索操作を終了する。 */
		/**−−−証明書本体領域または鍵ペアID領域の解放する。 */
		if((*ptrCert) != 0x00)
		{
			IDMan_StFree ((*ptrCert));
		}
		if((*ptrKeyID) != 0x00)
		{
			IDMan_StFree ((*ptrKeyID));
		}
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**返り値をリターンする。 */
	return iReturn;
}


/**
* 公開鍵証明書（PKC）の正当性チェック関数
* （パラメータの公開鍵証明書（PKC）の正当性チェックを行う）
* @author University of Tsukuba
* @param Sessinhandle セッションハンドル
* @param pPKC DER形式のX.509公開鍵証明書のバイナリデータ
* @param PKCLen DER形式のX.509公開鍵証明書のバイナリデータのレングス
* @param Algorithm ハッシュアルゴリズム
* @param pKeyObjectHandle 公開鍵オブジェクトハンドル
* @return int 0:正常 
*             1:検証失敗
*            -2:ICカードリーダにICカードが挿入されてない場合
*            -3:ICカードリーダが途中で差し替えられた場合
*            -6:異常
*            -9:初期化処理未実施
*            -10:リーダ接続エラーの場合
* @since 2008.02
* @version 1.0
*/
int IDMan_IPCheckPKC(	CK_SESSION_HANDLE Sessinhandle,
						unsigned char* pPKC,
						unsigned short int PKCLen,
						int Algorithm ,
						CK_OBJECT_HANDLE* pKeyObjectHandle )
{

	long					lret;								//返り値
	int						iReturn;							//返り値
	CK_RV					rv;									//返り値
	int						iret;								//返り値
	unsigned char			Certdata[2048];						//証明部データ
	unsigned long int		iCertLen;							//証明部データレングス
	unsigned char			Signature[2048];					//署名部データ
	unsigned long int		iSignatureLen;						//署名部データレングス
	CK_MECHANISM		strMechanismDigest;					//メカニズム情報（Digest）
	CK_MECHANISM		strMechanismVerify;					//メカニズム情報（Verify）
	CK_BYTE_PTR				pHash;								//ハッシュデータ
	CK_ULONG				lHashLen;							//ハッシュデータ長
	unsigned char				SignatureData[1024];				//署名対象フォーマットデータ
	unsigned long int		SignatureDataLen;					//署名対象フォーマットデータ

	iReturn = RET_IPOK;

	/**パラメータ公開鍵証明書（PKC）から証明書部（署名対象データ）と署名部を取得する。 */
	memset(Certdata,0x00, sizeof(Certdata));
	memset(Signature,0x00, sizeof(Signature));
	lret = IDMan_CmGetCertificateSign( pPKC, PKCLen, Certdata,(long*) &iCertLen,Signature,(long*)&iSignatureLen);
	/**パラメータ公開鍵証明書（PKC）から証明書部（署名対象データ）と署名部の取得エラーの場合、 */
	if (lret != RET_IPOK) 
	{
		iReturn = RET_IPNG;
		/**−異常リターンする。 */
		return iReturn;
	}
	/**ダイジェスト作成開始を行う。 */
	strMechanismDigest.mechanism = CERSIG_ALGORITHM;
	rv = C_DigestInit( Sessinhandle, &strMechanismDigest);
	/**ダイジェスト作成開始エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}
	/**ダイジェスト作成を行う。 */
	rv = C_DigestUpdate( Sessinhandle, Certdata,(CK_ULONG)iCertLen);
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

	/**証明書の署名対象データフォーマットを作成する。 */
	memset(SignatureData,0x00,sizeof(SignatureData));
	SignatureDataLen = CERSIG_PADDING_SIZE;
	iret = IDMan_CmPadding( Algorithm ,SignatureDataLen, pHash, lHashLen ,SignatureData );
	/**証明書の署名対象データフォーマット作成エラーの場合、 */
	if (iret != RET_IPOK) 
	{
		/**−返り値に異常リターンする。 */
		iReturn = RET_IPNG;
		IDMan_StFree (pHash);
		return iReturn;
	}
	IDMan_StFree (pHash);

	/**署名検証の開始を行う。 */
	strMechanismVerify.mechanism = CERSIG_ALGORITHM;
	rv = C_VerifyInit( Sessinhandle, (CK_MECHANISM_PTR)&strMechanismVerify,(*pKeyObjectHandle));
	/**署名検証の開始エラーの場合、 */
	if (rv != CKR_OK) 
	{
		/**−返り値に異常リターンする。 */
		iReturn = IDMan_IPRetJudgment(rv);
		return iReturn;
	}

	/**署名を検証する。 */
	rv =  C_Verify( Sessinhandle, SignatureData, SignatureDataLen, Signature, iSignatureLen);
	if (rv != CKR_OK) 
	{
		/**−署名データ不正か確認する。 */
		/**−署名データ不正の場合、 */
		if(rv == CKR_SIGNATURE_INVALID)
		{
			/**−−公開鍵証明書の検証に失敗をリターンする。 */
			iReturn = RET_IPOK_NO_VERIFY;
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

int IDMan_CheckCardStatus(CK_SESSION_HANDLE Sessinhandle)
{
	CK_RV   rv;  
	int 	iReturn = RET_IPOK;

        rv = C_GetCardStatus(Sessinhandle);
	if (rv != CKR_OK)
	{
		if (rv == CKR_TOKEN_NOT_PRESENT)
			iReturn = RET_IPNG_TOKEN_NOT_PRESENT;
		else
			iReturn = RET_IPNG; 
	}

        return iReturn;
}
