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
 * ID管理のCCIDドライバ：リーダ端末制御関数
 * \file IDMan_CcReaderControl.c
 */

#include "IDMan_CcCcid.h"

static int CheckBulkInMsg(uchar *Msg, CardMng *card_mng, int chkPresence);

/*
 *	Slot Error (bError)
 */
#define RDR_ERR_CMD_ABORTED					0xFF
#define RDR_ERR_ICC_MUTE					0xFE
#define RDR_ERR_XFR_PARITY_ERROR			0xFD
#define RDR_ERR_XFR_OVERRUN					0xFC
#define RDR_ERR_HW_ERROR					0xFB
#define RDR_ERR_BAD_ATR_TS					0xF8
#define RDR_ERR_BAD_ATR_TCK					0xF7
#define RDR_ERR_ICC_PROTOCOL_NOT_SUPPORTED	0xF6
#define RDR_ERR_ICC_CLASS_NOT_SUPPORTED		0xF5
#define RDR_ERR_PROCEDURE_BYTE_CONFLICT		0xF4
#define RDR_ERR_DEACTIVATED_PROTOCOL		0xF3
#define RDR_ERR_BUSY_WITH_AUTO_SEQUENCE		0xF2
#define RDR_ERR_PIN_TIMEOUT					0xF0
#define RDR_ERR_PIN_CANCELLED				0xEF
#define RDR_ERR_CMD_SLOT_BUSY				0xE0
#define RDR_ERR_SLOT_NOT_EXIST				0x05
#define RDR_ERR_CMD_NOT_SUPPORTED			0x00


#define CCID_BULK_MSG_HDR_SIZE		10	///< CCID バルクメッセージヘッダ長


/**
 * @brief カードの状態を確認します。
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] Flag 電力供給の状態要求フラグ
 * <table>
 *   <tr><td>=0</td>    <td>カードの有無のみ判定</td></tr>
 *   <tr><td>≠0</td><td>カードの有無、電力供給の有無を判定</td></tr>
 * </table>
 *
 * @return エラーコード
 * @retval CCID_OK 成功。（カードあり、電力供給されています）
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_NO_POWERED_CARD 電力が供給されていません。
 *
 * @since 2008.02
 * @version 1.0
 */
int CardStateConfirm(ReaderMng* gReder, char SlotNum, char Flag)
{
	switch (gReder->cards_mng[(int)SlotNum].State) {
	case 0:	// カードなし
		return CCID_ERR_NO_CARD;
		
	case 2:	// 電力供給中
		break;
		
	case 1:	// カードあり
	default:
		if (Flag)
			return CCID_ERR_NO_POWERED_CARD;	// 電力供給なし
		break;
	}

	return CCID_OK;
}


/**
 * @brief カード状態を取得します。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込み失敗。
 * @retval CCID_ERR_NO_DATA_SIZE 指定サイズの応答を読み込めていません。
 * @retval CCID_ERR_COMMAND_FATAL リーダ命令失敗。
 * @retval RDR_ERR_SLOT_NOT_EXIST スロットは存在しない。
 * @retval RDR_ERR_CMD_NOT_SUPPORTED リーダがサポートしていない命令。
 * @retval CCID_ERR_CMD_SLOT_BUSY スロットビジー（コマンド処理中に次のコマンドが発行された）。
 *
 * @since 2008.02
 * @version 1.0
 */
int GetCardStatus(ReaderMng* gReder, char SlotNum)
{
	char	cmd[CCID_BULK_MSG_HDR_SIZE];
	int		iRet;
	int		len;
	int		counter = 3;
	static int	fail = 0;

retry:
	///リーダからカードへの電力供給切断要求パケット作成
	cmd[0] = 0x65;
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;
	cmd[5] = SlotNum;
	cmd[6] = (gReder->ccid_mng.bSeq)++;
	cmd[7] = cmd[8] = cmd[9] = 0;

	len = CCID_BULK_MSG_HDR_SIZE;

	if (fail) {
		iRet = -123;
		goto writeerr;
	}
	///作成したコマンドをリーダへ送信。
	iRet = WriteUSB(gReder, len, (unsigned char*)cmd);
	///送信に失敗した場合、
	if (iRet != len) {
		///−書き込みエラー（リターン）
		fail++;
	writeerr:
		return CCID_ERR_USB_WRITE;
	}

	///スロット状態応答を取得します。
	iRet = ReadUSB(gReder, len, (unsigned char*)cmd);
	///受信に失敗した場合、
	if (iRet == 0) {
		///−応答受信エラー（リターン）
		if (counter-- > 0)
			goto retry;
		return CCID_ERR_USB_READ;
	}

	///スロット状態応答データがない場合、
	if (iRet < CCID_OFFSET_ERROR+1) {
		///−充分なサイズを読み込めていないエラー（リターン）
		printf("%s: not ehough size(%d < %d) error!\n", 
		       __FUNCTION__, iRet, CCID_OFFSET_ERROR+1);
		return CCID_ERR_NO_DATA_SIZE;
	}

	///応答を解析して、エラーコードを取得します。
	iRet = CheckBulkInMsg((uchar*)cmd, &(gReder->cards_mng[(int)SlotNum]), 1);

	///エラーコードを返す（リターン）
	return iRet;
}


/**
 * @brief Bulk-IN メッセージヘッダの解析
 * Bulk-IN メッセージヘッダの bmICCStatus/bmCommandStatus/bError を解析します。
 *
 * @author University of Tsukuba
 *
 * @param[in] Msg Bulk-INメッセージへのポインタ（ヘッダ部 10 バイトがそろっていること）
 * @param[in,out] card_mng カード管理構造体へのポインタ
 * @param[in] chkPresence カード有無、電力供給有無をステータスに反映するフラグ
 * @li ＝ 0 ステータスを変更しない
 * @li ≠ 0 ステータスを変更する
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_SLOT_NOT_EXIST スロットは存在しない。
 * @retval CCID_ERR_ICC_MUTE カードミュート(ICC_MUTE)。
 * @retval CCID_ERR_PARITY_CHECK パリティエラー。
 * @retval CCID_ERR_CMD_NOT_SUPPORTED サポートしていないコマンド。
 * @retval CCID_ERR_CMD_SLOT_BUSY スロットビジー（コマンド処理中に次のコマンドが発行された）。
 * @retval CCID_ERR_BUSY_WITH_AUTO_SEQUENCE 自動シーケンスが動作中エラー。
 * @retval CCID_ERR_COMMAND_FATAL その他のエラー。
 *
 * @since 2008.02
 * @version 1.0
 */
static int CheckBulkInMsg(uchar *Msg, CardMng *card_mng, int chkPresence)
{
	int iRet = CCID_OK;

	//応答データよりカードの有無を調べる。
	if (chkPresence) {
		switch (Msg[CCID_OFFSET_STATUS] & CCID_ICC_STATUS_RFU) {
		///カードあり＆アクティブ（電源供給あり）の場合、
		case CCID_ICC_STATUS_PRESENT_ACTIVE:
			///−リーダのカード状態をカードあり（電源供給あり）に設定
			card_mng->State = 2;
			break;
		///カードあり（電源供給なし）の場合、
		case CCID_ICC_STATUS_PRESENT_INACTIVE:
			///−リーダのカード状態をカードあり（電源供給なし）に設定
			card_mng->State = 1;
			break;
		///カードがない場合、
		case CCID_ICC_STATUS_NOPRESENT:
			///−スロットの状態をカードなしに設定します。
			card_mng->State = 0;
		default:
			break;
		}
	}
	///カード状態取得でエラーが発生した場合、
	if (Msg[CCID_OFFSET_STATUS] & CCID_CMD_STATUS_FAILED) {
		//−エラーコード解析し、戻り値とします。
		switch ((unsigned char)Msg[CCID_OFFSET_ERROR]) {
		///−指定されたスロットは存在しない場合、
		case RDR_ERR_SLOT_NOT_EXIST:
			///−−戻り値にスロットは存在しないエラーを設定します。
			iRet = CCID_ERR_SLOT_NOT_EXIST;
			break;
		//−カードミュート(ICC_MUTE)の場合、
		case RDR_ERR_ICC_MUTE:
			if ((Msg[CCID_OFFSET_STATUS] & CCID_ICC_STATUS_RFU)!=CCID_ICC_STATUS_NOPRESENT) {
				iRet = CCID_ERR_ICC_MUTE;
			}
			break;
		///−パリティエラー（XFR_PARITY_ERROR）が発生した場合、
		case RDR_ERR_XFR_PARITY_ERROR:
			///−−パリティエラーを返す（リターン）
			iRet = CCID_ERR_PARITY_CHECK;
			break;
		///−サポートしていないコマンドの場合、
		case RDR_ERR_CMD_NOT_SUPPORTED:
			///−−戻り値にサポートしていないコマンドエラーを設定します。
			iRet = CCID_ERR_CMD_NOT_SUPPORTED;
			break;
		///−コマンド処理中に次のコマンドが発行された場合、
		case RDR_ERR_CMD_SLOT_BUSY:
			///−−戻り値にスロットビジーエラーを設定します。
			iRet = CCID_ERR_CMD_SLOT_BUSY;
			break;
		///−−自動シーケンスが動作中エラーを設定します。
		case RDR_ERR_BUSY_WITH_AUTO_SEQUENCE:
			///−−−戻り値に自動シーケンスが動作中エラーを設定します。
			iRet = CCID_ERR_BUSY_WITH_AUTO_SEQUENCE;
			break;

		//−−制御パイプでコマンドがアボートされた場合、
		case RDR_ERR_CMD_ABORTED:
		//−−カードプロトコルをサポートしていない場合、
		case RDR_ERR_ICC_PROTOCOL_NOT_SUPPORTED:
		//−−ハードウェアエラー(HW_ERROR)の場合、
		case RDR_ERR_HW_ERROR:
		default:
			iRet = CCID_ERR_COMMAND_FATAL;
			break;
		}
	}
	
	///エラーコードを返します（リターン）
	return iRet;
}


/**
 * @brief カードに電力供給します。
 * 端末が電圧の自動選択機能をサポートする場合、この機能が利用されます。
 *
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 *
 * @param[in] Voltage カード電圧
 * <ul>
 *   <li>CCID_VOLTAGE_AUTO(0) - 自動選択
 *   <li>CCID_VOLTAGE_5V(1)   - 5V (class A)
 *   <li>CCID_VOLTAGE_3V(2)   - 3V (class B)
 *   <li>CCID_VOLTAGE_1_8V(3) - 1.8V (class C)
 * </ul>
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_NO_POWERED_CARD 電力が供給されていません。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込み失敗。
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 *
 * @since 2008.02
 * @version 1.0
 */
int PowerOn(ReaderMng* gReder, char SlotNum, uchar Voltage)
{
	char	cmd[CCID_BULK_MSG_HDR_SIZE + BUFFER_SIZE_RESP];
	int		iRet;
	int		len;

	///カードの有無を調べる
	if ((iRet = CardStateConfirm(gReder, SlotNum, 0))) {
		///−カードなし（リターン）
		return iRet;
	}

	if (gReder->ccid_mng.dwFeatures & CCID_FEATURES_AUTO_VOLTAGE)
		Voltage = CCID_VOLTAGE_AUTO;	/* 自動選択 */

	///カード電力供給処理ループ
	do {
		///−リーダからカードへの電力供給要求パケット作成
		cmd[0] = 0x62;
		cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;
		cmd[5] = SlotNum;
		cmd[6] = (gReder->ccid_mng.bSeq)++;
		cmd[7] = Voltage;
		cmd[8] = cmd[9] = 0;

		///−作成したコマンドをリーダへ送信。
		iRet = WriteUSB(gReder, CCID_BULK_MSG_HDR_SIZE, (unsigned char*)cmd);
		///−送信に失敗した場合、
		if (iRet != CCID_BULK_MSG_HDR_SIZE) {
			///−−書き込みエラー（リターン）
			return CCID_ERR_USB_WRITE;
		}

		///−データブロック応答を取得します。
		iRet = ReadUSB(gReder, sizeof(cmd), (unsigned char*)cmd);
		///受信に失敗した場合、
		if (iRet < CCID_BULK_MSG_HDR_SIZE) {
			///−−応答受信エラー（リターン）
			return CCID_ERR_USB_READ;
		}

		///−カードの電力供給に失敗した場合、
		if (cmd[CCID_OFFSET_STATUS] & CCID_CMD_STATUS_FAILED) {
			///−−指定電圧が 5V または自動選択の場合、
			if (Voltage < CCID_VOLTAGE_3V) {
				///−−−通信エラー（リターン）
				return CCID_ERR_COMMUNICATION;
			}
			///−−電圧を 1クラス上げて再試行（ループ）
			Voltage--;
		}
	} while(cmd[CCID_OFFSET_STATUS] & CCID_CMD_STATUS_FAILED);

	///応答を解析して、エラーコードを取得します。
	CheckBulkInMsg((uchar*)cmd, &(gReder->cards_mng[(int)SlotNum]), 1);

	len = GetLen(cmd, 1);

	IDMan_StMemset(&(gReder->cards_mng[(int)SlotNum].Atr), 0, sizeof(ATR));
	/// ATRデータ長を ATR 構造体に格納します。
	gReder->cards_mng[(int)SlotNum].Atr.DataLen = (len < MAX_ATR_SIZE) ? len : MAX_ATR_SIZE;
	/// ATRデータを ATR 構造体に格納します。（ ATR解析は IFDHSetProtocolParameters() にて行う）
	IDMan_StMemcpy(gReder->cards_mng[(int)SlotNum].Atr.Data, cmd + CCID_BULK_MSG_HDR_SIZE, len);

	///正常終了（リターン）
	return CCID_OK;
}


/**
 * @brief カードの電力供給を切ります。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_NO_POWERED_CARD 電力が供給されていません。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込み失敗。
 * @retval CCID_ERR_NO_DATA_SIZE 充分なサイズを読み込めていません。
 * @retval CCID_ERR_COMMAND_FATAL コマンド失敗エラー。
 * @retval CCID_ERR_SLOT_NOT_EXIST 指定されたスロットは存在しない。
 * @retval CCID_ERR_BUSY_WITH_AUTO_SEQUENCE 自動シーケンスが動作中。
 * @retval CCID_ERR_CMD_NOT_SUPPORTED サポートしていないコマンド。
 * @retval CCID_ERR_CMD_SLOT_BUSY コマンド処理中に次のコマンドが発行された。
 *
 * @since 2008.02
 * @version 1.0
 */
int PowerOff(ReaderMng* gReder, char SlotNum)
{
	unsigned char	cmd[CCID_BULK_MSG_HDR_SIZE];		// cmdのサイズは送受信するデータ長の大きい値を利用
	int				iRet;
	int				len;

	///カードの有無および電力供給状態を調べる
	if ((iRet = CardStateConfirm(gReder, SlotNum, 1))) {
		///−カードなし、または電力供給されていない（リターン）
		return iRet;
	}

	///リーダからカードへの電力供給切断要求パケット作成
	cmd[0] = 0x63;
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;
	cmd[5] = SlotNum;
	cmd[6] = (gReder->ccid_mng.bSeq)++;
	cmd[7] = cmd[8] = cmd[9] = 0;

	len = CCID_BULK_MSG_HDR_SIZE;

	///作成したコマンドをリーダへ送信。
	iRet = WriteUSB(gReder, len, cmd);
	///送信に失敗した場合、
	if (iRet != len) {
		///−書き込みエラー（リターン）
		return CCID_ERR_USB_WRITE;
	}

	///スロット状態応答を取得します。
	iRet = ReadUSB(gReder, len, cmd);
	///受信に失敗した場合、
	if (iRet == 0) {
		///−応答受信エラー（リターン）
		return CCID_ERR_USB_READ;
	}

	///スロット状態応答データがない場合、
	if (iRet < CCID_OFFSET_ERROR+1) {
		///−充分なサイズを読み込めていないエラー（リターン）
		return CCID_ERR_NO_DATA_SIZE;
	}

	///応答を解析して、エラーコードを取得します。
	iRet = CheckBulkInMsg(cmd, &(gReder->cards_mng[(int)SlotNum]), 0);
	///カードの電力供給切断に失敗した場合、
	if (iRet < 0) {
		switch (iRet) {
		//−指定されたスロットは存在しない場合、
		case CCID_ERR_SLOT_NOT_EXIST:
		//−自動シーケンスが動作中エラーを設定します。
		case CCID_ERR_BUSY_WITH_AUTO_SEQUENCE:
		//−サポートしていないコマンドの場合、
		case CCID_ERR_CMD_NOT_SUPPORTED:
		//−コマンド処理中に次のコマンドが発行された場合、
		case CCID_ERR_CMD_SLOT_BUSY:
			break;
		default:
			iRet = CCID_ERR_COMMUNICATION;
			break;
		}
		///−エラーコードを返します。（リターン）
		return iRet;
	}
	
	///リーダのカード状態をカードあり（電力供給なし）に設定
	if (gReder->cards_mng[(int)SlotNum].State)
		gReder->cards_mng[(int)SlotNum].State = 1; 

	IDMan_StMemset(&(gReder->cards_mng[(int)SlotNum].Atr), 0, sizeof(ATR));

	///正常終了（リターン）
	return CCID_OK;
}


/**
 * @brief カードを排出します。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] bFunction ファンクション
 * @li 01 - カード受け入れ（未対応）
 * @li 02 - カード排出（未対応）
 * @li 03 - カードキャプチャー（未対応）
 * @li 04 - カードロック
 * @li 05 - カードアンロック
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_NO_POWERED_CARD 電力が供給されていません。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込み失敗。
 * @retval CCID_ERR_NO_DATA_SIZE 充分なサイズを読み込めていません。
 * @retval CCID_ERR_COMMAND_FATAL コマンド失敗エラー。
 * @retval CCID_ERR_SLOT_NOT_EXIST 指定されたスロットは存在しない。
 * @retval CCID_ERR_BUSY_WITH_AUTO_SEQUENCE 自動シーケンスが動作中。
 * @retval CCID_ERR_CMD_NOT_SUPPORTED サポートしていないコマンド。
 * @retval CCID_ERR_CMD_SLOT_BUSY コマンド処理中に次のコマンドが発行された。
 *
 * @since 2008.02
 * @version 1.0
 */
int CardMechanical(ReaderMng* gReder, char SlotNum, uchar bFunction)
{
	unsigned char	cmd[CCID_BULK_MSG_HDR_SIZE];
	int				iRet;
	int				len;

	///カードの有無を調べる
	iRet = CardStateConfirm(gReder, SlotNum, 0);
	if (iRet==CCID_ERR_NO_CARD) {
		///−カードなし（リターン）
		return iRet;
	}
	///ファンクションが範囲外の場合、
	if (bFunction < 1 || bFunction > 5) {
		///−パラメータエラー（リターン）
		return CCID_ERR_PARAMETER;
	}

	///リーダからカードへの電力供給切断要求パケット作成
	cmd[0] = 0x71;
	cmd[1] = cmd[2] = cmd[3] = cmd[4] = 0;
	cmd[5] = SlotNum;
	cmd[6] = (gReder->ccid_mng.bSeq)++;
	cmd[7] = bFunction;
	cmd[8] = cmd[9] = 0;

	len = CCID_BULK_MSG_HDR_SIZE;

	///作成したコマンドをリーダへ送信。
	iRet = WriteUSB(gReder, len, cmd);
	///送信に失敗した場合、
	if (iRet != len) {
		///−書き込みエラー（リターン）
		return CCID_ERR_USB_WRITE;
	}

	///スロット状態応答を取得します。
	iRet = ReadUSB(gReder, len, cmd);
	///受信に失敗した場合、
	if (iRet == 0) {
		///−応答受信エラー（リターン）
		return CCID_ERR_USB_READ;
	}

	IDMan_StMemset(&(gReder->cards_mng[(int)SlotNum].Atr), 0, sizeof(ATR));

	///スロット状態応答データがない場合、
	if (iRet < CCID_OFFSET_ERROR+1) {
		///−充分なサイズを読み込めていないエラー（リターン）
		return CCID_ERR_NO_DATA_SIZE;
	}

	///応答を解析して、エラーコードを取得します。
	iRet = CheckBulkInMsg(cmd, &(gReder->cards_mng[(int)SlotNum]), 1);

	///エラーコードを返します（リターン）
	return iRet;
}


/**
 * @brief プロトコル＆パラメータ選択を処理します。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] ParamData パラメータ
 * @param[in] ParamLen パラメータ長( 5 or 7 )
 * <ul>
 *   <li>5 - T=0
 *   <li>7 - T=1
 * </ul>
 * @param[out] RecData 受信したデータ
 * @param[in,out] RecDataLen 受信データ長\n
 * 受信するデータ長を設定して、受信したデータ長を返します。
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_COMMAND サポートされていないコマンドです。
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 * @retval CCID_ERR_NO_DATA_SIZE 全てのデータを読み込んでいない。
 *
 * @since 2008.02
 * @version 1.0
 */
int TransmitPPS(ReaderMng* gReder, char SlotNum, char* ParamData, unsigned long ParamLen, char* RecData, int* RecDataLen)
{
	uchar	cmd[CCID_BULK_MSG_HDR_SIZE + BUFFER_SIZE_CMD];	/* CCID header + RDR_to_PC_Parameters abProtocolDataStructure */
	int		iRet;
	int		protocol;

	///カードの有無を調べる
	if ((iRet = CardStateConfirm(gReder, SlotNum, 0))) {
		///−カードなし（リターン）
		return iRet;
	}
	if (ParamLen != 5 && ParamLen != 7)	return CCID_ERR_COMMUNICATION;
	
	protocol = (ParamLen == 7) ? 1: 0;
	cmd[0] = 0x61;
	cmd[1] = ParamLen & 0xFF;
	cmd[2] = (ParamLen >> 8) & 0xFF;
	cmd[3] = (ParamLen >> 16) & 0xFF;
	cmd[4] = (ParamLen >> 24) & 0xFF;
	cmd[5] = SlotNum;
	cmd[6] = (gReder->ccid_mng.bSeq)++;
	cmd[7] = protocol;
	cmd[8] = cmd[9] = 0;

	IDMan_StMemcpy(cmd+CCID_BULK_MSG_HDR_SIZE, ParamData, ParamLen);
	ParamLen += CCID_BULK_MSG_HDR_SIZE;
	
	///プロトコル＆パラメータ選択命令をカードへ発行します。
	if (WriteUSB(gReder, ParamLen, cmd) == 0) {
		///−書き込みエラー（リターン）
		return CCID_ERR_USB_WRITE;
	}

	///パラメータ応答データがない場合、
	if ((iRet=ReadUSB(gReder, sizeof(cmd), cmd)) < CCID_BULK_MSG_HDR_SIZE) {
		///−充分なサイズを読み込めていないエラー（リターン）
		return CCID_ERR_NO_DATA_SIZE;
	}

	///コマンドエラーが発生している場合、
	if (cmd[CCID_OFFSET_STATUS] & CCID_CMD_STATUS_FAILED) {
		///−パラメータ設定コマンド未対応の場合、
		if (cmd[CCID_OFFSET_ERROR] == RDR_ERR_CMD_NOT_SUPPORTED) {
			///−−コマンドエラー（リターン）
			return CCID_ERR_COMMAND;
		} else
		///−パラメータが変更されていない場合、
		if ((cmd[CCID_OFFSET_ERROR] >= 1) && (cmd[CCID_OFFSET_ERROR] <= 127)) {
			//−−正常
		}
		///−その他のエラーの場合、
		else {
			///−−通信エラー（リターン）
			return CCID_ERR_COMMUNICATION;
		}
	}
	ParamLen -= CCID_BULK_MSG_HDR_SIZE;
	///プロトコル応答データを全て受信していない場合、
	if (iRet < (CCID_BULK_MSG_HDR_SIZE + ParamLen)) {
		///−充分なサイズを読み込めていないエラー（リターン）
		return CCID_ERR_NO_DATA_SIZE;
	}
	///応答データを \e RecData に格納
	IDMan_StMemcpy(RecData, cmd+CCID_BULK_MSG_HDR_SIZE, ParamLen);
	///応答データ長を \e *RecDataLen に格納
	*RecDataLen = ParamLen;

	///正常終了（リターン）
	return CCID_OK; 
}


/**
 * @brief プロトコル T=0 によるデータ読み込み
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[out] pBuffer 受信したデータ
 * @param[in,out] pLength 受信したデータ長
 * 受信するデータ長を設定して、受信したデータ長を返します。
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_NO_POWERED_CARD 電力が供給されていません。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_SUM_CHECK 受信データチェックサムエラー。
 * @retval CCID_ERR_NO_DATA_SIZE 指定サイズの応答を読み込めていません。
 *
 * @since 2008.02
 * @version 1.0
 */
int ReadDataT0(ReaderMng* gReder, char SlotNum, char* pBuffer, unsigned long * pLength)
{
	int				iRet;
	unsigned long	rxLength;
	unsigned long	len;

	///カードの有無および電力供給状態を調べる
	if ((iRet = CardStateConfirm(gReder, SlotNum, 1))) {
		///−カードなし、または電力供給されていない（リターン）
		return iRet;
	}

	rxLength = len = *pLength;
	*pLength = 0;

	///リーダへ読み込むデータ長を知らせます TransmitCommand()
	iRet = TransmitCommand(gReder, SlotNum, 0, pBuffer, (unsigned short)len, 0);
	if (iRet < 0) {
		///−データ書き込み失敗（リターン）
		return iRet; 
	}

	///リーダからデータを \e pBuffer に読み込みます。 ReceiveReply()
	iRet = ReceiveReply(gReder, SlotNum, &len, pBuffer, NULL);
	if (iRet < 0) {
		///−データ読み込み失敗（リターン）
		return iRet; 
	}

	///受信したデータ長を \e *pLength に格納
	*pLength = len;

	///受信したデータ長が指定サイズと一致しない場合、
	if (rxLength != len) {
		///−指定サイズの応答を読み込めていません（リターン）
		return CCID_ERR_NO_DATA_SIZE;
	}

	///正常終了（リターン）
	return CCID_OK; 
}


/**
 * @brief プロトコル T=1 でカードへブロック送信
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] ParamData 命令パラメータ
 * @param[in] ParamLen 命令パラメータ長
 * @param[out] RecData 受信したデータ
 * @param[in,out] RecDataLen 受信バッファサイズ(in)、受信したデータ長(out)
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_INSUFFICIENT_BUFFER バッファ不足。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_SUM_CHECK 受信データチェックサムエラー。
 * @retval CCID_ERR_USB_READ USB読み込みエラー。
 * @retval CCID_ERR_CANCEL 取り消しエラー
 * @retval CCID_ERR_TIMEOUT タイムアウトエラー。
 * @retval CCID_ERR_PARITY_CHECK パリティエラー。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_NO_POWERED_CARD 電力が供給されていません。
 * @retval CCID_ERR_SEND_LEN 送信データ長エラー
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 *
 * @since 2008.02
 * @version 1.0
 */
int BlockSendCardT1(ReaderMng* gReder, char SlotNum, 
					   char* ParamData, int ParamLen, char* RecData, unsigned long* RecDataLen)
{
	int 			iRet = CCID_OK;
	unsigned int	readTimeout;
	unsigned long	recMax;
	unsigned long	rxLen;
	CcidMng *		ccid_mng = &(gReder->ccid_mng);
	ProtocolMngT1 *	t1 = &(gReder->cards_mng[(int)SlotNum].ProtocolT1);

	///カードの有無および電力供給状態を調べる
	if ((iRet = CardStateConfirm(gReder, SlotNum, 1))) {
		///−カードなし、または電力供給されていない（リターン）
		return iRet;
	}

	if (RecData==NULL || RecDataLen==NULL || *RecDataLen < 4) {
		return CCID_ERR_INSUFFICIENT_BUFFER;
	}
	rxLen = *RecDataLen;
	*RecDataLen = 0;

	///受信タイムアウト値退避
	readTimeout = ccid_mng->readTimeout;

	/// ブロック待ち時間延長あり（ WTX \> 0 ）の場合、
	if (t1->Wtx > 1) {
		///−−受信タイムアウト値変更
		ccid_mng->readTimeout *=  t1->Wtx;
	}

	///交換レベルが文字の場合、（取得する文字数を指定します）
	if ((ccid_mng->dwFeatures & CCID_FEATURES_EXCHANGE_MASK)==0) {
		///−カードへ命令送信 TransmitCommand()
		recMax = 3;	//受信サイズ
		iRet = TransmitCommand(gReder, SlotNum, ParamLen, (const char*)ParamData, recMax, t1->Wtx);
		///−エラーの場合、戻り値にエラーコードを返す（リターン）
		if (iRet != CCID_OK)	return iRet;

		///−カードからの応答取得：プロローグフィールド(NAD+PCB+LEN) ReceiveReply()
		iRet = ReceiveReply(gReder, SlotNum, &recMax, RecData, NULL);
		///−エラーの場合、戻り値にエラーコードを返す（リターン）
		if (iRet != CCID_OK)	return iRet;

		///−取得する「情報フィールド長＋EDC(Error Detection Code)長」を計算
		recMax = RecData[2] + 1;
		///−カードへ継続データ取得命令を送信 TransmitCommand()
		iRet = TransmitCommand(gReder, SlotNum, 0, RecData, recMax, t1->Wtx);
		///−エラーの場合、戻り値にエラーコードを返す（リターン）
		if (iRet != CCID_OK)	return iRet;

		///−カードからの応答取得：情報フィールド(INF+EDC) ReceiveReply()
		iRet = ReceiveReply(gReder, SlotNum, &recMax, (char*)&RecData[3], NULL);
		///−エラーの場合、戻り値にエラーコードを返す（リターン）
		if (iRet != CCID_OK)	return iRet;
		
		///−取得したデータ長計算
		*RecDataLen = recMax + 3;
		// データRecData[0]〜RecData[*RecDataLen-1]
	}
	///交換レベルがデータ単位(TPDU/APDU)の場合、
	else
	{
		///−カードへ命令送信 TransmitCommand()
		iRet = TransmitCommand(gReder, SlotNum, ParamLen, (const char*)ParamData, 0, t1->Wtx);
		t1->Wtx = 0;
		///−エラーの場合、戻り値にエラーコードを返す（リターン）
		if (iRet != CCID_OK)	return iRet;

		///−カードからのブロック応答取得 ReceiveReply()
		*RecDataLen = rxLen;
		iRet = ReceiveReply(gReder, SlotNum, RecDataLen, RecData, NULL);
		///−エラーの場合、戻り値にエラーコードを返す（リターン）
		if (iRet != CCID_OK)	return iRet;

		//−取得したデータ長を \e *RecDataLen に格納
		// データRecData[0]〜RecData[*RecDataLen-1]
	}

	///取得サイズ再計算 NAD+PCB+LEN+INF+EDC
	recMax = 3 + RecData[2] + (t1->Edc);
	///再計算したサイズが取得データサイズより小さい場合、
	if (recMax < *RecDataLen) {
		///−再計算したサイズを取得データ長とします。
		*RecDataLen = recMax;
	}

	///受信タイムアウト復帰
	ccid_mng->readTimeout = readTimeout;

	///正常終了（リターン）
	return CCID_OK; 
}


/**
 * @brief カードに命令を送信します。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] SendDataLen 送信データ長
 * @param[in] pSendData 送信データ
 * @param[in] RecDataLen 受信データ長
 * @param[in] bBWI ブロック待ち時間整数 \n
 * この転送でブロック待ち時間を延長するために用いる値。
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_SEND_LEN 送信データ長エラー
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 *
 * @since 2008.02
 * @version 1.0
 */
int TransmitCommand(ReaderMng* gReder, uchar SlotNum, unsigned long SendDataLen,
	const char *pSendData, unsigned short RecDataLen, unsigned char bBWI)
{
	int				iRet;
	int				len;
	unsigned char	cmd[CCID_BULK_MSG_HDR_SIZE+BUFFER_SIZE_CMD];	// CCID header + APDU buffer

	///送信データ長が大きすぎる場合、
	if (SendDataLen > BUFFER_SIZE_CMD) {
		///−データ長エラー（リターン）
		return CCID_ERR_SEND_LEN;
	}

	/// CCIDヘッダ部作成
	cmd[0] = 0x6F;
	cmd[1] = SendDataLen & 0xFF;
	cmd[2] = (SendDataLen >> 8) & 0xFF;
	cmd[3] = (SendDataLen >> 16) & 0xFF;
	cmd[4] = (SendDataLen >> 24) & 0xFF;
	cmd[5] = SlotNum;
	cmd[6] = (gReder->ccid_mng.bSeq)++;
	cmd[7] = bBWI;
	cmd[8] = RecDataLen & 0xFF;
	cmd[9] = (RecDataLen >> 8) & 0xFF;

	IDMan_StMemcpy(cmd+CCID_BULK_MSG_HDR_SIZE, pSendData, SendDataLen);

	/// CCIDコマンドメッセージ送信 WriteUSB()
	len = CCID_BULK_MSG_HDR_SIZE+SendDataLen;
	iRet = WriteUSB(gReder, len, cmd);
	///送信エラーの場合、書き込みエラー（リターン）
	if (iRet != len)
		return CCID_ERR_USB_WRITE;

	///正常終了（リターン）
	return CCID_OK; 
}


/**
 * @brief カードから応答を受信します。
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in,out] pRecBufferLen 受信バッファサイズ(in)、受信データ長(out)
 * @param[out] pRecBuffer 受信データバッファ
 * @param[out] pLevelParameter データ継続パラメータバッファ (wLevelParameter)
 * バッファが設定されている場合、データ継続パラメータを返す。
 * （この引数はRDR_to_PC_DataBlock応答のみ有効。）
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_USB_READ USB読み込みエラー。
 * @retval CCID_ERR_CANCEL 取り消しエラー
 * @retval CCID_ERR_TIMEOUT タイムアウトエラー。
 * @retval CCID_ERR_PARITY_CHECK パリティエラー。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 * @retval CCID_ERR_INSUFFICIENT_BUFFER バッファ不足。
 *
 * @since 2008.02
 * @version 1.0
 */
int ReceiveReply(ReaderMng* gReder, char SlotNum, unsigned long *pRecBufferLen,
	char *pRecBuffer, char *pLevelParameter)
{
	uchar			cmd[CCID_BULK_MSG_HDR_SIZE+BUFFER_SIZE_CMD];	// CCID header + APDU buffer
	unsigned long	len;
	int				num;
	int				iRet = CCID_OK;

	/// CCID応答メッセージ受信ループ（時間延長応答の場合はループ）
	cmd[CCID_OFFSET_STATUS] = 0;
	do {
		///− CCID応答メッセージ受信 ReadUSB()
		len = sizeof(cmd);
		num = ReadUSB(gReder, len, cmd);
		///−応答メッセージ長が足りない場合、
		if (num < CCID_OFFSET_ERROR+1) {
			///−読み込みエラー（リターン）
			return CCID_ERR_USB_READ;
		}

		///−コマンドエラーが発生している場合、
		if (cmd[CCID_OFFSET_STATUS] & CCID_CMD_STATUS_FAILED) {
			switch (cmd[CCID_OFFSET_ERROR]) {
			///−−キャンセル（PIN_CANCELLED）された場合、
			case RDR_ERR_PIN_CANCELLED:
				///−−−要求した受信データ長 \< 2 の場合、
				if (*pRecBufferLen < 2) {
					///−−−−取り消しエラー（リターン）
					return CCID_ERR_CANCEL;
				}
				///−−−応答 0x64:0x01 を \e pRecBuffer に設定
				pRecBuffer[0]= 0x64;
				pRecBuffer[1]= 0x01;
				*pRecBufferLen = 2;
				///−−−正常終了
				return CCID_OK;

			///−−タイムアウト（PIN_TIMEOUT）が発生した場合、
			case RDR_ERR_PIN_TIMEOUT:
				///−−−要求した受信データ長 \< 2 の場合、
				if (*pRecBufferLen < 2) {
					///−−−−タイムアウトエラー（リターン）
					return CCID_ERR_TIMEOUT;
				}
				///−−−応答 0x64:0x01 を \e pRecBuffer に設定
				pRecBuffer[0]= 0x64;
				pRecBuffer[1]= 0x00;
				*pRecBufferLen = 2;
				///−−−正常終了
				return CCID_OK;

			///−−パリティエラー（XFR_PARITY_ERROR）が発生した場合、
			case RDR_ERR_XFR_PARITY_ERROR:
				///−−−パリティエラーを返す（リターン）
				return CCID_ERR_PARITY_CHECK;

			///−−カードがない場合、
			case RDR_ERR_ICC_MUTE:
				if ((cmd[CCID_OFFSET_STATUS] & CCID_ICC_STATUS_RFU)==CCID_ICC_STATUS_NOPRESENT) {
					///−−−カードなしエラーを返す（リターン）
					gReder->cards_mng[(int)SlotNum].State = 0;
					return CCID_ERR_NO_CARD;
				}
				break;
			///−−その他のエラーの場合、
			default:
				///−−−通信エラーを返す（リターン）
				return CCID_ERR_COMMUNICATION;
			}
		}
		///−時間延長応答の場合は、ループ継続
	} while (cmd[CCID_OFFSET_STATUS] & CCID_CMD_STATUS_TIME_EXTENSION);

	///応答より受信データ長取得
	len = GetLen(cmd, 1);
	/// 受信データ長 ≦ 受信バッファサイズ の場合
	if (len <= *pRecBufferLen) {
		///受信データ長を \e *pRecBufferLen に設定
		*pRecBufferLen = len;
	}
	/// 受信データ長 \> 受信バッファサイズ の場合
	else {
		///指定バッファ不足エラーを戻り値に設定
		len = *pRecBufferLen;
		iRet = CCID_ERR_INSUFFICIENT_BUFFER;
	}
	///受信応答をバッファ \e pRecBuffer に格納
	IDMan_StMemcpy(pRecBuffer, cmd+CCID_BULK_MSG_HDR_SIZE, len);

	///データ継続バッファが設定されている場合、
	if (cmd[0]==0x80 && pLevelParameter)
		*pLevelParameter = cmd[CCID_OFFSET_CHAIN_PARAMETERT];

	return iRet;
}


/**
 * @brief カードへデータ転送を行います。（T=0 CHAR） \n
 * 送受信応答処理をドライバ〜カード間で行い、全てのデータを送信します。 \n
 * このときリーダ(CCID)はデータ転送のみ行っています。
 *
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] SendDataLen 送信データサイズ
 * @param[in] pSendData 送信データのポインタ
 * @param[in,out] pRecBufferLen 受信バッファサイズ(in)、受信データ長(out)
 * @param[out] pRecBuffer 受信データバッファ
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 * @retval CCID_ERR_SEND_LEN 送信データ長エラー
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込みエラー。
 * @retval CCID_ERR_CANCEL 取り消しエラー
 * @retval CCID_ERR_TIMEOUT タイムアウトエラー。
 * @retval CCID_ERR_PARITY_CHECK パリティエラー。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_INSUFFICIENT_BUFFER バッファ不足。
 * @retval CCID_ERR_NO_DATA_SIZE 指定サイズの応答を読み込めていません。
 *
 * @since 2008.02
 * @version 1.0
 */
int T0XfrBlockCHAR(ReaderMng* gReder, char SlotNum,
	unsigned long SendDataLen, const uchar * pSendData, unsigned long *pRecBufferLen, uchar * pRecBuffer)
{
	unsigned char	cmd[5];
	int				iRet = CCID_OK;
	unsigned long	respLen = 0			/*応答データ長*/;
	unsigned long	inLen = 0			/*受信バッファ内のデータ数*/;
	unsigned long	actual;
	char			data_buffer[512];
	int				rcv_mode = 0;		//受信モードフラグ
	char			INS;
	char			*pBuffer=data_buffer;

	// ケース分類 Ref: 7816-4
	switch (SendDataLen) {
	/// Case 1 命令がなく、応答データがない場合、( \e SendDataLen == 4 )
	case 4:
		///−応答データ長 = 2、ステータス(SW1 \& SW2)のみ
		respLen = 2;
		++rcv_mode;
		break;

	/// Case 2 命令がなく、応答データがある場合、( \e SendDataLen == 5 )
	case 5:
		///−データフィールド長≠ 0 の場合、
		if (pSendData[4] != 0) {
			///−−応答データ長 = データフィールド長 + 2
			respLen = pSendData[4] + 2;
		} else {
			///−−応答データ長 = 256（データフィールド長 == 0） + 2
			respLen = 256 + 2;
		}
		++rcv_mode;
		break;

	/// Case 3 命令データがあり、応答データがない場合、
	default:
		///− 送信データ長 \> 5 で、 5 + コマンドデータ長に一致する場合、
		if (SendDataLen > 5 && SendDataLen == (unsigned long)(pSendData[4] + 5)) {
			///−応答データ長 = 2、ステータス(SW1 \& SW2)のみ
			respLen = 2;
		}
		///−その他
		else {
			///−−通信エラー（リターン）
			return CCID_ERR_COMMUNICATION;
		}
		break;
	}

	IDMan_StMemset(cmd, 0, sizeof(cmd));
	if (SendDataLen == 4) {
		IDMan_StMemcpy(cmd, pSendData, 4);
		pSendData += 4;
		SendDataLen = 0;
	}
	else {
		IDMan_StMemcpy(cmd, pSendData, 5);
		pSendData += 5;
		SendDataLen -= 5;
	}

	/// 命令 INS (0x6X or 0x9X)が間違っている場合、
	INS = cmd[1];
	if ((INS & 0xF0) == 0x60 || (INS & 0xF0) == 0x90) {
		///−通信エラー（リターン）
		return CCID_ERR_COMMUNICATION;
	}
	///カードへ命令を送信 TransmitCommand()
	iRet = TransmitCommand(gReder, SlotNum, 5, (const char*)cmd, 1, 0);
	if (iRet != CCID_OK)
		return iRet;

	///応答受信ループ
	while (1) {
		///−受信バッファにデータがない場合、
		if (inLen == 0) {
			//手続きバイトの受信
			///−− 1 バイトのみ応答受信 ReceiveReply()
			inLen = 1;
			iRet = ReceiveReply(gReder, SlotNum, &inLen, (char*)data_buffer, NULL);
			///−−命令エラーの場合、ループを抜ける（エラーコードは戻り値に設定済み）
			if (iRet < 0)	break;
			pBuffer = data_buffer;
		}
		///−応答を受信していない場合、
		if (inLen == 0) {
			///−−応答タイムアウトエラーを戻り値に設定
			iRet = CCID_ERR_READER_TIMEOUT;
			///−−ループを抜ける
			break;
		}

		///− NULL(0x60)を受信した場合、
		if (*pBuffer == 0x60) {
			///−−次の手続きバイト取得のため、１バイト取得命令 TransmitCommand()
			inLen = 0;
			iRet = TransmitCommand(gReder, SlotNum, 0, (const char*)cmd, 1, 0);
			///−−命令エラーの場合、ループを抜ける（エラーコードは戻り値に設定済み）
			if (iRet < 0)	break;
			//−−次の手続きバイトを待つため、応答受信ループ
		}
		///− ACK (INS or INS+1) を受信した場合、
		else if (*pBuffer == INS || *pBuffer == (INS ^ 0x01)) {
			// 残り全てのデータを転送できます
			///−−受信ポインタをインクリメント
			pBuffer++;
			inLen--;
			///−−受信モードの場合、
			if (rcv_mode) {
				actual = respLen - *pRecBufferLen;
				///−−−受信バッファにデータが要求サイズ以上残っている場合、
				if (inLen >= actual) {
					IDMan_StMemcpy( &pRecBuffer[*pRecBufferLen], pBuffer, actual );
					///−−−−−応答データ長の和を \e *pRecBufferLen に格納
					*pRecBufferLen += actual;
					inLen -= actual;
				}
				///−−−受信バッファのデータが足りない場合、
				else {
					actual -= inLen;
					///−−−−残り全ての応答データ長を取得 ReadDataT0()
					iRet = ReadDataT0(gReder, SlotNum, (pBuffer + inLen), &actual);
					///−−−−応答データありの場合、
					if (iRet == CCID_OK || iRet == CCID_ERR_NO_DATA_SIZE) {
						///−−−−−応答データを受信バッファ \e pRecBuffer に格納
						IDMan_StMemcpy( &pRecBuffer[*pRecBufferLen], pBuffer, (actual + inLen) );
						///−−−−−応答データ長の和を \e *pRecBufferLen に格納
						*pRecBufferLen += actual + inLen;
						inLen = 0;
					}
				}
			}
			///−−送信モードの場合、
			else {
				///−−−残り全ての送信データ長をカードに送信 TransmitCommand()
				actual = 1;	// ACK(1byte)を希望
				iRet = TransmitCommand(gReder, SlotNum, SendDataLen, (const char*)pSendData, actual,0);
				///−−−全てのデータ送信終了の場合、
				if (iRet == CCID_OK) {
					///−−−−送信データ長を 0 に設定
					pSendData += SendDataLen;
					SendDataLen = 0;
					inLen = 0;
				}
			}
			///−−全ての応答を読み込んだ場合、
			if (*pRecBufferLen == respLen) {
				///−−−正常終了を戻り値に設定
				iRet = CCID_OK;
				///−−−ループを抜ける
				break;
			}
			//−−応答が残っている場合、応答受信ループ
		} else
		///− ACK (~INS or ~(INS+1)) を受信した場合、
		if (*pBuffer == (INS ^ 0xFF) || *pBuffer == (INS ^ 0xFE)) {
			// 次の１バイトのデータを転送できます。
			///−−受信ポインタをインクリメント
			pBuffer++;
			inLen--;	// inLen = 0 ???
			///−−受信モードの場合、
			if (rcv_mode) {
				actual = 1;	// 次の1バイト
				///−−−受信バッファにデータが要求サイズ以上残っている場合、
				if (inLen >= actual) {
					IDMan_StMemcpy( &pRecBuffer[*pRecBufferLen], pBuffer, actual );
					///−−−−−応答データ長の和を \e *pRecBufferLen に格納
					*pRecBufferLen += actual;
					inLen -= actual;
				}
				///−−−受信バッファのデータが足りない場合、
				else {
					///−−−−次の応答データ 1バイトを取得 ReadDataT0()
					iRet = ReadDataT0(gReder, SlotNum, (pBuffer + inLen), &actual);
					///−−−−応答データありの場合、
					if (iRet == CCID_OK || iRet == CCID_ERR_NO_DATA_SIZE) {
						///−−−−−応答データを受信バッファ \e pRecBuffer に格納
						IDMan_StMemcpy( &pRecBuffer[*pRecBufferLen], pBuffer, (actual + inLen) );
						///−−−−−応答データ長の和を \e *pRecBufferLen に格納
						*pRecBufferLen += actual + inLen;
						inLen = 0;
					}
				}
			}
			///−−送信モードの場合、
			else {
				///−−−次の送信データ１バイトをカードに送信 TransmitCommand()
				iRet = TransmitCommand(gReder, SlotNum, 1,(const char*)pSendData, 1,0);
				///−−−送信失敗の場合、
				if (iRet == CCID_OK) {
					///−−−−送信バッファ＋１、送信データ長 -1 します。
					pSendData += 1;
					SendDataLen -= 1;
				}
			}
			///−−エラーの場合、ループを抜ける（エラーコードは戻り値に設定済み）
			if (iRet < 0)	break;
		} else
		///−カード状態を受信した場合、
		if ((*pBuffer & 0xF0) == 0x60 || (*pBuffer & 0xF0) == 0x90) {
			///−−カード状態 SW1 を受信バッファ \e pRecBuffer に格納
			pRecBuffer[*pRecBufferLen] = *pBuffer;
			///−−受信データ長 \e *pRecBufferLen を +1
			++(*pRecBufferLen);
			///−−受信バッファにデータがない場合、
			if (inLen == 0) {
				inLen = 1;
				///−−−−次の応答データ SW2 1バイトを取得 ReadDataT0()
				iRet = ReadDataT0(gReder, SlotNum, data_buffer, &inLen);
				///−−−−エラーの場合、ループを抜ける（エラーコードは戻り値に設定済み）
				if (iRet < 0)	break;
				
				pBuffer = data_buffer;
			}
			///−−カード状態 SW2 を受信バッファ \e pRecBuffer に格納
			pRecBuffer[*pRecBufferLen] = *pBuffer;
			///−−受信データ長 \e *pRecBufferLen を +1
			++(*pRecBufferLen);
			++pBuffer;
			--inLen;
			///−−正常終了を戻り値に設定
			iRet = CCID_OK;
			///−−ループを抜ける。
			break;
		}
		///−その他のデータを受信した場合、
		else {
			///−−通信エラーを戻り値に設定：サポートされていない手続きバイト
			iRet = CCID_ERR_COMMUNICATION;
			///−−ループを抜ける。
			break;
		}
	}
	///戻り値を返す（リターン）
	return iRet;
}


/**
 * @brief カードへデータ転送を行います。（short APDU T=0/1共通およびTPDU T=0）
 *
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] SendDataLen 送信データサイズ
 * @param[in] pSendData 送信データのポインタ
 * @param[in,out] pRecBufferLen 受信バッファサイズ(in)、受信データ長(out)
 * @param[out] pRecBuffer 受信データバッファ
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 * @retval CCID_ERR_SEND_LEN 送信データ長エラー
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込みエラー。
 * @retval CCID_ERR_CANCEL 取り消しエラー
 * @retval CCID_ERR_TIMEOUT タイムアウトエラー。
 * @retval CCID_ERR_PARITY_CHECK パリティエラー。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_INSUFFICIENT_BUFFER バッファ不足。
 *
 * @since 2008.02
 * @version 1.0
 */
int T0XfrBlockTPDU(ReaderMng* gReder, char SlotNum,
	unsigned long SendDataLen, const uchar *pSendData, unsigned long *pRecBufferLen, uchar *pRecBuffer)
{
	int iRet = CCID_OK;

	///命令データ長が端末の取り扱えるパケット最大値より大きい場合、
	if (SendDataLen > gReder->ccid_mng.dwMaxCCIDMessageLength-10) {
		///−通信エラー（リターン）
		return CCID_ERR_COMMUNICATION;
	}

	///命令データ長がドライバの取り扱える値[TPDU(4)+Lc(1)+data(256)+Le(1)]より大きい場合、
	if (SendDataLen > BUFFER_SIZE_CMD) {
		///−通信エラー（リターン）
		return CCID_ERR_COMMUNICATION;
	}
	
	iRet = TransmitCommand(gReder, SlotNum, SendDataLen, (const char*)pSendData, 0, 0);
	if (iRet != CCID_OK)	return iRet;

	return ReceiveReply(gReder, SlotNum, pRecBufferLen, (char*)pRecBuffer, NULL);
}


/**
 * @brief カードへデータ転送を行います。（Extended-APDU T=0/1共通）
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] SendDataLen 送信データサイズ
 * @param[in] pSendData 送信データのポインタ
 * @param[in,out] pRecBufferLen 受信バッファサイズ(in)、受信データ長(out)
 * @param[out] pRecBuffer 受信データバッファ
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_SEND_LEN 送信データ長エラー
 * @retval CCID_ERR_USB_WRITE	 USB書き込み失敗。
 * @retval CCID_ERR_USB_READ USB読み込みエラー。
 * @retval CCID_ERR_CANCEL 取り消しエラー
 * @retval CCID_ERR_TIMEOUT タイムアウトエラー。
 * @retval CCID_ERR_PARITY_CHECK パリティエラー。
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_COMMUNICATION 通信エラー。
 * @retval CCID_ERR_INSUFFICIENT_BUFFER バッファ不足。
 *
 * @since 2008.02
 * @version 1.0
 */
int T1XfrBlockExtendedAPDU(ReaderMng* gReder, char SlotNum,
	int SendDataLen, char *pSendData, unsigned long *pRecBufferLen, char *pRecBuffer)
{
	int				iRet;
	CcidMng *		ccid_mng = &(gReder->ccid_mng);
	char			wLevelParameter;
	unsigned long	send_data_len;
	unsigned long	sumLength;
	unsigned long	rec_data_len;
	int				buf_overflow_flag = 0;

	///送信済データ数初期化
	sumLength = 0;			
	///各種カウンタ初期化
	/* wLevelParameter
	 *	00:開始＆終了
	 *	01:開始＆最初のブロック
	 *	02:最終ブロック＆終了
	 *	03:ブロック継続
	 *	10:空abData＆次に応答
	 */
	wLevelParameter = 0x00;
	send_data_len = SendDataLen;	//送信ブロックのデータ数

	///送信データが大きい場合、送信ブロック長を計算 MAX 262bytes
	if (send_data_len > BUFFER_SIZE_CMD) {
		//−命令を含むブロックサイズを計算 TransmitCommand()内のバッファサイズに依存
		send_data_len = BUFFER_SIZE_CMD;	// 262bytes
		//− APDUブロックは継続します。
		wLevelParameter = 0x01;
	}
	if (send_data_len > ccid_mng->dwMaxCCIDMessageLength-10) {
		//−命令を含むブロックサイズを計算
		send_data_len = ccid_mng->dwMaxCCIDMessageLength-10;	// 261〜65544 bytes
		//− APDUブロックは継続します。
		wLevelParameter = 0x01;
	}

	///命令送信ループ
	while (1) {
		///− APDUブロック送信 TransmitCommand()
		iRet = TransmitCommand(gReder, SlotNum, send_data_len, pSendData,
								wLevelParameter, 0);
		///−送信に失敗した場合、エラーを返す（リターン）
		if (iRet != CCID_OK)	return iRet;

		sumLength += send_data_len;
		pSendData += send_data_len;

		///−送信終了の場合、 wLevelParameter = 0x00 or 0x02
		if ((0x02 == wLevelParameter) || (0x00 == wLevelParameter)) {
			///−−（ループ終了）
			break;
		}

		///− NULLブロックの読み込み ReceiveReply()
		iRet = ReceiveReply(gReder, SlotNum, &rec_data_len, NULL, NULL);
		///−受信に失敗した場合、エラーを返す（リターン）
		if (iRet != CCID_OK)	return iRet;

		///−残りのデータ長 \> 送信ブロック長 の場合、
		if (SendDataLen - sumLength > send_data_len) {
			///−−ブロック継続 wLevelParameter = 0x03
			wLevelParameter = 0x03;
		}
		else {
			///−−次は最終ブロック wLevelParameter = 0x02
			wLevelParameter = 0x02;
			///−−最後のブロック長計算
			send_data_len = SendDataLen - sumLength;
		}
	}

	///受信済みデータ数初期化
	sumLength = 0;
	///応答受信ループ
	do {
		rec_data_len = *pRecBufferLen - sumLength;
		///−応答ブロック受信 ReceiveReply()
		iRet = ReceiveReply(gReder, SlotNum, &rec_data_len, pRecBuffer,
			&wLevelParameter);
		///−受信バッファ不足の場合、
		if (iRet==CCID_ERR_INSUFFICIENT_BUFFER) {
			///−−バッファオーバーフローフラグON
			buf_overflow_flag = 1;

			///−−応答受信継続のため、戻り値を正常終了に設定
			iRet = CCID_OK;
		}
		///−エラーの場合、エラーを返す。（リターン）
		if (iRet != CCID_OK)	return iRet;

		///−受信バッファ・ポインタ更新
		pRecBuffer += rec_data_len;
		///−受信済みデータ数更新
		sumLength += rec_data_len;

		switch (wLevelParameter) {
		///−ブロック継続の場合、 wLevelParameter = 0x01 or 0x03 or 0x10
		// 開始ブロック
		case 0x01:
		// 継続ブロック
		case 0x03:
		// 継続ブロック（データは空）
		case 0x10:
			///−− NULLブロック送信 TransmitCommand()
			iRet = TransmitCommand(gReder, SlotNum, 0, NULL, 0, 0);
			///−−エラーの場合、エラーを返す。（リターン）
			if (iRet != CCID_OK)	return iRet;
			///−−（応答受信ループ）
			break;

		///−応答受信終了の場合、 wLevelParameter = 0x00 or 0x02
		// 応答 APDU 開始と終了
		case 0x00:
		// 応答 APDU 終了と最終データブロック
		case 0x02:
		default:
			///−−応答受信終了（応答受信ループ終了）
			break;
		}
	} while (wLevelParameter==0x01 || wLevelParameter==0x03 || wLevelParameter==0x10);

	///受信データ長を \e *pRecBufferLen に設定
	*pRecBufferLen = sumLength;

	///応答受信途中でバッファ不足になった場合、
	if (buf_overflow_flag) {
		///−バッファ不足エラー（リターン）
		return CCID_ERR_INSUFFICIENT_BUFFER;
	}

	return CCID_OK;
}
