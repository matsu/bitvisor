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
 * ICカードデータ処理プログラム
 * \file IDMan_PKCardData.c
 */

/* 作成ライブラリ */
#include "IDMan_PKPkcs11.h"
#include "IDMan_PKPkcs11i.h"
#include "IDMan_PKList.h"
#include "IDMan_PKCardData.h"
#include "IDMan_PKCardAccess.h"
#include "IDMan_StandardIo.h"
#include <core/string.h>



/**
 * テキストデータから数値取得関数
 * @author University of Tsukuba
 * @param pTxt テキストデータ
 * @param len テキストデータ長
 * @param pUlong 数値
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetUlByTxt(CK_BYTE_PTR pTxt, CK_LONG len, CK_ULONG_PTR pUlong)
{
	CK_LONG i, tmp;
	
	/** テキストデータを数値に変換する。*/
	*pUlong = 0;
	for (i = 0; i < len; i++)
	{
		tmp = pTxt[i] - '0';
		*pUlong = (*pUlong) * 10 + tmp;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * テキストデータからバージョン値取得関数
 * @author University of Tsukuba
 * @param pTxt テキストデータ
 * @param len テキストデータ長
 * @param pMajor major値
 * @param pMinor minor値
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetVerByTex(CK_BYTE_PTR pTxt, CK_ULONG len, CK_BYTE_PTR pMajor, CK_BYTE_PTR pMinor)
{
	CK_RV rv = CKR_OK;
	CK_BYTE tmp[32];
	CK_ULONG i, dotCount, tmpLen, tmpMajor, tmpMinor;
	CK_LONG dotPos;
	
	/** テキストデータからバージョン値を取得する。*/
	dotPos = -1;
	dotCount = 0;
	tmpLen = 0;
	for (i = 0; i < len; i++)
	{
		if (pTxt[i] >= '0' && pTxt[i] <= '9')
		{
			tmp[tmpLen++] = pTxt[i];
		}
		else if (pTxt[i] == '.')
		{
			if (dotCount > 0)
			{
				break;
			}
			tmp[tmpLen++] = pTxt[i];
			dotPos = i;
			dotCount++;
		}
		else
		{
			break;
		}
	}
	if (dotPos == -1)
	{
		dotPos = tmpLen;
	}
	
	/** major値を取得する。*/
	rv = GetUlByTxt(pTxt, dotPos, &tmpMajor);
	if (tmpMajor > 0xff)
	{
		tmpMajor = 0xff;
	}
	*pMajor = (CK_BYTE)tmpMajor;
	
	/** minor値を取得する。*/
	rv = GetUlByTxt(&pTxt[dotPos+1], tmpLen-1-dotPos, &tmpMinor);
	if (tmpMinor > 0xff)
	{
		tmpMinor = 0xff;
	}
	*pMinor = (CK_BYTE)tmpMinor;
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * バイトデータから数値取得関数
 * @author University of Tsukuba
 * @param pByte バイトデータ
 * @param len バイトデータ長
 * @param pUlong 数値
 * @return CK_RV CKR_OK:成功 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetUlByByte(CK_BYTE_PTR pByte, CK_ULONG len, CK_ULONG_PTR pUlong)
{
	CK_ULONG i;
	
	/** バイトデータ長不正の場合、*/
	if (len > sizeof(CK_ULONG))
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	*pUlong = 0;
	for (i = 0; i < len; i++)
	{
		*pUlong |= ((pByte[i] << (8 * (len - i - 1))));
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * バイトデータをテキストに変換関数
 * @author University of Tsukuba
 * @param HexData バイトデータ
 * @param ulHexSize バイトデータ長
 * @param TxtData テキストデータ
 * @param ulTxtSize テキストデータの長さ
 * @return 変換後の長さ
 * @since 2008.02
 * @version 1.0
 */
CK_ULONG TransHex2Tex(CK_BYTE_PTR HexData, CK_ULONG ulHexSize, CK_BYTE_PTR TxtData, CK_ULONG ulTxtSize)
{
	CK_ULONG cnt, cnt2;

	memset(TxtData, 0, ulTxtSize);
	/** テキストデータの最大長まで繰り返し。*/
	for(cnt = 0, cnt2 = 0; cnt < ulHexSize; cnt++)
	{
		/** −0xA0以上の場合、*/
		if(TxtData[cnt] >= 0xA0)
			/** −−右４ビットシフトし、55加算する。*/
			TxtData[cnt2] = ((HexData[cnt] & 0xF0) >> 4) + 55;
		/** −それ以外の場合、*/
		else
			/** −−右４ビットシフトし、0x30加算する。*/
			TxtData[cnt2] = ((HexData[cnt] & 0xF0) >> 4) + 0x30;

		/** −次のバイトに移動する。*/
		cnt2++;
		/** −0xA0以上の場合、*/
		if(TxtData[cnt] >= 0xA0)
			/** −−55加算する。*/
			TxtData[cnt2] = (HexData[cnt] & 0x0F) + 55;
		/** −それ以外の場合、*/
		else
			/** −−0x30加算する。*/
			TxtData[cnt2] = (HexData[cnt] & 0x0F) + 0x30;
		cnt2++;
	}
	/** テキストデータ長を返却する。*/
	return(cnt2);
}



/**
 * テキストをバイトデータに変換関数
 * @author University of Tsukuba
 * @param TxtData テキストデータ
 * @param ulTxtSize テキストデータの長さ
 * @param HexData バイトデータ
 * @param ulHexSize バイトデータ長
 * @return 変換後の長さ
 * @since 2008.02
 * @version 1.0
 */
CK_ULONG TransTex2Hex(CK_BYTE_PTR TxtData, CK_ULONG ulTxtSize, CK_BYTE_PTR HexData, CK_ULONG ulHexSize)
{
	CK_ULONG	cnt, cnt2;

	memset(HexData, 0, ulHexSize);
	/** テキストデータの最大長まで繰り返し。*/
	for(cnt = 0, cnt2 = 0; cnt2 < ulTxtSize; cnt++)
	{
		/** −'a'以上の場合、*/
		if(TxtData[cnt2] >= 0x61)
			/** −−0x57引き、左４ビットシフトする。*/
			HexData[cnt] = ( (TxtData[cnt2]) - 0x57 ) << 4;
		/** −'A'以上の場合、*/
		else if(TxtData[cnt2] >= 0x41)
			/** −−0x37引き、左４ビットシフトする。*/
			HexData[cnt] = ( (TxtData[cnt2]) - 0x37 ) << 4;
		/** −それ以外の場合、*/
		else if(TxtData[cnt2] >= 0x30)
			/** −−0x30引き、左４ビットシフトする。*/
			HexData[cnt] = ( (TxtData[cnt2] ) - 0x30 ) << 4;
		else
		{
			/** −−そのまま設定する。*/
			HexData[cnt] = TxtData[cnt2];
			cnt2++;
			continue;
		}

		/** −次のバイトに移動する。*/
		cnt2++;
		/** −'a'以上の場合、*/
		if(TxtData[cnt2] >= 0x61)
			/** −−0x57引く。*/
			HexData[cnt] |= ( (TxtData[cnt2]) - 0x57 );
		/** −'A'以上の場合、*/
		else if(TxtData[cnt2] >= 0x41)
			/** −−0x37引く。*/
			HexData[cnt] |= ( (TxtData[cnt2]) - 0x37 );
		/** −それ以外の場合、*/
		else
			/** −−0x30引く。*/
			HexData[cnt] |= ( (TxtData[cnt2]) - 0x30 );
		cnt2++;
	}
	/** バイトデータ長を返還する。*/
	return(cnt);
}



/**
 * TLVデータのLength取得関数
 * @author University of Tsukuba
 * @param iBuf カードデータ（Length開始位置）
 * @param len 取得したLength
 * @param num Lengthの開始位置から終了位置までのバイト数（0〜2）
 * @return CK_RV CKR_OK:成功 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetLength(CK_CHAR_PTR iBuf, CK_ULONG_PTR len, CK_ULONG_PTR num)
{
	CK_CHAR_PTR tmp;
	
	tmp = iBuf;
	/** Lengthデータが2バイト以上表現の場合、*/
	if (*tmp & 0x80)
	{
		/** −Lengthデータが2バイト表現の場合、*/
		if (*tmp & 0x01)
		{
			/** −−次の1バイトをLengthとして取得する。*/
			tmp++;
			*len = *tmp;
			*num = 1;
		}
		/** −Lengthデータが3バイト表現の場合、*/
		else if (*tmp & 0x02)
		{
			/** −−次の2バイトをLengthとして取得する。*/
			tmp++;
			*len = (*tmp << 8) & 0xff00;
			tmp++;
			*len |= *tmp;
			*num = 2;
		}
		/** −上記以外の場合、*/
		else
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
	}
	/** Lengthデータが1バイト表現の場合、*/
	else
	{
		/** −今の1バイトをLengthとして取得する。*/
		*len = *tmp;
		*num = 0;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * TLVデータのLength設定関数
 * @author University of Tsukuba
 * @param iBuf カードデータ（Length開始位置）
 * @param len 設定するLength
 * @param num Lengthの開始位置から終了位置までのバイト数（0〜2）
 * @return CK_RV CKR_OK:成功 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV SetLength(CK_CHAR_PTR iBuf, CK_ULONG len, CK_ULONG_PTR num)
{
	
	/** Lengthが127以下の場合、*/
	if (len <= 127)
	{
		/** −Lengthデータを1バイト表現に設定する。*/
		*iBuf = (CK_BYTE)len;
		*num = 0;
	}
	/** Lengthが255以下の場合、*/
	else if (len <= 255)
	{
		/** −Lengthデータを2バイト表現に設定する。*/
		*iBuf = 0x81;
		*(iBuf+1) = (CK_BYTE)len;
		*num = 1;
	}
	/** Lengthが65535以下の場合、*/
	else if (len <= 65535)
	{
		/** −Lengthデータを3バイト表現に設定する。*/
		*iBuf = 0x82;
		*(iBuf+1) = (CK_BYTE)((len & 0xff00) >> 8);
		*(iBuf+2) = (CK_BYTE)(len & 0xff);
		*num = 2;
	}
	/** 上記以外の場合、*/
	else
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * TLVデータ作成関数
 * @author University of Tsukuba
 * @param iBuf カード内のTLV構造データ
 * @param ppTlvDataList TLVデータリスト
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetTlvByBuf(CK_CHAR_PTR iBuf, CK_I_HEAD_PTR_PTR ppTlvDataList)
{
	CK_RV rv = CKR_OK;
	CK_ULONG tLen, len, tcnt, tlvCnt, num;
	CK_I_TLV_DATA_PTR pTlvData = CK_NULL_PTR;
	
	
	/** カードのデータが初期状態の場合、*/
	if (*iBuf == 0xFF)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** リストの初期化する。*/
	*ppTlvDataList = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (*ppTlvDataList == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(*ppTlvDataList), 0, sizeof(CK_I_HEAD));
	
	/** TLVデータを生成する。*/
	pTlvData = (CK_I_TLV_DATA_PTR)IDMan_StMalloc(sizeof(CK_I_TLV_DATA));
	/** 失敗の場合、*/
	if (pTlvData == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		IDMan_StFree (*ppTlvDataList);
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pTlvData), 0, sizeof(CK_I_TLV_DATA));

	/** Tagを取得する。*/
	pTlvData->tag = *iBuf;
	iBuf++;
	/** Lengthを取得する。*/
	rv = GetLength(iBuf, &tLen, &num);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値をそのまま設定し処理を抜ける。*/
		IDMan_StFree (pTlvData);
		IDMan_StFree (*ppTlvDataList);
		return rv;
	}
	iBuf += num;
	tcnt = 0;
	
	iBuf++;
	tcnt++;
	tlvCnt = 0;
	
	/** Lengthを格納する。*/
	pTlvData->len = tLen;
	
	/** 作成したTLVデータをリストに格納する。*/
	rv = AddElem(*ppTlvDataList, tlvCnt++, (CK_VOID_PTR)pTlvData, VAL_TYPE_TLV_DATA);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値をそのまま設定し処理を抜ける。*/
		IDMan_StFree (pTlvData);
		IDMan_StFree (*ppTlvDataList);
		return rv;
	}
	
	/** バッファ長分繰り返す。*/
	while (tcnt < tLen)
	{
		/** −TLVデータを生成する。*/
		pTlvData = CK_NULL_PTR;
		pTlvData = (CK_I_TLV_DATA_PTR)IDMan_StMalloc(sizeof(CK_I_TLV_DATA));
		/** −失敗の場合、*/
		if (pTlvData == CK_NULL_PTR)
		{
			/** −−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
			DestroyList(*ppTlvDataList);
			return CKR_HOST_MEMORY;
		}
		memset((void*)(pTlvData), 0, sizeof(CK_I_TLV_DATA));
		
		/** −Tagを取得する。*/
		pTlvData->tag = *iBuf;
		iBuf++;
		tcnt++;
		
		/** −Lengthを取得する。*/
		rv = GetLength(iBuf, &len, &num);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			IDMan_StFree (pTlvData);
			DestroyList(*ppTlvDataList);
			return rv;
		}
		iBuf += num;
		tcnt += num;
		
		/** −Lengthを格納する。*/
		pTlvData->len = len;
		iBuf++;
		tcnt++;
		
		/** −Valueを設定する。*/
		switch (pTlvData->tag)
		{
		case 0x30:
		case 0x61:
		case 0x70:
			break;
		default:
			memset(pTlvData->val, 0, sizeof(pTlvData->val));
			memcpy(pTlvData->val, iBuf, len);
			iBuf += len;
			tcnt += len;
			break;
		}
		
		/** −作成したTLVデータをリストに格納する。*/
		rv = AddElem(*ppTlvDataList, tlvCnt++, (CK_VOID_PTR)pTlvData, VAL_TYPE_TLV_DATA);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値をそのまま設定し処理を抜ける。*/
			IDMan_StFree (pTlvData);
			DestroyList(*ppTlvDataList);
			return rv;
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * TLVデータリストからDIRデータ作成関数
 * @author University of Tsukuba
 * @param pTlvDataList TLVデータリスト
 * @param tokenData トークンデータ
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetDirDataByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_TOKEN_DATA_PTR tokenData)
{
	CK_ULONG aidCount;
	CK_I_TLV_DATA_PTR pTlvData = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	
	
	/** DIRデータの領域を確保する。*/
	tokenData->dirData = (CK_I_DIR_DATA_PTR)IDMan_StMalloc(sizeof(CK_I_DIR_DATA));
	/** 失敗の場合、*/
	if (tokenData->dirData == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(tokenData->dirData), 0, sizeof(CK_I_DIR_DATA));
	
	aidCount = 0;
	elem = pTlvDataList->elem;
	/** TLVデータ数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −TLVデータをリストから取得する。*/
		pTlvData = (CK_I_TLV_DATA_PTR)elem->val;
		
		switch (pTlvData->tag)
		{
		/** −Tagが0x4Fの場合、*/
		case 0x4F:
			/** −−AIDを取得する。*/
			switch (aidCount)
			{
			/** −−1個目の場合、*/
			case 0:
				/** −−−PKCS#11用DFのAIDを取得する。*/
				tokenData->dirData->pkcs11DfAidLen = pTlvData->len;
				memcpy(tokenData->dirData->pkcs11DfAid, pTlvData->val, pTlvData->len);
				aidCount++;
				break;
			/** −−2個目の場合、*/
			case 1:
				/** −−−ID管理機能用DFのAIDを取得する。*/
				tokenData->dirData->idFuncDfAidLen = pTlvData->len;
				memcpy(tokenData->dirData->idFuncDfAid, pTlvData->val, pTlvData->len);
				aidCount++;
				break;
			/** −−上記以外の場合、*/
			default:
				/** −−−処理なし。*/
				break;
			}
			break;
		/** −上記以外の場合、*/
		default:
			/** −−処理なし。*/
			break;
		}
		
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * TLVデータリストからODFデータ作成関数
 * @author University of Tsukuba
 * @param pTlvDataList TLVデータリスト
 * @param tokenData トークンデータ
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetOdfDataByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_TOKEN_DATA_PTR tokenData)
{
	CK_I_TLV_DATA_PTR pTlvData = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	
	
	/** ODFデータの領域を確保する。*/
	tokenData->odfData = (CK_I_ODF_DATA_PTR)IDMan_StMalloc(sizeof(CK_I_ODF_DATA));
	/** 失敗の場合、*/
	if (tokenData->odfData == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(tokenData->odfData), 0, sizeof(CK_I_ODF_DATA));
	
	elem = pTlvDataList->elem;
	/** TLVデータ数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −TLVデータをリストから取得する。*/
		pTlvData = (CK_I_TLV_DATA_PTR)elem->val;
		
		switch (pTlvData->tag)
		{
		/** −Tagが0xA0の場合、*/
		case 0xA0:
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−秘密鍵情報のEFIDを取得する。*/
			memcpy(tokenData->odfData->prkEfid, &(pTlvData->val[pTlvData->len-4]), 4);
			break;
		/** −Tagが0xA1の場合、*/
		case 0xA1:
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−公開鍵情報のEFIDを取得する。*/
			memcpy(tokenData->odfData->pukEfid, &(pTlvData->val[pTlvData->len-4]), 4);
			break;
		/** −Tagが0xA3の場合、*/
		case 0xA3:
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−共通鍵情報のEFIDを取得する。*/
			memcpy(tokenData->odfData->sekEfid, &(pTlvData->val[pTlvData->len-4]), 4);
			break;
		/** −Tagが0xA5の場合、*/
		case 0xA5:
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−トラスト証明書情報のEFIDを取得する。*/
			memcpy(tokenData->odfData->trcEfid, &(pTlvData->val[pTlvData->len-4]), 4);
			break;
		/** −Tagが0xA6の場合、*/
		case 0xA6:
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−鍵ペア証明書情報のEFIDを取得する。*/
			memcpy(tokenData->odfData->pucEfid, &(pTlvData->val[pTlvData->len-4]), 4);
			break;
		/** −Tagが0xA7の場合、*/
		case 0xA7:
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−データ情報のEFIDを取得する。*/
			memcpy(tokenData->odfData->doEfid, &(pTlvData->val[pTlvData->len-4]), 4);
			break;
		/** −Tagが0xA8の場合、*/
		case 0xA8:
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−認証情報のEFIDを取得する。*/
			memcpy(tokenData->odfData->aoEfid, &(pTlvData->val[pTlvData->len-4]), 4);
			break;
		/** −上記以外の場合、*/
		default:
			/** −−処理なし。*/
			break;
		}
		
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * TLVデータリストからトークン情報作成関数
 * @author University of Tsukuba
 * @param pTlvDataList TLVデータリスト
 * @param tokenData トークンデータ
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetTokenDataByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_TOKEN_DATA_PTR tokenData)
{
	CK_RV rv = CKR_OK;
	CK_ULONG /*tlvCnt,*/ tmpLen, i, j;
	CK_I_TLV_DATA_PTR pTlvData = CK_NULL_PTR;
	CK_BYTE tmpBuf[1024];
	CK_MECHANISM_TYPE mechanismType;
	CK_MECHANISM_INFO_PTR mechanismInfo = CK_NULL_PTR;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	
	/** トークン情報の領域を確保する。*/
	tokenData->tokenInfo = (CK_TOKEN_INFO_PTR)IDMan_StMalloc(sizeof(CK_TOKEN_INFO));
	/** 失敗の場合、*/
	if (tokenData->tokenInfo == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(tokenData->tokenInfo), 0, sizeof(CK_TOKEN_INFO));
	
	/** メカニズムテーブルの領域を確保する。*/
	tokenData->mechanismTable = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (tokenData->mechanismTable == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(tokenData->mechanismTable), 0, sizeof(CK_I_HEAD));
	
	i = 0;
	elem = pTlvDataList->elem;
	/** TLVデータ数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −TLVデータをリストから取得する。*/
		pTlvData = (CK_I_TLV_DATA_PTR)elem->val;
		
		switch (i++)
		{
		case 0:
			/** −−最初のタグの為処理なし。*/
			break;
		case 1:
			/** −−ラベルを取得する。*/
			memcpy(tokenData->tokenInfo->label, pTlvData->val, pTlvData->len);
			break;
		case 2:
			/** −−製造者IDを取得する。*/
			memcpy(tokenData->tokenInfo->manufacturerID, pTlvData->val, pTlvData->len);
			break;
		case 3:
			/** −−モデルを取得する。*/
			memcpy(tokenData->tokenInfo->model, pTlvData->val, pTlvData->len);
			break;
		case 4:
			/** −−シリアル番号を取得する。*/
			tmpLen = TransHex2Tex(pTlvData->val, pTlvData->len, tmpBuf, pTlvData->len * 2);
			memcpy(tokenData->tokenInfo->serialNumber, tmpBuf, tmpLen);
			break;
		case 5:
			/** −−フラグを取得する。*/
			tokenData->tokenInfo->flags = 0;
			for (j = 0; j < pTlvData->len; j += 2)
			{
				tokenData->tokenInfo->flags |= ((pTlvData->val[j] << 8) | pTlvData->val[j+1]);
			}
			break;
		case 6:
			/** −−最大セッション数を取得する。*/
			tokenData->tokenInfo->ulMaxSessionCount = ((pTlvData->val[0] << 8) | pTlvData->val[1]);
			break;
		case 7:
			/** −−最大R/Wセッション数を取得する。*/
			tokenData->tokenInfo->ulMaxRwSessionCount = ((pTlvData->val[0] << 8) | pTlvData->val[1]);
			break;
		case 8:
			/** −−最大PIN長を取得する。*/
			tokenData->tokenInfo->ulMaxPinLen = (CK_ULONG)pTlvData->val[0];
			break;
		case 9:
			/** −−最小PIN長を取得する。*/
			tokenData->tokenInfo->ulMinPinLen = (CK_ULONG)pTlvData->val[0];
			break;
		case 10:
			/** −−総パブリックメモリを取得する。*/
			tokenData->tokenInfo->ulTotalPublicMemory = ((pTlvData->val[0] << 8) | pTlvData->val[1]);
			break;
		case 11:
			/** −−空きパブリックメモリを取得する。*/
			tokenData->tokenInfo->ulFreePublicMemory = ((pTlvData->val[0] << 8) | pTlvData->val[1]);
			break;
		case 12:
			/** −−総プライベートメモリを取得する。*/
			tokenData->tokenInfo->ulTotalPrivateMemory = ((pTlvData->val[0] << 8) | pTlvData->val[1]);
			break;
		case 13:
			/** −−空きプライベートメモリを取得する。*/
			tokenData->tokenInfo->ulFreePrivateMemory = ((pTlvData->val[0] << 8) | pTlvData->val[1]);
			break;
		case 14:
			/** −−ハードウェアバージョンを取得する。*/
			tokenData->tokenInfo->hardwareVersion.major = pTlvData->val[0];
			tokenData->tokenInfo->hardwareVersion.minor = pTlvData->val[1];
			break;
		case 15:
			/** −−ファームウェアバージョンを取得する。*/
			tokenData->tokenInfo->firmwareVersion.major = pTlvData->val[0];
			tokenData->tokenInfo->firmwareVersion.minor = pTlvData->val[1];
			break;
		case 16:
			/** −−UTC時間を取得する。*/
			tmpLen = TransHex2Tex(pTlvData->val, pTlvData->len, tmpBuf, pTlvData->len * 2);
			memcpy(tokenData->tokenInfo->utcTime, tmpBuf, tmpLen);
			break;
		default:
			/** −−メカニズム情報の領域確保する。*/
			mechanismInfo = CK_NULL_PTR;
			mechanismInfo = (CK_MECHANISM_INFO_PTR)IDMan_StMalloc(sizeof(CK_MECHANISM_INFO));
			/** −−失敗の場合、*/
			if (mechanismInfo == CK_NULL_PTR)
			{
				/** −−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
				return CKR_HOST_MEMORY;
			}
			memset((void*)(mechanismInfo), 0, sizeof(CK_MECHANISM_INFO));
			/** −−メカニズム情報を取得する。*/
			/** −−Lengthが不足の場合、*/
			if (pTlvData->len < 6)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			mechanismType = ((pTlvData->val[0] << 8) | pTlvData->val[1]);
			mechanismInfo->flags = ((pTlvData->val[2] << 24) | (pTlvData->val[3] << 16) | (pTlvData->val[4] << 8) | pTlvData->val[5]);
			
			if (pTlvData->len == 8)
			{
				mechanismInfo->ulMinKeySize = (CK_ULONG)((pTlvData->val[6] << 8) | pTlvData->val[7]) / 8;
				mechanismInfo->ulMaxKeySize = (CK_ULONG)((pTlvData->val[6] << 8) | pTlvData->val[7]) / 8;
			}
			else if (mechanismType == CKM_MD2 || mechanismType == CKM_MD5)
			{
				mechanismInfo->ulMinKeySize = 16;
				mechanismInfo->ulMaxKeySize = 16;
			}
			else if (mechanismType == CKM_SHA_1)
			{
				mechanismInfo->ulMinKeySize = 20;
				mechanismInfo->ulMaxKeySize = 20;
			}
			else
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−メカニズムテーブルに格納する。*/
			rv = SetElem(tokenData->mechanismTable, mechanismType, mechanismInfo, VAL_TYPE_DEFAULT);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値をそのまま設定し処理を抜ける。*/
				return rv;
			}
			break;
		}
		
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * TLVデータリストからオブジェクトテーブル追加関数
 * @author University of Tsukuba
 * @param pTlvDataList TLVデータリスト
 * @param objectTable オブジェクトテーブル
 * @param phObjectCnt オブジェクトハンドルカウンタ
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV AddObjTblByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_HEAD_PTR objectTable, CK_ULONG_PTR phObjectCnt)
{
	CK_RV rv = CKR_OK;
	CK_I_TLV_DATA_PTR pTlvData = CK_NULL_PTR;
	CK_ULONG count = 0;
	CK_I_ELEM_PTR elem = CK_NULL_PTR;
	CK_I_HEAD_PTR pObject = CK_NULL_PTR;
	CK_BYTE tag;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_ULONG attrType;
	CK_VOID_PTR  pAttrValue = CK_NULL_PTR;
	CK_ULONG ulAttrValueLen;
	/*char				LogBuff[1024];*/		//ログ出力バッファ
	
	/** TLVデータリストが0x00の場合、*/
	if (pTlvDataList == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** TLVデータリストを基に、オブジェクトをテーブルに追加する。*/
	/** オブジェクトを作成する。*/
	pObject = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (pObject == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pObject), 0, sizeof(CK_I_HEAD));
	
	count = 0;
	elem = pTlvDataList->elem;
	/** TLVデータ数分以下を繰り返す。*/
	while (elem != CK_NULL_PTR)
	{
		/** −TLVデータを取得する。*/
		pTlvData = (CK_I_TLV_DATA_PTR)elem->val;
		
		/** −Tagが0x30または0x70で、かつオブジェクトに要素ありの場合、*/
		if ((pTlvData->tag == 0x30 || pTlvData->tag == 0x70) && pObject->num > 0)
		{
			/** −−オブジェクトをテーブルに追加する。*/
			rv = SetElem(objectTable, (*phObjectCnt)++, (CK_VOID_PTR)pObject, VAL_TYPE_HEAD);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値をそのまま設定し処理を抜ける。*/
				return rv;
			}
			/** −−オブジェクトを作成する。*/
			pObject = CK_NULL_PTR;
			pObject = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
			/** −−失敗の場合、*/
			if (pObject == CK_NULL_PTR)
			{
				/** −−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
				return CKR_HOST_MEMORY;
			}
			memset((void*)(pObject), 0, sizeof(CK_I_HEAD));
		}
		/** −Tagが0xA0、0xA1、0xA2、0xA3のいずれかの場合、*/
		else if (pTlvData->tag == 0xA0 || pTlvData->tag == 0xA1 || pTlvData->tag == 0xA2 || pTlvData->tag == 0xA3)
		{
			/** −−属性情報を作成し、オブジェクトに追加する。*/
			/** −−次のTLVデータが0x00の場合、*/
			if (elem->next == CK_NULL_PTR)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−Tagを取得する。*/
			tag = pTlvData->tag;
			/** −−Valueを取得する。（数字に変換）*/
			rv = GetUlByByte(pTlvData->val, pTlvData->len, &attrType);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値をそのまま設定し処理を抜ける。*/
				return rv;
			}
			/** −−Valueの下位2バイト（属性タイプ）を取得する。*/
			attrType &= 0x0000FFFF;
			/** −−EFIDを示す属性タイプを変換する。*/
			switch (attrType)
			{
			case CKA_VALUE:
			case CKA_MODULUS:
				attrType = CKA_EFID;
				break;
			default:
				break;
			}
			
			/** −−次のTLVデータを取得する。*/
			elem = elem->next;
			pTlvData = (CK_I_TLV_DATA_PTR)elem->val;
			/** −−次のtagが0x04または0x0C以外の場合、*/
			if (pTlvData->tag != 0x04 && pTlvData->tag != 0x0C)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−属性情報の値のサイズを取得する。*/
			switch (attrType)
			{
			/** −−数字の場合はCK_ULONG型のサイズに設定。*/
			case CKA_CLASS:
			case CKA_KEY_TYPE:
			case CKA_KEY_GEN_MECHANISM:
			case CKA_CERTIFICATE_TYPE:
			case CKA_CERTIFICATE_CATEGORY:
			case CKA_JAVA_MIDP_SECURITY_DOMAIN:
				ulAttrValueLen = sizeof(CK_ULONG);
				break;
			/** −−日付の場合は2倍に設定する。*/
			case CKA_START_DATE:
			case CKA_END_DATE:
				ulAttrValueLen = pTlvData->len * 2;
				break;
			/** −−上記以外はそのまま設定する。*/
			default:
				ulAttrValueLen = pTlvData->len;
				break;
			}
			
			pAttrValue = CK_NULL_PTR;
			/** −−属性情報の値のサイズが0より大の場合、*/
			if (ulAttrValueLen > 0)
			{
				/** −−−属性情報の値の領域を確保する。*/
				pAttrValue = IDMan_StMalloc(ulAttrValueLen);
				/** −−−失敗の場合、*/
				if (pAttrValue == CK_NULL_PTR)
				{
					/** −−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
					return CKR_HOST_MEMORY;
				}
				memset((void*)(pAttrValue), 0, ulAttrValueLen);
				
				/** −−−属性情報の値を取得する。*/
				switch (tag)
				{
				/** −−−Tagが0xA0の場合、*/
				case 0xA0:
					switch (attrType)
					{
					/** −−−−以下は数字に変換する。*/
					case CKA_CLASS:
						rv = GetUlByByte(pTlvData->val, pTlvData->len, (CK_ULONG_PTR)pAttrValue);
						/** −−−−−失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
						break;
					/** −−−−以下はそのままバイトデータとして取得する。*/
					case CKA_TOKEN:
					case CKA_PRIVATE:
					case CKA_LABEL:
						memcpy(pAttrValue, pTlvData->val, pTlvData->len);
						break;
					/** −−−−上記以外、*/
					default:
						/** −−−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
						return CKR_GENERAL_ERROR;
						break;
					}
					break;
				/** −−−Tagが0xA1の場合、*/
				case 0xA1:
					switch (attrType)
					{
					/** −−−−以下は数字に変換する。*/
					case CKA_KEY_TYPE:
					case CKA_KEY_GEN_MECHANISM:
					case CKA_CERTIFICATE_TYPE:
					case CKA_CERTIFICATE_CATEGORY:
						rv = GetUlByByte(pTlvData->val, pTlvData->len, (CK_ULONG_PTR)pAttrValue);
						/** −−−−−失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
						break;
					/** −−−−以下は日付文字列に変換する。*/
					case CKA_START_DATE:
					case CKA_END_DATE:
						TransHex2Tex(pTlvData->val, pTlvData->len, (CK_BYTE_PTR)pAttrValue, ulAttrValueLen);
						break;
					/** −−−−以下はそのままバイトデータとして取得する。*/
					case CKA_APPLICATION:
					case CKA_OBJECT_ID:
					case CKA_TRUSTED:
					case CKA_CHECK_VALUE:
					case CKA_ID:
					case CKA_DERIVE:
					case CKA_LOCAL:
					case CKA_EFID:
						memcpy(pAttrValue, pTlvData->val, pTlvData->len);
						break;
					/** −−−−上記以外、*/
					default:
						/** −−−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
						return CKR_GENERAL_ERROR;
						break;
					}
					break;
				/** −−−Tagが0xA2の場合、*/
				case 0xA2:
					switch (attrType)
					{
					/** −−−−以下は数字に変換する。*/
					case CKA_JAVA_MIDP_SECURITY_DOMAIN:
						rv = GetUlByByte(pTlvData->val, pTlvData->len, (CK_ULONG_PTR)pAttrValue);
						/** −−−−−失敗の場合、*/
						if (rv != CKR_OK)
						{
							/** −−−−−−戻り値をそのまま設定し処理を抜ける。*/
							return rv;
						}
						break;
					/** −−−−以下はそのままバイトデータとして取得する。*/
					case CKA_ISSUER:
					case CKA_SERIAL_NUMBER:
					case CKA_URL:
					case CKA_HASH_OF_SUBJECT_PUBLIC_KEY:
					case CKA_HASH_OF_ISSUER_PUBLIC_KEY:
					case CKA_SUBJECT:
					case CKA_ID:
					case CKA_SENSITIVE:
					case CKA_DECRYPT:
					case CKA_UNWRAP:
					case CKA_SIGN:
					case CKA_SIGN_RECOVER:
					case CKA_EXTRACTABLE:
					case CKA_NEVER_EXTRACTABLE:
					case CKA_ALWAYS_SENSITIVE:
					case CKA_ALWAYS_AUTHENTICATE:
					case CKA_WRAP_WITH_TRUSTED:
					case CKA_EFID:
						memcpy(pAttrValue, pTlvData->val, pTlvData->len);
						break;
					/** −−−−上記以外、*/
					default:
						/** −−−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
						return CKR_GENERAL_ERROR;
						break;
					}
					break;
				/** −−−Tagが0xA3の場合、*/
				case 0xA3:
					switch (attrType)
					{
					/** −−−−以下はそのままバイトデータとして取得する。*/
					case CKA_EFID:
						memcpy(pAttrValue, pTlvData->val, pTlvData->len);
						break;
					/** −−−−上記以外、*/
					default:
						/** −−−−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
						return CKR_GENERAL_ERROR;
						break;
					}
					break;
				default:
					break;
				}
			}
			
			/** −−属性情報の領域を確保する。*/
			pAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
			/** −−失敗の場合、*/
			if (pAttr == CK_NULL_PTR)
			{
				IDMan_StFree(pAttrValue);
				/** −−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
				return CKR_HOST_MEMORY;
			}
			memset((void*)(pAttr), 0, sizeof(CK_ATTRIBUTE));
			/** −−属性情報を設定する。*/
			pAttr->type = (CK_ATTRIBUTE_TYPE)attrType;
			pAttr->pValue = pAttrValue;
			pAttr->ulValueLen = ulAttrValueLen;
			/** −−オブジェクトに属性情報を追加する。*/
			rv = SetElem(pObject, attrType, (CK_VOID_PTR)pAttr, VAL_TYPE_ATTRIBUTE);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				IDMan_StFree(pAttrValue);
				IDMan_StFree(pAttr);
				/** −−−戻り値をそのまま設定し処理を抜ける。*/
				return rv;
			}
			
			/** −−属性タイプがCKA_IDの場合、*/
			if (attrType == CKA_ID)
			{
				/** −−−インデックス属性情報の値の領域を確保する。*/
				pAttrValue = IDMan_StMalloc(sizeof(CK_ULONG));
				/** −−−失敗の場合、*/
				if (pAttrValue == CK_NULL_PTR)
				{
					/** −−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
					return CKR_HOST_MEMORY;
				}
				memset((void*)(pAttrValue), 0, sizeof(CK_ULONG));
				
				/** −−−Valueを数字に変換し、インデックス値を取得する。*/
				rv = GetUlByByte(pTlvData->val, 4, (CK_ULONG_PTR)pAttrValue);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値をそのまま設定し処理を抜ける。*/
					return rv;
				}
				
				/** −−−インデックス属性情報の領域を確保する。*/
				pAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
				/** −−−失敗の場合、*/
				if (pAttr == CK_NULL_PTR)
				{
					IDMan_StFree(pAttrValue);
					/** −−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
					return CKR_HOST_MEMORY;
				}
				memset((void*)(pAttr), 0, sizeof(CK_ATTRIBUTE));
				/** −−−インデックス属性情報を設定する。*/
				pAttr->type = CKA_INDEX;
				pAttr->pValue = pAttrValue;
				pAttr->ulValueLen = 4;
				/** −−−オブジェクトにインデックス属性情報を追加する。*/
				rv = SetElem(pObject, CKA_INDEX, (CK_VOID_PTR)pAttr, VAL_TYPE_ATTRIBUTE);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					IDMan_StFree(pAttrValue);
					IDMan_StFree(pAttr);
					/** −−−−戻り値をそのまま設定し処理を抜ける。*/
					return rv;
				}
			}
		}
		/** −次の要素を取得する。*/
		elem = elem->next;
	}
	
	/** オブジェクトをテーブルに追加する。*/
	rv = SetElem(objectTable, (*phObjectCnt)++, (CK_VOID_PTR)pObject, VAL_TYPE_HEAD);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		IDMan_StFree(pAttrValue);
		IDMan_StFree(pAttr);
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードの証明書実データをオブジェクトに追加する関数
 * @author University of Tsukuba
 * @param iBuf カードの証明書実データ
 * @param pObject オブジェクト
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV AddCertDataByBuf(CK_BYTE_PTR iBuf, CK_I_HEAD_PTR pObject)
{
	CK_RV rv = CKR_OK;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_VOID_PTR  pAttrValue = CK_NULL_PTR;
	CK_ULONG ulAttrValueLen, num;
	CK_BYTE_PTR p;
	
	
	/** カードのデータが初期状態の場合、*/
	if (*iBuf == 0xFF)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	p = iBuf;
	p++;
	/** Lengthを取得する。*/
	rv = GetLength(p, &ulAttrValueLen, &num);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	ulAttrValueLen += num+2;
	
	
	/** 属性情報の値の領域を確保する。*/
	pAttrValue = (CK_BYTE_PTR)IDMan_StMalloc(ulAttrValueLen);
	/** 失敗の場合、*/
	if (pAttrValue == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pAttrValue), 0, ulAttrValueLen);
	
	/** 属性情報の値を取得する。*/
	memcpy(pAttrValue, iBuf, ulAttrValueLen);
	
	/** 属性情報の領域を確保する。*/
	pAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
	/** 失敗の場合、*/
	if (pAttr == CK_NULL_PTR)
	{
		IDMan_StFree(pAttrValue);
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pAttr), 0, sizeof(CK_ATTRIBUTE));

	/** 属性情報を設定する。*/
	pAttr->type = CKA_VALUE;
	pAttr->pValue = pAttrValue;
	pAttr->ulValueLen = ulAttrValueLen;
	
	
	/** オブジェクトに属性情報を追加する。*/
	rv = SetElem(pObject, (CK_ULONG)pAttr->type, (CK_VOID_PTR)pAttr, VAL_TYPE_ATTRIBUTE);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		IDMan_StFree(pAttrValue);
		IDMan_StFree(pAttr);
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードのID/パスワード実データをオブジェクトに追加する関数
 * @author University of Tsukuba
 * @param iBuf カードのID/パスワード実データ
 * @param pObject オブジェクト
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV AddIdPassDataByBuf(CK_BYTE_PTR iBuf, CK_I_HEAD_PTR pObject)
{
	CK_RV rv = CKR_OK;
	CK_BYTE_PTR idPos, passPos;
	CK_ULONG tLen, tcnt, idLen, passLen, tmpLen, count, num;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_VOID_PTR  pAttrValue = CK_NULL_PTR;
	CK_I_HEAD_PTR pIdList = CK_NULL_PTR;
	CK_I_HEAD_PTR pPassList = CK_NULL_PTR;
	
	
	/** IDリストを作成する。*/
	pIdList = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (pIdList == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pIdList), 0, sizeof(CK_I_HEAD));
	
	/** パスワードリストを作成する。*/
	pPassList = (CK_I_HEAD_PTR)IDMan_StMalloc(sizeof(CK_I_HEAD));
	/** 失敗の場合、*/
	if (pPassList == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pPassList), 0, sizeof(CK_I_HEAD));
	
	
	/** オブジェクトにIDリストを追加する。*/
	rv = AddElem(pObject, CKA_VALUE_ID, (CK_VOID_PTR)pIdList, VAL_TYPE_HEAD);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	
	/** オブジェクトにパスワードリストを追加する。*/
	rv = AddElem(pObject, CKA_VALUE_PASSWORD, (CK_VOID_PTR)pPassList, VAL_TYPE_HEAD);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	
	
	/** カードのデータが初期状態の場合、*/
	if (*iBuf == 0xFF)
	{
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	
	iBuf++;
	
	/** Lengthを取得する。*/
	rv = GetLength(iBuf, &tLen, &num);
	/** 失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値をそのまま設定し処理を抜ける。*/
		return rv;
	}
	iBuf += num;
	tcnt = 0;
	
	iBuf++;
	tcnt++;
	count = 0;
	/** バッファ長分繰り返す。*/
	while (tcnt < tLen)
	{
		/** −Tagが0xA0の場合、*/
		if (*iBuf == 0xA0)
		{
			iBuf++;
			tcnt++;
			/** −−Length分進める。*/
			rv = GetLength(iBuf, &tmpLen, &num);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値をそのまま設定し処理を抜ける。*/
				return rv;
			}
			iBuf += num;
			tcnt += num;
			
			iBuf++;
			tcnt++;
			/** −−次のTagが0x0Cの場合、IDを取得する。*/
			if (*iBuf == 0x0C)
			{
				iBuf++;
				tcnt++;
				
				/** −−−IDのLengthを取得する。*/
				rv = GetLength(iBuf, &idLen, &num);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値をそのまま設定し処理を抜ける。*/
					return rv;
				}
				iBuf += num;
				tcnt += num;
				
				iBuf++;
				tcnt++;
				/** −−−IDの位置を取得する。*/
				idPos = iBuf;
				iBuf += idLen;
				tcnt += idLen;
				
				
				/** −−−次のTagが0x0Cの場合、パスワードを取得する。*/
				if (*iBuf == 0x0C)
				{
					iBuf++;
					tcnt++;
					
					/** −−−−パスワードのLengthを取得する。*/
					rv = GetLength(iBuf, &passLen, &num);
					/** −−−−失敗の場合、*/
					if (rv != CKR_OK)
					{
						/** −−−−−戻り値をそのまま設定し処理を抜ける。*/
						return rv;
					}
					iBuf += num;
					tcnt += num;
					
					iBuf++;
					tcnt++;
					/** −−−−パスワードの位置を取得する。*/
					passPos = iBuf;
					iBuf += passLen;
					tcnt += passLen;
					
					
					/** −−−−ID属性情報を作成し、IDリストに追加する。*/
					/** −−−−属性情報の値の領域を確保する。*/
					pAttrValue = IDMan_StMalloc(idLen);
					/** −−−−失敗の場合、*/
					if (pAttrValue == CK_NULL_PTR)
					{
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset((void*)(pAttrValue), 0, idLen);

					/** −−−−属性情報の値を取得する。*/
					memcpy(pAttrValue, idPos, idLen);
					/** −−−−属性情報の領域を確保する。*/
					pAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
					/** −−−−失敗の場合、*/
					if (pAttr == CK_NULL_PTR)
					{
						IDMan_StFree(pAttrValue);
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset((void*)(pAttr), 0, sizeof(CK_ATTRIBUTE));

					/** −−−−属性情報を設定する。*/
					pAttr->type = CKA_VALUE_ID;
					pAttr->pValue = pAttrValue;
					pAttr->ulValueLen = idLen;
					/** −−−−IDリストに属性情報を追加する。*/
					rv = AddElem(pIdList, count, (CK_VOID_PTR)pAttr, VAL_TYPE_ATTRIBUTE);
					/** −−−−失敗の場合、*/
					if (rv != CKR_OK)
					{
						IDMan_StFree(pAttrValue);
						IDMan_StFree(pAttr);
						/** −−−−−戻り値をそのまま設定し処理を抜ける。*/
						return rv;
					}
					
					
					/** −−−−パスワード属性情報を作成し、パスワードリストに追加する。*/
					/** −−−−属性情報の値の領域を確保する。*/
					pAttrValue = IDMan_StMalloc(passLen);
					/** −−−−失敗の場合、*/
					if (pAttrValue == CK_NULL_PTR)
					{
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset((void*)(pAttrValue), 0, passLen);

					/** −−−−属性情報の値を取得する。*/
					memcpy(pAttrValue, passPos, passLen);
					/** −−−−属性情報の領域を確保する。*/
					pAttr = (CK_ATTRIBUTE_PTR)IDMan_StMalloc(sizeof(CK_ATTRIBUTE));
					/** −−−−失敗の場合、*/
					if (pAttr == CK_NULL_PTR)
					{
						IDMan_StFree(pAttrValue);
						/** −−−−−戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
						return CKR_HOST_MEMORY;
					}
					memset((void*)(pAttr), 0, sizeof(CK_ATTRIBUTE));
					/** −−−−属性情報を設定する。*/
					pAttr->type = CKA_VALUE_PASSWORD;
					pAttr->pValue = pAttrValue;
					pAttr->ulValueLen = passLen;
					/** −−−−パスワードリストに属性情報を追加する。*/
					rv = AddElem(pPassList, count, (CK_VOID_PTR)pAttr, VAL_TYPE_ATTRIBUTE);
					/** −−−−失敗の場合、*/
					if (rv != CKR_OK)
					{
						IDMan_StFree(pAttrValue);
						IDMan_StFree(pAttr);
						/** −−−−−戻り値をそのまま設定し処理を抜ける。*/
						return rv;
					}
					
					count++;
				}
			}
		}
		/** −上記以外の場合、*/
		else
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * カードから実データ取得関数
 * @author University of Tsukuba
 * @param pObject オブジェクト
 * @param attrType 属性タイプ
 * @param pSlotData スロットデータ
 * @return CK_RV CKR_OK:成功 CKR_TOKEN_NOT_RECOGNIZED:不正なICカードを検出した CKR_GENERAL_ERROR:一般エラー CKR_FUNCTION_FAILED:失敗
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetValueData(CK_I_HEAD_PTR pObject, CK_ATTRIBUTE_TYPE attrType, CK_I_SLOT_DATA_PTR pSlotData)
{
	CK_RV rv = CKR_OK;
	CK_BYTE rcvBuf[4096];
	CK_ULONG rcvLen;
	CK_ATTRIBUTE_PTR pAttr = CK_NULL_PTR;
	CK_ULONG classVal;
	CK_ULONG tLen,num;
	CK_BYTE labelVal[256], efid[4];
	
	
	/** オブジェクトからCLASS属性を取得する。*/
	rv = GetElem(pObject, CKA_CLASS, (CK_VOID_PTR_PTR)&pAttr);
	/** 取得失敗の場合、*/
	if (rv != CKR_OK)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** CLASS属性値を取得する。*/
	memcpy((void*)&classVal, pAttr->pValue, sizeof(classVal));
	
	/** CLASS属性がCertificateの場合、*/
	if (classVal == CKO_CERTIFICATE)
	{
		/** −引数の属性がCKA_VALUEではない場合、*/
		if (attrType != CKA_VALUE)
		{
			/** −−戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
			return CKR_OK;
		}
		
		/** −オブジェクトから引数の属性を取得する。*/
		rv = GetElem(pObject, attrType, (CK_VOID_PTR_PTR)&pAttr);
		/** −取得成功の場合、*/
		if (rv == CKR_OK)
		{
			/** −−戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
			return CKR_OK;
		}
		
		/** −オブジェクトからEFID属性を取得する。*/
		rv = GetElem(pObject, CKA_EFID, (CK_VOID_PTR_PTR)&pAttr);
		/** −取得失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		/** −EFID長が不正の場合、*/
		if (pAttr->ulValueLen != 4)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		/** −EFIDを取得する。*/
		memcpy(efid, pAttr->pValue, pAttr->ulValueLen);
		
		/** −証明書実データ格納パスへのSELECT FILEコマンドを実行する。*/
		rcvLen = sizeof(rcvBuf);
		rv = SelectFileEF(pSlotData->hCard, &(pSlotData->dwActiveProtocol), &efid[2], 2, rcvBuf, &rcvLen);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値を設定し処理を抜ける。*/
			return rv;
		}
		
		
		/** −証明書実データ格納パスへのREAD BINARYコマンドを実行する。*/
		rcvLen = sizeof(rcvBuf);
		rv = ReadBinary(pSlotData->hCard, &(pSlotData->dwActiveProtocol), EF_CDFZV, rcvBuf, &rcvLen);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値を設定し処理を抜ける。*/
			return rv;
		}
		
		
		/** −ICカードから取得したBINARYデータを基に、実データをオブジェクトに追加する。*/
		rv = AddCertDataByBuf(rcvBuf, pObject);
		/** −失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
			return CKR_TOKEN_NOT_RECOGNIZED;
		}
	}
	/** CLASS属性がDataの場合、*/
	else if (classVal == CKO_DATA)
	{
		/** −オブジェクトからLABEL属性を取得する。*/
		rv = GetElem(pObject, CKA_LABEL, (CK_VOID_PTR_PTR)&pAttr);
		/** −取得失敗の場合、*/
		if (rv != CKR_OK)
		{
			/** −−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
			return CKR_GENERAL_ERROR;
		}
		
		/** −LABEL属性値を取得する。*/
		memset(labelVal, 0, sizeof(labelVal));
		memcpy(labelVal, pAttr->pValue, pAttr->ulValueLen);
		
		/** −LABEL属性が"ID/PASS"の場合、*/
		if (memcmp(labelVal, CK_LABEL_ID_PASS, pAttr->ulValueLen) == 0)
		{
			/** −−引数の属性がCKA_VALUE_IDまたはCKA_VALUE_PASSWORDではない場合、*/
			if (attrType != CKA_VALUE_ID && attrType != CKA_VALUE_PASSWORD)
			{
				/** −−−戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
				return CKR_OK;
			}
			
			/** −−オブジェクトから引数の属性を取得する。*/
			rv = GetElem(pObject, attrType, (CK_VOID_PTR_PTR)&pAttr);
			/** −−取得成功の場合、*/
			if (rv == CKR_OK)
			{
				/** −−−戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
				return CKR_OK;
			}
			
			/** −−オブジェクトからEFID属性を取得する。*/
			rv = GetElem(pObject, CKA_EFID, (CK_VOID_PTR_PTR)&pAttr);
			/** −−取得失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−EFID長が不正の場合、*/
			if (pAttr->ulValueLen != 4)
			{
				/** −−−戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
				return CKR_GENERAL_ERROR;
			}
			/** −−EFIDを取得する。*/
			memcpy(efid, pAttr->pValue, pAttr->ulValueLen);
			
			/** −−ID/パスワード実データ格納パスへのSELECT FILEコマンドを実行する。*/
			rcvLen = sizeof(rcvBuf);
			rv = SelectFileEF(pSlotData->hCard, &(pSlotData->dwActiveProtocol), &efid[2], 2, rcvBuf, &rcvLen);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値を設定し処理を抜ける。*/
				return rv;
			}
			
			
			/** −−ID/パスワード実データ格納パスレングスまでのREAD BINARYコマンドを実行する。*/
			rcvLen = sizeof(rcvBuf);
			rv = ReadBinary(pSlotData->hCard, &(pSlotData->dwActiveProtocol), EF_DODFLEN, rcvBuf, &rcvLen);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値を設定し処理を抜ける。*/
				return rv;
			}

			/** −−カードのデータが初期状態でない場合、*/
			if (rcvBuf[0] != 0xFF)
			{

				/** −−−Lengthを取得する。*/
				rv = GetLength(rcvBuf+1, &tLen, &num);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
					return CKR_TOKEN_NOT_RECOGNIZED;
				}
	
				/** −−−ID/パスワード実データ格納パスへのREAD BINARYコマンドを実行する。*/
				rcvLen = 1 + 1 + num + tLen + 1;
				rv = ReadBinary(pSlotData->hCard, &(pSlotData->dwActiveProtocol), EF_DODFV, rcvBuf, &rcvLen);
				/** −−−失敗の場合、*/
				if (rv != CKR_OK)
				{
					/** −−−−戻り値を設定し処理を抜ける。*/
					return rv;
				}
			}

			/** −−ICカードから取得したBINARYデータを基に、実データをオブジェクトに追加する。*/
			rv = AddIdPassDataByBuf(rcvBuf, pObject);
			/** −−失敗の場合、*/
			if (rv != CKR_OK)
			{
				/** −−−戻り値に不正なICカードを検出した（CKR_TOKEN_NOT_RECOGNIZED）を設定し処理を抜ける。*/
				return CKR_TOKEN_NOT_RECOGNIZED;
			}
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}
