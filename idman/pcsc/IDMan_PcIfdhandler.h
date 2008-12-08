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
/*
 * Copyright (c) 1999-2003 David Corcoran <corcoran@linuxnet.com>
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * Changes to this license can be made only by the copyright author with 
 * explicit written consent.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ID管理のCCIDドライバ：IFDハンドラ関数 各種定義\n
 * Athena Smartcard Solutionsのリーダ ASE-III(USB)のローレベル呼出を提供します。
 * \file IDMan_PcIfdhandler.h
 */

#ifndef _ifd_handler_h_
#define _ifd_handler_h_

#include "IDMan_PcPcsclite.h"

/*
 * IFD Handlerで有効な構造体一覧
 */

/// スマートカード入出力ヘッダ構造体
  typedef struct _SCARD_IO_HEADER {
	DWORD Protocol;		///< プロトコル\n ATR_PROTOCOL_TYPE_T0 or ATR_PROTOCOL_TYPE_T1
	DWORD Length;		///< (未使用)
  } SCARD_IO_HEADER, *PSCARD_IO_HEADER;


/*
 * IFD Handler関数（引数）
 */
#define TAG_IFD_ATR					0x0303	///<要求タグ: ATR(Answer-to-Reset)データ取得
#define TAG_IFD_SIMULTANEOUS_ACCESS	0x0FAF	///<要求タグ:同時アクセス可能なリーダ端末数
#define TAG_IFD_SLOTS_NUMBER		0x0FAE	///<要求タグ:リーダ端末のスロット数
#define TAG_IFD_THREAD_SAFE			0x0FAD	///<要求タグ:端末のスレッドセーフ・フラグ
#define TAG_IFD_SLOT_THREAD_SAFE	0x0FAC	///<要求タグ:スロットのスレッドセーフ・フラグ

#define IFD_POWER_UP			500		///<カードへ電力供給し、ATRを取得
#define IFD_POWER_DOWN			501		///<カードへの電力供給を切断
#define IFD_RESET				502		///<カードをリセット

#define IFD_NEGOTIATE_PTS1		1	///<PTS1引数ありビット
#define IFD_NEGOTIATE_PTS2		2	///<PTS2引数ありビット
#define IFD_NEGOTIATE_PTS3		4	///<PTS3引数ありビット


/*
 * IFD Handler関数（戻り値）
 */
#define IFD_SUCCESS					0		///<正常終了
#define IFD_ERROR_TAG				600		///<要求タグが間違っています
#define IFD_ERROR_SET_FAILURE		601
#define IFD_ERROR_VALUE_READ_ONLY	602
#define IFD_ERROR_PTS_FAILURE		605		///<プロトコル選択失敗
#define IFD_ERROR_NOT_SUPPORTED	606
#define IFD_PROTOCOL_NOT_SUPPORTED	607		///<サポートしていないプロトコル
#define IFD_ERROR_POWER_ACTION		608		///<電力供給エラー
#define IFD_ERROR_SWALLOW			609
#define IFD_ERROR_EJECT				610
#define IFD_ERROR_CONFISCATE		611
#define IFD_COMMUNICATION_ERROR		612		///<通信エラー
#define IFD_RESPONSE_TIMEOUT		613		///<応答タイムアウト
#define IFD_NOT_SUPPORTED			614		///<未サポート
#define IFD_ICC_PRESENT				615		///<カードあり（電力供給なし）
#define IFD_ICC_NOT_PRESENT			616		///<カードなし
#define IFD_NO_SUCH_DEVICE			617		///<デバイスなし


//この値はPCSC/CCIDライブラリ化におけるCCID独自仕様です。
#define IFD_ICC_PRESENT_POWER_ON	630		///<カードあり（電力供給あり）


/*
 * IFDハンドラ関数(IFD handler Ver 2.0)
 */

  RESPONSECODE IFDHCreateChannel( DWORD, DWORD );
  RESPONSECODE IFDHCloseChannel( DWORD );
  RESPONSECODE IFDHGetCapabilities( DWORD, DWORD, PDWORD, PUCHAR );
  RESPONSECODE IFDHSetProtocolParameters( DWORD, DWORD, UCHAR, UCHAR, UCHAR, UCHAR );
  RESPONSECODE IFDHPowerICC( DWORD, DWORD, PUCHAR, PDWORD );
  RESPONSECODE IFDHTransmitToICC( DWORD, SCARD_IO_HEADER, PUCHAR, 
				   DWORD, PUCHAR, PDWORD, PSCARD_IO_HEADER );
  RESPONSECODE IFDHControl( DWORD, PUCHAR, DWORD, PUCHAR, PDWORD );
  RESPONSECODE IFDHICCPresence( DWORD );



#endif /* _ifd_hander_h_ */
