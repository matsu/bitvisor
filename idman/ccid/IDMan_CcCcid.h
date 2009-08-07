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
 * ID管理のCCIDドライバ：ドライバ内で用いる関数および各種定義
 * \file IDMan_CcCcid.h
 */


#ifndef __CCID_H
#define __CCID_H
#ifndef IDMAN_STDIO
#define IDMAN_STDIO
#endif
#include "IDMan_StandardIo.h"	// ID管理の標準関数

#include "core/types.h"
#include "common/list.h"
#include "usb.h"
#ifdef NTTCOM
#include "usb_device.h"
#endif
#include "uhci.h"

#include "IDMan_CcAtr.h"
#include "IDMan_CcProtocol.h"


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#if defined __GNUC__
#define INTERNAL __attribute__ ((visibility("hidden")))
#define EXTERNAL __attribute__ ((visibility("default")))
#else
#define INTERNAL
#define EXTERNAL
#endif

#ifndef bool
#define bool unsigned short
#endif
#ifndef TRUE
  #define TRUE	1
#endif 
#ifndef FALSE
  #define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#ifndef uLong
#define uLong	unsigned long
#endif


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define CCID_CLASS		0x0B	// Chip Card Interface Device Class

/* 端末がサポートする機能 dwFeatures */
#define CCID_FEATURES_AUTO_CONF_ATR	0x00000002	///< ATRを元にパラメータを自動設定
#define CCID_FEATURES_AUTO_VOLTAGE	0x00000008	///< カード供給電圧自動選択
#define CCID_FEATURES_AUTO_BAUD		0x00000020	///< 通信速度自動設定
#define CCID_FEATURES_AUTO_PPS_PROP	0x00000040	///< CCIDによって行われる自動パラメータ交換
#define CCID_FEATURES_AUTO_PPS_CUR	0x00000080	///< 活性パラメータに応じて行われる自動パラメータ交換
#define CCID_FEATURES_AUTO_IFSD		0x00000400	///< 自動IFSD交換(T=1)
#define CCID_FEATURES_CHARACTER		0x00000000	///< 交換レベル：文字
#define CCID_FEATURES_TPDU			0x00010000	///< 交換レベル：TPDU
#define CCID_FEATURES_SHORT_APDU	0x00020000	///< 交換レベル：SHORT APDU
#define CCID_FEATURES_EXTENDED_APDU	0x00040000	///< 交換レベル：EXTENDED APDU
#define CCID_FEATURES_EXCHANGE_MASK	0x00070000	///< 交換レベルマスク


/* See CCID rev1.10 specs 6.2.6	*/		/* Table 6.2-3 Slot Status register */
								/* bmICCStatus */
#define CCID_ICC_STATUS_PRESENT_ACTIVE		0x00	/* 00 0000 00 */
#define CCID_ICC_STATUS_PRESENT_INACTIVE	0x01	/* 00 0000 01 */
#define CCID_ICC_STATUS_NOPRESENT			0x02	/* 00 0000 10 */
#define CCID_ICC_STATUS_RFU					0x03	/* 00 0000 11 */

								/* bmCommandStatus */
#define CCID_CMD_STATUS_FAILED				0x40	/* 01 0000 00 */
#define CCID_CMD_STATUS_TIME_EXTENSION		0x80	/* 10 0000 00 */
#define CCID_CMD_STATUS_RFU					0xC0	/* 11 0000 00 */

/*
 * Possible values : bPowerSelect (PC_to_RDR_IccPowerOn)
 * 3 -> 1.8V, 3V, 5V
 * 2 -> 3V, 5V
 * 1 -> 5V only (default)
 * 0 -> automatic (selection made by the reader)
 */
#define CCID_VOLTAGE_AUTO	0
#define CCID_VOLTAGE_5V		1
#define CCID_VOLTAGE_3V		2
#define CCID_VOLTAGE_1_8V	3

/*
 * CCIDのコマンド、応答のオフセット値
 */
#define CCID_OFFSET_BSEQ				6	// bSeq
#define CCID_OFFSET_STATUS				7	// bStatus
#define CCID_OFFSET_ERROR				8	// bError
#define CCID_OFFSET_CHAIN_PARAMETERT	9	// bChainParameter in RDR_to_PC_DataBlock


/*
 *	プロトコル＆パラメータ選択
 */
#define CCID_MAX_LENGTH_PPS		6

#define GetLen(a, x) ((((((a[x+3] << 8) + a[x+2]) << 8) + a[x+1]) << 8) + a[x])


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#define BUFFER_SIZE_BULK		300
#define BUFFER_SIZE_CMD			(4+1+256+1)	// 通信バッファサイズ(MAX=APDUヘッダ+Lc+data+Le)
#define BUFFER_SIZE_RESP		(1+256+2)	// 受信バッファサイズ(MAX=reader status+data+sw)

#define READER_NUMBER_MAX		1			// 最大リーダ端末数（変更不可）
#define SLOTS_NUMBER_MAX		1			// 最大カードスロット数（変更不可）

#define DEFAULT_READ_TIMEOUT	2			// 受信タイムアウト初期値（秒）


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/// カード管理構造体
typedef struct {
	int 				State;	///<リーダのカード状態 ( 0:カードなし / 1:カードあり / 2:電力供給中 )
	ATR 				Atr;
	ProtocolMngT1		ProtocolT1;
	char				NewProtocol;	// ATR_PROTOCOL_TYPE_T0 or ATR_PROTOCOL_TYPE_T1
} CardMng;


#define BUS_DEVICE_STRSIZE 32

/// USB入出力管理構造体
typedef struct {
	struct ID_USB_DEV_HANDLE*	DevHandle;
	struct ID_USB_DEVICE*		Dev;
	char						BusDev[BUS_DEVICE_STRSIZE];
	int							Interface;
	int							BulkIn;
	int							BulkOut;
	int							OpenedSlots;	///<使用しているスロット数
	int							ReaderID;		///< リーダ識別子（VendorID << 16 + ProductID）
} UsbMng;


///brief CCID(Chip Card Interface Device)クラス情報管理構造体
typedef struct {

	unsigned char bSeq;						///< CCID シーケンス番号

	/*=== Interface ===*/
	// [7]
	int bInterfaceProtocol;					///< インターフェースプロトコル（=0）

	/*=== Descriptor ===*/
	// [4]
	unsigned char bMaxSlotIndex;			///< スロット数（0:1slot〜）

	// [10]〜[13]
	uLong dwDefaultClock;					///< カード周波数初期値（KHz)

	// [23]〜[26]
	uLong dwMaxDataRate;					///< カードデータ転送最大速度（KHz）

	// [28]〜[31]
	uLong dwMaxIFSD;						///< T=1で利用するIFSD最大値

	// [40]〜[43]
	uLong dwFeatures;						///< CCIDがサポートするインテリジェント機能

	// [44]〜[47]
	uLong dwMaxCCIDMessageLength;			///< 拡張APDUのメッセージ最大値（261+10〜65544+10）

	// [52]
	char bPINSupport;						///< PINサポート情報（0〜3）

	// CCID Class-Specific Request GET_DATA_RATES
	uLong *arrayOfSupportedDataRates;		///< 端末がサポートする通信速度配列
	uLong supportedDataRates[256+1];		///< 端末がサポートする通信速度配列（実体）
	unsigned int readTimeout;				///< 受信ポートタイムアウト値（秒）：カード要求毎、動的に変化
} CcidMng;

/// リーダ端末管理構造体
typedef struct {
	UsbMng		usb_mng;							/// USB入出力管理構造体
	CcidMng		ccid_mng;						///< CCIDクラス情報構造体
	CardMng		cards_mng[SLOTS_NUMBER_MAX];	///< カード管理構造体

} ReaderMng;


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* return codes */
#define CCID_OK								0		///<正常
#define CCID_ERR_ATR_CHECK					-1		///<ATRエラー			
#define CCID_ERR_BUSY_WITH_AUTO_SEQUENCE	-2		///< F2: 自動シーケンスが動作中							
#define CCID_ERR_CANCEL						-3		///<取り消しエラー		
#define CCID_ERR_CMD_NOT_SUPPORTED			-4		///< 00:コマンドをサポートしていない					
#define CCID_ERR_CMD_SLOT_BUSY				-5		///< E0: コマンド処理中に次のコマンドが発行された				
#define CCID_ERR_COMMAND					-6		///<コマンドエラー			
#define CCID_ERR_COMMAND_FATAL				-7		///<コマンド失敗				
#define CCID_ERR_COMMUNICATION				-8		///<通信エラー				
#define CCID_ERR_ICC_MUTE					-9		///< FE: カードアクセス中にリーダがタイムアウト			
#define CCID_ERR_INSUFFICIENT_BUFFER		-10		///<バッファ不足						
#define CCID_ERR_NO_CARD					-11		///<カードがありません。			
#define CCID_ERR_NO_DATA_SIZE				-12		///<データサイズエラー				
#define CCID_ERR_NO_MATCHING				-13		///<不一致エラー				
#define CCID_ERR_NO_POWERED_CARD			-14		///<カードに電力供給されていません。					
#define CCID_ERR_PARAMETER					-15		///<パラメータエラー			
#define CCID_ERR_PARITY_CHECK				-16		///<パリティエラー				
#define CCID_ERR_PROTOCOL					-17		///< T=1 プロトコルエラー			
#define CCID_ERR_READER_TIMEOUT				-18		///<リーダ：タイムアウトエラー				
#define CCID_ERR_RECEIVE_ABORT				-19		///< ABORT要求受信				
#define CCID_ERR_RECEIVE_RESYNCH			-20		///< RESYNCH応答受信					
#define CCID_ERR_RECEIVE_VPP				-21		///< Vpp誤り通知受信				
#define CCID_ERR_RETRY						-22		///<リトライエラー		
#define CCID_ERR_SEND_LEN					-23		///<データ長エラー			
#define CCID_ERR_SLOT_NOT_EXIST				-24		///< 指定スロットは存在しない				
#define CCID_ERR_SUM_CHECK					-25		///<チェックサムエラー			
#define CCID_ERR_TIMEOUT					-26		///<タイムアウト			
#define CCID_ERR_USB_READ					-27		///<USBデバイスからの読み込みエラー			
#define CCID_ERR_USB_WRITE 					-28		///<USBデバイスへの書き込みエラー			



/*
 * ATR解析関数
 */
extern int   SetATR(ReaderMng* gReder, char SlotNum);
extern uchar GetIFSCData(ATR* AtrData);
extern uchar GetBWICWIData(ATR* AtrData);


/*
 * リーダ端末制御関数
 */
extern int CardStateConfirm(ReaderMng* gReder, char SlotNum, char Flag);

extern int GetCardStatus(ReaderMng* gReder, char SlotNum);
extern int PowerOn(ReaderMng* gReder, char SlotNum, uchar Voltage);
extern int PowerOff(ReaderMng* gReder, char SlotNum);
extern int CardMechanical(ReaderMng* gReder, char SlotNum, uchar bFunction);
extern int TransmitPPS(ReaderMng* gReder, char SlotNum, char* ParamData, unsigned long ParamLen, char* RecData, int* RecDataLen);
extern int BlockSendCardT1(ReaderMng* gReder, char SlotNum, char* ParamData, int ParamLen, char* RecData, unsigned long* RecDataLen);
extern int T1NegociateIFSD(ReaderMng* gReder, uchar SlotNum,  int Ifsd);
extern int ReadDataT0(ReaderMng* gReder, char SlotNum, char* pBuffer, unsigned long * pLength);



extern int TransmitCommand(ReaderMng* gReder, uchar SlotNum,
	unsigned long SendDataLen, const char *pSendData, unsigned short RecDataLen, unsigned char bBWI);
extern int ReceiveReply(ReaderMng* gReder, char SlotNum,
	unsigned long *pRecBufferLen, char *pRecBuffer, char *pLevelParameter);


extern int T0XfrBlockTPDU(ReaderMng* gReder, char SlotNum,
	unsigned long SendDataLen, const uchar *pSendData, unsigned long *pRecBufferLen, uchar *pRecBuffer);
extern int T0XfrBlockCHAR(ReaderMng* gReder, char SlotNum,
	unsigned long SendDataLen, const uchar *pSendData, unsigned long *pRecBufferLen, uchar * pRecBuffer);
extern int T1XfrBlockExtendedAPDU(ReaderMng* gReder, char SlotNum,
			int SendDataLen, char *pSendData, unsigned long *pRecBufferLen, char *pRecBuffer);



/*
 * 内部関数
 */

extern void InitLeaderCardProtocol(ReaderMng* gReder, char SlotNum);
extern int T1XfrBlockTPDU(ReaderMng* gReder, uchar SlotNum, int txLength, uchar* pTxBuffer, unsigned long *pRxLength, uchar* pRxBuffer);
extern int FindBaudRateTable(unsigned long BaudRate, unsigned long *Table);
extern int ExchangePPSData(ReaderMng* gReder, char SlotNum, uchar *PpsData, uchar *PTS1);


/*
 * libUSBラッパ関数
 */
extern bool OpenUSB(ReaderMng* Readers, ReaderMng* gReder);
extern bool CloseUSB(ReaderMng* gReder);

extern int WriteUSB(ReaderMng* gReder, int WriteSize, unsigned char* WriteData);
extern int ReadUSB(ReaderMng* gReder, int Len, unsigned char *Buf);


#endif	//__CCID_H
