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
 * ID管理のPCSCライブラリ：リーダ制御関数
 * \file IDMan_PcReaderfactory.c
 */


#include "IDMan_StandardIo.h"
#include "IDMan_PcMisc.h"

#include "IDMan_PcReaderfactory.h"
#include "IDMan_PcEventhandler.h"

#ifndef NULL
# define NULL	0
#endif


///リーダーのコンテキスト（インスタンス）
READER_CONTEXT sReaderContext;
///全コンテキスト数
static DWORD dwNumReadersContexts = 0;

extern struct _readerTracker readerTracker;


/**
 * @brief 検出したリーダのコンテキスト初期化して登録。\n
 * IFDへの通信チャンネルをオープン、リーダ属性取得等。
 *
 * @param[in] lpcReader リーダ名称
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_TARGET 無効な対象エラー。
 *
 * @since 2008.02
 * @version 1.0
 */
INTERNAL LONG RFAddReader(LPSTR lpcReader)
{
	DWORD dwGetSize;
	UCHAR ucGetData[1];
	LONG rv;

	///リーダ名またはデバイス名が指定されていない場合、
	if (lpcReader == NULL) {
		///−名称エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	///リーダ名称が長すぎる場合、
	if (IDMan_StStrlen(lpcReader) >= MAX_READERNAME) {
		///−名称エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	 //リーダコンテキストが既に存在する場合、
	if (dwNumReadersContexts) {
		//−このライブラリで使用できるリーダコンテキストは１つのみ（リターン）
		return SCARD_E_NO_MEMORY;
	}

	// Check and set the readername to see if it must be enumerated 
	///リーダコンテキストにリーダ名を設定
	IDMan_StStrcpy(sReaderContext.lpcReader, lpcReader);

	/*
	 * 論理ユニット番号 0xDDDDCCCC => DDDD リーダ端末番号 CCCC カードスロット番号
	 * sReaderContext.dwSlot = (i << 16) + dwSlot;
	 */
	///コンテキスト \e rContext に論理ユニット番号を登録
	sReaderContext.dwSlot = 0;


	sReaderContext.dwContexts = 0;
	// 共有コンテキストIDを設定（このライブラリは１つのみ）
	sReaderContext.dwIdentity = (1) << ((sizeof(DWORD) / 2) * 8);
	sReaderContext.readerState = NULL;
	sReaderContext.psHandle.hCard = 0;

	dwNumReadersContexts += 1;

	///IFDへの通信チャンネルをオープンします。 IFDHCreateChannel()
	rv = IFDHCreateChannel(sReaderContext.dwSlot, 0/*dummy*/);
	///オープンエラーの場合、
	if (rv != IFD_SUCCESS) {
		/*
		 * リーダに接続できません
		 */

		///−コンテキスト \e sReaderContext を未使用状態に初期化
		sReaderContext.readerState = NULL;
		sReaderContext.dwIdentity = 0;

		dwNumReadersContexts -= 1;
		
		///−無効な対象エラー（リターン）
		return SCARD_E_INVALID_TARGET;
	}

	///リーダの全ての属性（カード状態、リーダ名等）を初期化
	rv = EHInitEventHandler(&sReaderContext);
	///エラーの場合、
	if (rv != SCARD_S_SUCCESS) {
		///エラーを返す（リターン）
		return rv;
	}

	/*
	 * Call on the driver to see if there are multiple slots 
	 */

	///リーダのカードスロット数取得 IFDHGetCapabilities()
	dwGetSize = sizeof(ucGetData);
	rv = IFDHGetCapabilities(sReaderContext.dwSlot, TAG_IFD_SLOTS_NUMBER,
								&dwGetSize, ucGetData);
	///スロット数取得に失敗した場合、
	if (rv != IFD_SUCCESS || dwGetSize != 1 || ucGetData[0] == 0) {
		/**
		 * Reader does not have this defined.  Must be a single slot
		 * reader so we can just return SCARD_S_SUCCESS. 
		 */
		///−単一スロットとして取り扱えるように正常終了（リターン）
		return SCARD_S_SUCCESS;
	}
	///取得したスロット数＝ 1 の場合、
	if (rv == IFD_SUCCESS && dwGetSize == 1 && ucGetData[0] == 1) {
		/*
		 * Reader has this defined and it only has one slot 
		 */
		///−正常終了（リターン）
		return SCARD_S_SUCCESS;
	}

	///取得したスロット数＞ 1 の場合、エラーを返す（リターン）
	return SCARD_E_NO_MEMORY;
}


/**
 * @brief 指定したリーダ名のコンテキストを取り除きます。
 *
 * @param[in] lpcReader リーダ名称
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_TARGET 無効な対象エラー。
 *
 * @since 2008.02
 * @version 1.0
 */
INTERNAL LONG RFRemoveReader(LPSTR lpcReader)
{
	LONG rv;
	PREADER_CONTEXT rContext;

	///リーダ名が指定されていない場合、
	if (lpcReader == 0) {
		///無効な対象エラー（リターン）
		return SCARD_E_INVALID_VALUE;
	}

	///リーダ名 \e lpcReader が取得したコンテキストのリーダ名と一致した場合、
	if (sReaderContext.readerState && IDMan_StStrcmp(lpcReader, sReaderContext.lpcReader) == 0) {
		rContext = &sReaderContext;
		///−取得したリーダの属性（カード状態、リーダ名等）を破棄 EHDestroyEventHandler()
		rv = EHDestroyEventHandler(rContext);

		///−IFDへの通信チャンネルをクローズします。 IFDHCloseChannel()
		rv = IFDHCloseChannel(rContext->dwSlot);

		///−リーダコンテキストを初期化します
		IDMan_StMemset(rContext->lpcReader, 0x00, MAX_READERNAME);
		rContext->dwContexts = 0;
		rContext->dwSlot = 0;		//論理ユニット番号初期化（リーダ番号 0 : スロット番号 0 ）
		rContext->dwIdentity = 0;
		rContext->readerState = NULL;
		rContext->psHandle.hCard = 0;

		readerTracker.driver = NULL;
		readerTracker.bus_device[0] = '\0';		//デバイス名消去

		dwNumReadersContexts = 0;
	}
	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}


/**
 * @brief カード接続ハンドルに一致するコンテキストを取得
 *
 * @param[in] dwIdentity カード接続ハンドル
 * @param[out] sReader リーダコンテキストを格納するバッファ
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_E_INVALID_VALUE 無効な値エラー。
 *
 * @since 2008.02
 * @version 1.0
 */
INTERNAL LONG RFReaderInfoById(DWORD dwIdentity, PREADER_CONTEXT * sReader)
{
	SCARDHANDLE hCard = dwIdentity;
	///ハンドルの下位8bitsをクリアしてIDを取得
	dwIdentity = dwIdentity >> ((sizeof(DWORD) / 2) * 8);
	dwIdentity = dwIdentity << ((sizeof(DWORD) / 2) * 8);

	///取得したIDとリーダコンテキストのIDが一致する場合、
	if (dwIdentity == sReaderContext.dwIdentity && hCard == sReaderContext.psHandle.hCard)
	{
		///−リーダコンテキストをバッファに設定 \e *sReader
		*sReader = &sReaderContext;
		///−正常終了（リターン）
		return SCARD_S_SUCCESS;
	}
	///無効な値エラー（リターン）
	return SCARD_E_INVALID_VALUE;
}


/**
 * @brief コンテキストよりカードイベント（挿入された、抜かれた、リセットした）を取得
 *
 * @param[in] rContext リーダコンテキスト構造体
 * @param[in] hCard カード接続ハンドル
 * @retval SCARD_S_SUCCESS イベントなし。
 * @retval SCARD_W_REMOVED_CARD カードが抜かれている。
 * @retval SCARD_W_RESET_CARD カードがリセットされている。
 * @retval SCARD_E_INVALID_VALUE 無効なイベント。
 * @retval SCARD_E_INVALID_HANDLE \e hCard は無効なハンドルです。
 *
 * @since 2008.02
 * @version 1.0
 */
INTERNAL LONG RFCheckReaderEventState(PREADER_CONTEXT rContext, SCARDHANDLE hCard)
{
	///ハンドルに一致するリーダコンテキストがない場合、
	if (rContext->psHandle.hCard != hCard) {
		///−無効なハンドルエラー（リターン）
		return SCARD_E_INVALID_HANDLE;
	}
	///カードが抜かれている場合、
	if (rContext->psHandle.dwEventStatus == SCARD_REMOVED) {
		///−カードが抜かれている警告を返す（リターン）
		return SCARD_W_REMOVED_CARD;
	} else
	///カードがリセットされている場合、
	if (rContext->psHandle.dwEventStatus == SCARD_RESET) {
		///−カードリセット警告を返す（リターン）
		return SCARD_W_RESET_CARD;
	} else
	///イベントがない場合、
	if (rContext->psHandle.dwEventStatus == 0) {
		///−正常終了（リターン）
		return SCARD_S_SUCCESS;
	}
	///その他のイベントの場合、無効な値エラー（リターン）
	return SCARD_E_INVALID_VALUE;
}
