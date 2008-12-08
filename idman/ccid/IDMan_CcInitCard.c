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
 * ID管理のCCIDドライバ：カード初期化関数
 * \file IDMan_CcInitCard.c
 */

#include "IDMan_CcCcid.h"


/**
 * @brief 対象通信速度をテーブルから検索し、その有無を返します。
 * @author University of Tsukuba
 *
 * @param[in] BaudRate 検索対象通信速度
 * @param[in] Table 通信速度テーブルのポインタ
 * テーブルの最後を示すデータは 0です。
 *
 * @retval TRUE あり。
 * @retval FALSE なし。
 *
 * @since 2008.02
 * @version 1.0
 */
int FindBaudRateTable(unsigned long BaudRate, unsigned long *Table)
{
	int cnt;

	///通信速度検索ループ（テーブルの値が 0で終了）
	for (cnt=0; Table[cnt]; cnt++) 
	{
		///−（テーブル値 +2) \> 対象通信速度 \> （テーブル値 -2） の場合、
		if ((BaudRate < (Table[cnt] + 2)) && (BaudRate > (Table[cnt] - 2))) 
		{
			///−−データあり（リターン）
			return TRUE;
		}
	}

	///データなし（リターン）
	return FALSE;
}


/**
 * @brief 対象通信速度をテーブルから検索し、その有無を返します。
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] PpsData 検索対象通信速度
 * @param[out] PTS1 応答 TA1を格納するバッファ
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_SEND_LEN 送信データ長エラー
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込みエラー。
 * @retval CCID_ERR_CANCEL 取り消しエラー。
 * @retval CCID_ERR_TIMEOUT タイムアウトエラー。
 * @retval CCID_ERR_PARITY_CHECK パリティエラー。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 * @retval CCID_ERR_INSUFFICIENT_BUFFER バッファ不足。
 * @retval CCID_ERR_NO_MATCHING パラメータ不一致。
 *
 * @since 2008.02
 * @version 1.0
 */
int ExchangePPSData(ReaderMng* gReder, char SlotNum, uchar *PpsData, uchar *PTS1)
{
	char 			ReplyData[CCID_MAX_LENGTH_PPS];
	int				cnt;
	int				iRet;
	unsigned long	TransmitLen;
	unsigned long	ReplyLen;

	///パラメータ数取得
	TransmitLen = 3;
	if (PpsData[1] & 0x10)	TransmitLen++;
	if (PpsData[1] & 0x20)	TransmitLen++;
	if (PpsData[1] & 0x40)	TransmitLen++;

	///パラメータEDC計算
	PpsData[TransmitLen - 1] = PpsData[0];
	for (cnt = 1; cnt < (TransmitLen - 1); cnt++) {
		PpsData[TransmitLen - 1] ^= PpsData[cnt];
	}

	/// PPSコマンド送信
	iRet = TransmitCommand(gReder, SlotNum, TransmitLen, (const char*)PpsData,
		(gReder->ccid_mng.dwFeatures & CCID_FEATURES_EXCHANGE_MASK) ? 4 : 0, 0);
	///送信エラーの場合、エラーコードを返す（リターン）
	if (iRet != CCID_OK)	return iRet;

	/// PPS応答受信
	ReplyLen = sizeof(ReplyData);
	iRet = ReceiveReply(gReder, SlotNum, &ReplyLen, ReplyData, NULL);
	///受信エラーの場合、エラーコードを返す（リターン）
	if (iRet != CCID_OK)	return iRet;
	
	///要求PPSパラメータと応答パラメータが一致しない場合、
	if ((TransmitLen == ReplyLen) && IDMan_StMemcmp(PpsData, ReplyData, TransmitLen)) {
		///−戻り値にエラーコードを設定する。
		iRet = CCID_ERR_NO_MATCHING;
	} else
	if (TransmitLen < ReplyLen)
		iRet = CCID_ERR_NO_MATCHING;
	else
	if ((ReplyData[1] & 0x10) && (ReplyData[2] != PpsData[2]))
		iRet = CCID_ERR_NO_MATCHING;

	///デフォルト TA1 を \e *PTS1 に格納
	*PTS1 = 0x11;

	/// 要求PPSパラメータおよび応答PPSパラメータに PTS1パラメータが設定されている場合、
	if ((PpsData[1] & 0x10) && (ReplyData[1] & 0x10)) {
		///−応答パラメータの PTS1パラメータを \e *PTS1 に格納
		*PTS1 = ReplyData[2];
	}

	///応答PPSパラメータを \e PpsData に格納
	IDMan_StMemcpy(PpsData, ReplyData, ReplyLen);

	///終了（リターン）
	return iRet;
}
