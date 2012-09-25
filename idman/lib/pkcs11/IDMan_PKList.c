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
 * リスト処理プログラム
 * \file IDMan_PKList.c
 */

/* 作成ライブラリ */
#include "IDMan_PKPkcs11.h"
#include "IDMan_PKPkcs11i.h"
#include "IDMan_PKList.h"
#include "IDMan_StandardIo.h"
#include <core/string.h>



/**
 * 要素を無条件で最後尾に追加関数
 * @author University of Tsukuba
 * @param pList リストのポインタ
 * @param key キー
 * @param val 要素
 * @param valType 要素タイプ
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV AddElem(CK_I_HEAD_PTR pList, CK_ULONG key, CK_VOID_PTR val, CK_ULONG valType)
{
	CK_I_ELEM_PTR pElem = CK_NULL_PTR; 
	CK_I_ELEM_PTR tmpElem = CK_NULL_PTR;
	
	
	/** リストが0x00の場合、*/
	if(pList == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}

	/** 要素の領域を確保する。*/
	pElem = (CK_I_ELEM_PTR)IDMan_StMalloc(sizeof(CK_I_ELEM));
	/** 失敗の場合、*/
	if(pElem == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pElem), 0, sizeof(CK_I_ELEM));

	/** 要素にデータを設定する。*/
	pElem->key = key;
	pElem->val = val;
	pElem->valType = valType;
	pElem->next = CK_NULL_PTR;
	
	/** リストテーブルに要素が存在しない場合、*/
	if( pList->num == 0)
	{
		/** −先頭に設定する。*/
		pList->elem = pElem;
		pList->num++;
	}
	/** 存在する場合、*/
	else
	{
		tmpElem = pList->elem;
		/** −無条件で処理を繰り返す。*/
		while(1)
		{
			/** −−次要素がない場合、*/
			if(tmpElem->next == CK_NULL_PTR)
			{
				/** −−−次要素ポインタに要素ポインタを設定する。*/
				tmpElem->next = pElem;
				/** −−−要素数をインクリメントする。*/
				pList->num++;
				/** −−−繰り返し処理を終了する。*/
				break;
			}
			/** −−上記以外の場合、*/
			else
			{
				/** −−−次要素ポインタに移動する。*/
				tmpElem = tmpElem->next;
			}
		}
    }
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 要素追加関数
 * @author University of Tsukuba
 * @param pList リストのポインタ
 * @param key キー
 * @param val 要素
 * @param valType 要素タイプ
 * @return CK_RV CKR_OK:成功 CKR_HOST_MEMORY:ホストメモリ不足 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV SetElem(CK_I_HEAD_PTR pList, CK_ULONG key, CK_VOID_PTR val, CK_ULONG valType)
{
	CK_I_ELEM_PTR pElem = CK_NULL_PTR; 
	CK_I_ELEM_PTR tmpElem = CK_NULL_PTR;
	
	
	/** リストが0x00の場合、*/
	if(pList == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}

	/** 要素の領域を確保する。*/
	pElem = (CK_I_ELEM_PTR)IDMan_StMalloc(sizeof(CK_I_ELEM));
	/** 失敗の場合、*/
	if(pElem == CK_NULL_PTR)
	{
		/** −戻り値にホストメモリ不足（CKR_HOST_MEMORY）を設定し処理を抜ける。*/
		return CKR_HOST_MEMORY;
	}
	memset((void*)(pElem), 0, sizeof(CK_I_ELEM));

	/** 要素にデータを設定する。*/
	pElem->key = key;
	pElem->val = val;
	pElem->valType = valType;
	pElem->next = CK_NULL_PTR;
	
	/** リストテーブルに要素が存在しない場合、*/
	if( pList->num == 0)
	{
		/** −先頭に設定する。*/
		pList->elem = pElem;
		pList->num++;
	}
	/** 存在する場合、*/
	else
	{
		tmpElem = pList->elem;
		/** −無条件で処理を繰り返す。*/
		while(1)
		{
			/** −−キーが同一の場合、*/
			if(key == tmpElem->key)
			{
				/** −−−古い領域を開放する。*/
				IDMan_StFree(tmpElem->val);
				/** −−−値を設定する。*/
				tmpElem->val = pElem->val;
				/** −−−確保した要素を開放する。*/
				IDMan_StFree(pElem);
				/** −−−繰り返し処理を終了する。*/
				break;
			}
			/** −−次要素がない場合、*/
			else if(tmpElem->next == CK_NULL_PTR)
			{
				/** −−−次要素ポインタに要素ポインタを設定する。*/
				tmpElem->next = pElem;
				/** −−−要素数をインクリメントする。*/
				pList->num++;
				/** −−−繰り返し処理を終了する。*/
				break;
			}
			/** −−上記以外の場合、*/
			else
			{
				/** −−−次要素ポインタに移動する。*/
				tmpElem = tmpElem->next;
			}
		}
    }
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 要素取得関数
 * @author University of Tsukuba
 * @param pList リストのポインタ
 * @param key キー
 * @param val 要素
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV GetElem(CK_I_HEAD_PTR pList, CK_ULONG key, CK_VOID_PTR_PTR val)
{
	CK_I_ELEM_PTR tmpElem = CK_NULL_PTR;
	
	
	/** リストが0x00の場合、*/
	if(pList == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 要素が存在しない場合、*/
	if(pList->elem == CK_NULL_PTR)
	{
		/** −戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
		return CKR_ARGUMENTS_BAD;
	}
	/** 上記以外の場合、*/
	else
	{
		tmpElem = pList->elem;
		/** −処理を繰り返す。*/
		while(1)
		{
			/** −−キーが同一の場合、*/
			if(key == tmpElem->key)
			{
				/** −−−値を設定し、繰り返し処理を終了する。*/
				*val = tmpElem->val;
				break;
			}
			/** −−次要素がない場合、*/
			else if(tmpElem->next == CK_NULL_PTR)
			{
				/** −−−戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
				return CKR_ARGUMENTS_BAD;
			}
			/** −−上記以外の場合、次要素を設定し、処理を繰り返す。*/
			else
			{
				tmpElem = tmpElem->next;
			}
		}
	}
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 属性タイプ値取得関数
 * @author University of Tsukuba
 * @param attrType 属性タイプ
 * @return CK_ULONG ATTR_VAL_TYPE_NUM:数字 ATTR_VAL_TYPE_BYTE:1バイト ATTR_VAL_TYPE_BYTE_ARRAY:バイト列 ATTR_VAL_TYPE_INVALID:不正
 * @since 2008.02
 * @version 1.0
 */
CK_ULONG GetAttrValType(CK_ATTRIBUTE_TYPE attrType)
{
	CK_ULONG rvAttr;
	
	
	/** 属性タイプに応じて属性タイプ値を設定する。*/
	switch (attrType)
	{
	/** 属性タイプが数字の場合、*/
	case CKA_CLASS:
	case CKA_KEY_TYPE:
	case CKA_KEY_GEN_MECHANISM:
	case CKA_CERTIFICATE_TYPE:
	case CKA_CERTIFICATE_CATEGORY:
	case CKA_JAVA_MIDP_SECURITY_DOMAIN:
	case CKA_INDEX:
		/** −数字を設定する。*/
		rvAttr = ATTR_VAL_TYPE_NUM;
		break;
	/** 属性タイプが1バイトの場合、*/
	case CKA_TOKEN:
	case CKA_PRIVATE:
	case CKA_TRUSTED:
	case CKA_DERIVE:
	case CKA_LOCAL:
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
		/** −1バイトを設定する。*/
		rvAttr = ATTR_VAL_TYPE_BYTE;
		break;
	/** 属性タイプがバイト配列の場合、*/
	case CKA_LABEL:
	case CKA_VALUE:
	case CKA_START_DATE:
	case CKA_END_DATE:
	case CKA_APPLICATION:
	case CKA_OBJECT_ID:
	case CKA_CHECK_VALUE:
	case CKA_ID:
	case CKA_ISSUER:
	case CKA_SERIAL_NUMBER:
	case CKA_URL:
	case CKA_HASH_OF_SUBJECT_PUBLIC_KEY:
	case CKA_HASH_OF_ISSUER_PUBLIC_KEY:
	case CKA_SUBJECT:
		/** −バイト配列を設定する。*/
		rvAttr = ATTR_VAL_TYPE_BYTE_ARRAY;
		break;
	/** 上記以外の場合、*/
	default:
		/** −不正を設定する。*/
		rvAttr = ATTR_VAL_TYPE_INVALID;
		break;
	}
	
	/** 戻り値に属性タイプ値を設定し処理を抜ける。*/
	return rvAttr;
}



/**
 * CK_ATTRIBUTE開放関数
 * @author University of Tsukuba
 * @param pAttr CK_ATTRIBUTEのポインタ
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV DestroyAttribute(CK_ATTRIBUTE_PTR pAttr)
{
	CK_ULONG rvAttr;
	
	/** CK_ATTRIBUTEが0x00の場合、*/
	if (pAttr == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 属性の値により、メンバを解放する。*/
	rvAttr = GetAttrValType(pAttr->type);
	switch (rvAttr)
	{
	/** 以下はメンバを解放する。*/
	case ATTR_VAL_TYPE_BYTE:
	case ATTR_VAL_TYPE_BYTE_ARRAY:
		if(pAttr->pValue)
		{
			IDMan_StFree(pAttr->pValue);
		}
		break;
	/** 上記以外は処理をしない。*/
	default:
		break;
	}
	
	/** 本体を解放する。*/
	IDMan_StFree(pAttr);
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * CK_I_TLV_DATA開放関数
 * @author University of Tsukuba
 * @param pTlvData CK_I_TLV_DATAのポインタ
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV DestroyTlvData(CK_I_TLV_DATA_PTR pTlvData)
{
	
	/** 本体を解放する。*/
	if(pTlvData != CK_NULL_PTR)
	{
		IDMan_StFree(pTlvData);
	}
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * CK_I_TOKEN_DATA開放関数
 * @author University of Tsukuba
 * @param pTokenData CK_I_TOKEN_DATAのポインタ
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV DestroyTokenData(CK_I_TOKEN_DATA_PTR pTokenData)
{
	
	/** CK_I_TOKEN_DATAが0x00でない場合、*/
	if (pTokenData != CK_NULL_PTR)
	{
		/** −メンバを解放する。*/
		if(pTokenData->dirData != CK_NULL_PTR)
		{
			IDMan_StFree(pTokenData->dirData);
		}
		if(pTokenData->odfData != CK_NULL_PTR)
		{
			IDMan_StFree(pTokenData->odfData);
		}
		if(pTokenData->tokenInfo != CK_NULL_PTR)
		{
			IDMan_StFree(pTokenData->tokenInfo);
		}
		DestroyList(pTokenData->mechanismTable);
		DestroyList(pTokenData->objectTable);
		
		/** −本体を解放する。*/
		IDMan_StFree(pTokenData);
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * CK_I_SLOT_DATA開放関数
 * @author University of Tsukuba
 * @param pSlotData CK_I_SLOT_DATAのポインタ
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV DestroySlotData(CK_I_SLOT_DATA_PTR pSlotData)
{

	/** CK_I_SLOT_DATAが0x00でない場合、*/
	if(pSlotData != CK_NULL_PTR)
	{
		/** −メンバを解放する。*/
		if(pSlotData->slotInfo != CK_NULL_PTR)
		{
			IDMan_StFree(pSlotData->slotInfo);
		}
		/** −本体を解放する。*/
		DestroyTokenData(pSlotData->tokenData);
		IDMan_StFree(pSlotData);
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * CK_I_SESSION_DATA開放関数
 * @author University of Tsukuba
 * @param pSessionData CK_I_SESSION_DATAのポインタ
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV DestroySessionData(CK_I_SESSION_DATA_PTR pSessionData)
{


	/** CK_I_SESSION_DATAが0x00でない場合、*/
	if(pSessionData != CK_NULL_PTR)
	{
		/** −メンバを解放する。*/
		if(pSessionData->sessionInfo != CK_NULL_PTR)
		{
			IDMan_StFree(pSessionData->sessionInfo);
		}
		DestroyList(pSessionData->srchObject);
		/** −本体を解放する。*/
		IDMan_StFree(pSessionData);
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * リスト開放関数
 * @author University of Tsukuba
 * @param pList リストのポインタ
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV DestroyList(CK_I_HEAD_PTR pList)
{
	/*CK_ULONG i;*/
	CK_I_ELEM_PTR next = CK_NULL_PTR;
	CK_I_ELEM_PTR tmp = CK_NULL_PTR;
	
	
	/** リストが0x00の場合、*/
	if (pList == CK_NULL_PTR)
	{
		/** −戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
		return CKR_OK;
	}
	
	tmp = pList->elem;
	/** 要素の数分繰り返す。*/
	while (tmp != CK_NULL_PTR)
	{
		next = tmp->next;
		/** −要素を解放する。*/
		DestroyElem(tmp);
		tmp = next;
	}
	
	/** リストヘッダを解放する。*/
	IDMan_StFree(pList);
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 要素開放関数
 * @author University of Tsukuba
 * @param pElem CK_I_ELEM_PTRのポインタ
 * @return CK_RV CKR_OK:成功
 * @since 2008.02
 * @version 1.0
 */
CK_RV DestroyElem(CK_I_ELEM_PTR pElem)
{
	/** 要素valのタイプをチェックする。*/
	switch (pElem->valType)
	{
	/** 要素valがリストの場合、*/
	case VAL_TYPE_HEAD:
		/** −リスト要素val解放処理をする。*/
		DestroyList((CK_I_HEAD_PTR)pElem->val);
		break;
	/** 要素valがCK_I_SESSION_DATAの場合、*/
	case VAL_TYPE_SESSION_DATA:
		/** −CK_I_SESSION_DATA要素val解放処理をする。*/
		DestroySessionData((CK_I_SESSION_DATA_PTR)pElem->val);
		break;
	/** 要素valがCK_I_SLOT_DATAの場合、*/
	case VAL_TYPE_SLOT_DATA:
		/** −CK_I_SLOT_DATA要素val解放処理をする。*/
		DestroySlotData((CK_I_SLOT_DATA_PTR)pElem->val);
		break;
	/** 要素valがCK_I_TOKEN_DATAの場合、*/
	case VAL_TYPE_TOKEN_DATA:
		/** −CK_I_TOKEN_DATA要素val解放処理をする。*/
		DestroyTokenData((CK_I_TOKEN_DATA_PTR)pElem->val);
		break;
	/** 要素valがCK_ATTRIBUTEの場合、*/
	case VAL_TYPE_ATTRIBUTE:
		/** −CK_ATTRIBUTE要素val解放処理をする。*/
		DestroyAttribute((CK_ATTRIBUTE_PTR)pElem->val);
		break;
	/** 要素valがCK_I_TLV_DATAの場合、*/
	case VAL_TYPE_TLV_DATA:
		/** −CK_I_TLV_DATA要素val解放処理をする。*/
		DestroyTlvData((CK_I_TLV_DATA_PTR)pElem->val);
		break;
	/** 上記以外の場合、*/
	default:
		/** −その他要素val解放処理をする。*/
		if(pElem->val != CK_NULL_PTR)
		{
			IDMan_StFree(pElem->val);
		}
		break;
	}
	
	/** 要素本体解放処理をする。*/
	IDMan_StFree(pElem);
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}



/**
 * 要素削除関数
 * @author University of Tsukuba
 * @param pList リストのポインタ
 * @param key キー
 * @return CK_RV CKR_OK:成功 CKR_ARGUMENTS_BAD:引数不正 CKR_GENERAL_ERROR:一般エラー
 * @since 2008.02
 * @version 1.0
 */
CK_RV DelElem(CK_I_HEAD_PTR pList, CK_ULONG key)
{
	CK_I_ELEM_PTR currElem = CK_NULL_PTR;
	CK_I_ELEM_PTR lastElem = CK_NULL_PTR;
	
	
	/** リストが0x00の場合、*/
	if (pList == CK_NULL_PTR)
	{
		/** −戻り値に一般エラー（CKR_GENERAL_ERROR）を設定し処理を抜ける。*/
		return CKR_GENERAL_ERROR;
	}
	
	/** 要素が存在しない場合、*/
	if (pList->elem == CK_NULL_PTR)
	{
		/** −戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
		return CKR_ARGUMENTS_BAD;
	}
	/** 上記以外の場合、*/
	else
	{
		currElem = pList->elem;
		lastElem = CK_NULL_PTR;
		
		/** −処理を繰り返す。*/
		while(1)
		{
			/** −−該当要素の場合、*/
			if (key == currElem->key)
			{
				/** −−−該当要素をリストから外す。*/
				if (lastElem == CK_NULL_PTR)
				{
					pList->elem = currElem->next;
				}
				else
				{
					lastElem->next = currElem->next;
				}
				
				/** −−−要素を解放する。*/
				DestroyElem(currElem);
				
				/** −−−要素数をデクリメントする。*/
				pList->num--;
				break;
			}
			/** −−次要素がない場合、*/
			else if (currElem->next == CK_NULL_PTR)
			{
				/** −−−戻り値に引数不正（CKR_ARGUMENTS_BAD）を設定し処理を抜ける。*/
				return CKR_ARGUMENTS_BAD;
				break;
			}
			/** −−上記以外の場合、次要素に移動する。*/
			else
			{
				lastElem = currElem;
				currElem = currElem->next;
			}
		}
	}
	
	/** 戻り値に成功（CKR_OK）を設定し処理を抜ける。*/
	return CKR_OK;
}
