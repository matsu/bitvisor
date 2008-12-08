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
 * ID管理のCCIDドライバ：ATR解析関数
 * \file IDMan_CcAtr.c
 */

#include "IDMan_CcCcid.h"


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#ifndef INVERSE_DATA
#define INVERSE_DATA(a)		((((a) << 7) & 0x80) | \
				(((a) << 5) & 0x40) | \
				(((a) << 3) & 0x20) | \
				(((a) << 1) & 0x10) | \
				(((a) >> 1) & 0x08) | \
				(((a) >> 3) & 0x04) | \
				(((a) >> 5) & 0x02) | \
				(((a) >> 7) & 0x01))
#endif

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

static int ibList[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };

/**
 * @brief ATRデータを解析し、ATR 管理構造体に格納します。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] AtrData ATRデータ
 * @param[in] AtrLen ATRデータ長
 *
 * @return エラーコード
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_ATR_CHECK 解析失敗。
 *
 * @since 2008.02
 * @version 1.0
 */
int SetATR(ReaderMng* gReder, char SlotNum)
{
	int				cnt = 0;
	uchar			AtrDataBuf[MAX_ATR_SIZE];
	ATR*			Atr = &(gReder->cards_mng[(int)SlotNum].Atr);
	uchar*			AtrData = Atr->Data;
	unsigned int	AtrLen  = Atr->DataLen;
	uchar			TDi;
	int				ProtocolNum = 0;

	/// ATRデータ長が不十分な場合（少なくとも TS:T0 は存在）、
	if (AtrLen < 2) {
		///−エラーを返す。（リターン）
		return CCID_ERR_ATR_CHECK;
	}

	/// ATRの反転規約(inverse convention)の場合、
	if (AtrData[0] == 0x03) {
		///− ATRをデコードして ATR 構造体に格納します。
		for (cnt = 0; cnt < AtrLen; cnt++) {
			AtrDataBuf[cnt] = ~(INVERSE_DATA (AtrData[cnt]));
		}
	}
	/// ATRが直接規約(direct convention)の場合、
	else {
		///− ATRを ATR 構造体に格納します。
		IDMan_StMemcpy(AtrDataBuf, AtrData, AtrLen);
	}
	/// TS を格納する。
	Atr->TS = AtrDataBuf[0];

	// TSは 0x3B(正) / 0x3F(反) 指定のみ
	/// TS が規約外の場合、
	if ((Atr->TS != 0x3B) && (Atr->TS != 0x3F)) {
		///−エラーを返す。（リターン）
		return CCID_ERR_ATR_CHECK;
	}

	/// T0 を格納する。
	cnt = 1;
	Atr->T0 = TDi = AtrDataBuf[1];

	///履歴文字数を格納する。
	Atr->hbn = TDi & 0x0F;

	// TCK(チェック文字)はデフォルト(T0)で存在しない
	(Atr->TCK).present = 0;

	///インターフェース文字を展開ループ。
	while (cnt < AtrLen) {
		///− ATRデータが足りない場合、
		if (cnt + ibList[(0xF0 & TDi) >> 4] >= AtrLen) {
			///−−エラーを返す。（リターン）
			return CCID_ERR_ATR_CHECK;
		}
		///− TAi が存在する場合、
		if (TDi & 0x10) {
			///−−インターフェース文字 TAi を ATR 構造体に格納
			cnt++;
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TA].value = AtrDataBuf[cnt];
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TA].present = 1;
		} else {
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TA].present = 0;
		}

		///− TBi が存在する場合、
		if (TDi & 0x20) {
			///−−インターフェース文字 TBi を ATR 構造体に格納
			cnt++;
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TB].value = AtrDataBuf[cnt];
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TB].present = 1;
		} else {
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TB].present = 0;
		}

		///− TCi が存在する場合、
		if (TDi & 0x40) {
			///−−インターフェース文字 TCi を ATR 構造体に格納
			cnt++;
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TC].value = AtrDataBuf[cnt];
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TC].present = 1;
		} else {
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TC].present = 0;
		}

		///− TDi が存在する場合、
		if (TDi & 0x80) {
			///−−インターフェース文字 TDi を ATR 構造体に格納
			cnt++;
			TDi = Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TD].value = AtrDataBuf[cnt];
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TD].present = 1;
			(Atr->TCK).present = ((TDi & 0x0F) != ATR_PROTOCOL_TYPE_T0);
			if (ProtocolNum >= ATR_MAX_PROTOCOLS)
				return CCID_ERR_ATR_CHECK;
			ProtocolNum++;
		}
		///−継続データがない場合、
		else {
			Atr->ib[ProtocolNum][ATR_INTERFACE_BYTE_TD].present = 0;
			///−−ループ終了
			break;
		}
	}

	///プロトコル数を格納する。
	Atr->pn = ProtocolNum + 1;

	/// ATRデータに履歴文字全てが存在しない場合、
	if ((cnt + Atr->hbn) >= AtrLen) {
		///−エラーを返す。（リターン）
		return CCID_ERR_ATR_CHECK;
	}

	///履歴文字を格納する。
	IDMan_StMemcpy(Atr->hb, AtrDataBuf + cnt + 1, Atr->hbn);
	cnt += (Atr->hbn);

	/// TCKが存在する場合に格納する。
	if ((Atr->TCK).present) {
		cnt++;
		if (cnt >= AtrLen)
			return CCID_ERR_ATR_CHECK;
		(Atr->TCK).value = AtrDataBuf[cnt];
	}

	/// ATR文字数を格納する。
	Atr->DataLen = cnt + 1;
	return CCID_OK;
}


/**
 * @brief カードの情報フィールド長を取得します。
 * @author University of Tsukuba
 *
 * @param[in] Atr ATR管理構造体
 *
 * @return カードの情報フィールド長(Information Field Size Card)
 *
 * @since 2008.02
 * @version 1.0
 */
uchar GetIFSCData(ATR* Atr)
{
	int cnt;

	for ( cnt = 1 ; cnt < Atr->pn ; ++cnt )
	{
		/* TDiで最初の T=1 を見つける */
		if  (Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].present && 
			(Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1) 
		{
			if (Atr->ib[cnt + 1][ATR_INTERFACE_BYTE_TA].present)
			{
				return Atr->ib[cnt + 1][ATR_INTERFACE_BYTE_TA].value;
			}
			else
			{
				return 32;
			}
		}
	}
	return 32;
}


/**
 * @brief ブロック待ち時間整数(Block Waiting time Integer)および \n
 * @author University of Tsukuba
 *
 * 文字待ち時間整数(Character Waiting time Integer)を取得します。
 * @param[in] Atr ATR管理構造体
 *
 * @return ブロック待ち時間整数（BWI:b7-b4、CWI:b3-b0）
 *
 * @since 2008.02
 * @version 1.0
 */
uchar GetBWICWIData(ATR* Atr)
{
	int cnt;

	for ( cnt = 1 ; cnt < Atr->pn ; ++cnt )
	{
		/* TDi(cnt>1)で最初の T=1 を見つける */
		if  (Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].present && 
			(Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1)
		{
			if (Atr->ib[cnt + 1][ATR_INTERFACE_BYTE_TB].present)
			{
				return (Atr->ib[cnt + 1][ATR_INTERFACE_BYTE_TB].value);
			}
			else
			{
				return 0x4D;
			}
		}
	}
	return 0x4D;
}


