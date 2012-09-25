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
 * ID管理のCCIDドライバ：プロトコル T=1 制御関数 各種定義
 * \file IDMan_CcProtocol.h
 */

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

typedef unsigned char uchar;


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define BLOCK_T1_LEN_MAX				 259	///< T1ブロック長の最大値 NAD(1)+PCB(1)+LEN(1)+INF(254)+EDC(2)
#define BLOCK_T1_INF_MAX				 254	///< T1ブロックの情報フィールド長の最大値
#define BLOCK_T1_I						0x00	///< 情報ブロック型
#define BLOCK_T1_R						0x80	///< 受信準備完了ブロック型
#define BLOCK_T1_S						0xC0	///< 監視ブロック型
#define BLOCK_T1_MASK					0xC0	///< ブロック種別判定マスク

///要求
#define BLOCK_T1_S_REQUEST_RESYNCH		0xC0	///< RESYNCH要求
#define BLOCK_T1_S_REQUEST_ABORT		0xC2	///< ABORT要求
#define BLOCK_T1_S_REQUEST_IFS			0xC1	///< 情報フィールド長変更要求
#define BLOCK_T1_S_REQUEST_WTX			0xC3	///< ブロック待ち時間延長(WTX)要求
///応答
#define BLOCK_T1_S_RESPONSE_ABORT		0xE2	///< ABORT応答
#define BLOCK_T1_S_RESPONSE_RESYNCH		0xE0	///< RESYNCH応答
#define BLOCK_T1_S_RESPONSE_IFS			0xE1	///< 情報フィールド長変更応答
#define BLOCK_T1_S_RESPONSE_WTX			0xE3	///< ブロック待ち時間延長(WTX)応答


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/// T1ブロック管理構造体
typedef struct {
	uchar BlockData[BLOCK_T1_LEN_MAX];	///< T1ブロックデータ
	int   BlockLen;						///< T1ブロックデータ長
} BlockMngT1;


/// T1プロトコル管理構造体
typedef struct {
	int				Ifsc;				///< カードの情報フィールド長
	int				Edc;				///< 誤り検出符号バイト数(1:LRC、2:CRC)
	uchar			SequenceNum; 		///< 送信シーケンス番号
	uchar			SequenceNumCard; 	///< カードからの情報ブロック送信シーケンス番号 (=nr)
	BlockMngT1		BlockSend;			///< 送信ブロック
	BlockMngT1		BlockReceive;		///< 受信ブロック
	uchar			Wtx;				///< WTX(BWT extension)
	unsigned int	RetryNum;			///< リトライ回数
	int				State;				///< 状態（SENDING/RECEIVING/RESYNCH/DEAD）
	int				Ifsd;				///< 端末の情報フィールド長
	char			More;				///< 送信情報ブロックの継続ブロックありフラグ
	uchar			PreviousBlock[5];	///<最後の送信データを格納：受信準備完了ブロック(R)用
} ProtocolMngT1;


#endif
