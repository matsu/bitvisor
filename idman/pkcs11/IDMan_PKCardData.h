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
 * リスト処理ヘッダファイル
 * \file IDMan_PKCardData.h
 */


/* 標準ライブラリ */


/* 外部ライブラリ */


/* 作成ライブラリ */


#ifndef _IDMAN_PKCARDDATA_H_
#define _IDMAN_PKCARDDATA_H_


CK_RV GetUlByTxt(CK_BYTE_PTR pTxt, CK_LONG len, CK_ULONG_PTR pUlong);
CK_RV GetVerByTex(CK_BYTE_PTR pTxt, CK_ULONG len, CK_BYTE_PTR pMajor, CK_BYTE_PTR pMinor);
CK_RV GetUlByByte(CK_BYTE_PTR pByte, CK_ULONG len, CK_ULONG_PTR pUlong);
CK_ULONG TransHex2Tex(CK_BYTE_PTR HexData, CK_ULONG ulHexSize, CK_BYTE_PTR TxtData, CK_ULONG ulTxtSize);
CK_ULONG TransTex2Hex(CK_BYTE_PTR TxtData, CK_ULONG ulTxtSize, CK_BYTE_PTR HexData, CK_ULONG ulHexSize);
CK_RV GetLength(CK_CHAR_PTR iBuf, CK_ULONG_PTR len, CK_ULONG_PTR num);
CK_RV SetLength(CK_CHAR_PTR iBuf, CK_ULONG len, CK_ULONG_PTR num);
CK_RV GetTlvByBuf(CK_CHAR_PTR iBuf, CK_I_HEAD_PTR_PTR ppTlvDataList);
CK_RV GetDirDataByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_TOKEN_DATA_PTR tokenData);
CK_RV GetOdfDataByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_TOKEN_DATA_PTR tokenData);
CK_RV GetTokenDataByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_TOKEN_DATA_PTR tokenData);
CK_RV AddObjTblByTlv(CK_I_HEAD_PTR pTlvDataList, CK_I_HEAD_PTR objectTable, CK_ULONG_PTR phObjectCnt);
CK_RV AddCertDataByBuf(CK_BYTE_PTR iBuf, CK_I_HEAD_PTR pObject);
CK_RV AddIdPassDataByBuf(CK_BYTE_PTR iBuf, CK_I_HEAD_PTR pObject);
CK_RV GetValueData(CK_I_HEAD_PTR pObject, CK_ATTRIBUTE_TYPE attrType, CK_I_SLOT_DATA_PTR pSlotData);



#endif
