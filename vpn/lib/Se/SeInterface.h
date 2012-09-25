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

// Secure VM Project
// VPN Client Module (IPsec Driver) Source Code
// 
// Developed by Daiyuu Nobori (dnobori@cs.tsukuba.ac.jp)

// SeInterface.h
// 概要: SeInterface.c のヘッダ

#ifndef	SEINTERFACE_H
#define SEINTERFACE_H

// セキュア VM 側が定義すべきシステムコール一覧
struct SE_SYSCALL_TABLE
{
	// メモリ管理
	void *(*SysMemoryAlloc)(UINT size);
	void *(*SysMemoryReAlloc)(void *addr, UINT size);
	void (*SysMemoryFree)(void *addr);
	// ロック
	UINT (*SysGetCurrentCpuId)();
	SE_HANDLE (*SysNewLock)();
	void (*SysLock)(SE_HANDLE lock_handle);
	void (*SysUnlock)(SE_HANDLE lock_handle);
	void (*SysFreeLock)(SE_HANDLE lock_handle);
	// 時刻およびタイマ
	UINT (*SysGetTickCount)();
	SE_HANDLE (*SysNewTimer)(SE_SYS_CALLBACK_TIMER *callback, void *param);
	void (*SysSetTimer)(SE_HANDLE timer_handle, UINT interval);
	void (*SysFreeTimer)(SE_HANDLE timer_handle);
	// 物理 NIC
	void (*SysGetPhysicalNicInfo)(SE_HANDLE nic_handle, SE_NICINFO *info);
	void (*SysSendPhysicalNic)(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes);
	void (*SysSetPhysicalNicRecvCallback)(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param);
	// 仮想 NIC
	void (*SysGetVirtualNicInfo)(SE_HANDLE nic_handle, SE_NICINFO *info);
	void (*SysSendVirtualNic)(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes);
	void (*SysSetVirtualNicRecvCallback)(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param);
	// データの読み書き
	bool (*SysSaveData)(char *name, void *data, UINT data_size);
	bool (*SysLoadData)(char *name, void **data, UINT *data_size);
	void (*SysFreeData)(void *data);
	// RSA 署名操作
	bool (*SysRsaSign)(char *key_name, void *data, UINT data_size, void *sign, UINT *sign_buf_size);
	// ログの出力
	void (*SysLog)(char *type, char *message);
};

// NIC 情報
struct SE_NICINFO
{
	UCHAR MacAddress[SE_ETHERNET_MAC_ADDR_SIZE];		// MAC アドレス
	UINT Mtu;											// MTU
	UINT MediaType;										// メディア種類
	UINT64 MediaSpeed;									// メディアスピード (bps)
};

// NIC 情報におけるメディア種類
#define SE_MEDIA_TYPE_ETHERNET						0	// Ethernet

// 内部状態
struct SE_ROOT
{
	SE_SYSCALL_TABLE *SysCall;							// システムコールテーブル
	bool OpenSslInited;									// OpenSSL を初期化したかどうか
	SE_LOCK *TickLock;									// Tick 値関係のロック
	UINT LastTick;										// 前回の Tick 値
	UINT TickRoundCounter;								// Tick 値の周回カウンタ
	UINT64 StartTick64;									// システムが開始したときの Tick64 の値
};

#ifdef	SE_INTERNAL
extern SE_ROOT *rt;
#endif	// SE_INTERNAL

// エクスポート関数プロトタイプ
void VPN_IPsec_Init(SE_SYSCALL_TABLE *syscall_table, bool init_openssl);
SE_HANDLE VPN_IPsec_Client_Start(SE_HANDLE physical_nic_handle, SE_HANDLE virtual_nic_handle, char *config_name);
void VPN_IPsec_Client_Stop(SE_HANDLE client_handle);
void VPN_IPsec_Free();

// システムコール関数スタブプロトタイプ
void *SeSysMemoryAlloc(UINT size);
void *SeSysMemoryReAlloc(void *addr, UINT size);
void SeSysMemoryFree(void *addr);
UINT SeSysGetCurrentCpuId();
SE_HANDLE SeSysNewLock();
void SeSysLock(SE_HANDLE lock_handle);
void SeSysUnlock(SE_HANDLE lock_handle);
void SeSysFreeLock(SE_HANDLE lock_handle);
UINT SeSysGetTickCount();
SE_HANDLE SeSysNewTimer(SE_SYS_CALLBACK_TIMER *callback, void *param);
void SeSysSetTimer(SE_HANDLE timer_handle, UINT interval);
void SeSysFreeTimer(SE_HANDLE timer_handle);
void SeSysGetPhysicalNicInfo(SE_HANDLE nic_handle, SE_NICINFO *info);
void SeSysSendPhysicalNic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes);
void SeSysSetPhysicalNicRecvCallback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param);
void SeSysGetVirtualNicInfo(SE_HANDLE nic_handle, SE_NICINFO *info);
void SeSysSendVirtualNic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes);
void SeSysSetVirtualNicRecvCallback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param);
bool SeSysSaveData(char *name, void *data, UINT data_size);
bool SeSysLoadData(char *name, void **data, UINT *data_size);
void SeSysFreeData(void *data);
bool SeSysRsaSign(char *key_name, void *data, UINT data_size, void *sign, UINT *sign_buf_size);
void SeSysLog(char *type, char *message);

// その他関数プロトタイプ
SE_BUF *SeRsaSign(char *key_name, void *data, UINT data_size);

// 内部関数プロトタイプ
void SeIntInit(bool init_openssl);
void SeIntFree();


#endif	// SEINTERFACE_H


