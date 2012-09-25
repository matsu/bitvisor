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
 * ID管理のCCIDドライバ：ATR解析関数で用いる各種定義
 * \file IDMan_CcAtr.h
 */
#ifndef _CCATR_H
#define _CCATR_H


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define MAX_ATR_SIZE		33	///< ATR最大データ長(TS〜TCK)

#define ATR_MAX_HISTORICAL	15	///< 履歴文字の最大文字数
#define ATR_MAX_PROTOCOLS	7	///< 最大プロトコル数
#define ATR_MAX_IB			4	///< プロトコル当たりの最大インターフェース文字数

#define ATR_PROTOCOL_TYPE_T0	0	///<CPU搭載カード プロトコル T=0
#define ATR_PROTOCOL_TYPE_T1	1	///<CPU搭載カード プロトコル T=1
#define ATR_PROTOCOL_TYPE_T2	2	/* Protocol type T=2 */
#define ATR_PROTOCOL_TYPE_T3	3	/* Protocol type T=3 */
#define ATR_PROTOCOL_TYPE_T14	14	/* Protocol type T=14 */
#define ATR_PROTOCOL_TYPE_T16	16	///<メモリカード RAWプロトコル

#define ATR_INTERFACE_BYTE_TA	0
#define ATR_INTERFACE_BYTE_TB	1
#define ATR_INTERFACE_BYTE_TC	2
#define ATR_INTERFACE_BYTE_TD	3



/// ATR管理構造体
typedef struct {
	unsigned char		Data[MAX_ATR_SIZE];					///< ATRデータ
	unsigned			DataLen;							///< ATRデータ長(TS〜TCK)
	unsigned char		TS;									///< 開始文字 ( 直接 0x3B / 反転 0x3F )
	unsigned char		T0;									///< 書式 b7-b4: TA1-TD1 あり、b3-b0: 履歴文字数
	struct {
		unsigned char	value;								///< データ
		char			present;							///< 存在フラグ
	}					TCK,								///< チェック文字 (T0を除く）
						ib[ATR_MAX_PROTOCOLS][ATR_MAX_IB];	///< プロトコル
	int					pn; 								///< プロトコル数
	unsigned char		hb[ATR_MAX_HISTORICAL];				///< 履歴文字
	int					hbn;								///< 履歴文字数
} ATR;


#endif
