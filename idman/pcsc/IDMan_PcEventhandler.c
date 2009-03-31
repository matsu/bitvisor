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
 * ID管理のPCSCライブラリ：リーダ情報管理関数（カードの挿入／排出イベント、ATR更新、プロトコル、カード状態等）
 * \file IDMan_PcEventhandler.c
 */

#include "IDMan_StandardIo.h"

#include "IDMan_PcMisc.h"
#include "IDMan_PcReaderfactory.h"
#include "IDMan_PcEventhandler.h"

#ifndef NULL
# define NULL	0
#endif

/**
 * @brief リーダ属性管理構造体（インスタンス）\n
 * リーダについての状態情報を格納しています。
 *
 * @since 2008.02
 * @version 1.0
 */
READER_STATE readerState;
///最後に検査したカードの有無状態 ( SCARD_UNKNOWN or SCARD_ABSENT or SCARD_PRESENT )
static DWORD dwCurrentState = 0;
static short isRun = 0;


/**
 * @brief 取得したリーダの属性（カード状態、リーダ名等）を破棄
 *
 * @param[in] rContext リーダコンテキスト構造体
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_F_UNKNOWN_ERROR 内部エラー（カード状態取得失敗）。
 *
 * @since 2008.02
 * @version 1.0
 */
LONG EHDestroyEventHandler(PREADER_CONTEXT rContext)
{
	///リーダ属性管理構造体がコンテキスト \e rContext に登録されていない場合、
	if (NULL == rContext->readerState) {
		///−正常終了（リターン）
		return SCARD_S_SUCCESS;
	}

	///リーダ名がコンテキスト \e rContext のリーダ属性管理構造体に設定されていない場合、
	if ('\0' == rContext->readerState->readerName[0]) {
		///−正常終了（リターン）
		return SCARD_S_SUCCESS;
	}

	/*
	 * Zero out the public status struct to allow it to be recycled and
	 * used again 
	 */
	///コンテキスト \e rContext のリーダ属性管理構造体を初期化
	IDMan_StMemset(rContext->readerState->readerName, 0,
		sizeof(rContext->readerState->readerName));
	IDMan_StMemset(rContext->readerState->cardAtr, 0,
		sizeof(rContext->readerState->cardAtr));
	rContext->readerState->readerState = 0;
	rContext->readerState->cardAtrLength = 0;
	rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}


/**
 * @brief リーダの全ての属性（カード状態、リーダ名等）を初期化
 *
 * @param[in] rContext リーダコンテキスト構造体
 *
 * @retval SCARD_S_SUCCESS 成功。
 * @retval SCARD_F_UNKNOWN_ERROR 内部エラー（カード状態取得失敗）。
 *
 * @since 2008.02
 * @version 1.0
 */
INTERNAL LONG EHInitEventHandler(PREADER_CONTEXT rContext)
{
	LONG rv;
	DWORD dwStatus = 0;

	///カード状態を取得します。 IFDHICCPresence()
	rv = IFDHICCPresence(rContext->dwSlot);
	///カードあり(電源供給)の場合、
	if (rv == IFD_SUCCESS) {
		///−カード状態をカードありに設定
		dwStatus |= SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;
	} else
	if (rv == IFD_ICC_PRESENT) {
		///−カード状態をカードありに設定
		dwStatus |= SCARD_PRESENT;
	} else
	///カードなしの場合、
	if (rv == IFD_ICC_NOT_PRESENT) {
		///−カード状態をカードなしに設定
		dwStatus |= SCARD_ABSENT;
	}
	///状態取得に失敗した場合、
	else {
		///−内部エラー（リターン）
		return SCARD_F_UNKNOWN_ERROR;
	}

	/*
	 * 全ての属性をこのリーダのコンテキストに設定
	 */
	///リーダ属性管理構造体をコンテキスト \e rContext に登録
	rContext->readerState = &readerState;
	
	///取得したカード状態をコンテキスト \e rContext に設定
	{
		int i;
		char *dst = rContext->readerState->readerName;
		const char *src = rContext->lpcReader;
		int len = sizeof(rContext->readerState->readerName) - 1;
		for (i=0; src[i] && i<len; ++i) {
			dst[i] = src[i];
		}
		dst[i] = 0;
	}
	rContext->readerState->cardAtrLength = 0;
	rContext->readerState->readerState = dwStatus;
	rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

	///最後に検査したカードの有無状態　初期化
	dwCurrentState = 0;
	///カード遷移処理開始フラグ　初期化
	isRun = 0;

	///正常終了（リターン）
	return SCARD_S_SUCCESS;
}


/**
 * @brief リーダのカード状態遷移処理
 *
 * @param[in,out] rContext リーダコンテキスト構造体
 *
 * @since 2008.02
 * @version 1.0
 */
void EHStatusHandler(PREADER_CONTEXT rContext)
{
	LONG rv;
	LPCSTR lpcReader;

	///リーダが存在していない場合は処理しない（リターン）
	if (rContext->readerState==NULL)	return;

	///コンテキストよりリーダ名取得
	lpcReader = rContext->lpcReader;

	///最初のカード状態遷移処理の場合、
	if (!isRun) {
		///−リーダのカード状態を「カードなし」にします
		rContext->readerState->readerState |= SCARD_ABSENT;
		rContext->readerState->readerState &= ~(SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE | SCARD_SPECIFIC | SCARD_SWALLOWED | SCARD_UNKNOWN);
		///−コンテキストのカードATR長に 0 を設定
		rContext->readerState->cardAtrLength = 0;
		///−コンテキストのカードプロトコルを未設定に
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

		///−最後に検出したカード有無状態をカードなしに設定
		dwCurrentState = SCARD_ABSENT;

		isRun = 1;
	}

	///カードの有無を取得します。 IFDHICCPresence()
	rv = IFDHICCPresence(rContext->dwSlot);
	///カードあり（電力供給あり）の場合、
	if (rv == IFD_ICC_PRESENT_POWER_ON) {
		///−カード状態はすでに電力供給済み、処理なし
		/*
		 * この値はPCSC/CCIDライブラリ化におけるCCID独自仕様です。
		 * 通常仕様では IFDHICCPresence() はこの値を返さない。
		 */
	} else
	///カードあり（電力供給なし）の場合、
	if (rv == IFD_ICC_PRESENT) {
		/*
		 * Power and reset the card 
		 */
		///−スリープ 200msec
		void usleep ();	/* FIXME */
		usleep(500000);	/*usleep(200000);*/
		///−コンテキストのカードATR長に最大値を設定
		rContext->readerState->cardAtrLength = MAX_ATR_SIZE;
		///−カードへ電源供給します。 IFDHPowerICC()
		rv = IFDHPowerICC(rContext->dwSlot, IFD_POWER_UP,
				rContext->readerState->cardAtr,
				&rContext->readerState->cardAtrLength);

		///−電力供給後、コンテキストのカードプロトコルを未設定状態にします。
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;
		///−カードへの電力供給に成功した場合、
		if (rv == IFD_SUCCESS) {
			///−−リーダのカード状態を「カードあり＋電力供給中＋PTS交渉待ち」にします
			rContext->readerState->readerState |= SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE;
			rContext->readerState->readerState &= ~(SCARD_ABSENT | SCARD_SPECIFIC | SCARD_SWALLOWED | SCARD_UNKNOWN);

			/*
			 * Notify the card has been reset 
			 */
			///−−カードハンドルが存在する場合、
			if (rContext->psHandle.hCard != 0) {
				///−−−リーダコンテキストにカードリセットイベントを設定
				rContext->psHandle.dwEventStatus = SCARD_RESET;
			}
		}
		///−カードへの電力供給に失敗した場合、
		else
		{
			///−−リーダのカード状態を「カードあり＋電力供給待ち」にします
			rContext->readerState->readerState |= SCARD_PRESENT | SCARD_SWALLOWED;
			rContext->readerState->readerState &= ~(SCARD_ABSENT | SCARD_POWERED | SCARD_NEGOTIABLE | SCARD_SPECIFIC | SCARD_UNKNOWN);
			rContext->readerState->cardAtrLength = 0;
			rContext->readerState->cardAtr[0] = '\0';
		}
		///−最後に検出したカード有無状態を「カードあり」に設定
		dwCurrentState = SCARD_PRESENT;
	} else
	///カードなしの場合、
	if (rv == IFD_ICC_NOT_PRESENT) {
		///−最後に検出したカード有無状態が　カードありorカード状態不明　の場合、
		if (dwCurrentState == SCARD_PRESENT || dwCurrentState == SCARD_UNKNOWN) {
			/*
			 * Change the status structure 
			 */

			/*
			 * Notify the card has been removed 
			 */
			///−−カードハンドルが存在する場合、
			if (rContext->psHandle.hCard != 0) {
				///−−−コンテキストに「カードが抜かれた」イベントを設定
				rContext->psHandle.dwEventStatus = SCARD_REMOVED;
			}

			///−−コンテキストのカードATR長に 0 を設定
			rContext->readerState->cardAtrLength = 0;
			///−−コンテキストのカードプロトコルを未設定に
			rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;
			///−−コンテキストのリーダのカード状態を「カードなし」に設定
			rContext->readerState->readerState |= SCARD_ABSENT;
			rContext->readerState->readerState &= ~(SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE | SCARD_SPECIFIC | SCARD_SWALLOWED | SCARD_UNKNOWN);
			///−−最後に検出したカード有無状態を「カードなし」に設定
			dwCurrentState = SCARD_ABSENT;
		}
	}
	///取得に失敗した場合、
	else {
		///−コンテキストのリーダのカード状態に「カード状態不明」を設定
		rContext->readerState->readerState |= SCARD_UNKNOWN;
		rContext->readerState->readerState &= ~(SCARD_ABSENT | SCARD_PRESENT | SCARD_POWERED | SCARD_NEGOTIABLE | SCARD_SPECIFIC | SCARD_SWALLOWED);
		rContext->readerState->cardAtrLength = 0;
		rContext->readerState->cardProtocol = SCARD_PROTOCOL_UNSET;

		///−最後に検出したカード有無状態をカードありに設定
		dwCurrentState = SCARD_UNKNOWN;
	}
}
