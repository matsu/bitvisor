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
 * ID管理のCCIDドライバ：IFD ハンドラ関数\n
 * CCID のローレベル呼出を提供します。
 * @file IDMan_CcIfdhandler.c
 */

#include "IDMan_CcCcid.h"
#include "IDMan_PcIfdhandler.h"



/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
static int GetIFDErrCode(int CCIDErrCode);

static ReaderMng gReaderMng[READER_NUMBER_MAX];

/**
 * @brief ビットレート調整係数D
 */
int DI[16] = { 0, 1, 2, 4, 8, 16, 32, 64, 
					12, 20, 0, 0, 0, 0, 0, 0 };
/**
 * @brief クロックレート（周波数）変換係数F
 */
int FI[16] = { 372, 372, 558, 744, 1116, 1488, 1860, 0, 
					0, 512, 768, 1024, 1536, 2048, 0, 0 };


/**
 * @brief IFD handlerへの通信チャンネルをオープンします。\n
 * チャンネルがオープンすれば、リーダはカードの状態を
 * IFDHICCPresence()で求めることができます。
 *
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号\n
 * これはカードスロット、またはリーダが複数ある時に使用します。\n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号\n
 * Resource managerは自動的にこれらを設定します。
 * デフォルトでは、リーダがカードスロットを１つ以上持っていなければ
 * 全ての関数で Lun は無視され、資源管理は、ドライバの新しいインスタンスを
 * ロードします 。
 *
 * @param[in] Channel チャンネルID（USBリーダは未使用）
 * この値は次のようになります（参考）:
 * <ul>
 *   <li>0x000001 - /dev/pcsc/1
 *   <li>0x000002 - /dev/pcsc/2
 *   <li>0x000003 - /dev/pcsc/3
 * </ul>
 * USBリーダは、このパラメーターを無視し、バスの問い合わせを行います。
 *
 * @retval IFD_SUCCESS 成功。
 * @retval IFD_COMMUNICATION_ERROR 失敗。
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHCreateChannel( DWORD Lun, DWORD Channel )
{
	int UnitNum = (Lun & 0xFFFF0000) >> 16;

	///論理ユニット番号 \e Lun が間違っている場合、
	if (UnitNum >= READER_NUMBER_MAX) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}

	///USBポートのオープン OpenUSB()
	if (OpenUSB(gReaderMng, &gReaderMng[UnitNum]) != TRUE) {
		///−USBポートのオープンに失敗、通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	} else
	if ((IFDHICCPresence(Lun) == IFD_COMMUNICATION_ERROR)
		&& (IFDHICCPresence(Lun) == IFD_COMMUNICATION_ERROR)
		&& (IFDHICCPresence(Lun) == IFD_COMMUNICATION_ERROR))
	{
		CloseUSB(&gReaderMng[UnitNum]);
		return IFD_COMMUNICATION_ERROR;
	}

	///正常終了（リターン）
	return IFD_SUCCESS;
}


/**
 * @brief IFD handlerへの通信チャンネルを閉じます。\n
 * 通信チャンネルを閉じる前に、リーダは、カードの電源が落ちたことを確かめて、
 * 端末の電源を落とします。\n
 * （カード電力供給切断、リーダ端末リセット、USBクローズ等）
 *
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号\n
 * スロット番号は無視されます。\n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号
 *
 * @retval IFD_SUCCESS 成功。
 * @retval IFD_COMMUNICATION_ERROR 失敗。
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHCloseChannel( DWORD Lun )
{
	int UnitNum = (Lun & 0xFFFF0000) >> 16;
	int cnt;
	int iRet;

	///論理ユニット番号 \e Lun が間違っている場合、
	if (UnitNum >= READER_NUMBER_MAX) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}
	
	///受信タイムアウトを初期値に戻す。
	gReaderMng[UnitNum].ccid_mng.readTimeout = DEFAULT_READ_TIMEOUT;
	
	///カードへ電力が供給されている場合はOFFにします。
	for (cnt = 0 ; cnt < SLOTS_NUMBER_MAX ; ++cnt)
		if (gReaderMng[UnitNum].cards_mng[cnt].State == 2) {
			iRet = PowerOff(&gReaderMng[UnitNum], cnt);
			gReaderMng[UnitNum].cards_mng[cnt].Atr.DataLen = 0;
		}
	
	/// USB をクローズします。
	CloseUSB(&gReaderMng[UnitNum]);

	///リーダの USBハンドルをクリアします。
	gReaderMng[UnitNum].usb_mng.DevHandle = 0;

	///正常終了（リターン）
	return IFD_SUCCESS;
}


/**
 * @brief スロット/カードの能力情報を取得します。
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号（未使用）\n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号
 *
 * @param[in] Tag 情報要求タグ
 * タグは IDMan_PcIfdhandler.h で定義されています。
 * <table>
 *   <tr><td>TAG_IFD_SLOTS_NUMBER</td>		<td>リーダ端末のスロット数</td></tr>
 * </table>
 * @param[in,out] Length 要求データのサイズ\n
 * 要求データバッファ \e Value のサイズを設定します。\n
 * 戻る時に、要求データ長が設定されます。
 * @param[out] Value 要求データ用バッファ
 *
 * @note
 * \e Value のサイズ \e *Length は要求データを格納するのに充分な大きさであること。
 * タグのデータサイズはすべて 1バイトです。
 *
 * @retval IFD_SUCCESS 成功。
 * @retval IFD_COMMUNICATION_ERROR 失敗。
 * @retval IFD_ERROR_TAG 無効なタグです。
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHGetCapabilities( DWORD Lun, DWORD Tag, PDWORD Length, PUCHAR Value )
{
	int		UnitNum = (Lun & 0xFFFF0000) >> 16;
	uchar	SlotNum = Lun & 0x0000FFFF;

	///論理ユニット番号 \e Lun が間違っている場合、
	if (UnitNum >= READER_NUMBER_MAX || SlotNum >= SLOTS_NUMBER_MAX) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}
	///能力情報要求タグを検査します。
	if(Tag == TAG_IFD_SLOTS_NUMBER)
	{
		///−リーダ端末のスロット数取得タグの場合、
		///−−要求データバッファ長が 1以上の場合、
		if (Length && *Length >= 1) {
			///−−要求データ長を設定 \e *Length = 1
			*Length = 1;
			///−−リーダ端末のスロット数を設定 \e *Value = \b 1
			*Value = 1 ;
		}
		///−−要求処理終了
	}
	else
	{
		///−その他のタグの場合、
		///−無効なタグエラー（リターン）
		return IFD_ERROR_TAG;
	}

	///正常終了（リターン）
	return IFD_SUCCESS;
}

/**
 * @brief プロトコルおよび3つのPTSパラメータを送って、使用するカード/スロットの
 * プロトコル＆パラメータをセットします。
 *
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号\n
 * これはカードスロット、またはリーダが複数ある時に使用します。\n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号
 *
 * @param[in] Protocol CPU搭載カードプロトコル\n
 * 設定する値は IDMan_PcPcsclite.h で定義されています。
 *   @li SCARD_PROTOCOL_T0 - プロトコル T=0
 *   @li SCARD_PROTOCOL_T1 - プロトコル T=0
 * @param[in] Flags 交渉対象パラメータPTSの論理OR値
 *   @li IFD_NEGOTIATE_PTS1 - パラメータ PTS1交渉 (FI/DI)
 *   @li IFD_NEGOTIATE_PTS2 - パラメータ PTS2交渉
 *   @li IFD_NEGOTIATE_PTS3 - パラメータ PTS3交渉
 *
 * @param[in] PTS1 パラメータPTS1値
 * @param[in] PTS2 パラメータPTS2値
 * @param[in] PTS3 パラメータPTS3値
 *
 * @retval IFD_SUCCESS 成功。
 * @retval IFD_ERROR_PTS_FAILURE PTSに失敗しました。
 * @retval IFD_COMMUNICATION_ERROR 通信エラー（論理番号間違い、カードなし等を含む）
 * @retval IFD_PROTOCOL_NOT_SUPPORTED サポートしていないプロトコルです。
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHSetProtocolParameters( DWORD Lun, DWORD Protocol, UCHAR Flags,
										UCHAR PTS1, UCHAR PTS2, UCHAR PTS3)
{
	int				iRet;
	int				UnitNum = (Lun & 0xFFFF0000) >> 16;
	uchar			SlotNum = Lun & 0x0000FFFF;
	ATR*			Atr;
	uchar			Pps[CCID_MAX_LENGTH_PPS];
	CcidMng			*ccid_mng;
	DWORD			BaudRateCard;
	DWORD			BaudRateDefault;
#ifndef NTTCOM
	double			F;
	double			D;
#else
	long 			F;
	long 			D;
#endif
	unsigned char	TA1Buff;
	int				ProtocolDefault =-1;
	int				cnt;
	int				RecDataLen;
	ProtocolMngT1	*t1;
#ifndef NTTCOM
	double			etu;
	double			EGT;
	double			BWT;
	double			CWT;
	double			WWT;
#else
	long			etu;
	long			EGT;
	long			BWT;
	long			CWT;
	long			WWT;
#endif
	DWORD			TimeOut;

	///論理ユニット番号 \e Lun が間違っている場合、
	if (UnitNum >= READER_NUMBER_MAX || SlotNum >= SLOTS_NUMBER_MAX) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}

	///CPU搭載カードプロトコルが T=0 / T=1 のどちらでもない場合、
	if (Protocol != SCARD_PROTOCOL_T0 && Protocol != SCARD_PROTOCOL_T1) {
		///−未対応プロトコル（リターン）
		return IFD_PROTOCOL_NOT_SUPPORTED;
	}

	///カードがない場合、
	if (!gReaderMng[UnitNum].cards_mng[SlotNum].State) {
		///通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}

	/// 自動パラメータ交渉機能がある場合、
	if (gReaderMng[UnitNum].ccid_mng.dwFeatures & CCID_FEATURES_AUTO_PPS_PROP) {
		///−パラメータ設定不要のため正常終了。（リターン）
		return IFD_SUCCESS;
	}

	IDMan_StMemset(Pps, 0, sizeof(Pps));
	///プロトコル T=0 の場合、
	if (Protocol == SCARD_PROTOCOL_T0) {
		///−カードを初期化するプロトコル変数を 0 (T=0) に設定
		Pps[1] |= ATR_PROTOCOL_TYPE_T0;
	} else
	///プロトコル T=1 の場合、
	if (Protocol == SCARD_PROTOCOL_T1) {
		///−カードを初期化するプロトコル変数を 1 (T=1) に設定
		Pps[1] |= ATR_PROTOCOL_TYPE_T1;
	}
	///プロトコル T=0 以外の場合、
	else {
		///−未対応プロトコルエラー（リターン）
		return IFD_PROTOCOL_NOT_SUPPORTED;
	}
	
	ccid_mng = &(gReaderMng[UnitNum].ccid_mng);
	Atr = &(gReaderMng[UnitNum].cards_mng[SlotNum].Atr);
	/// ATR解析
	SetATR(&gReaderMng[UnitNum], SlotNum);

	///現在の動作モードを確認する。
	if (Atr->ib[1][ATR_INTERFACE_BYTE_TA].present)
	{
		///−固有モード（TA2が存在している）の場合、プロトコルを確認する。
		if( Pps[1] != ((Atr->ib[1][ATR_INTERFACE_BYTE_TA].value) & 0x0F))
		{
			///−−指定プロトコルと一致しない場合、
			///−−未対応プロトコルエラー（リターン）
			return IFD_PROTOCOL_NOT_SUPPORTED;
		}
		///交渉モード（基本動作モード）の場合は未処理
	}	

	///プロトコル T=1 の場合、
	if (Protocol == SCARD_PROTOCOL_T1) {
		///−リーダ／カードのプロトコル T=1 を初期化します
		InitLeaderCardProtocol(&gReaderMng[UnitNum], SlotNum);
	}

	/// PTS1パラメータがなく、TA1パラメータが存在する場合、
	if (Atr->ib[0][ATR_INTERFACE_BYTE_TA].present) {

		///−クロックレート変換値 F およびビットレート調整値 D を取得
		///−TA1の有無を確認
		if (Atr->ib[0][ATR_INTERFACE_BYTE_TA].present) 
		{
			///−−TA1が存在する場合、
#ifndef NTTCOM
			///−−TA1を基にクロックレート変換値 F を設定
			F =	(double)(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
			///−−TA1を基にビットレート調整値 D を設定
			D =	(double)(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#else
			///−−TA1を基にクロックレート変換値 F を設定
			F =	(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
			///−−TA1を基にビットレート調整値 D を設定
			D =	(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#endif
		}
		else
		{
			///−−TA1が存在しない場合、
#ifndef NTTCOM
			///−−デフォルトのクロックレート変換値 F を設定
			F =	(double)(FI[1]);
			///−−デフォルトのビットレート調整値 D を設定
			D =	(double)(DI[1]);
#else
			///−−デフォルトのクロックレート変換値 F を設定
			F =	(FI[1]);
			///−−デフォルトのビットレート調整値 D を設定
			D =	(DI[1]);
#endif
		}

		if ((0 == F) || (0 == D)) {
			F = 372;
			D = 1;
		}

		///−通信速度計算 BaudRate = F x D/F
		BaudRateCard = (DWORD)(1000 * ccid_mng->dwDefaultClock * D / F);
		BaudRateDefault = (DWORD)(1000 * ccid_mng->dwDefaultClock * 1 / 372);

		///−カードの通信速度が端末サポート範囲内の場合、
		if ((BaudRateCard > BaudRateDefault)
			&& (BaudRateCard <= ccid_mng->dwMaxDataRate))
		{
			///−−端末が通信速度テーブルを持っていない、またはカードと同じ通信速度が見つかった場合、
			if ((NULL == ccid_mng->arrayOfSupportedDataRates)
					|| FindBaudRateTable(BaudRateCard, ccid_mng->arrayOfSupportedDataRates)) {

				///−−−パラメータ PTS1として TA1の値を利用
				Pps[1] |= 0x10;
				Pps[2] = Atr->ib[0][ATR_INTERFACE_BYTE_TA].value;
			}
			///−−端末が通信速度テーブルを持っている場合、
			else {
				///−−− TA2が存在する場合、
				if (Atr->ib[1][ATR_INTERFACE_BYTE_TA].present) {
					///−−−−通信エラー（リターン）：端末はカードの通信速度をサポートしない
					return IFD_COMMUNICATION_ERROR;
				}
			}
		} else
		/*
		 * TA1
		 *   上位4bits= FI（クロックレート変換値インデックス）、
		 *   下位4bits=DI（ビットレート調整値インデックス）
		 * 例）0x97 => FI=9、DI=7
		 * 下記の処理は、DI値を7までサポートするため、DI値を6、5と変えて調整しています。
		 */
		///−カードの通信速度が端末よりも速く、TA1が 0x97以下で、端末が通信速度テーブルを持っている場合、
		if ((BaudRateCard > ccid_mng->dwMaxDataRate +2)
			&& (Atr->ib[0][ATR_INTERFACE_BYTE_TA].value <= 0x97)
			&& ccid_mng->arrayOfSupportedDataRates)
		{
			///−−オリジナル TA1値を退避
			TA1Buff = Atr->ib[0][ATR_INTERFACE_BYTE_TA].value;
			///−− TA1が 0x95以上のときの調整処理ループ
			while (Atr->ib[0][ATR_INTERFACE_BYTE_TA].value > 0x94) {
				///−−− TA1のビットレート調整値を１段下げます
				Atr->ib[0][ATR_INTERFACE_BYTE_TA].value--;


				///−−−クロックレート変換値 F およびビットレート調整値 D を取得
				///−−−TA1の有無を確認
				if (Atr->ib[0][ATR_INTERFACE_BYTE_TA].present) 
				{
					///−−−−TA1が存在する場合、
#ifndef NTTCOM
					///−−−−TA1を基にクロックレート変換値 F を設定
					F =	(double)(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
					///−−−−TA1を基にビットレート調整値 D を設定
					D =	(double)(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#else
					///−−−−TA1を基にクロックレート変換値 F を設定
					F =	(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
					///−−−−TA1を基にビットレート調整値 D を設定
					D =	(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#endif
				}
				else
				{
					///−−−−TA1が存在しない場合、
#ifndef NTTCOM
					///−−デフォルトのクロックレート変換値 F を設定
					F =	(double)(FI[1]);
					///−−−−デフォルトのビットレート調整値 D を設定
					D =	(double)(DI[1]);
#else
					///−−デフォルトのクロックレート変換値 F を設定
					F =	(FI[1]);
					///−−−−デフォルトのビットレート調整値 D を設定
					D =	(DI[1]);
#endif
				}

				///−−−カード通信速度計算　BaudRate = F x D/F
				BaudRateCard = (unsigned int) (1000 * ccid_mng->dwDefaultClock * D / F);

				///−−−カードと同じ通信速度が速度テーブルで見つからない場合、
				if (FindBaudRateTable(BaudRateCard, ccid_mng->arrayOfSupportedDataRates)==FALSE) {
					///−−−−調整処理ループ
					continue;
				}
				///−−−パラメータ PTS1として調整した TA1の値を利用
				Pps[1] |= 0x10;
				Pps[2] = Atr->ib[0][ATR_INTERFACE_BYTE_TA].value;
				break;
			}

			//−−オリジナル TA1値を戻す
			Atr->ib[0][ATR_INTERFACE_BYTE_TA].value = TA1Buff;
		}
	}

	/* Pps	[0] request/confirm = 0xFF
	 *		[1] b0-b3: 選択プロトコル
	 *			b4: PTS1あり（Pps[2])
	 *			b5: PTS2あり（Pps[3])
	 *			b6: PTS3あり（Pps[4])
	 *			b7: 予約(0)
	 *		[2] FI & DI (= TA1)
	 *		[3] b0: N=255サポート(1) ／ デフォルト(0)
	 *			b1: EGT(Extra Guardtime) = 12etu (1) ／デフォルト(0)
	 *			b2-b7: 予約(0)
	 *		[4] 
	 *		[5] PCK 検査用コード
	 */

	Pps[0] = 0xFF;	// 開始コード

	///端末に自動Pps機能がなく、TA2が存在しない（=negotiable mode）場合、
	if ((! (ccid_mng->dwFeatures & CCID_FEATURES_AUTO_PPS_CUR))
		&& (! Atr->ib[1][ATR_INTERFACE_BYTE_TA].present))
	{
		///−リーダ端末／カード間のデフォルト通信プロトコルを取得
		///−−デフォルトプロトコルを検索します。
		for (cnt=0; cnt<ATR_MAX_PROTOCOLS; cnt++)
			if (Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].present && (ProtocolDefault == -1))
			{
				ProtocolDefault = Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].value & 0x0F;
			}

		///−−TA2が存在する場合、（specific mode）は固有モードのプロトコルを利用
		if (Atr->ib[1][ATR_INTERFACE_BYTE_TA].present) {
			ProtocolDefault = Atr->ib[1][ATR_INTERFACE_BYTE_TA].value & 0x0F;
		}
		// デフォルトプロトコルが見つからない場合は初期値として T=0を利用
		if (ProtocolDefault == -1) {
			ProtocolDefault = ATR_PROTOCOL_TYPE_T0;
		}

		///−要求プロトコル≠デフォルトプロトコル、または \e PTS1 パラメータ指定ありの場合、
		if (((Pps[1] & 0x0F) != ProtocolDefault) || (Pps[1] & 0x10)) {
			///−−プロトコルパラメータ選択を実行がエラーとなった場合、
			if ((iRet=ExchangePPSData(&gReaderMng[UnitNum], SlotNum, Pps, &Pps[2])) != CCID_OK) {
				///−−−カードなしの場合、
				if (iRet == CCID_ERR_NO_CARD) {
					///−−−−リーダのカード状態をカードなしに設定
					gReaderMng[UnitNum].cards_mng[SlotNum].State = 0;
				}
				///−−プロトコルパラメータ選択エラーを返す（リターン）
				return IFD_ERROR_PTS_FAILURE;
			}
		}
	}

	///現在の動作モードを確認する。
	if (Atr->ib[1][ATR_INTERFACE_BYTE_TA].present)
	{
		///−固有モードの場合、ボーレート指定ありフラグを確認する。
		if((Atr->ib[1][ATR_INTERFACE_BYTE_TA].value) & 0x10)
		{
			///−−ボーレート指定ありフラグが0x10の場合、
			///−−通信エラー（リターン）を返す（リターン）
			return IFD_COMMUNICATION_ERROR;
		}
		///交渉モード（基本動作モード）の場合は未処理
	}	

	///指定プロトコルが T=1の場合、
	if (Protocol == SCARD_PROTOCOL_T1) {
		///−初期パラメータ準備	（仕様書PC_to_RDR_SetParameters参照）
		uchar ParamT1[7] = {
			0x11,	// bmFindexDindex		Fi/Di
			0x10,	// bmTCCKST1
			0x00,	// bGuardTimeT1
			0x4D,	// bmWaitingIntegersT1	BWI/CWI
			0x00,	// bClockStop
			0x20,	// bIFSC
			0x00	// bNadValue
		};
		t1 = &(gReaderMng[UnitNum].cards_mng[SlotNum].ProtocolT1);

		///−初期パラメータの FI & DI (PTS1) を変更
		if (Pps[1] & 0x10)	ParamT1[0] = Pps[2];

		///− CRC チェックサムの場合、パラメータにチェックサム型 CRCを設定
		// TC3 bit0: CRC(1) / LRC(0)
		if (t1->Edc == 2)	ParamT1[1] |= 0x01;

		///− ATRが反転規約の場合、パラメータに規約を設定
		if (Atr->TS == 0x3F)	ParamT1[1] |= 0x02;

		///−TC1の有無を確認する。
		if (Atr->ib[0][ATR_INTERFACE_BYTE_TC].present)
		{
			///−−TC1が存在する場合、
			///−− 拡張保護時間(Extra guard time=TC1)をパラメータに設定
			ParamT1[2] = (Atr->ib[0][ATR_INTERFACE_BYTE_TC].value);
		}
		else
		{
			///−−TC1が存在しない場合、
			///−− 拡張保護時間(0)をパラメータに設定
			ParamT1[2] = 0;
		}

		///− BWI/CWIをパラメータに設定
		ParamT1[3] = GetBWICWIData(Atr);	// bmWaitingIntegersT1

		//＝＝＝通信タイムアウト値 ccid_mng->readTimeout 計算＝＝＝


		///−クロックレート変換値 F およびビットレート調整値 D を取得
		///−TA1の有無を確認
		if (Atr->ib[0][ATR_INTERFACE_BYTE_TA].present) 
		{
			///−−TA1が存在する場合、
#ifndef NTTCOM
			///−−TA1を基にクロックレート変換値 F を設定
			F =	(double)(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
			///−−TA1を基にビットレート調整値 D を設定
			D =	(double)(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#else
			///−−TA1を基にクロックレート変換値 F を設定
			F =	(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
			///−−TA1を基にビットレート調整値 D を設定
			D =	(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#endif
		}
		else
		{
			///−−TA1が存在しない場合、
#ifndef NTTCOM
			///−−デフォルトのクロックレート変換値 F を設定
			F =	(double)(FI[1]);
			///−−デフォルトのビットレート調整値 D を設定
			D =	(double)(DI[1]);
#else
			///−−デフォルトのクロックレート変換値 F を設定
			F =	(FI[1]);
			///−−デフォルトのビットレート調整値 D を設定
			D =	(DI[1]);
#endif
		}

		///− ISOカードではない場合、
		if ((0 == F) || (0 == D) || (0 == ccid_mng->dwDefaultClock)) {
			ccid_mng->readTimeout = 60;
		} else {
		///− ISOカードの場合、
			///−− etu計算
			etu = F / D / ccid_mng->dwDefaultClock;

			///−− EGT(Extra Guard Time)計算
			EGT = 12 * etu + (F / D) * ParamT1[2] / ccid_mng->dwDefaultClock;

			///−− カードの BWT 計算
			BWT = 11 * etu + (1 << ((ParamT1[3] & 0xF0) >> 4)) * 960 * 372 / ccid_mng->dwDefaultClock;

			///−− カードの CWT 計算
			CWT = (11 + (1<<(ParamT1[3] & 0x0F))) * etu;

			///−−通信タイムアウト計算
			ccid_mng->readTimeout = 260*EGT + BWT + 260*CWT;
			ccid_mng->readTimeout = ccid_mng->readTimeout/1000 +1;
		}

		///− IFSCをパラメータに設定
		ParamT1[5] = GetIFSCData(Atr);

		///−パラメータコマンド発行 TransmitPPS()
		iRet = TransmitPPS(&gReaderMng[UnitNum], SlotNum, (char*)ParamT1, sizeof(ParamT1), (char*)ParamT1, &RecDataLen);
		if (iRet != CCID_OK) {
			return IFD_COMMUNICATION_ERROR;
		}
	}
	///指定プロトコルが T=0 の場合、
	else {
		uchar ParamT0[5] = {
			0x11,	/* Fi/Di			*/
			0x00,	/* TCCKS			*/
			0x00,	/* GuardTime		*/
			0x0A,	/* WaitingInteger	*/
			0x00	/* ClockStop		*/
		};

		///−初期パラメータの FI & DI (PTS1) を変更
		if (Pps[1] & 0x10)	ParamT0[0] = Pps[2];

		///− ATRが反転規約の場合、パラメータに規約を設定
		if (Atr->TS == 0x3F)	ParamT0[1] |= 0x02;

		///−TC1の有無を確認する。
		if (Atr->ib[0][ATR_INTERFACE_BYTE_TC].present)
		{
			///−−TC1が存在する場合、
			///−− 拡張保護時間(Extra guard time=TC1)をパラメータに設定
			ParamT0[2] = (Atr->ib[0][ATR_INTERFACE_BYTE_TC].value);
		}
		else
		{
			///−−TC1が存在しない場合、
			///−− 拡張保護時間(0)をパラメータに設定
			ParamT0[2] = 0;
		}

		///−TC2の有無を確認する。
		if (Atr->ib[1][ATR_INTERFACE_BYTE_TC].present)
		{
			///−−TC2が存在する場合、
			///−−待ち時間整数(Waiting time Integer=TC2)をパラメータに設定
			ParamT0[3] = Atr->ib[1][ATR_INTERFACE_BYTE_TC].value;
		}
		else
		{
			///−−TC2が存在しない場合、
			///−−待ち時間整数(10)をパラメータに設定
			ParamT0[3] = 10;
		}

		//＝＝＝通信タイムアウト値 ccid_mng->readTimeout 計算＝＝＝

		///−クロックレート変換値 F およびビットレート調整値 D を取得
		///−TA1の有無を確認
		if (Atr->ib[0][ATR_INTERFACE_BYTE_TA].present) 
		{
			///−−TA1が存在する場合、
#ifndef NTTCOM
			///−−TA1を基にクロックレート変換値 F を設定
			F =	(double)(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
			///−−TA1を基にビットレート調整値 D を設定
			D =	(double)(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#else
			///−−TA1を基にクロックレート変換値 F を設定
			F =	(FI[((Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4)]);
			///−−TA1を基にビットレート調整値 D を設定
			D =	(DI[(Atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F)]);
#endif
		}
		else
		{
			///−−TA1が存在しない場合、
#ifndef NTTCOM
			///−−デフォルトのクロックレート変換値 F を設定
			F =	(double)(FI[1]);
			///−−デフォルトのビットレート調整値 D を設定
			D =	(double)(DI[1]);
#else
			///−−デフォルトのクロックレート変換値 F を設定
			F =	(FI[1]);
			///−−デフォルトのビットレート調整値 D を設定
			D =	(DI[1]);
#endif
		}

		// may happen with non ISO cards
		///− ISOカードではない場合、
		if ((0 == F) || (0 == D) || (0 == ccid_mng->dwDefaultClock)) {
			ccid_mng->readTimeout = 60;
		} else {
		///− ISOカードの場合、
			///−− EGT(Extra Guard Time)計算
			EGT = 12 * F / D / ccid_mng->dwDefaultClock + (F / D) * ParamT0[2] / ccid_mng->dwDefaultClock;

			///−−カードの WWT(work waiting time)計算
			WWT = 960 * ParamT0[3] * F / ccid_mng->dwDefaultClock;

			///−−通信タイムアウト計算
			ccid_mng->readTimeout = DEFAULT_READ_TIMEOUT;
			TimeOut  = 261 * EGT + (3 + 3) * WWT;
			TimeOut = TimeOut/1000 +1;
			if (ccid_mng->readTimeout < TimeOut)
				ccid_mng->readTimeout = TimeOut;

			TimeOut = 5 * EGT + (1 + 259) * WWT;
			TimeOut = TimeOut/1000 +1;
			if (ccid_mng->readTimeout < TimeOut)
				ccid_mng->readTimeout = TimeOut;

		}

		///−パラメータコマンド発行 TransmitPPS()
		iRet = TransmitPPS(&gReaderMng[UnitNum], SlotNum, (char*)ParamT0, sizeof(ParamT0), (char*)ParamT0, &RecDataLen);
		if (iRet != CCID_OK)
			return IFD_COMMUNICATION_ERROR;
	}

	///指定プロトコルが T=1の場合、
	if (Protocol == SCARD_PROTOCOL_T1) {
		t1 = &(gReaderMng[UnitNum].cards_mng[SlotNum].ProtocolT1);

		///−カード情報フィールド長(IFSC)を設定
		t1->Ifsc = GetIFSCData(Atr);

		///−自動IFSD交換機能がない場合、
		if (! (ccid_mng->dwFeatures & CCID_FEATURES_AUTO_IFSD)) {
			///−− IFSD交換を実行 T1NegociateIFSD()
			if (T1NegociateIFSD(&gReaderMng[UnitNum], SlotNum, ccid_mng -> dwMaxIFSD) < 0)
				return IFD_COMMUNICATION_ERROR;
		}
		t1->Ifsd = ccid_mng->dwMaxIFSD;
	}

	gReaderMng[UnitNum].cards_mng[SlotNum].NewProtocol = (Protocol==SCARD_PROTOCOL_T1)? ATR_PROTOCOL_TYPE_T1: ATR_PROTOCOL_TYPE_T0;

	///正常終了（リターン）
	return IFD_SUCCESS;
}


/**
 * @brief 電力をコントロールし、Lunによって指定されたリーダ/スロットで
 * カード・リーダの信号をリセットします。\n
 * ATR用バッファ引数 \e Atr/AtrLength は必須となります。
 *
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号\n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号
 *
 * @param[in] Action カードへの動作指令
 * <ul>
 *   <li>IFD_POWER_UP - 電力を供給しATRとそのサイズを取得できなければ、
 * カードをリセットします。
 *   <li>IFD_POWER_DOWN - 電力が供給されているならば、カードの電力を切断します。
 * (ATRおよびそのサイズは 0 でなければならない)
 *   <li>IFD_RESET - カードを直ぐにリセットします。電力が供給されていない場合は
 * 電力を供給します。（ATRとそのサイズを返します）
 * </ul>
 * @param[out] Atr 取得した ATR(Answer to Reset) データ用バッファ。
 * （バッファ \e Atr のサイズ ≧ MAX_ATR_SIZE）
 * @param[out] AtrLength 取得した ATRデータ長。( \e *AtrLength ≦ MAX_ATR_SIZE )
 *
 * @retval IFD_SUCCESS 成功。
 * @retval IFD_ERROR_POWER_ACTION 電力供給エラー（ONまたはOFF）。
 * @retval IFD_COMMUNICATION_ERROR 通信エラー（リセットまたは電力切断のエラー等含む）。
 * @retval IFD_NOT_SUPPORTED サポートされていません。
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHPowerICC( DWORD Lun, DWORD Action, PUCHAR Atr, PDWORD AtrLength )
{
	int		UnitNum = (Lun & 0xFFFF0000) >> 16;
	uchar	SlotNum = Lun & 0x0000FFFF;
	int		iRet;
	int		OnVoltage = CCID_VOLTAGE_5V;
	

	///論理ユニット番号 \e Lun が間違っている場合、
	if (UnitNum >= READER_NUMBER_MAX || SlotNum >= SLOTS_NUMBER_MAX) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}
	///現カードプロトコルがメモリカードRAWプロトコルの場合、
	if (gReaderMng[UnitNum].cards_mng[SlotNum].NewProtocol == ATR_PROTOCOL_TYPE_T16) {
		///サポートされていません（リターン）
		return IFD_NOT_SUPPORTED;
	}

	/// ATR引数( \e Atr \& \e *AtrLength )の0クリア
	*AtrLength = 0;
	IDMan_StMemset(Atr, 0, MAX_ATR_SIZE);

	///動作指令を検査します。
	switch (Action) {
		///−リセット指令またはカードへの電力供給指令の場合、
		case IFD_RESET:
		case IFD_POWER_UP:
			///−−カードに電力が供給されていない場合、
			if (gReaderMng[UnitNum].cards_mng[SlotNum].State != 2) {
				///−−−カードに電力供給します PowerOn()
				iRet = PowerOn(&gReaderMng[UnitNum], SlotNum, OnVoltage);
				///−−−エラーの場合、
				if (iRet < 0) {
					///−−−−電力供給エラー（リターン）
					return IFD_ERROR_POWER_ACTION;
				}
				///−−−電力供給フラグを電力供給中に設定
				gReaderMng[UnitNum].cards_mng[SlotNum].State = 2;
			}
			///−−最後に取得した ATR応答データ長を設定 \e *AtrLength
			*AtrLength = gReaderMng[UnitNum].cards_mng[SlotNum].Atr.DataLen;
			///−−ATR応答データがある場合、
			if (*AtrLength) {
				///−−− ATR応答データをバッファ \e Atr に格納
				IDMan_StMemcpy(Atr, gReaderMng[UnitNum].cards_mng[SlotNum].Atr.Data, *AtrLength);
			}
			///−−プロトコル T=1を初期化 InitLeaderCardProtocol()
			InitLeaderCardProtocol(&gReaderMng[UnitNum], SlotNum);
			///−−動作指令処理終了
			break;
		///−電力供給切断指令の場合、
		case IFD_POWER_DOWN:
			///−−カードに電力が供給されている場合、
			if (gReaderMng[UnitNum].cards_mng[SlotNum].State == 2) {
				///−−−カードへの電力供給を切断します PowerOff()
				iRet = PowerOff(&gReaderMng[UnitNum], SlotNum);
				///−−−エラーの場合、
				if (iRet < 0) {
					///−−−−電力供給の切断エラー（リターン）
					return IFD_ERROR_POWER_ACTION;
				}
			}
			///−− リーダの ATRデータ長を 0 に設定します
			gReaderMng[UnitNum].cards_mng[SlotNum].Atr.DataLen = 0;
			///−−動作指令処理終了
			break;
		///−その他の動作指令の場合、
		default:
			///−−サポートされていません（リターン）
			return IFD_NOT_SUPPORTED;
	}

	///正常終了（リターン）
	return IFD_SUCCESS;
}


/**
 * @brief \e Lun で指定したカード/スロットとAPDU交換します。 \n
 * ドライバは、T=0/T=1のどちらのプロトコルでも交換を行ないます。 \n
 * この関数を呼ぶことで全てのプロトコルの違いを抽象化します。
 *
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号 \n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号
 *
 * @param[in] SendPci 送信するヘッダ構造体\n
 * 設定するSCARD_IO_HEADER構造体メンバ\n
 *  Protocol - ATR_PROTOCOL_TYPE_T0 または ATR_PROTOCOL_TYPE_T1 \n
 *  Length   - 未使用。
 *
 * @param[in] pTxBuffer 送信する APDU \n
 * 例 (0x00 0xA4 0x00 0x00 0x02 0x3F 0x00)
 * @param[in] TxLength 送信する APDU のサイズ。
 *
 * @param[in,out] pRxBuffer 受信 APDU \n
 * 例 (0x61 0x14)
 * @param[in,out] pRxLength 受信 APDU のサイズバッファへのポインタ。\n
 * この変数には \e pRxBuffer のバッファサイズを設定して渡し、
 * 戻り時には受信したAPDUのサイズが設定されます。\n
 * エラー時には 0 が設定されます。\n
 * Resource managerはセキュリティ上の理由で一時的なAPDUバッファを 0 にしますn
 *
 * @param[out] RecvPci 受信したヘッダ構造体（未使用）\n
 * 設定するSCARD_IO_HEADER構造体メンバ\n
 *  Protocol - ATR_PROTOCOL_TYPE_T0 または ATR_PROTOCOL_TYPE_T1 \n
 *  Length   - 未使用。
 *
 * @note
 * このドライバは、「DWG Smart-Card Integrated Circuit(s) Card Interface Devices Rev 1.1」の
 * 仕様を満たす端末をサポートします。
 *
 * @return エラーコード
 * @retval IFD_SUCCESS 成功。
 * @retval IFD_COMMUNICATION_ERROR
 * <ul>
 *   <li>送信データなし。
 *   <li>カードあり、電力が供給されていません。
 *   <li>送信エラー
 * </ul>
 *
 * @retval IFD_SUCCESS 正常終了。
 * @retval IFD_ICC_NOT_PRESENT カードなし。
 * @retval IFD_PROTOCOL_NOT_SUPPORTED 未対応プロトコルです。
 * @retval IFD_COMMUNICATION_ERROR 通信エラー。
 * @retval IFD_RESPONSE_TIMEOUT
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHTransmitToICC( DWORD Lun, SCARD_IO_HEADER SendPci, 
									PUCHAR pTxBuffer, DWORD TxLength, 
									PUCHAR pRxBuffer, PDWORD pRxLength, 
									PSCARD_IO_HEADER RecvPci )
{
	int			UnitNum = (Lun & 0xFFFF0000) >> 16;
	uchar		SlotNum = Lun & 0x0000FFFF;
	CcidMng *	ccid_mng;
	DWORD		rxBufLen = 0;
	int			iRet;

	///受信データ長バッファが未設定の場合、
	if (pRxLength==NULL) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}
	///論理ユニット番号 \e Lun が間違っている、または送信データがない場合、
	if (UnitNum >= READER_NUMBER_MAX || SlotNum >= SLOTS_NUMBER_MAX
			|| pTxBuffer == NULL || TxLength == 0) {
		///−受信データ長 \e *pRxLength を 0 に設定
		*pRxLength = 0;
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}
	///カードに電力が供給されていない場合、
	if (gReaderMng[UnitNum].cards_mng[SlotNum].State == 1) {
		///−受信データ長 \e *pRxLength を 0 に設定
		*pRxLength = 0;
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}
	///カードがない場合、
	if (gReaderMng[UnitNum].cards_mng[SlotNum].State == 0) {
		///−受信データ長 \e *pRxLength を 0 に設定
		*pRxLength = 0;
		///−カードなし（リターン）
		return IFD_ICC_NOT_PRESENT;
	}
	///カードの現在のプロトコルが指定プロトコルと一致しない場合、
	if (gReaderMng[UnitNum].cards_mng[SlotNum].NewProtocol != SendPci.Protocol) {
		///−受信データ長 \e *pRxLength を 0 に設定
		*pRxLength = 0;
		///−未サポートプロトコル（リターン）
		return IFD_PROTOCOL_NOT_SUPPORTED; 
	}

	//カードに電力が供給されている

	ccid_mng = &(gReaderMng[UnitNum].ccid_mng);
	///受信バッファサイズ退避
	rxBufLen = *pRxLength;

	/*交換レベルのおける転送方法分類
	 *	１、T=0 CHAR
	 *	２、T=1 CHAR/TPDU
	 *	３、short APDU ( T=0/1 共通 )
	 *		T=0 TPDU
	 *	４、extended APDU ( T=0/1 共通 )
	 */
	// 交換レベルを検査します。
	switch (ccid_mng->dwFeatures & CCID_FEATURES_EXCHANGE_MASK) {
	///交換レベル TPDU の場合、
	case CCID_FEATURES_TPDU:
		///−指定プロトコルが T=0 の場合、
		if (SendPci.Protocol == ATR_PROTOCOL_TYPE_T0) {
			///−− TPDUデータ送信 T0XfrBlockTPDU()
			DW_UL (pRxLength,
			iRet = T0XfrBlockTPDU(&gReaderMng[UnitNum], SlotNum,
						TxLength, pTxBuffer, pRxLength, pRxBuffer);
			);
			///−−内部エラーコードをIFDハンドラの値に変換
			iRet = GetIFDErrCode( iRet );
		} else
		///−指定プロトコルが T=1 の場合、
		if (SendPci.Protocol == ATR_PROTOCOL_TYPE_T1) {
			///−− TPDUデータ送信 T1XfrBlockTPDU()
			DW_UL (pRxLength,
			iRet = T1XfrBlockTPDU(&gReaderMng[UnitNum], SlotNum,
						TxLength, pTxBuffer, pRxLength, pRxBuffer);
			);
			///−−内部エラーコードをIFDハンドラの値に変換
			iRet = GetIFDErrCode( iRet );
		} else {
			return IFD_PROTOCOL_NOT_SUPPORTED;
		}
		break;

	///交換レベル SHORT-APDU の場合、
	case CCID_FEATURES_SHORT_APDU:
		///− APDU データ送信 T0XfrBlockTPDU()
		DW_UL (pRxLength,
		iRet = T0XfrBlockTPDU(&gReaderMng[UnitNum], SlotNum,
					TxLength, pTxBuffer, pRxLength, pRxBuffer);
		);
		///−内部エラーコードをIFDハンドラの値に変換
		iRet = GetIFDErrCode( iRet );
		break;

	///交換レベル EXTENDED-APDU の場合、
	case CCID_FEATURES_EXTENDED_APDU:
		///− Extended-APDUデータ送信 T1XfrBlockExtendedAPDU()
		DW_UL (pRxLength,
		iRet = T1XfrBlockExtendedAPDU(&gReaderMng[UnitNum], SlotNum,
			TxLength, (char*)pTxBuffer, (unsigned long*)pRxLength, (char*)pRxBuffer);
		);
		///−内部エラーコードをIFDハンドラの値に変換
		iRet = GetIFDErrCode( iRet );
		break;

	///交換レベル 文字 の場合、
	case CCID_FEATURES_CHARACTER:
		///−指定プロトコルが T=0 の場合、
		if (SendPci.Protocol == ATR_PROTOCOL_TYPE_T0) {
			///−− T=0 文字 データ送信 T0XfrBlockCHAR()
			DW_UL (pRxLength,
			iRet = T0XfrBlockCHAR(&gReaderMng[UnitNum], SlotNum,
						TxLength, pTxBuffer, pRxLength, pRxBuffer);
			);
			///−−内部エラーコードをIFDハンドラの値に変換
			iRet = GetIFDErrCode( iRet );
		} else
		///−指定プロトコルが T=1 の場合、
		if (SendPci.Protocol == ATR_PROTOCOL_TYPE_T1) {
			DW_UL (pRxLength,
			iRet = T1XfrBlockTPDU(&gReaderMng[UnitNum], SlotNum,
						TxLength, pTxBuffer, pRxLength, pRxBuffer);
			);

			///−−内部エラーコードをIFDハンドラの値に変換
			iRet = GetIFDErrCode( iRet );
		}
		///−プロトコルが T=0/T=1 以外の場合、
		else {
			///−−プロトコルがサポートされていないエラーを戻り値に設定
			iRet = IFD_PROTOCOL_NOT_SUPPORTED;
		}
		break;

	//ここは利用されない
	default:
		iRet = IFD_COMMUNICATION_ERROR;
	}
	///戻り値を返す（リターン）
	return iRet;
}


/**
 * @brief 論理ユニット番号 \e Lun で指定されたリーダと直接データ交換します。\n
 * \b CTBCS (Card Terminal Basic Command Set)仕様の命令を受け付けます。 \n
 * このドライバでは論理ユニット番号 \e Lun のカード排出のみサポートします。
 *
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号\n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号
 *
 * @param[in] TxBuffer 送信データ
 * @param[in] TxLength 送信データサイズ。
 * @param[in,out] RxBuffer 受信データ
 * @param[in,out] RxLength 受信データサイズ。\n
 * この変数には \e RxBuffer のバッファサイズを渡します。\n
 * 戻り時には受信したデータのサイズが設定されます。
 *
 * @note
 * 受信データ長 \e *RxLength はエラー時に 0 が設定されます。\n
 * このドライバではメモリカードをサポートしません。
 *
 * @return エラーコード
 * @retval IFD_SUCCESS 成功。
 * @retval IFD_COMMUNICATION_ERROR 処理失敗。
 * 通信エラー以外で起きる要因：
 * <ul>
 *   <li>電力が供給されていない。
 *   <li>電力供給OFFエラー。
 *   <li>カードがありません。
 * </ul>
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHControl( DWORD Lun,
						   PUCHAR TxBuffer, DWORD TxLength,
						   PUCHAR RxBuffer, PDWORD RxLength )
{

	int		iRet;
	///リーダ番号取得
	int		UnitNum = (Lun & 0xFFFF0000) >> 16;
	///スロット番号取得
	uchar	SlotNum = Lun & 0x0000FFFF;


	///論理ユニット番号 \e Lun が間違っている場合、
	if (UnitNum >= READER_NUMBER_MAX || SlotNum >= SLOTS_NUMBER_MAX) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}

	/// \b CTBCS (Card Terminal Basic Command Set)によるカード排出要求の場合、
	if (TxLength == 5 && TxBuffer[0] == 0x20/*CLA*/ && TxBuffer[1] == 0x15/*INS:EJECT ICC*/ &&
							  TxBuffer[3] == 0x00/*P2:no data for LCD&PAD*/ && TxBuffer[4] == 0x00) {
		///−カード電力供給をOFF
		iRet = PowerOff(&gReaderMng[UnitNum], SlotNum);
		///−エラーの場合、
		if (iRet < 0) {
			///−−受信データ長バッファ \e RxLength がNULLでない場合、
			if (RxLength) {
				///−−−受信したデータ長を0に設定 \e *RxLength = 0
				*RxLength = 0;
			}
			///−−通信エラー（リターン）
			return IFD_COMMUNICATION_ERROR;
		}
		///−リーダのカード ATR データ長を 0 にします。
		gReaderMng[UnitNum].cards_mng[SlotNum].Atr.DataLen = 0;
		///−受信バッファが 2バイト以上ある場合、
		if (RxLength && *RxLength >= 2) {
			///−−成功のステータスコードサイズを \e *RxLength に設定
			*RxLength = 2;
			///−−成功のステータスコード(0x90:0x00)を \e RxBuffer に設定
			RxBuffer[0] = 0x90;
			RxBuffer[1] = 0x00;
		}

		//−カード排出 (失敗は無視、サポートしていない)
		iRet = CardMechanical(&gReaderMng[UnitNum], SlotNum, 0x02);
	}

	///正常終了（リターン）
	return IFD_SUCCESS;
}


/**
 * @brief 論理ユニット番号 \e Lun で指定されたカードの有無を取得します
 *
 * @author University of Tsukuba
 *
 * @param[in] Lun 論理ユニット番号\n
 * これはカードスロット、またはリーダが複数ある時に使用します。\n
 * 0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号
 *
 * @retval IFD_ICC_PRESENT_POWER_ON カードあり（電力供給あり）。
 * @retval IFD_ICC_PRESENT カードあり（電力供給なし）。
 * @retval IFD_ICC_NOT_PRESENT カードなし。
 * @retval IFD_COMMUNICATION_ERROR 通信エラー（カード状態取得失敗）。
 *
 * @since 2008.02
 * @version 1.0
 */
RESPONSECODE IFDHICCPresence( DWORD Lun )
{
	int				iRet;
	int				UnitNum = (Lun & 0xFFFF0000) >> 16;
	uchar			SlotNum = Lun & 0x0000FFFF;
	unsigned int	ReadTimeOutBuffer;

	///論理ユニット番号 \e Lun が間違っている場合、
	if (UnitNum >= READER_NUMBER_MAX || SlotNum >= SLOTS_NUMBER_MAX) {
		///−通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}
	ReadTimeOutBuffer = gReaderMng[UnitNum].ccid_mng.readTimeout;
	gReaderMng[UnitNum].ccid_mng.readTimeout = DEFAULT_READ_TIMEOUT;

	///カード状態を取得します GetCardStatus()
	iRet = GetCardStatus(&gReaderMng[UnitNum], SlotNum);

	gReaderMng[UnitNum].ccid_mng.readTimeout = ReadTimeOutBuffer;

	///エラーの場合、
	if (iRet < 0) {
		///通信エラー（リターン）
		return IFD_COMMUNICATION_ERROR;
	}

	///リーダのカード状態を確認します。
	switch (gReaderMng[UnitNum].cards_mng[SlotNum].State) {
	///−カードあり（電力供給あり）（リターン）
	case 2:
		return IFD_ICC_PRESENT_POWER_ON;	//この値はPCSC/CCIDライブラリ化におけるCCID独自仕様です。

	///−カードあり（電力供給なし）（リターン）
	case 1:
		return IFD_ICC_PRESENT;

	///−カードなし（リターン）
	case 0:
	default:
		return IFD_ICC_NOT_PRESENT;
	}
}

/**
 * @brief 内部エラーコードをIFDハンドラの値へと変換します。
 * @author University of Tsukuba
 *
 * @param[in] CCIDErrCode 内部エラーコード
 *
 * @return IFDハンドラのエラーコード
 * @retval IFD_SUCCESS 正常終了。
 * @retval IFD_ICC_NOT_PRESENT カードなし。
 * @retval IFD_RESPONSE_TIMEOUT タイムアウト。
 * @retval IFD_ERROR_POWER_ACTION 電力供給エラー。
 * @retval IFD_COMMUNICATION_ERROR 通信エラー。
 *
 * @since 2008.02
 * @version 1.0
 */
static int GetIFDErrCode(int CCIDErrCode)
{
	int iRet;

	switch (CCIDErrCode) {
	case CCID_OK:
		iRet = IFD_SUCCESS;
		break;

	///カードなしの場合、
	case CCID_ERR_NO_CARD:
		///−戻り値 = IFDHカードなし
		iRet = IFD_ICC_NOT_PRESENT;
		break;
	///タイムアウトの場合、
	case CCID_ERR_READER_TIMEOUT:
	case CCID_ERR_TIMEOUT:
		///−戻り値 = IFDH通信エラー
		iRet = IFD_RESPONSE_TIMEOUT;
		break;

	///電力供給エラーの場合、
	case CCID_ERR_NO_POWERED_CARD:
		///−戻り値 = 電力供給エラー
		iRet = IFD_ERROR_POWER_ACTION;
		break;

	///通信エラーの場合、
	case CCID_ERR_COMMUNICATION:
	///その他
	default:
		///−戻り値 = IFDH通信エラー
		iRet = IFD_COMMUNICATION_ERROR;
		break;
	}
	return iRet;
}
