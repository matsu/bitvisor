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
 * ID管理の標準共通機能プログラム
 * \file IDMan_StandardIo.c
 */

#include <core/types.h>
#include <common/list.h>
#include <usb.h>
#ifdef NTTCOM
#include <core/mm.h>

#define CHELP_OPENSSL_SOURCE	
#include <chelp.h>
#undef OPENSSL_NO_ENGINE
#endif

/* 外部ライブラリ関数名 */
#include <openssl/rsa.h>
#include <openssl/err.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <openssl/pem.h>
#ifndef NTTCOM
#include <string.h>
#endif


/* 作成ライブラリ関数名 */
#include "IDMan_StandardIo.h"

#define MAX_MS 0xFFFFFFFF	//SysGetTickCountのMAX値
#define PRECISION 0x0A		//SysGetTickCountの精度

#ifdef DEBUG
/**
* デバッグ情報出力関数
* @author University of Tsukuba
* @param *pParam	情報
* @return void
* @since 2008.02
* @version 1.0
*/
void IDMan_CmDebugOutPut ( char *pParam)
{

	printf(pParam);
}
#endif

/**
*メモリ取得関数
* @author University of Tsukuba
* @param siSize	領域のバイト数
* @return	void*	領域の割付が成功したら領域への先頭ポインタを返す領域の割付が失敗したら0x00を返す
* @since 2008.02
* @version 1.0
*/
void *IDMan_StMalloc (unsigned long int siSize)
{
	void			*pMem;


	/** 確保メモリサイズが０以下の場合、 */
	if ( siSize <= 0 )
	{
		/** −エラーログを出力する。 */
		pMem = 0x00;
	}
	else
	{
		/** メモリの確保を行う。 */
#ifndef NTTCOM
		pMem = (void *)malloc(siSize);
#else
		pMem = (void *)alloc(siSize);
#endif
		if(pMem != 0x00)
		{
			IDMan_StMemset(pMem, 0, siSize);
		}
	}
	return pMem;
}

/**
*メモリ開放関数
* @author University of Tsukuba
* @param *ptr	解放する領域の先頭ポインタ
* @return	void なし
* @since 2008.02
* @version 1.0
*/
void IDMan_StFree (void *ptr)
{


	/** 開放先アドレスが0x00以外の場合、 */
	if( ptr != 0x00)
	{
		/** −メモリの開放を行う。 */
		free(ptr);
		ptr = 0x00;
	}

	return;
}

#if 0
/**
* ファイルシステム読込関数
* @author University of Tsukuba
* @return int 0:正常 -1:異常
* @since 2008.02
* @version 1.0
*/
int IDMan_StReadSetData(char *pMemberName,char *pInfo,unsigned long int* len)
{

	struct config_value * ConfigData;

#ifndef NTTCOM
	/** ファイルシステム読込を行う。 */
	ConfigData = config_get_value(pMemberName);
	if(ConfigData == 0x00)
	{
		return -1;
	}
	memcpy(pInfo,ConfigData->value,ConfigData->size);
	(*len) = ConfigData->size;
	IDMan_StFree(ConfigData->value);
	IDMan_StFree(ConfigData);
#endif
	return 0;
}
#endif

#if 0
/**
* USBの初期化処理（usb_initのパススルー関数）
* @author University of Tsukuba
* @return int usb_init()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
void IDMan_StUsbInit(void)
{
	usb_init();
	return;
}

/**
* システム全てのバス検索処理（usb_find_bussesのパススルー関数）
* @author University of Tsukuba
* @return int usb_find_busses()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbFindBusses(void)
{
	return usb_find_busses();
}

/**
* システム全てのデバイス検索処理（usb_find_devicesのパススルー関数）
* @author University of Tsukuba
* @return int usb_find_devices()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbFindDevices(void)
{
	return usb_find_devices();
}

/**
* バスの取得処理（usb_get_bussesのパススルー関数）
* @author University of Tsukuba
* @return int usb_get_busses()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
struct ID_USB_BUS *IDMan_StUsbGetBusses(void)
{
	return (struct ID_USB_BUS *)usb_get_busses();
}

/**
* USBオープン処理（usb_openのパススルー関数）
* @author University of Tsukuba
* @param usb_open()と同様
* @return ID_USB_DEV_HANDLE usb_open()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
struct ID_USB_DEV_HANDLE *IDMan_StUsbOpen(struct ID_USB_DEVICE *dev)
{
	return (struct ID_USB_DEV_HANDLE*)usb_open((struct usb_device*) dev);
}

/**
* インタフェース情報取得処理（usb_claim_interfaceのパススルー関数）
* @author University of Tsukuba
* @param usb_claim_interface()と同様
* @return int usb_claim_interface()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbClaimInterface(struct ID_USB_DEV_HANDLE *dev, int interface)
{
	return usb_claim_interface((struct usb_dev_handle *)dev, interface);
}

/**
* USBデバイスデータ書き込み処理（usb_bulk_writeのパススルー関数）
* @author University of Tsukuba
* @param usb_bulk_write()と同様
* @return int usb_bulk_write()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbBulkWrite(struct ID_USB_DEV_HANDLE *dev, int ep, char *bytes, int size, int timeout)
{
	return usb_bulk_write((struct usb_dev_handle*) dev, ep, bytes, size, timeout);
}

/**
* USBデバイスデータ読み込み処理（usb_bulk_readのパススルー関数）
* @author University of Tsukuba
* @param usb_bulk_read()と同様
* @return int usb_bulk_read()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbBulkRead(struct ID_USB_DEV_HANDLE *dev, int ep, char *bytes, int size, int timeout)
{
	return usb_bulk_read((struct usb_dev_handle *)dev, ep, bytes, size, timeout);
}

/**
* インデックス指定説明情報取得処理（usb_get_string_simpleのパススルー関数）
* @author University of Tsukuba
* @param usb_get_string_simple()と同様
* @return int usb_get_string_simple()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbGetStringSimple(struct ID_USB_DEV_HANDLE *dev, int index, char *buf,ID_SIZE_T buflen)
{
	return usb_get_string_simple((struct usb_dev_handle*) dev,  index,  buf, buflen);
}

/**
* インターフェース情報の解放処理（usb_release_interfaceのパススルー関数）
* @author University of Tsukuba
* @param usb_release_interface()と同様
* @return int usb_release_interface()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbReleaseInterface(struct ID_USB_DEV_HANDLE *dev, int interface)
{
	return usb_release_interface((struct usb_dev_handle*) dev, interface);
}

/**
* USBリセット処理（usb_resetのパススルー関数）
* @author University of Tsukuba
* @param usb_reset()と同様
* @return int usb_reset()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbReset(struct ID_USB_DEV_HANDLE *dev)
{
	return usb_reset((struct usb_dev_handle *)dev);
}


/**
* USBクローズ処理（usb_closeのパススルー関数）
* @author University of Tsukuba
* @param usb_close()と同様
* @return int usb_close()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbClose(struct ID_USB_DEV_HANDLE *dev)
{
	return usb_close((struct usb_dev_handle*)dev);
}

/**
* コントロールメッセージ送信処理（usb_control_msgのパススルー関数）
* @author University of Tsukuba
* @param usb_control_msg()と同様
* @return int usb_control_msg()の戻り値と同様
* @since 2008.02
* @version 1.0
*/
int IDMan_StUsbControlMsg(struct ID_USB_DEV_HANDLE *dev, int requesttype, int request,
						  int value, int index, char *bytes, int size, int timeout)
{
	return usb_control_msg((struct usb_dev_handle*)dev,requesttype,  request,
							  value, index, bytes,  size,  timeout);
}
#endif

/**
* メモリコピー処理
* @author University of Tsukuba
* @param dstpp コピー先アドレス
* @param srcpp コピー元アドレス
* @param len コピーバイト数
* @return void * コピー先アドレス
* @since 2008.02
* @version 1.0
*/
void * IDMan_StMemcpy(void *dstpp, const void *srcpp, ID_SIZE_T len)
{
	char		*dstpp_sub;
	const char	*srcpp_sub;

	dstpp_sub = (char *)dstpp;
	srcpp_sub = (char *)srcpp;
	while(len > 0)
	{
		*dstpp_sub = *srcpp_sub;
		dstpp_sub++;
		srcpp_sub++;
		len--;
	}
	return dstpp;
}
/**
* メモリセット処理
* @author University of Tsukuba
* @param dstpp 設定先アドレス
* @param c 設定する値
* @param len 設定バイト数
* @return void * 設定先アドレス
* @since 2008.02
* @version 1.0
*/
void * IDMan_StMemset(void *dstpp,int c , unsigned long int len)
{
	char		*dstpp_sub;

	dstpp_sub = (char *)dstpp;
	while(len > 0)
	{
		*dstpp_sub = c;
		dstpp_sub++;
		len--;
	}
	return dstpp;
}

/**
* メモリ比較処理
* @author University of Tsukuba
* @param s1 比較元アドレス1
* @param s2 比較元アドレス2
* @param len 比較バイト数
* @return int 正数:比較元アドレス1 > 比較元アドレス2 0:比較元アドレス1 = 比較元アドレス2 負数:比較元アドレス1 < 比較元アドレス2 
* @since 2008.02
* @version 1.0
*/
int IDMan_StMemcmp(const void *s1, const void *s2, unsigned long int len)
{
	unsigned char	*s1_sub;
	unsigned char	*s2_sub;
	int				ret;

	if(len == 0)
	{
		return 0;
	}

	s1_sub = (unsigned char *)s1;
	s2_sub = (unsigned char *)s2;

	while(*s1_sub == *s2_sub)
	{
		len--;
		if( len == 0 )
		{
			return 0;
		}
		s1_sub++;
		s2_sub++;
	}
	ret =  *s1_sub-*s2_sub;
	return ret;
}

/**
* 文字列長取得処理
* @author University of Tsukuba
* @param str 文字列アドレス
* @return unsigned long int 文字列長
* @since 2008.02
* @version 1.0
*/
unsigned long int IDMan_StStrlen(const char *str)
{
	unsigned long int ret;

	for(ret = 0;str[ret];ret++);
	return ret;
}

/**
* 文字列連結処理
* @author University of Tsukuba
* @param dest 連結先の文字列アドレス
* @param src 連結文字列アドレス
* @return char * 連結先の文字アドレス(連結後)
* @since 2008.02
* @version 1.0
*/
char * IDMan_StStrcat(char * dest, const char * src)
{
	char *dest_sub;
	unsigned long int len;

	for(len = 0;dest[len];len++);

	dest_sub = dest + len;
	while(*src)
	{
		*dest_sub = *src;
		dest_sub++;
		src++;
	}
	*dest_sub='\0';

	return dest;
}

/**
* 文字列検索処理
* @author University of Tsukuba
* @param s 検索対象文字列アドレス
* @param c_in 検索文字
* @return char * 発見時：一致文字列の先頭アドレス 存在しない場合:0x00
* @since 2008.02
* @version 1.0
*/
char * IDMan_StStrchr(const char *s, int c_in)
{
	c_in = (char)c_in;
	while (*s != c_in) {
		if (*s == '\0')
		{
			return (0x00);
		}
		s++;
	}
	return ((char *)s);
}

/**
* 文字列検索処理
* @author University of Tsukuba
* @param s 検索対象文字列アドレス
* @param c_in 検索文字
* @return char * 発見時：一致文字列の先頭アドレス 存在しない場合:0x00
* @since 2008.02
* @version 1.0
*/
char * IDMan_StStrcpy(char * dest, const char * src)
{
	char * dest_sub;

	dest_sub = dest;
	while(*src)
	{
		*dest_sub = *src;
		dest_sub++;
		src++;
	}
	*dest_sub = '\0';

	return dest;
}

/**
* 文字列の比較処理
* @author University of Tsukuba
* @param p1 比較文字列アドレス1
* @param p2 比較文字列アドレス2
* @return int 正数:比較文字列アドレス1 > 比較文字列アドレス2 0:比較文字列アドレス1 = 比較文字列アドレス2 負数:比較文字列アドレス1 < 比較文字列アドレス2
* @since 2008.02
* @version 1.0
*/
int IDMan_StStrcmp(const char *p1, const char *p2)
{
	while(*p1 == *p2)
	{
		if(*p1 == '\0')
		{
			return 0;
		}
		p1++;
		p2++;
	}
	return (unsigned char)*p1-(unsigned char)*p2;
}

/**
* 文字スペースチェック処理（サブ関数）
* @author University of Tsukuba
* @param str 文字列
* @return int 0:スペース文字でない場合 !0:スペース文字
* @since 2008.02
* @version 1.0
*/
int StIsspace(unsigned char str)
{
	switch(str)
	{
		case ' ':
		case '\f':
		case '\n':
		case '\r':
		case '\t':
		case '\v':
		return !0;
	}
	return 0;
}

/**
* 文字列の数値と符号チェック処理（サブ関数）
* @author University of Tsukuba
* @param str 文字列
* @param flag 符号フラグ( 負数の場合1を返す)
* @return char * 文字列の数値位置アドレス
* @since 2008.02
* @version 1.0
*/
static char * StSpace_sign(const char * s,int *flag)
{
	while(StIsspace(*s))
	{
		s++;
	}
	switch(*s)
	{
		case '-':*flag=1;
		case '+':s++;
	}
	return (char*)s;
}

/**
* 文字の数字チェック処理（サブ関数）
* @author University of Tsukuba
* @param str 文字
* @return int 0:数字以外 1:数字
* @since 2008.02
* @version 1.0
*/
int StIsdigit(int str)
{
	return str>='0' && str<='9';
}

/**
* 文字列整数型（int）変換処理
* @author University of Tsukuba
* @param nptr 変換対象文字列
* @return int 変換後整数
* @since 2008.02
* @version 1.0
*/
int IDMan_StAtoi(const char *nptr)
{
	
	int flag=0;
	int ret=0;

	nptr = StSpace_sign(nptr,&flag);
	while(StIsdigit(*nptr))
	{
		ret= 10 * ret + (*nptr-'0');
		nptr++;
	}
	if(flag)
	{
		return -ret;
	}
	else
	{
		return ret;
	}
}


/**
* 文字列整数型（long）変換処理
* @author University of Tsukuba
* @param nptr 変換対象文字列
* @return long 変換後整数
* @since 2008.02
* @version 1.0
*/
long int IDMan_StAtol(const char *nptr)
{
	
	int flag=0;
	long int ret=0;

	nptr = StSpace_sign(nptr,&flag);
	while(StIsdigit(*nptr))
	{
		ret= 10 * ret + (*nptr-'0');
		nptr++;
	}
	if(flag)
	{
		return -ret;
	}
	else
	{
		return ret;
	}
}

#ifdef ZZZ
/**
* マイクロ秒sleep標準共通関数
* @author University of Tsukuba
* @param time_ms sleepする時間（マイクロ秒）
* @return void なし
* @since 2008.02
* @version 1.0
*/
void IDMan_StMsleep(unsigned long int time_ms)
{

	unsigned long int  start_ms;	//スタート時間
	unsigned long int   now_ms;		//現在時間
	unsigned long int  limit_ms;	//限界値
	unsigned long int  sb_ms;		//作業領域
	unsigned long int  aim_ms;		//目標時間
	int flg;						//限界フラグ

	/**sleepする時間、MAX値、精度を基に限界値を求める。*/
	limit_ms = MAX_MS - time_ms - PRECISION;

	/**VM起動してからスタート時間を取得する。*/
	start_ms = clock_gettick();

	/**スタート時間をチェックする。 */
	/**スタート時間が限界値を超える場合、 */
	if(limit_ms < start_ms)
	{
		/**−sleepする時間、スタート時間、MAX値を基に目標時間を取得する。*/
		sb_ms = MAX_MS - start_ms;
		aim_ms = time_ms - sb_ms;
		flg = 1;
	}
	/**スタート時間が限界値を超えない場合、 */
	else
	{
		/**−sleepする時間とスタート時間を基に目標時間を取得する。*/
		aim_ms = start_ms + time_ms;
		flg = 0;
	}
	/**sleepする時間だけ処理を繰り返す。 */
	while (1)
	{
		/**−現在時間を取得する。*/
		now_ms = clock_gettick();
		/**−スタート時間が限界値を超えていたかチェックする。*/
		/**−スタート時間が限界値を超えていた場合、*/
		if(flg)
		{
			/**−−現在時間をチェックする。*/
			/**−−現在時間がスタート時間より小さく目標時間を超えていた場合、*/
			if(start_ms > now_ms && aim_ms <= now_ms)
			{
				/**−−−繰り返し処理を終了する。*/
				break;
			}
		}
		/**−スタート時間が限界値を超えていなかった場合、*/
		else
		{
			/**−−現在時間をチェックする。*/
			/**−−現在時間が目標時間を超えていた場合、*/
			if( now_ms >= aim_ms)
			{
				/**−−−繰り返し処理を終了する。*/
				break;
			}
		}
	}
	/**リターンする。 */
	return;
}
#endif




/**
*ハッシュ値生成関数
* @author University of Tsukuba
* @param *ptr ハッシュ対象データ
* @param lLenIn ハッシュデータ長
* @param lalgorithm ハッシュアルゴリズム
* @param *ptrData  	ハッシュ値
* @param *lLenOut	ハッシュ値データ長
* @return	long	0：正常 -1：異常
* @since 2008.02
* @version 1.0
*/
long IDMan_CmMkHash(void *ptr, long lLenIn, long lalgorithm, void *ptrData, long *lLenOut)
{
	long 			lRet = -1L;
	SHA_CTX 		ctx;
	SHA256_CTX 		ctx2;
	SHA512_CTX 		ctx3;
	char 			szErrBuff[1024];

	ERR_load_crypto_strings();

	IDMan_StMemset(szErrBuff, 0x00, sizeof(szErrBuff));
	/** ハッシュ対象データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( ptr == 0x00 )
	{ 
		/** −処理を中断する。 */
		return lRet;
	}
	/** ハッシュデータ長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( lLenIn == 0 ){
		/** −処理を中断する。 */
		return lRet;
	}
	/** ハッシュ値パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( ptrData == 0x00 )
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** ハッシュ値データ長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( lLenOut == 0x00 ) 
	{
		/** −処理を中断する。 */
		return lRet;
	}
	else
	{
		/** −戻り値の初期化を行う。 */
		*lLenOut=0;
	}

	/** ハッシュ化を行う。 */
	switch(lalgorithm)
	{
		case 0:
			break;
		/** アルゴリズムが0x220の場合、 */
		case ALGORITHM_SHA1:
			/** −ハッシュ変換　SHA1を行う。 */
			SHA1_Init (&ctx);
			SHA1_Update (&ctx, ptr, lLenIn);
			SHA1_Final (ptrData, &ctx);
			OPENSSL_cleanse (&ctx, sizeof (ctx));
			/** −ハッシュ変換が成功の場合、 */
			if ( ptrData != 0x00)
			{
				/** −−長さを設定する。 */
				*lLenOut = HASH_LEN_SHA1;
				lRet =0;
			}
			/** −ハッシュ変換が失敗の場合、 */
			else
			{
				/** −−エラーログを出力する。 */
				ERR_error_string(ERR_get_error(), szErrBuff);
			}
			break;
		/** アルゴリズムが0x250の場合、 */
		case ALGORITHM_SHA256:
			/** −ハッシュ変換　SHA256を行う。 */
			SHA256_Init (&ctx2);
			SHA256_Update (&ctx2, ptr, lLenIn);
			SHA256_Final (ptrData, &ctx2);
			OPENSSL_cleanse (&ctx2, sizeof (ctx2));
			/** −ハッシュ変換が成功の場合、 */
			if ( ptrData != 0x00)
			{
				/** −−長さを設定する。 */
				*lLenOut = HASH_LEN_SHA256;
				lRet =0;
			}
			/** −ハッシュ変換が失敗の場合、 */
			else
			{
				/** −−エラーログを出力する。 */
				ERR_error_string(ERR_get_error(), szErrBuff);
			}
			break;
		/** アルゴリズムが0x270の場合、 */
		case ALGORITHM_SHA512:
			/** −ハッシュ変換　SHA512を行う。 */
			SHA512_Init (&ctx3);
			SHA512_Update (&ctx3, ptr, lLenIn);
			SHA512_Final (ptrData, &ctx3);
			OPENSSL_cleanse (&ctx3, sizeof (ctx3));
			/** −ハッシュ変換が成功の場合、 */
			if ( ptrData != 0x00)
			{
				/** −−長さを設定する。 */
				*lLenOut = HASH_LEN_SHA512;
				lRet =0;
			}
			/** −ハッシュ変換が失敗の場合、 */
			else
			{
				/** −−エラーログを出力する。 */
				ERR_error_string(ERR_get_error(), szErrBuff);
			}
			break;
		default:
			break;
	}
	return lRet;
}



/**
*RSA復号関数
* @author University of Tsukuba
* @param *pKey 鍵データ
* @param lKeyLen 鍵データ長
* @param *pData 対象データ
* @param lDataLen 対象データ長
* @param *pChgData 変換データ
* @param *lChgDataLen 変換データ長
* @return	long	0：正常 -1：異常
* @since 2008.02
* @version 1.0
*/
long IDMan_CmChgRSA( void *pKey, long lKeyLen, void *pData, long lDataLen, void *pChgData, long *lChgDataLen)
{
	long 			lRet = -1L;
	RSA 			*rsa;
	RSA 			*rsa2;
	long 			generate_key=0;
	unsigned char 	*pInData;
	unsigned char 	*pOutData;
	int 			iRetLen=0;
	char 			szErrBuff[1024];

	ERR_load_crypto_strings();

	IDMan_StMemset(szErrBuff, 0x00, sizeof(szErrBuff));

	/** 鍵データパラメータチェックを行う。 */
	/** −エラー発生した場合、 */
	if ( pKey == 0x00 )
	{
		/** −処理を中断する。 */
	 	return lRet;
	}
	/** 鍵データ長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( lKeyLen == 0 )
	{ 
		/** −処理を中断する。 */
		return lRet;
	}
	/** 対象データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( pData == 0x00 )
	{ 
		/** −処理を中断する。 */
		return lRet;
	}
	/** 対象データ長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( lDataLen == 0 ) 
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 変換データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( pChgData == 0x00 ) 
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 変換データ長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if ( lChgDataLen  == 0x00 ) 
	{
		/** −処理を中断する。 */
		return lRet;	
	}
	pInData = (unsigned char *)pData;
	pOutData = (unsigned char *)pChgData;

	/**アルゴリズムRSAF4を設定*/
	generate_key=ALGORITHM_RSAF4;
	/** 鍵作成を行う。 */
	rsa2 = RSA_generate_key(lKeyLen, generate_key, 0x00, 0x00);
	/** 鍵作成に失敗した場合、 */
	if ( rsa2 == 0x00 )
	{
		/** 処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		*lChgDataLen=0;
	 	return lRet;
	}
	else
	{
		rsa = RSA_new();
		BN_hex2bn(&(rsa->e), BN_bn2hex(rsa2->e));

		/** −公開鍵データを設定する。 */
		rsa->n = BN_bin2bn(pKey+7,128,0x00);
	}
	/** −公開鍵の復号化・公開鍵を行う。 */
	iRetLen = RSA_public_decrypt( lDataLen, pInData, pOutData, rsa, RSA_NO_PADDING );

	/** 戻り値チェックを行う。 */
	/** エラー発生した場合、 */
	if ( iRetLen <= 0 )
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		*lChgDataLen=0;
	}
	/** 正常の場合、 */
	else
	{
		/** −データと長さを設定する。 */
		*lChgDataLen = (long)iRetLen;
		IDMan_StMemcpy((char *)pChgData, pOutData, iRetLen );
		lRet = 0;
	}

	RSA_free(rsa);
	RSA_free(rsa2);
	return lRet;
}
/**
*公開鍵証明書からsubjectDN、issusrDNの取得関数
* @author University of Tsukuba
* @param *ptr 公開鍵証明書データ
* @param lLen 公開鍵証明書データ長
* @param *subjectDN 公開鍵証明書のsubjectDN
* @param *issuerDN 公開鍵証明書のissuerDN
* @return	long	0：正常 -1：異常
* @since 2008.02
* @version 1.0
*/
long IDMan_CmGetDN(void *ptr,long lLen,char *subjectDN,char *issuerDN)
{
	long 			lRet = -1L;
	X509 			*x509=0x00;
	char 			*p;
	X509_NAME 		*x509_name;
	int				iRet =0;
	char			szErrBuff[1023];
	BIO 			* BIOptr;

	ERR_load_crypto_strings();

	IDMan_StMemset(szErrBuff, 0x00, sizeof(szErrBuff));

	/** 公開鍵証明書データパラメータチェックを行う。 */
	/** エラー発生した場合 */
	if(ptr == 0x00)  
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 公開鍵証明書長データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if(lLen == 0 )  
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 公開鍵証明書のsubjectDNパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if( subjectDN == 0x00 )  
	{
		/** −処理を中断する。 */
		return lRet;
	}
	
	/** 公開鍵証明書のissuerDNパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if(issuerDN == 0x00 )  
	{
		/** −処理を中断する。 */
		return lRet;
	}
	
	/** X509構造体の初期化を行う。 */
	/** エラー発生した場合、 */
	if((x509 = X509_new())== 0x00) 
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		return lRet;
	}

	/** BIO構造の作成を行う。 */
	BIOptr = BIO_new(BIO_s_mem());

	/** BIO構造のリセットを行う。 */
	iRet = BIO_reset(BIOptr);
	/** エラー発生した場合、 */
	if(iRet != 1)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造へ公開鍵証明書データの設定を行う。 */
	iRet = BIO_write(BIOptr,ptr,lLen);
	/** エラー発生した場合、 */
	if( iRet != lLen )
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** X509構造体の作成を行う。 */
	x509 =(X509 *) d2i_X509_bio(BIOptr,0x00);
	/** エラー発生した場合、 */
	if(x509 == 0x00)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造の解放を行う。 */
	BIO_free(BIOptr);

	/** issuerDN取得する。 */
	x509_name = X509_get_issuer_name(x509);
	/** エラー発生した場合、 */
	if ( x509_name == 0x00)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		return lRet;
	}
	/** issuerDN Ascii変換を行う。 */
	p = X509_NAME_oneline(x509_name, 0, 0);
	/** エラー発生した場合、 */
	if (p == 0x00){
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		return lRet;
	}
	IDMan_StStrcpy(issuerDN,p);

	/** subjectDN取得する。 */
	x509_name = X509_get_subject_name(x509);
	/** エラー発生した場合、 */
	if ( x509_name == 0x00)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		return lRet;
	}
	/** subjectDN Ascii変換を行う。 */
	p = X509_NAME_oneline(x509_name, 0, 0);
	/** エラー発生した場合、 */
	if ( p == 0x00)
	{ 
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		return lRet;
	}
	IDMan_StStrcpy(subjectDN,p);

	lRet = 0;
	/** X509構造体の開放を行う。 */
	if(x509 !=0x00)	X509_free(x509);

	return lRet;
}
/**
*公開鍵証明書から証明書と署名の取得関数
* @author University of Tsukuba
* @param *ptr 公開鍵証明書
* @param lLen 公開鍵証明書長
* @param *pCertificate 証明書データ
* @param *lCertificateLen 証明書データ長
* @param *pSign 署名データ
* @param *lSignLen 署名データ長
* @return	long	0：正常 -1：異常
* @since 2008.02
* @version 1.0
*/
long IDMan_CmGetCertificateSign(void *ptr,long lLen,void *pCertificate,long *lCertificateLen,void *pSign,long *lSignLen)
{
	long 			lRet = -1L;
	X509 			*x509=0x00;
	int				iRet;
	char 			szErrBuff[1024];
	BIO 			* BIOptr;

	ERR_load_crypto_strings();

	IDMan_StMemset(szErrBuff, 0x00, sizeof(szErrBuff));

	/** 公開鍵証明書パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if(ptr == (void *)0x00)  
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 公開鍵証明書長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if( lLen == 0)  
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 証明書データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if( pCertificate == (void *)0x00)  
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 署名データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if( pSign == (void *)0x00)  
	{
		/** −処理を中断する。 */
		return lRet;
	}

	/** X509構造体の初期化を行う。 */
	/** エラー発生した場合、 */
	if((x509 = X509_new())== 0x00) 
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		return lRet;
	}

	/** X509構造体の作成を行う。 */
	/** BIO構造の作成を行う。 */
	BIOptr = BIO_new(BIO_s_mem());

	/** BIO構造のリセットを行う。 */
	iRet = BIO_reset(BIOptr);
	/** エラー発生した場合、 */
	if(iRet != 1)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造へ公開鍵証明書データの設定を行う。 */
	iRet = BIO_write(BIOptr,ptr,lLen);
	/** エラー発生した場合、 */
	if( iRet != lLen )
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** X509構造体の作成を行う。 */
	x509 =(X509 *) d2i_X509_bio(BIOptr,0x00);
	/** エラー発生した場合、 */
	if((x509 == 0x00))
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造の解放を行う。 */
	BIO_free(BIOptr);

	/** 証明書部の取得を行う。 */
	iRet = i2d_X509_CINF(x509->cert_info,(unsigned char **)&pCertificate);
	if (iRet != 0) {
		*lCertificateLen = iRet;
	}
	/** エラー発生した場合、 */
	else
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		return lRet;
	}
	/** 署名の取得を行う。 */
	/** 署名がある場合、 */
	if( x509->signature->length >= 1 )
	{
		/** −戻り値に値を設定する。 */
		IDMan_StMemcpy(pSign, x509->signature->data,x509->signature->length );
		*lSignLen = x509->signature->length;
	}

	/** X509の開放を行う。 */
	if(x509 !=0x00)	X509_free(x509);

	lRet =0;

	return lRet;
}

/**
*公開鍵証明書から公開鍵の取得関数
* @author University of Tsukuba
* @param *ptr 公開鍵証明書
* @param lLen 公開鍵証明書長
* @param *pPublicKey 公開鍵データ
	* @param *lPublicKeyLen 公開鍵データ長
* @return	long	0：正常 -1：異常
* @since 2008.02
* @version 1.0
*/
long IDMan_CmGetPublicKey(void *ptr,long lLen,void *pPublicKey,unsigned long *lPublicKeyLen)
{
	long 			lRet = -1L;
	X509_PUBKEY 	*pubkey=0x00;
	X509 			*x509=0x00;
	int 			iRet =0;
	char 			szErrBuff[1024];
	BIO 			* BIOptr;

	ERR_load_crypto_strings();

	IDMan_StMemset(szErrBuff, 0x00, sizeof(szErrBuff));

	/** 公開鍵証明書パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if(ptr == (void *)0x00)  
	{
		/** −処理を中断する。 */
		return lRet;
	}

	/** 公開鍵証明書長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if( lLen == 0)  
	{
		/** −処理を中断する。 */
		return lRet;
	}

	/** X509構造体の初期化を行う。 */
	/** エラー発生した場合、 */
	if((x509 = X509_new())== 0x00) 
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		return lRet;
	}

	/** X509構造体の作成を行う。 */

	/** BIO構造の作成を行う。 */
	BIOptr = BIO_new(BIO_s_mem());

	/** BIO構造のリセットを行う。 */
	iRet = BIO_reset(BIOptr);
	/** エラー発生した場合、 */
	if(iRet != 1)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造へ公開鍵証明書データの設定を行う。 */
	iRet = BIO_write(BIOptr,ptr,lLen);
	/** エラー発生した場合、 */
	if( iRet != lLen )
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** X509構造体の作成を行う。 */
	x509 =(X509 *) d2i_X509_bio(BIOptr,0x00);
	/** エラー発生した場合、 */
	if((x509 == 0x00))
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造の解放を行う。 */
	BIO_free(BIOptr);

	/** 証明書から公開鍵を取得する。 */
	pubkey = X509_get_X509_PUBKEY(x509);
	/** エラー発生した場合、 */
	if( pubkey == 0x00)
	{ 
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		return lRet;
	}
	/** 公開鍵を存在する場合、 */
	if( pubkey->public_key->length > 0 )
	{
		/** −取得する。 */
		*lPublicKeyLen= pubkey->public_key->length;
		IDMan_StMemcpy( pPublicKey, pubkey->public_key->data, *lPublicKeyLen);
		lRet=0;
	}

	/** X509構造体の開放を行う。 */
	if(x509 !=0x00)	X509_free(x509);

	return lRet;
}

/**
*乱数生成関数
* @author University of Tsukuba
* @param lLen 出力データ長
* @param *pData 乱数データ
* @return   long    0：正常 -1：異常
* @since 2008.02
* @version 1.0
*/
long IDMan_CmMakeRand(long lLen,  void *pData )
{
	BIGNUM 			bn;
	long 			lRet=-1L;
	unsigned char 	*p;	
	int 			i;

	/** 出力データ長パラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if( lLen <= 0)
	{
		/** −処理を中断する。 */
		return lRet;
	}

	/** 乱数データパラメータチェックを行う。*/
	/** エラー発生した場合、 */
	if( pData == 0x00)
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** BIGNUM変数の初期化を行う。 */
	BN_init( &bn);

	p= pData;
	for(i=0;i<lLen;i++)
	{
		/** 乱数の取得を行う。 */
		BN_rand(&bn,8,lLen+1,0);
		*(p+i)= atoi(BN_bn2dec(&bn));
	}
	lRet =0;
	BN_free(&bn);
	return lRet;
}

/**
*失効リストチェック関数
* @author University of Tsukuba
* @param *pInData 証明書データ 
* @param lInLen 証明書データ長
* @param *data CRLファイルデータ
* @param len CRLファイルデータレングス
* @return   long  0：未失効 1:失効中 -1：異常 
* @since 2008.02
* @version 1.0
*/
long IDMan_CmCheckCRL(void *pInData, long lInLen, void * data,unsigned long int len )
{
	long 			lRet=-1L;
	/*FILE 			*fp;*/
	X509 			*x509=0x00;
	X509_CRL 		*x509crl=0x00;
	X509_NAME 		*x509crlissuer = 0x00;
	int				iRet=0;
	int				idx;
	X509_REVOKED	rtmp;
	char			*p,*p2;
	char 			szErrBuff[1024];
	BIO 			* BIOptr;

	ERR_load_crypto_strings();

	IDMan_StMemset(szErrBuff, 0x00, sizeof(szErrBuff));

	/** 証明書データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if (pInData == 0x00) 
	{
		/** −処理を中断する。 */
		return lRet;
	}
	/** 証明書長データパラメータチェックを行う。 */
	/** エラー発生した場合、 */
	if (lInLen == 0)
	{
		/** −処理を中断する。 */
		return lRet;
	}

	/** 失効データパラメータチェックを行う。 */
	/** 失効データが存在しない場合、 */
	if (data==0x00 || len == 0)
	{
		/** −正常終了する。 */
		lRet =0;
		return lRet;
	}

	
	/** X509構造体の初期化を行う。 */
	/** エラー発生した場合、 */
	if((x509 = X509_new())== 0x00) 
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		return lRet;
	}

	/** DERファイルから、X509構造体にデータを読み込む。 */
	/** BIO構造の作成を行う。 */
	BIOptr = BIO_new(BIO_s_mem());

	/** BIO構造のリセットを行う。 */
	iRet = BIO_reset(BIOptr);
	/** エラー発生した場合、 */
	if(iRet != 1)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造へ公開鍵証明書データの設定を行う。 */
	iRet = BIO_write(BIOptr,pInData,lInLen);
	/** エラー発生した場合、 */
	if( iRet != lInLen )
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** X509構造体の作成を行う。 */
	x509 =(X509 *) d2i_X509_bio(BIOptr,0x00);
	/** エラー発生した場合、 */
	if((x509 == 0x00))
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造の解放を行う。 */
	BIO_free(BIOptr);

	/** issuerDNの取得を行う。 */
	X509_NAME *x509issuer = X509_get_issuer_name(x509);
	p = X509_NAME_oneline(x509issuer, 0, 0);	
	/** CRL情報の取得を行う。 */
	x509crl = X509_CRL_new();

	/** BIO構造の作成を行う。 */
	BIOptr = BIO_new(BIO_s_mem());

	/** BIO構造のリセットを行う。 */
	iRet = BIO_reset(BIOptr);
	/** エラー発生した場合、 */
	if(iRet != 1)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造へ失効リストデータの設定を行う。 */
	iRet = BIO_write(BIOptr,data,len);
	/** エラー発生した場合、 */
	if( iRet != (int)len )
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		BIO_free(BIOptr);
		return lRet;
	}
	/** x509crl構造へ失効リストデータの設定を行う。 */
	x509crl = (X509_CRL *) d2i_X509_CRL_bio(BIOptr,0x00);
	/** エラー発生した場合、 */
	if(x509crl==0x00) 
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		if(x509crl !=0x00)	X509_CRL_free(x509crl);
		BIO_free(BIOptr);
		return lRet;
	}

	/** BIO構造の解放を行う。 */
	BIO_free(BIOptr);

	/** シリアル番号の取得を行う。 */
	rtmp.serialNumber = X509_get_serialNumber(x509);

	/** CRLのissuerDNの取得を行う。 */
	/** エラー発生した場合、 */
	x509crlissuer = X509_CRL_get_issuer(x509crl);
	if ( x509crlissuer == 0x00 )
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		if(x509crl !=0x00)	X509_CRL_free(x509crl);
		return lRet;
	}

	/** 証明書とCRLのissuerDNの比較を行う。 */
	p2 = X509_NAME_oneline(x509crlissuer, 0, 0);
	/** エラー発生した場合、 */
	if( strcmp(p, p2) != 0)
	{
		/** −処理を中断する。 */
		ERR_error_string(ERR_get_error(), szErrBuff);
		if(x509 !=0x00)	X509_free(x509);
		if(x509crl !=0x00)	X509_CRL_free(x509crl);
		return lRet;
	}

	/** CRLのシリアル番号をソートする。 */
	if (!sk_is_sorted(x509crl->crl->revoked)) 
	{
		sk_sort(x509crl->crl->revoked);
	}
	/** CRLのシリアル番号を検索する。 */
	idx = sk_X509_REVOKED_find(x509crl->crl->revoked, &rtmp);
	/** 該当シリアル番号が存在する場合、 */
	if (idx >= 0) 
	{
		/** −戻り値に１を設定する。 */
		lRet =1;
	}
	/** 該当シリアル番号が存在しない場合、 */
	else
	{
		/** −戻り値に０を設定する。 */
		lRet =0;
	}

	/** X509の開放を行う。 */
	if(x509 !=0x00)	X509_free(x509);

	/** X509CRLの開放を行う。 */
	if(x509crl !=0x00)	X509_CRL_free(x509crl);

	return lRet;
}

