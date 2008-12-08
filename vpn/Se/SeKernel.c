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

// SeKernel.c
// 概要: カーネル系操作

#define SE_INTERNAL
#include <Se/Se.h>

// カーネル系ライブラリの初期化
void SeInitKernel()
{
	rt->TickLock = SeNewLock();

	rt->StartTick64 = SeTick64Internal();
}

// カーネル系ライブラリの解放
void SeFreeKernel()
{
	SeDeleteLock(rt->TickLock);
}

// タイマエントリの比較関数
int SeTimerEntryCompare(void *p1, void *p2)
{
	SE_TIMER_ENTRY *e1, *e2;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	e1 = *(SE_TIMER_ENTRY **)p1;
	e2 = *(SE_TIMER_ENTRY **)p2;
	if (e1 == NULL || e2 == NULL)
	{
		return 0;
	}

	if (e1->Tick > e2->Tick)
	{
		return 1;
	}
	else if (e1->Tick < e2->Tick)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

// タイマの解放
void SeTimerFree(SE_TIMER *t)
{
	UINT i;
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	SeSysFreeTimer(t->TimerHandle);

	for (i = 0;i < SE_LIST_NUM(t->TimerEntryList);i++)
	{
		SE_TIMER_ENTRY *e = SE_LIST_DATA(t->TimerEntryList, i);

		SeFree(e);
	}

	SeFreeList(t->TimerEntryList);
	SeDeleteLock(t->TimerEntryLock);

	SeFree(t);
}

// タイマのコールバック関数
void SeTimerCallback(SE_HANDLE timer_handle, void *param)
{
	SE_TIMER *t;
	bool invoke_callback = false;
	UINT64 now = 0;
	// 引数チェック
	if (timer_handle == NULL || param == NULL)
	{
		return;
	}

	t = (SE_TIMER *)param;

	SeLock(t->TimerEntryLock);
	{
		UINT i;
		SE_LIST *o = NULL;

		now = SeTick64();

		// 登録されているタイマエントリのうち現在時刻またはそれよりも前に
		// 発動すべきもののリストを取得する
		for (i = 0;i < SE_LIST_NUM(t->TimerEntryList);i++)
		{
			SE_TIMER_ENTRY *e = SE_LIST_DATA(t->TimerEntryList, i);

			if (now >= e->Tick)
			{
				if (o == NULL)
				{
					o = SeNewList(NULL);
				}

				SeAdd(o, e);
			}
		}

		if (o != NULL)
		{
			// 不要になったタイマエントリをリストから削除する
			for (i = 0;i < SE_LIST_NUM(o);i++)
			{
				SE_TIMER_ENTRY *e = SE_LIST_DATA(o, i);

				SeDelete(t->TimerEntryList, e);

				SeFree(e);
			}

			SeFreeList(o);

			t->CurrentTimerTargetTick = 0;

			invoke_callback = true;
		}

		// タイマ調整
		SeTimerAdjust(t);
	}
	SeUnlock(t->TimerEntryLock);

	if (invoke_callback)
	{
		// コールバック関数を呼び出す
		t->TimerCallback(t, now, t->TimerCallbackParam);
	}
}

// タイマの調整
void SeTimerAdjust(SE_TIMER *t)
{
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	SeLock(t->TimerEntryLock);
	{
		UINT num = SE_LIST_NUM(t->TimerEntryList);

		if (num >= 1)
		{
			SE_TIMER_ENTRY *e = SE_LIST_DATA(t->TimerEntryList, 0);

			// タイマの状態が変化したかどうか調べる
			if (e->Tick != t->CurrentTimerTargetTick)
			{
				UINT64 now = SeTick64();
				UINT64 interval64;
				UINT interval;

				// 先頭のタイマエントリまでの時間を求める
				if (e->Tick > now)
				{
					interval64 = e->Tick - now;
				}
				else
				{
					interval64 = 0;
				}

				interval = (UINT)interval64;
				if (interval == 0)
				{
					interval = 1;
				}

				t->CurrentTimerTargetTick = e->Tick;

				// タイマを設定する
				SeSysSetTimer(t->TimerHandle, interval);
			}
		}
	}
	SeUnlock(t->TimerEntryLock);
}

// タイマのセット
void SeTimerSet(SE_TIMER *t, UINT interval)
{
	SE_TIMER_ENTRY *e;
	// 引数チェック
	if (t == NULL)
	{
		return;
	}

	// タイマエントリの挿入
	SeLock(t->TimerEntryLock);
	{
		SE_TIMER_ENTRY st;
		UINT64 target_tick = SeTick64() + (UINT64)interval;

		SeZero(&st, sizeof(st));
		st.Tick = target_tick;

		// 同一の Tick を指すタイマエントリが存在するかどうかチェックする
		if (SeSearch(t->TimerEntryList, &st) == NULL)
		{
			// タイマエントリの新規作成
			e = SeZeroMalloc(sizeof(SE_TIMER_ENTRY));
			e->Tick = target_tick;

			// 挿入
			SeInsert(t->TimerEntryList, e);

			// タイマを調整
			SeTimerAdjust(t);
		}
	}
	SeUnlock(t->TimerEntryLock);
}

// タイマの新規作成
SE_TIMER *SeTimerNew(SE_TIMER_CALLBACK *callback, void *callback_param)
{
	SE_TIMER *t;
	// 引数チェック
	if (callback == NULL)
	{
		return NULL;
	}

	t = SeZeroMalloc(sizeof(SE_TIMER));

	t->TimerHandle = SeSysNewTimer(SeTimerCallback, t);
	t->TimerEntryList = SeNewList(SeTimerEntryCompare);

	t->TimerCallback = callback;
	t->TimerCallbackParam = callback_param;

	t->TimerEntryLock = SeNewLock();

	return t;
}

// ETH の情報を取得
void SeEthGetInfo(SE_ETH *e, SE_NICINFO *info)
{
	// 引数チェック
	if (e == NULL || info == NULL)
	{
		return;
	}

	SeCopy(info, &e->Info, sizeof(SE_ETH));
}

// ETH の解放
void SeEthFree(SE_ETH *e)
{
	UINT i;
	void *p;
	// 引数チェック
	if (e == NULL)
	{
		return;
	}

	for (i = 0;i < SE_LIST_NUM(e->SenderMacList);i++)
	{
		SE_ETH_SENDER_MAC *m = SE_LIST_DATA(e->SenderMacList, i);

		SeFree(m);
	}

	while ((p = SeGetNext(e->RecvQueue)) != NULL)
	{
		SeFree(p);
	}

	while ((p = SeGetNext(e->SendQueue)) != NULL)
	{
		SeFree(p);
	}

	SeFreeQueue(e->RecvQueue);
	SeFreeQueue(e->SendQueue);
	SeDeleteLock(e->RecvQueueLock);

	SeFreeList(e->SenderMacList);

	SeDeleteLock(e->SenderMacListLock);

	SeFree(e);
}

// 古い MAC アドレスをリストから削除
void SeEthDeleteOldSenderMacList(SE_ETH *e)
{
	UINT64 now;
	// 引数チェック
	if (e == NULL)
	{
		return;
	}

	now = SeTick64();

	SeLock(e->SenderMacListLock);
	{
		UINT i;
		SE_LIST *o = NULL;

		for (i = 0;i < SE_LIST_NUM(e->SenderMacList);i++)
		{
			SE_ETH_SENDER_MAC *m = SE_LIST_DATA(e->SenderMacList, i);

			if (m->Expires <= now)
			{
				if (o == NULL)
				{
					o = SeNewList(NULL);
				}

				SeAdd(o, m);
			}
		}

		if (o != NULL)
		{
			for (i = 0;i < SE_LIST_NUM(o);i++)
			{
				SE_ETH_SENDER_MAC *m = SE_LIST_DATA(o, i);

				SeDelete(e->SenderMacList, m);

				SeFree(m);
			}

			SeFreeList(o);
		}
	}
	SeUnlock(e->SenderMacListLock);
}

// MAC アドレスをリストへ追加
void SeEthAddSenderMacList(SE_ETH *e, UCHAR *mac_address)
{
	// 引数チェック
	if (e == NULL || mac_address == NULL)
	{
		return;
	}

	SeLock(e->SenderMacListLock);
	{
		UINT i;
		bool exists = false;

		for (i = 0;i < SE_LIST_NUM(e->SenderMacList);i++)
		{
			SE_ETH_SENDER_MAC *m = SE_LIST_DATA(e->SenderMacList, i);

			if (SeCmp(m->MacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE) == 0)
			{
				m->Expires = SeTick64() + (UINT64)SE_ETH_SENDER_MAC_EXPIRES;

				exists = true;
			}
		}

		if (exists == false)
		{
			SE_ETH_SENDER_MAC *m = SeZeroMalloc(sizeof(SE_ETH_SENDER_MAC));

			m->Expires = SeTick64() + (UINT64)SE_ETH_SENDER_MAC_EXPIRES;

			SeCopy(m->MacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE);

			SeAdd(e->SenderMacList, m);
		}

		SeEthDeleteOldSenderMacList(e);
	}
	SeUnlock(e->SenderMacListLock);
}

// 指定した MAC アドレスがリストに登録されているかどうかチェックする
bool SeEthIsSenderMacAddressExistsInList(SE_ETH *e, UCHAR *mac_address)
{
	bool ret = false;
	// 引数チェック
	if (e == NULL || mac_address == NULL)
	{
		return false;
	}

	SeLock(e->SenderMacListLock);
	{
		UINT i;
		bool exists = false;

		for (i = 0;i < SE_LIST_NUM(e->SenderMacList);i++)
		{
			SE_ETH_SENDER_MAC *m = SE_LIST_DATA(e->SenderMacList, i);

			if (SeCmp(m->MacAddress, mac_address, SE_ETHERNET_MAC_ADDR_SIZE) == 0)
			{
				m->Expires = SeTick64() + (UINT64)SE_ETH_SENDER_MAC_EXPIRES;

				ret = true;
			}
		}

		SeEthDeleteOldSenderMacList(e);
	}
	SeUnlock(e->SenderMacListLock);

	return ret;
}

// NIC の送信キューの追加
void SeEthSendAdd(SE_ETH *e, void *packet, UINT packet_size)
{
	// 引数チェック
	if (e == NULL || packet == NULL)
	{
		return;
	}

	SeInsertQueue(e->SendQueue, SeClone(packet, packet_size));
}

// NIC の送信キューに溜まっているパケットを全部送信
UINT SeEthSendAll(SE_ETH *e)
{
	UINT num_packet;
	UINT *packet_sizes;
	void **packets;
	UINT i;
	void *p;
	// 引数チェック
	if (e == NULL)
	{
		return 0;
	}

	num_packet = e->SendQueue->num_item;

	if (num_packet == 0)
	{
		return 0;
	}

	packet_sizes = SeMalloc(sizeof(UINT) * num_packet);
	packets = SeMalloc(sizeof(void *) * num_packet);

	i = 0;
	while ((p = SeGetNext(e->SendQueue)) != NULL)
	{
		UINT size = SeMemSize(p);

		packets[i] = p;
		packet_sizes[i] = size;

		i++;
	}

	SeEthSend(e, num_packet, packets, packet_sizes);

	for (i = 0;i < num_packet;i++)
	{
		void *p = packets[i];

		SeFree(p);
	}

	SeFree(packet_sizes);
	SeFree(packets);

	return num_packet;
}

// NIC へ送信
void SeEthSend(SE_ETH *e, UINT num_packets, void **packets, UINT *packet_sizes)
{
	UINT i;
	// 引数チェック
	if (e == NULL)
	{
		return;
	}

	for (i = 0;i < num_packets;i++)
	{
		UINT size = packet_sizes[i];
		UCHAR *packet = (UCHAR *)packets[i];

		if (size >= SE_ETHERNET_HEADER_SIZE)
		{
			UCHAR *mac = packet + 6;

			SeEthAddSenderMacList(e, mac);
		}
	}

	if (e->NicType == SE_NIC_PHYSICAL)
	{
		SeSysSendPhysicalNic(e->NicHandle, num_packets, packets, packet_sizes);
	}
	else
	{
		SeSysSendVirtualNic(e->NicHandle, num_packets, packets, packet_sizes);
	}
}

// パケットの解析
UINT SeEthParseEthernetPacket(void *packet, UINT packet_size, UCHAR *my_mac_addr)
{
	UCHAR *p = (UCHAR *)packet;
	UINT ret = 0;
	UCHAR *mac_src = p + 6;
	UCHAR *mac_dst = p;

	if (packet == NULL)
	{
		return 0;
	}

	if (packet_size >= SE_ETHERNET_HEADER_SIZE && packet_size <= SE_ETHERNET_MAX_PACKET_SIZE)
	{
		bool is_broadcast = true;
		bool b1 = true, b2 = true;
		bool invalid = false;
		UINT i;

		for (i = 0;i < 6;i++)
		{
			if (mac_dst[i] != 0xff)
			{
				is_broadcast = false;
			}
			if (mac_src[i] != 0xff)
			{
				b1 = false;
			}
			if (mac_src[i] != 0x00)
			{
				b2 = false;
			}
		}

		if (b1 || b2 || SeCmp(mac_src, mac_dst, 6) == 0)
		{
			invalid = true;
		}

		if (mac_dst[0] & 0x01)
		{
			is_broadcast = true;
		}

		if (invalid == false)
		{
			ret |= SE_ETHER_PACKET_TYPE_VALID;
		}

		if (is_broadcast)
		{
			ret |= SE_ETHER_PACKET_TYPE_BROADCAST;
		}
		else
		{
			ret |= SE_ETHER_PACKET_TYPE_UNICAST;
		}
	}

	if (ret & SE_ETHER_PACKET_TYPE_VALID)
	{
		if (ret & SE_ETHER_PACKET_TYPE_UNICAST)
		{
			if (my_mac_addr != NULL)
			{
				if (SeCmp(my_mac_addr, mac_dst, SE_ETHERNET_MAC_ADDR_SIZE) == 0)
				{
					ret |= SE_ETHER_PACKET_TYPE_FOR_ME;
				}
			}
		}
		else if (ret & SE_ETHER_PACKET_TYPE_BROADCAST)
		{
			ret |= SE_ETHER_PACKET_TYPE_FOR_ME;
		}
	}

	return ret;
}

// NIC からの受信コールバック関数
void SeEthNicCallback(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes, void *param)
{
	SE_ETH *e;
	UINT i;
	UINT num_insert_packets;
	// 引数チェック
	if (nic_handle == NULL || param == NULL)
	{
		return;
	}

	e = (SE_ETH *)param;
	num_insert_packets = 0;

	SeLock(e->RecvQueueLock);
	{
		for (i = 0;i < num_packets;i++)
		{
			UINT size = packet_sizes[i];
			UCHAR *packet = (UCHAR *)packets[i];

			if (size >= SE_ETHERNET_HEADER_SIZE)
			{
				UCHAR *src_mac = packet + 6;
				UCHAR *dst_mac = packet;

				if (SeEthIsSenderMacAddressExistsInList(e, src_mac) == false)
				{
					bool b = false;
					UINT type = SeEthParseEthernetPacket(packet, size, e->MyMacAddress);

					if (type & SE_ETHER_PACKET_TYPE_VALID)
					{
						if (e->IsPromiscusMode)
						{
							b = true;
						}
						else
						{
							if (type & SE_ETHER_PACKET_TYPE_FOR_ME)
							{
								b = true;
							}
						}
					}

					if (b)
					{
						SeInsertQueue(e->RecvQueue, SeClone(packet, size));
						num_insert_packets++;
					}
				}
			}
		}
	}
	SeUnlock(e->RecvQueueLock);

	if (num_insert_packets != 0)
	{
		e->RecvCallback(e, e->RecvCallbackParam);
	}
}

// 次の受信パケットの取得
void *SeEthGetNextRecvPacket(SE_ETH *e)
{
	void *ret;
	// 引数チェック
	if (e == NULL)
	{
		return NULL;
	}

	SeLock(e->RecvQueueLock);
	{
		ret = SeGetNext(e->RecvQueue);
	}
	SeUnlock(e->RecvQueueLock);

	return ret;
}

// ランダムな MAC アドレスの生成
void SeEthGenRandMacAddress(UCHAR *mac_address, void *key, UINT key_size)
{
	UCHAR hash[SE_SHA1_HASH_SIZE];
	// 引数チェック
	if (mac_address == NULL)
	{
		return;
	}

	if (key == NULL || key_size == 0)
	{
		SeRand(hash, sizeof(hash));
	}
	else
	{
		SeSha1(hash, key, key_size);
	}

	SeCopy(mac_address, hash, SE_ETHERNET_MAC_ADDR_SIZE);

	mac_address[0] = 0x00;
	mac_address[1] = 0xAC;
}

// 新しい ETH の作成
SE_ETH *SeEthNew(SE_HANDLE nic_handle, UINT nic_type, SE_ETH_RECV_CALLBACK *recv_callback, void *recv_callback_param)
{
	SE_ETH *e;
	// 引数チェック
	if (nic_handle == NULL || recv_callback == NULL)
	{
		return NULL;
	}

	e = SeZeroMalloc(sizeof(SE_ETH));
	e->NicHandle = nic_handle;
	e->RecvCallback = recv_callback;
	e->RecvCallbackParam = recv_callback_param;
	e->NicType = nic_type;
	e->SenderMacList = SeNewList(NULL);
	e->SenderMacListLock = SeNewLock();
	e->RecvQueue = SeNewQueue();
	e->RecvQueueLock = SeNewLock();
	e->SendQueue = SeNewQueue();
	e->IsPromiscusMode = true;

	SeEthGenRandMacAddress(e->MyMacAddress, NULL, 0);

	if (e->NicType == SE_NIC_PHYSICAL)
	{
		SeSysGetPhysicalNicInfo(nic_handle, &e->Info);
		SeSysSetPhysicalNicRecvCallback(nic_handle, SeEthNicCallback, e);
	}
	else
	{
		SeSysGetVirtualNicInfo(nic_handle, &e->Info);
		SeSysSetVirtualNicRecvCallback(nic_handle, SeEthNicCallback, e);
	}

	return e;
}

// データの読み込み
SE_BUF *SeLoad(char *name)
{
	SE_BUF *b;
	void *data;
	UINT data_size;
	// 引数チェック
	if (name == NULL)
	{
		return NULL;
	}

	if (SeSysLoadData(name, &data, &data_size) == false)
	{
		return NULL;
	}

	b = SeNewBuf();
	SeWriteBuf(b, data, data_size);
	SeSeekBuf(b, 0, 0);

	SeSysFreeData(data);

	return b;
}

// データの書き込み
bool SeSave(char *name, void *data, UINT data_size)
{
	bool ret;
	// 引数チェック
	if (name == NULL || data == NULL)
	{
		return false;
	}

	ret = SeSysSaveData(name, data, data_size);

	return ret;
}

// 64 bit Tick 値の取得
UINT64 SeTick64Internal()
{
	UINT64 ret;

	SeLock(rt->TickLock);
	{
		UINT tick = SeTick();

		if (rt->LastTick > tick)
		{
			// Tick の値が 1 周した
			rt->TickRoundCounter++;
		}

		rt->LastTick = tick;

		ret = (UINT64)tick + (UINT64)rt->TickRoundCounter * 4294967296ULL;
	}
	SeUnlock(rt->TickLock);

	return ret;
}
UINT64 SeTick64()
{
	UINT64 tick64_internal = SeTick64Internal();

	return (tick64_internal - rt->StartTick64) + 1ULL;
}

// Tick 値の取得
UINT SeTick()
{
	return SeSysGetTickCount();
}

// 現在の CPU ID の取得
UINT SeGetCurrentCpuId()
{
	return SeSysGetCurrentCpuId();
}

// ロックの作成
SE_LOCK *SeNewLock()
{
	SE_LOCK *lock = SeZeroMalloc(sizeof(SE_LOCK));

	lock->LockHandle = SeSysNewLock();
	lock->LockedCounter = 0;
	lock->CpuId = INFINITE;

	return lock;
}

// ロック
void SeLock(SE_LOCK *lock)
{
	UINT cpu_id;
	// 引数チェック
	if (lock == NULL)
	{
		return;
	}

	cpu_id = SeGetCurrentCpuId();

	if (lock->CpuId == cpu_id)
	{
		lock->LockedCounter++;
		return;
	}

	SeSysLock(lock->LockHandle);

	lock->CpuId = cpu_id;
	lock->LockedCounter++;

	return;
}

// ロック解除
void SeUnlock(SE_LOCK *lock)
{
	// 引数チェック
	if (lock == NULL)
	{
		return;
	}

	if ((--lock->LockedCounter) > 0)
	{
		return;
	}

	lock->CpuId = INFINITE;

	SeSysUnlock(lock->LockHandle);
}

// ロックの削除
void SeDeleteLock(SE_LOCK *lock)
{
	// 引数チェック
	if (lock == NULL)
	{
		return;
	}

	SeSysFreeLock(lock->LockHandle);

	SeFree(lock);
}

// デバッグ出力
void SeDebug(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	SeLogArgs(SE_LOG_DEBUG, fmt, args);

	va_end(args);
}

// その他のログ出力
void SeFatal(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	SeLogArgs(SE_LOG_FATAL, fmt, args);

	va_end(args);
}
void SeInfo(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	SeLogArgs(SE_LOG_INFO, fmt, args);

	va_end(args);
}
void SeError(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	SeLogArgs(SE_LOG_ERROR, fmt, args);

	va_end(args);
}
void SeWarning(char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	SeLogArgs(SE_LOG_WARNING, fmt, args);

	va_end(args);
}

// バイナリデータのデバッグ出力
void SeDebugBin(void *data, UINT size)
{
	char *tmp;
	UINT tmp_size;
	// 引数チェック
	if (data == NULL)
	{
		return;
	}

	tmp_size = (size + 2) * 3;
	tmp = SeMalloc(tmp_size);

	SeBinToStrEx(tmp, tmp_size, data, size);

	SeDebug("%s", tmp);

	SeFree(tmp);
}

// ログの出力
void SeLogArgs(char *type, char *fmt, va_list args)
{
	char *data = SeFormatEx(fmt, args);

	SeSysLog(type, data);

	SeFree(data);
}
void SeLog(char *type, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	SeLogArgs(type, fmt, args);

	va_end(args);
}


