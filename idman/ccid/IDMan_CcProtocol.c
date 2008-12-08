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
 * ID管理のCCIDドライバ：プロトコル T=1 制御関数
 * \file IDMan_CcProtocol.c
 */

#include "IDMan_CcCcid.h"


static int BlockSendSupervisory(ReaderMng* gReder, uchar SlotNum, uchar Pcb, uchar Data);

enum { SENDING, RECEIVING, RESYNCH, DEAD };

// T1ブロックのOFFSET位置
#define T1_PCB 1
#define T1_LEN 2

// PCB エラー種別 BlockSendReceiveReady()に渡すときに使用
#define PCB_ERR_FREE		0	// エラーなし
#define PCB_ERR_EDC_PARITY	1	// エラー：EDC and/or パリティ
#define PCB_ERR_OTHER		2	// その他のエラー


/**
 * @brief 論理冗長検査(Logical Redundancy Check)用データ作成
 * @author University of Tsukuba
 *
 * @param[in] Data 対象データ
 * @param[in] Len 対象データ長
 * @param[out] Edc 論理冗長検査用データ(LRC)
 *
 * @since 2008.02
 * @version 1.0
 */
static void MakeLRC(char* Data, int Len, uchar* Edc)
{
	int cnt;

	*Edc = 0;

	for (cnt = 0 ; cnt < Len ; ++cnt)
		(*Edc) ^= Data[cnt];
}


/**
 * @brief 巡回冗長検査(Cyclic Redundancy Check)用データ作成
 * @author University of Tsukuba
 *
 * @param[in] data 対象データ
 * @param[in] Len 対象データ長
 * @param[out] Edc1 巡回冗長検査用データ(high byte)
 * @param[out] Edc2 巡回冗長検査用データ(low byte)
 *
 * @since 2008.02
 * @version 1.0
 */
static void MakeCRC(char* Data, int Len, uchar* Edc1, uchar* Edc2)
{
	int cnt;
	unsigned short	Crc = 0xFFFF;
	unsigned short	work1;
	unsigned short	work2;

	for (cnt = 0 ; cnt < Len ; ++cnt) {
		work1 = (Crc >> 8) ^ Data[cnt];
		Crc <<= 8;
		work2 = work1 ^ (work1 >> 4);
		Crc ^= work2;
		work2 <<= 5;
		Crc ^= work2;
		work2 <<= 7;
		Crc ^= work2;
	}

	*Edc1 = (Crc >> 8) & 0x00FF;
	*Edc2 = Crc & 0x00FF;
}


/**
 * @brief リーダ／カードのプロトコル T=1 を初期化します。
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 *
 * @since 2008.02
 * @version 1.0
 */
void InitLeaderCardProtocol(ReaderMng* gReder, char SlotNum)
{
	int cnt;
	ATR *Atr = &(gReder->cards_mng[(int)SlotNum].Atr);

	///カード情報フィールド長(IFSC)を設定
	gReder->cards_mng[(int)SlotNum].ProtocolT1.Ifsc = (GetIFSCData(Atr) > BLOCK_T1_INF_MAX ? BLOCK_T1_INF_MAX : GetIFSCData(Atr));

	///端末情報フィールド長(IFSD)を設定
	gReder->cards_mng[(int)SlotNum].ProtocolT1.Ifsd = 32;

	///誤り検出符号(EDC)を設定
	gReder->cards_mng[(int)SlotNum].ProtocolT1.Edc =  1;
	//T1のエラー検出コード(Error Detection Code)型を取得
	for (cnt = 1 ; cnt < Atr->pn ; ++cnt) {
		/* 最初のプロトコル T=1 を見つける: bit3-bit0 on TD2 */
		if (Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].present && 
			(Atr->ib[cnt][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1) 
		{
			/* 発見したらエラー検出コード型を返す
			  ( bit0 on TC3 - Interface Char for T=1 Error Detection Code ) */
			if (Atr->ib[cnt+1][ATR_INTERFACE_BYTE_TC].present)
			{
				gReder->cards_mng[(int)SlotNum].ProtocolT1.Edc = (Atr->ib[cnt+1][ATR_INTERFACE_BYTE_TC].value & 0x01) + 1 ;
				break;
			}
			else
			{
				break;
			}
		}
	}

	///送信シーケンス番号を設定
	gReder->cards_mng[(int)SlotNum].ProtocolT1.SequenceNum = 1;
	gReder->cards_mng[(int)SlotNum].ProtocolT1.SequenceNumCard = 0;

	gReder->cards_mng[(int)SlotNum].ProtocolT1.RetryNum  = 3;
	gReder->cards_mng[(int)SlotNum].ProtocolT1.Wtx = 0;
	gReder->cards_mng[(int)SlotNum].ProtocolT1.State = SENDING;
	gReder->cards_mng[(int)SlotNum].ProtocolT1.More = 0;
}


/**
 * @brief 応答データの解析
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] ResponseData 応答データ
 * @param[in] ResponseLen 応答データ長
 * @param[out] OutBuf 受信したデータ
 * @param[out] OutBufLen 受信データ長
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 *
 * @since 2008.02
 * @version 1.0
 */
static int AnalysisReceptionData(ReaderMng* gReder, char SlotNum,
						char* ResponseData, int ResponseLen, char* OutBuf, int* OutBufLen)
{
	uchar ReadBuff;
	uchar Edc1;
	uchar Edc11;
	uchar Edc2;
	uchar Edc22;

	*OutBufLen = 0;

	///応答データ長 \< 3 の場合、
	if (ResponseLen < 3) {
		///−プロトコル T=1 エラー（リターン）
		return CCID_ERR_PROTOCOL;
	}

	///ヘッダ部 3バイトを \e OutBuf に格納
	ReadBuff = ResponseData[0];
	OutBuf[*OutBufLen] = ReadBuff;
	(*OutBufLen)++;
	
	ReadBuff = ResponseData[1];
	OutBuf[*OutBufLen] = ReadBuff;
	(*OutBufLen)++;
	
	ReadBuff = ResponseData[2];
	OutBuf[*OutBufLen] = ReadBuff;
	(*OutBufLen)++;

	///情報フィールド長の最大値(254)を超えているか、応答データ長が短い場合、
	if (ReadBuff > BLOCK_T1_INF_MAX || ResponseLen < (ReadBuff + 3)) {
		///−プロトコル T=1 エラー（リターン）
		return CCID_ERR_PROTOCOL;
	}

	///情報フィールドがある場合、
	if (ReadBuff > 0) {
		///−情報フィールドを \e OutBuf に格納]
		IDMan_StMemcpy(OutBuf+3, ResponseData+3, ReadBuff);
		///−データ長を \e OutBufLen に格納
		(*OutBufLen) += ReadBuff;
	}
	
	// 誤り検出符号読み込み
	///誤り検出符号種別が LRCの場合、
	if (gReder->cards_mng[(int)SlotNum].ProtocolT1.Edc == 1) {
		///−LRC計算
		MakeLRC(OutBuf, *OutBufLen, &Edc11);

		// NAD(1)+PCB(1)+LEN(1)+INF+EDC(1)
		///−応答データ長が短い場合、
		if (ResponseLen < (ReadBuff + 4)) {
			///−−プロトコル T=1 エラー（リターン）
			return CCID_ERR_PROTOCOL;
		}

		///−応答LRC部を \e OutBuf に格納
		Edc1 = ResponseData[ReadBuff + 3];
		OutBuf[*OutBufLen] = Edc1;
		(*OutBufLen)++;

		///−算出したLRCと応答LRCが一致しない場合、
		if (Edc1 != Edc11) {
			///−−チェックサムLRC エラー（リターン）
			return CCID_ERR_SUM_CHECK;
		}
	}
	///誤り検出符号種別が CRCの場合、
	else {
		///−CRC計算
		MakeCRC(OutBuf, *OutBufLen, &Edc11, &Edc22);

		// NAD(1)+PCB(1)+LEN(1)+INF+EDC(2)
		///−応答データ長が短い場合、
		if (ResponseLen < (ReadBuff + 5)) {
			///−−プロトコル T=1 エラー（リターン）
			return CCID_ERR_PROTOCOL;
		}

		///−応答CRC部を \e OutBuf に格納
		Edc1 = ResponseData[ReadBuff + 3];
		OutBuf[*OutBufLen] = Edc1;
		(*OutBufLen)++;

		Edc2 = ResponseData[ReadBuff + 4];
		OutBuf[*OutBufLen] = Edc2;
		(*OutBufLen)++;

		///−算出したCRCと応答CRCが一致しない場合、
		if (Edc1 != Edc11 || Edc2 != Edc22) {
			///−−チェックサムCRC エラー（リターン）
			return CCID_ERR_SUM_CHECK;
		}
	}

	///正常終了（リターン）
	return CCID_OK;
}


/**
 * @brief I/R/Sブロック送信\n
 * \e ProtocolT1.BlockSend に格納された送信データを BlockSendCardT1() で送信します。
 *
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体\n
 * \e ProtocolT1.BlockSend に送信データを格納しておきます。
 * @param[in] SlotNum スロット番号
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
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 *
 * @since 2008.02
 * @version 1.0
 */
static int BlockSendIRS(ReaderMng* gReder, uchar SlotNum)
{
	char			ResponseData[BUFFER_SIZE_BULK];
	int				iRet;
	unsigned long	ResponseLen;
	BlockMngT1*		pRecv = &(gReder->cards_mng[(int)SlotNum].ProtocolT1.BlockReceive);
	BlockMngT1*		pSend = &(gReder->cards_mng[(int)SlotNum].ProtocolT1.BlockSend);
	uchar			Edc1;
	uchar			Edc2;
	uchar			Cnt = pSend->BlockData[2];

	///誤り検出符号(EDC)種別が LRCの場合、
	if (gReder->cards_mng[(int)SlotNum].ProtocolT1.Edc == 1) { 
		///−LRC計算
		MakeLRC((char*)(pSend->BlockData), pSend->BlockLen, &Edc1);
		///−算出したLRCを送信ブロックに追加
		pSend->BlockData[3 + Cnt] = Edc1; 
		///−送信ブロック長 +1
		pSend->BlockLen++;
	}
	///誤り検出符号種別が CRCの場合、
	else {
		///−CRC計算
		MakeCRC((char*)(pSend->BlockData), pSend->BlockLen, &Edc1, &Edc2);
		///−算出したCRCを送信ブロックに追加
		pSend->BlockData[3 + Cnt] = Edc1; 
		pSend->BlockData[4 + Cnt] = Edc2; 
		///−送信ブロック長 +2
		pSend->BlockLen += 2;
	}
	IDMan_StMemcpy(gReder->cards_mng[(int)SlotNum].ProtocolT1.PreviousBlock, pSend->BlockData,
						(3 + gReder->cards_mng[(int)SlotNum].ProtocolT1.Edc));
	
	///ブロック送信 BlockSendCardT1()
	ResponseLen = sizeof(ResponseData);
	iRet = BlockSendCardT1(gReder, SlotNum,
					(char*)pSend->BlockData, pSend->BlockLen, ResponseData, &ResponseLen);
	if (iRet < 0) {
		///−ブロック送信エラー（リターン）
		return iRet;
	}

	///受信した応答の解析 AnalysisReceptionData()
	iRet = AnalysisReceptionData(gReder, SlotNum,
					ResponseData, ResponseLen, (char*)pRecv->BlockData, &(pRecv->BlockLen));

	///解析結果を返す（リターン）
	return iRet;
}


/**
 * @brief 前回送信したRブロック送信\n
 * \e ProtocolT1.BlockSend に格納された送信データを BlockSendCardT1() で送信します。
 *
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体\n
 * \e ProtocolT1.BlockSend に送信データを格納しておきます。
 * @param[in] SlotNum スロット番号
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
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 *
 * @since 2008.02
 * @version 1.0
 */
static int ReBlockSendReceiveReady(ReaderMng* gReder, uchar SlotNum)
{
	ProtocolMngT1* t1 = &(gReder->cards_mng[(int)SlotNum].ProtocolT1);
	int iRet;
	unsigned long ResponseLen;
	char ResponseData[BUFFER_SIZE_BULK];

	///再送するため、前回のブロックを \e block に格納
	ResponseLen = 3 + t1->Edc;	// データ長 4 or 5
	IDMan_StMemcpy(t1->BlockSend.BlockData, t1->PreviousBlock, ResponseLen);
	t1->BlockSend.BlockLen = ResponseLen;

	///ブロック送信 BlockSendCardT1()
	ResponseLen = sizeof(ResponseData);
	iRet = BlockSendCardT1(gReder, SlotNum,
					(char*)t1->BlockSend.BlockData, t1->BlockSend.BlockLen, ResponseData, &ResponseLen);
	if (iRet < 0) {
		///−ブロック送信エラー（リターン）
		return iRet;
	}

	///受信した応答の解析 AnalysisReceptionData()
	iRet = AnalysisReceptionData(gReder, SlotNum,
					ResponseData, ResponseLen, (char*)t1->BlockReceive.BlockData, &(t1->BlockReceive.BlockLen));

	///解析結果を返す（リターン）
	return iRet;
}


/**
 * @brief 情報ブロック(I)作成＆送信
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] data 情報データ
 * @param[in] Len 情報データ長（≦254）
 * @param[in] MoreData 継続ブロックありフラグ
 * @param[in] SequenceNum 送信シーケンス番号のカウントアップフラグ
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_SUM_CHECK 受信データチェックサムエラー。
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 *
 * @since 2008.02
 * @version 1.0
 */
static int BlockSendInformation(ReaderMng* gReder, uchar SlotNum, uchar* Data, int Len, char MoreData, int SequenceNum)
{
	uchar			Pcb = 0;
	uchar			uLen;
	int				iRet;
	int				cnt;
	ProtocolMngT1	*t1 = &(gReder->cards_mng[(int)SlotNum].ProtocolT1);

	///送信シーケンス番号をカウントアップフラグがある場合、
	if (SequenceNum) {
		///−送信シーケンス番号カウントアップ
		t1->SequenceNum = (t1->SequenceNum + 1) % 2;
	}

	///送信シーケンス番号をプロトコル制御バイトPCBに設定
	if (t1->SequenceNum)	Pcb |= 0x40;

	///継続ブロックありをプロトコル制御バイトPCBに設定
	t1->More = MoreData;
	if (MoreData)	Pcb |= 0x20;
	else		Pcb &= 0xdf;

	/**
	 * \e gReder 内の送信ブロックの prologue フィールドを設定
	 * @li [0] = NAD(Node Address byte)
	 * @li [1] = PCB(Protocol Control Byte)
	 * @li [2] = 情報データ長
	 */
	uLen = Len;
	t1->BlockSend.BlockData[0] = 0x00;
	t1->BlockSend.BlockData[1] = Pcb;
	t1->BlockSend.BlockData[2] = uLen;

	/// \e gReder 内の送信ブロックの情報フィールドに情報データをコピー
	for (cnt = 0 ; cnt < uLen ; ++cnt)
		t1->BlockSend.BlockData[3 + cnt] = Data[cnt];

	/// \e gReder 内の送信ブロック長を設定
	t1->BlockSend.BlockLen = uLen + 3;

	///ブロック送信 BlockSendIRS()
	iRet = BlockSendIRS(gReder, SlotNum);
	
	///ブロック送信のエラーコードを返す（リターン）
	return iRet;
}


/**
 * @brief 監視ブロック(S)作成＆送信
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] Pcb プロトコル制御バイト
 * <ul>
 *   <li>BLOCK_T1_S_REQUEST_RESYNCH - RESYNCH要求
 *   <li>BLOCK_T1_S_RESPONSE_RESYNCH - RESYNCH応答
 *   <li>BLOCK_T1_S_REQUEST_IFS - 情報フィールド長変更要求
 *   <li>BLOCK_T1_S_RESPONSE_IFS - 情報フィールド長変更応答
 *   <li>BLOCK_T1_S_REQUEST_WTX - ブロック待ち時間延長要求
 *   <li>BLOCK_T1_S_RESPONSE_WTX - ブロック待ち時間延長応答
 * </ul>
 * @param[in] Data 情報データ\n
 * このデータはIFSおよびWTX以外のブロックでは無効です。
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_SUM_CHECK 受信データチェックサムエラー。
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 *
 * @since 2008.02
 * @version 1.0
 */
static int BlockSendSupervisory(ReaderMng* gReder, uchar SlotNum, uchar Pcb, uchar Data)
{
	///情報フィールド長 Len = 0
	uchar Len = 0;
	int iRet;
	ProtocolMngT1 *t1 = &(gReder->cards_mng[(int)SlotNum].ProtocolT1);

	/**
	 *プロトコル制御バイトを検査して情報フィールド長が \b 1 の要求／応答の場合、
	 * @li 0xC1 - 情報フィールド長変更要求
	 * @li 0xE1 - 情報フィールド長変更応答
	 * @li 0xC3 - ブロック待ち時間延長(WTX)要求
	 */
	if (Pcb == BLOCK_T1_S_REQUEST_IFS || Pcb == BLOCK_T1_S_RESPONSE_IFS || Pcb == BLOCK_T1_S_RESPONSE_WTX) 
	{
		///−情報フィールド長変数 Len = 1 に設定
		Len = 1;
	}

	/**
	 * \e gReder 内の送信ブロックの prologue フィールドを設定
	 * @li [0] = NAD(Node Address byte)
	 * @li [1] = PCB(Protocol Control Byte)
	 * @li [2] = 情報フィールド長 Len
	 */
	t1->BlockSend.BlockData[0] = 0x00;
	t1->BlockSend.BlockData[1] = Pcb;
	t1->BlockSend.BlockData[2] = Len;

	
	/// \e gReder 内の送信ブロックの情報フィールドに情報データをコピー
	if (Len)
	{
		t1->BlockSend.BlockData[3] = Data;
	}

	/// \e gReder 内の送信ブロック長を設定
	t1->BlockSend.BlockLen = Len + 3;	// NAD(1)+PCB(1)+LEN(1)+INF(1)

	///ブロック送信 BlockSendIRS()
	iRet = BlockSendIRS(gReder, SlotNum);
	
	///ブロック送信のエラーコードを返す（リターン）
	return iRet;
}


/**
 * @brief 受信準備完了ブロック(R)作成＆送信
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] SequenceNumRec 受信シーケンス番号(1/0)
 * @param[in] ErrType エラー種別
 * @li 0 - エラーなし
 * @li 1 - (EDC and/or パリティ)エラー
 * @li 2 - その他のエラー
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_SUM_CHECK 受信データチェックサムエラー。
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 *
 * @since 2008.02
 * @version 1.0
 */
static int BlockSendReceiveReady(ReaderMng* gReder, uchar SlotNum, uchar SequenceNumRec, uchar ErrType)
{
	int iRet;
	ProtocolMngT1 *t1 = &(gReder->cards_mng[(int)SlotNum].ProtocolT1);

	/**
	 * \e gReder 内の送信ブロックの prologue フィールドを設定
	 * @li [0] = NAD(Node Address byte)
	 * @li [1] = PCB(Protocol Control Byte)
	 * @li [2] = 情報フィールド長 = 0
	 */
	t1->BlockSend.BlockData[0] = 0x00;
	t1->BlockSend.BlockData[1] = BLOCK_T1_R;
	t1->BlockSend.BlockData[2] = 0x00;

	///受信シーケンス番号 \e SequenceNumRec を \e gReder 内の送信ブロックのプロトコル制御バイトPCBに設定
	if (SequenceNumRec) {
		t1->BlockSend.BlockData[1] |= 0x10;
	}
	///エラーによる要求の場合、
	if (ErrType) {
		///−エラータイプ \e ErrType を \e gReder 内の送信ブロックのプロトコル制御バイトPCBに設定
		t1->BlockSend.BlockData[1] |= ErrType;
	}

	/// \e gReder 内の送信ブロック長を設定
	t1->BlockSend.BlockLen = 3;

	///ブロック送信 BlockSendIRS()
	iRet = BlockSendIRS(gReder, SlotNum);

	///ブロック送信のエラーコードを返す（リターン）
	return iRet;
}


/**
 * @brief カードから受信した監視ブロック(I)処理\n
 * 送信対象のT1データをブロック単位で送信します。
 *
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 * @retval CCID_ERR_USB_WRITE USB書き込み失敗。
 * @retval CCID_ERR_SUM_CHECK 受信データチェックサムエラー。
 * @retval CCID_ERR_RECEIVE_ABORT ABORT要求受信。
 * @retval CCID_ERR_RECEIVE_RESYNCH RESYNCH応答受信。
 * @retval CCID_ERR_RECEIVE_VPP Vppエラー通知受信。
 *
 * @since 2008.02
 * @version 1.0
 */
static int BlockReceptionInformation(ReaderMng* gReder, uchar SlotNum)
{
	int iRet;

	///カードから受信した監視ブロックのポインタを \e gReder から取得
	ProtocolMngT1 *t1 = &(gReder->cards_mng[(int)SlotNum].ProtocolT1);

	///ブロック待ち時間延長要求の場合、
	if (t1->BlockReceive.BlockData[T1_PCB] == BLOCK_T1_S_REQUEST_WTX) {
		///−監視ブロックに情報フィールドがない場合、
		if (t1->BlockReceive.BlockLen < 5) {
			///−−プロトコル T=1 エラー（リターン）
			return CCID_ERR_PROTOCOL;
		}

		///−取得した延長時間を \e gReder に設定
		t1->Wtx = *(t1->BlockReceive.BlockData + 3);

		///−ブロック待ち時間延長応答を送信 BlockSendSupervisory()
		iRet = BlockSendSupervisory(gReder, SlotNum, BLOCK_T1_S_RESPONSE_WTX, t1->Wtx);

		///−ブロック待ち時間延長応答エラーの場合、
		if (iRet < 0) {
			///−−応答送信エラーを返す（リターン）
			return iRet;
		}
	} else
	///情報フィールド長変更要求の場合、
	if (t1->BlockReceive.BlockData[1] == BLOCK_T1_S_REQUEST_IFS) {
		///−監視ブロックに情報フィールドがない場合、
		if (t1->BlockReceive.BlockLen < 5) {
			///−−プロトコル T=1 エラー（リターン）
			return CCID_ERR_PROTOCOL;
		}

		///−取得したカードの情報フィールド長を \e gReder に設定
		t1->Ifsc = *(t1->BlockReceive.BlockData + 3);

		///−情報フィールド長変更応答送信 BlockSendSupervisory()
		iRet = BlockSendSupervisory(gReder, SlotNum, BLOCK_T1_S_RESPONSE_IFS, t1->Ifsc);
		if (iRet < 0) {
			///−−応答送信エラー（リターン）
			return iRet;
		}
	} else
	/// ABORT要求の場合、
	if (t1->BlockReceive.BlockData[1] == BLOCK_T1_S_REQUEST_ABORT) {
		///−ABORT応答送信 BlockSendSupervisory()
		iRet = BlockSendSupervisory(gReder, SlotNum, BLOCK_T1_S_RESPONSE_ABORT, 0);
		if (iRet < 0) {
			///−−応答送信エラー（リターン）
			return iRet;
		}
		///−エラーコード ABORT受信を返す（リターン）
		return CCID_ERR_RECEIVE_ABORT;
	} else
	/// RESYNCH応答の場合、
	if (t1->BlockReceive.BlockData[1] == BLOCK_T1_S_RESPONSE_RESYNCH) {
		///−エラーコード RESYNCH受信を返す（リターン）
		return CCID_ERR_RECEIVE_RESYNCH;
	}
	/// Vppエラー通知
	else {
		///−エラーコード Vppエラー受信を返す（リターン）
		return CCID_ERR_RECEIVE_VPP;
	}

	///正常終了（リターン）
	return CCID_OK;
}


/**
 * @brief T1データをブロックに分割して送信 (T=1 CHAR/TPDU 共通) \n
 * T1ブロック送受信応答処理をドライバ〜リーダ(CCID)間で行い、全てのデータを送信します。 \n
 * リーダ〜カード間の送受信応答はリーダが制御しています。 \n
 * ブロック転送プロトコル T=1 で制御します。 \n
 * ブロック構成 - NAD/PCB/LEN/INF/EDC \n
 * ブロック - 情報(I)/受信(R)/監視(S)
 *
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] pTxBuffer T1データ
 * @param[in] txLength T1データ長
 * @param[out] pRxBuffer 応答データ
 * @param[out] pRxLength 応答データ長
 *
 * @retval CCID_OK 成功。
 * @retval CCID_ERR_PROTOCOL プロトコル T=1 エラー。（データ長、EDC等）
 * @retval CCID_ERR_NO_CARD カードがありません。
 * @retval CCID_ERR_NO_POWERED_CARD 電力が供給されていません。
 * @retval CCID_ERR_RECEIVE_VPP Vppエラー通知受信。
 *
 * @since 2008.02
 * @version 1.0
 */
int T1XfrBlockTPDU(ReaderMng* gReder, uchar SlotNum,
	int txLength, uchar* pTxBuffer, unsigned long *pRxLength, uchar* pRxBuffer)
{
	uchar			rsp_type;
	int				iRet;
	int				bytes;	// T1データサイズ(LEN) MAX 254bytes
	char			SequenceNum = 0;
	unsigned long	counter;
	int 			reqCntReTransmit;	//再送要求カウンタ
	int 			reqCntResynch;		// RESYNCH 要求カウンタ
	char 			MoreData;
	char			finished;

	///カードから受信したブロックのポインタを \e gReder から取得
	ProtocolMngT1 *t1 = &(gReder->cards_mng[(int)SlotNum].ProtocolT1);
	BlockMngT1* BlockReceive = &(t1->BlockReceive);

	///カードの有無および電力供給状態を調べる
	if ((iRet = CardStateConfirm(gReder, SlotNum, 1))) {
		///−カードなし、または電力供給されていない（リターン）
		return iRet;
	}

	/*================================
		カードへ命令を送信
	================================ */

	///最初のブロックで送信するバイト数を取得
	bytes = (txLength > t1->Ifsc ? t1->Ifsc : txLength);

	///継続ブロックを調べる（送信するT1データが情報フィールド長の最大値を超えるか）
	MoreData = (txLength > t1->Ifsc);
	
	///初回データ分を情報ブロック(I)で送信 BlockSendInformation()
	iRet = BlockSendInformation(gReder, SlotNum, pTxBuffer, bytes, MoreData, 1);


	///カウンタ＆状態初期化
	t1->State = SENDING;
	reqCntReTransmit = t1->RetryNum;	//再送要求カウンタ初期値設定
	reqCntResynch = 3;				// RESYNCH要求カウンタ初期値設定
	counter = 0;					//送信済みデータのバイト数クリア

	///ブロック送受信ループ
	finished = 0;					//送信終了フラグ
	while (!finished) {
		///−再同期(RESYNCH)要求を 3回行ってもエラーの場合、
		if (iRet < 0 && reqCntResynch == 0 && t1->State == RESYNCH) {
			///−− T1状態をDEADに設定
			t1->State = DEAD;
			///−−エラーを返す（リターン）
			return iRet;
		} else
		///−再送要求を規定回数行ってもエラーの場合、
		if (iRet < 0 && reqCntReTransmit==0 && reqCntResynch) {
			///−− RESYNCH要求カウント
			reqCntResynch--;
			///−−再同期(RESYNCH)要求送信 BlockSendSupervisory()
			iRet = BlockSendSupervisory(gReder, SlotNum, BLOCK_T1_S_REQUEST_RESYNCH, 0);
			///−− T1状態をRESYNCHに設定（ブロック送受信ループ）
			t1->State = RESYNCH;
			continue;
		} else
		///− T=1プロトコルのエラーが発生した場合、
		if ((iRet==CCID_ERR_PARITY_CHECK || iRet==CCID_ERR_SUM_CHECK || iRet==CCID_ERR_PROTOCOL)
				&& reqCntReTransmit) {
			///−−再送要求カウント
			reqCntReTransmit--;

			///−−前回の送信ブロックが 受信準備完了(R)の場合、
			if ((t1->PreviousBlock[T1_PCB] & BLOCK_T1_MASK) == BLOCK_T1_R) {
				///−−−前回のブロック（再送要求）を再送 ReBlockSendReceiveReady()
				iRet = ReBlockSendReceiveReady(gReder, SlotNum);
			}
			///−−前回の送信ブロックが 受信準備完了(R)ではない場合、
			else {
				///−−−再送要求：受信準備完了ブロック(R)送信 BlockSendReceiveReady()
				iRet = BlockSendReceiveReady(gReder, SlotNum, t1->SequenceNum,
							(iRet==CCID_ERR_PROTOCOL) ? PCB_ERR_OTHER: PCB_ERR_EDC_PARITY );
			}
			///−−（ブロック送受信ループ）
			continue;
		} else
		///−その他のエラーの場合、
		if (iRet < 0) {
			///−− T1状態をDEADに設定
			t1->State = DEAD;
			return iRet;
		}

		/**＃＃＃ 有効なブロックの処理 ＃＃＃*/

		// 受信ブロックタイプ取得
		rsp_type = (((BlockReceive->BlockData[T1_PCB] & 0x80) == BLOCK_T1_I)? BLOCK_T1_I: (BlockReceive->BlockData[1] & 0xC0));	///<ブロック種別 I/R/S 取得
		switch (rsp_type) {
		///−情報ブロック(I)を受信した場合、
		case BLOCK_T1_I:
			//カードからの情報ブロック受信
			///−−命令データの分割ブロック全ての送信を完了した場合、
			if (t1->State==SENDING && !MoreData) {
				///−−−送信終了、受信モードへ状態遷移
				t1->State = RECEIVING;
				///−−−最初の応答ブロックの受信シーケンス番号取得
				SequenceNum = ((BlockReceive->BlockData[T1_PCB] >> 6) & 0x01);///<送信シーケンス番号 (0/1)
				///−−−カウンタ初期化
				counter = 0;			//受信済データ長 = 0
				*pRxLength = 0;			//受信データ長 = 0
			}

			//−−ブロック受信モード

			///−−リトライカウント初期値設定
			reqCntReTransmit = t1->RetryNum;

			///−−−シーケンス番号が連続していない場合、
			if (SequenceNum != ((BlockReceive->BlockData[T1_PCB] >> 6) & 0x01)) {
				///−−−−再送要求：受信準備完了ブロック(R)送信（応答ブロック受信ループ） BlockSendReceiveReady()
				iRet = BlockSendReceiveReady(gReder, SlotNum, SequenceNum, PCB_ERR_OTHER);
				continue;
			}

			///−−受信したブロックのシーケンス番号を \e gReder に格納
			t1->SequenceNumCard = ((BlockReceive->BlockData[T1_PCB] >> 6) & 0x01);

			///−−次に受信するブロックのシーケンス番号計算
			SequenceNum = (((BlockReceive->BlockData[T1_PCB] >> 6) & 0x01) + 1) % 2;

			///−−受信した情報フィールド長取得
			bytes = (BlockReceive->BlockData[T1_LEN]);	///<ブロックの情報フィールド長
			///−−情報フィールドがある場合、
			if (bytes) {
				///−−−受信バッファ \e pRxBuffer に格納
				IDMan_StMemcpy(pRxBuffer + counter, &(BlockReceive->BlockData[3]), bytes);
			}
			///−−受信済みデータ長計算
			counter += bytes;
			///−−受信済みデータ長を \e pRxLength に格納
			*pRxLength = counter;

			///−−−継続する受信ブロックが存在する場合、
			MoreData = ((BlockReceive->BlockData[T1_PCB] >> 5) & 0x01);	///<継続ブロック有無 0:なし、1:あり
			if (MoreData) {
				///−−−−受信準備完了ブロック(R)送信（受信したシーケンス番号+1） BlockSendReceiveReady()
				iRet = BlockSendReceiveReady(gReder, SlotNum, SequenceNum, 0);
			} else {
				///−−−（ループ終了）
				finished = 1;
			}
			break;

		///−受信準備完了ブロック(R)を受信した場合、
		case BLOCK_T1_R:
			///−− PCBに未使用のbit5(0)が 1、または情報フィールド長(ブロックの情報フィールド長)あり（規約違反）の再送要求の場合、
			if (BlockReceive->BlockData[T1_PCB] & 0x20 || (BlockReceive->BlockData[T1_LEN])) {
				///−−−再送要求を規定回数行ってもエラーの場合、
				if (reqCntReTransmit == 0){
					///−−−−再同期へ移行するため、エラーを設定（ブロック送受信ループ）
					iRet = CCID_ERR_PROTOCOL;
					continue;
				}
				///−−−リトライカウント
				reqCntReTransmit--;

				///−−−前回の送信ブロックが 受信準備完了(R)の場合、
				if ((t1->PreviousBlock[T1_PCB] & BLOCK_T1_MASK) == BLOCK_T1_R) {
					///−−−−前回のブロック（再送要求）を再送 ReBlockSendReceiveReady()
					iRet = ReBlockSendReceiveReady(gReder, SlotNum);
				}
				///−−−前回の送信ブロックが 受信準備完了(R)ではない場合、
				else {
					///−−−−再送要求：受信準備完了ブロック(R)送信 BlockSendReceiveReady()
					iRet = BlockSendReceiveReady(gReder, SlotNum, t1->SequenceNum, PCB_ERR_OTHER);
				}
				///−−−（ブロック送受信ループ）
				continue;
			}
			///−−ブロック送信モード
			if (t1->State==SENDING) {
				///−−−カード側からエラー(受信準備完了ブロックにエラー)再送要求の場合、
				if ((BlockReceive->BlockData[T1_PCB] & 0x0F)) {
					///−−−−再送要求を規定回数行ってもエラーの場合、
					if (reqCntReTransmit == 0) {
						///−−−−−再同期へ移行するため、エラーを設定（ブロック送受信ループ）
						iRet = CCID_ERR_PROTOCOL;
						continue;
					}
					///−−−−リトライカウント
					reqCntReTransmit--;
					///−−−−前回の送信ブロックが 受信準備完了(R)の場合、
					if ((t1->PreviousBlock[T1_PCB] & BLOCK_T1_MASK) == BLOCK_T1_R) {
						///−−−−−前回のブロック（再送要求）を再送 ReBlockSendReceiveReady()
						iRet = ReBlockSendReceiveReady(gReder, SlotNum);
					} else {
						///−−−−−情報ブロック(I)再送 BlockSendInformation()
						iRet = BlockSendInformation(gReder, SlotNum, pTxBuffer + counter, bytes, MoreData, 0);
					}
				} else
				///−−−ブロック送信モードで次のシーケンス番号(受信シーケンス番号 (0/1))を要求された場合、
				if (((BlockReceive->BlockData[T1_PCB] >> 4) & 0x01) != (t1->SequenceNum)) { 
					///−−−−送信データが残っている場合、
					if (MoreData) {
						///−−−−−リトライカウンタ初期値設定
						reqCntReTransmit = t1->RetryNum;
						///−−−−−送信済みデータのバイト数計算
						counter += bytes;
						///−−−−−次のブロックで送信するデータのバイト数計算
						bytes = ((txLength - counter)> t1->Ifsc ? t1->Ifsc : (txLength - counter));

						///−−−−−継続ブロックを調べる（送信するT1データが最大ブロック長を超えるか）
						MoreData = ((txLength - counter) > t1->Ifsc);

						///−−−−−データを情報ブロック(I)で送信 BlockSendInformation()
						iRet = BlockSendInformation(gReder, SlotNum, pTxBuffer + counter, bytes, MoreData, 1);
					}
					///−−−− \e pTxBuffer の全データの送信終了している場合、
					else {
						///−−−−−再送要求を規定回数行ってもエラーの場合、
						if (reqCntReTransmit == 0) {
							///−−−−−−再同期のため、エラーを設定（ブロック送受信ループ）
							iRet = CCID_ERR_PROTOCOL;
							continue;
						}
						///−−−−−リトライカウント
						reqCntReTransmit--;
						///−−−−−前回の送信ブロックが 受信準備完了(R)の場合、
						if ((t1->PreviousBlock[T1_PCB] & BLOCK_T1_MASK) == BLOCK_T1_R) {
							///−−−−−−前回のブロック（再送要求）を再送 ReBlockSendReceiveReady()
							iRet = ReBlockSendReceiveReady(gReder, SlotNum);
						} else {
							///−−−−−−肯定応答(ACK)を取得するために受信準備完了ブロック(R)送信 BlockSendReceiveReady()
							iRet = BlockSendReceiveReady(gReder, SlotNum, t1->SequenceNum, PCB_ERR_OTHER);
						}
					}
				}
			} else
			///−−ブロック受信モード
			if (t1->State==RECEIVING) {
				///−−−再送要求を規定回数行ってもエラーの場合、
				if (reqCntReTransmit == 0) {
					///−−−再同期へ移行するため、エラーを設定（ブロック送受信ループ）
					iRet = CCID_ERR_PROTOCOL;
					continue;
				}
				///−−−リトライカウント
				reqCntReTransmit--;
				///−−−前回の送信ブロックが 受信準備完了(R)の場合、
				if ((t1->PreviousBlock[T1_PCB] & BLOCK_T1_MASK) == BLOCK_T1_R) {
					///−−−−前回のブロック（再送要求）を再送 ReBlockSendReceiveReady()
					iRet = ReBlockSendReceiveReady(gReder, SlotNum);
				} else {
					///−−−受信準備完了ブロック(R)送信 BlockSendReceiveReady()
					iRet = BlockSendReceiveReady(gReder, SlotNum, (t1->SequenceNumCard + 1) % 2, 0);
				}
			}
			break;

		///−監視ブロック(S)を受信した場合、
		case BLOCK_T1_S:
			///−−監視ブロック(S)を解析して、対応する応答を返す BlockReceptionInformation()
			iRet = BlockReceptionInformation(gReder, SlotNum);
			///−−再送要求カウンタ初期値設定
			reqCntReTransmit = t1->RetryNum;
			
			///−−解析結果：Vppエラー通知を受信した場合、
			if (iRet == CCID_ERR_RECEIVE_VPP) {
				///−−−対応するエラーコードを返す（リターン）
				return iRet;
			} else
			///−−解析結果：ABORT要求を受信した場合、
			if (iRet == CCID_ERR_RECEIVE_ABORT) {
				///−−−再同期へ移行するため、エラーを設定（ブロック送信ループ）
				reqCntReTransmit = 0;
				iRet = CCID_ERR_PROTOCOL;
				continue;
			} else
			// ドライバから要求するのは RESYNCHのみなので応答も RESYNCHのみ。
			///−−解析結果：再同期(RESYNCH)応答を受信した場合（再同期完了）、
			if (iRet==CCID_ERR_RECEIVE_RESYNCH) {
				///−−−最初のブロックで送信するバイト数を取得
				bytes = (txLength > t1->Ifsc ? t1->Ifsc : txLength);
				///−−−継続ブロックを調べる（送信するT1データが情報フィールド長の最大値を超えるか）
				MoreData = (txLength > t1->Ifsc);
				///−−−初回データ分を情報ブロック(I)で送信 BlockSendInformation()
				iRet = BlockSendInformation(gReder, SlotNum, pTxBuffer, bytes, MoreData, 1);
				///−−−カウンタ＆状態初期化
				t1->State = SENDING;
				reqCntReTransmit = t1->RetryNum;	//再送要求カウンタ初期値設定
				reqCntResynch = 3;				// RESYNCH要求カウンタ初期値設定
				counter = 0;					//送信済みデータのバイト数クリア
			}
			break;
		}
	}

	///最後に実行された関数のエラーコードを返す（リターン）
	return iRet;
}


/**
 * @brief IFSD設定命令送信
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ管理構造体
 * @param[in] SlotNum スロット番号
 * @param[in] Ifsd IFSD最大値
 *
 * @return 受信データ長（ \> 0 ）
 * @retval -1 エラー
 *
 * @since 2008.02
 * @version 1.0
 */
int T1NegociateIFSD(ReaderMng* gReder, uchar SlotNum, int Ifsd)
{
	unsigned int	RetryNum;
	int				iRet;
	ProtocolMngT1	*t1 = &(gReder->cards_mng[SlotNum].ProtocolT1);

	RetryNum = t1->RetryNum + 1;

	/// IFSD設定ループ
	do {
		//リーダ端末の情報フィールド長を最大値に設定し、失敗したときはデフォルト値を使用します。
		///− IFSD設定命令(S-block)送信
		iRet = BlockSendSupervisory(gReder, SlotNum, BLOCK_T1_S_REQUEST_IFS, Ifsd);

		///−リトライ残 = 0 の場合、
		RetryNum--;

		if (RetryNum == 0) {
			///−−リトライエラー（ループ終了）
			iRet = CCID_ERR_RETRY;
			break;
		}

		///−パリティエラー、T=1プロトコルエラー、受信データエラーの場合、リトライします。（ループ）
	} while ((iRet==CCID_ERR_PARITY_CHECK)									// パリティエラー
			|| (iRet==CCID_ERR_PROTOCOL)									// EDC 等エラー
			|| (t1->BlockReceive.BlockData[0] != 0)							// NAD
			|| (t1->BlockReceive.BlockData[1] != BLOCK_T1_S_RESPONSE_IFS)	// PCB
			|| (t1->BlockReceive.BlockData[2] != 1)							// データレングス
			|| (t1->BlockReceive.BlockData[3] != Ifsd));					// IFSD

	///エラーの場合、
	if (iRet != CCID_OK) {
		///−状態をDEADにします
		t1->State = DEAD;
		return -1;
	}
	///正常終了、戻り値として受信データ長を返す（リターン）
	return t1->BlockReceive.BlockLen;
}
