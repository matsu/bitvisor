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
 * ID管理のPCSCライブラリ：リーダ構造体制御関数 各種定義
 * \file IDMan_PcReaderfactory.h
 */

#ifndef __readerfactory_h__
#define __readerfactory_h__

#include "IDMan_PcIfdhandler.h"

	///カード接続ハンドル構造体
	struct RdrCliHandles
	{
		SCARDHANDLE hCard;		///<カード接続ハンドル
		DWORD dwEventStatus;	///<カードから送られる最近のイベント
	};

	///リーダコンテキスト構造体
	struct ReaderContext
	{
		char lpcReader[MAX_READERNAME];	///<リーダ名称
		struct RdrCliHandles psHandle;	///<カード接続ハンドル構造体

		DWORD dwSlot;			///<論理ユニット番号（0xXXXXYYYY - XXXXはリーダ番号、YYYYはスロット番号）
		DWORD dwIdentity;		/* 共有ID 上位16ビット:接続コンテキスト番号(1〜、0:なし) - カードハンドルに利用されます */
		DWORD dwContexts;		/* 共有してオープンしているコンテキストの数(-1は排他モード) */

		struct pubReaderStatesList *readerState; /* link to the reader state */
		/* we can't use PREADER_STATE here since IDMan_PcEventhandler.h can't be
		 * included because of circular dependencies */
	};

	typedef struct ReaderContext READER_CONTEXT, *PREADER_CONTEXT;

	#define BUS_DEVICE_STRSIZE	256

	///リーダ接続監視構造体
	struct _readerTracker
	{
		char status;							///<リーダ接続状態
		char bus_device[BUS_DEVICE_STRSIZE];	///<デバイス名 "dirname:filename"
		char fullName[MAX_READERNAME];			///<フルリーダ名（シリアル番号を含む場合あり）

		struct _driverTracker *driver;			///<対応するUSBデバイス情報構造体
	};

	LONG RFAddReader(LPSTR);
	LONG RFRemoveReader(LPSTR);
	LONG RFReaderInfoById(DWORD, struct ReaderContext **);
	LONG RFCheckReaderEventState(PREADER_CONTEXT, SCARDHANDLE);

#endif
