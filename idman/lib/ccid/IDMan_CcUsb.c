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
 * ID管理のCCIDドライバ：libUSBラッパ関数
 * \file IDMan_CcUsb.c
 */

#include "IDMan_CcCcid.h"

static struct ID_USB_INTERFACE * GetUsbInterface(struct ID_USB_DEVICE *usb_dev);
static unsigned long *GetBaudRates(ReaderMng* gReder);

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define USB_BULK_WRITE_TIMEOUT 2000  ///タイムアウト時間 5秒→2秒に変更

/*
 * 対応する端末情報は複数登録してもよいが同時接続しないこと。
 * また、PCSCドライバの登録内容(IDMan_PcInfo_plist.h)と一致すること。
 */
/*
 * 対応端末情報構造体
 */
static struct {
	ID_UNUM16 VendorID;	///<ベンダーID
	ID_UNUM16 ProductID;	///<製品ID
} CCIDC_TBL[] = {
	// Chip Card Interface Device Class
	{ 0x04E6, 0x5116 },	// SCM SCR 3310
	{ 0x04E6, 0x511A },	// SCM SCR 3310 NTTCom
	{ 0x0DC3, 0x1004 },	// Athena ASE IIIe USBv2
	{ 0x0DC3, 0x1102 },	// Athena ASEDrive IIIe KB USB
	{ 0x04E6, 0x5111 },	// SCM SCR 331-DI
	{ 0x04E6, 0x5113 },	// SCM SCR 333
	{ 0x04E6, 0x5115 },	// SCM SCR 335
	{ 0x04E6, 0x5117 },	// SCM SCR 3320
	{ 0x04E6, 0x5119 },	// SCM SCR 3340 ExpressCard54
	{ 0x04E6, 0x511C },	// Axalto Reflex USB v3
	{ 0x04E6, 0x511D },	// SCM SCR 3311
	{ 0x04E6, 0x5120 },	// SCM SCR 331-DI NTTCom
	{ 0x04E6, 0x5121 },	// SCM SDI 010
	{ 0x04E6, 0xE001 },	// SCM SCR 331
	{ 0x04E6, 0x5410 },	// SCM SCR 355
	{ 0x04E6, 0xE003 },	// SCM SPR 532
	{ 0x08E6, 0x3437 },	// Gemplus GemPC Twin
	{ 0x08E6, 0x3438 },	// Gemplus GemPC Key
	{ 0x08E6, 0x4433 },	// Gemplus GemPC433 SL
	{ 0x08E6, 0x3478 },	// Gemplus GemPC Pinpad
	{ 0x08E6, 0x3479 },	// Gemplus GemCore POS Pro
	{ 0x08E6, 0x3480 },	// Gemplus GemCore SIM Pro
	{ 0x08E6, 0x34EC },	// Gemplus GemPC Express
	{ 0x08E6, 0xACE0 },	// Verisign Secure Token
	{ 0x08E6, 0x1359 },	// VeriSign Secure Storage Token
	{ 0x076B, 0x1021 },	// OmniKey CardMan 1021
	{ 0x076B, 0x3021 },	// OmniKey CardMan 3121
	{ 0x076B, 0x3621 },	// OmniKey CardMan 3621
	{ 0x076B, 0x3821 },	// OmniKey CardMan 3821
	{ 0x076B, 0x4321 },	// OmniKey CardMan 4321
	{ 0x076B, 0x5121 },	// OmniKey CardMan 5121
	{ 0x076B, 0x5125 },	// OmniKey CardMan 5125
	{ 0x076B, 0x5321 },	// OmniKey CardMan 5321
	{ 0x076B, 0x6622 },	// OmniKey CardMan 6121
	{ 0x076B, 0xA022 },	// Teo by Xiring
	{ 0x0783, 0x0006 },	// C3PO LTC31
	{ 0x0783, 0x0007 },	// C3PO TLTC2USB
	{ 0x0783, 0x0008 },	// C3PO LTC32 USBv2 with keyboard support
	{ 0x0783, 0x0009 },	// C3PO KBR36
	{ 0x0783, 0x0010 },	// C3PO LTC32
	{ 0x0783, 0x9002 },	// C3PO TLTC2USB
	{ 0x09C3, 0x0013 },	// ActivCard USB Reader 3.0
	{ 0x09C3, 0x0014 },	// Activkey Sim
	{ 0x047B, 0x020B },	// Silitek SK-3105
	{ 0x413c, 0x2100 },	// Dell keyboard SK-3106
	{ 0x413c, 0X2101 },	// Dell smart card reader keyboard
	{ 0x046a, 0x0005 },	// Cherry XX33
	{ 0x046a, 0x0010 },	// Cherry XX44
	{ 0x046a, 0x002D },	// Cherry ST1044U
	{ 0x046a, 0x003E },	// Cherry SmartTerminal ST-2XXX
	{ 0x072f, 0x90cc },	// ACS ACR 38U-CCID
	{ 0x0b97, 0x7762 },	// O2 Micro Oz776
	{ 0x0b97, 0x7772 },	// O2 Micro Oz776
	{ 0x0D46, 0x3001 },	// KOBIL KAAN Base
	{ 0x0D46, 0x3002 },	// KOBIL KAAN Advanced
	{ 0x0d46, 0x3003 },	// KOBIL KAAN SIM III
	{ 0x0d46, 0x3010 },	// KOBIL EMV CAP - SecOVID Reader III
	{ 0x0d46, 0x4000 },	// KOBIL mIDentity
	{ 0x0d46, 0x4001 },	// KOBIL mIDentity
	{ 0x073D, 0x0B00 },	// Eutron Digipass 860
	{ 0x073D, 0x0C00 },	// Eutron SIM Pocket Combo
	{ 0x073D, 0x0C01 },	// Eutron Smart Pocket
	{ 0x073D, 0x0007 },	// Eutron CryptoIdentity
	{ 0x073D, 0x0008 },	// Eutron CryptoIdentity
	{ 0x09BE, 0x0002 },	// SmartEpad
	{ 0x0416, 0x3815 },	// Winbond
	{ 0x03F0, 0x1024 },	// HP USB Smart Card Keyboard
	{ 0x03F0, 0x0824 },	// HP USB Smartcard Reader
	{ 0x0B81, 0x0200 },	// id3 CL1356D
	{ 0x058F, 0x9520 },	// Alcor Micro AU9520
	{ 0x15E1, 0x2007 },	// RSA SecurID
	{ 0x0BF8, 0x1005 },	// Fujitsu Siemens SmartCard Keyboard USB 2A
	{ 0x0BF8, 0x1006 },	// Fujitsu Siemens SmartCard USB 2A
	{ 0x0DF6, 0x800A },	// Sitecom USB simcard reader MD-010
	{ 0x0973, 0x0003 },	// SchlumbergerSema Cyberflex Access
	{ 0x0471, 0x040F },	// Philips JCOP41V221
	{ 0x04B9, 0x1400 },	// SafeNet IKey4000
	{ 0x1059, 0x000C },	// G&D CardToken 350
	{ 0x1059, 0x000D },	// G&D CardToken 550
	{ 0x17EF, 0x1003 },	// Lenovo Integrated Smart Card Reader
	{ 0x19E7, 0x0002 },	// Charismathics token
	{ 0x09C3, 0x0008 },	// ActivCard USB Reader 2.0
	{ 0x0783, 0x0003 },	// C3PO LTC31
	{ 0x0C4B, 0x0300 },	// Reiner-SCT cyberJack pinpad(a)
};


#define ID_NUMBER	(sizeof(CCIDC_TBL)/sizeof(CCIDC_TBL[0]))


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef NULL
# define NULL	0
#endif

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +	libusb系ヘッダにて未定義
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
#ifndef USB_ENDPOINT_TYPE_BULK
#define USB_ENDPOINT_TYPE_BULK	2
#endif
#ifndef	USB_ENDPOINT_DIR_MASK
#define USB_ENDPOINT_DIR_MASK	0x80
#endif
#ifndef	USB_ENDPOINT_IN
#define USB_ENDPOINT_IN			0x80
#endif
#ifndef	USB_ENDPOINT_OUT
#define USB_ENDPOINT_OUT		0x00
#endif

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/**
 * @brief USBデバイスのオープン
 * @author University of Tsukuba
 *
 * @param[in] Readers 使用中のリーダ構造体リスト
 * @param[out] gReder オープンしたリーダ構造体
 *
 * @retval TRUE 成功。
 * @retval FALSE 失敗。
 *
 * @since 2008.02
 * @version 1.0
 */
bool OpenUSB(ReaderMng* Readers, ReaderMng* gReder)
{
	static struct ID_USB_BUS*	usb_bus = NULL;
	struct ID_USB_BUS*			work_bus;
	struct ID_USB_DEVICE*		usb_dev;
	int							dev_cnt;
	int							cnt1;
	int							cnt2;
	int							cnt3;
	char						bus_device[BUS_DEVICE_STRSIZE];
	int							dev_use;
	struct ID_USB_DEV_HANDLE*	dev_handle;
	int							interface;
	int							iRet;
	int							bEndpointAddress;

	///USB初期化
	IDMan_StUsbInit();
	/// USB BUS検索
	IDMan_StUsbFindBusses();
	/// USB デバイス検索
	IDMan_StUsbFindDevices();

	/// USB BUS取得
	usb_bus = IDMan_StUsbGetBusses();
	///取得できない場合、
	if (usb_bus == NULL)
	{
		///−失敗（リターン）
		return FALSE;
	}

	/// USB デバイスが既にオープンされている場合、
	if (gReder->usb_mng.DevHandle != NULL)
	{
		///−失敗（リターン）
		return FALSE;
	}

	///−全USB BUS検索ループ
#ifndef NTTCOM
	for (work_bus = usb_bus; work_bus; work_bus = work_bus->next)
#else
	for (work_bus = usb_bus; work_bus; work_bus = LIST_NEXT(usb_busses, work_bus))
#endif
	{
		///−− BUSに接続されているデバイス検索ループ
#ifndef NTTCOM
		for (usb_dev = work_bus->devices; usb_dev; usb_dev = usb_dev->next)
#else
		for (usb_dev = work_bus->device; usb_dev; usb_dev = usb_dev->next)
#endif
		{
			if (usb_dev->descriptor.idVendor==0 && usb_dev->descriptor.idProduct==0)	continue;
			///サポートするUSBデバイス検索ループ
			for (dev_cnt = 0; dev_cnt < ID_NUMBER ; dev_cnt++)
			{
				///−−−検出したUSBデバイスをサポートしていない場合、
				if (usb_dev->descriptor.idVendor != CCIDC_TBL[dev_cnt].VendorID
					|| usb_dev->descriptor.idProduct != CCIDC_TBL[dev_cnt].ProductID)
				{
					///−−−− BUSに接続されているデバイス検索ループへ
					continue;
				}
				/*+++++++++++++++++++++++++++++++++++++++++++++++++++
				 * 検出したUSBデバイスをサポートしている場合の処理
				 +++++++++++++++++++++++++++++++++++++++++++++++++++*/

				///−−−検出した対応デバイス名を取得 "dirname/filename"
				{
					IDMan_StMemset(bus_device, 0, BUS_DEVICE_STRSIZE);
#ifndef NTTCOM
					for (cnt1=0; work_bus->dirname[cnt1] && cnt1<(BUS_DEVICE_STRSIZE-1); ++cnt1) {
						bus_device[cnt1] = work_bus->dirname[cnt1];
					}
					if (cnt1 >= (BUS_DEVICE_STRSIZE-1))	return FALSE;
					bus_device[cnt1++] = '/';
					if (cnt1 >= (BUS_DEVICE_STRSIZE-1))	return FALSE;
					for (cnt3=0; usb_dev->filename[cnt3] && cnt1<(BUS_DEVICE_STRSIZE-1); ++cnt1, ++cnt3) {
						bus_device[cnt1] = usb_dev->filename[cnt3];
					}
					if (cnt1 >= (BUS_DEVICE_STRSIZE-1) && usb_dev->filename[cnt3])
						return FALSE;
#else
					IDMan_StStrcpy(bus_device, "002");
                                	IDMan_StStrcat(bus_device, ":");
                                	IDMan_StStrcat(bus_device, "002");
					cnt1 = 7;
#endif
				}

				///−−−検出したデバイスは既に使用している場合、
				dev_use = FALSE;
				for (cnt2 = 0; cnt2 < READER_NUMBER_MAX ; cnt2++) {
					if (IDMan_StStrcmp(Readers[cnt2].usb_mng.BusDev, bus_device) == 0)
						dev_use = TRUE;
				}
				if (dev_use) {
					///−−−−デバイス使用中、BUSに接続されているデバイス検索ループへ
					continue;
				}

				///−−− USB デバイスをオープンします。
				dev_handle = IDMan_StUsbOpen(usb_dev);
				///−−− オープンできない場合、
				if (!dev_handle) {
					///−−−− BUSに接続されているデバイス検索ループへ
					continue;
				}

				/*+++++++++++++++++++++++++++
				 * オープンできた場合の処理
				 +++++++++++++++++++++++++++*/
				if (usb_dev->config == NULL) {
					IDMan_StUsbClose(dev_handle);
					return FALSE;
				}

				struct ID_USB_INTERFACE *usb_interface = NULL;
				usb_interface = GetUsbInterface(usb_dev);
				if (usb_interface == NULL) {
					IDMan_StUsbClose(dev_handle);
					return FALSE;
				}
				
				//−−− CCIDクラスディスクプタ長が54ではない場合、
				if (usb_interface->altsetting->extralen != 54) {
					///−−−−CCIDクラスデバイスではない（リターン）
					IDMan_StUsbClose(dev_handle);
					return FALSE;
				}
				
				interface = usb_interface->altsetting->bInterfaceNumber;
				iRet = IDMan_StUsbClaimInterface(dev_handle, interface);
				if (iRet < 0) {
					IDMan_StUsbClose(dev_handle);
					return FALSE;
				}

				///−−−デバイス情報を \e gReder に格納
				gReder->usb_mng.DevHandle = dev_handle;
				gReder->usb_mng.Dev = usb_dev;
				{
					for (cnt1=0; bus_device[cnt1] && cnt1<BUS_DEVICE_STRSIZE; ++cnt1) {
						gReder->usb_mng.BusDev[cnt1] = bus_device[cnt1];
					}
					for ( ; cnt1 < BUS_DEVICE_STRSIZE; ++cnt1)
						gReder->usb_mng.BusDev[cnt1] = 0;
				}
				gReder->usb_mng.Interface = interface;
				gReder->usb_mng.OpenedSlots = 1;
				gReder->usb_mng.ReaderID = (usb_dev->descriptor.idVendor << 16) + usb_dev->descriptor.idProduct;

				///−−−エンドポイントを \e gReder に格納
				for (cnt1=0; cnt1<usb_interface->altsetting->bNumEndpoints; cnt1++) {
					if (usb_interface->altsetting->endpoint[cnt1].bmAttributes != USB_ENDPOINT_TYPE_BULK)
						continue;

					bEndpointAddress = usb_interface->altsetting->endpoint[cnt1].bEndpointAddress;

					if ((bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_IN)
						gReder->usb_mng.BulkIn = bEndpointAddress;

					if ((bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_ENDPOINT_OUT)
						gReder->usb_mng.BulkOut = bEndpointAddress;
				}

				///−−− CCID 情報を \e gReder に格納
				gReder->ccid_mng.bSeq = 0;
				gReder->ccid_mng.bMaxSlotIndex = usb_interface->altsetting->extra[4];
				gReder->ccid_mng.bInterfaceProtocol = usb_interface->altsetting->bInterfaceProtocol;
				gReder->ccid_mng.dwDefaultClock = GetLen(usb_interface->altsetting->extra, 10);
				gReder->ccid_mng.dwMaxDataRate = GetLen(usb_interface->altsetting->extra, 23);
				gReder->ccid_mng.dwMaxIFSD = GetLen(usb_interface->altsetting->extra, 28);
				gReder->ccid_mng.dwFeatures = GetLen(usb_interface->altsetting->extra, 40);
				gReder->ccid_mng.dwMaxCCIDMessageLength = GetLen(usb_interface->altsetting->extra, 44);
				gReder->ccid_mng.bPINSupport = usb_interface->altsetting->extra[52];
				gReder->ccid_mng.readTimeout = DEFAULT_READ_TIMEOUT;
				gReder->ccid_mng.arrayOfSupportedDataRates = GetBaudRates(gReder);

				///−−− 検出した対応デバイスのオープン成功（リターン）
				return TRUE;
			}
		}
	}
	///対応デバイスを検出できない（リターン）
	return FALSE;
}


/**
 * @brief USBデバイスにデータを書き込みます。
 * @author University of Tsukuba
 *
 * @param[in] gReder リーダ構造体
 * @param[in] WriteSize 書き込むサイズ
 * @param[in,out] WriteData 書き込むデータ
 *
 * @return 書き込んだサイズ ( ≧ 0)
 *
 * @since 2008.02
 * @version 1.0
 */
int WriteUSB(ReaderMng* gReder, int WriteSize, unsigned char* WriteData)
{
	int iRet;

	iRet = IDMan_StUsbBulkWrite(gReder->usb_mng.DevHandle,
			   gReder->usb_mng.BulkOut,
			   (char*)WriteData, WriteSize, USB_BULK_WRITE_TIMEOUT);
	if (iRet < 0) {
		return 0;
	}

	return WriteSize;
}


/**
 * @brief USBデバイスからデータを読み込みます。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ構造体
 * @param[in] Len 読み込むデータバッファのサイズ
 * @param[out] Buf 読み込むデータバッファ
 *
 * @return 読み込んだサイズ ( ≧ 0)
 *
 * @since 2008.02
 * @version 1.0
 */
int ReadUSB(ReaderMng* gReder, int Len, unsigned char *Buf)
{
	int	iRet= 0;

	///重複フレームループ
	do {
		///−リーダからデータを読み込みます
		iRet = IDMan_StUsbBulkRead(gReder->usb_mng.DevHandle,
							 gReder->usb_mng.BulkIn,
							 (char*)Buf, Len,
							 gReder->ccid_mng.readTimeout * 1000);
		///−受信データがない場合、
		if (iRet < 0) {
			///−戻り値に 0 を返す。（リターン）
			return 0;
		}

		///−重複フレームの場合、ループします。
	} while ((iRet > CCID_OFFSET_BSEQ) && (Buf[CCID_OFFSET_BSEQ] < gReder->ccid_mng.bSeq -1));

	///取得したデータ数を返す（リターン）
	return iRet;
}


/**
 * @brief USBデバイスをクローズします。
 * @author University of Tsukuba
 *
 * @param[in] gReder OpenUSBでオープンしたリーダ構造体
 *
 * @retval TRUE 成功。
 *
 * @since 2008.02
 * @version 1.0
 */
bool CloseUSB(ReaderMng* gReder)
{
	if (gReder->usb_mng.DevHandle == NULL)	return TRUE;
	if (gReder->ccid_mng.arrayOfSupportedDataRates)
	{
		gReder->ccid_mng.arrayOfSupportedDataRates = NULL;
	}
	IDMan_StUsbReset(gReder->usb_mng.DevHandle);
	IDMan_StUsbReleaseInterface(gReder->usb_mng.DevHandle, gReder->usb_mng.Interface);
	IDMan_StUsbClose(gReder->usb_mng.DevHandle);

	/* 初期化を行う */
	gReder->usb_mng.OpenedSlots = 0;
	gReder->usb_mng.DevHandle = NULL;
	gReder->usb_mng.Dev = NULL;
	gReder->usb_mng.BusDev[0] = '\0';
	gReder->usb_mng.Interface = 0;

	return TRUE;
}


/**
 * @brief USBインターフェースへのポインタを取得します。
 * 複数のインターフェースを持つ場合は最初のCCIDクラスまたはベンダ独自クラスを取得します。
 *
 * @author University of Tsukuba
 *
 * @param[in] dev 検出したUSBデバイス構造体へのポインタ
 *
 * @return	USBインターフェースへのポインタ。見つからない場合はNULL。
 *
 * @since 2008.02
 * @version 1.0
 */
static struct ID_USB_INTERFACE * GetUsbInterface(struct ID_USB_DEVICE *usb_dev)
{
	struct ID_USB_INTERFACE *usb_interface = NULL;
	int cnt;

	///ドライバが対応するUSBインターフェースを検索ループ
	for (cnt=0; usb_dev->config && cnt<usb_dev->config->bNumInterfaces; cnt++)
	{
		/* FIXME: NULL POINTER EXCEPTION 回避 */
		if (!usb_dev->config->interface[cnt].altsetting)
			continue;
		///−CCID(Chip Card Interface Device)クラスの場合、ループ
		if (usb_dev->config->interface[cnt].altsetting->bInterfaceClass != CCID_CLASS)	continue;

		///−USBインターフェースへのポインタ取得して、ループ終了
		usb_interface = &usb_dev->config->interface[cnt];
		break;
	}

	return usb_interface;
}


/**
 * @brief 端末がサポートする通信速度配列を取得します。
 * @author University of Tsukuba
 *
 * @param[in,out] gReder リーダ構造体
 *
 * @return	端末がサポートする通信速度配列へのポインタ。見つからない場合はNULL。
 *
 * @since 2008.02
 * @version 1.0
 */
static unsigned long *GetBaudRates(ReaderMng* gReder)
{
	int		iRate;
	int		cnt;
	int		RateNum;
	char	ReadBuf[256*sizeof(unsigned long)];

	///端末がサポートしている選択可能な通信速度配列を取得します。
	iRate = IDMan_StUsbControlMsg(gReder->usb_mng.DevHandle,
		0xA1, 
		0x03, 
		0x00, 
		gReder->usb_mng.Interface,
		ReadBuf, sizeof(ReadBuf),
		gReder->ccid_mng.readTimeout * 1000);

	///端末がサポートしている選択可能な通信速度配列が取得できない場合は NULL を返す。（リターン）
	if (iRate <= 0)	return NULL;

	///選択可能な通信速度配列のデータサイズがおかしい場合は NULL を返す。（リターン）
	if (iRate % sizeof(unsigned long))	return NULL;

	///端末がサポートしている選択可能な通信速度配列の数を計算します。
	iRate /= sizeof(unsigned long);

	///端末がサポートしているデータ通信速度の数を取得します。
	RateNum = GetUsbInterface(gReder->usb_mng.Dev)->altsetting->extra[27];
	///取得したデータ通信速度の数（≠ 0）と選択可能な数が一致しないの場合、
	if ((iRate != RateNum) && RateNum) {
		///−選択可能な数 \> 取得したサポート数の場合、
		if (iRate > RateNum) {
			///−−取得したサポートしている通信速度の数を選択可能な数とします。
			iRate = RateNum;
		}
	}

	IDMan_StMemset(gReder->ccid_mng.supportedDataRates, 0, sizeof(gReder->ccid_mng.supportedDataRates));
	for (cnt=0; cnt<iRate; cnt++) {
		gReder->ccid_mng.supportedDataRates[cnt] = GetLen(ReadBuf, cnt*4);
	}
	gReder->ccid_mng.supportedDataRates[cnt] = 0;

	return gReder->ccid_mng.supportedDataRates;
}
