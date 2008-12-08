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
/*
 * Copyright (c) 1999-2003 David Corcoran <corcoran@linuxnet.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * Changes to this license can be made only by the copyright author with 
 * explicit written consent.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ID管理のPCSCライブラリ：スマートカードリーダと通信を取り扱う関数(PCSC-API)
 * \file IDMan_PcWinscard.c
 */

/**
 * @file
 * @brief This handles smartcard reader communications.
 * This is the heart of the M$ smartcard API.
 *
 * @since 2008.02
 * @version 1.0
 */

#include "IDMan_StandardIo.h"

#include "IDMan_PcWinscard.h"
#include "IDMan_PcReaderfactory.h"
#include "IDMan_PcProthandler.h"
#include "IDMan_PcEventhandler.h"

/** used for backward compatibility */
#define SCARD_PROTOCOL_ANY_OLD	 0x1000	

#define CARD_HANDLE_LOW	0xABCD	// 一意なカード識別ID

/** Some defines for context stack. */
#define SCARD_EXCLUSIVE_CONTEXT -1

///スマートカード入出力構造体（インスタンス） プロトコル T=0
SCARD_IO_REQUEST g_rgSCardT0Pci = { SCARD_PROTOCOL_T0, 8 };
///スマートカード入出力構造体（インスタンス） プロトコル T=1
SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, 8 };

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef NULL
# define NULL	0
#endif

///アプリケーションコンテキスト構造体
static struct _psContext
{
	SCARDCONTEXT hContext;
	SCARDHANDLE hCard;
	LPSTR readerName;				// リーダ名
} psContext;

// アプリケーションコンテキスト内のリーダ名用バッファ
static char tmpReaderName[MAX_READERNAME];

/**
 * 初期化コードが１回だけ実行されるようにします。
 */
static short isExecuted = 0;


///インスタンスは IDMan_PcEventhandler.c に存在します
extern READER_STATE readerState;
///インスタンスは IDMan_PcReaderfactory.c に存在します
extern READER_CONTEXT sReaderContext;

///ホットプラグデバイス検索関数
extern LONG HPSearchHotPluggables(void);


/**
 * @brief PC/SCリソースとの通信用コンテキストを作成します
 * PC/SCアプリケーションが呼び出す最初の関数です。\n
 * 作成できるコンテキストは１つです。\n
 * 既に作成している場合は同じコンテキストを返します。
 *
 * @param[in] dwScope 接続有効範囲。
 * ローカルまたはリモートの接続等を指定します。
 * <ul>
 *   <li>SCARD_SCOPE_USER - 未使用。
 *   <li>SCARD_SCOPE_TERMINAL - 未使用。
 *   <li>SCARD_SCOPE_GLOBAL - 未使用。
 *   <li>SCARD_SCOPE_SYSTEM - ローカルマシン。
 * </ul>
 * @param[in] pvReserved1 予約。
 * @param[in] pvReserved2 予約。
 * @param[out] phContext アプリで確保したコンテキストバッファ（ハンドルを返します）
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_VALUE 有効範囲 \e dwScope が間違っています。
 * @retval SCARD_E_INVALID_PARAMETER phContextがnullです。
 * @retval SCARD_E_NO_MEMORY 既にコンテキストを作成しています。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG SCardEstablishContext(DWORD dwScope, LPCVOID pvReserved1,
	LPCVOID pvReserved2, LPSCARDCONTEXT phContext)
{
	///コンテキストバッファがNULLの場合、
	if (phContext == 0) {
		///−パラメータエラー（リターン）
		return SCARD_E_INVALID_PARAMETER;
	}

	///未定義の有効範囲が指定された場合、
	if (dwScope != SCARD_SCOPE_USER && dwScope != SCARD_SCOPE_TERMINAL &&
		dwScope != SCARD_SCOPE_SYSTEM && dwScope != SCARD_SCOPE_GLOBAL)
	{
		///−コンテキストバッファ \e *phContext に 0 を設定
		*phContext = 0;
		///−有効範囲エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}
	
	///コンテキスト作成が初回の場合、
	if (isExecuted == 0) {
		///−リーダ管理領域初期化
		{
			int i;
			//−リーダコンテキストの状態初期化
			sReaderContext.readerState = NULL;
			//−リーダ属性管理構造体初期化
			/*
			 * Zero out each value in the struct 
			 */
			for (i=0; i<MAX_READERNAME; ++i)
				readerState.readerName[i] = 0;
			for (i=0; i<MAX_ATR_SIZE; ++i)
				readerState.cardAtr[i] = 0;
			readerState.readerState = 0;
			readerState.cardAtrLength = 0;
			readerState.cardProtocol = SCARD_PROTOCOL_UNSET;
		}

		///−アプリで利用するスマートカード入出力構造体( \b SCARD_PCI_T0/SCARD_PCI_T1 )のプロトコル初期化
		g_rgSCardT0Pci.dwProtocol = SCARD_PROTOCOL_T0;
		g_rgSCardT1Pci.dwProtocol = SCARD_PROTOCOL_T1;

		///−アプリケーションコンテキストのハンドル初期化
		psContext.hContext = 0;

		/*
		 * Initially set the hcard struct to zero
		 */
		///−アプリケーションコンテキストのカードハンドル初期化
		psContext.hCard = 0;
		///−アプリケーションコンテキストのリーダ名称初期化
		psContext.readerName = NULL;

		///−初期化コード終了フラグセット
		isExecuted = 1;
	}

	///既にコンテキストを作成している場合、
	if (psContext.hContext != 0) {
		///−エラー（リターン）
		return SCARD_E_NO_MEMORY;
	}

	/*
	 * Unique identifier for this server so that it can uniquely be
	 * identified by clients and distinguished from others
	 */
	///アプリケーションコンテキストのハンドルを作成 \e *phContext
	*phContext = (PCSCLITE_SVC_IDENTITY + 0x0001);
	psContext.hContext = *phContext;

	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}

/**
 * @brief 作成したPC/SCリソースとの通信用コンテキストを破棄します。\n
 * PC/SCアプリケーションが最後に呼び出す関数です。
 *
 * @param[in] hContext 接続を閉じるコンテキスト ハンドル
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_HANDLE hContext は無効なハンドルです。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG SCardReleaseContext(SCARDCONTEXT hContext)
{
	/*
	 * Make sure this context has been opened
	 */
	///指定されたハンドルに一致するコンテキストがない場合、
	if (hContext != psContext.hContext || hContext==0) {
		///−無効なハンドル（リターン）
		return SCARD_E_INVALID_HANDLE;
	}
	
	///コンテキストのカード接続ハンドルが存在する場合、
	if (psContext.hCard != 0) {
		LONG rv;
		/*
		 * We will use SCardStatus to see if the card has been 
		 * reset there is no need to reset each time
		 * Disconnect is called 
		 */
		///−カード状態を取得 SCardStatus()
		rv = SCardStatus(psContext.hCard, 0, 0, 0, 0, 0, 0);
		///−カードがリセット、または抜かれている場合、
		if (rv == SCARD_W_RESET_CARD || rv == SCARD_W_REMOVED_CARD)
		{
			///−−実行アクションなしで接続を切ります SCardDisconnect()
			SCardDisconnect(psContext.hCard, SCARD_LEAVE_CARD);
		}
		///−その他の場合、
		else {
			///−−カードリセットで接続を切ります SCardDisconnect()
			SCardDisconnect(psContext.hCard, SCARD_RESET_CARD);
		}
		///−コンテキストのカード接続ハンドルを初期化
		psContext.hCard = 0;
	}
	///コンテキストハンドルを初期化
	psContext.hContext = 0;

	///初期化コード実行済みの場合、
	if (isExecuted)
	{
		///−リーダコンテキストを取り除く
		RFRemoveReader(sReaderContext.lpcReader);
	}
	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}

/**
 * @brief システムの現在利用可能なリーダのリストを取得します。\n
 * \e mszReaders はアプリで確保された文字列バッファへのポインタです。
 * アプリが \e mszGroups と \e mszReaders に NULL を指定した場合、
 * この関数は \e pcchReaders に必要なバッファサイズを返します。
 *
 * @param[in] hContext PC/SC接続コンテキスト。
 * @param[in] mszGroups リーダリストを取得するグループリスト（未使用）
 * @param[out] mszReaders マルチストリング形式のリーダリスト
 * @param pcchReaders [in,out] NULL(\\0)を含むバッファサイズ。
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_PARAMETER バッファサイズ指定エラー。
 * @retval SCARD_E_INVALID_HANDLE 無効なハンドルです。
 * @retval SCARD_E_INSUFFICIENT_BUFFER バッファサイズが不足しています。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG SCardListReaders(SCARDCONTEXT hContext, LPCSTR mszGroups,
	LPSTR mszReaders, LPDWORD pcchReaders)
{
	DWORD dwReadersLen;

	///リーダリストのバッファサイズが設定されていない場合、
	if (pcchReaders == NULL) {
		///−パラメータエラー（リターン）
		return SCARD_E_INVALID_PARAMETER;
	}

	///スマートカードコンテキストがオープンされていない場合、
	if (hContext != psContext.hContext || hContext == 0) {
		///−無効なハンドル（リターン）
		return SCARD_E_INVALID_HANDLE;
	}

	/**
	 * リーダ端末接続状態検索 HPSearchHotPluggables() \n
	 * （このときシステムより現在利用可能なリーダ名を取得）
	 */
	HPSearchHotPluggables();

	///リーダ名をマルチストリング形式にした場合のサイズを計算
	dwReadersLen = IDMan_StStrlen(readerState.readerName);
	if (dwReadersLen)	dwReadersLen += 2;
	
	///バッファ \e mszReaders が設定されていない、またはバッファサイズ \e *pcchReaders が 0 の場合、
	if ((mszReaders == NULL) || (*pcchReaders == 0)) {
		///−必要なバッファサイズを \e *pcchReaders に設定
		*pcchReaders = dwReadersLen;
		///−正常終了（リターン）
		return SCARD_S_SUCCESS;
	}
	///バッファサイズが足りない場合、
	if (*pcchReaders < dwReadersLen) {
		///−必要なバッファサイズを \e *pcchReaders に設定
		*pcchReaders = dwReadersLen;
		///−充分なバッファがありません（リターン）
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	/*
	 * マルチストリング形式で作成
	 */
	///リーダ名を \e mszReaders に格納（末尾の \\0 を含む）
	IDMan_StStrcpy(mszReaders, readerState.readerName);

	///格納したリーダ名の末尾 \\0 の後ろに \\0 を追加
	mszReaders[IDMan_StStrlen(readerState.readerName)+1] = '\0';	/* Add the last null */

	///取得したデータ長を \e *pcchReaders に設定
	*pcchReaders = dwReadersLen;

	///正常終了
	return SCARD_S_SUCCESS;
}

/**
 * @brief PC/SCリソース接続用コンテキストを作成します。\n
 * フレンドリ名のリーダに接続するコンテキストを作成します。\n
 * 最初の接続でカードへの電力供給とリセット処理が走ります。
 *
 * @param[in] hContext PC/SCリソース接続用コンテキスト
 * @param[in] szReader 接続するリーダ名（最大52バイト NULL( \\0 )を含む）
 * @param[in] dwShareMode 接続モード（排他／共有）
 * <ul>
 *   <li>SCARD_SHARE_SHARED - リーダを他のアプリと共有します。
 *   <li>SCARD_SHARE_EXCLUSIVE - リーダを独占使用します
 *   <li>SCARD_SHARE_DIRECT - カードなしでもリーダを直接制御します。
 *       リーダにカードがなくても、リーダに制御コマンドを送信する SCardControl() 
 *       の前に SCARD_SHARE_DIRECT を使用することができます
 * </ul>
 * @param[in] dwPreferredProtocols 希望するプロトコル
 * <ul>
 *   <li>SCARD_PROTOCOL_T0 - プロトコル T=0
 *   <li>SCARD_PROTOCOL_T1 - プロトコル T=1
 *   <li>SCARD_PROTOCOL_ANY_OLD - プロトコル T=0 & T=1
 *   <li>SCARD_PROTOCOL_RAW - メモリ型カード（未対応、エラーを返します）
 * </ul>
 * dwPreferredProtocols は接続可能なプロトコルのビットマスクです。
 * 好ましいプロトコルがないならば (SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1) を使用できます。
 * @param[out] phCard 作成されるカード接続ハンドル
 * @param[out] pdwActiveProtocol この接続で確立したプロトコル。
 *
 * @return エラーコード
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_NO_SMARTCARD カードがありません。
 * @retval SCARD_E_INVALID_PARAMETER 無効なパラメータ。
 * @retval SCARD_E_UNKNOWN_READER 不明なリーダ。
 * @retval SCARD_E_INVALID_HANDLE hContext は無効なハンドルです。
 * @retval SCARD_E_INVALID_VALUE 共有モード、プロトコル、リーダ名が無効な値です。
 * @retval SCARD_E_READER_UNAVAILABLE 指定されたリーダは利用可能な状態ではありません。
 * @retval SCARD_E_SHARING_VIOLATION 排他使用されています。
 * @retval SCARD_E_PROTO_MISMATCH 要求プロトコルは利用できません。
 * @retval SCARD_F_INTERNAL_ERROR 内部エラー。
 * @retval SCARD_W_UNRESPONSIVE_CARD 応答（制御）できないカード。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG SCardConnect(SCARDCONTEXT hContext, LPCSTR szReader,
	DWORD dwShareMode, DWORD dwPreferredProtocols, LPSCARDHANDLE phCard,
	LPDWORD pdwActiveProtocol)
{
	PREADER_CONTEXT rContext = NULL;
	DWORD dwStatus;

	/*
	 * Check for NULL parameters
	 */
	///ハンドル \e phCard またはプロトコル \e pdwActiveProtocol のバッファが指定されていない場合、
	if (phCard == NULL || pdwActiveProtocol == NULL) {
		///−パラメータエラー（リターン）
		return SCARD_E_INVALID_PARAMETER;
	}

	///ハンドル \e phCard を初期化
	*phCard = 0;

	///リーダ名 \e szReader が指定されていない場合、
	if (szReader == NULL && *szReader==0) {
		///−不明なリーダ エラー（リターン）
		return SCARD_E_UNKNOWN_READER;
	}

	///リーダ名 \e szReader が長すぎる場合、
	if (IDMan_StStrlen(szReader) >= MAX_READERNAME) {
		///−無効な値エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	///プロトコル \e dwPreferredProtocols が間違っている場合、
	if (!(dwPreferredProtocols & SCARD_PROTOCOL_T0) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_T1) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_RAW) &&
			!(dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD)) {
		///−無効な値エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	///共有モードが間違っている場合、
	if (dwShareMode != SCARD_SHARE_EXCLUSIVE &&
			dwShareMode != SCARD_SHARE_SHARED &&
			dwShareMode != SCARD_SHARE_DIRECT) {
		///−無効な値エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	///リーダ名 \e szReader がリーダコンテキストと一致しない場合、
	if (IDMan_StStrcmp(szReader, sReaderContext.lpcReader) != 0) {
		///−不明なリーダ エラー（リターン）
		return SCARD_E_UNKNOWN_READER;
	}
	rContext = &sReaderContext;

	///カード状態遷移処理 EHStatusHandler()
	EHStatusHandler(rContext);

	/*
	 * Make sure the reader is working properly
	 */
	///リーダが利用可能な状態ではない場合、
	if ((rContext->readerState == NULL)
		|| (rContext->readerState->readerState & SCARD_UNKNOWN)) {
		///−エラーを返す（リターン）
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * 排他モードでないならば接続します。
	 */
	///既に接続している場合、
	if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT
		|| rContext->dwContexts > 0)
	{
		///−共有違反エラー（リターン）
		return SCARD_E_SHARING_VIOLATION;
	}

	/*******************************************
	 *
	 * カードの有無を調べます。
	 *
	 *******************************************/
	///カード状態取得
	dwStatus = rContext->readerState->readerState;
	///共有モードが直接制御でない場合、
	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		///−取得したカード状態が「カードなし」の場合、
		if (!(dwStatus & SCARD_PRESENT))
		{
			///−−カードなしエラー（リターン）
			return SCARD_E_NO_SMARTCARD;
		}
	}

	/*******************************************
	 *
	 * ここではリセット応答(ATR:Answer To Reset)を解読し、
	 * 利用するためのプロトコルを設定します。
	 *
	 *******************************************/
	if (dwPreferredProtocols & SCARD_PROTOCOL_RAW)
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_RAW;
	///指定プロトコルが T=0/T=1 で、かつ 共有モード \e dwShareMode が直接制御でない場合、
	else
	if (dwShareMode != SCARD_SHARE_DIRECT)
	{
		/*
		 * この時点でプロトコルはまだセットされていません
		 */
		///−カードのプロトコルが設定されていない場合、
		if (SCARD_PROTOCOL_UNSET == rContext->readerState->cardProtocol)
		{
			UCHAR ucAvailable, ucDefault;
			int ret;

			///−−プロトコルを取得（有効プロトコル＆デフォルトプロトコル） PHGetProtocol()
			ret = PHGetProtocol(rContext->readerState->cardAtr,
				rContext->readerState->cardAtrLength,
				&ucDefault,
				&ucAvailable);

			///−−要求プロトコル \e dwPreferredProtocols が SCARD_PROTOCOL_ANY_OLD の場合、
			if (dwPreferredProtocols & SCARD_PROTOCOL_ANY_OLD) {
				///−−−要求プロトコル \e dwPreferredProtocols を SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1 に変更
				dwPreferredProtocols = SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1;
			}

			///−−要求プロトコルを設定 PHSetProtocol()
			ret = PHSetProtocol(rContext, dwPreferredProtocols,
				ucAvailable, ucDefault);

			/* エラーの場合、 cardProtocol = SCARD_PROTOCOL_UNSET となっている  */
			///−−プロトコル設定に失敗した場合、
			if (SET_PROTOCOL_PPS_FAILED == ret) {
				///−−−−応答（制御）できないカード警告（リターン）
				return SCARD_W_UNRESPONSIVE_CARD;
			}
			///−−引数エラーの場合、
			if (SET_PROTOCOL_WRONG_ARGUMENT == ret) {
				///−−−要求プロトコルは利用できません（リターン）
				return SCARD_E_PROTO_MISMATCH;
			}

			/* 交渉結果のプロトコルを設定 */
			///−−リーダコンテキストのカードプロトコルに設定できたプロトコルを設定
			rContext->readerState->cardProtocol = ret;
		}
		/**
		 *−カードのプロトコルが設定されていて
		 * 要求プロトコルと設定されているプロトコルが一致しない場合、
		 */
		else
		if (! (dwPreferredProtocols & rContext->readerState->cardProtocol)) {
			///−−要求プロトコルは利用できません（リターン）
			return SCARD_E_PROTO_MISMATCH;
		}
	}

	/// カード接続ハンドルを設定 \e *phCard
	*phCard = rContext->dwIdentity + CARD_HANDLE_LOW;

	/*
	 * Add a connection to the context stack
	 */
	rContext->dwContexts += 1;

	/*
	 * Add this handle to the handle list
	 */
	///リーダコンテキストにカード接続ハンドルが存在する場合、
	if (rContext->psHandle.hCard != 0) {
		//−既に設定されています（リターン）
		/*
		 * Clean up - there is no more room
		 */
		if (rContext->dwContexts == SCARD_EXCLUSIVE_CONTEXT)
			rContext->dwContexts = 0;
		else
		///−既にリーダコンテキストが利用されている場合、
		if (rContext->dwContexts > 0) {
			///−−リーダコンテキストをクリアします。
			rContext->dwContexts -= 1;
		}
		///−カード接続ハンドル \e *phCard に 0 を設定
		*phCard = 0;
		///−内部エラー（リターン）
		return SCARD_F_INTERNAL_ERROR;
	}
	
	///リーダコンテキストにカード接続ハンドルを追加します。
	rContext->psHandle.hCard = *phCard;
	///リーダコンテキストのイベントを初期化
	rContext->psHandle.dwEventStatus = 0;

	///リーダコンテキストのハンドルと指定されたハンドルが一致する場合、
	if (psContext.hContext == hContext)
	{
		/*
		 * Find an empty spot to put the hCard value 
		 */
		///−リーダコンテキストのカード接続ハンドルが設定されていない場合、
		if (psContext.hCard == 0)
		{
			///−−リーダコンテキストにカード接続ハンドルを設定
			psContext.hCard = *phCard;
		} else {
			///−−内部エラー（リターン）
			return SCARD_F_INTERNAL_ERROR;
		}
	}
	///リーダコンテキストのハンドルと指定されたハンドルが一致しない場合、
	else {
		///−無効な値エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}
	///接続したプロトコルを設定 \e *pdwActiveProtocol
	*pdwActiveProtocol = rContext->readerState->cardProtocol;
	///リーダコンテキストに接続したリーダ名を設定
	psContext.readerName = tmpReaderName;
	{
		int i;
		ID_SIZE_T len = sizeof(tmpReaderName) - 1;
		for (i=0; szReader[i] && i<len; ++i) {
			tmpReaderName[i] = szReader[i];
		}
		tmpReaderName[i] = 0;
	}

	///正常終了
	return SCARD_S_SUCCESS;
}

/**
 * @brief SCardConnect() で作成された接続を切断します。
 *
 * dwDisposition は次の値を設定できますcan have the following values:
 *
 * @param[in] hCard カード接続ハンドル。
 * @param[in] dwDisposition リーダの実行ファンクション。
 * <ul>
 *   <li>SCARD_LEAVE_CARD - 何もしない。
 *   <li>SCARD_RESET_CARD - カードをリセットします (warm reset)
 *   <li>SCARD_UNPOWER_CARD - カードの電源供給をOFF->ONします (cold reset)
 *   <li>SCARD_EJECT_CARD - カードを排出します。
 * </ul>
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_HANDLE \e hCard は無効なハンドルです。
 * @retval SCARD_E_INVALID_VALUE 無効な値エラー。
 * 
 * @since 2008.02
 * @version 1.0
 */
LONG SCardDisconnect(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	///ハンドルが 0 の場合、
	if (hCard == 0) {
		///−無効なハンドルエラー（リターン）
		return SCARD_E_INVALID_HANDLE;
	}

	///ハンドルよりリーダコンテキストを取得 RFReaderInfoById()
	rv = RFReaderInfoById(hCard, &rContext);
	///ハンドル \e hCard に一致するコンテキストが見つからなかった場合、
	if (rv != SCARD_S_SUCCESS) {
		///−無効な値エラー（リターン）
		return rv;
	}

	///無効な実行ファンクション \e dwDisposition が指定された場合、
	if ((dwDisposition != SCARD_LEAVE_CARD)
		&& (dwDisposition != SCARD_UNPOWER_CARD)
		&& (dwDisposition != SCARD_RESET_CARD)
		&& (dwDisposition != SCARD_EJECT_CARD)) {
		///−無効な値エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	///実行ファンクション \e dwDisposition がリセットまたは電力供給OFF-\>ONの場合、
	if (dwDisposition == SCARD_RESET_CARD || dwDisposition == SCARD_UNPOWER_CARD)
	{
		/*
		 * 現在 pcsc-lite は絶えず電力を供給しています。
		 */
		///−実行ファンクションがリセットの場合、
		if (SCARD_RESET_CARD == dwDisposition) {
			///−−カードをリセットします。 IFDHPowerICC()
			rv = IFDHPowerICC(rContext->dwSlot, IFD_RESET,
					rContext->readerState->cardAtr,
					&rContext->readerState->cardAtrLength);
		}
		///−実行ファンクションが電力供給OFF-\>ONの場合、
		else
		{
			///−−カードの電力供給をOFFにします。 IFDHPowerICC()
			rv = IFDHPowerICC(rContext->dwSlot, IFD_POWER_DOWN,
					rContext->readerState->cardAtr,
					&rContext->readerState->cardAtrLength);
			///−−カードの電力供給をONにします。 IFDHPowerICC()
			rv = IFDHPowerICC(rContext->dwSlot, IFD_POWER_UP,
					rContext->readerState->cardAtr,
					&rContext->readerState->cardAtrLength);
		}

		/* 電力供給後はプロトコルは未設定状態 */
		///−コンテキストのカードプロトコルを未設定にします。
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

		/*
		 * Notify the card has been reset
		 */
		///−コンテキストのイベントにカードリセットを設定
		rContext->psHandle.dwEventStatus = SCARD_RESET;

		///−実行ファンクション処理が正常終了した場合、
		if (rv == SCARD_S_SUCCESS)
		{
			///−−コンテキストのカード状態を「カードあり＋電力供給中＋PTS交渉待ち」にします
			rContext->readerState->readerState |= SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~(SCARD_ABSENT | SCARD_SPECIFIC | SCARD_SWALLOWED | SCARD_UNKNOWN);
		}
		///−実行ファンクション処理に失敗した場合、
		else
		{
			///−−コンテキストのカード状態が「カードなし」の場合、
			if (rContext->readerState->readerState & SCARD_ABSENT) {
				///−−−コンテキストのカード状態の「カードあり」をクリアします
				rContext->readerState->readerState &= ~SCARD_PRESENT;
			}
			///−−コンテキストのカード状態が「カードなし」以外の場合、
			else {
				///−−−コンテキストのカード状態に「カードあり」を設定
				rContext->readerState->readerState |= SCARD_PRESENT;
			}
			/* SCARD_ABSENT flag is already set */
			///−−コンテキストのカード状態に「電力供給まち」を追加設定
			rContext->readerState->readerState |= SCARD_SWALLOWED;
			rContext->readerState->readerState &= ~(SCARD_POWERED | SCARD_NEGOTIABLE | SCARD_SPECIFIC | SCARD_UNKNOWN);
			///−−コンテキストのカードATR長に 0 を設定
			rContext->readerState->cardAtrLength = 0;
		}
	}
	///実行ファンクション \e dwDisposition がカード排出の場合、
	else if (dwDisposition == SCARD_EJECT_CARD)
	{
		UCHAR controlBuffer[5];
		UCHAR receiveBuffer[MAX_BUFFER_SIZE];
		DWORD receiveLength;

		/*
		 * Set up the CTBCS command for Eject ICC
		 */
		///−カード排出 CTBCSコマンド作成
		controlBuffer[0] = 0x20;	// CLA
		controlBuffer[1] = 0x15;	// INS =EJECT ICC
		// P1  = ICC-Interface 1-14
		controlBuffer[2] = (rContext->dwSlot & 0x0000FFFF) + 1;
		// P2 (0x00 for CardTerminals without keypad and display)
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;	// LC =継続データなし
		receiveLength = 2;
		///−コマンド送信 IFDHControl()
		rv = IFDHControl(rContext->dwSlot, controlBuffer, 5,
			receiveBuffer, &receiveLength);
	}
	else if (dwDisposition == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing
		 */
	}

	///リーダコンテキストのハンドルを初期化します。
	rContext->psHandle.hCard = 0;
	///リーダコンテキストのイベントを初期化します。
	rContext->psHandle.dwEventStatus = 0;

	/*
	 * Remove a connection from the context stack
	 */
	///コンテキスト数をリセット
	rContext->dwContexts -= 1;

	if (rContext->dwContexts < 0)
		rContext->dwContexts = 0;

	///コンテキストのカード接続ハンドルを初期化
	psContext.hCard = 0;
	///コンテキストのリーダ名に NULL を設定
	psContext.readerName = NULL;

	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}

/**
 * @brief 一連のコマンドあるいは処理を行うため一時的に排他的な
 * アクセス・モードを確立します。
 *
 * @param[in] hCard SCardConnect で作成されたカード接続ハンドル。
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_HANDLE \e hCard は無効なハンドルです。
 * @retval SCARD_E_INVALID_VALUE \e hCard は無効なハンドルです。
 * @retval SCARD_E_READER_UNAVAILABLE 指定されたリーダは利用可能な状態ではありません。
 * @retval SCARD_W_REMOVED_CARD カードが抜かれている。
 * @retval SCARD_W_RESET_CARD カードがリセットされている。
 * 
 * @since 2008.02
 * @version 1.0
 */
LONG SCardBeginTransaction(SCARDHANDLE hCard)
{
	LONG rv;
	PREADER_CONTEXT rContext;

	///ハンドルが 0 の場合、
	if (hCard == 0) {
		///−無効なハンドルエラー（リターン）
		return SCARD_E_INVALID_HANDLE;
	}

	///ハンドル \e hCard よりリーダコンテキストを取得 RFReaderInfoById()
	rv = RFReaderInfoById(hCard, &rContext);
	///ハンドル \e hCard に一致するコンテキストが見つからなかった場合、
	if (rv != SCARD_S_SUCCESS) {
		///−無効な値エラー（リターン）
		return rv;
	}

	///カード状態遷移処理 EHStatusHandler()
	EHStatusHandler(rContext);

	/*
	 * Make sure the reader is working properly
	 */
	///リーダが利用可能な状態ではない場合、
	if ((rContext->readerState == NULL)
		|| (rContext->readerState->readerState & SCARD_UNKNOWN)) {
		///−エラーを返す（リターン）
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * カードが抜かれたり、リセットされていないかイベント検査
	 */
	///カードイベントがある場合、 RFCheckReaderEventState()
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS) {
		///−エラーを返す（リターン）
		return rv;
	}

	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}

/**
 * @brief SCardBeginTransaction() で開始したトランザクション処理を終了します。
 *
 * @param[in] hCard SCardConnect() で作成されたカード接続ハンドル。
 * @param[in] dwDisposition リーダの実行ファンクション。
 * <ul>
 *   <li>SCARD_LEAVE_CARD - 何もしない。
 *   <li>SCARD_RESET_CARD - カードをリセットします (warm reset)
 *   <li>SCARD_UNPOWER_CARD - カードの電力供給をOFF-\>ONします (cold reset)
 *   <li>SCARD_EJECT_CARD - カードを排出します。
 * </ul>
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_HANDLE \p hCard は無効なハンドルです。
 * @retval SCARD_E_READER_UNAVAILABLE 指定されたリーダは利用可能な状態ではありません。
 * @retval SCARD_W_REMOVED_CARD カードが抜かれている。
 * @retval SCARD_W_RESET_CARD カードがリセットされている。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG SCardEndTransaction(SCARDHANDLE hCard, DWORD dwDisposition)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;

	///ハンドルが 0 の場合、
	if (hCard == 0) {
		///−無効なハンドルエラー（リターン）
		return SCARD_E_INVALID_HANDLE;
	}

	///無効な実行ファンクション \e dwDisposition が指定された場合、
	if ((dwDisposition != SCARD_LEAVE_CARD)
		&& (dwDisposition != SCARD_UNPOWER_CARD)
		&& (dwDisposition != SCARD_RESET_CARD)
		&& (dwDisposition != SCARD_EJECT_CARD)) {
		///−無効な値エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	///ハンドル \e hCard よりリーダコンテキストを取得 RFReaderInfoById()
	rv = RFReaderInfoById(hCard, &rContext);
	///ハンドル \e hCard に一致するコンテキストが見つからなかった場合、
	if (rv != SCARD_S_SUCCESS) {
		///−無効な値エラー（リターン）
		return rv;
	}

	///カード状態遷移処理 EHStatusHandler()
	EHStatusHandler(rContext);

	///リーダが利用可能な状態ではない場合、
	if ((rContext->readerState == NULL)
		|| (rContext->readerState->readerState & SCARD_UNKNOWN)) {
		///−エラーを返す（リターン）
		return SCARD_E_READER_UNAVAILABLE;
	}

	/*
	 * カードが抜かれたり、リセットされていないかイベント検査
	 */
	///カードイベントがある場合、 RFCheckReaderEventState()
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS) {
		///−エラーを返す（リターン）
		return rv;
	}

	///実行ファンクション \e dwDisposition がリセットまたは電力供給OFF-\>ONの場合、
	if (dwDisposition == SCARD_RESET_CARD || dwDisposition == SCARD_UNPOWER_CARD)
	{
		/*
		 * 現在 pcsc-lite は絶えず電力を供給しています。
		 */
		///−実行ファンクションがリセットの場合、
		if (SCARD_RESET_CARD == dwDisposition) {
			///−−カードをリセットします。 IFDHPowerICC()
			rv = IFDHPowerICC(rContext->dwSlot, IFD_RESET,
					rContext->readerState->cardAtr,
					&rContext->readerState->cardAtrLength);
		}
		///−実行ファンクションが電力供給OFF-\>ONの場合、
		else
		{
			///−−カードの電力供給をOFFにします。 IFDHPowerICC()
			rv = IFDHPowerICC(rContext->dwSlot, IFD_POWER_DOWN,
					rContext->readerState->cardAtr,
					&rContext->readerState->cardAtrLength);
			///−−カードの電力供給をONにします。 IFDHPowerICC()
			rv = IFDHPowerICC(rContext->dwSlot, IFD_POWER_UP,
					rContext->readerState->cardAtr,
					&rContext->readerState->cardAtrLength);
		}

		/* 電力供給後はプロトコルは未設定状態 */
		///−コンテキストのカードプロトコルを未設定にします。
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

		///−コンテキストのイベントにカードリセットを設定
		rContext->psHandle.dwEventStatus = SCARD_RESET;

		///−実行ファンクション処理が正常終了した場合、
		if (rv == SCARD_S_SUCCESS) {
			///−−コンテキストのカード状態を「カードあり＋電力供給中＋PTS交渉待ち」にします
			rContext->readerState->readerState |= SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~(SCARD_ABSENT | SCARD_SPECIFIC | SCARD_SWALLOWED | SCARD_UNKNOWN);
		}
		///−実行ファンクション処理に失敗した場合、
		else {
			///−−コンテキストのカード状態が「カードなし」の場合、
			if (rContext->readerState->readerState & SCARD_ABSENT) {
				///−−−コンテキストのカード状態の「カードあり」をクリアします
				rContext->readerState->readerState &= ~SCARD_PRESENT;
			}
			///−−コンテキストのカード状態が「カードなし」以外の場合、
			else {
				///−−−コンテキストのカード状態に「カードあり」を設定
				rContext->readerState->readerState |= SCARD_PRESENT;
			}
			/* SCARD_ABSENT flag is already set */
			///−−コンテキストのカード状態に「電力供給まち」を追加設定
			rContext->readerState->readerState |= SCARD_SWALLOWED;
			rContext->readerState->readerState &= ~(SCARD_POWERED | SCARD_NEGOTIABLE | SCARD_SPECIFIC | SCARD_UNKNOWN);
			///−−コンテキストのカードATR長に 0 を設定
			rContext->readerState->cardAtrLength = 0;
		}
	}
	///実行ファンクション \e dwDisposition がカード排出の場合、
	else if (dwDisposition == SCARD_EJECT_CARD)
	{
		UCHAR controlBuffer[5];
		UCHAR receiveBuffer[MAX_BUFFER_SIZE];
		DWORD receiveLength;

		/*
		 * Set up the CTBCS command for Eject ICC
		 */
		///−カード排出 CTBCSコマンド作成
		controlBuffer[0] = 0x20;	// CLA
		controlBuffer[1] = 0x15;	// INS =EJECT ICC
		// P1  = ICC-Interface 1-14
		controlBuffer[2] = (rContext->dwSlot & 0x0000FFFF) + 1;
		// P2 (0x00 for CardTerminals without keypad and display)
		controlBuffer[3] = 0x00;
		controlBuffer[4] = 0x00;	// LC =継続データなし
		receiveLength = 2;
		///−コマンド送信 IFDHControl()
		rv = IFDHControl(rContext->dwSlot, controlBuffer, 5,
			receiveBuffer, &receiveLength);
		/* receiveBuffer
		 * [0] :[1]
		 * 0x90:0x00	成功
		 * 0x90:0x01	成功、カードが抜かれた
		 * 0x62:0x00	規定時間内にカードは抜かれませんでした
		 */
	}
	else if (dwDisposition == SCARD_LEAVE_CARD)
	{
		/*
		 * Do nothing
		 */
	}

	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}

/**
 * @brief \e hCard に接続したリーダの現在の状態を返します。\n
 *
 * \e szReaderName には friendly name が保存されます。 
 * \e pcchReaderLen には \e szReaderName のために確保されたバッファのサイズが、また
 * \e pcbAtrLen には \e pbAtr のために確保されたバッファのサイズが入ります。
 * もし、これらのどちらかが小さい場合、関数は SCARD_E_INSUFFICIENT_BUFFER を
 * 返し、\e pcchReaderLen と \e pcbAtrLen には必要なサイズが入ります。
 * 現在の状態とプロトコルはそれぞれ \e pdwState と \e pdwProtocol にセットされます。
 *
 * @param[in] hCard SCardConnect で作成された接続ハンドル。
 * @param[out] mszReaderNames 状態を取得するリーダ名 (Friendly name)
 * @param[in,out] pcchReaderLen \e szReaderName（マルチストリング形式）のサイズまたは取得サイズ
 * @param[out] pdwState 現在のカード状態。（ビットマスクの論理和）
 * <ul>
 *   <li>SCARD_ABSENT - リーダにカードがありません。
 *   <li>SCARD_PRESENT - リーダにカードがありますが、使用できる位置にありません。
 *   <li>SCARD_SWALLOWED - リーダにカードがあり、使用できる位置にあります。
 *                         但し、カードに電力供給されていません。
 *   <li>SCARD_POWERED - カードに電力供給されていますが、
 *                       リーダ・ドライバは、カードのモードがわかりません。
 *   <li>SCARD_NEGOTIABLE - カードはリセットされ PTS 交渉を待っています。
 *   <li>SCARD_SPECIFIC - カードはリセットされ通信回線（プロトコル等）を確立しました。
 * </ul>
 * @param[out] pdwProtocol リーダの現在のプロトコル
 * <ul>
 *   <li>SCARD_PROTOCOL_T0 - プロトコル T=0
 *   <li>SCARD_PROTOCOL_T1 - プロトコル T=1
 * </ul>
 * @param[out] pbAtr リーダのカードの現在の ATR(Answer To Reset)
 * @param[out] pcbAtrLen ATRのサイズ
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_PARAMETER 無効なパラメータ。
 * @retval SCARD_E_INVALID_HANDLE 無効なハンドル。
 * @retval SCARD_E_INVALID_VALUE 無効な値。
 * @retval SCARD_E_INSUFFICIENT_BUFFER バッファサイズが不足しています。
 * @retval SCARD_E_READER_UNAVAILABLE 指定されたリーダは利用可能な状態ではありません。
 * @retval SCARD_F_INTERNAL_ERROR 内部エラー。
 * @retval SCARD_W_REMOVED_CARD カードが抜かれている。
 * @retval SCARD_W_RESET_CARD カードがリセットされている。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG SCardStatus(SCARDHANDLE hCard, LPSTR mszReaderNames,
	LPDWORD pcchReaderLen, LPDWORD pdwState,
	LPDWORD pdwProtocol, LPBYTE pbAtr, LPDWORD pcbAtrLen)
{
	DWORD dwReaderLen = 0, dwAtrLen = 0;
	LONG rv;
	PREADER_CONTEXT rContext = NULL;
	
	///各データ長バッファ \e pcchReaderLen および \e pcbAtrLen が設定されていない場合、
	if (pcchReaderLen == NULL || pcbAtrLen == NULL) {
		///−パラメータエラー（リターン）
		return SCARD_E_INVALID_PARAMETER;
	}

	// length passed from caller
	dwReaderLen = *pcchReaderLen;
	dwAtrLen = *pcbAtrLen;

	///出力パラメータを初期化
	if (pdwState)
		*pdwState = 0;

	if (pdwProtocol)
		*pdwProtocol = 0;

	*pcchReaderLen = 0;
	*pcbAtrLen = 0;

	///リーダのコンテキストが存在しない場合、
	if (!(psContext.readerName && IDMan_StStrcmp(psContext.readerName, readerState.readerName) == 0))
	{
		///−指定されたリーダは利用可能な状態ではありません。（リターン）
		return SCARD_E_READER_UNAVAILABLE;
	}

	///ハンドル \e hCard よりリーダコンテキストを取得 RFReaderInfoById()
	rv = RFReaderInfoById(hCard, &rContext);
	///ハンドル \e hCard に一致するコンテキストが見つからなかった場合、
	if (rv != SCARD_S_SUCCESS) {
		///−無効な値エラー（リターン）
		return rv;
	}

	///カード状態遷移処理 EHStatusHandler()
	EHStatusHandler(rContext);

	///リーダが利用可能な状態ではない場合、
	if ((rContext->readerState == NULL)
		|| (rContext->readerState->readerState & SCARD_UNKNOWN)) {
		///−エラーを返す（リターン）
		return SCARD_E_READER_UNAVAILABLE;
	}

	///コンテキストのリーダ名のサイズまたは ATRデータ長が異常な場合、
	if (IDMan_StStrlen(rContext->lpcReader) > MAX_BUFFER_SIZE
			|| rContext->readerState->cardAtrLength > MAX_ATR_SIZE
			|| rContext->readerState->cardAtrLength < 0) {
		///−内部エラー（リターン）
		return SCARD_F_INTERNAL_ERROR;
	}

	// カードが抜かれたり、リセットされていないかイベント検査
	///カードイベントを検査 RFCheckReaderEventState()
	rv = RFCheckReaderEventState(rContext, hCard);
	///充分なバッファがない以外のエラーの場合、
	if (rv != SCARD_S_SUCCESS && rv != SCARD_E_INSUFFICIENT_BUFFER)
	{
		///−エラーを返す（リターン）
		return rv;
	}

	///リーダ名のサイズを設定 \e *pcchReaderLen
	*pcchReaderLen = IDMan_StStrlen(rContext->lpcReader) + 1;
	///状態を取得するリーダ名がある場合、
	if (mszReaderNames) {
		///−リーダ名バッファサイズが不足している場合、
		if (*pcchReaderLen > dwReaderLen) {
			///−−バッファサイズが不足しています（リターン）
			rv = SCARD_E_INSUFFICIENT_BUFFER;
		}
		///−コンテキストのリーダ名を \e mszReaderNames に格納
		//strncpy(mszReaderNames, rContext->lpcReader, dwReaderLen);
		DWORD n = dwReaderLen;
		char *d = mszReaderNames;
		const char *s = rContext->lpcReader;
		do {
			if ((*d++ = *s++) == 0) {
				/* NUL pad the remaining n-1 bytes */
				while (--n != 0)
					*d++ = 0;
				break;
			}
		} while (--n != 0);
	}

	///カード状態バッファ \e pdwState が設定されている場合、
	if (pdwState) {
		///−コンテキストのカード状態を \e *pdwState に設定
		*pdwState = rContext->readerState->readerState;
	}

	///プロトコルバッファ \e pdwProtocol が設定されている場合、
	if (pdwProtocol) {
		///−コンテキストのカードプロトコルを \e *pdwProtocol に設定
		*pdwProtocol = rContext->readerState->cardProtocol;
	}

	/// ATRデータ長を \e *pcbAtrLen に設定
	*pcbAtrLen = rContext->readerState->cardAtrLength;
	/// ATRデータバッファ \e pbAtr が設定されている場合、
	if (pbAtr) {
		///−用意された ATRデータのバッファサイズが不足している場合、
		if (*pcbAtrLen > dwAtrLen) {
			///−−バッファサイズが不足しています（リターン）
			rv = SCARD_E_INSUFFICIENT_BUFFER;
		}
		///−コンテキストの ATRデータを \e pbAtr に格納
		IDMan_StMemcpy(pbAtr, rContext->readerState->cardAtr,
			(*pcbAtrLen > dwAtrLen)? dwAtrLen: rContext->readerState->cardAtrLength);
	}
	///終了（リターン）
	return rv;
}


/**
 * @brief SCardConnect()によって接続されたリーダのスマートカードに
 * コマンド APDU(Application Protocol Data Unit) を送ります。\n
 *
 * カードからのAPDU応答は、結果を \e pbRecvBuffer に、そのサイズを \e SpcbRecvLength に
 * 格納します。
 * \e SSendPci と \e SRecvPci は次のような構造体です:
 * @code
 * typedef struct {
 *    DWORD dwProtocol;    // SCARD_PROTOCOL_T0 または SCARD_PROTOCOL_T1
 *    DWORD cbPciLength;   // この構造体のサイズ - 未使用
 * } SCARD_IO_REQUEST;
 * @endcode
 * 
 * @param[in] hCard カード接続ハンドル。
 * @param[in] pioSendPci 送信情報構造体
 * <ul>
 *   <li>SCARD_PCI_T0 - 前もって定義された T=0 構造体
 *   <li>SCARD_PCI_T1 - 前もって定義された T=1 構造体
 * </ul>
 * @param[in] pbSendBuffer 送信データバッファ（カードに送信する APDU）
 * @param[in] cbSendLength 送信データ長（APDU のサイズ）
 * @param[out] pioRecvPci 受信情報の構造体 \n
 * このパラメータは IFD handler IFDHTransmitToICCに依存しています。 \n
 * CCIDドライバでは未使用です。
 * @param[out] pbRecvBuffer カードからの応答
 * @param[in,out] pcbRecvLength 応答のサイズ
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_NO_SMARTCARD カードがありません。
 * @retval SCARD_E_INVALID_PARAMETER パラメータエラー。
 * @retval SCARD_E_INVALID_HANDLE 無効な hCard ハンドル。
 * @retval SCARD_E_INSUFFICIENT_BUFFER バッファサイズが不足しています。
 * @retval SCARD_E_NOT_TRANSACTED APDU 交換できませんでした。
 * @retval SCARD_E_PROTO_MISMATCH 接続しているプロトコルは望んでいるものとは異なります。
 * @retval SCARD_E_INVALID_VALUE 無効な値（プロトコル、リーダ名、等）
 * @retval SCARD_E_READER_UNAVAILABLE 指定されたリーダは利用可能な状態ではありません。
 * @retval SCARD_W_RESET_CARD カードがリセットされています。
 * @retval SCARD_W_REMOVED_CARD リーダからカードが抜かれています。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG SCardTransmit(SCARDHANDLE hCard, LPCSCARD_IO_REQUEST pioSendPci,
	LPCBYTE pbSendBuffer, DWORD cbSendLength,
	LPSCARD_IO_REQUEST pioRecvPci, LPBYTE pbRecvBuffer,
	LPDWORD pcbRecvLength)
{
	LONG rv;
	PREADER_CONTEXT rContext = NULL;
	SCARD_IO_HEADER sSendPci, sRecvPci;
	DWORD dwRxLength, tempRxLength;

	///ハンドルが 0 の場合、
	if (hCard == 0) {
		///−無効なハンドルエラー（リターン）
		return SCARD_E_INVALID_HANDLE;
	}

	///受信データ長バッファが設定されていない場合、
	if (pcbRecvLength == NULL) {
		///−パラメータエラー（リターン）
		return SCARD_E_INVALID_PARAMETER;
	}

	dwRxLength = *pcbRecvLength;
	*pcbRecvLength = 0;

	///送信情報構造体または送受信バッファが設定されていない場合、
	if (pbSendBuffer == NULL || pbRecvBuffer == NULL || pioSendPci == NULL) {
		///−パラメータエラー（リターン）
		return SCARD_E_INVALID_PARAMETER;
	}

	///ハンドルに一致するリーダコンテキストが見つからない場合、
	if (psContext.hContext == 0 || psContext.hCard != hCard) {
		///−無効なハンドル（リターン）
		return SCARD_E_INVALID_HANDLE;
	}

	///リーダコンテキストのリーダ名に一致するリーダ属性管理構造体が見つからない場合、
	if (!(psContext.readerName && IDMan_StStrcmp(psContext.readerName, readerState.readerName) == 0)) {
		///−指定されたリーダは利用可能な状態ではありません。（リターン）
		return SCARD_E_READER_UNAVAILABLE;
	}

	// APDU は少なくても 4バイト以上
	/// 送信データ長が \e cbSendLength が 4バイト未満の場合、
	if (cbSendLength < 4) {
		///−パラメータエラー（リターン）
		return SCARD_E_INVALID_PARAMETER;
	}

	// SCardControl は少なくても 2つの状態ワードが必要
	///受信応答バッファサイズ \e *pcbRecvLength が 2バイト未満の場合、
	if (dwRxLength < 2) {
		///−バッファサイズが不足しています（リターン）
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	///送信データ長が送信可能な最大値を超えている場合、
	if ((cbSendLength > MAX_BUFFER_SIZE_EXTENDED)
		|| (*pcbRecvLength > MAX_BUFFER_SIZE_EXTENDED)) {
		///−バッファサイズが不足しています（リターン）
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	///ハンドル \e hCard よりリーダコンテキストを取得 RFReaderInfoById()
	rv = RFReaderInfoById(hCard, &rContext);
	///ハンドル \e hCard に一致するコンテキストが見つからなかった場合、
	if (rv != SCARD_S_SUCCESS) {
		///−無効な値エラー（リターン）
		return rv;
	}

	///カード状態遷移処理 EHStatusHandler()
	EHStatusHandler(rContext);

	// リーダが適切に働いていることを確かめます。
	///リーダが利用可能な状態ではない場合、
	if ((rContext->readerState == NULL)
		|| (rContext->readerState->readerState & SCARD_UNKNOWN)) {
		///−エラーを返す（リターン）
		return SCARD_E_READER_UNAVAILABLE;
	}

	// カードが抜かれたり、リセットされていないかイベント検査
	if ((rv = RFCheckReaderEventState(rContext, hCard)) != SCARD_S_SUCCESS)
		return rv;

	/*
	 * Check for some common errors
	 */
	///送信情報構造体のプロトコルが RAWの場合
	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_RAW) {
		///−未対応プロトコル（リターン）
		return SCARD_E_PROTO_MISMATCH;
	}

	///カードがない場合、
	if (rContext->readerState->readerState & SCARD_ABSENT) {
		///−カードなしエラー（リターン）
		return SCARD_E_NO_SMARTCARD;
	}
	///送信情報 \e pioSendPci のプロトコルが旧プロトコル（互換用）でない場合、
	if (pioSendPci->dwProtocol != SCARD_PROTOCOL_ANY_OLD) {
		///−送信情報 \e pioSendPci のプロトコルがリーダコンテキストと一致しない場合、
		if (pioSendPci->dwProtocol != rContext->readerState->cardProtocol) {
			///−−プロトコル不一致エラー（リターン）
			return SCARD_E_PROTO_MISMATCH;
		}
	}

	/*
	 * 応急処置: PC/SC はビットマスクのため 1 から開始します。
	 * しかし、IFD_Handler は 0 または 1 を使用します。
	 * 例）T=0
	 * PC/SC		1/2 (=SCARD_PROTOCOL_T0/SCARD_PROTOCOL_T1)
	 * IFD_Handler	0/1
	 */

	///送信プロトコルをデフォルト値 T=0 に設定
	sSendPci.Protocol = 0; /* protocol T=0 by default */
	///送信情報のプロトコルが T=1 の場合、
	if (pioSendPci->dwProtocol == SCARD_PROTOCOL_T1) {
		///−送信プロトコルを T=1 に設定
		sSendPci.Protocol = 1;
	}
	///送信情報のプロトコルが旧プロトコル互換の場合、
	else if (pioSendPci->dwProtocol == SCARD_PROTOCOL_ANY_OLD) {
		///−送信プロトコルをリーダコンテキストのカードプロトコル(T=0 or T=1)に設定
		sSendPci.Protocol = rContext->readerState->cardProtocol;
	}

	///送信情報データサイズを送信情報構造体より取得
	sSendPci.Length = pioSendPci->cbPciLength;

	tempRxLength = dwRxLength;

	///コマンド送信 IFDHTransmitToICC()
	rv = IFDHTransmitToICC(rContext->dwSlot, sSendPci,
		(LPBYTE) pbSendBuffer, cbSendLength,
		pbRecvBuffer, &dwRxLength, &sRecvPci);

	///受信プロトコルバッファがある場合、
	if (pioRecvPci) {
		///−取得した受信プロトコル情報を設定 \e pioRecvPci
		pioRecvPci->dwProtocol = sRecvPci.Protocol;
		pioRecvPci->cbPciLength = sRecvPci.Length;
	}

	///コマンド送信エラーの場合、
	if (rv != IFD_SUCCESS) {
		///−受信データ長 \e *pcbRecvLength に 0 を設定
		*pcbRecvLength = 0;
		///−データ交換エラー（リターン）
		return SCARD_E_NOT_TRANSACTED;
	}

	/*
	 * Available is less than received
	 */
	///受信バッファサイズが受信データ長より少ない場合、
	if (tempRxLength < dwRxLength) {
		///−受信データ長 \e *pcbRecvLength に 0 を設定
		*pcbRecvLength = 0;
		///−バッファが不足しています（リターン）
		return SCARD_E_INSUFFICIENT_BUFFER;
	}

	///取得した受信データ長を \e *pcbRecvLength に設定
	*pcbRecvLength = dwRxLength;
	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}
