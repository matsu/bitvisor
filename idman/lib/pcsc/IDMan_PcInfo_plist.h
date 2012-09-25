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
 * ID管理のPCSCライブラリ：CCIDでサポートしているリーダ端末定義
 * \file IDMan_PcInfo_plist.h
 */

#ifndef __infoplist_h__
#define __infoplist_h__


/*
 * 対応する端末情報は複数登録してもよいが同時接続しないこと。
 * また、CCIDドライバの登録内容(IDMan_CcUsb.c)と一致すること。
 */
///サポートするUSBデバイス情報構造体
static struct _driverTracker
{
	long manuID;	///<ベンダーID
	long productID;	///<製品ID

	char *readerName;
} driverTracker[] =
{
	// Chip Card Interface Device Class
	{ 0x04E6, 0x5116, "SCM SCR 3310" },
	{ 0x04E6, 0x511A, "SCM SCR 3310 NTTCom" },
	{ 0x0DC3, 0x1004, "Athena ASE IIIe USBv2" },
	{ 0x0DC3, 0x1102, "Athena ASEDrive IIIe KB USB" },
	{ 0x04E6, 0x5111, "SCM SCR 331-DI" },
	{ 0x04E6, 0x5113, "SCM SCR 333" },
	{ 0x04E6, 0x5115, "SCM SCR 335" },
	{ 0x04E6, 0x5117, "SCM SCR 3320" },
	{ 0x04E6, 0x5119, "SCM SCR 3340 ExpressCard54" },
	{ 0x04E6, 0x511C, "Axalto Reflex USB v3" },
	{ 0x04E6, 0x511D, "SCM SCR 3311" },
	{ 0x04E6, 0x5120, "SCM SCR 331-DI NTTCom" },
	{ 0x04E6, 0x5121, "SCM SDI 010" },
	{ 0x04E6, 0xE001, "SCM SCR 331" },
	{ 0x04E6, 0x5410, "SCM SCR 355" },
	{ 0x04E6, 0xE003, "SCM SPR 532" },
	{ 0x08E6, 0x3437, "Gemplus GemPC Twin" },
	{ 0x08E6, 0x3438, "Gemplus GemPC Key" },
	{ 0x08E6, 0x4433, "Gemplus GemPC433 SL" },
	{ 0x08E6, 0x3478, "Gemplus GemPC Pinpad" },
	{ 0x08E6, 0x3479, "Gemplus GemCore POS Pro" },
	{ 0x08E6, 0x3480, "Gemplus GemCore SIM Pro" },
	{ 0x08E6, 0x34EC, "Gemplus GemPC Express" },
	{ 0x08E6, 0xACE0, "Verisign Secure Token" },
	{ 0x08E6, 0x1359, "VeriSign Secure Storage Token" },
	{ 0x076B, 0x1021, "OmniKey CardMan 1021" },
	{ 0x076B, 0x3021, "OmniKey CardMan 3121" },
	{ 0x076B, 0x3621, "OmniKey CardMan 3621" },
	{ 0x076B, 0x3821, "OmniKey CardMan 3821" },
	{ 0x076B, 0x4321, "OmniKey CardMan 4321" },
	{ 0x076B, 0x5121, "OmniKey CardMan 5121" },
	{ 0x076B, 0x5125, "OmniKey CardMan 5125" },
	{ 0x076B, 0x5321, "OmniKey CardMan 5321" },
	{ 0x076B, 0x6622, "OmniKey CardMan 6121" },
	{ 0x076B, 0xA022, "Teo by Xiring" },
	{ 0x0783, 0x0006, "C3PO LTC31" },
	{ 0x0783, 0x0007, "C3PO TLTC2USB" },
	{ 0x0783, 0x0008, "C3PO LTC32 USBv2 with keyboard support" },
	{ 0x0783, 0x0009, "C3PO KBR36" },
	{ 0x0783, 0x0010, "C3PO LTC32" },
	{ 0x0783, 0x9002, "C3PO TLTC2USB" },
	{ 0x09C3, 0x0013, "ActivCard USB Reader 3.0" },
	{ 0x09C3, 0x0014, "Activkey Sim" },
	{ 0x047B, 0x020B, "Silitek SK-3105" },
	{ 0x413c, 0x2100, "Dell keyboard SK-3106" },
	{ 0x413c, 0X2101, "Dell smart card reader keyboard" },
	{ 0x046a, 0x0005, "Cherry XX33" },
	{ 0x046a, 0x0010, "Cherry XX44" },
	{ 0x046a, 0x002D, "Cherry ST1044U" },
	{ 0x046a, 0x003E, "Cherry SmartTerminal ST-2XXX" },
	{ 0x072f, 0x90cc, "ACS ACR 38U-CCID" },
	{ 0x0b97, 0x7762, "O2 Micro Oz776" },
	{ 0x0b97, 0x7772, "O2 Micro Oz776" },
	{ 0x0D46, 0x3001, "KOBIL KAAN Base" },
	{ 0x0D46, 0x3002, "KOBIL KAAN Advanced" },
	{ 0x0d46, 0x3003, "KOBIL KAAN SIM III" },
	{ 0x0d46, 0x3010, "KOBIL EMV CAP - SecOVID Reader III" },
	{ 0x0d46, 0x4000, "KOBIL mIDentity" },
	{ 0x0d46, 0x4001, "KOBIL mIDentity" },
	{ 0x073D, 0x0B00, "Eutron Digipass 860" },
	{ 0x073D, 0x0C00, "Eutron SIM Pocket Combo" },
	{ 0x073D, 0x0C01, "Eutron Smart Pocket" },
	{ 0x073D, 0x0007, "Eutron CryptoIdentity" },
	{ 0x073D, 0x0008, "Eutron CryptoIdentity" },
	{ 0x09BE, 0x0002, "SmartEpad" },
	{ 0x0416, 0x3815, "Winbond" },
	{ 0x03F0, 0x1024, "HP USB Smart Card Keyboard" },
	{ 0x03F0, 0x0824, "HP USB Smartcard Reader" },
	{ 0x0B81, 0x0200, "id3 CL1356D" },
	{ 0x058F, 0x9520, "Alcor Micro AU9520" },
	{ 0x15E1, 0x2007, "RSA SecurID" },
	{ 0x0BF8, 0x1005, "Fujitsu Siemens SmartCard Keyboard USB 2A" },
	{ 0x0BF8, 0x1006, "Fujitsu Siemens SmartCard USB 2A" },
	{ 0x0DF6, 0x800A, "Sitecom USB simcard reader MD-010" },
	{ 0x0973, 0x0003, "SchlumbergerSema Cyberflex Access" },
	{ 0x0471, 0x040F, "Philips JCOP41V221" },
	{ 0x04B9, 0x1400, "SafeNet IKey4000" },
	{ 0x1059, 0x000C, "G&D CardToken 350" },
	{ 0x1059, 0x000D, "G&D CardToken 550" },
	{ 0x17EF, 0x1003, "Lenovo Integrated Smart Card Reader" },
	{ 0x19E7, 0x0002, "Charismathics token" },
	{ 0x09C3, 0x0008, "ActivCard USB Reader 2.0" },
	{ 0x0783, 0x0003, "C3PO LTC31" },
	{ 0x0C4B, 0x0300, "Reiner-SCT cyberJack pinpad(a)" },
};
#define DRIVER_TRACKER_SIZE_STEP (sizeof(driverTracker)/sizeof(struct _driverTracker))

#endif
