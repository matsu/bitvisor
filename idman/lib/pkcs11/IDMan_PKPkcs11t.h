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

#include "pkcs11.h"

#define CK_NULL_PTR ((void *)0)

/* 属性タイプ値 */
#define CKA_EFID		(CKA_VENDOR_DEFINED + 1) /* ライブラリ独自 */
#define CKA_INDEX		(CKA_VENDOR_DEFINED + 2) /* ライブラリ独自 */
#define CKA_VALUE_ID		0x00000900 /* ライブラリ独自 */
#define CKA_VALUE_PASSWORD	0x00000901 /* ライブラリ独自 */

/* オブジェクトラベル値（ライブラリ独自） */
#define CK_LABEL_PRIVATE_KEY	"SecretKey"
#define CK_LABEL_PUBLIC_KEY	"PublicKey"
#define CK_LABEL_TRUST_CERT	"TrustCertificate"
#define CK_LABEL_PUBLIC_CERT	"PublicCertificate"
#define CK_LABEL_USER_PIN	"UserPin"
#define CK_LABEL_SO_PIN		"So  Pin"
#define CK_LABEL_ID_PASS	"ID/PASS"
#define CK_LABEL_PKCY_KEY	"PKCYKey"
#define CK_LABEL_PKCZ_KEY	"PKCZKey"
#define CK_LABEL_PKCP_KEY	"PKCPKey"

/* スロット情報 */
#define SLOTINFO_MANUFACTURERID \
	"Athena Smartcard Solutions Inc."/* スロットの製造者名【半角英数字】 */
#define SLOTINFO_HARDWAREVERSION "1.0" /* スロットのハードウェアバージョン */
#define SLOTINFO_FIRMWAREVERSION "1.0" /* スロットのファームウェアバージョン */

/* ICカードのAID */
#define APPLICATIONID_AID "A000000063504B43532D3135"

CK_RV C_GetCardStatus (CK_SESSION_HANDLE hSession);
