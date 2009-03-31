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
 * ICカード管理層
 * \file IDMan_ICSCard.c
 */

/* 作成ライブラリ関数名 */
#include "IDMan_ICSCard.h"

#ifdef IDMAN_CLIENT
	#include "IDMan_StandardIo.h"
	#include "IDMan_PcPcsclite.h"
	#include "IDMan_PcWinscard.h"
	#include <core/string.h>
#else
	#include <stdio.h>
	#include <stdlib.h>
	#include <sys/time.h>
	#include <time.h>
	#include <unistd.h>
	#include <string.h>
	#include <arpa/inet.h>
	#include <winscard.h>
	#include <reader.h>
	#include <fcntl.h>
	#include <sys/mman.h>
	#include <errno.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/errno.h>
	#include <semaphore.h>
#endif


/**
 * PC/SCリソースとの接続コンテキストを生成する関数
 * @author University of Tsukuba
 * @param phContext コンテキスト
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_VALUE:パラメータ値無効
 * @since 2008.02
 * @version 1.0
 */
long IDMan_SCardEstablishContext(unsigned long* phContext)
{
	long 	sts=0 			;

	/** PC/SCリソースとの接続コンテキストを生成する。 */
	sts = SCardEstablishContext(SCARD_SCOPE_SYSTEM, 0x00, 0x00, phContext);

	/** 関数の戻り値を設定し終了する。 */
	return ( sts )	;
}

/**
 * PC/SCリソースとの接続コンテキストを開放する関数
 * @author University of Tsukuba
 * @param hContext コンテキスト
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_HANDLE:ハンドル無効
 * @since 2008.02
 * @version 1.0
 */
long IDMan_SCardReleaseContext(unsigned long hContext)
{
	long 	sts=0 			;

	/** 接続コンテキストを開放する。 */
	sts = SCardReleaseContext( hContext );

	return(sts);
}
/**
 * 現在利用可能なカードリーダのリストを取得する関数
 * @author University of Tsukuba
 * @param hContext コンテキスト
 * @param mszReaders リーダのリスト
 * @param pcchReaders リーダのリスト長
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_HANDLE:ハンドル無効
 *              SCARD_E_INSUFFICIENT_BUFFER:データ受信バッファが小さすぎる
 * @since 2008.02
 * @version 1.0
 */
long IDMan_SCardListReaders(unsigned long hContext, char* mszReaders, unsigned long* pcchReaders)
{
	long sts=0 ;

	/** 利用可能なカードリーダのリストを取得する。 */
	UL_DW (pcchReaders,
	sts = SCardListReaders( hContext, 0x00, mszReaders, pcchReaders );
	);

	/** 関数の戻りを設定し終了する。 */
	return( sts )	;
}

/**
 * 指定したカードリーダへの接続を確立する関数
 * @author University of Tsukuba
 * @param hContext コンテキスト
 * @param szReader リーダ名
 * @param dwShareMode 接続タイプのモード
 *                    SCARD_SHARE_SHARED:他アプリケーションとカードリーダを共有
 *                    SCARD_SHARE_EXCLUSIVE:他アプリケーションとのカードリーダ共有を許可しない
 *                    SCARD_SHARE_DIRECT:カードなしでリーダ制御可能
 * @param phCard カードハンドル
 * @param pdwActiveProtocol 確立されたプロトコル
 *                          SCARD_PROTOCOL_T0:T=0プロトコル(本システムでは使用しない)
 *                          SXARD_PROTOCOL_T1:T=1プロトコル
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_HANDLE:ハンドル無効
 *              SCARD_E_INVALID_VALUE:パラメータ値無効
 *              SCARD_E_NOT_READY:読み取り装置またはスマートカードはコマンドを受け取る準備ができていない
 *              SCARD_E_READER_UNAVAILABELE:指定された読み取り装置は現在使用できない
 *              SCARD_E_SHARING_VIOLATION:他に完了していない接続があるためスマートカードにアクセスできない
 *              SCARD_E_UNSUPPORTED_FEATURE:要求されたプロトコルはこのスマートカードでサポートされていない
 * @since 2008.02
 * @version 1.0
 */
long IDMan_SCardConnect(unsigned long hContext, const char* szReader, unsigned long dwShareMode, unsigned long* phCard, unsigned long* pdwActiveProtocol)
{
	long 	sts=0			;

	/** 指定したカードリーダへの接続を確立する。 */
	UL_DW (pdwActiveProtocol,
	sts = SCardConnect(hContext, szReader, dwShareMode, SCARD_PROTOCOL_T1, phCard, pdwActiveProtocol) ;
	);

	/** 関数の戻りを設定し終了する。 */
	return( sts )	;
}

/**
 * 確立したカードリーダへの接続を終了する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param dwDisposition 接続後のリーダの状態
 *                      SCARD_LEAVE_CARD:そのまま
 *                      SCARD_RESET_CARD:カードリセット
 *                      SCARD_UNPOWER_CARD:カード電源断
 *                      SCARD_EJECT_CARD:カード抜き取り
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_HANDLE:ハンドル無効
 *              SCARD_E_INVALID_VALUE:パラメータ値無効
 * @since 2008.02
 * @version 1.0
 */
long IDMan_SCardDisconnect(unsigned long hCard, unsigned long dwDisposition)
{
	long 	sts=0 			;

	/** −確立したカードリーダへの接続を終了する。 */
	sts = SCardDisconnect( hCard, dwDisposition );

	return ( sts );
}

/**
 * 接続したスマートカードへのＡＰＤＵコマンドを送信する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param pdwActiveProtocol 確立されたプロトコル
 *                          SCARD_PROTOCOL_T0:T=0プロトコル(本システムでは使用しない)
 *                          SCARD_PROTOCOL_T1:T=1プロトコル
 * @param pbSendBuffer 送信バッファ
 * @param cdSendLength 送信バッファ長
 * @param pbRecvBuffer 受信バッファ
 * @param pcbRecvLength 受信バッファ長
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_HANDLE:ハンドル無効
 *              SCARD_E_NOT_TRANSACTED:存在しないトランザクションを終了しようとした
 *              SCARD_E_PROTO_MISMATCH:要求したプロトコルはスマートカードに現在使用されているプロトコルと互換性がない
 *              SCARD_E_INVALID_VALUE:パラメータ値無効
 *              SCARD_E_READER_UNAVAILABLE:指定された読み取り装置は現在利用できない
 *              SCARD_W_RESET_CARD:スマートカードがリセットされ、共有状態情報は無効
 *              SCARD_W_REMOVED_CARD:スマートカードが取り出された
 * @since 2008.02
 * @version 1.0
 */
long IDMan_SCardTransmit(unsigned long hCard, unsigned long* pdwActiveProtocol, const unsigned char* pbSendBuffer, unsigned long cdSendLength, unsigned char* pbRecvBuffer, unsigned long* pcbRecvLength)
{
	SCARD_IO_REQUEST pioRecvPci	;
	SCARD_IO_REQUEST pioSendPci	;
	long sts ;

	pioSendPci = *SCARD_PCI_T1;

	/** Read Binaryマンドの場合、 */
	if ((( *pbSendBuffer == 0x00) && ( *(pbSendBuffer+1) == 0xb0))
	||  (( *pbSendBuffer == 0x00) && ( *(pbSendBuffer+1) == 0xd6)))
	{
		/** −分割送信を行う。 */
		sts = IDMan_SCardSetBinay(hCard, pdwActiveProtocol, (unsigned char*)pbSendBuffer, cdSendLength,pbRecvBuffer, pcbRecvLength );
	}
	/** Read Binaryマンド以外の場合、 */
	else
	{
		/** −接続したスマートカードへＡＰＤＵコマンドを送信し、PCSCからの戻り値を返却する。 */
		UL_DW (pcbRecvLength,
		sts = SCardTransmit( hCard, &pioSendPci, pbSendBuffer, cdSendLength, &pioRecvPci, pbRecvBuffer, pcbRecvLength );
		);
	}
	if( sts != SCARD_S_SUCCESS)
	{
		return(sts);
	}

	return ( sts );
}

/**
 * 接続したカードリーダの現在のステータスを取得する関数
 * @author University of Tsukuba
 * @param hCard カードハンドル
 * @param szReaderName リーダ名
 * @param pcchReaderLen リーダ名長
 * @param pdwState リーダ現在の状態
 *                 SCARD_ABSENT:リーダにカードなし
 *                 SCARD_PRESENT:リーダにカードはあるが、使える位置まで入っていない
 *                 SCARD_SWALLOWED:リーダにカードはあるが、カードの電源が入っていない
 *                 SCARD_POWERED:リーダのドライバがカードのモードを認識していない
 *                 SCARD_NEGOTIABLE:カードがリセットされ、ＰＴＳネゴシエーション待ち
 *                 SCARD_SPECIFIC:カードがリセットされ、特定の通信プロトコルが確立した
 * @param pdwProtocol プロトコル
 *                    SCARD_PROTOCOL_T0:T=0プロトコル(本システムでは使用しない)
 *                    SCARD_PROTOCOL_T1:T=1プロトコル
 * @param pbAtr カードの現在のＡＴＲ(初期応答)
 * @param pcbAtrLen ＡＴＲ長
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_HANDLE:ハンドル無効
 *              SCARD_E_INSUFFICIENT_BUFFER:データ受信バッファが小さすぎる
 *              SCARD_E_READER_UNAVAILABLE:指定された読み取り装置は現在利用できない
 * @since 2008.02
 * @version 1.0
 */
long IDMan_SCardStatus(unsigned long hCard, char* szReaderName, unsigned long* pcchReaderLen, unsigned long* pdwState, unsigned long* pdwProtocol, unsigned char* pbAtr, unsigned long* pcbAtrLen)
{
	long sts ;

	/** 接続したカードリーダの現在のステータスを取得し、戻り値を返却する。 */
	UL_DW (pcchReaderLen, UL_DW (pdwState, UL_DW (pdwProtocol,
	UL_DW (pcbAtrLen,
	sts = SCardStatus( hCard, szReaderName, pcchReaderLen, pdwState, pdwProtocol, pbAtr, pcbAtrLen );
	););););

	return ( sts );
}


/** 
* TLVレングス表記に変換する関数
* @author University of Tsukuba
* @param lSize  変換前データ
* @param pOutBuff 変換後データ
* @return int   0：正常 -1：異常
* @since 2008.02
* @version 1.0
*/
long IDMan_SCardSizeLongToChar(unsigned long *lSize, unsigned char *pOutBuff )
{

	unsigned char szSizeBuff[16];

	memset(szSizeBuff, 0x00, sizeof(szSizeBuff));
	memcpy(szSizeBuff,lSize, sizeof(lSize));
	/** 変換前サイズが256以上の場合、*/
	if ( *lSize >= 256)
	{
		/** −char3バイトに変換する。 */
		*pOutBuff     = 0;
		*(pOutBuff+1) = szSizeBuff[1];
		*(pOutBuff+2) = szSizeBuff[0];
		*lSize=3;
	}
	/** 上記以外の場合、*/
	else
	{
		/** −char1バイトに変換する。 */
		*pOutBuff = (char)*lSize;
		*lSize=1;
	}
		
	return 0;
}

/**
 * READBINSIZEで指定されたレングス単位にデータ送信・受信を行う関数
 * @author University of Tsukuba
 * @param hHnd カードハンドル
 * @param lPro 確立されたプロトコル
 *                          SCARD_PROTOCOL_T0:T=0プロトコル(本システムでは使用しない)
 *                          SCARD_PROTOCOL_T1:T=1プロトコル
 * @param pInBuff 送信バッファ
 * @param iInSize 送信バッファ長
 * @param pOutBuff 受信バッファ
 * @param pOutSize 受信バッファ長
 * @return long SCARD_S_SUCCESS:成功
 *              SCARD_E_INVALID_HANDLE:ハンドル無効
 *              SCARD_E_NOT_TRANSACTED:存在しないトランザクションを終了しようとした
 *              SCARD_E_PROTO_MISMATCH:要求したプロトコルはスマートカードに現在使用されているプロトコルと互換性がない
 *              SCARD_E_INVALID_VALUE:パラメータ値無効
 *              SCARD_E_READER_UNAVAILABLE:指定された読み取り装置は現在利用できない
 *              SCARD_W_RESET_CARD:スマートカードがリセットされ、共有状態情報は無効
 *              SCARD_W_REMOVED_CARD:スマートカードが取り出された
 * @since 2008.02
 * @version 1.0
 */
/*************************************/
long IDMan_SCardSetBinay(unsigned long hHnd, unsigned long *lPro, unsigned char *pInBuff, 
							unsigned long iInSize, unsigned char *pOutBuff, unsigned long *pOutSize)
{
	long				rv;
	unsigned char		szSendBuff[256];
	long				lSendSize;
	int					iLoopCnt;
	int					i;
	unsigned long			lSetLen;
	unsigned char		szTempBuff[64];
	unsigned char   	szRecvBuff[READBINSIZE*3+1];
	DWORD				lRecvSize;
	int					iErr;
	unsigned char		*p;
	SCARD_IO_REQUEST 	pioRecvPci	;
	SCARD_IO_REQUEST 	pioSendPci	;
	int					iFlg;
	int					iPos;
	int					iPutSize;


	pioSendPci = *SCARD_PCI_T1;
	rv=0;
	lSendSize=sizeof(szSendBuff);
	iLoopCnt=0;
	*pOutSize=0;
	iErr=0;
	iPos=0;
	iPutSize =0;

	iFlg = 0;
	if (( *pInBuff == 0x00) && ( *(pInBuff+1) == 0xd6))
	{
		iFlg = 1;
	}

	p=(unsigned char *)&iLoopCnt;;

	/** 送信文字列から、データレングスを取得する。 */
	if ( *(pInBuff+4) != 0x00 )
	/** 1バイト表記の場合、*/
	{
		/** −その値を使用する。 */
		iLoopCnt=(int)(*(pInBuff+4));
		iPos=4+1;
	}
	/** 3バイト表記の場合、 */
	else
	{
		/** −3バイトを数字型に変換する。 */
		*p = *(pInBuff+6);
		*(p+1) = *(pInBuff+5);
		iPos=4+3;
	}
	

	/** データ送信を行う */
	for(i=0;i<iLoopCnt;i+=READBINSIZE)
	{
		memset(szSendBuff, 0x00, sizeof(szSendBuff));
		if ( iFlg == 1 )
		{
			szSendBuff[1]= 0xD6;
		}
		else
		{
			szSendBuff[1]= 0xB0;
		}

		lSendSize =2;

		/** 読み込み位置の算出を行う。 */
		lSetLen =i;
		memset(szTempBuff, 0x00, sizeof(szTempBuff));
		IDMan_SCardSizeLongToChar(&lSetLen, szTempBuff);
		/** 読み込み位置が 0xFF以下の場合、*/
		if( lSetLen == 1 )
		{
			/** −値を設定する。*/
			szSendBuff[lSendSize]= 0x00;
			szSendBuff[lSendSize+1]= szTempBuff[0];
		}
		/** 読み込み位置が 0x100以上の場合、*/
		else
		{
			/** −2バイト表記に変更して値を設定する。*/
			lSetLen -= 1;
			memcpy(szSendBuff+lSendSize, szTempBuff+1,lSetLen );
		}
		lSetLen=2;
		/** 送信レングスの計算を行う */
		lSendSize += lSetLen;
		if( i+READBINSIZE > iLoopCnt )
		{
			/** 送信済みレングス＋1回の送信レングスの値がデータ長を超えてた場合、残りのレングスを算出して、設定する */
			lSetLen = iLoopCnt-i;
		}
		else
		{
			lSetLen =READBINSIZE;
		}
		iPutSize = lSetLen;
		memset(szTempBuff, 0x00, sizeof(szTempBuff));
		IDMan_SCardSizeLongToChar(&lSetLen, szTempBuff);
		memcpy(szSendBuff+lSendSize, szTempBuff,lSetLen );
		lSendSize += lSetLen;

		if ( iFlg == 1 )
		{
			memcpy(szSendBuff+5, pInBuff+i+iPos,iPutSize);
			lSendSize += iPutSize;
		}

		memset(szRecvBuff, 0x00, sizeof(szRecvBuff));
		lRecvSize = sizeof(szRecvBuff);

		/** 接続したスマートカードへＡＰＤＵコマンドを送信し、PCSCからの戻り値を返却する。 */
		rv = SCardTransmit( hHnd, &pioSendPci, szSendBuff, lSendSize, &pioRecvPci, szRecvBuff,&lRecvSize );
		if( rv == SCARD_S_SUCCESS )
		{
			/** 受信ステータスが0x9000の場合、 */
			if (( szRecvBuff[lRecvSize-2]== 0x90)
			&&  ( szRecvBuff[lRecvSize-1]== 0x00))
			{
				/** −データ連結を行う。 */
				memcpy(pOutBuff+*pOutSize ,szRecvBuff, lRecvSize-2);
				*pOutSize +=lRecvSize-2;
			}
			/** 受信ステータスが上記以外の場合、 */
			else{
				/** −受信バッファのステータスを設定し、処理を中止する。 */
				memcpy(pOutBuff ,szRecvBuff, 2);
				*pOutSize =2;
				iErr=1;
				break;
			}
		}
		else{
			iErr=1;
			break;
		}
	}
	/** エラーが発生してない場合、 */
	if (iErr==0)
	{
		/** −正常ステータスの0x9000をデータ連結する。 */
		*(pOutBuff+*pOutSize)=0x90;	
		*(pOutBuff+*pOutSize+1)=0x00;
		*pOutSize +=2;
	}
	return(rv);
}
