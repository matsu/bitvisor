/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008 
 *      National Institute of Information and Communications Technology
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

// Secure VM Project
// VPN Client Module (IPsec Driver) Source Code
// 
// Developed by Daiyuu Nobori (dnobori@cs.tsukuba.ac.jp)

// Se.h
// 概要: VPN モジュール全体用のヘッダ (外部プログラムから使用するときはこれをインクルードする)

#ifndef	SE_H
#define SE_H

//
// 全体で使用される定数の定義
//

// Ethernet 関係
#define SE_ETHERNET_MAC_ADDR_SIZE		6			// MAC アドレスサイズ
#define SE_ETHERNET_MAX_PAYLOAD_SIZE	1500		// 最大ペイロードサイズ
#define SE_ETHERNET_HEADER_SIZE			14			// ヘッダサイズ
#define SE_ETHERNET_MAX_PACKET_SIZE		(SE_ETHERNET_MAX_PAYLOAD_SIZE + SE_ETHERNET_HEADER_SIZE)	// 最大パケットサイズ

// IPv4 関係
#define SE_IP4_IP_ADDR_SIZE				4			// IP アドレスサイズ
#define SE_IP4_MAX_PAYLOAD_SIZE			65535		// 最大ペイロードサイズ

// IPv6 関係
#define SE_IP6_IP_ADDR_SIZE				4			// IP アドレスサイズ
#define SE_IP6_MAX_PAYLOAD_SIZE			65535		// 最大ペイロードサイズ

// ログの種類
#define SE_LOG_INFO						"INFO"		// 情報
#define SE_LOG_DEBUG					"DEBUG"		// デバッグ
#define SE_LOG_WARNING					"WARNING"	// 警告
#define SE_LOG_ERROR					"ERROR"		// エラー
#define SE_LOG_FATAL					"FATAL"		// 致命的なエラー


//
// 各ヘッダファイルのインクルード
//

// 型定義
#include <Se/SeTypes.h>

// ドライバインターフェイス
#include <Se/SeInterface.h>

// カーネル系操作
#include <Se/SeKernel.h>

// メモリ管理
#include <Se/SeMemory.h>

// 文字列操作
#include <Se/SeStr.h>

// 暗号化アルゴリズム (抽象化レイヤ)
#include <Se/SeCrypto.h>

// 設定ファイル読み込み
#include <Se/SeConfig.h>

// パケット解析モジュール
#include <Se/SePacket.h>

// IPv4 プロトコルスタック
#include <Se/SeIp4.h>

// IPv6 プロトコルスタック
#include <Se/SeIp6.h>

// IKE (ISAKMP) プロトコルスタック
#include <Se/SeIke.h>

// IPsec 処理
#include <Se/SeSec.h>

// VPN 処理 (IPv4 / IPv6 共通部分)
#include <Se/SeVpn.h>

// VPN 処理 (IPv4)
#include <Se/SeVpn4.h>

// VPN 処理 (IPv6)
#include <Se/SeVpn6.h>


#endif	// SE_H

