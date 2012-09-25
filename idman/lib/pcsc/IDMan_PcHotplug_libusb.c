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
 * ID管理のPCSCライブラリ：ホットプラグデバイス検索関数
 * \file IDMan_PcHotplug_libusb.c
 */

#include "IDMan_StandardIo.h"

#include "IDMan_PcReaderfactory.h"
#include "IDMan_PcInfo_plist.h"	// ASEDriveIIIe-USB-3.4 info.plist参照

#ifdef NTTCOM
#include "core/types.h"
#include "common/list.h"
#include "usb.h"
#include "usb_device.h"
#include "uhci.h"
#else
#include <usb.h>
#endif

#define ADD_SERIAL_NUMBER

#define READER_ABSENT		0	///<リーダ接続状態：未接続
#define READER_PRESENT		1	///<リーダ接続状態：接続済み
#define READER_FAILED		2	///<リーダ接続状態：取得失敗

#ifndef FALSE
#define FALSE			0
#define TRUE			1
#endif
#ifndef NULL
# define NULL	0
#endif


static short isUsbInit = 0;

///リーダ接続監視構造体（インスタンス）
struct _readerTracker readerTracker;


/**
 * @brief リーダの接続状態を検索します。
 *
 * @retval 1 USBデバイスあり
 * @retval 0 USBデバイスなし
 *
 * @since 2008.02
 * @version 1.0
 */
LONG HPSearchHotPluggables(void)
{
	int i;
	struct ID_USB_BUS *bus;
	struct ID_USB_DEVICE *dev;
	char bus_device[BUS_DEVICE_STRSIZE];


	/// リーダ接続状態が初期化されていない場合、
	if (!isUsbInit) {
		///−リーダ接続監視構造体を初期化
		readerTracker.driver = NULL;
		readerTracker.status = READER_ABSENT;	//リーダ接続状態：未接続
		readerTracker.bus_device[0] = '\0';		//デバイス名消去
		///−USBを初期化します。
		IDMan_StUsbInit();
		isUsbInit = 1;
	}

	/// USB BUS検索
	IDMan_StUsbFindBusses();
	/// USB デバイス検索
	IDMan_StUsbFindDevices();

	///リーダ接続状態を初期化（未接続）
	readerTracker.status = READER_ABSENT;

	///全USB BUS検索ループ
#ifndef NTTCOM
	for (bus = IDMan_StUsbGetBusses(); bus; bus = bus->next) {
#else
	for (bus = IDMan_StUsbGetBusses(); bus; bus = LIST_NEXT(usb_busses, bus)) {
#endif
		///−全USBデバイス検索ループ
#ifndef NTTCOM
		for (dev = bus->devices; dev; dev = dev->next) {
#else
		for (dev = bus->device; dev; dev = dev->next) {
#endif
			/* check if the device is supported by one driver */
			///−−サポートするUSBデバイス検索ループ
			for (i=0; i<DRIVER_TRACKER_SIZE_STEP; i++) {
				///−−−検出したUSBデバイスをサポートしていない場合、
				if (dev->descriptor.idVendor != driverTracker[i].manuID ||
					dev->descriptor.idProduct != driverTracker[i].productID) {
					///−−−−サポートするUSBデバイス検索ループへ
					continue;
				}

				//−−−検出したUSBデバイスをサポートしている場合、
				///−−−−検出した対応デバイス名を取得 "dirname:filename"
#ifndef NTTCOM
				IDMan_StStrcpy(bus_device, bus->dirname);
				IDMan_StStrcat(bus_device, ":");
				IDMan_StStrcat(bus_device, dev->filename);
#else
				IDMan_StStrcpy(bus_device, "002");
				IDMan_StStrcat(bus_device, ":");
				IDMan_StStrcat(bus_device, "002");
#endif
				
				//bus_device[BUS_DEVICE_STRSIZE - 1] = '\0';

				///−−−−検出状態を新規に設定
				int newreader = TRUE;

				///−−−−検出したリーダ端末は既に接続されている場合、
				//rv = strncmp(readerTracker.bus_device, bus_device, BUS_DEVICE_STRSIZE);
				{
					int rv = 0;
					const char *s1 = readerTracker.bus_device;
					const char *s2 = bus_device;
					ID_SIZE_T n = BUS_DEVICE_STRSIZE;
					do {
						if (*s1 != *s2++) {
							rv = (*(unsigned char *)s1 - *(unsigned char *)--s2);
							break;
						}
						if (*s1++ == 0)	break;
					} while (--n != 0);
					if (rv == 0)
					{
						///−−−−−リーダ接続状態を接続済みに設定
						readerTracker.status = READER_PRESENT;
						///−−−−−検出状態を既存に設定
						newreader = FALSE;
					}
				}

				///−−−−検出デバイスは既存の場合は サポートするUSBデバイス検索ループへ
				if (newreader==FALSE)	continue;

				///−−−−新規に検出したUSBデバイスをリーダ接続監視構造体に登録
				//HPAddHotPluggable(dev, bus_device, &driverTracker[i]);

				//リーダ接続監視構造体は１つしか存在しない（複数のリーダには対応しない）
				///−−−−対応するUSBデバイス情報構造体が既に設定されている場合は サポートするUSBデバイス検索ループへ
				if (readerTracker.driver != NULL)	continue;

				int j;
				///リーダ接続監視構造体に作成したデバイス名を格納
				//strlcpy(readerTracker.bus_device, bus_device, sizeof(readerTracker.bus_device));
				{
					char *dst = readerTracker.bus_device;
					const char *src = bus_device;
					int len = sizeof(readerTracker.bus_device) - 1;
					for (j=0; src[j] && j<len; ++j) {
						dst[j] = src[j];
					}
					dst[j] = 0;
				}
				///リーダ接続監視構造体に対応するUSBデバイス情報構造体を登録
				readerTracker.driver = &driverTracker[i];

#ifdef ADD_SERIAL_NUMBER
				///検出したUSBデバイス情報にシリアル番号がある場合、
				if (dev->descriptor.iSerialNumber) {
					struct ID_USB_DEV_HANDLE *device;
					char serialNumber[MAX_READERNAME];

					///−USBデバイスをオープンします。
					device = IDMan_StUsbOpen(dev);
					/// SetConfiguration
					usb_control_msg (device, 0, 9, 1, 0,
							 NULL, 0, 1000);
					///−USBデバイスのシリアル番号を文字列として取得します。
					IDMan_StUsbGetStringSimple(device, dev->descriptor.iSerialNumber,
						serialNumber, MAX_READERNAME);
					///−USBデバイスをクローズします。
					IDMan_StUsbClose(device);

					///−フルUSBデバイス名を作成してリーダ接続監視構造体に登録します。　書式:"%s (%s)"
					{
						char *dst = readerTracker.fullName;
						const char *src = readerTracker.driver->readerName;
						int len = MAX_READERNAME-1;
						for (j=0; src[j] && j<len; ++j) {
							dst[j] = src[j];
						}
						dst[j] = 0;
						if ((j+2+1+IDMan_StStrlen(serialNumber)) < len) {
							IDMan_StStrcat(readerTracker.fullName, " (");
							IDMan_StStrcat(readerTracker.fullName, serialNumber);
							IDMan_StStrcat(readerTracker.fullName, ")");
						}
					}
				}
				///検出したUSBデバイス情報にシリアル番号がない場合、
				else
#endif
				{
					///−USBデバイス名をリーダ接続監視構造体に登録します。
					char *dst = readerTracker.fullName;
					const char *src = readerTracker.driver->readerName;
					for (j=0; src[j] && j<(MAX_READERNAME-1); ++j) {
						dst[j] = src[j];
					}
					dst[j] = 0;
				}
				///検出したリーダのコンテキストを登録できた場合、 RFAddReader()
				if (RFAddReader(readerTracker.fullName) == SCARD_S_SUCCESS) {
					///−リーダ接続監視状態を接続済みに設定
					readerTracker.status = READER_PRESENT;
				}
				///検出したリーダのコンテキストを登録できなかった場合、
				else {
					///−リーダ接続監視状態を未接続に設定
					readerTracker.status = READER_FAILED;
				}
			}
		} /* End of USB device for..loop */
	} /* End of USB bus for..loop */

	/*
	 * check if all the previously found readers are still present
	 */
	///リーダが未接続なのに対応ドライバの割り当てがある場合、
	if (readerTracker.status == READER_ABSENT &&
			readerTracker.driver != NULL) {
		///−検出したリーダのコンテキストを除きます。 RFRemoveReader()
		RFRemoveReader(readerTracker.fullName);
		///−リーダ接続監視構造体を初期化
		readerTracker.driver = NULL;
		readerTracker.status = READER_ABSENT;
		readerTracker.bus_device[0] = '\0';
		return 0;
	}

	///正常終了（リターン）
	return 1;
}
