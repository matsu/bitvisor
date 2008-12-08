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

// SeKernel.h
// 概要: SeKernel.c のヘッダ

#ifndef	SEKERNEL_H
#define	SEKERNEL_H

// 定数
#define SE_ETH_SENDER_MAC_EXPIRES		SE_TIMESPAN(1, 0, 0)	// 送信 MAC アドレスリストの有効期限

// ロック
struct SE_LOCK
{
	SE_HANDLE LockHandle;
	UINT CpuId;
	UINT LockedCounter;
};

// Ethernet Sender MAC
struct SE_ETH_SENDER_MAC
{
	UINT64 Expires;
	UCHAR MacAddress[SE_ETHERNET_MAC_ADDR_SIZE];
};

// Ethernet
struct SE_ETH
{
	SE_HANDLE NicHandle;
	SE_NICINFO Info;
	SE_ETH_RECV_CALLBACK *RecvCallback;
	void *RecvCallbackParam;
	UINT NicType;
	SE_LOCK *SenderMacListLock;
	SE_LIST *SenderMacList;
	SE_QUEUE *RecvQueue;
	SE_LOCK *RecvQueueLock;
	SE_QUEUE *SendQueue;
	UCHAR MyMacAddress[SE_ETHERNET_MAC_ADDR_SIZE];
	bool IsPromiscusMode;
};

// タイマエントリ
struct SE_TIMER_ENTRY
{
	UINT64 Tick;
};

// タイマ
struct SE_TIMER
{
	SE_HANDLE TimerHandle;
	SE_LIST *TimerEntryList;
	SE_TIMER_CALLBACK *TimerCallback;
	void *TimerCallbackParam;
	SE_LOCK *TimerEntryLock;
	UINT64 CurrentTimerTargetTick;
};

// NIC の種類
#define SE_NIC_PHYSICAL				0	// 物理 NIC
#define SE_NIC_VIRTUAL				1	// 仮想 NIC

// Ethernet パケットの種類
#define SE_ETHER_PACKET_TYPE_VALID			1	// 有効
#define SE_ETHER_PACKET_TYPE_BROADCAST		2	// ブロードキャスト
#define SE_ETHER_PACKET_TYPE_UNICAST		4	// ユニキャスト
#define SE_ETHER_PACKET_TYPE_FOR_ME			8	// 自分自身に対するパケット

// 関数プロトタイプ
void SeInitKernel();
void SeFreeKernel();

SE_TIMER *SeTimerNew(SE_TIMER_CALLBACK *callback, void *callback_param);
int SeTimerEntryCompare(void *p1, void *p2);
void SeTimerCallback(SE_HANDLE timer_handle, void *param);
void SeTimerSet(SE_TIMER *t, UINT interval);
void SeTimerAdjust(SE_TIMER *t);
void SeTimerFree(SE_TIMER *t);

SE_ETH *SeEthNew(SE_HANDLE nic_handle, UINT nic_type, SE_ETH_RECV_CALLBACK *recv_callback, void *recv_callback_param);
void SeEthFree(SE_ETH *e);
void SeEthNicCallback(SE_HANDLE nic_handle, UINT num_packets, void **packets, UINT *packet_sizes, void *param);
void SeEthSend(SE_ETH *e, UINT num_packets, void **packets, UINT *packet_sizes);
void SeEthSendAdd(SE_ETH *e, void *packet, UINT packet_size);
UINT SeEthSendAll(SE_ETH *e);
void SeEthGetInfo(SE_ETH *e, SE_NICINFO *info);
void SeEthDeleteOldSenderMacList(SE_ETH *e);
void SeEthAddSenderMacList(SE_ETH *e, UCHAR *mac_address);
bool SeEthIsSenderMacAddressExistsInList(SE_ETH *e, UCHAR *mac_address);
void *SeEthGetNextRecvPacket(SE_ETH *e);
void SeEthGenRandMacAddress(UCHAR *mac_address, void *key, UINT key_size);
UINT SeEthParseEthernetPacket(void *packet, UINT packet_size, UCHAR *my_mac_addr);

SE_BUF *SeLoad(char *name);
bool SeSave(char *name, void *data, UINT data_size);

void SeLogArgs(char *type, char *fmt, va_list args);
void SeLog(char *type, char *fmt, ...);
void SeDebug(char *fmt, ...);
void SeDebugBin(void *data, UINT size);
void SeFatal(char *fmt, ...);
void SeInfo(char *fmt, ...);
void SeWarning(char *fmt, ...);
void SeError(char *fmt, ...);

UINT SeGetCurrentCpuId();
SE_LOCK *SeNewLock();
void SeLock(SE_LOCK *lock);
void SeUnlock(SE_LOCK *lock);
void SeDeleteLock(SE_LOCK *lock);

UINT SeTick();
UINT64 SeTick64();
UINT64 SeTick64Internal();


#endif	// SEKERNEL_H

