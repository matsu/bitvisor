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

#ifdef CRYPTO_VPN
#include "cpu.h"
#include "cpu_mmu.h"
#include "crypt.h"
#include "current.h"
#include "initfunc.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "testfs.h"
#include "time.h"
#include "timer.h"
#include "vcpu.h"
#include "ve_common.h"
#include "vmmcall.h"

// ロック
typedef struct CRYPT_LOCK
{
	spinlock_t spin_lock;
} CRYPT_LOCK;
// タイマ
typedef struct CRYPT_TIMER
{
	void *timer_obj;
	SE_SYS_CALLBACK_TIMER *callback;
	void *param;
} CRYPT_TIMER;
// NIC
struct VPN_NIC
{
	VPN_CTX *VpnCtx;					// コンテキスト
	SE_NICINFO NicInfo;					// NIC 情報
	SE_SYS_CALLBACK_RECV_NIC *RecvCallback;	// パケット受信時のコールバック
	void *RecvCallbackParam;			// コールバックパラメータ
	SE_QUEUE *SendPacketQueue;			// 送信パケットキュー
	SE_QUEUE *RecvPacketQueue;			// 受信パケットキュー
	bool IsVirtual;						// 仮想 NIC かどうか
	SE_LOCK *Lock;						// ロック
};
// VPN クライアントコンテキスト
struct VPN_CTX
{
	SE_HANDLE VpnClientHandle;			// VPN クライアントハンドル
	VPN_NIC *PhysicalNic;				// 物理 NIC
	SE_HANDLE PhysicalNicHandle;		// 物理 NIC ハンドル
	VPN_NIC *VirtualNic;				// 仮想 NIC
	SE_HANDLE VirtualNicHandle;			// 仮想 NIC ハンドル
	SE_LOCK *LogQueueLock;				// ログのキューのロック
	SE_QUEUE *LogQueue;					// ログのキュー
};

static VPN_CTX *vpn_ctx = NULL;			// VPN クライアントコンテキスト

// ve (Virtual Ethernet)
static spinlock_t ve_lock;

// Virtual Ethernet (ve) メイン処理
void crypt_ve_main(UCHAR *in, UCHAR *out)
{
	VE_CTL *cin, *cout;
	VPN_NIC *nic = NULL;
	// 引数チェック
	if (in == NULL || out == NULL)
	{
		return;
	}

	cin = (VE_CTL *)in;
	cout = (VE_CTL *)out;

	memset(cout, 0, sizeof(VE_CTL));

	if (cin->EthernetType == VE_TYPE_PHYSICAL)
	{
		// 物理的な LAN カード
		nic = vpn_ctx->PhysicalNic;
	}
	else if (cin->EthernetType == VE_TYPE_VIRTUAL)
	{
		// 仮想的な LAN カード
		nic = vpn_ctx->VirtualNic;
	}

	if (nic != NULL)
	{
		if (cin->Operation == VE_OP_GET_LOG)
		{
			// ログから 1 行取得
			SeLock(vpn_ctx->LogQueueLock);
			{
				char *str = SeGetNext(vpn_ctx->LogQueue);

				if (str != NULL)
				{
					cout->PacketSize = SeStrSize(str);
					SeStrCpy((char *)cout->PacketData, sizeof(cout->PacketData), str);

					SeFree(str);
				}

				cout->NumQueue = vpn_ctx->LogQueue->num_item;

				cout->RetValue = 1;
			}
			SeUnlock(vpn_ctx->LogQueueLock);
		}
		else if (cin->Operation == VE_OP_GET_NEXT_SEND_PACKET)
		{
			// 次に送信すべきパケットの取得 (vpn -> vmm -> guest)
			SeLock(nic->Lock);
			{
				// 古いパケットがある場合は破棄
				crypt_flush_old_packet_from_queue(nic->SendPacketQueue);

				// パケットが 1 つ以上キューにあるかどうか
				if (nic->SendPacketQueue->num_item >= 1)
				{
					// キューから次のパケットをとる
					void *packet_data = SeGetNext(nic->SendPacketQueue);
					UINT packet_data_size = SeMemSize(packet_data);
					UINT packet_size_real = packet_data_size - sizeof(UINT64);
					void *packet_data_real = ((UCHAR *)packet_data) + sizeof(UINT64);

					memcpy(cout->PacketData, packet_data_real, packet_size_real);
					cout->PacketSize = packet_size_real;

					cout->NumQueue = nic->SendPacketQueue->num_item;

					// メモリ解放
					SeFree(packet_data);
				}

				cout->RetValue = 1;
			}
			SeUnlock(nic->Lock);
		}
		else if (cin->Operation == VE_OP_PUT_RECV_PACKET)
		{
			bool flush = false;
			UINT num_packets = 0;
			void **packets = NULL;
			UINT *packet_sizes = NULL;

			// 受信したパケットの書き込み (guest -> vmm -> vpn)
			SeLock(nic->Lock);
			{
				// 受信パケットは、パフォーマンス向上のため
				// すぐに vpn に渡さずにいったん受信キューにためる
				void *packet_data;
				UINT packet_size = cin->PacketSize;

				if (packet_size >= 1)
				{
					packet_data = SeClone(cin->PacketData, packet_size);

					SeInsertQueue(nic->RecvPacketQueue, packet_data);
				}

				if (cin->NumQueue == 0)
				{
					// もうこれ以上受信パケットが無い場合は
					// flush する (vpn に一気に渡す)
					flush = true;
				}

				cout->RetValue = 1;

				if (flush)
				{
					UINT i;
					void *p;

					num_packets = nic->RecvPacketQueue->num_item;
					packets = SeMalloc(sizeof(void *) * num_packets);
					packet_sizes = SeMalloc(sizeof(UINT *) * num_packets);

					i = 0;

					while (true)
					{
						UINT size;
						p = SeGetNext(nic->RecvPacketQueue);
						if (p == NULL)
						{
							break;
						}

						size = SeMemSize(p);

						packets[i] = p;
						packet_sizes[i] = size;

						i++;
					}
				}
			}
			SeUnlock(nic->Lock);

			if (flush)
			{
				UINT i;

				crypt_nic_recv_packet(nic, num_packets, packets, packet_sizes);

				for (i = 0;i < num_packets;i++)
				{
					SeFree(packets[i]);
				}

				SeFree(packets);
				SeFree(packet_sizes);
			}
		}
	}
}

// Virtual Ethernet (ve) ハンドラ
void crypt_ve_handler()
{
	UINT i;
	UCHAR data[VE_BUFSIZE];
	UCHAR data2[VE_BUFSIZE];
	intptr addr;
	bool ok = true;

	spinlock_lock(&ve_lock);

	current->vmctl.read_general_reg(GENERAL_REG_RBX, &addr);

	for (i = 0;i < (VE_BUFSIZE / 4);i++)
	{
		if (read_linearaddr_l((u32)((ulong)(((UCHAR *)addr) + (i * 4))), (u32 *)(data + (i * 4))) != VMMERR_SUCCESS)
		{
			ok = false;
			break;
		}
	}
	
#ifndef	CRYPTO_VPN
	ok = false;
#endif	// CRYPTO_VPN

	if (ok == false)
	{
		current->vmctl.write_general_reg(GENERAL_REG_RAX, 0);

		spinlock_unlock(&ve_lock);
		return;
	}

	crypt_ve_main(data, data2);

	for (i = 0;i < (VE_BUFSIZE / 4);i++)
	{
		if (write_linearaddr_l((u32)((ulong)(((UCHAR *)addr) + (i * 4))), *((UINT *)(&data2[i * 4]))) != VMMERR_SUCCESS)
		{
			ok = false;
			break;
		}
	}

	if (ok == false)
	{
		current->vmctl.write_general_reg(GENERAL_REG_RAX, 0);

		spinlock_unlock(&ve_lock);
		return;
	}

	current->vmctl.write_general_reg(GENERAL_REG_RAX, 1);

	spinlock_unlock(&ve_lock);
}

// Virtual Ethernet (ve) の初期化
void crypt_ve_init()
{
	printf("Initing Virtual Ethernet (VE) for VPN Client Module...\n");
	spinlock_init(&ve_lock);
	vmmcall_register("ve", crypt_ve_handler);
	printf("Virtual Ethernet (VE): Ok.\n");
}

INITFUNC ("vmmcal9", crypt_ve_init);

// 物理 NIC の初期化
VPN_NIC *crypt_init_physical_nic(VPN_CTX *ctx)
{
	VPN_NIC *n = SeZeroMalloc(sizeof(VPN_NIC));

	SeStrToBinEx(n->NicInfo.MacAddress, sizeof(n->NicInfo.MacAddress),
		"00:12:12:12:12:12");
	n->NicInfo.MediaSpeed = 1000000000;
	n->NicInfo.MediaType = SE_MEDIA_TYPE_ETHERNET;
	n->NicInfo.Mtu = 1500;

	n->RecvPacketQueue = SeNewQueue();
	n->SendPacketQueue = SeNewQueue();

	n->VpnCtx = ctx;

	n->IsVirtual = false;

	n->Lock = SeNewLock();

	return n;
}

// 仮想 NIC の作成
VPN_NIC *crypt_init_virtual_nic(VPN_CTX *ctx)
{
	VPN_NIC *n = SeZeroMalloc(sizeof(VPN_NIC));

	SeStrToBinEx(n->NicInfo.MacAddress, sizeof(n->NicInfo.MacAddress),
		"00:AC:AC:AC:AC:AC");
	n->NicInfo.MediaSpeed = 1000000000;
	n->NicInfo.MediaType = SE_MEDIA_TYPE_ETHERNET;
	n->NicInfo.Mtu = 1500;

	n->RecvPacketQueue = SeNewQueue();
	n->SendPacketQueue = SeNewQueue();

	n->VpnCtx = ctx;

	n->IsVirtual = true;

	n->Lock = SeNewLock();

	return n;
}

// VPN クライアントの初期化
void crypt_init_vpn()
{
	vpn_ctx = SeZeroMalloc(sizeof(VPN_CTX));

	// ログ用キューの初期化
	vpn_ctx->LogQueue = SeNewQueue();
	vpn_ctx->LogQueueLock = SeNewLock();

	// 物理 NIC の作成
	vpn_ctx->PhysicalNic = crypt_init_physical_nic(vpn_ctx);
	vpn_ctx->PhysicalNicHandle = (SE_HANDLE)vpn_ctx->PhysicalNic;

	// 仮想 NIC の作成
	vpn_ctx->VirtualNic = crypt_init_virtual_nic(vpn_ctx);
	vpn_ctx->VirtualNicHandle = (SE_HANDLE)vpn_ctx->VirtualNic;

	// VPN Client の作成
	vpn_ctx->VpnClientHandle = VPN_IPsec_Client_Start(vpn_ctx->PhysicalNicHandle, vpn_ctx->VirtualNicHandle, "config.txt");
}

// 提供システムコール: メモリ確保
void *crypt_sys_memory_alloc(UINT size)
{
	UCHAR *p;
	if (size == 0)
	{
		size = 1;
	}

	p = alloc(size + (UINT)sizeof(void *));
	if (p == NULL)
	{
		panic("alloc(%u) failed.\n", size + (UINT)sizeof(void *));
	}

	*((UINT *)p) = size;

	return p + sizeof(void *);
}

// 提供システムコール: メモリ再確保
void *crypt_sys_memory_realloc(void *addr, UINT size)
{
	UCHAR *p;
	UCHAR *new_p;
	UINT old_size;
	UINT copy_size;
	if (addr == NULL)
	{
		return NULL;
	}
	if (size == 0)
	{
		size = 1;
	}

	p = addr - sizeof(void *);

	old_size = *((UINT *)p);

	if (old_size < size)
	{
		copy_size = old_size;
	}
	else
	{
		copy_size = size;
	}

	new_p = alloc(size + (UINT)sizeof(void *));
	if (new_p == NULL)
	{
		panic("alloc(%u) failed.\n", size + (UINT)sizeof(void *));
	}

	memcpy(new_p + sizeof(void *), p + sizeof(void *), copy_size);

	free(p);

	*((UINT *)new_p) = size;

	return new_p + sizeof(void *);
}

// 提供システムコール: メモリ解放
void crypt_sys_memory_free(void *addr)
{
	UCHAR *p;
	if (addr == NULL)
	{
		return;
	}

	p = addr - sizeof(void *);

	free(p);
}

// 提供システムコール: ログの出力 (画面に表示)
void crypt_sys_log(char *type, char *message)
{
	char *lf = "\n";

	if (message[strlen(message) - 1] == '\n')
	{
		lf = "";
	}

	if (type != NULL && SeStrLen(type) >= 1)
	{
		printf("%s: %s%s", type, message, lf);
	}
	else
	{
		printf("%s%s", message, lf);
	}

	crypt_add_log_queue(type, message);
}

// ログキューにログデータを追加
void crypt_add_log_queue(char *type, char *message)
{
	char *tmp;
	char tmp2[512];
	// 引数チェック
	if (type == NULL || message == NULL)
	{
		return;
	}
	if (vpn_ctx == NULL)
	{
		return;
	}

	tmp = SeCopyStr(message);
	if (SeStrLen(tmp) >= 1)
	{
		if (tmp[SeStrLen(tmp) - 1] == '\n')
		{
			tmp[SeStrLen(tmp) - 1] = 0;
		}
	}
	if (SeStrLen(tmp) >= 1)
	{
		if (tmp[SeStrLen(tmp) - 1] == '\r')
		{
			tmp[SeStrLen(tmp) - 1] = 0;
		}
	}

	if (type != NULL && SeStrLen(type) >= 1)
	{
		SeFormat(tmp2, sizeof(tmp2), "%s: %s", type, tmp);
	}
	else
	{
		SeStrCpy(tmp2, sizeof(tmp2), tmp);
	}

	SeFree(tmp);

	SeLock(vpn_ctx->LogQueueLock);
	{
		while (vpn_ctx->LogQueue->num_item > CRYPT_MAX_LOG_QUEUE_LINES)
		{
			char *p = SeGetNext(vpn_ctx->LogQueue);

			SeFree(p);
		}

		SeInsertQueue(vpn_ctx->LogQueue, SeCopyStr(tmp2));
	}
	SeUnlock(vpn_ctx->LogQueueLock);
}

// 提供システムコール: 現在の CPU ID の取得
UINT crypt_sys_get_current_cpu_id()
{
	return (UINT)get_cpu_id();
}

// 提供システムコール: ロックの作成
SE_HANDLE crypt_sys_new_lock()
{
	CRYPT_LOCK *o = crypt_sys_memory_alloc(sizeof(CRYPT_LOCK));
	
	memset(o, 0, sizeof(CRYPT_LOCK));

	spinlock_init(&o->spin_lock);

	return (SE_HANDLE)o;
}

// 提供システムコール: ロック
void crypt_sys_lock(SE_HANDLE lock_handle)
{
	CRYPT_LOCK *o = (CRYPT_LOCK *)lock_handle;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	spinlock_lock(&o->spin_lock);
}

// 提供システムコール: ロック解除
void crypt_sys_unlock(SE_HANDLE lock_handle)
{
	CRYPT_LOCK *o = (CRYPT_LOCK *)lock_handle;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	spinlock_unlock(&o->spin_lock);
}

// 提供システムコール: ロックの解放
void crypt_sys_free_lock(SE_HANDLE lock_handle)
{
	CRYPT_LOCK *o = (CRYPT_LOCK *)lock_handle;
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	crypt_sys_memory_free(o);
}

// 提供システムコール: システム時刻の取得
UINT crypt_sys_get_tick_count()
{
	UINT64 t = chelp_div_64_32_64((get_cpu_time() + (UINT64)1), 1000);

	return (UINT)t;
}

void SeTimerCallback(SE_HANDLE timer_handle, void *param);


// タイマ用内部コールバックルーチン
void crypt_timer_callback(void *handle, void *data)
{
	CRYPT_TIMER *t = (CRYPT_TIMER *)data;
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	t->callback((SE_HANDLE)t, t->param);
}

// 提供システムコール: タイマの作成
SE_HANDLE crypt_sys_new_timer(SE_SYS_CALLBACK_TIMER *callback, void *param)
{
	CRYPT_TIMER *t;
	// 引数チェック
	if (callback == NULL)
	{
		return NULL;
	}

	t = crypt_sys_memory_alloc(sizeof(CRYPT_TIMER));
	memset(t, 0, sizeof(CRYPT_TIMER));

	t->param = param;
	t->callback = callback;

	t->timer_obj = timer_new(crypt_timer_callback, t);

	return (SE_HANDLE)t;
}

// 提供システムコール: タイマのセット
void crypt_sys_set_timer(SE_HANDLE timer_handle, UINT interval)
{
	CRYPT_TIMER *t = (CRYPT_TIMER *)timer_handle;
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	timer_set(t->timer_obj, chelp_mul_64_64_64((UINT64)interval, (UINT64)1000));
}

// 提供システムコール: タイマの解放
void crypt_sys_free_timer(SE_HANDLE timer_handle)
{
	CRYPT_TIMER *t = (CRYPT_TIMER *)timer_handle;
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	timer_free(t->timer_obj);

	crypt_sys_memory_free(t);
}

// 提供システムコール: データの書き込み
bool crypt_sys_save_data(char *name, void *data, UINT data_size)
{
	// 実装されていない
	chelp_printf("crypt.c: SysSaveData() is not implemented.\n");

	return false;
}

// 提供システムコール: データの読み込み
bool crypt_sys_load_data(char *name, void **data, UINT *data_size)
{
	UCHAR *tmp_data;
	UINT tmp_size;
	// 引数チェック
	if (name == NULL || data == NULL || data_size == NULL)
	{
		return false;
	}

	// testfs からデータを読み込み
	if (testfs_loaddata(name, (void **)&tmp_data, &tmp_size) == 0)
	{
		return false;
	}

	*data = crypt_sys_memory_alloc(tmp_size);
	memcpy(*data, tmp_data, tmp_size);

	*data_size = tmp_size;

	return true;
}

// 提供システムコール: 読み込んだデータの解放
void crypt_sys_free_data(void *data)
{
	// 引数チェック
	if (data == NULL)
	{
		return;
	}

	crypt_sys_memory_free(data);
}

// 提供システムコール: RSA 署名操作
bool crypt_sys_rsa_sign(char *key_name, void *data, UINT data_size, void *sign, UINT *sign_buf_size)
{
	SE_BUF *ret_buf;
	SE_KEY *key;
	SE_BUF *key_file_buf;
	SE_BUF *tmp_buf;
	void *tmp;
	UINT tmp_size;
	// 引数チェック
	if (key_name == NULL || data == NULL || data_size == 0 || sign == NULL || sign_buf_size == NULL)
	{
		return false;
	}

	chelp_printf("crypt.c: SysRsaSign() is called for key \"%s\".\n", key_name);

	// 秘密鍵ファイルの読み込み
	if (crypt_sys_load_data(key_name, &tmp, &tmp_size) == false)
	{
		chelp_printf("crypt.c: key \"%s\" not found.\n", key_name);
		*sign_buf_size = 0;
		return false;
	}

	key_file_buf = SeNewBuf();
	SeWriteBuf(key_file_buf, tmp, tmp_size);
	crypt_sys_free_data(tmp);

	tmp_buf = SeMemToBuf(key_file_buf->Buf, key_file_buf->Size);
	SeFreeBuf(key_file_buf);

	key = SeBufToKey(tmp_buf, true, true, NULL);

	if (key == NULL)
	{
		key = SeBufToKey(tmp_buf, true, false, NULL);
	}

	if (key == NULL)
	{
		chelp_printf("crypt.c: Loading Key Failed.\n");
		SeFreeBuf(tmp_buf);
		*sign_buf_size = 0;
		return false;
	}

	SeFreeBuf(tmp_buf);

	ret_buf = SeRsaSignWithPadding(data, data_size, key);
	if (ret_buf == NULL)
	{
		chelp_printf("crypt.c: SeRsaSignWithPadding() Failed.\n");
		SeFreeKey(key);
		*sign_buf_size = 0;
		return false;
	}

	if (*sign_buf_size < ret_buf->Size)
	{
		chelp_printf("crypt.c: Not Enough Buffer Space.\n");
		*sign_buf_size = ret_buf->Size;
		SeFreeBuf(ret_buf);
		SeFreeKey(key);
		*sign_buf_size = ret_buf->Size;
		return false;
	}

	*sign_buf_size = ret_buf->Size;

	SeCopy(sign, ret_buf->Buf, ret_buf->Size);
	SeFreeBuf(ret_buf);
	SeFreeKey(key);

	return true;
}

// 提供システムコール: 物理 NIC の情報の取得
void crypt_sys_get_physical_nic_info(SE_HANDLE nic_handle, SE_NICINFO *info)
{
	VPN_NIC	*n = (VPN_NIC *)nic_handle;
	// 引数チェック
	if (n == NULL)
	{
		return;
	}

	SeCopy(info, &n->NicInfo, sizeof(SE_NICINFO));
}

// 送信パケットキューから古いパケットを削除する
void crypt_flush_old_packet_from_queue(SE_QUEUE *q)
{
	UINT64 now = SeTick64();
	UINT num = 0;

	while (true)
	{
		void *data = SePeekNext(q);
		UINT64 *time_stamp;

		if (data == NULL)
		{
			break;
		}

		time_stamp = (UINT64 *)data;

		if (now <= ((*time_stamp) + CRYPT_SEND_PACKET_LIFETIME))
		{
			break;
		}

		data = SeGetNext(q);

		SeFree(data);

		num++;
	}
}

// 提供システムコール: 物理 NIC を用いてパケットを送信
void crypt_sys_send_physical_nic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes)
{
	VPN_NIC	*n = (VPN_NIC *)nic_handle;
	UINT i;
	// 引数チェック
	if (n == NULL || num_packets == 0 || packets == NULL || packet_sizes == NULL)
	{
		return;
	}

	SeLock(n->Lock);
	{
		// パケットをキューに格納する
		for (i = 0;i < num_packets;i++)
		{
			void *packet = packets[i];
			UINT size = packet_sizes[i];

			UCHAR *packet_copy = SeMalloc(size + sizeof(UINT64));
			SeCopy(packet_copy + sizeof(UINT64), packet, size);
			*((UINT64 *)packet_copy) = SeTick64();

			SeInsertQueue(n->SendPacketQueue, packet_copy);
		}

		// 古いパケットを送信キューから削除する
		crypt_flush_old_packet_from_queue(n->SendPacketQueue);
	}
	SeUnlock(n->Lock);
}

// 提供システムコール: 物理 NIC からパケットを受信した際のコールバックを設定
void crypt_sys_set_physical_nic_recv_callback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	VPN_NIC	*n = (VPN_NIC *)nic_handle;
	// 引数チェック
	if (n == NULL)
	{
		return;
	}

	n->RecvCallback = callback;
	n->RecvCallbackParam = param;
}

// 提供システムコール: 仮想 NIC の情報の取得
void crypt_sys_get_virtual_nic_info(SE_HANDLE nic_handle, SE_NICINFO *info)
{
	VPN_NIC	*n = (VPN_NIC *)nic_handle;

	SeCopy(info, &n->NicInfo, sizeof(SE_NICINFO));
}

// 提供システムコール: 仮想 NIC を用いてパケットを送信
void crypt_sys_send_virtual_nic(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes)
{
	VPN_NIC	*n = (VPN_NIC *)nic_handle;
	UINT i;
	// 引数チェック
	if (n == NULL || num_packets == 0 || packets == NULL || packet_sizes == NULL)
	{
		return;
	}

	SeLock(n->Lock);
	{
		// パケットをキューに格納する
		for (i = 0;i < num_packets;i++)
		{
			void *packet = packets[i];
			UINT size = packet_sizes[i];

			UCHAR *packet_copy = SeMalloc(size + sizeof(UINT64));
			SeCopy(packet_copy + sizeof(UINT64), packet, size);
			*((UINT64 *)packet_copy) = SeTick64();

			SeInsertQueue(n->SendPacketQueue, packet_copy);
		}

		// 古いパケットを送信キューから削除する
		crypt_flush_old_packet_from_queue(n->SendPacketQueue);
	}
	SeUnlock(n->Lock);
}

// 提供システムコール: 仮想 NIC からパケットを受信した際のコールバックを設定
void crypt_sys_set_virtual_nic_recv_callback(SE_HANDLE nic_handle, SE_SYS_CALLBACK_RECV_NIC *callback, void *param)
{
	VPN_NIC	*n = (VPN_NIC *)nic_handle;
	// 引数チェック
	if (n == NULL)
	{
		return;
	}

	n->RecvCallback = callback;
	n->RecvCallbackParam = param;
}

// 物理 / 仮想 NIC でパケットを受信したことを通知しパケットデータを渡す
void crypt_nic_recv_packet(VPN_NIC *n, UINT num_packets, void **packets, UINT *packet_sizes)
{
	// 引数チェック
	if (n == NULL || num_packets == 0 || packets == NULL || packet_sizes == NULL)
	{
		return;
	}

	n->RecvCallback((SE_HANDLE)n, num_packets, packets, packet_sizes, n->RecvCallbackParam);
}

// 暗号化モジュールと VPN モジュールの初期化
void crypt_init_crypto_and_vpn()
{
	static SE_SYSCALL_TABLE st;
	printf("crypt_init_crypto_and_vpn(): Started.\n");

	// 暗号化モジュールと VPN モジュールが使用するシステムコールテーブルの定義
	memset(&st, 0, sizeof(st));

	st.SysMemoryAlloc = crypt_sys_memory_alloc;
	st.SysMemoryReAlloc = crypt_sys_memory_realloc;
	st.SysMemoryFree = crypt_sys_memory_free;
	st.SysGetCurrentCpuId = crypt_sys_get_current_cpu_id;
	st.SysNewLock = crypt_sys_new_lock;
	st.SysLock = crypt_sys_lock;
	st.SysUnlock = crypt_sys_unlock;
	st.SysFreeLock = crypt_sys_free_lock;
	st.SysGetTickCount = crypt_sys_get_tick_count;
	st.SysNewTimer = crypt_sys_new_timer;
	st.SysSetTimer = crypt_sys_set_timer;
	st.SysFreeTimer = crypt_sys_free_timer;
	st.SysSaveData = crypt_sys_save_data;
	st.SysLoadData = crypt_sys_load_data;
	st.SysFreeData = crypt_sys_free_data;
	st.SysRsaSign = crypt_sys_rsa_sign;
	st.SysGetPhysicalNicInfo = crypt_sys_get_physical_nic_info;
	st.SysSendPhysicalNic = crypt_sys_send_physical_nic;
	st.SysSetPhysicalNicRecvCallback = crypt_sys_set_physical_nic_recv_callback;
	st.SysGetVirtualNicInfo = crypt_sys_get_virtual_nic_info;
	st.SysSendVirtualNic = crypt_sys_send_virtual_nic;
	st.SysSetVirtualNicRecvCallback = crypt_sys_set_virtual_nic_recv_callback;
	st.SysLog = crypt_sys_log;

	// 暗号化モジュールの初期化
	InitCryptoLibrary(&st, NULL, 0);

#ifdef	CRYPTO_VPN
	// VPN クライアントモジュールの初期化
	VPN_IPsec_Init(&st, false);

	// VPN クライアントの初期化
	crypt_init_vpn();
#endif	// CRYPTO_VPN

	printf("crypt_init_crypto_and_vpn(): Finished.\n");
}
#else /* CRYPTO_VPN */
void
crypt_init_crypto_and_vpn (void)
{
}
#endif /* CRYPTO_VPN */
