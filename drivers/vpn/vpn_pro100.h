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

#ifndef _CORE_VPN_PRO100_H
#define _CORE_VPN_PRO100_H

#include "io.h"
#include "pci.h"
#define UCHAR	unsigned char
#define UINT	unsigned int
#define DWORD	unsigned int

/// 定数
#define	PRO100_MAX_OP_BLOCK_SIZE				(PRO100_MAX_PACKET_SIZE + 16)
#define	PRO100_MAX_PACKET_SIZE					1568
#define	PRO100_NUM_RECV_BUFFERS					256

#define	PRO100_PCI_CONFIG_32_CSR_MMAP_ADDR_REG	16
#define	PRO100_PCI_CONFIG_32_CSR_IO_ADDR_REG	20
#define	PRO100_CSR_SIZE							64
#define	PRO100_CSR_OFFSET_SCB_STATUS_WORD_0		0
#define	PRO100_CSR_OFFSET_SCB_STATUS_WORD_1		1
#define	PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0	2
#define	PRO100_CSR_OFFSET_SCB_COMMAND_WORD_1	3
#define	PRO100_CSR_OFFSET_SCB_GENERAL_POINTER	4
#define	PRO100_CSR_OFFSET_SCB_PORT				8
#define	PRO100_CSR_OFFSET_SCB_MDI				16

#define	PRO100_CU_CMD_NOOP						0
#define	PRO100_CU_CMD_START						1
#define	PRO100_CU_CMD_RESUME					2
#define	PRO100_CU_CMD_HPQ_START					3
#define	PRO100_CU_CMD_LOAD_DUMP_ADDR			4
#define	PRO100_CU_CMD_DUMP_STAT					5
#define	PRO100_CU_CMD_LOAD_CU_BASE				6
#define	PRO100_CU_CMD_DUMP_AND_RESET_STAT		7
#define	PRO100_CU_CMD_CU_STAT_RESUME			10
#define	PRO100_CU_CMD_HPQ_RESUME				11

#define	PRO100_RU_CMD_NOOP						0
#define	PRO100_RU_CMD_START						1
#define	PRO100_RU_CMD_RESUME					2
#define	PRO100_RU_CMD_RECV_DMA_REDIRECT			3
#define	PRO100_RU_CMD_ABORT						4
#define	PRO100_RU_CMD_LOAD_HEADER_DATA_SIZE		5
#define	PRO100_RU_CMD_LOAD_RU_BASE				6
#define	PRO100_RU_CMD_RBD_RESUME				7

#define	PRO100_CU_OP_NOP						0
#define	PRO100_CU_OP_IA_SETUP					1
#define	PRO100_CU_OP_CONFIG						2
#define	PRO100_CU_OP_MCAST_ADDR_SETUP			3
#define	PRO100_CU_OP_TRANSMIT					4
#define	PRO100_CU_OP_LOAD_MICROCODE				5
#define	PRO100_CU_OP_DUMP						6
#define	PRO100_CU_OP_DIAG						7

// マクロ
#define	PRO100_GET_RU_COMMAND(x)		((x) & 0x07)
#define	PRO100_GET_CU_COMMAND(x)		((((UINT)(x)) >> 4) & 0x0f)
#define	PRO100_MAKE_CU_RU_COMMAND(cu, ru)	(((((cu) & 0x0f) << 4) & 0xf0) | ((ru) & 0x07))

/// 構造体
// RFD
typedef struct
{
	UINT status : 13;
	UINT ok : 1;
	UINT zero1 : 1;
	UINT c : 1;
	UINT zero2 : 3;
	UINT sf : 1;
	UINT h : 1;
	UINT zero3 : 9;
	UINT s : 1;
	UINT el : 1;
	UINT link_address;
	UINT reserved;
	UINT recved_bytes : 14;
	UINT f : 1;
	UINT eof : 1;
	UINT buffer_size : 14;
	UINT zero4 : 2;
	UCHAR data[PRO100_MAX_PACKET_SIZE];
} PRO100_RFD;

// 受信バッファ
typedef struct PRO100_RECV
{
	phys_t rfd_ptr;						// RFD の物理アドレス
	PRO100_RFD *rfd;					// RFD の仮想アドレス
	struct PRO100_RECV *next_recv;		// 次の受信バッファ
} PRO100_RECV;

// コンテキスト
typedef struct
{
	struct pci_device *dev;				// デバイス
	DWORD csr_mm_addr;					// MMIO アドレス
	DWORD csr_io_addr;					// IO ポートアドレス
	int csr_io_handler;					// IO ハンドラ
	void *csr_mm_handler;				// MMIO ハンドラ
	UINT int_mask_guest_set;			// ゲスト OS が設定した割り込みビット
	bool guest_cu_started;				// ゲスト OS によって CU が開始されたかどうか
	bool guest_cu_suspended;			// ゲスト OS によって CU がサスペンドされたかどうか
	UINT guest_last_general_pointer;	// ゲスト OS によって最後にポインタに書かれたデータ
	UINT guest_last_counter_pointer;	// ゲスト OS によって最後にカウンタデータアドレスが書かれたデータ
	phys_t guest_cu_start_pointer;		// 先頭のゲスト OS によるオペレーションへのポインタ
	phys_t guest_cu_current_pointer;	// 現在のゲスト OS によるオペレーションへのポインタ
	phys_t guest_cu_next_pointer;		// 次のゲスト OS によるオペレーションへのポインタ (Suspend 時)
	spinlock_t lock;				// ロック
	UCHAR mac_address[6];				// MAC アドレス
	UCHAR padding1[2];
	bool use_standard_txcb;				// Extended TxCB を使用しない
	bool cu_base_inited;
	bool ru_base_inited;
	bool host_ru_started;				// RU を既に開始したかどうか
	UINT num_recv;						// 受信バッファ数
	PRO100_RECV *recv;					// 受信バッファ配列
	PRO100_RECV *first_recv;			// 最初の受信バッファ
	PRO100_RECV *last_recv;				// 最後の受信バッファ
	PRO100_RECV *current_recv;			// 現在注目している受信バッファ
	phys_t guest_rfd_first;				// ゲスト OS が指定した最初の受信バッファ
	phys_t guest_rfd_current;			// 現在注目している受信バッファ
	bool guest_ru_suspended;			// ゲスト OS によって RU がサスペンドされたかどうか
	bool vpn_inited;					// VPN Client が初期化されているかどうか
	struct netdata *net_handle;			// VPN Client のハンドル
	net_recv_callback_t *CallbackRecvPhyNic;	// 物理 NIC からパケットを受信した際のコールバック
	void *CallbackRecvPhyNicParam;			// 物理 NIC からパケットを受信した際のコールバックのパラメータ
	net_recv_callback_t *CallbackRecvVirtNic;	// ゲスト OS がパケットを送信しようとした際のコールバック
	void *CallbackRecvVirtNicParam;			// ゲスト OS がパケットを送信しようとした際のコールバックのパラメータ
} PRO100_CTX;

// STAT/ACK レジスタ
typedef struct
{
	UINT fcp : 1;
	UINT reserved : 1;
	UINT swi : 1;
	UINT mdi : 1;
	UINT rnr : 1;
	UINT cna : 1;
	UINT fr : 1;
	UINT cx_tno : 1;
} PRO100_STAT_ACK;

// SCB ステータスワードビット
typedef struct
{
	UINT reserved1 : 1;
	UINT ru_status : 4;
	UINT cu_status : 3;
} PRO100_SCB_STATUS_WORD_BIT;

// 割り込み制御ビット
typedef struct
{
	UINT mask_all : 1;
	UINT si : 1;
	UINT fcp : 1;
	UINT er : 1;
	UINT rnr : 1;
	UINT cna : 1;
	UINT fr : 1;
	UINT cx : 1;
} PRO100_INT_BIT;

// オペレーションブロック
typedef struct
{
	UINT reserved1 : 8;
	UINT reserved2 : 4;
	UINT transmit_overrun : 1;
	UINT ok : 1;
	UINT reserved3 : 1;
	UINT c : 1;
	UINT op : 3;
	UINT transmit_flexible_mode : 1;
	UINT transmit_raw_packet : 1;
	UINT reserved4 : 3;
	UINT transmit_cid : 5;
	UINT i : 1;
	UINT s : 1;
	UINT el : 1;
	UINT link_address;
} PRO100_OP_BLOCK;

// コマンドブロック (最大サイズ)
typedef struct
{
	PRO100_OP_BLOCK op_block;
	UCHAR data[PRO100_MAX_OP_BLOCK_SIZE - sizeof(PRO100_OP_BLOCK)];
} PRO100_OP_BLOCK_MAX;

// マルチキャストアドレスセットアップオペレーション
typedef struct
{
	PRO100_OP_BLOCK op_block;
	UINT count : 14;
	UINT reserved : 2;
} PRO100_OP_MCAST_ADDR_SETUP;

// 送信オペレーション
typedef struct
{
	PRO100_OP_BLOCK op_block;
	UINT tbd_array_address;
	UINT data_bytes : 14;
	UINT zero : 1;
	UINT eof : 1;
	UINT threshold : 8;
	UINT tbd_count : 8;
} PRO100_OP_TRANSMIT;

// TBD
typedef struct
{
	UINT data_address;
	UINT size : 15;
	UINT zero1 : 1;
	UINT el : 1;
	UINT zero2 : 15;
} PRO100_TBD;

// 関数プロトタイプ
void pro100_init();
void pro100_new(struct pci_device *pci_device);
void pro100_init_recv_buffer(PRO100_CTX *ctx);
void pro100_alloc_recv_buffer(PRO100_CTX *ctx);
int pro100_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
			union mem *data);
int pro100_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
			 union mem *data);
int pro100_io_handler(core_io_t io, union mem *data, void *arg);
int pro100_mm_handler(void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags);
bool pro100_hook_write(PRO100_CTX *ctx, UINT offset, UINT data, UINT size);
bool pro100_hook_read(PRO100_CTX *ctx, UINT offset, UINT size, UINT *data);
void pro100_write(PRO100_CTX *ctx, UINT offset, UINT data, UINT size);
void pro100_flush(PRO100_CTX *ctx);
UINT pro100_read(PRO100_CTX *ctx, UINT offset, UINT size);
void pro100_mem_read(void *buf, phys_t addr, UINT size);
void pro100_mem_write(phys_t addr, void *buf, UINT size);
void pro100_proc_guest_op(PRO100_CTX *ctx);
void pro100_wait_cu_ru_accepable(PRO100_CTX *ctx);
void pro100_exec_cu_op(PRO100_CTX *ctx, PRO100_OP_BLOCK_MAX *op, UINT size);
void pro100_init_cu_base_addr(PRO100_CTX *ctx);
void pro100_init_ru_base_addr(PRO100_CTX *ctx);
UINT pro100_get_op_size(UINT op, void *data);
void pro100_generate_int(PRO100_CTX *ctx);
void *pro100_alloc_page(phys_t *ptr);
void pro100_free_page(void *v, phys_t ptr);
void pro100_beep(UINT freq, UINT msecs);
void pro100_sleep(UINT msecs);
UINT pro100_read_send_packet(PRO100_CTX *ctx, phys_t addr, void *buf);
void pro100_send_packet_to_line(PRO100_CTX *ctx, void *buf, UINT size);
void pro100_poll_ru(PRO100_CTX *ctx);
void pro100_write_recv_packet(PRO100_CTX *ctx, void *buf, UINT size);

char *pro100_get_ru_command_string(UINT ru);
char *pro100_get_cu_command_string(UINT cu);
void pro100_get_stat_ack_string(char *str, UCHAR value);
void pro100_get_int_bit_string(char *str, UCHAR value);

PRO100_CTX *pro100_get_ctx();
void pro100_init_vpn_client(PRO100_CTX *ctx);

#endif	// _CORE_VPN_PRO100_H

