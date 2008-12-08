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
 * ID管理のPCSCライブラリ：プロトコル制御関数
 * \file IDMan_PcProthandler.c
 */

#include "IDMan_StandardIo.h"

#include "IDMan_PcMisc.h"
#include "IDMan_PcReaderfactory.h"
#include "IDMan_PcProthandler.h"


#ifndef NULL
# define NULL	0
#endif


#define SCARD_CONVENTION_DIRECT  0x0001
#define SCARD_CONVENTION_INVERSE 0x0002

	///スマートカード管理構造体
	typedef struct _SMARTCARD_EXTENSION
	{

		/// ATR(Answer-to-Reset) 管理構造体
		struct _ATR
		{
			DWORD Length;						///< ATR 文字数
			UCHAR Value[MAX_ATR_SIZE];			///< ATR 文字
			DWORD HistoryLength;				///<履歴文字数
			UCHAR HistoryValue[MAX_ATR_SIZE];	///<履歴文字
		} ATR;

		DWORD ReadTimeout;	///<読み込みタイムアウト

		/// カード機能構造体
		struct _CardCapabilities
		{
			UCHAR AvailableProtocols;	///<有効なプロトコル bitmask(SCARD_PROTOCOL_T0 / SCARD_PROTOCOL_T1)
			UCHAR CurrentProtocol;		///<現在のプロトコル
			UCHAR Convention;			///<通信規約 (SCARD_CONVENTION_DIRECT / SCARD_CONVENTION_INVERSE)
			USHORT ETU;

			///プロトコルパラメータ構造体
			struct _PtsData
			{
				UCHAR F1;
				UCHAR D1;
				UCHAR I1;
				UCHAR P1;
				UCHAR N1;
			} PtsData;

			/// プロトコル T=1 パラメータ構造体
			struct _T1
			{
				USHORT BGT;
				USHORT BWT;		///<ブロック待ち時間
				USHORT CWT;		///<文字待ち時間
				USHORT CGT;
				USHORT WT;
			} T1;

			/// プロトコル T=0 パラメータ構造体
			struct _T0
			{
				USHORT BGT;
				USHORT BWT;		///<ブロック待ち時間
				USHORT CWT;		///<文字待ち時間
				USHORT CGT;
				USHORT WT;
			}
			T0;

		} CardCapabilities;

		/*
		 * PREADER_CONNECTION psReaderConnection; 
		 */

	} SMARTCARD_EXTENSION, *PSMARTCARD_EXTENSION;


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +	スマートカードのATR(Answer-to-Reset)解析関数
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/**
 * @brief スマートカードからのATR(Answer-to-Reset)応答を解析します。
 *
 * @param psExtension スマートカード管理構造体
 * @param pucAtr 解析するATR(Answer-to-Reset)データ
 * @param dwLength ATR(Answer-to-Reset)データ長(\>1)
 * @return エラーコード
 * @retval 1 解読成功。
 * @retval 0 解読できない
 *
 * @since 2008.02
 * @version 1.0
 */
static short ATRDecodeAtr(PSMARTCARD_EXTENSION psExtension,
	PUCHAR pucAtr, DWORD dwLength)
{
	USHORT p;
	UCHAR K, TCK;				/* MSN of T0/Check Sum */
	UCHAR Y1i, T;				/* MSN/LSN of TDi */
	int i = 1;					/* value of the index in TAi, TBi, etc. */

	/*
	 * Zero out everything 
	 */
	p = K = TCK = Y1i = T = 0;
	
	///データ長＜ 2の場合、
	if (dwLength < 2)
		///−解読失敗エラー（リターン）
		return 0;	/* @retval 0 Atr must have TS and T0 */

	/*
	 * Zero out the bitmasks 
	 */
	/// \e psExtension のプロトコル認識をクリア
	psExtension->CardCapabilities.AvailableProtocols = SCARD_PROTOCOL_UNSET;
	psExtension->CardCapabilities.CurrentProtocol = SCARD_PROTOCOL_UNSET;

	///初期文字(TS)が反転規約の場合、
	if (pucAtr[0] == 0x3F) {
		///− \e psExtension のカード規約に反転規約を使用するように設定
		psExtension->CardCapabilities.Convention = SCARD_CONVENTION_INVERSE;
	}
	///初期文字(TS)が直接規約の場合、
	else
	if (pucAtr[0] == 0x3B) {
		///− \e psExtension のカード規約に直接規約を使用するように設定
		psExtension->CardCapabilities.Convention = SCARD_CONVENTION_DIRECT;
	}
	///初期化文字(TS)が間違っている場合、
	else
	{
		///− \e psExtension をクリア
		IDMan_StMemset(psExtension, 0x00, sizeof(SMARTCARD_EXTENSION));
		///−解読失敗エラー（リターン）
		return 0;	/* @retval 0 Unable to decode TS byte */
	}

	/*
	 * Here comes the platform dependant stuff 
	 */

	/*
	 * Decode the T0 byte 
	 */
	///インターフェース文字の有無取得
	Y1i = pucAtr[1] >> 4;	/* Get the MSN in Y1 */
	///履歴文字数取得
	K = pucAtr[1] & 0x0F;	/* Get the LSN in K */

	p = 2;

	/*
	 * Examine Y1 
	 */
	///インターフェース文字を全て取得するまでループ
	do
	{
		short TAi, TBi, TCi, TDi;	/* Interface characters */

		///−インターフェース文字取得 TAi/TBi/TCi/TDi
		TAi = (Y1i & 0x01) ? pucAtr[p++] : -1;
		TBi = (Y1i & 0x02) ? pucAtr[p++] : -1;
		TCi = (Y1i & 0x04) ? pucAtr[p++] : -1;
		TDi = (Y1i & 0x08) ? pucAtr[p++] : -1;

		/*
		 * Examine TDi to determine protocol and more 
		 */
		///− TDi が存在する場合、
		if (TDi >= 0) {
			///−−次のインターフェース文字の有無取得
			Y1i = TDi >> 4;	/* Get the MSN in Y1 */
			///−−プロトコルタイプ取得
			T = TDi & 0x0F;	/* Get the LSN in K */

			/*
			 * Set the current protocol TD1 (first TD only)
			 */
			///−−現在のプロトコルが未定義の場合、
			if (psExtension->CardCapabilities.CurrentProtocol == SCARD_PROTOCOL_UNSET) {
				switch (T) {
				///−−−取得したプロトコルタイプが T=0 の場合、
				case 0:
					///−−−−− \e psExtension の現在のプロトコルに T=0 を設定
					psExtension->CardCapabilities.CurrentProtocol = SCARD_PROTOCOL_T0;
					break;
				///−−−取得したプロトコルタイプが T=1 の場合、
				case 1:
					///−−−−− \e psExtension の現在のプロトコルに T=1 を設定
					psExtension->CardCapabilities.CurrentProtocol = SCARD_PROTOCOL_T1;
					break;
				///−−−取得したプロトコルタイプがその他の場合、
				default:
					///−−−−未対応プロトコルのため解読失敗エラー（リターン）
					return 0; /* @retval 0 Unable to decode LNS */
				}
			}

			///−−取得したプロトコルタイプが T=0 の場合、
			if (0 == T) {
				///−−− \e psExtension の有効プロトコルに T=0 を設定
				psExtension->CardCapabilities.AvailableProtocols |=
					SCARD_PROTOCOL_T0;
				///−−− \e psExtension のT=0時間変数初期化
				psExtension->CardCapabilities.T0.BGT = 0;
				psExtension->CardCapabilities.T0.BWT = 0;
				psExtension->CardCapabilities.T0.CWT = 0;
				psExtension->CardCapabilities.T0.CGT = 0;
				psExtension->CardCapabilities.T0.WT = 0;
			}
			else
			///−−取得したプロトコルタイプが T=1 の場合、
			if (1 == T) {
				///−−− \e psExtension の有効プロトコルに T=1 を設定
				psExtension->CardCapabilities.AvailableProtocols |=
					SCARD_PROTOCOL_T1;
				///−−− \e psExtension のT=1時間変数初期化
				psExtension->CardCapabilities.T1.BGT = 0;
				psExtension->CardCapabilities.T1.BWT = 0;
				psExtension->CardCapabilities.T1.CWT = 0;
				psExtension->CardCapabilities.T1.CGT = 0;
				psExtension->CardCapabilities.T1.WT = 0;
			}
			else
			//−−取得したプロトコルタイプが T=15 の場合、
			if (15 == T) {
				//−−− \e psExtension の有効プロトコルに T=15 を設定
				psExtension->CardCapabilities.AvailableProtocols |=
					SCARD_PROTOCOL_T15;
			}
			else {
				/*
				 * Do nothing for now since other protocols are not
				 * supported at this time 
				 */
			}

			/* test presence of TA2 */
			///−− TA2 が存在する場合、
			if ((2 == i) && (TAi >= 0)) {
				///−−−固有(SPECIFIC)モードのプロトコルタイプ取得
				T = TAi & 0x0F;
				switch (T) {
				///−−−取得したプロトコルタイプが T=0 の場合、
				case 0:
					///−−−− \e psExtension の現在のプロトコルおよび有効プロトコルに T=0 を設定
					psExtension->CardCapabilities.CurrentProtocol =
						psExtension->CardCapabilities.AvailableProtocols =
						SCARD_PROTOCOL_T0;
					break;

				///−−−取得したプロトコルタイプが T=1 の場合、
				case 1:
					///−−−− \e psExtension の現在のプロトコルおよび有効プロトコルに T=1 を設定
					psExtension->CardCapabilities.CurrentProtocol =
						psExtension->CardCapabilities.AvailableProtocols =
						SCARD_PROTOCOL_T1;
					break;

				///−−−取得したプロトコルタイプがその他の場合、
				default:
					///−−−−未対応プロトコルのため解読失敗エラー（リターン）
					return 0; /* @retval 0 Unable do decode T protocol */
				}
			}
		}
		///− TDi が存在しない場合、
		else {
			///−−次のインターフェース文字なし（インターフェース文字解析終了）
			Y1i = 0;
		}

		///取得インターフェース文字数がATRの最大文字数を超えた場合、
		if (p > MAX_ATR_SIZE) {
			///−− \e psExtension をクリア
			IDMan_StMemset(psExtension, 0x00, sizeof(SMARTCARD_EXTENSION));
			///−−解読失敗エラー（リターン）
			return 0;	/* @retval 0 Maximum attribute size */
		}
		/* next interface characters index */
		///−インターフェース文字インデックス i カウント
		i++;
	} while (Y1i != 0);

	///プロトコルが取得できなかった場合、
	if (psExtension->CardCapabilities.CurrentProtocol == SCARD_PROTOCOL_UNSET) {
		///− \e psExtension の現在のプロトコルおよび有効プロトコルに T=0 を設定
		psExtension->CardCapabilities.CurrentProtocol = SCARD_PROTOCOL_T0;
		psExtension->CardCapabilities.AvailableProtocols |= SCARD_PROTOCOL_T0;
	}

	/*
	 * Take care of the historical characters 
	 */
	/// \e psExtension に履歴文字数を設定
	psExtension->ATR.HistoryLength = K;
	/// \e psExtension に履歴文字を取得して格納
	IDMan_StMemcpy(psExtension->ATR.HistoryValue, &pucAtr[p], K);

	p = p + K;

	/*
	 * Check to see if TCK character is included It will be included if
	 * more than T=0 is supported 
	 */
	///有効プロトコルが T=1 の場合、
	if (psExtension->CardCapabilities.AvailableProtocols & SCARD_PROTOCOL_T1) {
		///−チェック文字(TCK)を取得
		TCK = pucAtr[p++];
	}
	/// \e psExtension にATR(Answer-to-Reset)文字を格納
	IDMan_StMemcpy(psExtension->ATR.Value, pucAtr, p);
	/// \e psExtension にATR(Answer-to-Reset)文字数を格納
	psExtension->ATR.Length = p;	/* modified from p-1 */

	///正常終了（リターン）
	return 1; /* @retval 1 Success */
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +	プロトコル制御関数
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/**
 * @brief リセット後すぐに使用されるデフォルトプロトコルと
 * カードでサポートされている有効プロトコルを返します。
 *
 * @date 2007/09/20
 * @param[in] pucAtr ATR(Answer-to-Reset)データ
 * @param[in] dwLength ATRデータ長
 * @param[out] pucDefaultProt デフォルトプロトコル
 * @param[out] pucAvailProt カードでサポートされているプロトコル
 * @retval 0 正常終了
 * @retval GET_PROTOCOL_WRONG_ARGUMENT ポインタ引数 pucDefaultProt または pucAvailProt がNULL
 *
 * @since 2008.02
 * @version 1.0
 */
INTERNAL DWORD PHGetProtocol(PUCHAR pucAtr, DWORD dwLength, PUCHAR pucDefaultProt, PUCHAR pucAvailProt)
{
	SMARTCARD_EXTENSION sSmartCard;

	if (pucDefaultProt==NULL || pucAvailProt == NULL)
		return GET_PROTOCOL_WRONG_ARGUMENT;
	/*
	 * カード機能構造体を初期化
	 */
	IDMan_StMemset(&sSmartCard, 0x00, sizeof(SMARTCARD_EXTENSION));

	/// ATR解析が成功した場合、 ATRDecodeAtr()
	if (ATRDecodeAtr(&sSmartCard, pucAtr, dwLength))
	{
		///−有効なプロトコルを設定 \e *pucAvailProt
		*pucAvailProt = sSmartCard.CardCapabilities.AvailableProtocols;
		///−デフォルトプロトコルを設定 \e *pucDefaultProt
		*pucDefaultProt = sSmartCard.CardCapabilities.CurrentProtocol;
		return 0;
	}
	/// ATR解析が失敗した場合、
	else
	{
		///−プロトコルを 0 クリア \e *pucAvailProt および \e *pucDefaultProt
		*pucAvailProt = 0x00;
		*pucDefaultProt = 0x00;
		return 0;
	}
}


/**
 * @brief 使用するプロトコルを設定。
 * SCardConnectは、使用するプロトコルのビットマスク \e dwPreferredProtocols で指定します。
 * 基本的に、それが利用可能 \e ucAvailable ならば、 T=1 が最初に使用されます。
 * そうでないならば、デフォルト値 T=0 が使用されます。
 *
 * @param[in] rContext リーダコンテキスト構造体
 * @param[in] dwPreferred 要求プロトコル
 * @param[in] ucAvailable 有効プロトコル
 * @param[in] ucDefault デフォルトプロトコル
 * <ul>
 *   <li>SCARD_PROTOCOL_T0 - プロトコル T=0</li>
 *   <li>SCARD_PROTOCOL_T1 - プロトコル T=1</li>
 * </ul>
 * @return 設定できたプロトコル
 * @retval GET_PROTOCOL_WRONG_ARGUMENT 引数エラー。
 *
 * @since 2008.02
 * @version 1.0
 */
INTERNAL DWORD PHSetProtocol(struct ReaderContext * rContext,
	DWORD dwPreferred, UCHAR ucAvailable, UCHAR ucDefault)
{
	DWORD protocol;
	LONG rv;
	UCHAR ucChosen;

	/* App has specified no protocol */
	///プロトコル \e dwPreferred が指定されていない場合、
	if (dwPreferred == 0) {
		///−引数エラー（リターン）
		return SET_PROTOCOL_WRONG_ARGUMENT;
	}

	///要求されたプロトコル \e dwPreferred が有効プロトコルではない場合、
	if (! (dwPreferred & ucAvailable))
	{
		/* Note:
		 * dwPreferred must be either SCARD_PROTOCOL_T0 or SCARD_PROTOCOL_T1
		 * if dwPreferred == SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1 the test
		 * (SCARD_PROTOCOL_T0 == dwPreferred) will not work as expected
		 * and the debug message will not be correct.
		 *
		 * This case may only occur if
		 * dwPreferred == SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1
		 * and ucAvailable == 0 since we have (dwPreferred & ucAvailable) == 0
		 * and the case ucAvailable == 0 should never occur (the card is at
		 * least T=0 or T=1)
		 */
		///−引数エラー（リターン）
		return SET_PROTOCOL_WRONG_ARGUMENT;
	}

	///デフォルトプロトコルを設定（サポートされていない場合に返す戻り値）
	protocol = ucDefault;

	/* keep only the available protocols */
	///要求プロトコルの中で有効なもののみ取得
	dwPreferred &= ucAvailable;

	/* we try to use T=1 first */
	///取得した要求プロトコルに T=1 が存在する場合、
	if (dwPreferred & SCARD_PROTOCOL_T1) {
		///−設定するプロトコルに T=1 を使用
		ucChosen = SCARD_PROTOCOL_T1;
	} else
	///取得した要求プロトコルに T=0 が存在する場合、
	if (dwPreferred & SCARD_PROTOCOL_T0) {
		///−設定するプロトコルに T=0 を使用
		ucChosen = SCARD_PROTOCOL_T0;
	}
	///取得した要求プロトコルに T=0/T=1 が存在しない場合、
	else {
		///−サポート対象外で引数エラー（リターン）
		return SET_PROTOCOL_WRONG_ARGUMENT;
	}

	/// PTS設定 IFDHSetProtocolParameters()
	rv = IFDHSetProtocolParameters(rContext->dwSlot, ucChosen,
		0x00, 0x00, 0x00, 0x00);
	///設定に成功した場合、
	if (IFD_SUCCESS == rv) {
		///−設定したプロトコルを取得
		protocol = ucChosen;
	} else
	///その他のエラーの場合、
	{
		/* ISO 7816-3:1997 ch. 7.2 PPS protocol page 14
		 * - PPS交換が失敗した場合、インターフェース機器は
		 *   リセットするかカードを排出することとします。
		 */
		///−プロトコル設定エラー（リターン）\n
		return SET_PROTOCOL_PPS_FAILED;
	}

	///プロトコルを返す（リターン）
	return protocol;
}
