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
 * PKCS#11ライブラリプログラム
 * \file IDMan_PKPkcs11.c
 */

typedef struct CK_CUR_THREAD
{
	int td_pinned;
} CK_CUR_THREAD;
CK_CUR_THREAD *curthread;

/* 作成ライブラリ */

#include "IDMan_StandardIo.h"
#include "IDMan_PKPkcs11.h"
#include "IDMan_PKPkcs11i.h"
#include "IDMan_PKList.h"
#include "IDMan_PKCardData.h"
#include "IDMan_PKCardAccess.h"
#include <core/string.h>



/* 関数ポインタリスト */
static const CK_FUNCTION_LIST funcList = {
	{ 2, 20 },
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

#define CK_PKCS11_FUNCTION_INFO(func) \
	func,
#include "IDMan_PKPkcs11f.h"
};
#undef CK_PKCS11_FUNCTION_INFO
#undef CK_NEED_ARG_LIST

CK_BBOOL libInitFlag = CK_FALSE;	//ライブラリ初期化フラグ
CK_BBOOL slotFlag = CK_FALSE;	//スロットデータフラグ
CK_I_HEAD_PTR slotTable = CK_NULL_PTR;	//スロットテーブル
CK_I_HEAD_PTR sessionTable = CK_NULL_PTR;	//セッションテーブル
CK_ULONG hObjectCnt = 1;		//オブジェクトハンドルカウンタ（初期値=1）
CK_ULONG hSessionCnt = 1;		//セッションハンドルカウンタ（初期値=1）
CK_ULONG hContext = 0;			//スロット接続コンテキスト
CK_BYTE mszReaders[1024];		//リーダ名
CK_ULONG dwReaders;				//リーダ名サイズ



/**
 * 関数ポインタリストを取得する関数
 * @author University of Tsukuba
 * @param pFunctionList 関数アドレスリストポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR *pFunctionList)
{
	/** Cryptokiの関数ポインタリストを、引数のpFunctionListに設定する。*/
	*pFunctionList = (CK_FUNCTION_LIST_PTR) &funcList;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * PKCS#11ライブラリ初期化関数
 * @param pReserved 予備（0x00ポインタを指定）
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CANT_LOCK:ロック不可 CKR_CRYPTOKI_ALREADY_INITIALIZED:cryptoki初期化済 CKR_HOST_MEMORY:ホストメモリ不足 CKR_NEED_TO_CREATE_THREADS:スレッド生成必要 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗 CKR_DEVICE_ERROR:デバイスエラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_Initialize(CK_VOID_PTR pReserved)
{
	CK_RV 				rv = CKR_OK;

	/** 初期化フラグがTRUEの場合、*/
	if (libInitFlag == CK_TRUE)
	{
		/** −戻り値に初期化済（CKR_CRYPTOKI_ALREADY_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_ALREADY_INITIALIZED;
	}
	
	/** スロットデータテーブルの領域を確保する。*/
	slotTable = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (slotTable == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(slotTable), 0, sizeof(CK_I_HEAD));
	
	/** セッションデータテーブルの領域を確保する。*/
	sessionTable = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (sessionTable == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		IDMan_StFree(slotTable);
		slotTable = CK_NULL_PTR;
		return CKR_HOST_MEMORY;
	}
	memset((void*)(sessionTable), 0, sizeof(CK_I_HEAD));
	
	/** スロットと接続する。*/
	rv = CardEstablishContext(&hContext);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にデバイスエラー（CKR_DEVICE_ERROR）を設定し処理を抜ける。*/
		IDMan_StFree(slotTable);
		slotTable = CK_NULL_PTR;
		IDMan_StFree(sessionTable);
		sessionTable = CK_NULL_PTR;
		return CKR_DEVICE_ERROR;
	}
	
	/** スロットリスト情報を取得する。*/
	memset(mszReaders, 0x00, sizeof(mszReaders));
	dwReaders = sizeof(mszReaders);
	rv = CardListReaders(hContext, mszReaders, &dwReaders);
	/** 失敗の場合、*/
	if (rv != CKR_OK || 0 >= dwReaders )
	{
		/** −戻り値にデバイスエラー（CKR_DEVICE_ERROR）を設定し処理を抜ける。*/
		/** −スロットとの接続を終了する。*/
		rv = CardReleaseContext(&hContext);
		IDMan_StFree(slotTable);
		slotTable = CK_NULL_PTR;
		IDMan_StFree(sessionTable);
		sessionTable = CK_NULL_PTR;
		return CKR_DEVICE_ERROR;
	}

	/** 初期化フラグにTRUEを設定する。*/
	libInitFlag = CK_TRUE;
	/**スロットデータフラグにTRUEを設定する。*/
	slotFlag = CK_TRUE;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * PKCS#11ライブラリを終了する関数
 * @author University of Tsukuba
 * @param pReserved 予備（0x00ポインタを指定）
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_Finalize(CK_VOID_PTR pReserved)
{
	CK_RV rv = CKR_OK;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** スロット接続コンテキストが0x00ではない場合、*/
	if (hContext != 0x00)
	{

		/** −スロットとの接続を終了する。*/
		rv = CardReleaseContext(&hContext);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
			return CKR_FUNCTION_FAILED;
		}
		hContext = 0x00;
	}
	
	/** セッションデータテーブルの領域を解放する。*/
	DestroyList(sessionTable);
	sessionTable = CK_NULL_PTR;
	
	/** スロットデータテーブルの領域を解放する。*/
	DestroyList(slotTable);
	slotTable = CK_NULL_PTR;
	
	/** ハンドルカウンタをクリアする。*/
	hSessionCnt = 1;
	hObjectCnt = 1;
	
	/**スロットデータフラグにFALSEを設定する。*/
	slotFlag = CK_FALSE;

	/** 初期化フラグにFALSEを設定する。*/
	libInitFlag = CK_FALSE;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * ライブラリ情報を取得する関数
 * @author University of Tsukuba
 * @param pInfo ライブラリ情報ポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetInfo(CK_INFO_PTR pInfo)
{

	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}

	/** 固定値として保持しているCryptoki情報を、引数のpInfoに設定する。*/
	pInfo->cryptokiVersion.major = CK_INFO_CK_VER_MAJ;
	pInfo->cryptokiVersion.minor = CK_INFO_CK_VER_MIN;
	memset(pInfo->manufacturerID, ' ', sizeof(pInfo->manufacturerID));
	memcpy(pInfo->manufacturerID, CK_INFO_MAN_ID, sizeof(CK_INFO_MAN_ID));
	pInfo->flags = CK_INFO_FLAG;
	memset(pInfo->libraryDescription, ' ', sizeof(pInfo->libraryDescription));
	memcpy(pInfo->libraryDescription, CK_INFO_LIB_DCR, sizeof(CK_INFO_LIB_DCR));
	pInfo->libraryVersion.major = CK_INFO_LIB_VER_MAJ;
	pInfo->libraryVersion.minor = CK_INFO_LIB_VER_MIN;

	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * スロットリスト取得関数
 * @author University of Tsukuba
 * @param tokenPresent TRUE:カード有りのスロットリストを返す ／ FALSE:接続されているすべてのスロットリストを返す
 * @param pSlotList スロットIDリストポインタ
 * @param pulCount スロットIDリスト件数
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_BUFFER_TOO_SMALL:バッファ長不足 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetSlotList(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount)
{
	CK_RV rv = CKR_OK;
	CK_ULONG count = 0;
	CK_ULONG p, i;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_SLOT_INFO_PTR pSlotInfo = CK_NULL_PTR;
	CK_I_TOKEN_DATA_PTR pTokenData = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	CK_CHAR manuId[1024];
	CK_BYTE hardVer[32], firmVer[32];
	CK_BYTE major, minor;
	CK_LONG manuIdLen, hardVerLen, firmVerLen;
	
	
ReGetSlotList:

	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}

	/** スロットデータフラグがFALSEの場合、*/
	if (slotFlag == CK_FALSE)
	{
		/**−スロットデータテーブルの領域を確保する。*/
		slotTable = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
		/** 失敗の場合、*/
		if (slotTable == CK_NULL_PTR)
		{
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset((void*)(slotTable), 0, sizeof(CK_I_HEAD));
		
		/**−セッションデータテーブルの領域を確保する。*/
		sessionTable = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
		/** 失敗の場合、*/
		if (sessionTable == CK_NULL_PTR)
		{
			/**−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset((void*)(sessionTable), 0, sizeof(CK_I_HEAD));

		/**−スロットデータフラグにTRUEを設定する。*/
		slotFlag = CK_TRUE;
	}

	/** スロット接続コンテキストが0x00の場合、*/
	if ( hContext == 0x00  )
	{
	
		/** −スロットと接続する。*/
		rv = CardEstablishContext(&hContext);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値にデバイスエラー（CKR_DEVICE_ERROR）を設定し処理を抜ける。*/
			return CKR_DEVICE_ERROR;
		}
		
		/** −スロットリスト情報を取得する。*/
		memset(mszReaders, 0x00, sizeof(mszReaders));
		dwReaders = sizeof(mszReaders);
		rv = CardListReaders(hContext, mszReaders, &dwReaders);
		/** −失敗の場合、*/
		if (rv != CKR_OK || 0 >= dwReaders )
		{
			/** −−戻り値にデバイスエラー（CKR_DEVICE_ERROR）を設定し処理を抜ける。*/
			return CKR_DEVICE_ERROR;
		}
	}

	/** スロットテーブル件数が0の場合、*/
	if ( slotTable->num == 0)
	{
		/** −スロットテーブルを作成する。*/
		count = 0;
		p = 0;
		/** −リーダ名リスト長分繰り返す。*/
		for (i = 0; i < dwReaders; i++)
		{
			/** −−リーダ名の区切りの場合、*/
			if (mszReaders[i] == 0x00 || i == dwReaders-1)
			{
				/** −−−リーダ名長が0の場合、*/
				if (p == i)
				{
					/** −−−−繰り返し処理を続ける。*/
					p++;
					continue;
				}
				
				/** −−−スロットデータ領域を確保する。*/
				pSlotData = CK_NULL_PTR;
				pSlotData = (CK_I_SLOT_DATA_PTR)IDMan_StMalloc(sizeof(CK_I_SLOT_DATA));
				/** −−−失敗の場合、*/
				if (pSlotData == CK_NULL_PTR)
				{
					/** −−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
					return CKR_HOST_MEMORY;
				}
				memset((void*)(pSlotData), 0, sizeof(CK_I_SLOT_DATA));

				/** −−−リーダ名長を取得する。*/
				pSlotData->dwReaderLen = i - p;
				if (mszReaders[i] != 0x00)
				{
					pSlotData->dwReaderLen++;
				}
				
				/** −−−リーダ名を取得する。*/
				memset(pSlotData->reader, 0, sizeof(pSlotData->reader));
				memcpy(pSlotData->reader, &mszReaders[p], pSlotData->dwReaderLen);
				
				/** −−−スロットIDを設定する。*/
				pSlotData->slotID = count;
				
				/** −−−スロット情報領域を確保する。*/
				pSlotInfo = CK_NULL_PTR;
				pSlotInfo = (CK_SLOT_INFO_PTR)IDMan_StMalloc(sizeof(CK_SLOT_INFO));
				/** −−−失敗の場合、*/
				if (pSlotInfo == CK_NULL_PTR)
				{
					IDMan_StFree(pSlotData);
					/** −−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
					return CKR_HOST_MEMORY;
				}
				memset((void*)(pSlotInfo), 0, sizeof(CK_SLOT_INFO));
				
				/** −−−スロット情報をスロットデータに格納する。*/
				pSlotData->slotInfo = pSlotInfo;
				
				/** −−−取得したリーダ名をスロット情報のスロット記述に設定する。*/
				memset(pSlotData->slotInfo->slotDescription, ' ', sizeof(pSlotData->slotInfo->slotDescription));
				if (pSlotData->dwReaderLen < sizeof(pSlotData->slotInfo->slotDescription))
				{
					memcpy(pSlotData->slotInfo->slotDescription, pSlotData->reader, pSlotData->dwReaderLen);
				}
				else
				{
					memcpy(pSlotData->slotInfo->slotDescription, pSlotData->reader, sizeof(pSlotData->slotInfo->slotDescription));
				}
				
				/** −−−スロット情報にデフォルト値を設定する。*/
				memset(pSlotData->slotInfo->manufacturerID, ' ', sizeof(pSlotData->slotInfo->manufacturerID));
				pSlotData->slotInfo->flags = 0;
				pSlotData->slotInfo->hardwareVersion.major = 1;
				pSlotData->slotInfo->hardwareVersion.minor = 0;
				pSlotData->slotInfo->firmwareVersion.major = 1;
				pSlotData->slotInfo->firmwareVersion.minor = 0;
				
				/** −−−最初のスロットの場合、スロット情報（ハードウェアバージョン、ファームウェアバージョン、製造者名）を設定する。*/
				if (count == 0)
				{

					/** −−−−製造者名を取得する。*/
					memset(manuId,0x00,sizeof(manuId));
					memcpy(manuId,SLOTINFO_MANUFACTURERID,sizeof(SLOTINFO_MANUFACTURERID));
					manuIdLen = strlen((char*)manuId);
					if ((CK_ULONG)manuIdLen < sizeof(pSlotData->slotInfo->manufacturerID))
					{
						memcpy(pSlotData->slotInfo->manufacturerID, manuId, manuIdLen);
					}
					else
					{
						memcpy(pSlotData->slotInfo->manufacturerID, manuId, sizeof(pSlotData->slotInfo->manufacturerID));
					}
					
					/** −−−−ハードウェアバージョンを取得する。*/
					memset(hardVer,0x00,sizeof(hardVer));
					memcpy(hardVer,SLOTINFO_HARDWAREVERSION,sizeof(SLOTINFO_HARDWAREVERSION));
					hardVerLen = strlen((char*)hardVer);
					GetVerByTex(hardVer, hardVerLen, &major, &minor);
					pSlotData->slotInfo->hardwareVersion.major = major;
					pSlotData->slotInfo->hardwareVersion.minor = minor;
					
					/** −−−−ファームウェアバージョンを取得する。*/
					memset(firmVer,0x00,sizeof(firmVer));
					memcpy(firmVer,SLOTINFO_FIRMWAREVERSION,sizeof(SLOTINFO_FIRMWAREVERSION));
					firmVerLen = strlen((char*)firmVer);
					GetVerByTex(firmVer, firmVerLen, &major, &minor);
					pSlotData->slotInfo->firmwareVersion.major = major;
					pSlotData->slotInfo->firmwareVersion.minor = minor;
				}
				
				/** −−−カードに接続する。*/
				rv = CardConnect(hContext, pSlotData->reader, &(pSlotData->hCard), &(pSlotData->dwActiveProtocol));
				/** −−−リーダが利用不可能の場合、*/
				if (rv == CKR_DEVICE_ERROR)
				{
					/**−−−−PKCS#11ライブラリを終了する。 */
					rv = C_Finalize((CK_VOID_PTR)0x00);
					/** −−−−初期化フラグにTRUEを設定する。*/
					libInitFlag = CK_TRUE;
					IDMan_StFree(pSlotInfo);
					IDMan_StFree(pSlotData);
					/** −−−−C_GetSlotListを再度実行する。*/
					goto ReGetSlotList ;
				}
				/** −−−カードが存在しない場合、*/
				if (rv == CKR_TOKEN_NOT_PRESENT)
				{
					IDMan_StFree(pSlotInfo);
					IDMan_StFree(pSlotData);
					/** −−−−−−戻り値にカードが存在しない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
					return CKR_TOKEN_NOT_PRESENT;
				}
				/** −−−異常場合、*/
				if (rv != CKR_OK)
				{
					IDMan_StFree(pSlotInfo);
					IDMan_StFree(pSlotData);
					/** −−−−−−戻り値を設定し処理を抜ける。*/
					return rv;
				}
				/** −−−正常の場合、*/
				if (rv == CKR_OK)
				{
					/** −−−−カードのステータス情報を取得する。*/
					rv = CardStatus(pSlotData->hCard, pSlotData->reader);
					/** −−−−正常の場合、*/
					if (rv == CKR_OK)
					{
						/** −−−−−トークンデータ領域を確保する。*/
						pTokenData = CK_NULL_PTR;
						pTokenData = (CK_I_TOKEN_DATA_PTR)IDMan_StMalloc(sizeof(CK_I_TOKEN_DATA));
						/** −−−−−失敗の場合、*/
						if (pTokenData == CK_NULL_PTR)
						{
							IDMan_StFree(pSlotInfo);
							IDMan_StFree(pSlotData);
							/** −−−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
							return CKR_HOST_MEMORY;
						}
						memset((void*)(pTokenData), 0, sizeof(CK_I_TOKEN_DATA));
						/** −−−−−オブジェクトテーブル領域を確保する。*/
						pTokenData->objectTable = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
						/** −−−−−失敗の場合、*/
						if (pTokenData->objectTable == CK_NULL_PTR)
						{
							IDMan_StFree(pTokenData);
							IDMan_StFree(pSlotInfo);
							IDMan_StFree(pSlotData);
							/** −−−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
							return CKR_HOST_MEMORY;
						}
						memset((void*)(pTokenData->objectTable), 0, sizeof(CK_I_HEAD));
						
						/** −−−−−トークンデータをスロットデータに格納する。*/
						pSlotData->tokenData = pTokenData;
					}
				}
				
				/** −−−作成したスロットデータをスロットデータテーブルに格納する。*/
				rv = SetElem(slotTable, pSlotData->slotID, (CK_VOID_PTR)pSlotData, VAL_TYPE_SLOT_DATA);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−領域解放。*/
					IDMan_StFree(pTokenData->objectTable);
					IDMan_StFree(pTokenData);
					IDMan_StFree(pSlotInfo);
					IDMan_StFree(pSlotData);
					/** −−−−戻り値をそのまま設定し処理を抜ける。*/
					return rv;
				}
				
				/** −−−カウンタとポインタを進める。*/
				count++;
				if (i < dwReaders-1)
				{
					p = i + 1;
				}
			}
		}
	}
	
	count = 0;
	elem = slotTable->elem;
	/** スロット数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −スロットを取得する。*/
		pSlotData = (CK_I_SLOT_DATA_PTR)elem->val;
		/** −取得したカード有無の状態を基に、スロットリスト長を設定する。*/
		/** −該当する場合、*/
		if (tokenPresent == CK_FALSE || (tokenPresent == CK_TRUE && pSlotData->tokenData != CK_NULL_PTR))
		{
			/** −−引数のpSlotListが0x00以外の場合、*/
			if (pSlotList != CK_NULL_PTR)
			{
				/** −−スロットIDを、引数のpSlotListに設定する。*/
				pSlotList[count] = pSlotData->slotID;
			}
			/** −−スロットリスト長を加算する。*/
			count++;
		}
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	/** 引数のpSlotListが0x00の場合、*/
	if (pSlotList == CK_NULL_PTR)
	{
		/** −スロットリスト長を、引数のpulCountに設定する。*/
		*pulCount = count;
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	/** 引数のpulCountがスロットリスト長より小さい場合、*/
	if (*pulCount < count)
	{
		/** −戻り値にバッファ長不足（CKR_BUFFER_TOO_SMALL）を設定し処理を抜ける。*/
		return CKR_BUFFER_TOO_SMALL;
	}

	/** スロットリスト長を、引数のpulCountに設定する。*/
	*pulCount = slotTable->num;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * スロット情報取得関数
 * @author University of Tsukuba
 * @param slotID スロットID
 * @param pInfo スロット情報ポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_HOST_MEMORY:ホストメモリ不足 CKR_SLOT_ID_INVALID:スロットID不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
	CK_RV rv = CKR_OK;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}

	/** スロットリストから、引数のスロットIDを基に該当するスロット情報を取得する。*/
	rv = GetElem(slotTable, slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にスロットID無効（CKR_SLOT_ID_INVALID）を設定し処理を抜ける。*/
		return CKR_SLOT_ID_INVALID;
	}
	
	/** 取得したスロット情報を基に、引数のpInfoにデータ（ハードウェアバージョン、ファームウェアバージョン、製造者名、フラグ、スロットの記述）を設定する。*/
	memcpy(pInfo->slotDescription, pSlotData->slotInfo->slotDescription, sizeof(pInfo->slotDescription));
	memcpy(pInfo->manufacturerID, pSlotData->slotInfo->manufacturerID, sizeof(pInfo->manufacturerID));
	pInfo->flags = pSlotData->slotInfo->flags;
	pInfo->hardwareVersion = pSlotData->slotInfo->hardwareVersion;
	pInfo->firmwareVersion = pSlotData->slotInfo->firmwareVersion;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * トークン情報を取得する関数
 * @author University of Tsukuba
 * @param slotID スロットID
 * @param pInfo トークン情報ポインタ
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_SLOT_ID_INVALID:スロットID不正 CKR_TOKEN_NOT_PRESENT:カードが挿入されていない CKR_TOKEN_NOT_RECOGNIZED:不正なICカードを検出した CKR_ARGUMENTS_BAD:引数不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
	CK_RV rv = CKR_OK;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_BYTE rcvBuf[4096];
	CK_BYTE pkcs11DfAid[512], pkcs11DfAidHex[512];
	CK_ULONG rcvLen/*, i*/;
	CK_LONG pkcs11DfAidLen, pkcs11DfAidHexLen;
	CK_I_HEAD_PTR pTlvDataList = CK_NULL_PTR;
	/*CK_I_ELEM_PTR elem = CK_NULL_PTR;*/
	/*CK_I_HEAD_PTR pObject = CK_NULL_PTR;*/
	/*CK_BYTE efid[4];*/
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	
	/** スロットテーブルから、引数のスロットIDを基に該当するスロット情報を取得する。*/
	rv = GetElem(slotTable, slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にスロットID無効（CKR_SLOT_ID_INVALID）を設定し処理を抜ける。*/
		return CKR_SLOT_ID_INVALID;
	}
	
	
	/** トークンデータまたはカードハンドルが0x00の場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR || pSlotData->hCard == 0x00)
	{
		/** −戻り値にカードが挿入されていない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}
	
	/** カードのステータス情報を取得する。*/
	rv = CardStatus(pSlotData->hCard, pSlotData->reader);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にカードが挿入されていない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}

	/** PKCS#11用DFのAIDを取得する。*/
	memset(pkcs11DfAid,0x00,sizeof(pkcs11DfAid));
	memcpy(pkcs11DfAid,APPLICATIONID_AID,sizeof(APPLICATIONID_AID));
	pkcs11DfAidLen = strlen((char*)pkcs11DfAid);

	/** 取得したAIDをバイトデータに変換する。*/
	pkcs11DfAidHexLen = TransTex2Hex(pkcs11DfAid, pkcs11DfAidLen, pkcs11DfAidHex, pkcs11DfAidLen / 2);
	/** 失敗の場合、*/
	if (pkcs11DfAidHexLen != pkcs11DfAidLen / 2)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** PKCS#11用DFへのSELECT FILEコマンドを実行する。*/
	rcvLen = sizeof(rcvBuf);
	rv = SelectFileDF(pSlotData->hCard, &(pSlotData->dwActiveProtocol), pkcs11DfAidHex, pkcs11DfAidHexLen, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return rv;
	}
	/** DIRをメモリより取得する。*/
	rv = GetICCardData( EF_DIR, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	
	/** TLVデータリストを基に、DIRデータを作成する。*/
	rv = GetDirDataByTlv(pTlvDataList, pSlotData->tokenData);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		DestroyList(pTlvDataList);
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	/** ODFをメモリより取得する。*/
	rv = GetICCardData( EF_ODF, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	
	/** TLVデータリストを基に、ODFデータを作成する。*/
	rv = GetOdfDataByTlv(pTlvDataList, pSlotData->tokenData);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		DestroyList(pTlvDataList);
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	/** トークン情報をメモリより取得する。*/
	rv = GetICCardData( EF_TOKEN, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	
	/** TLVデータリストを基に、トークンデータを作成する。*/
	rv = GetTokenDataByTlv(pTlvDataList, pSlotData->tokenData);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	
	/** トークンデータを基に、引数のpInfoにデータ（ラベル、製造者ID、モデル、シリアル番号等）を設定する。*/
	memcpy(pInfo->label, pSlotData->tokenData->tokenInfo->label, sizeof(pInfo->label));
	memcpy(pInfo->manufacturerID, pSlotData->tokenData->tokenInfo->manufacturerID, sizeof(pInfo->manufacturerID));
	memcpy(pInfo->model, pSlotData->tokenData->tokenInfo->model, sizeof(pInfo->model));
	memcpy(pInfo->serialNumber, pSlotData->tokenData->tokenInfo->serialNumber, sizeof(pInfo->serialNumber));
	pInfo->flags = pSlotData->tokenData->tokenInfo->flags;
	pInfo->ulMaxSessionCount = pSlotData->tokenData->tokenInfo->ulMaxSessionCount;
	pInfo->ulSessionCount = pSlotData->tokenData->tokenInfo->ulSessionCount;
	pInfo->ulMaxRwSessionCount = pSlotData->tokenData->tokenInfo->ulMaxRwSessionCount;
	pInfo->ulRwSessionCount = pSlotData->tokenData->tokenInfo->ulRwSessionCount;
	pInfo->ulMaxPinLen = pSlotData->tokenData->tokenInfo->ulMaxPinLen;
	pInfo->ulMinPinLen = pSlotData->tokenData->tokenInfo->ulMinPinLen;
	pInfo->ulTotalPublicMemory = pSlotData->tokenData->tokenInfo->ulTotalPublicMemory;
	pInfo->ulFreePublicMemory = pSlotData->tokenData->tokenInfo->ulFreePublicMemory;
	pInfo->ulTotalPrivateMemory = pSlotData->tokenData->tokenInfo->ulTotalPrivateMemory;
	pInfo->ulFreePrivateMemory = pSlotData->tokenData->tokenInfo->ulFreePrivateMemory;
	pInfo->hardwareVersion = pSlotData->tokenData->tokenInfo->hardwareVersion;
	pInfo->firmwareVersion = pSlotData->tokenData->tokenInfo->firmwareVersion;
	memcpy(pInfo->utcTime, pSlotData->tokenData->tokenInfo->utcTime, sizeof(pInfo->utcTime));
	
	/** ID管理機能用DFへのSELECT FILEコマンドを実行する。*/
	rcvLen = sizeof(rcvBuf);
	rv = SelectFileDF(pSlotData->hCard, &(pSlotData->dwActiveProtocol), pSlotData->tokenData->dirData->idFuncDfAid, pSlotData->tokenData->dirData->idFuncDfAidLen, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}

	/** 秘密鍵情報（PRKDF）をメモリより取得する。*/
	rv = GetICCardData( EF_PRKDF, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}

	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを基に、オブジェクト情報をオブジェクトテーブルに追加する。*/
	rv = AddObjTblByTlv(pTlvDataList, pSlotData->tokenData->objectTable, &hObjectCnt);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	
	/** 公開鍵情報（PUKDF）をメモリより取得する。*/
	rv = GetICCardData( EF_PUKDF, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを基に、オブジェクト情報をオブジェクトテーブルに追加する。*/
	rv = AddObjTblByTlv(pTlvDataList, pSlotData->tokenData->objectTable, &hObjectCnt);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	
	/** トラストアンカーとなる証明書情報（CDFZ）をメモリより取得する。*/
	rv = GetICCardData( EF_CDFZ, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}

	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを基に、オブジェクト情報をオブジェクトテーブルに追加する。*/
	rv = AddObjTblByTlv(pTlvDataList, pSlotData->tokenData->objectTable, &hObjectCnt);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	
	/** 鍵ペアに対応する公開鍵証明書情報（CDFX）をメモリより取得する。*/
	rv = GetICCardData( EF_CDFX, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}

	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを基に、オブジェクト情報をオブジェクトテーブルに追加する。*/
	rv = AddObjTblByTlv(pTlvDataList, pSlotData->tokenData->objectTable, &hObjectCnt);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	
	/** PIN情報（AODF）をメモリより取得する。*/
	rv = GetICCardData( EF_AODF, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}

	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを基に、オブジェクト情報をオブジェクトテーブルに追加する。*/
	rv = AddObjTblByTlv(pTlvDataList, pSlotData->tokenData->objectTable, &hObjectCnt);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	
	/** ID／パスワード情報（DODF）をメモリより取得する。*/
	rv = GetICCardData( EF_DODF, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
	/** ICカードから取得したBINARYデータを基に、TLVデータリストを作成する。*/
	rv = GetTlvByBuf(rcvBuf, (CK_I_HEAD_PTR_PTR)&pTlvDataList);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを基に、オブジェクト情報をオブジェクトテーブルに追加する。*/
	rv = AddObjTblByTlv(pTlvDataList, pSlotData->tokenData->objectTable, &hObjectCnt);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_RECOGNIZED;
	}
	/** TLVデータリストを解放する。*/
	DestroyList(pTlvDataList);
	pTlvDataList = CK_NULL_PTR;
	
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * サポートメカニズム（アルゴリズム）を取得する関数
 * @author University of Tsukuba
 * @param slotID スロットID
 * @param pMechanismList メカニズムタイプポインタ
 * @param pulCount メカニズムタイプ件数
 * @return CK_RV CKR_OK:成功 CKR_BUFFER_TOO_SMALL:バッファ長不足 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_SLOT_ID_INVALID:スロットID不正 CKR_TOKEN_NOT_PRESENT:カードが挿入されていない CKR_TOKEN_NOT_RECOGNIZED:不正なICカードを検出した CKR_ARGUMENTS_BAD:引数不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetMechanismList(CK_SLOT_ID slotID, CK_MECHANISM_TYPE_PTR pMechanismList, CK_ULONG_PTR pulCount)
{
	CK_RV rv = CKR_OK;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_ULONG count = 0;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** スロットテーブルから、引数のスロットIDを基に該当するスロット情報を取得する。*/
	rv = GetElem(slotTable, slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にスロットID無効（CKR_SLOT_ID_INVALID）を設定し処理を抜ける。*/
		return CKR_SLOT_ID_INVALID;
	}
	
	/** 取得したスロットデータからトークンデータを取得する。*/
	/** トークンデータがない場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが挿入されていない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}
	
	/** 取得したトークンデータからメカニズムデータテーブルを取得する。*/
	/** メカニズムデータテーブルのデータが0件の場合、*/
	if (pSlotData->tokenData->mechanismTable == CK_NULL_PTR || pSlotData->tokenData->mechanismTable->num ==0)
	{
		/** −引数のpulCountに0を設定する。*/
		*pulCount = 0;
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	
	/** 引数のpMechanismListが0x00の場合、*/
	if (pMechanismList == CK_NULL_PTR)
	{
		/** −引数のpulCountにメカニズムデータテーブルのデータ件数を設定する。*/
		*pulCount = pSlotData->tokenData->mechanismTable->num;
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	
	/** 引数のpulCountがメカニズムデータテーブルのデータ件数より小さい場合、*/
	if (*pulCount < pSlotData->tokenData->mechanismTable->num)
	{
		/** −戻り値にバッファ長不足（CKR_BUFFER_TOO_SMALL）を設定し処理を抜ける。*/
		return CKR_BUFFER_TOO_SMALL;
	}
	
	/** メカニズムデータテーブルを基に、引数のpMechanismListにメカニズムタイプのリストを作成する。*/
	count = 0;
	elem = pSlotData->tokenData->mechanismTable->elem;
	/** メカニズム数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −メカニズムタイプを、引数のpMechanismListに設定する。*/
		pMechanismList[count++] = (CK_MECHANISM_TYPE)elem->key;
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	/** 引数のpulCountにメカニズムデータテーブルのデータ件数を設定する。*/
	*pulCount = pSlotData->tokenData->mechanismTable->num;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * メカニズム（アルゴリズム）情報取得関数
 * @author University of Tsukuba
 * @param slotID スロットID
 * @param type メカニズムタイプ
 * @param pInfo メカニズム情報ポインタ
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_MECHANISM_INVALID:メカニズム不正 CKR_SLOT_ID_INVALID:スロットID不正 CKR_TOKEN_NOT_PRESENT:カードが挿入されていない CKR_TOKEN_NOT_RECOGNIZED:不正なICカードを検出した CKR_ARGUMENTS_BAD:引数不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR pInfo)
{
	CK_RV rv = CKR_OK;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_MECHANISM_INFO_PTR mechanismInfo = CK_NULL_PTR;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** スロットテーブルから、引数のスロットIDを基に該当するスロット情報を取得する。*/
	rv = GetElem(slotTable, slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にスロットID不正（CKR_SLOT_ID_INVALID）を設定し処理を抜ける。*/
		return CKR_SLOT_ID_INVALID;
	}
	
	/** 取得したスロットデータからトークンデータを取得する。*/
	/** トークンデータがない場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが挿入されていない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}
	
	/** 取得したトークンデータからメカニズムデータテーブルを取得する。*/
	/** メカニズムデータテーブルのデータが0件の場合、*/
	if (pSlotData->tokenData->mechanismTable == CK_NULL_PTR || pSlotData->tokenData->mechanismTable->num ==0)
	{
		/** −戻り値にメカニズム不正（CKR_MECHANISM_INVALID）を設定し処理を抜ける。*/
		return CKR_MECHANISM_INVALID;
	}
	
	/** 引数のtypeを基に、取得したメカニズムデータテーブルからメカニズムデータを取得する。*/
	rv = GetElem(pSlotData->tokenData->mechanismTable, type, (CK_VOID_PTR_PTR)&mechanismInfo);
	/** 該当するメカニズムデータがない場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にメカニズム不正（CKR_MECHANISM_INVALID）を設定し処理を抜ける。*/
		return CKR_MECHANISM_INVALID;
	}
	
	/** 取得したメカニズムデータを基に、引数のpInfoにメカニズム情報を設定する。*/
	pInfo->flags = mechanismInfo->flags;
	pInfo->ulMinKeySize = mechanismInfo->ulMinKeySize;
	pInfo->ulMaxKeySize = mechanismInfo->ulMaxKeySize;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * セッションを確立する関数
 * @author University of Tsukuba
 * @param slotID スロットID
 * @param flags セッションタイプ指定フラグ
 * @param pApplication アプリケーションの定義ポインタ（0x00ポインタを指定）
 * @param Notify 通知コールバック関数のアドレス（0x00ポインタを指定）
 * @param phSession セッションハンドルポインタ
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_SESSION_COUNT:セッション数不正 CKR_SESSION_PARALLEL_NOT_SUPPORTED:並列なセッションはサポートされていない CKR_SESSION_READ_WRITE_SO_EXISTS:SO読書きセッションは存在する CKR_SLOT_ID_INVALID:スロットID不正 CKR_TOKEN_NOT_PRESENT:カードが挿入されていない CKR_TOKEN_NOT_RECOGNIZED:不正なICカードを検出した CKR_TOKEN_WRITE_PROTECTED:ICカードはライトプロテクトされている CKR_ARGUMENTS_BAD:引数不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags, CK_VOID_PTR pApplication, CK_NOTIFY Notify, CK_SESSION_HANDLE_PTR phSession)
{
	CK_RV rv = CKR_OK;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_SESSION_INFO_PTR pSessionInfo = CK_NULL_PTR;
	CK_SESSION_HANDLE hNewSession;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** スロットテーブルから、引数のスロットIDを基に該当するスロット情報を取得する。*/
	rv = GetElem(slotTable, slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にスロットID不正（CKR_SLOT_ID_INVALID）を設定し処理を抜ける。*/
		return CKR_SLOT_ID_INVALID;
	}
	
	/** 取得したスロットデータからトークンデータを取得する。*/
	/** トークンデータがない場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが挿入されていない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}
	
	
	/** 取得したトークンデータから、現在のセッション数とセッション数の上限値を取得する。*/
	/** 現在のセッション数とセッション数の上限値が等しい場合、*/
	if (pSlotData->tokenData->tokenInfo->ulSessionCount == pSlotData->tokenData->tokenInfo->ulMaxSessionCount)
	{
		/** −戻り値にセッション数不正（CKR_SESSION_COUNT）を設定し処理を抜ける。*/
		return CKR_SESSION_COUNT;
	}
	
	/** 新規セッション情報を作成する。*/
	pSessionInfo = (CK_SESSION_INFO_PTR)IDMan_StMalloc(sizeof(CK_SESSION_INFO));
	/** 失敗の場合、*/
	if (pSessionInfo == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pSessionInfo), 0, sizeof(CK_SESSION_INFO));
	
	/** 新規セッション情報に引数のスロットIDとフラグを設定する。*/
	pSessionInfo->slotID = slotID;
	pSessionInfo->flags = flags;
	/** 新規セッション情報のセッション状態フラグを設定する。*/
	/** R/Wセッションの場合、*/
	if (pSessionInfo->flags & CKF_RW_SESSION)
	{
		/** −R/Wかつパブリックに設定する。*/
		pSessionInfo->state = CKS_RW_PUBLIC_SESSION;
	}
	/** R/Oセッションの場合、*/
	else
	{
		/** −R/Oかつパブリックに設定する。*/
		pSessionInfo->state = CKS_RO_PUBLIC_SESSION;
	}
	
	/** 新規セッションデータを作成する。*/
	pSessionData = (CK_I_SESSION_DATA_PTR)IDMan_StMalloc(sizeof(CK_I_SESSION_DATA));
	/** 失敗の場合、*/
	if (pSessionData == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pSessionData), 0, sizeof(CK_I_SESSION_DATA));
	
	/** 新規セッションデータに新規セッション情報を設定する。*/
	pSessionData->sessionInfo = pSessionInfo;
	
	/** 新規セッションハンドルを作成する。*/
	hNewSession = hSessionCnt++;
	
	/** 新規セッションデータをセッションテーブルに追加する。*/
	rv = SetElem(sessionTable, hNewSession, (CK_VOID_PTR)pSessionData, VAL_TYPE_SESSION_DATA);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −領域を解放する。*/
		IDMan_StFree(pSessionInfo);
		IDMan_StFree(pSessionData);
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	
	/** 取得したトークンデータの現在のセッション数を更新する。*/
	pSlotData->tokenData->tokenInfo->ulSessionCount++;
	
	/** 新規セッションハンドルを引数に設定する。*/
	*phSession = hNewSession;
	
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * セッションを切断する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_CloseSession(CK_SESSION_HANDLE hSession)
{
	CK_RV rv = CKR_OK;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** 引数のセッションハンドルに該当するセッション情報の領域を解放し、セッションテーブルから削除する。*/
	rv = DelElem(sessionTable, hSession);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}

	/** スロットデータフラグがTRUEの場合、*/
	if (slotFlag == CK_TRUE)
	{
		/** −スロットデータテーブルが0x00ではない場合、*/
		if (slotTable != CK_NULL_PTR)
		{
			elem = slotTable->elem;
			
			/** −−スロットデータ数分以下を繰り返す。*/
			while (elem != CK_NULL_PTR)
			{
				/** −−−スロットデータを取得する。*/
				pSlotData = (CK_I_SLOT_DATA_PTR)elem->val;
				
				/** −−−スロットデータが0x00以外の場合、*/
				if (pSlotData != CK_NULL_PTR )
				{
					/** −−−−カードハンドルが0x00以外の場合、*/
					if (pSlotData->hCard != 0x00)
					{
						/** −−−−−トークンとの接続を終了する。*/
						rv = CardDisconnect(pSlotData->hCard);
					}
				}
				/** −−−次の要素を取得する。*/
				elem = elem->next;
			}
		}
	
		/** −セッションデータテーブルの領域を解放する。*/
		DestroyList(sessionTable);
		sessionTable = CK_NULL_PTR;
		
		/** −スロットデータテーブルの領域を解放する。*/
		DestroyList(slotTable);
		slotTable = CK_NULL_PTR;
		
		/** −ハンドルカウンタをクリアする。*/
		hSessionCnt = 1;
		hObjectCnt = 1;

		/**−スロットデータフラグにFALSEを設定する。*/
		slotFlag = CK_FALSE;
	}

	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * すべてのセッション切断関数
 * @author University of Tsukuba
 * @param slotID スロットID
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_SLOT_ID_INVALID:スロットID不正 CKR_TOKEN_NOT_PRESENT:カードが挿入されていない CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_CloseAllSessions(CK_SLOT_ID slotID)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルが0x00の場合、*/
	if (sessionTable == CK_NULL_PTR)
	{
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	

	elem = sessionTable->elem;
	/** セッションテーブルを検索し、引数のスロットIDに該当するセッションデータを全て解放する。*/
	/** セッション数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −セッションデータを取得する。*/
		pSessionData = (CK_I_SESSION_DATA_PTR)elem->val;
		
		/** −取得したセッションデータのスロットIDが引数のスロットIDと等しい場合、*/
		if (pSessionData->sessionInfo->slotID == slotID)
		{
			/** −−該当するセッションデータの領域を解放し、セッションテーブルから削除する。*/
			rv = DelElem(sessionTable, elem->key);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
		}
		/** −次の要素を取得する。*/
		elem = elem->next;
	}

	/** スロットデータフラグがTRUEの場合、*/
	if (slotFlag == CK_TRUE)
	{
		/** −スロットデータテーブルが0x00ではない場合、*/
		if (slotTable != CK_NULL_PTR)
		{
			elem = slotTable->elem;
			
			/** −−スロットデータ数分以下を繰り返す。*/
			while (elem != CK_NULL_PTR)
			{
				/** −−−スロットデータを取得する。*/
				pSlotData = (CK_I_SLOT_DATA_PTR)elem->val;
				
				/** −−−スロットデータが0x00以外の場合、*/
				if (pSlotData != CK_NULL_PTR )
				{
					/** −−−−カードハンドルが0x00以外の場合、*/
					if (pSlotData->hCard != 0x00)
					{
						/** −−−−−トークンとの接続を終了する。*/
						rv = CardDisconnect(pSlotData->hCard);
					}
				}
				/** −−−次の要素を取得する。*/
				elem = elem->next;
			}
		}
	
		/** −セッションデータテーブルの領域を解放する。*/
		DestroyList(sessionTable);
		sessionTable = CK_NULL_PTR;
		
		/** −スロットデータテーブルの領域を解放する。*/
		DestroyList(slotTable);
		slotTable = CK_NULL_PTR;
		
		/** −ハンドルカウンタをクリアする。*/
		hSessionCnt = 1;
		hObjectCnt = 1;

		/**−スロットデータフラグにFALSEを設定する。*/
		slotFlag = CK_FALSE;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * セッション状態を取得する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pInfo セッション状態ポインタ
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_ARGUMENTS_BAD:引数不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetSessionInfo(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR pInfo)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** 取得したセッションデータを基に、引数のpInfoにデータを設定する。*/
	pInfo->slotID = pSessionData->sessionInfo->slotID;
	pInfo->state = pSessionData->sessionInfo->state;
	pInfo->flags = pSessionData->sessionInfo->flags;
	pInfo->ulDeviceError = pSessionData->sessionInfo->ulDeviceError;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * トークンにログインする関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param userType ユーザタイプ
 * @param pPin パスワード文字列ポインタ
 * @param ulPinLen パスワード文字列長
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_NOT_INITIALIZED:オペレーション未初期化 CKR_PIN_INCORRECT:パスワード指定誤り CKR_PIN_LOCKED:パスワードがロックされている CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_SESSION_READ_ONLY_EXISTS:読取りのみセッションは存在する CKR_USER_ALREADY_LOGGED_IN:ユーザログイン済 CKR_USER_ANOTHER_ALREADY_LOGGED_IN:他ユーザログイン済 CKR_USER_PIN_NOT_INITIALIZED:ユーザPIN未初期化 CKR_USER_TOO_MANY_TYPES:ユーザ種別過多 CKR_USER_TYPE_INVALID:ユーザ種別不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType, CK_CHAR_PTR pPin, CK_ULONG ulPinLen)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_BBOOL hitFlg = CK_FALSE;
	CK_BYTE rcvBuf[4096];
	CK_ULONG rcvLen/*, i*/;
	CK_ULONG classVal;
	CK_BYTE labelVal[256], efid[4];
	char				strMinPinLen[16];
	char				strMaxPinLen[16];
	unsigned long int	lMinPinLen;
	unsigned long int	lMaxPinLen;
	int					iret;

	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** セッションがログイン状態の場合、*/
	if (pSessionData->sessionInfo->state == CKS_RO_USER_FUNCTIONS ||
		pSessionData->sessionInfo->state == CKS_RW_USER_FUNCTIONS ||
		pSessionData->sessionInfo->state == CKS_RW_SO_FUNCTIONS)
	{
		/** −戻り値にユーザログイン済（CKR_USER_ALREADY_LOGGED_IN）を設定し処理を抜ける。*/
		return CKR_USER_ALREADY_LOGGED_IN;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** スロットデータ内のトークンデータが0x00の場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが抜かれた（CKR_DEVICE_REMOVED）を設定し処理を抜ける。*/
		return CKR_DEVICE_REMOVED;
	}
	

#if 1
	/** 設定情報から最大PIN長を取得する。*/
	memset(strMaxPinLen,0x00,sizeof(strMaxPinLen));
	iret = IDMan_StReadSetData(MAXPINLEN, strMaxPinLen,&lMaxPinLen);
	/**設定情報から最大PIN長取得エラーの場合、 */
	if(iret < 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	lMaxPinLen=atol(strMaxPinLen);

	/** 設定情報から最小PIN長を取得する。*/
	memset(strMinPinLen,0x00,sizeof(strMinPinLen));
	iret = IDMan_StReadSetData(MINPINLEN, strMinPinLen,&lMinPinLen);
	/**設定情報から最小PIN長取得エラーの場合、 */
	if(iret < 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	lMinPinLen=atol(strMinPinLen);
#else
	lMaxPinLen=16;
	lMinPinLen=16;
#endif

	/** PINの長さをチェックする。 */
	if (ulPinLen < lMinPinLen ||
		ulPinLen > lMaxPinLen)
	{
		/** −戻り値にパスワード指定誤り（CKR_PIN_INCORRECT）を設定し処理を抜ける。*/
		return CKR_PIN_INCORRECT;
	}
	
	/** userTypeがユーザ、SO以外の場合、*/
	if (userType != CKU_USER && userType != CKU_SO)
	{
		/** −戻り値にユーザ種別不正（CKR_USER_TYPE_INVALID）を設定し処理を抜ける。*/
		return CKR_USER_TYPE_INVALID;
	}
	
	/** オブジェクトテーブルを検索する。*/
	elem = pSlotData->tokenData->objectTable->elem;
	/** オブジェクト数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −オブジェクトを取得する。*/
		pObject = (CK_I_HEAD_PTR)elem->val;
		
		/** −オブジェクトからCLASS属性を取得する。*/
		rv = GetElem(pObject, CKA_CLASS, (CK_VOID_PTR_PTR)&pAttr);
		/** −取得失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		
		/** −CLASS属性値を取得する。*/
		memcpy((void*)&classVal, pAttr->pValue, sizeof(classVal));
		
		/** −CLASS属性がDataの場合、*/
		if (classVal == CKO_DATA)
		{
			/** −−オブジェクトからLABEL属性を取得する。*/
			rv = GetElem(pObject, CKA_LABEL, (CK_VOID_PTR_PTR)&pAttr);
			/** −−取得失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			
			/** −−LABEL属性値を取得する。*/
			memset(labelVal, 0, sizeof(labelVal));
			memcpy(labelVal, pAttr->pValue, pAttr->ulValueLen);
			
			/** −−LABEL属性がユーザ種別に対応する場合、*/
			if ((userType == CKU_USER && pAttr->ulValueLen == sizeof(CK_LABEL_USER_PIN)-1 && memcmp(labelVal, CK_LABEL_USER_PIN, pAttr->ulValueLen) == 0) ||
				(userType == CKU_SO && pAttr->ulValueLen == sizeof(CK_LABEL_SO_PIN)-1 && memcmp(labelVal, CK_LABEL_SO_PIN, pAttr->ulValueLen) == 0))
			{
				/** −−−オブジェクトからEFID属性を取得する。*/
				rv = GetElem(pObject, CKA_EFID, (CK_VOID_PTR_PTR)&pAttr);
				/** −−−取得失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
					return CKR_GENERAL_ERROR;
				}
				/** −−−EFID長が不正の場合、*/
				if (pAttr->ulValueLen != 4)
				{
					/** −−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
					return CKR_GENERAL_ERROR;
				}
				/** −−−EFIDを取得する。*/
				memcpy(efid, pAttr->pValue, pAttr->ulValueLen);
				
				hitFlg = CK_TRUE;
				break;
			}
		}
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	/** 該当するPINのパスがない場合、*/
	if (hitFlg == CK_FALSE)
	{
		/** −戻り値にパスワード指定誤り（CKR_PIN_INCORRECT）を設定し処理を抜ける。*/
		return CKR_PIN_INCORRECT;
	}
	
	/** PINのパスへのSELECT FILEコマンドを実行する。*/
	rcvLen = sizeof(rcvBuf);
	rv = SelectFileEF(pSlotData->hCard, &(pSlotData->dwActiveProtocol), &efid[2], 2, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}
	
	/** SELECTしたPINへのVERIFYコマンドを実行する。*/
	rcvLen = sizeof(rcvBuf);
	rv = Verify(pSlotData->hCard, &(pSlotData->dwActiveProtocol), pPin, ulPinLen, rcvBuf, &rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	
	/** セッションデータにPINを格納する。*/
	pSessionData->ulPinLen = ulPinLen;
	memset((void*)pSessionData->pin, 0, sizeof(pSessionData->pin));
	memcpy(pSessionData->pin, pPin, ulPinLen);
	
	/** セッション状態フラグをログイン状態に設定する。*/
	/** SOログインの場合、*/
	if (userType == CKU_SO)
	{
		/** −R/Wセッションの場合、*/
		if (pSessionData->sessionInfo->flags & CKF_RW_SESSION)
		{
			/** −−R/WかつSOに設定する。*/
			pSessionData->sessionInfo->state = CKS_RW_SO_FUNCTIONS;
		}
		/** −R/Oセッションの場合、*/
		else
		{
			/** −−戻り値にユーザ種別不正（CKR_USER_TYPE_INVALID）を設定し処理を抜ける。*/
			return CKR_USER_TYPE_INVALID;
		}
	}
	/** ユーザログインの場合、*/
	else if (userType == CKU_USER)
	{
		/** −R/Wセッションの場合、*/
		if (pSessionData->sessionInfo->flags & CKF_RW_SESSION)
		{
			/** −−R/Wかつユーザに設定する。*/
			pSessionData->sessionInfo->state = CKS_RW_USER_FUNCTIONS;
		}
		/** −R/Oセッションの場合、*/
		else
		{
			/** −−R/Oかつユーザに設定する。*/
			pSessionData->sessionInfo->state = CKS_RO_USER_FUNCTIONS;
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * トークンからログアウトする関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_USER_NOT_LOGGED_IN:ユーザ未ログイン CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_Logout(CK_SESSION_HANDLE hSession)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** セッションがログアウト状態の場合、*/
	if (pSessionData->sessionInfo->state == CKS_RO_PUBLIC_SESSION ||
		pSessionData->sessionInfo->state == CKS_RW_PUBLIC_SESSION)
	{
		/** −戻り値にユーザ未ログイン（CKR_USER_NOT_LOGGED_IN）を設定し処理を抜ける。*/
		return CKR_USER_NOT_LOGGED_IN;
	}
	
	/** セッションデータのPINを解放する。*/
	pSessionData->ulPinLen = 0;
	memset((void*)pSessionData->pin, 0, sizeof(pSessionData->pin));
	
	/** セッションデータの状態フラグをログアウト状態に設定する。*/
	/** R/Wセッションの場合、*/
	if (pSessionData->sessionInfo->flags & CKF_RW_SESSION)
	{
		/** −パブリックかつR/Wに設定する。*/
		pSessionData->sessionInfo->state = CKS_RW_PUBLIC_SESSION;
	}
	/** R/Oセッションの場合、*/
	else
	{
		/** −パブリックかつR/Oに設定する。*/
		pSessionData->sessionInfo->state = CKS_RO_PUBLIC_SESSION;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * オブジェクト検索を開始する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pTemplate 属性テーブルポインタ
 * @param ulCount 属性テーブル数
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_ATTRIBUTE_TYPE_INVALID:属性タイプ不正 CKR_ATTRIBUTE_VALUE_INVALID:属性値不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_ACTIVE:オペレーション活性 CKR_PIN_EXPIRED:パスワード期限切れ CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
	CK_RV rv = CKR_OK;
	CK_ULONG rvAttr;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_ULONG i;


	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** 検索中の場合、*/
	if (pSessionData->srchFlg == CK_TRUE)
	{
		/** −検索条件オブジェクト削除*/
		if (pSessionData->srchObject != CK_NULL_PTR)
		{
			DestroyList(pSessionData->srchObject);
			pSessionData->srchObject = CK_NULL_PTR;
		}
	}
	
	/** 検索用属性テーブルがある場合、*/
	if (ulCount > 0)
	{
		/** −検索条件オブジェクトを作成する。*/
		pSessionData->srchObject = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
		/** −失敗の場合、*/
		if (pSessionData->srchObject == CK_NULL_PTR)
		{
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset((void*)(pSessionData->srchObject), 0, sizeof(CK_I_HEAD));
		
		/** −属性テーブル数まで繰り返す。*/
		for (i = 0; i < ulCount; i++)
		{
			/** −−属性情報の領域を確保する。*/
			pAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
			/** −−失敗の場合、*/
			if (pAttr == CK_NULL_PTR)
			{
				/** −−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
				return CKR_HOST_MEMORY;
			}
			memset((void*)pAttr, 0, sizeof(CK_ATTRIBUTE));
			/** −−属性の種別、値の長さを設定。*/
			pAttr->type = pTemplate[i].type;
			pAttr->ulValueLen = pTemplate[i].ulValueLen;
			
			/** −−属性の値が0x00以外の場合、*/
			if (pTemplate[i].pValue != CK_NULL_PTR)
			{
				/** −−−属性の値を設定する。*/
				rvAttr = GetAttrValType(pAttr->type);
				switch (rvAttr)
				{
				/** −−−以下は数字で設定する。*/
				case ATTR_VAL_TYPE_NUM:
					pAttr->pValue = pTemplate[i].pValue;
					break;
				/** −−−以下は1バイトで設定する。*/
				case ATTR_VAL_TYPE_BYTE:
					/** −−−−値の領域を確保する。*/
					pAttr->pValue = IDMan_StMalloc(pTemplate[i].ulValueLen);
					/** −−−−失敗の場合、*/
					if (pAttr->pValue == CK_NULL_PTR)
					{
						IDMan_StFree(pAttr);
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset(pAttr->pValue, 0, pTemplate[i].ulValueLen);
					memcpy(pAttr->pValue, pTemplate[i].pValue, pTemplate[i].ulValueLen);
					break;
				/** −−−以下はバイト配列で設定する。*/
				case ATTR_VAL_TYPE_BYTE_ARRAY:
					/** −−−−値の領域を確保する。*/
					pAttr->pValue = IDMan_StMalloc(pTemplate[i].ulValueLen);
					/** −−−−失敗の場合、*/
					if (pAttr->pValue == CK_NULL_PTR)
					{
						IDMan_StFree(pAttr);
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset(pAttr->pValue, 0, pTemplate[i].ulValueLen);
					memcpy(pAttr->pValue, pTemplate[i].pValue, pTemplate[i].ulValueLen);
					break;
				/** −−−上記以外の場合、*/
				default:
					/** −−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
					return CKR_GENERAL_ERROR;
					break;
				}
			}
			/** −−属性の値が0x00の場合、*/
			else
			{
				/** −−−値に0x00を設定。*/
				pAttr->pValue = CK_NULL_PTR;
				/** −−−長さ0に設定。*/
				pAttr->ulValueLen = 0;
			}
			
			/** −−属性情報を追加する。*/
			rv = SetElem(pSessionData->srchObject, pAttr->type, (CK_VOID_PTR)pAttr, VAL_TYPE_ATTRIBUTE);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				IDMan_StFree(pAttr->pValue);
				IDMan_StFree(pAttr);
				/** −−−戻り値をそのまま設定し処理を抜ける。*/
				return rv; 
			}
		}
	}
	/** 検索中フラグをONにする。*/
	pSessionData->srchFlg = CK_TRUE;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * オブジェクトを検索する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param phObject オブジェクトハンドルポインタ
 * @param ulMaxObjectCount 最大オブジェクト数
 * @param pulObjectCount オブジェクト数ポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_NOT_INITIALIZED:オペレーション未初期化 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxObjectCount, CK_ULONG_PTR pulObjectCount)
{
	CK_RV rv = CKR_OK;
	CK_ULONG rvAttr;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_I_ELEM_PTR elemAttr = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR elemAttrVal = CK_NULL_PTR;
	CK_BBOOL hitFlg = CK_FALSE;
	CK_ULONG count = 0;
	CK_ULONG numVal, numValSrch;
	CK_BYTE byteVal[256], byteValSrch[256];
	CK_I_HEAD_PTR tmpList = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** 検索中でない場合、*/
	if (pSessionData->srchFlg == CK_FALSE)
	{
		/** −戻り値にオペレーション未初期化（CKR_OPERATION_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_OPERATION_NOT_INITIALIZED;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** スロットデータ内のトークンデータが0x00の場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが抜かれた（CKR_DEVICE_REMOVED）を設定し処理を抜ける。*/
		return CKR_DEVICE_REMOVED;
	}
	
	/** オブジェクトテーブルが0x00または0件の場合、*/
	if (pSlotData->tokenData->objectTable == CK_NULL_PTR || pSlotData->tokenData->objectTable->num == 0)
	{
		/** −引数のオブジェクト件数を0に設定する。*/
		phObject = CK_NULL_PTR;
		*pulObjectCount = 0;
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	
	/** オブジェクトテーブルを検索する。*/
	count = 0;
	elem = pSlotData->tokenData->objectTable->elem;
	/** オブジェクト数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −オブジェクトを取得する。*/
		pObject = (CK_I_HEAD_PTR)elem->val;
		hitFlg = CK_TRUE;
		
		/** −検索条件オブジェクトが設定されている場合、*/
		if (pSessionData->srchObject != CK_NULL_PTR && pSessionData->srchObject->num > 0)
		{
			/** −−検索条件オブジェクトの属性情報を検索する。*/
			elemAttr = pSessionData->srchObject->elem;
			/** −−検索条件オブジェクトの属性情報数分繰り返す。*/
			while (elemAttr != CK_NULL_PTR && hitFlg == CK_TRUE)
			{
				/** −−−オブジェクトから、該当する属性情報を取得する。*/
				rv = GetElem(pObject, elemAttr->key, (CK_VOID_PTR_PTR)&pAttr);
				/** −−−該当情報なしの場合、フラグOFF。*/
				if (rv != CKR_OK)
				{
					hitFlg = CK_FALSE;
				}
				/** −−−該当情報ありの場合、*/
				else
				{
					/** −−−−属性情報の値が一致するか判定する。*/
					elemAttrVal = (CK_ATTRIBUTE_PTR)elemAttr->val;
					
					rvAttr = GetAttrValType(elemAttrVal->type);
					switch (rvAttr)
					{
					/** −−−−以下は数字で比較する。*/
					case ATTR_VAL_TYPE_NUM:
						/** −−−−−数字の属性値を取得する。*/
						/* WORKAROUND: sizeof (ulong) != sizeof (int) bug */
						numVal = 0;
						numValSrch = 0;
						memcpy((void*)&numVal, pAttr->pValue, sizeof(numVal) < pAttr->ulValueLen ? sizeof(numVal) : pAttr->ulValueLen);
						memcpy((void*)&numValSrch, elemAttrVal->pValue, sizeof(numValSrch) < elemAttrVal->ulValueLen ? sizeof(numValSrch) : elemAttrVal->ulValueLen);
						/** −−−−−値が一致しない場合、フラグOFF。*/
						if (/*pAttr->ulValueLen != elemAttrVal->ulValueLen || */numVal != numValSrch)
						{
							hitFlg = CK_FALSE;
						}
						break;
					/** −−−−以下はバイトデータで比較する。*/
					case ATTR_VAL_TYPE_BYTE:
					case ATTR_VAL_TYPE_BYTE_ARRAY:
						/** −−−−−バイトデータの属性値を取得する。*/
						memset(byteVal, 0, sizeof(byteVal));
						memcpy(byteVal, pAttr->pValue, pAttr->ulValueLen);
						memset(byteValSrch, 0, sizeof(byteValSrch));
						memcpy(byteValSrch, elemAttrVal->pValue, elemAttrVal->ulValueLen);
						/** −−−−−値が一致しない場合、フラグOFF。*/
						if (pAttr->ulValueLen != elemAttrVal->ulValueLen || memcmp(byteVal, byteValSrch, pAttr->ulValueLen) != 0)
						{
							hitFlg = CK_FALSE;
						}
						break;
					/** −−−−上記以外の場合、*/
					default:
						/** −−−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
						return CKR_GENERAL_ERROR;
						break;
					}
				}
				/** −−−次の要素を取得する。*/
				elemAttr = elemAttr->next;
			}
		}
		
		/** −該当ありの場合、*/
		if (hitFlg == CK_TRUE)
		{
			/** −−オブジェクトから、Private属性情報を取得する。*/
			rv = GetElem(pObject, CKA_PRIVATE, (CK_VOID_PTR_PTR)&pAttr);
			/** −−Private属性情報ありの場合、*/
			if (rv == CKR_OK)
			{
				/** −−−Private属性値を取得する。*/
				memset(byteVal, 0, sizeof(byteVal));
				memcpy(byteVal, pAttr->pValue, pAttr->ulValueLen);
				/** −−−パブリックオブジェクト、またはプライベートオブジェクトかつログイン済みの場合、*/
				if ( byteVal[0] == CK_FALSE ||
					( byteVal[0] == CK_TRUE &&
						(pSessionData->sessionInfo->state == CKS_RO_USER_FUNCTIONS ||
						pSessionData->sessionInfo->state == CKS_RW_USER_FUNCTIONS ||
						pSessionData->sessionInfo->state == CKS_RW_SO_FUNCTIONS) ) )
				{
					/** −−−−引数のphObjectに該当オブジェクトハンドルを設定する。*/
					phObject[count++] = (CK_OBJECT_HANDLE)elem->key;
				}
			}
		}
		
		/** −検索最大件数に達した場合、*/
		if (count >= ulMaxObjectCount)
		{
			/** −−繰り返し処理を終了する。*/
			break;
		}
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	/** 引数のオブジェクト件数を設定する。*/
	*pulObjectCount = count;
	
	/** 検索オブジェクトがID/パスワードの場合、ID/パスワードの件数を設定する。*/
	/** 検索条件オブジェクトが設定されている場合、*/
	if (pSessionData->srchObject != CK_NULL_PTR && pSessionData->srchObject->num > 0)
	{
		/** −フラグをONにする。*/
		hitFlg = CK_TRUE;
		
		/** −検索条件オブジェクトから、CLASS属性情報を取得する。*/
		rv = GetElem(pSessionData->srchObject, CKA_CLASS, (CK_VOID_PTR_PTR)&pAttr);
		/** −該当情報ありの場合、*/
		if (rv == CKR_OK)
		{
			/** −−CLASS属性値を取得する。*/
			memcpy((void*)&numVal, pAttr->pValue, sizeof(numVal));
			/** −−値がDATAでない場合、*/
			if (numVal != CKO_DATA)
			{
				/** −−−フラグをOFFにする。*/
				hitFlg = CK_FALSE;
			}
		}
		/** −該当情報なしの場合、*/
		else
		{
			/* −−フラグをOFFにする。*/
			hitFlg = CK_FALSE;
		}
		
		/** −検索条件オブジェクトから、LABEL属性情報を取得する。*/
		rv = GetElem(pSessionData->srchObject, CKA_LABEL, (CK_VOID_PTR_PTR)&pAttr);
		/** −該当情報あり、かつフラグONの場合、*/
		if (rv == CKR_OK && hitFlg == CK_TRUE)
		{
			/** −−LABEL属性値を取得する。*/
			memset(byteVal, 0, sizeof(byteVal));
			memcpy(byteVal, pAttr->pValue, pAttr->ulValueLen);
			/** −−値がID/パスワードでない場合、*/
			if (pAttr->ulValueLen != sizeof(CK_LABEL_ID_PASS) - 1 || memcmp(byteVal, CK_LABEL_ID_PASS, pAttr->ulValueLen) != 0)
			{
			/** −− フラグをOFFにする。*/
				hitFlg = CK_FALSE;
			}
		}
		/** −該当情報なしの場合、*/
		else
		{
			/** −−フラグをOFFにする。*/
			hitFlg = CK_FALSE;
		}
		
		
		/** −オブジェクトテーブルを検索する。*/
		elem = pSlotData->tokenData->objectTable->elem;
		/** −フラグONの場合、オブジェクト数分以下を繰り返す。*/
		while (elem != CK_NULL_PTR && hitFlg == CK_TRUE)
		{
			/** −−オブジェクトを取得する。*/
			pObject = (CK_I_HEAD_PTR)elem->val;
			
			/** −−実データを取得する。*/
			rv = GetValueData(pObject, CKA_VALUE_ID, pSlotData);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値をそのまま設定し処理を抜ける。*/
				return rv;
			}
			
			/** −−オブジェクトから、IDリスト属性を取得する。*/
			rv = GetElem(pObject, CKA_VALUE_ID, (CK_VOID_PTR_PTR)&tmpList);
			/** −−該当情報ありの場合、*/
			if (rv == CKR_OK)
			{
				/** −−−引数のオブジェクト件数に、IDリストの要素数を設定する。*/
				*pulObjectCount = tmpList->num;
				/** −−−IDリストの要素数分オブジェクトハンドルを設定する。*/
				for (count = 0; count < *pulObjectCount; count++)
				{
					/** −−−−検索最大件数に達した場合、*/
					if (count >= ulMaxObjectCount)
					{
						/** −−−−−繰り返し処理を終了する。*/
						break;
					}
					/** −−−−0番目のオブジェクトハンドルをコピーする。*/
					phObject[count] = phObject[0];
				}
				break;
			}
			/** −−次の要素を取得する。*/
			elem = elem->next;
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * オブジェクト検索を終了する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_NOT_INITIALIZED:オペレーション未初期化 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** 検索中でない場合、*/
	if (pSessionData->srchFlg == CK_FALSE)
	{
		/** −戻り値にオペレーション未初期化（CKR_OPERATION_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_OPERATION_NOT_INITIALIZED;
	}
	
	/** 検索オブジェクト削除。*/
	DestroyList(pSessionData->srchObject);
	pSessionData->srchObject = CK_NULL_PTR;
	
	/** 検索中フラグをOFFにする。*/
	pSessionData->srchFlg = CK_FALSE;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * オブジェクト属性値を取得する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param hObject オブジェクトハンドル
 * @param pTemplate 属性テーブルポインタ
 * @param ulCount 属性テーブル数
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_ATTRIBUTE_SENSITIVE:属性不明 CKR_ATTRIBUTE_TYPE_INVALID:属性タイプ不正 CKR_BUFFER_TOO_SMALL:バッファ長不足 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OBJECT_HANDLE_INVALID:オブジェクトハンドル不正 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_TOKEN_NOT_RECOGNIZED:不正なICカードを検出した CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_ULONG i, idCnt, passCnt, tmpCnt;
	CK_I_HEAD_PTR tmpList = CK_NULL_PTR;
	CK_BYTE byteVal[256];
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** スロットデータ内のトークンデータが0x00の場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが抜かれた（CKR_DEVICE_REMOVED）を設定し処理を抜ける。*/
		return CKR_DEVICE_REMOVED;
	}
	
	/** オブジェクトテーブルが0x00または0件の場合、*/
	if (pSlotData->tokenData->objectTable == CK_NULL_PTR || pSlotData->tokenData->objectTable->num == 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** オブジェクトテーブルから、引数のオブジェクトハンドルを基に該当するオブジェクトを取得する。*/
	rv = GetElem(pSlotData->tokenData->objectTable, hObject, (CK_VOID_PTR_PTR)&pObject);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にオブジェクトハンドル不正（CKR_OBJECT_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_OBJECT_HANDLE_INVALID;
	}
	
	/** オブジェクトから、Private属性情報を取得する。*/
	rv = GetElem(pObject, CKA_PRIVATE, (CK_VOID_PTR_PTR)&pAttr);
	/** Private属性情報ありの場合、*/
	if (rv == CKR_OK)
	{
		/** −Private属性値を取得する。*/
		memset(byteVal, 0, sizeof(byteVal));
		memcpy(byteVal, pAttr->pValue, pAttr->ulValueLen);
		/** −プライベートオブジェクトかつログイン済みでない場合、*/
		if ( byteVal[0] == CK_TRUE &&
				(pSessionData->sessionInfo->state == CKS_RO_PUBLIC_SESSION ||
				pSessionData->sessionInfo->state == CKS_RW_PUBLIC_SESSION) )
		{
			/** −−戻り値にオブジェクトハンドル不正（CKR_OBJECT_HANDLE_INVALID）を設定し処理を抜ける。*/
			return CKR_OBJECT_HANDLE_INVALID;
		}
	}
	/** Private属性情報なしの場合、*/
	else
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	idCnt = 0;
	passCnt = 0;
	/** 属性テーブル数分繰り返す。*/
	for (i = 0; i < ulCount; i++)
	{
		/** −実データを取得する。*/
		rv = GetValueData(pObject, pTemplate[i].type, pSlotData);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
		
		/** −属性がIDまたはパスワードの場合、*/
		if (pTemplate[i].type == CKA_VALUE_ID || pTemplate[i].type == CKA_VALUE_PASSWORD)
		{
			if (pTemplate[i].type == CKA_VALUE_ID)
			{
				tmpCnt = idCnt++;
			}
			else
			{
				tmpCnt = passCnt++;
			}
			
			/** −−オブジェクトから、該当属性情報リストを取得する。*/
			rv = GetElem(pObject, pTemplate[i].type, (CK_VOID_PTR_PTR)&tmpList);
			/** −−該当属性情報リストありの場合、*/
			if (rv == CKR_OK)
			{
				/** −−−該当属性情報リストから、該当属性情報を取得する。*/
				rv = GetElem(tmpList, tmpCnt, (CK_VOID_PTR_PTR)&pAttr);
				/** −−−該当属性情報ありの場合、*/
				if (rv == CKR_OK)
				{
					/** −−−−属性値が0x00の場合、*/
					if (pTemplate[i].pValue == CK_NULL_PTR)
					{
					/** −−−−−サイズのみ設定する。*/
						pTemplate[i].ulValueLen = pAttr->ulValueLen;
					}
					/** −−−−属性値が0x00でない場合、*/
					else
					{
						/** −−−−−属性値の領域が十分確保されている場合、属性値とサイズを設定する。*/
						if (pTemplate[i].ulValueLen >= pAttr->ulValueLen)
						{
							memcpy((void*)pTemplate[i].pValue, pAttr->pValue, pAttr->ulValueLen);
							pTemplate[i].ulValueLen = pAttr->ulValueLen;
						}
						/** −−−−−属性値の領域が十分確保されていない場合、*/
						else
						{
							/** −−−−−−戻り値にバッファ長不足（CKR_BUFFER_TOO_SMALL）を設定し処理を抜ける。*/
							return CKR_BUFFER_TOO_SMALL;
						}
					}
				}
				/** −−−該当属性情報なしの場合、*/
				else
				{
					/** −−−−属性の値の長さに-1を設定する。*/
					pTemplate[i].ulValueLen = -1;
				}
			}
			/** −−該当属性情報なしの場合、*/
			else
			{
				/** −−−属性の値の長さに-1を設定する。*/
				pTemplate[i].ulValueLen = -1;
			}
		}
		/** −上記以外の場合、*/
		else
		{
			/** −−オブジェクトから、該当属性情報を取得する。*/
			rv = GetElem(pObject, pTemplate[i].type, (CK_VOID_PTR_PTR)&pAttr);
			/** −−該当属性情報ありの場合、*/
			if (rv == CKR_OK)
			{
				/** −−−属性値が0x00の場合、*/
				if (pTemplate[i].pValue == CK_NULL_PTR)
				{
					/** −−−−サイズのみ設定する。*/
					pTemplate[i].ulValueLen = pAttr->ulValueLen;
				}
				/** −−−属性値が0x00でない場合、*/
				else
				{
					/** −−−−属性値の領域が十分確保されている場合、属性値とサイズを設定する。*/
					if (pTemplate[i].ulValueLen >= pAttr->ulValueLen)
					{
						memcpy((void*)pTemplate[i].pValue, pAttr->pValue, pAttr->ulValueLen);
						pTemplate[i].ulValueLen = pAttr->ulValueLen;
					}
					/** −−−−属性値の領域が十分確保されていない場合、*/
					else
					{
						/** −−−−−戻り値にバッファ長不足（CKR_BUFFER_TOO_SMALL）を設定し処理を抜ける。*/
						return CKR_BUFFER_TOO_SMALL;
					}
				}
			}
			/** −−該当属性情報なしの場合、*/
			else
			{
				/** −−−属性の値の長さに-1を設定する。*/
				pTemplate[i].ulValueLen = -1;
			}
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 署名処理を初期する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pMechanism メカニズム情報ポインタ
 * @param hKey オブジェクトハンドル
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_KEY_FUNCTION_NOT_PERMITTED:関数不許可 CKR_KEY_HANDLE_INVALID:鍵ハンドル不正 CKR_KEY_SIZE_RANGE:鍵長不正 CKR_KEY_TYPE_INCONSISTENT:鍵種別が一致しない CKR_MECHANISM_INVALID:メカニズム不正 CKR_MECHANISM_PARAM_INVALID:メカニズムパラメータ不正 CKR_OPERATION_ACTIVE:オペレーション活性 CKR_PIN_EXPIRED:パスワード期限切れ CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_USER_NOT_LOGGED_IN:ユーザ未ログイン CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_SignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	/*CK_ULONG count = 0;*/
	/*CK_I_ELEM_PTR elem = CK_NULL_PTR;*/
	/*CK_MECHANISM_INFO_PTR mechanismInfo = CK_NULL_PTR;*/
	/*CK_BBOOL hitFlg = CK_FALSE;*/
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 取得したスロットデータからトークンデータを取得する。*/
	/** トークンデータがない場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 引数のhKeyを基に、オブジェクトテーブルから該当する鍵オブジェクトを検索する。*/
	rv = GetElem(pSlotData->tokenData->objectTable, hKey, (CK_VOID_PTR_PTR)&pObject);
	/** 該当する鍵がない場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に鍵ハンドル不正（CKR_KEY_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_KEY_HANDLE_INVALID;
	}
	
	/** セッションデータに鍵オブジェクトを設定する。*/
	pSessionData->signObject = pObject;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 署名を行う関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pData データポインタ
 * @param ulDataLen データ長
 * @param pSignature 署名データポインタ
 * @param pulSignatureLen 署名データ長ポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_BUFFER_TOO_SMALL:バッファ長不足 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DATA_INVALID:データ不正 CKR_DATA_LEN_RANGE:データ長不正 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_NOT_INITIALIZED:オペレーション未初期化 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_USER_NOT_LOGGED_IN:ユーザ未ログイン CKR_FUNCTION_REJECTED:関数拒否 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_Sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen)
{
	CK_RV rv = CKR_OK;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_BYTE rcvBuf[4096], /*data[1024],*/ efid[4], tmpSignature[2048];
	CK_ULONG rcvLen, tmpSignatureLen/*, i*/;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** C_SignInit未実行の場合、*/
	if (pSessionData->signObject == CK_NULL_PTR)
	{
		/** −戻り値にオペレーション未初期化（CKR_OPERATION_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_OPERATION_NOT_INITIALIZED;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	/** 引数のpSignatureが0x00ではなく、前回Sign時と同じデータを使う場合、*/
	if (pSignature != CK_NULL_PTR &&
		pSessionData->signDataLen > 0 &&
		pSessionData->signDataLen == ulDataLen &&
		memcmp(pSessionData->signData, pData, ulDataLen) == 0)
	{
		/** −前回Sign時の署名データを取得する。*/
		tmpSignatureLen = pSessionData->signSignatureLen;
		memcpy(tmpSignature, pSessionData->signSignature, tmpSignatureLen);
	}
	/** 上記以外の場合、*/
	else
	{
		/** −データ長不正の場合、*/
		if (ulDataLen < 65 || ulDataLen > 128)
		{
			/** −−戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
			return CKR_ARGUMENTS_BAD;
		}
		
		/** −取得したセッションデータから鍵の格納パスを取得する。*/
		rv = GetElem(pSessionData->signObject, CKA_EFID, (CK_VOID_PTR_PTR)&pAttr);
		/** −取得失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		/** −EFID長が不正の場合、*/
		if (pAttr->ulValueLen != 4)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		/** −EFIDを取得する。*/
		memcpy(efid, pAttr->pValue, pAttr->ulValueLen);
		
		/** −鍵の実データ格納パスへのSELECT FILEコマンドを実行する。*/
		rcvLen = sizeof(rcvBuf);
		rv = SelectFileEF(pSlotData->hCard, &(pSlotData->dwActiveProtocol), &efid[2], 2, rcvBuf, &rcvLen);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値を設定し処理を抜ける。*/
			return rv;
		}
		
		/** −鍵の実データ格納パスへのINTERNAL AUTHENTICATEコマンドを実行する。*/
		rcvLen = sizeof(rcvBuf);
		rv = InternalAuth(pSlotData->hCard, &(pSlotData->dwActiveProtocol), pData, ulDataLen, rcvBuf, &rcvLen);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値を設定し処理を抜ける。*/
			return rv;
		}
		
		/** −署名データを取得する。*/
		tmpSignatureLen = rcvLen - 2;
		memcpy(tmpSignature, rcvBuf, tmpSignatureLen);
	}
	
	/** 引数のpSignatureが0x00の場合、*/
	if (pSignature == CK_NULL_PTR)
	{
		/** −署名データ長を引数のpulSignatureLenに設定する。*/
		*pulSignatureLen = tmpSignatureLen;
		
		/** −データをセッションに格納する。*/
		memcpy(pSessionData->signData, pData, ulDataLen);
		pSessionData->signDataLen = ulDataLen;
		
		/** −署名データをセッションに格納する。*/
		memcpy(pSessionData->signSignature, tmpSignature, tmpSignatureLen);
		pSessionData->signSignatureLen = tmpSignatureLen;
	}
	/** 上記以外の場合、*/
	else
	{
		/** −署名データ長不足の場合、*/
		if (*pulSignatureLen < tmpSignatureLen)
		{
			/** −−戻り値にバッファ長不足（CKR_BUFFER_TOO_SMALL）を設定し処理を抜ける。*/
			return CKR_BUFFER_TOO_SMALL;
		}
		/** −署名データ長を引数のpulSignatureLenに設定する。*/
		*pulSignatureLen = tmpSignatureLen;
		
		/** −署名データを引数のpSignatureに設定する。*/
		memcpy(pSignature, tmpSignature, tmpSignatureLen);
		
		/** −データと署名データをセッションからクリアする。*/
		memset(pSessionData->signData, 0, sizeof(pSessionData->signData));
		pSessionData->signDataLen = 0;
		memset(pSessionData->signSignature, 0, sizeof(pSessionData->signSignature));
		pSessionData->signSignatureLen = 0;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * ダイジェスト作成を開始する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pMechanism メカニズム情報ポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_MECHANISM_INVALID:メカニズム不正 CKR_MECHANISM_PARAM_INVALID:メカニズムパラメータ不正 CKR_OPERATION_ACTIVE:オペレーション活性 CKR_PIN_EXPIRED:パスワード期限切れ CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_USER_NOT_LOGGED_IN:ユーザ未ログイン CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_DigestInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** 引数のpMechanismのメカニズムタイプが対応可能かチェックする。*/
	switch (pMechanism->mechanism)
	{
	/** 対応可能な場合、*/
	case CKM_SHA_1:
	case CKM_SHA256:
	case CKM_SHA512:
		/** −チェック終了。*/
		break;
	/** 対応可能でない場合、*/
	default:
		/** −戻り値にメカニズム不正（CKR_MECHANISM_INVALID）を設定し処理を抜ける。*/
		return CKR_MECHANISM_INVALID;
		break;
	}
	
	/** 取得したメカニズムタイプをセッションデータに設定する。*/
	pSessionData->digestMechanism = pMechanism->mechanism;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * ダイジェストを作成する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pPart ハッシュするデータポインタ
 * @param ulPartLen ハッシュするデータ長
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_NOT_INITIALIZED:オペレーション未初期化 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** C_DigestInit未実行の場合、*/
	if (pSessionData->digestMechanism == 0)
	{
		/** −戻り値にオペレーション未初期化（CKR_OPERATION_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_OPERATION_NOT_INITIALIZED;
	}
	
	/** ハッシュするデータを取得する。*/
	memcpy(pSessionData->digestInData, pPart, ulPartLen);
	pSessionData->digestInDataLen = ulPartLen;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * ダイジェスト作成を終了する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pDigest ダイジェストデータポインタ
 * @param pulDigestLen ダイジェストデータ長ポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_BUFFER_TOO_SMALL:バッファ長不足 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_NOT_INITIALIZED:オペレーション未初期化 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest, CK_ULONG_PTR pulDigestLen)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_LONG /*lalgorithm,*/ tmpDataLen, lRet;
	CK_BYTE tmpData[512];
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** C_DigestInit未実行の場合、*/
	if (pSessionData->digestMechanism == 0)
	{
		/** −戻り値にオペレーション未初期化（CKR_OPERATION_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_OPERATION_NOT_INITIALIZED;
	}
	
	/** ハッシュするデータ未取得の場合、*/
	if (pSessionData->digestInDataLen == 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 引数のpDigestが0x00の場合、*/
	if (pDigest == CK_NULL_PTR)
	{
		/** −メカニズムタイプに応じてダイジェスト長（固定値）を、引数のpulDigestLenに設定する。*/
		switch (pSessionData->digestMechanism)
		{
		case CKM_SHA_1:
			*pulDigestLen = 20;
			break;
		case CKM_SHA256:
			*pulDigestLen = 32;
			break;
		case CKM_SHA512:
			*pulDigestLen = 64;
			break;
		/** −対応可能でない場合、*/
		default:
			/** −−戻り値にメカニズム不正（CKR_MECHANISM_INVALID）を設定し処理を抜ける。*/
			return CKR_MECHANISM_INVALID;
			break;
		}
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	
	/** メカニズムタイプ及び入力データを基に、OpenSSL関数を呼び、ダイジェスト作成結果を取得する。*/
	memset(tmpData, 0, sizeof(tmpData));
	tmpDataLen = sizeof(tmpData);
	lRet = IDMan_CmMkHash(pSessionData->digestInData, pSessionData->digestInDataLen, pSessionData->digestMechanism, tmpData, &tmpDataLen);
	/** 失敗の場合、*/
	if (lRet != 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** ダイジェスト作成結果を引数に設定する。*/
	memcpy(pDigest, tmpData, tmpDataLen);
	*pulDigestLen = tmpDataLen;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 署名検証を開始する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pMechanism メカニズム情報ポインタ
 * @param hKey 鍵オブジェクトハンドル
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_KEY_FUNCTION_NOT_PERMITTED:関数不許可 CKR_KEY_HANDLE_INVALID:鍵ハンドル不正 CKR_KEY_SIZE_RANGE:鍵長不正 CKR_KEY_TYPE_INCONSISTENT:鍵種別が一致しない CKR_MECHANISM_INVALID:メカニズム不正 CKR_MECHANISM_PARAM_INVALID:メカニズムパラメータ不正 CKR_OPERATION_ACTIVE:オペレーション活性 CKR_PIN_EXPIRED:パスワード期限切れ CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_USER_NOT_LOGGED_IN:ユーザ未ログイン CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_VerifyInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 取得したスロットデータからトークンデータを取得する。*/
	/** トークンデータがない場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 引数のhKeyを基に、オブジェクトテーブルから該当する鍵オブジェクトを検索する。*/
	rv = GetElem(pSlotData->tokenData->objectTable, hKey, (CK_VOID_PTR_PTR)&pObject);
	/** 該当する鍵がない場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に鍵ハンドル不正（CKR_KEY_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_KEY_HANDLE_INVALID;
	}
	
	/** セッションデータに鍵オブジェクトを設定する。*/
	pSessionData->verifyObject = pObject;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 署名を検証する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pData 検証するデータポインタ
 * @param ulDataLen 検証するデータ長
 * @param pSignature 署名データポインタ
 * @param ulSignatureLen 署名データ長
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DATA_INVALID:データ不正 CKR_DATA_LEN_RANGE:データ長不正 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_FUNCTION_CANCELED:関数がキャンセルされた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OPERATION_NOT_INITIALIZED:オペレーション未初期化 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_SIGNATURE_INVALID:署名データ不正 CKR_SIGNATURE_LEN_RANGE:署名データ長不正 CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG ulSignatureLen)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_BYTE decData[2048], tmpData[2048];
	CK_LONG decDataLen, lRet;

	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** C_VerifyInit未実行の場合、*/
	if (pSessionData->verifyObject == CK_NULL_PTR)
	{
		/** −戻り値にオペレーション未初期化（CKR_OPERATION_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_OPERATION_NOT_INITIALIZED;
	}
	
	/** 取得したセッションデータから鍵を取得する。*/
	rv = GetElem(pSessionData->verifyObject, (CK_ULONG)CKA_VALUE, (CK_VOID_PTR_PTR)&pAttr);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	
	/** 取得した鍵を基に、OpenSSL関数を呼び、引数のpSignatureの復号処理を行う。*/
	lRet = IDMan_CmChgRSA( pAttr->pValue, pAttr->ulValueLen, pSignature, ulSignatureLen, decData, &decDataLen);
	/** 失敗の場合、*/
	if (lRet != 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 引数のulDataLenが復号したデータ長よりも大きい場合、*/
	if (ulDataLen > decDataLen)
	{
		/** −戻り値にデータ長不正（CKR_DATA_LEN_RANGE）を設定し処理を抜ける。*/
		return CKR_DATA_LEN_RANGE;
	}
	
	/** 引数のulDataLenが復号したデータ長よりも小さい場合、*/
	if (ulDataLen < decDataLen)
	{
		/** −引数のpDataに先頭から0パディングを行い、データ長を揃える。*/
		memset(tmpData, 0x00, sizeof(tmpData));
		memcpy(&tmpData[decDataLen-ulDataLen], pData, ulDataLen);
	}
	else
	{
		memset(tmpData, 0x00, sizeof(tmpData));
		memcpy(tmpData, pData, ulDataLen);
	}
	
	/** 復号したデータとパディング後のpDataが一致しない場合、*/
	if (memcmp(tmpData, decData, decDataLen) != 0)
	{
		/** −戻り値に署名データ不正（CKR_SIGNATURE_INVALID）を設定し処理を抜ける。*/
		return CKR_SIGNATURE_INVALID;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * オブジェクトを作成する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param pTemplate 属性テーブルポインタ
 * @param ulCount 属性テーブル数
 * @param phObject オブジェクトハンドルポインタ
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_ATTRIBUTE_READ_ONLY:属性は読取りのみ CKR_ATTRIBUTE_TYPE_INVALID:属性タイプ不正 CKR_ATTRIBUTE_VALUE_INVALID:属性値不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_DOMAIN_PARAMS_INVALID:ドメインパラメータ不正 CKR_HOST_MEMORY:ホストメモリ不足 CKR_PIN_EXPIRED:パスワード期限切れ CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_SESSION_READ_ONLY:セッションは読取りのみ CKR_TEMPLATE_INCOMPLETE:テンプレート不完全 CKR_TEMPLATE_INCONSISTENT:テンプレート不一致 CKR_TOKEN_WRITE_PROTECTED:ICカードはライトプロテクトされている CKR_USER_NOT_LOGGED_IN:ユーザ未ログイン CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_CreateObject(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phObject)
{
	CK_RV rv = CKR_OK;
	CK_ULONG rvAttr;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_VOID_PTR pAttrValue = CK_NULL_PTR;
	CK_ULONG i;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** セッションタイプが読出しのみの場合、*/
	if (pSessionData->sessionInfo->state == CKS_RO_PUBLIC_SESSION ||
		pSessionData->sessionInfo->state == CKS_RO_USER_FUNCTIONS)
	{
		/** −戻り値にセッションは読取りのみ（CKR_SESSION_READ_ONLY）を設定し処理を抜ける。*/
		return CKR_SESSION_READ_ONLY;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** スロットデータ内のトークンデータが0x00の場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが抜かれた（CKR_DEVICE_REMOVED）を設定し処理を抜ける。*/
		return CKR_DEVICE_REMOVED;
	}
	
	/** オブジェクトテーブルが0x00または0件の場合、*/
	if (pSlotData->tokenData->objectTable == CK_NULL_PTR || pSlotData->tokenData->objectTable->num == 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** オブジェクトを作成する。*/
	pObject = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (pObject == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pObject), 0, sizeof(CK_I_HEAD));
	
	/** 属性テーブル数分繰り返す。*/
	for (i = 0; i < ulCount; i++)
	{
		/** −属性値を作成する。*/
		pAttrValue = CK_NULL_PTR;
		/** −属性値のサイズが0より大の場合、*/
		if (pTemplate[i].ulValueLen > 0)
		{
			/** −−属性タイプに応じて属性値を取得する。*/
			rvAttr = GetAttrValType(pTemplate[i].type);
			switch (rvAttr)
			{
			/** −−以下は数字として取得する。*/
			case ATTR_VAL_TYPE_NUM:
				pAttrValue = (CK_VOID_PTR)pTemplate[i].pValue;
				break;
			/** −−以下は1バイトとして取得する。*/
			case ATTR_VAL_TYPE_BYTE:
				/** −−−属性情報の値の領域を確保する。*/
				pAttrValue = IDMan_StMalloc(pTemplate[i].ulValueLen);
				/** 失敗の場合、*/
				if (pAttrValue == CK_NULL_PTR)
				{
					/** −−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
					return CKR_HOST_MEMORY;
				}
				memcpy(pAttrValue, pTemplate[i].pValue, pTemplate[i].ulValueLen);
				break;
			/** −−以下はバイト配列として取得する。*/
			case ATTR_VAL_TYPE_BYTE_ARRAY:
				/** −−−属性情報の値の領域を確保する。*/
				pAttrValue = IDMan_StMalloc(pTemplate[i].ulValueLen);
				/** −−−失敗の場合、*/
				if (pAttrValue == CK_NULL_PTR)
				{
					/** −−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
					return CKR_HOST_MEMORY;
				}
				memcpy(pAttrValue, pTemplate[i].pValue, pTemplate[i].ulValueLen);
				break;
			/** −−上記以外、*/
			default:
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
				break;
			}
		}
		
		/** −属性を作成する。*/
		pAttr = CK_NULL_PTR;
		pAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
		/** −失敗の場合、*/
		if (pAttr == CK_NULL_PTR)
		{
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset((void*)pAttr, 0, sizeof(CK_ATTRIBUTE));
		pAttr->type = pTemplate[i].type;
		pAttr->ulValueLen = pTemplate[i].ulValueLen;
		pAttr->pValue = pAttrValue;
		
		
		/** −属性をオブジェクトに追加する。*/
		rv = SetElem(pObject, pAttr->type, (CK_VOID_PTR)pAttr, VAL_TYPE_ATTRIBUTE);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			IDMan_StFree(pAttr->pValue);
			IDMan_StFree(pAttr);
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
	}
	
	/** オブジェクトをテーブルに追加する。*/
	rv = SetElem(pSlotData->tokenData->objectTable, hObjectCnt, (CK_VOID_PTR)pObject, VAL_TYPE_HEAD);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		DestroyList(pObject);
		pObject = CK_NULL_PTR;
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	
	/** オブジェクトハンドルを引数のphObjectに設定する。*/
	*phObject = hObjectCnt++;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * オブジェクトを破棄する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param hObject オブジェクトハンドル
 * @return CK_RV CKR_OK:成功 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OBJECT_HANDLE_INVALID:オブジェクトハンドル不正 CKR_PIN_EXPIRED:パスワード期限切れ CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_SESSION_READ_ONLY:セッションは読取りのみ CKR_TOKEN_WRITE_PROTECTED:ICカードはライトプロテクトされている CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** セッションタイプが読出しのみの場合、*/
	if (pSessionData->sessionInfo->state == CKS_RO_PUBLIC_SESSION ||
		pSessionData->sessionInfo->state == CKS_RO_USER_FUNCTIONS)
	{
		/** −戻り値にセッションは読取りのみ（CKR_SESSION_READ_ONLY）を設定し処理を抜ける。*/
		return CKR_SESSION_READ_ONLY;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** スロットデータ内のトークンデータが0x00の場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが抜かれた（CKR_DEVICE_REMOVED）を設定し処理を抜ける。*/
		return CKR_DEVICE_REMOVED;
	}
	
	/** オブジェクトテーブルが0x00または0件の場合、*/
	if (pSlotData->tokenData->objectTable == CK_NULL_PTR || pSlotData->tokenData->objectTable->num == 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** オブジェクトテーブルから、引数のオブジェクトハンドルを基に該当するオブジェクトを削除する。*/
	rv = DelElem(pSlotData->tokenData->objectTable, hObject);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * オブジェクト属性値を設定する関数
 * @author University of Tsukuba
 * @param hSession セッションハンドル
 * @param hObject オブジェクトハンドル
 * @param pTemplate 属性テーブルポインタ
 * @param ulCount 属性テーブル数
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_ATTRIBUTE_READ_ONLY:属性は読取りのみ CKR_ATTRIBUTE_TYPE_INVALID:属性タイプ不正 CKR_ATTRIBUTE_VALUE_INVALID:属性値不正 CKR_CRYPTOKI_NOT_INITIALIZED:cryptoki未初期化 CKR_DEVICE_ERROR:デバイスエラー CKR_DEVICE_MEMORY:デバイスメモリ不足 CKR_DEVICE_REMOVED:カードが抜かれた CKR_HOST_MEMORY:ホストメモリ不足 CKR_OBJECT_HANDLE_INVALID:オブジェクトハンドル不正 CKR_SESSION_CLOSED:セッション終了 CKR_SESSION_HANDLE_INVALID:セッションハンドル不正 CKR_SESSION_READ_ONLY:セッションは読取りのみ CKR_TEMPLATE_INCONSISTENT:テンプレート不一致 CKR_TOKEN_WRITE_PROTECTED:ICカードはライトプロテクトされている CKR_USER_NOT_LOGGED_IN:ユーザ未ログイン CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV C_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_I_HEAD_PTR pIdList = CK_NULL_PTR;
	CK_I_HEAD_PTR pPassList = CK_NULL_PTR;
	CK_I_HEAD_PTR pIdCopyList = CK_NULL_PTR;
	CK_I_HEAD_PTR pPassCopyList = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR elemAttr = CK_NULL_PTR;
	CK_BBOOL hitFlg = CK_FALSE;
	CK_ULONG index = 0;
	CK_BBOOL updateFlg = CK_FALSE;
	CK_ATTRIBUTE_PTR pIdAttr = CK_NULL_PTR;
	CK_ATTRIBUTE_PTR pPassAttr = CK_NULL_PTR;
	CK_BYTE rcvBuf[1024], data[2048], idPass[2048], tmp[2048], efid[4];
	CK_ULONG rcvLen, dataLen, idPassLen, tmpLen, num, i;
	CK_ULONG numVal/*, numValSrch*/;
	CK_BYTE byteVal[256]/*, byteValSrch[256]*/;
	
	/** 初期化フラグがFALSEの場合、*/
	if (libInitFlag == CK_FALSE)
	{
		/** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
		return CKR_CRYPTOKI_NOT_INITIALIZED;
	}
	
	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
	rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_SESSION_HANDLE_INVALID;
	}
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
	rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** スロットデータ内のトークンデータが0x00の場合、*/
	if (pSlotData->tokenData == CK_NULL_PTR)
	{
		/** −戻り値にカードが抜かれた（CKR_DEVICE_REMOVED）を設定し処理を抜ける。*/
		return CKR_DEVICE_REMOVED;
	}
	
	/** オブジェクトテーブルが0x00または0件の場合、*/
	if (pSlotData->tokenData->objectTable == CK_NULL_PTR || pSlotData->tokenData->objectTable->num == 0)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** オブジェクトテーブルから、引数のオブジェクトハンドルを基に該当するオブジェクトを取得する。*/
	rv = GetElem(pSlotData->tokenData->objectTable, hObject, (CK_VOID_PTR_PTR)&pObject);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値にオブジェクトハンドル不正（CKR_OBJECT_HANDLE_INVALID）を設定し処理を抜ける。*/
		return CKR_OBJECT_HANDLE_INVALID;
	}
	
	/** オブジェクトから、Private属性情報を取得する。*/
	rv = GetElem(pObject, CKA_PRIVATE, (CK_VOID_PTR_PTR)&pAttr);
	/** Private属性情報ありの場合、*/
	if (rv == CKR_OK)
	{
		/** −Private属性値を取得する。*/
		memset(byteVal, 0, sizeof(byteVal));
		memcpy(byteVal, pAttr->pValue, pAttr->ulValueLen);
		
		/** −プライベートオブジェクトかつログイン済みでない場合、*/
		if ( byteVal[0] == CK_TRUE &&
				(pSessionData->sessionInfo->state == CKS_RO_PUBLIC_SESSION ||
				pSessionData->sessionInfo->state == CKS_RW_PUBLIC_SESSION) )
		{
			/** −−戻り値にオブジェクトハンドル不正（CKR_OBJECT_HANDLE_INVALID）を設定し処理を抜ける。*/
			return CKR_OBJECT_HANDLE_INVALID;
		}
	}
	/** Private属性情報なしの場合、*/
	else
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** オブジェクトから、Class属性情報を取得する。*/
	rv = GetElem(pObject, CKA_CLASS, (CK_VOID_PTR_PTR)&pAttr);
	/** Class属性情報ありの場合、*/
	if (rv == CKR_OK)
	{
		/** −Class属性値を取得する。*/
		memcpy((void*)&numVal, pAttr->pValue, sizeof(numVal));
		/** −データクラスでない場合、*/
		if (numVal != CKO_DATA)
		{
			/** −−戻り値にオブジェクトハンドル不正（CKR_OBJECT_HANDLE_INVALID）を設定し処理を抜ける。*/
			return CKR_OBJECT_HANDLE_INVALID;
		}
	}
	/** Class属性情報なしの場合、*/
	else
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** オブジェクトからIDのリストを取得する。*/
	rv = GetElem(pObject, CKA_VALUE_ID, (CK_VOID_PTR_PTR)&pIdList);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** オブジェクトからパスワードのリストを取得する。*/
	rv = GetElem(pObject, CKA_VALUE_PASSWORD, (CK_VOID_PTR_PTR)&pPassList);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	
	/** IDのコピーリストを作成する。*/
	pIdCopyList = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (pIdCopyList == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pIdCopyList), 0, sizeof(CK_I_HEAD));
	
	/** IDのリストをコピーする。*/
	elem = pIdList->elem;
	/** ID数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −IDを取得する。*/
		elemAttr = (CK_ATTRIBUTE_PTR)elem->val;
		
		/** −IDのコピーを作成する。*/
		pIdAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
		/** −失敗の場合、*/
		if (pIdAttr == CK_NULL_PTR)
		{
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset((void*)pIdAttr, 0, sizeof(CK_ATTRIBUTE));
		pIdAttr->type = elemAttr->type;
		pIdAttr->ulValueLen = elemAttr->ulValueLen;
		/** −IDコピーの属性値を作成する。*/
		pIdAttr->pValue = IDMan_StMalloc(elemAttr->ulValueLen);
		/** −失敗の場合、*/
		if (pIdAttr->pValue == CK_NULL_PTR)
		{
			IDMan_StFree(pIdAttr);
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset(pIdAttr->pValue, 0, elemAttr->ulValueLen);
		memcpy(pIdAttr->pValue, elemAttr->pValue, elemAttr->ulValueLen);
		
		/** −IDをコピーリストに追加する。*/
		rv = AddElem(pIdCopyList, elem->key, (CK_VOID_PTR)pIdAttr, VAL_TYPE_ATTRIBUTE);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	
	/** パスワードのコピーリストを作成する。*/
	pPassCopyList = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (pPassCopyList == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pPassCopyList), 0, sizeof(CK_I_HEAD));
	
	/** パスワードのリストをコピーする。*/
	elem = pPassList->elem;
	/** パスワード数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −パスワードを取得する。*/
		elemAttr = (CK_ATTRIBUTE_PTR)elem->val;
		
		/** −パスワードのコピーを作成する。*/
		pPassAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
		/** −失敗の場合、*/
		if (pPassAttr == CK_NULL_PTR)
		{
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset((void*)pPassAttr, 0, sizeof(CK_ATTRIBUTE));
		pPassAttr->type = elemAttr->type;
		pPassAttr->ulValueLen = elemAttr->ulValueLen;
		/** −パスワードコピーの属性値を作成する。*/
		pPassAttr->pValue = IDMan_StMalloc(elemAttr->ulValueLen);
		/** −失敗の場合、*/
		if (pPassAttr->pValue == CK_NULL_PTR)
		{
			IDMan_StFree(pPassAttr);
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			return CKR_HOST_MEMORY;
		}
		memset(pPassAttr->pValue, 0, elemAttr->ulValueLen);
		memcpy(pPassAttr->pValue, elemAttr->pValue, elemAttr->ulValueLen);
		
		/** −パスワードをコピーリストに追加する。*/
		rv = AddElem(pPassCopyList, elem->key, (CK_VOID_PTR)pPassAttr, VAL_TYPE_ATTRIBUTE);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			IDMan_StFree(pPassAttr->pValue);
			IDMan_StFree(pPassAttr);
			return rv;
		}
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	
	/** 属性テーブル数分繰り返す。*/
	for (i = 0; i < ulCount; i++)
	{
		/** −属性がIDの場合、*/
		if (pTemplate[i].type == CKA_VALUE_ID)
		{
			/** −−IDが0x00またはサイズ0の場合、*/
			if (pTemplate[i].pValue == CK_NULL_PTR || pTemplate[i].ulValueLen <= 0)
			{
				/** −−−戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
				return CKR_ARGUMENTS_BAD;
			}
			
			/** −−次の属性があり、かつパスワードの場合、*/
			if (i+1 < ulCount && pTemplate[i+1].type == CKA_VALUE_PASSWORD)
			{
				/** −−−IDのリストに、既に同じIDが存在するか検索する。*/
				elem = pIdCopyList->elem;
				/** −−−ID数分以下を繰り返す。*/
				while (elem != CK_NULL_PTR)
				{
					/** −−−−番号を取得する。*/
					index = elem->key;
					
					/** −−−−IDを取得する。*/
					elemAttr = (CK_ATTRIBUTE_PTR)elem->val;
					
					/** −−−−IDが等しい場合、*/
					if (pTemplate[i].ulValueLen == elemAttr->ulValueLen &&
						memcmp(pTemplate[i].pValue, elemAttr->pValue, pTemplate[i].ulValueLen) == 0)
					{
						/** −−−−−フラグをONにする。*/
						hitFlg = CK_TRUE;
						/** −−−−−検索終了。*/
						break;
					}
					/** −−−−次の要素を取得する。*/
					elem = elem->next;
				}
				
				/** −−−パスワードが0x00でなく、かつサイズが0より大の場合、*/
				if (pTemplate[i+1].pValue != CK_NULL_PTR && pTemplate[i+1].ulValueLen > 0)
				{
					/** −−−−パスワード属性を作成する。*/
					pPassAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
					/** −−−−失敗の場合、*/
					if (pPassAttr == CK_NULL_PTR)
					{
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset((void*)pPassAttr, 0, sizeof(CK_ATTRIBUTE));
					pPassAttr->type = pTemplate[i+1].type;
					pPassAttr->ulValueLen = pTemplate[i+1].ulValueLen;
					/** −−−−パスワード属性値を作成する。*/
					pPassAttr->pValue = IDMan_StMalloc(pTemplate[i+1].ulValueLen);
					/** −−−−失敗の場合、*/
					if (pPassAttr->pValue == CK_NULL_PTR)
					{
						IDMan_StFree(pPassAttr);
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset(pPassAttr->pValue, 0, pTemplate[i+1].ulValueLen);
					memcpy(pPassAttr->pValue, pTemplate[i+1].pValue, pTemplate[i+1].ulValueLen);
				}
				
				/** −−−同じIDが存在する場合、*/
				if (hitFlg == CK_TRUE)
				{
					/** −−−−パスワードが0x00でなく、かつサイズが0より大の場合、*/
					if (pTemplate[i+1].pValue != CK_NULL_PTR && pTemplate[i+1].ulValueLen > 0)
					{
						/** −−−−−パスワードを更新する。*/
						rv = SetElem(pPassCopyList, index, (CK_VOID_PTR)pPassAttr, VAL_TYPE_ATTRIBUTE);
						/** 失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
					}
					/** −−−−パスワードが0x00の場合、*/
					else if (pTemplate[i+1].pValue == CK_NULL_PTR)
					{
						/** −−−−−IDを削除する。*/
						rv = DelElem(pIdCopyList, index);
						/** 失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
						/** −−−−−パスワードを削除する。*/
						rv = DelElem(pPassCopyList, index);
						/** 失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
					}
					/** −−−−上記以外の場合、*/
					else
					{
						/** −−−−−戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
						return CKR_ARGUMENTS_BAD;
					}
				}
				/** −−−同じIDが存在しない場合、*/
				else
				{
					/** −−−−パスワードが0x00でなく、かつサイズが0より大の場合、*/
					if (pTemplate[i+1].pValue != CK_NULL_PTR && pTemplate[i+1].ulValueLen > 0)
					{
						/** −−−−−番号を進める。*/
						index++;
						/** −−−−−ID属性を作成する。*/
						pIdAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
						/** −−−−−失敗の場合、*/
						if (pIdAttr == CK_NULL_PTR)
						{
							/** −−−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
							return CKR_HOST_MEMORY;
						}
						memset((void*)pIdAttr, 0, sizeof(CK_ATTRIBUTE));
						pIdAttr->type = pTemplate[i].type;
						pIdAttr->ulValueLen = pTemplate[i].ulValueLen;
						/** −−−−−ID属性値を作成する。*/
						pIdAttr->pValue = IDMan_StMalloc(pTemplate[i].ulValueLen);
						/** −−−−−失敗の場合、*/
						if (pIdAttr->pValue == CK_NULL_PTR)
						{
							IDMan_StFree(pIdAttr);
							/** −−−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
							return CKR_HOST_MEMORY;
						}
						memset(pIdAttr->pValue, 0, pTemplate[i].ulValueLen);
						memcpy(pIdAttr->pValue, pTemplate[i].pValue, pTemplate[i].ulValueLen);
						
						/** −−−−−パスワードを追加する。*/
						rv = AddElem(pPassCopyList, index, (CK_VOID_PTR)pPassAttr, VAL_TYPE_ATTRIBUTE);
						/** 失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
						
						/** −−−−−IDを追加する。*/
						rv = AddElem(pIdCopyList, index, (CK_VOID_PTR)pIdAttr, VAL_TYPE_ATTRIBUTE);
						/** 失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−パスワードを削除する。*/
							DelElem(pPassCopyList, index);
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
					}
					/** −−−−上記以外の場合、*/
					else
					{
						/** −−−−−戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
						return CKR_ARGUMENTS_BAD;
					}
				}
				/** −−−データ更新フラグをONにする。*/
				updateFlg = CK_TRUE;
			}
		}
	}
	
	/** データが更新された場合、*/
	if (updateFlg == CK_TRUE)
	{
		idPassLen = 0;
		elem = pIdCopyList->elem;
		/** −ID数分以下を繰り返す。*/
		while (elem != CK_NULL_PTR)
		{
			/** −−番号を取得する。*/
			index = elem->key;
			
			/** −−IDを取得する。*/
			pIdAttr = (CK_ATTRIBUTE_PTR)elem->val;
			
			/** −−対応するパスワードを取得する。*/
			rv = GetElem(pPassCopyList, index, (CK_VOID_PTR_PTR)&pPassAttr);
			/** −−取得成功の場合、*/
			if (rv == CKR_OK)
			{
				tmpLen = 0;
				/** −−−IDのTLVを作成する。*/
				tmp[tmpLen++] = 0x0C;
				/** −−−Lengthを設定する。*/
				rv = SetLength(&tmp[tmpLen], pIdAttr->ulValueLen, &num);
				/** 失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値をそのまま設定し処理を抜ける。*/
					return rv;
				}
				tmpLen += num+1;
				memcpy(&tmp[tmpLen], pIdAttr->pValue, pIdAttr->ulValueLen);
				tmpLen += pIdAttr->ulValueLen;
				
				/** −−−パスワードのTLVを作成する。*/
				tmp[tmpLen++] = 0x0C;
				/** −−−Lengthを設定する。*/
				rv = SetLength(&tmp[tmpLen], pPassAttr->ulValueLen, &num);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値をそのまま設定し処理を抜ける。*/
					return rv;
				}
				tmpLen += num+1;
				memcpy(&tmp[tmpLen], pPassAttr->pValue, pPassAttr->ulValueLen);
				tmpLen += pPassAttr->ulValueLen;
				
				
				/** −−−ID/PASSのTLVを作成する。*/
				idPass[idPassLen++] = 0xA0;
				/** −−−Lengthを設定する。*/
				rv = SetLength(&idPass[idPassLen], tmpLen, &num);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値をそのまま設定し処理を抜ける。*/
					return rv;
				}
				idPassLen += num+1;
				memcpy(&idPass[idPassLen], tmp, tmpLen);
				idPassLen += tmpLen;
			}
			/** −−次の要素を取得する。*/
			elem = elem->next;
		}
		/** −先頭のTLを設定し、カードデータを作成する。*/
		memset(data, 0xFF, sizeof(data));
		dataLen = 0;
		data[dataLen++] = 0x30;
		
		/** −Lengthを設定する。*/
		rv = SetLength(&data[dataLen], idPassLen, &num);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
		dataLen += num+1;
		memcpy(&data[dataLen], idPass, idPassLen);
		dataLen += idPassLen;
		
		/** −データサイズ超過の場合、*/
		if (dataLen > MAX_SIZE_ID_PASS)
		{
			/** −−戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
			return CKR_ARGUMENTS_BAD;
		}
		
		
		/** −オブジェクトからEFID属性を取得する。*/
		rv = GetElem(pObject, CKA_EFID, (CK_VOID_PTR_PTR)&pAttr);
		/** 取得失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		/** −EFID長が不正の場合、*/
		if (pAttr->ulValueLen != 4)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		/** −EFIDを取得する。*/
		memcpy(efid, pAttr->pValue, pAttr->ulValueLen);
		
		/** −ID/パスワード実データ格納パスへのSELECT FILEコマンドを実行する。*/
		rcvLen = sizeof(rcvBuf);
		rv = SelectFileEF(pSlotData->hCard, &(pSlotData->dwActiveProtocol), &efid[2], 2, rcvBuf, &rcvLen);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値を設定し処理を抜ける。*/
			return rv;
		}
		
		/** −ID/パスワード実データ格納パスへのUPDATE BINARYコマンドを実行する。*/
		rcvLen = sizeof(rcvBuf);
		rv = UpdateBinary(pSlotData->hCard, &(pSlotData->dwActiveProtocol), data, MAX_SIZE_ID_PASS, rcvBuf, &rcvLen);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
			return CKR_FUNCTION_FAILED;
		}
		
		/** −オブジェクトから現在のIDリストを削除する。*/
		rv = DelElem(pObject, CKA_VALUE_ID);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
		
		/** −オブジェクトから現在のパスワードリストを削除する。*/
		rv = DelElem(pObject, CKA_VALUE_PASSWORD);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
		
		/** −オブジェクトにIDコピーリストを追加する。*/
		rv = AddElem(pObject, CKA_VALUE_ID, (CK_VOID_PTR)pIdCopyList, VAL_TYPE_HEAD);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
		
		/** −オブジェクトにパスワードコピーリストを追加する。*/
		rv = AddElem(pObject, CKA_VALUE_PASSWORD, (CK_VOID_PTR)pPassCopyList, VAL_TYPE_HEAD);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			return rv;
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}

CK_RV C_GetCardStatus(CK_SESSION_HANDLE hSession)
{
	CK_RV rv = CKR_OK;
	CK_I_SESSION_DATA_PTR pSessionData = CK_NULL_PTR;
	CK_I_SLOT_DATA_PTR pSlotData = CK_NULL_PTR;

	/** 初期化フラグがFALSEの場合、*/
        if (libInitFlag == CK_FALSE)
        {
                /** −戻り値にcryptoki未初期化（CKR_CRYPTOKI_NOT_INITIALIZED）を設定し処理を抜ける。*/
                return CKR_CRYPTOKI_NOT_INITIALIZED;
        }

	/** セッションテーブルから、引数のセッションハンドルを基に該当するセッションデータを取得する。*/
        rv = GetElem(sessionTable, hSession, (CK_VOID_PTR_PTR)&pSessionData);
        /** 取得失敗の場合、*/
        if (rv != CKR_OK)
        {
                /** −戻り値にセッションハンドル不正（CKR_SESSION_HANDLE_INVALID）を設定し処理を抜ける。*/
                return CKR_SESSION_HANDLE_INVALID;
        }
	
	/** スロットテーブルから、セッションデータのスロットIDを基に該当するスロットデータを取得する。*/
        rv = GetElem(slotTable, pSessionData->sessionInfo->slotID, (CK_VOID_PTR_PTR)&pSlotData);
        /** 取得失敗の場合、*/
        if (rv != CKR_OK)
        {
                /** −戻り値にスロットID無効（CKR_SLOT_ID_INVALID）を設定し処理を抜ける。*/
                return CKR_SLOT_ID_INVALID;
        }

	/** カードのステータス情報を取得する。*/
        rv = CardStatus(pSlotData->hCard, pSlotData->reader);
        /** 失敗の場合、*/
        if (rv != CKR_OK)
        {
                /** −戻り値にカードが挿入されていない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
                return CKR_TOKEN_NOT_PRESENT;
        }

	return rv;
}
