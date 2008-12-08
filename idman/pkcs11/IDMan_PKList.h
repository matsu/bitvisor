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
 * \file IDMan_PKList.h
 */


/* 標準ライブラリ */


/* 外部ライブラリ */


/* 作成ライブラリ */


#ifndef _IDMAN_PKLIST_H_
#define _IDMAN_PKLIST_H_


CK_RV AddElem(CK_I_HEAD_PTR pList, CK_ULONG key, CK_VOID_PTR val, CK_ULONG valType);
CK_RV SetElem(CK_I_HEAD_PTR pList, CK_ULONG key, CK_VOID_PTR val, CK_ULONG valType);
CK_RV GetElem(CK_I_HEAD_PTR pList, CK_ULONG key, CK_VOID_PTR_PTR val);
CK_ULONG GetAttrValType(CK_ATTRIBUTE_TYPE attrType);
CK_RV DestroyAttribute(CK_ATTRIBUTE_PTR pAttr);
CK_RV DestroyTlvData(CK_I_TLV_DATA_PTR pTlvData);
CK_RV DestroyTokenData(CK_I_TOKEN_DATA_PTR pTokenData);
CK_RV DestroySlotData(CK_I_SLOT_DATA_PTR pSlotData);
CK_RV DestroySessionData(CK_I_SESSION_DATA_PTR pSessionData);
CK_RV DestroyList(CK_I_HEAD_PTR pList);
CK_RV DestroyElem(CK_I_ELEM_PTR pElem);
CK_RV DelElem(CK_I_HEAD_PTR pList, CK_ULONG key);



#endif
