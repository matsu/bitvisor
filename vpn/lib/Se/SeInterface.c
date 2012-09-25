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

// SeInterface.c
// 概要: ドライバインターフェイス

#define SE_INTERNAL
#include <Se/Se.h>

// 唯一のグローバル変数
SE_ROOT *rt = NULL;

// IPsec クライアントの開始
SE_HANDLE VPN_IPsec_Client_Start(SE_HANDLE physical_nic_handle, SE_HANDLE virtual_nic_handle, char *config_name)
{
	// 引数チェック
	if (physical_nic_handle == NULL || virtual_nic_handle == NULL || config_name == NULL)
	{
		return NULL;
	}

	return (SE_HANDLE)SeVpnInit(physical_nic_handle, virtual_nic_handle, config_name);
}

// IPsec クライアントの停止
void VPN_IPsec_Client_Stop(SE_HANDLE client_handle)
{
	// 引数チェック
	if (client_handle == NULL)
	{
		return;
	}

	SeVpnFree((SE_VPN *)client_handle);
}

// IPsec モジュール全体の初期化
void SeIntInit(bool init_openssl)
{
	// カーネル系
	SeInitKernel();

	// 暗号化ライブラリ
	SeInitCrypto(init_openssl);
}

// IPsec モジュール全体の解放
void SeIntFree()
{
	// 暗号化ライブラリ
	SeFreeCrypto();

	// カーネル系
	SeFreeKernel();
}

// IPsec モジュールの初期化 (エクスポート関数)
void VPN_IPsec_Init(SE_SYSCALL_TABLE *syscall_table, bool init_openssl)
{
	// 引数チェック
	if (syscall_table == NULL)
	{
		return;
	}
	if (rt != NULL)
	{
		return;
	}

	// SE_ROOT 構造体の初期化
	rt = syscall_table->SysMemoryAlloc(sizeof(SE_ROOT));
	SeZero(rt, sizeof(SE_ROOT));

	// システムコールテーブルの設定
	rt->SysCall = syscall_table;

	// モジュールの初期化
	SeIntInit(init_openssl);
}

// IPsec モジュールの解放 (エクスポート関数)
void VPN_IPsec_Free()
{
	if (rt == NULL)
	{
		return;
	}

	// モジュールの解放
	SeIntFree();

	// SE_ROOT 構造体の解放
	rt->SysCall->SysMemoryFree(rt);

	rt = NULL;
}

// システムコール: メモリの確保
void *SeSysMemoryAlloc(UINT size)
{
	// 引数チェック
	if (size == 0)
	{
		size = 1;
	}

	return rt->SysCall->SysMemoryAlloc(size);
}

// システムコール: メモリの再確保
void *SeSysMemoryReAlloc(void *addr, UINT size)
{
	// 引数チェック
	if (addr == NULL)
	{
		return NULL;
	}
	if (size == 0)
	{
		size = 1;
	}

	return rt->SysCall->SysMemoryReAlloc(addr, size);
}

// システムコール: メモリの解放
void SeSysMemoryFree(void *addr)
{
	// 引数チェック
	if (addr == NULL)
	{
		return;
	}

	rt->SysCall->SysMemoryFree(addr);
}

// システムコール: 現在の CPU ID の取得
UINT SeSysGetCurrentCpuId()
{
	return rt->SysCall->SysGetCurrentCpuId();
}

// システムコール: ロックの作成
SE_HANDLE SeSysNewLock()
{
	return rt->SysCall->SysNewLock();
}

// システムコール: ロック
void SeSysLock(SE_HANDLE lock_handle)
{
	// 引数チェック
	if (lock_handle == NULL)
	{
		return;
	}

	rt->SysCall->SysLock(lock_handle);
}

// システムコール: ロック解除
void SeSysUnlock(SE_HANDLE lock_handle)
{
	// 引数チェック
	if (lock_handle == NULL)
	{
		return;
	}

	rt->SysCall->SysUnlock(lock_handle);
}

// システムコール: ロックの解放
void SeSysFreeLock(SE_HANDLE lock_handle)
{
	// 引数チェック
	if (lock_handle == NULL)
	{
		return;
	}

	rt->SysCall->SysFreeLock(lock_handle);
}

// システムコール: 起動してからの時刻の取得
UINT SeSysGetTickCount()
{
	return rt->SysCall->SysGetTickCount();
}

// システムコール: タイマの作成
SE_HANDLE SeSysNewTimer(SE_SYS_CALLBACK_TIMER *callback, void *param)
{
	// 引数チェック
	if (callback == NULL || param == NULL)
	{
		return NULL;
	}

	return rt->SysCall->SysNewTimer(callback, param);
}

// システムコール: タイマのセット
void SeSysSetTimer(SE_HANDLE timer_handle, UINT interval)
{
	// 引数チェック
	if (timer_handle == NULL)
	{
		return;
	}

	rt->SysCall->SysSetTimer(timer_handle, interval);
}

// システムコール: タイマの解放
void SeSysFreeTimer(SE_HANDLE timer_handle)
{
	// 引数チェック
	if (timer_handle == NULL)
	{
		return;
	}

	rt->SysCall->SysFreeTimer(timer_handle);
}

// システムコール: 物理 NIC 情報の取得
void SeSysGetPhysicalNicInfo(SE_HANDLE nic_handle, SE_NICINFO *info)
{
	// 引数チェック
	if (nic_handle == NULL || info == NULL)
	{
		return;
	}

	rt->SysCall->SysGetPhysicalNicInfo(nic_handle, info);
}

// システムコール: 物理 NIC へのパケットの送信
void SeSysSendPhysicalNic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes)
{
	// 引数チェック
	if (nic_handle == NULL || num_packets == 0 || packet_sizes == NULL || packets == NULL)
	{
		return;
	}

	rt->SysCall->SysSendPhysicalNic(nic_handle, num_packets, packets, packet_sizes);
}

// システムコール: 物理 NIC からのパケット受信コールバック関数の設定
void SeSysSetPhysicalNicRecvCallback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	// 引数チェック
	if (nic_handle == NULL || callback == NULL || param == NULL)
	{
		return;
	}

	rt->SysCall->SysSetPhysicalNicRecvCallback(nic_handle, callback, param);
}

// システムコール: 仮想 NIC 情報の取得
void SeSysGetVirtualNicInfo(SE_HANDLE nic_handle, SE_NICINFO *info)
{
	// 引数チェック
	if (nic_handle == NULL || info == NULL)
	{
		return;
	}

	rt->SysCall->SysGetVirtualNicInfo(nic_handle, info);
}

// システムコール: 仮想 NIC へのパケットの送信
void SeSysSendVirtualNic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes)
{
	// 引数チェック
	if (nic_handle == NULL || num_packets == 0 || packet_sizes == NULL || packets == NULL)
	{
		return;
	}

	rt->SysCall->SysSendVirtualNic(nic_handle, num_packets, packets, packet_sizes);
}

// システムコール: 仮想 NIC からのパケット受信コールバック関数の設定
void SeSysSetVirtualNicRecvCallback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	// 引数チェック
	if (nic_handle == NULL || callback == NULL || param == NULL)
	{
		return;
	}

	rt->SysCall->SysSetVirtualNicRecvCallback(nic_handle, callback, param);
}

// システムコール: データの保存
bool SeSysSaveData(char *name, void *data, UINT data_size)
{
	// 引数チェック
	if (name == NULL || data == NULL)
	{
		return false;
	}

	return rt->SysCall->SysSaveData(name, data, data_size);
}

// システムコール: データの読み込み
bool SeSysLoadData(char *name, void **data, UINT *data_size)
{
	// 引数チェック
	if (name == NULL || data == NULL)
	{
		return false;
	}

	return rt->SysCall->SysLoadData(name, data, data_size);
}

// システムコール: データの解放
void SeSysFreeData(void *data)
{
	// 引数チェック
	if (data == NULL)
	{
		return;
	}

	rt->SysCall->SysFreeData(data);
}

// システムコール: RSA 署名の実施
bool SeSysRsaSign(char *key_name, void *data, UINT data_size, void *sign, UINT *sign_buf_size)
{
	// 引数チェック
	if (key_name == NULL || data == NULL || sign == NULL || sign_buf_size == NULL)
	{
		return false;
	}

	return rt->SysCall->SysRsaSign(key_name, data, data_size, sign, sign_buf_size);
}

// システムコール: ログの出力
void SeSysLog(char *type, char *message)
{
	// 引数チェック
	if (type == NULL)
	{
		type = "Info";
	}
	if (message == NULL)
	{
		message = "No Message";
	}

	rt->SysCall->SysLog(type, message);
}

// RSA 署名の実施
SE_BUF *SeRsaSign(char *key_name, void *data, UINT data_size)
{
	UINT buffer_size = 8192;
	UCHAR *tmp;
	SE_BUF *ret;
	// 引数チェック
	if (key_name == NULL || data == NULL)
	{
		return NULL;
	}

	tmp = SeMalloc(buffer_size);
	if (SeSysRsaSign(key_name, data, data_size, tmp, &buffer_size) == false)
	{
		SeFree(tmp);
		return NULL;
	}

	ret = SeMemToBuf(tmp, buffer_size);

	SeFree(tmp);

	return ret;
}

