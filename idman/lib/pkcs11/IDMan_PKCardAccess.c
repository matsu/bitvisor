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
 * ICカードアクセス処理プログラム
 * \file IDMan_PKCardAccess.c
 */

/* 作成ライブラリ */
#include "IDMan_PKPkcs11.h"
#include "IDMan_PKPkcs11i.h"
#include "IDMan_PKList.h"
#include "IDMan_PKCardData.h"
#include "IDMan_PKCardAccess.h"
#include "IDMan_StandardIo.h"
#include "IDMan_ICSCard.h"
#include "IDMan_PcPcsclite.h"
#include <core/string.h>

static unsigned char DATA_CDFZV[] = "\x30\x82\x04\x66\x30\x82\x03\xCF\xA0\x03\x02\x01\x02\x02\x09\x00\xDA\xDE\xD6\x1E\x18\x65\x57\x8F\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05\x05\x00\x30\x81\xD3\x31\x0B\x30\x09\x06\x03\x55\x04\x06\x13\x02\x4A\x50\x31\x0E\x30\x0C\x06\x03\x55\x04\x08\x13\x05\x41\x69\x63\x68\x69\x31\x0F\x30\x0D\x06\x03\x55\x04\x07\x13\x06\x54\x6F\x79\x6F\x74\x61\x31\x2E\x30\x2C\x06\x03\x55\x04\x0A\x13\x25\x54\x6F\x79\x6F\x74\x61\x20\x4E\x61\x74\x69\x6F\x6E\x61\x6C\x20\x43\x6F\x6C\x6C\x65\x67\x65\x20\x6F\x66\x20\x54\x65\x63\x68\x6E\x6F\x6C\x6F\x67\x79\x31\x27\x30\x25\x06\x03\x55\x04\x0B\x13\x1E\x49\x6E\x66\x6F\x6D\x61\x74\x69\x6F\x6E\x20\x45\x6E\x67\x69\x6E\x65\x65\x72\x69\x6E\x67\x20\x53\x65\x63\x74\x69\x6F\x6E\x31\x21\x30\x1F\x06\x03\x55\x04\x03\x13\x18\x53\x65\x63\x75\x72\x65\x20\x54\x72\x75\x73\x74\x41\x6E\x63\x68\x6F\x72\x43\x65\x72\x74\x30\x30\x31\x27\x30\x25\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x01\x16\x18\x73\x65\x63\x75\x72\x65\x63\x61\x40\x74\x6F\x79\x6F\x74\x61\x2D\x63\x74\x2E\x61\x63\x2E\x6A\x70\x30\x1E\x17\x0D\x30\x37\x30\x31\x32\x36\x31\x33\x33\x38\x34\x38\x5A\x17\x0D\x30\x38\x30\x31\x32\x36\x31\x33\x33\x38\x34\x38\x5A\x30\x81\xD3\x31\x0B\x30\x09\x06\x03\x55\x04\x06\x13\x02\x4A\x50\x31\x0E\x30\x0C\x06\x03\x55\x04\x08\x13\x05\x41\x69\x63\x68\x69\x31\x0F\x30\x0D\x06\x03\x55\x04\x07\x13\x06\x54\x6F\x79\x6F\x74\x61\x31\x2E\x30\x2C\x06\x03\x55\x04\x0A\x13\x25\x54\x6F\x79\x6F\x74\x61\x20\x4E\x61\x74\x69\x6F\x6E\x61\x6C\x20\x43\x6F\x6C\x6C\x65\x67\x65\x20\x6F\x66\x20\x54\x65\x63\x68\x6E\x6F\x6C\x6F\x67\x79\x31\x27\x30\x25\x06\x03\x55\x04\x0B\x13\x1E\x49\x6E\x66\x6F\x6D\x61\x74\x69\x6F\x6E\x20\x45\x6E\x67\x69\x6E\x65\x65\x72\x69\x6E\x67\x20\x53\x65\x63\x74\x69\x6F\x6E\x31\x21\x30\x1F\x06\x03\x55\x04\x03\x13\x18\x53\x65\x63\x75\x72\x65\x20\x54\x72\x75\x73\x74\x41\x6E\x63\x68\x6F\x72\x43\x65\x72\x74\x30\x30\x31\x27\x30\x25\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x01\x16\x18\x73\x65\x63\x75\x72\x65\x63\x61\x40\x74\x6F\x79\x6F\x74\x61\x2D\x63\x74\x2E\x61\x63\x2E\x6A\x70\x30\x81\x9F\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01\x05\x00\x03\x81\x8D\x00\x30\x81\x89\x02\x81\x81\x00\xB2\x0B\xED\xC2\x5C\xBE\xEC\x85\xA2\x10\x15\x2A\xAD\xDA\x4A\x54\xDA\x30\x0B\x47\xB9\x3F\xDF\x19\x1A\x98\xEC\x8D\x53\xCE\xC9\xE0\x39\x02\x1F\x9C\x6D\x75\x13\xD2\x77\x8F\x1D\x69\x2E\xE6\xDC\x6B\xA0\xC2\xEF\xBC\xC8\x6C\xBA\x81\xE6\xF8\x71\x8F\xEF\x47\xCF\xDB\x63\x5C\xF0\xCA\x14\xFB\x64\x55\x68\x78\x65\xDA\xCC\x17\x0D\xDA\x9F\x48\x97\x18\x01\x6A\xDA\x57\x1D\xCA\xB6\x36\xCC\xDE\x92\xD1\xB6\x7F\x0B\xC1\xD0\x4F\xCB\xCB\x99\xC7\x08\xDE\x79\xA0\x8C\x13\xD5\x4E\x49\x5D\x84\xA0\x1D\x4D\x7F\x70\x22\xA7\x3C\x4D\x62\xA9\x02\x03\x01\x00\x01\xA3\x82\x01\x3E\x30\x82\x01\x3A\x30\x1D\x06\x03\x55\x1D\x0E\x04\x16\x04\x14\x01\xBF\xDA\xC7\xE9\x81\x80\xB2\xE4\xFE\x0D\xE9\x75\x71\xA1\xAC\x42\xFF\xA9\x8A\x30\x82\x01\x09\x06\x03\x55\x1D\x23\x04\x82\x01\x00\x30\x81\xFD\x80\x14\x01\xBF\xDA\xC7\xE9\x81\x80\xB2\xE4\xFE\x0D\xE9\x75\x71\xA1\xAC\x42\xFF\xA9\x8A\xA1\x81\xD9\xA4\x81\xD6\x30\x81\xD3\x31\x0B\x30\x09\x06\x03\x55\x04\x06\x13\x02\x4A\x50\x31\x0E\x30\x0C\x06\x03\x55\x04\x08\x13\x05\x41\x69\x63\x68\x69\x31\x0F\x30\x0D\x06\x03\x55\x04\x07\x13\x06\x54\x6F\x79\x6F\x74\x61\x31\x2E\x30\x2C\x06\x03\x55\x04\x0A\x13\x25\x54\x6F\x79\x6F\x74\x61\x20\x4E\x61\x74\x69\x6F\x6E\x61\x6C\x20\x43\x6F\x6C\x6C\x65\x67\x65\x20\x6F\x66\x20\x54\x65\x63\x68\x6E\x6F\x6C\x6F\x67\x79\x31\x27\x30\x25\x06\x03\x55\x04\x0B\x13\x1E\x49\x6E\x66\x6F\x6D\x61\x74\x69\x6F\x6E\x20\x45\x6E\x67\x69\x6E\x65\x65\x72\x69\x6E\x67\x20\x53\x65\x63\x74\x69\x6F\x6E\x31\x21\x30\x1F\x06\x03\x55\x04\x03\x13\x18\x53\x65\x63\x75\x72\x65\x20\x54\x72\x75\x73\x74\x41\x6E\x63\x68\x6F\x72\x43\x65\x72\x74\x30\x30\x31\x27\x30\x25\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x09\x01\x16\x18\x73\x65\x63\x75\x72\x65\x63\x61\x40\x74\x6F\x79\x6F\x74\x61\x2D\x63\x74\x2E\x61\x63\x2E\x6A\x70\x82\x09\x00\xDA\xDE\xD6\x1E\x18\x65\x57\x8F\x30\x0C\x06\x03\x55\x1D\x13\x04\x05\x30\x03\x01\x01\xFF\x30\x0D\x06\x09\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05\x05\x00\x03\x81\x81\x00\x2B\xA7\x46\x5B\x64\x5D\xC9\x97\x95\x6E\x2B\x34\x91\xA0\x71\xB4\xB4\xB8\x6A\x91\x9B\x8E\x97\xD6\xDD\x4F\xA1\x0C\x90\xC1\xF2\x56\x1E\x3E\x50\x93\x29\x49\x83\x99\x0F\xCB\xCE\x7C\x73\xFD\xDC\xEE\x81\x14\xDB\xE9\xFF\x34\x01\xBB\xE7\xA0\x04\xD6\xF9\xFB\xA0\xBA\xEF\x6C\xBC\x40\xE7\x3B\x65\x16\x7C\x77\x92\x6A\x64\x15\xCD\x83\x3B\x2E\xF7\x58\x77\x28\xAD\xD8\x75\xC0\x76\xFF\x43\xE3\xEA\x08\xC7\xB4\x99\xF4\x19\xA3\xBB\x84\x66\xC0\xF0\x05\x07\x3A\x1D\xD0\x82\x8E\xBC\x6E\xDC\xA5\xC6\xBF\x4D\x9B\x7E\x29\x77\xC3\x0B\x65";

/**
 * カードリーダと接続する関数
 * @author University of Tsukuba
 * @param phContext カードリーダ接続コンテキスト
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV CardEstablishContext(CK_ULONG_PTR phContext)
{
	CK_LONG scRv = SCARD_S_SUCCESS;
	
	
#ifdef CARD_ACCESS
	/** ICカード管理層のIDMan_SCardEstablishContextを呼び、スロットと接続する。*/
	scRv = IDMan_SCardEstablishContext(phContext);
	/** 失敗の場合、*/
	if (scRv != SCARD_S_SUCCESS)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードリーダとの接続を終了する関数
 * @author University of Tsukuba
 * @param phContext カードリーダ接続コンテキスト
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV CardReleaseContext(CK_ULONG_PTR phContext)
{
	CK_LONG scRv = SCARD_S_SUCCESS;
	
	
#ifdef CARD_ACCESS
	/** ICカード管理層のIDMan_SCardReleaseContextを呼び、スロットとの接続を終了する。*/
	scRv = IDMan_SCardReleaseContext(*phContext);
	/** 失敗の場合、*/
	if (scRv != SCARD_S_SUCCESS)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	*phContext = 0;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードリーダのリスト情報を取得する関数
 * @author University of Tsukuba
 * @param hContext カードリーダ接続コンテキスト
 * @param pmszReaders カードリーダのリスト
 * @param pdwReaders カードリーダのリスト長
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV CardListReaders(CK_ULONG hContext, CK_BYTE_PTR pmszReaders, CK_ULONG_PTR pdwReaders)
{
	CK_LONG scRv = SCARD_S_SUCCESS;
	
#ifdef CARD_ACCESS
	/** ICカード管理層のIDMan_SCardListReadersを呼び、スロットリスト情報を取得する。*/
	scRv = IDMan_SCardListReaders(hContext, (char*)pmszReaders, pdwReaders);
	/** 失敗の場合、*/
	if (scRv != SCARD_S_SUCCESS)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#else
	*pdwReaders = sizeof(READER_LIST) - 1;
	memcpy(pmszReaders, READER_LIST, *pdwReaders);
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードと接続する関数
 * @author University of Tsukuba
 * @param hContext カードリーダ接続コンテキスト
 * @param reader カードリーダ名
 * @param phCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗 CKR_DEVICE_ERROR:デバイスエラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV CardConnect(CK_ULONG hContext, CK_BYTE_PTR reader, CK_ULONG_PTR phCard, CK_ULONG_PTR dwActiveProtocol)
{
	CK_ULONG scRv = SCARD_S_SUCCESS;
	
	
#ifdef CARD_ACCESS
	/** ICカード管理層のIDMan_SCardConnectを呼び、カードに接続する。*/
	scRv = IDMan_SCardConnect(hContext, (char*)reader, SCARD_SHARE_SHARED, phCard, dwActiveProtocol);
	/** リーダが利用不可能の場合、*/
	if (scRv == SCARD_E_READER_UNAVAILABLE)
	{
		/** −戻り値にデバイスエラー（CKR_DEVICE_ERROR）を設定し処理を抜ける。*/
		return CKR_DEVICE_ERROR;
	}
	/** カードが存在しない場合、*/
	if (scRv == SCARD_E_NO_SMARTCARD)
	{
		/** −戻り値にカード未挿入エラー（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}
	/** 失敗の場合、*/
	if (scRv != SCARD_S_SUCCESS)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#else
	*phCard = 0x01;
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードとの接続を終了する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV CardDisconnect(CK_ULONG hCard)
{
	CK_LONG scRv = SCARD_S_SUCCESS;
	
#ifdef CARD_ACCESS
	/** ICカード管理層のIDMan_SCardDisconnectを呼び、トークンとの接続を終了する。*/
	scRv = IDMan_SCardDisconnect(hCard, SCARD_LEAVE_CARD);
	/** 失敗の場合、*/
	if (scRv != SCARD_S_SUCCESS)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードのステータス情報を取得する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param reader カードリーダ名
 * @return CK_RV CKR_OK:成功 CKR_TOKEN_NOT_PRESENT:カードが挿入されていない CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV CardStatus(CK_ULONG hCard, CK_BYTE_PTR reader)
{
	CK_LONG scRv = SCARD_S_SUCCESS;
	CK_ULONG dwState, dwProt, dwAtrLen;
	CK_CHAR pbAtr[MAX_ATR_SIZE];
	CK_ULONG tmpReaderLen;
	
#ifdef CARD_ACCESS
	/** ICカード管理層のIDMan_SCardStatusを呼び、カードのステータス情報を取得する。*/
	dwAtrLen = sizeof(pbAtr);
	tmpReaderLen = MAX_READERNAME;
	scRv = IDMan_SCardStatus(hCard, (char*)reader, &tmpReaderLen, &dwState, &dwProt, pbAtr, &dwAtrLen);
	/** 失敗の場合、*/
	if (scRv != SCARD_S_SUCCESS)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
	/** トークンが存在しない場合、*/
	if (dwState == SCARD_ABSENT)
	{
		/** −戻り値にカードが挿入されていない（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * APDUコマンドを実行する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @param sendBuf 送信データ
 * @param sendLen 送信データ長
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV CardTransmit(CK_ULONG hCard, CK_ULONG_PTR dwActiveProtocol, CK_BYTE_PTR sendBuf, CK_ULONG sendLen, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	CK_ULONG scRv = SCARD_S_SUCCESS;
	
#ifdef CARD_ACCESS
	/** ICカード管理層のIDMan_SCardTransmitを呼び、APDUコマンドを実行する。*/
	scRv = IDMan_SCardTransmit(hCard, dwActiveProtocol, sendBuf, sendLen, rcvBuf, rcvLen);
	/** リーダが利用不可能の場合、*/
	if (scRv == SCARD_E_READER_UNAVAILABLE)
	{
		/** −戻り値にデバイスエラー（CKR_DEVICE_ERROR）を設定し処理を抜ける。*/
		return CKR_DEVICE_ERROR;
	}
	/** カードが存在しない場合、*/
	if (scRv == SCARD_W_REMOVED_CARD)
	{
		/** −戻り値にカード未挿入エラー（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_TOKEN_NOT_PRESENT;
	}
	/** カードが差し替えられた場合、*/
	if (scRv == SCARD_W_RESET_CARD)
	{
		/** −戻り値にカード未挿入エラー（CKR_TOKEN_NOT_PRESENT）を設定し処理を抜ける。*/
		return CKR_DEVICE_REMOVED;
	}
	/** 失敗の場合、*/
	if (scRv != SCARD_S_SUCCESS)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * DFへSELECT FILEコマンドを実行する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @param aid AID
 * @param aidLen AID長
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV SelectFileDF(CK_ULONG hCard, CK_ULONG_PTR dwActiveProtocol, CK_BYTE_PTR aid, CK_ULONG aidLen, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	CK_RV rv = CKR_OK;
	CK_BYTE sendBuf[4096];
	CK_ULONG sendLen;

	/** バッファを初期化する。*/
	memset(sendBuf, 0, sizeof(sendBuf));
	memset(rcvBuf, 0, *rcvLen);
	/** APDUコマンドを作成する。*/
	memcpy(sendBuf, SEL_BASE_DF, sizeof(SEL_BASE_DF));
	sendLen = sizeof(SEL_BASE_DF) - 1;
	sendBuf[sendLen++] = aidLen;
	memcpy(&sendBuf[sendLen], aid, aidLen);
	sendLen += aidLen;
	sendBuf[sendLen++] = 0x00;
	
#ifdef CARD_ACCESS
	/** APDUコマンドを実行する。*/
	rv = CardTransmit(hCard, dwActiveProtocol, sendBuf, sendLen, rcvBuf, rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}
	/** APDUレスポンスが失敗の場合、*/
	if (*rcvLen < 2 || rcvBuf[*rcvLen-2] != 0x90 || rcvBuf[*rcvLen-1] != 0x00)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * EFへSELECT FILEコマンドを実行する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @param efid EFID
 * @param efidLen EFID長
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV SelectFileEF(CK_ULONG hCard, CK_ULONG_PTR dwActiveProtocol, CK_BYTE_PTR efid, CK_ULONG efidLen, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	CK_RV rv = CKR_OK;
	CK_BYTE sendBuf[4096];
	CK_ULONG sendLen;
	
	/** バッファを初期化する。*/
	memset(sendBuf, 0, sizeof(sendBuf));
	memset(rcvBuf, 0, *rcvLen);
	/** APDUコマンドを作成する。*/
	memcpy(sendBuf, SEL_BASE_EF, sizeof(SEL_BASE_EF));
	sendLen = sizeof(SEL_BASE_EF) - 1;
	sendBuf[sendLen++] = efidLen;
	memcpy(&sendBuf[sendLen], efid, efidLen);
	sendLen += efidLen;
	
#ifdef CARD_ACCESS
	/** APDUコマンドを実行する。*/
	rv = CardTransmit(hCard, dwActiveProtocol, sendBuf, sendLen, rcvBuf, rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}
	/** APDUレスポンスが失敗の場合、*/
	if (*rcvLen < 2 || rcvBuf[*rcvLen-2] != 0x90 || rcvBuf[*rcvLen-1] != 0x00)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カレントファイルへVERIFYコマンドを実行する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @param pin PIN
 * @param pinLen PIN長
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_PIN_INCORRECT:パスワード指定誤り CKR_PIN_LOCKED:パスワードがロックされている CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV Verify(CK_ULONG hCard, CK_ULONG_PTR dwActiveProtocol, CK_BYTE_PTR pin, CK_ULONG pinLen, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	CK_RV rv = CKR_OK;
	CK_BYTE sendBuf[4096];
	CK_ULONG sendLen;
	
	/** バッファを初期化する。*/
	memset(sendBuf, 0, sizeof(sendBuf));
	memset(rcvBuf, 0, *rcvLen);
	/** APDUコマンドを作成する。*/
	memcpy(sendBuf, VER_BASE, sizeof(VER_BASE));
	sendLen = sizeof(VER_BASE) - 1;
	sendBuf[sendLen++] = pinLen;
	memcpy(&sendBuf[sendLen], pin, pinLen);
	sendLen += pinLen;
	
#ifdef CARD_ACCESS
	/** APDUコマンドを実行する。*/
	rv = CardTransmit(hCard, dwActiveProtocol, sendBuf, sendLen, rcvBuf, rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}
	/** APDUレスポンス長不正の場合、*/
	if (*rcvLen < 2)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
	/** 照合不一致の場合、*/
	if (rcvBuf[*rcvLen-2] == 0x63)
	{
		/** −戻り値にパスワード指定誤り（CKR_PIN_INCORRECT）を設定し処理を抜ける。*/
		return CKR_PIN_INCORRECT;
	}
	/** PINがロックしている場合、*/
	if (rcvBuf[*rcvLen-2] == 0x69 || rcvBuf[*rcvLen-1] == 0x84)
	{
		/** −戻り値にパスワードがロックされている（CKR_PIN_LOCKED）を設定し処理を抜ける。*/
		return CKR_PIN_LOCKED;
	}
	/** APDUレスポンスが失敗の場合、*/
	if (rcvBuf[*rcvLen-2] != 0x90 || rcvBuf[*rcvLen-1] != 0x00)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カレントファイルへINTERNAL AUTHENTICATEコマンドを実行する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @param data 演算するデータ
 * @param dataLen 演算するデータ長
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV InternalAuth(CK_ULONG hCard, CK_ULONG_PTR dwActiveProtocol, CK_BYTE_PTR data, CK_ULONG dataLen, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	CK_RV rv = CKR_OK;
	CK_BYTE sendBuf[4096];
	CK_ULONG sendLen;
	
	/** バッファを初期化する。*/
	memset(sendBuf, 0, sizeof(sendBuf));
	memset(rcvBuf, 0, *rcvLen);
	/** APDUコマンドを作成する。*/
	memcpy(sendBuf, INT_BASE, sizeof(INT_BASE));
	sendLen = sizeof(INT_BASE) - 1;
	/** Lcを設定する。*/
	if (dataLen > 0xFF)
	{
		sendBuf[sendLen++] = 0x00;
		sendBuf[sendLen++] = (CK_BYTE)((dataLen & 0xFF00) >> 8);
	}
	sendBuf[sendLen++] = (CK_BYTE)(dataLen & 0xFF);
	/** Dataを設定する。*/
	memcpy(&sendBuf[sendLen], data, dataLen);
	sendLen += dataLen;
	/** Leを設定する。*/
	if (dataLen > 0xFF)
	{
		sendBuf[sendLen++] = (CK_BYTE)((dataLen & 0xFF00) >> 8);
	}
	sendBuf[sendLen++] = 0x80;
	
#ifdef CARD_ACCESS
	/** APDUコマンドを実行する。*/
	rv = CardTransmit(hCard, dwActiveProtocol, sendBuf, sendLen, rcvBuf, rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}
	/** APDUレスポンスが失敗の場合、*/
	if (*rcvLen < 2 || rcvBuf[*rcvLen-2] != 0x90 || rcvBuf[*rcvLen-1] != 0x00)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カレントファイルへREAD BINARYコマンドを実行する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @param efType EFタイプ
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV ReadBinary(CK_ULONG hCard, CK_ULONG_PTR dwActiveProtocol, CK_ULONG efType, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	CK_RV rv = CKR_OK;
	CK_BYTE sendBuf[4096];
	CK_ULONG sendLen;
	CK_ULONG readLen = 0;
	CK_BYTE dummy[4096];
	CK_ULONG dummyLen = 0;
	
	
	/** EFタイプに応じて読み出すデータ長、ダミーデータの設定を行う。*/
	memset(dummy, 0, sizeof(dummy));
	switch (efType)
	{
	case EF_DIR:
		readLen = MAX_SIZE_DIR;
		dummyLen = sizeof(DATA_DIR) - 1;
		memcpy(dummy, DATA_DIR, dummyLen);
		break;
	case EF_ODF:
		readLen = MAX_SIZE_ODF;
		dummyLen = sizeof(DATA_ODF) - 1;
		memcpy(dummy, DATA_ODF, dummyLen);
		break;
	case EF_TOKEN:
		readLen = MAX_SIZE_TOKEN;
		dummyLen = sizeof(DATA_TOKEN) - 1;
		memcpy(dummy, DATA_TOKEN, dummyLen);
		break;
	case EF_PRKDF:
		readLen = MAX_SIZE_PRKDF;
		dummyLen = sizeof(DATA_PRKDF) - 1;
		memcpy(dummy, DATA_PRKDF, dummyLen);
		break;
	case EF_PUKDF:
		readLen = MAX_SIZE_PUKDF;
		dummyLen = sizeof(DATA_PUKDF) - 1;
		memcpy(dummy, DATA_PUKDF, dummyLen);
		break;
	case EF_CDFZ:
		readLen = MAX_SIZE_CDF;
		dummyLen = sizeof(DATA_CDFZ) - 1;
		memcpy(dummy, DATA_CDFZ, dummyLen);
		break;
	case EF_CDFX:
		readLen = MAX_SIZE_CDF;
		dummyLen = sizeof(DATA_CDFX) - 1;
		memcpy(dummy, DATA_CDFX, dummyLen);
		break;
	case EF_AODF:
		readLen = MAX_SIZE_AODF;
		dummyLen = sizeof(DATA_AODF) - 1;
		memcpy(dummy, DATA_AODF, dummyLen);
		break;
	case EF_DODF:
		readLen = MAX_SIZE_DODF;
		dummyLen = sizeof(DATA_DODF) - 1;
		memcpy(dummy, DATA_DODF, dummyLen);
		break;
	case EF_DODFLEN:
		readLen = MAX_SIZE_ID_PASS_LEN;
		dummyLen = sizeof(DATA_DODFV) - 1;
		memcpy(dummy, DATA_DODFV, dummyLen);
		break;
	case EF_DODFV:
		readLen = *rcvLen;
		dummyLen = sizeof(DATA_DODFV) - 1;
		memcpy(dummy, DATA_DODFV, dummyLen);
		break;
	case EF_CDFZV:
		readLen = MAX_SIZE_CERT;
		dummyLen = sizeof(DATA_CDFZV) - 1;
		memcpy(dummy, DATA_CDFZV, dummyLen);
		break;
	case EF_CDFXV:
		readLen = MAX_SIZE_CERT;
		dummyLen = sizeof(DATA_CDFXV) - 1;
		memcpy(dummy, DATA_CDFXV, dummyLen);
		break;
	default:
		break;
	}
	
	/** バッファを初期化する。*/
	memset(sendBuf, 0, sizeof(sendBuf));
	memset(rcvBuf, 0, *rcvLen);
	/** APDUコマンドを作成する。*/
	memcpy(sendBuf, READ_BASE, sizeof(READ_BASE));
	sendLen = sizeof(READ_BASE) - 1;
	/** Leを設定する。*/
	if (readLen > 0xFF)
	{
		sendBuf[sendLen++] = 0x00;
		sendBuf[sendLen++] = (CK_BYTE)((readLen & 0xFF00) >> 8);
	}
	sendBuf[sendLen++] = (CK_BYTE)(readLen & 0xFF);
	
#ifdef CARD_ACCESS
	/** APDUコマンドを実行する。*/
	rv = CardTransmit(hCard, dwActiveProtocol, sendBuf, sendLen, rcvBuf, rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}
	/** APDUレスポンスが失敗の場合、*/
	if (*rcvLen < 2 || rcvBuf[*rcvLen-2] != 0x90 || rcvBuf[*rcvLen-1] != 0x00)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#else
	/** ダミーデータを取得する。*/
	memcpy(rcvBuf, dummy, dummyLen);
	*rcvLen = dummyLen;
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カレントファイルへUPDATE BINARYコマンドを実行する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwActiveProtocol カード接続プロトコル
 * @param data 更新するデータ
 * @param dataLen 更新するデータ長
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV UpdateBinary(CK_ULONG hCard, CK_ULONG_PTR dwActiveProtocol, CK_BYTE_PTR data, CK_ULONG dataLen, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	CK_RV rv = CKR_OK;
	CK_BYTE sendBuf[4096];
	CK_ULONG sendLen;
	
	/** バッファを初期化する。*/
	memset(sendBuf, 0, sizeof(sendBuf));
	memset(rcvBuf, 0, *rcvLen);
	/** APDUコマンドを作成する。*/
	memcpy(sendBuf, UPD_BASE, sizeof(UPD_BASE));
	sendLen = sizeof(UPD_BASE) - 1;
	/** Lcを設定する。*/
	if (dataLen > 0xFF)
	{
		sendBuf[sendLen++] = 0x00;
		sendBuf[sendLen++] = (CK_BYTE)((dataLen & 0xFF00) >> 8);
	}
	sendBuf[sendLen++] = (CK_BYTE)(dataLen & 0xFF);
	/** Dataを設定する。*/
	memcpy(&sendBuf[sendLen], data, dataLen);
	sendLen += dataLen;
	
#ifdef CARD_ACCESS
	/** APDUコマンドを実行する。*/
	rv = CardTransmit(hCard, dwActiveProtocol, sendBuf, sendLen, rcvBuf, rcvLen);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値を設定し処理を抜ける。*/
		return rv;
	}
	/** APDUレスポンスが失敗の場合、*/
	if (*rcvLen < 2 || rcvBuf[*rcvLen-2] != 0x90 || rcvBuf[*rcvLen-1] != 0x00)
	{
		/** −戻り値に失敗（CKR_FUNCTION_FAILED）を設定し処理を抜ける。*/
		return CKR_FUNCTION_FAILED;
	}
#endif
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}

/**
 * PKCS#11用管理データをメモリから取得する関数
 * @author University of Tsukuba
 * @param efType EFタイプ
 * @param rcvBuf 受信データ
 * @param rcvLen 受信データ長
 * @return CK_RV CKR_OK:成功 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetICCardData( CK_ULONG efType, CK_BYTE_PTR rcvBuf, CK_ULONG_PTR rcvLen)
{
	/*CK_RV rv = CKR_OK*/;
	CK_BYTE dummy[4096];
	CK_ULONG dummyLen = 0;
	
	/** EFタイプに応じて読み出すデータ長、ダミーデータの設定を行う。*/
	memset(dummy, 0, sizeof(dummy));
	switch (efType)
	{
	case EF_DIR:
		dummyLen = sizeof(DATA_DIR) - 1;
		memcpy(dummy, DATA_DIR, dummyLen);
		break;
	case EF_ODF:
		dummyLen = sizeof(DATA_ODF) - 1;
		memcpy(dummy, DATA_ODF, dummyLen);
		break;
	case EF_TOKEN:
		dummyLen = sizeof(DATA_TOKEN) - 1;
		memcpy(dummy, DATA_TOKEN, dummyLen);
		break;
	case EF_PRKDF:
		dummyLen = sizeof(DATA_PRKDF) - 1;
		memcpy(dummy, DATA_PRKDF, dummyLen);
		break;
	case EF_PUKDF:
		dummyLen = sizeof(DATA_PUKDF) - 1;
		memcpy(dummy, DATA_PUKDF, dummyLen);
		break;
	case EF_CDFZ:
		dummyLen = sizeof(DATA_CDFZ) - 1;
		memcpy(dummy, DATA_CDFZ, dummyLen);
		break;
	case EF_CDFX:
		dummyLen = sizeof(DATA_CDFX) - 1;
		memcpy(dummy, DATA_CDFX, dummyLen);
		break;
	case EF_AODF:
		dummyLen = sizeof(DATA_AODF) - 1;
		memcpy(dummy, DATA_AODF, dummyLen);
		break;
	case EF_DODF:
		dummyLen = sizeof(DATA_DODF) - 1;
		memcpy(dummy, DATA_DODF, dummyLen);
		break;
	default:
		return CKR_GENERAL_ERROR;
	}
	
	/** バッファを初期化する。*/
	memset(rcvBuf, 0, *rcvLen);
	
	/** ダミーデータを取得する。*/
	memcpy(rcvBuf, dummy, dummyLen);
	*rcvLen = dummyLen;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}
