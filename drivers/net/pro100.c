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

#include "pci.h"
#include <core.h>
#include <core/mmio.h>
#include <core/time.h>
#include <net/netapi.h>
#include <Se/Se.h>
#include "pro100.h"
#include "../../core/beep.h"	/* DEBUG */

#define SeCopy			memcpy
#define SeZero(addr, len)	memset (addr, 0, len)
#define SeZeroMalloc(len)	memset (alloc (len), 0, len)
#define SeMalloc		alloc
#define SeFree			free

#ifdef VTD_TRANS
#include "passthrough/vtd.h"
int add_remap() ;
u32 vmm_start_inf() ;
u32 vmm_term_inf() ;
#endif // of VTD_TRANS

//#define	PRO100_PASS_MODE

//#define	debugprint(fmt, args...) printf(fmt, ## args); pro100_sleep(70);

#define	debugprint(fmt, args...) do if (0) printf (fmt, ## args); while (0)


static PRO100_CTX *pro100_ctx = NULL;

static const char driver_name[] = "pro100";
static const char driver_longname[] = "VPN for Intel PRO/100";

static void
GetPhysicalNicInfo (void *handle, struct nicinfo *info)
{
	info->mtu = 1500;
	info->media_speed = 1000000000;
	SeCopy (info->mac_address, pro100_get_ctx()->mac_address, 6);
}

static void
SendPhysicalNic (void *handle, unsigned int num_packets, void **packets,
		 unsigned int *packet_sizes, bool print_ok)
{
	if (true)
	{
		UINT i;
		for (i = 0;i < num_packets;i++)
		{
			void *data = packets[i];
			UINT size = packet_sizes[i];

			pro100_send_packet_to_line (handle, data, size);
		}
	}
}

static void
SetPhysicalNicRecvCallback (void *handle, net_recv_callback_t *callback,
			    void *param)
{
	if (true)
	{
		PRO100_CTX *ctx = handle;

		ctx->CallbackRecvPhyNic = callback;
		ctx->CallbackRecvPhyNicParam = param;
	}
}

static void
GetVirtualNicInfo (void *handle, struct nicinfo *info)
{
	info->mtu = 1500;
	info->media_speed = 1000000000;
	SeCopy(info->mac_address, pro100_get_ctx()->mac_address, 6);
}

static void
SendVirtualNic (void *handle, unsigned int num_packets, void **packets,
		unsigned int *packet_sizes, bool print_ok)
{
	if (true)
	{
		UINT i;
		for (i = 0;i < num_packets;i++)
		{
			void *data = packets[i];
			UINT size = packet_sizes[i];

			pro100_write_recv_packet (handle, data, size);
		}
	}
}

static void
SetVirtualNicRecvCallback (void *handle, net_recv_callback_t *callback,
			   void *param)
{
	if (true)
	{
		PRO100_CTX *ctx = handle;

		ctx->CallbackRecvVirtNic = callback;
		ctx->CallbackRecvVirtNicParam = param;
	}
}

static struct nicfunc phys_func = {
	.get_nic_info = GetPhysicalNicInfo,
	.send = SendPhysicalNic,
	.set_recv_callback = SetPhysicalNicRecvCallback,
}, virt_func = {
	.get_nic_info = GetVirtualNicInfo,
	.send = SendVirtualNic,
	.set_recv_callback = SetVirtualNicRecvCallback,
};

PRO100_CTX *pro100_get_ctx()
{
	if (pro100_ctx == NULL)
	{
		debugprint("Error: No PRO/100 Devices!\n");
		pro100_beep(1234, 10000);
		while (true);
	}

	return pro100_ctx;
}

static void
mmio_hphys_access (phys_t gphysaddr, bool wr, void *buf, uint len, u32 flags)
{
	void *p;

	if (!len)
		return;
	p = mapmem_hphys (gphysaddr, len, (wr ? MAPMEM_WRITE : 0) | flags);
	ASSERT (p);
	if (wr)
		memcpy (p, buf, len);
	else
		memcpy (buf, p, len);
	unmapmem (p, len);
}

static void
mmio_gphys_access (phys_t gphysaddr, bool wr, void *buf, uint len, u32 flags)
{
	void *p;

	if (!len)
		return;
	p = mapmem_gphys (gphysaddr, len, (wr ? MAPMEM_WRITE : 0) | flags);
	ASSERT (p);
	if (wr)
		memcpy (p, buf, len);
	else
		memcpy (buf, p, len);
	unmapmem (p, len);
}

// VPN Client の初期化
void pro100_init_vpn_client(PRO100_CTX *ctx)
{
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	if (ctx->vpn_inited)
	{
		// すでに初期化されている
		return;
	}

	// VPN Client の初期化
	//ctx->vpn_handle = VPN_IPsec_Client_Start((SE_HANDLE)ctx, (SE_HANDLE)ctx, "config.txt");
	net_init (ctx->net_handle, ctx, &phys_func, ctx, &virt_func);
	net_start (ctx->net_handle);

	ctx->vpn_inited = true;
}

// BEEP 再生
void pro100_beep(UINT freq, UINT msecs)
{
	beep_on();
	beep_set_freq(freq);
	pro100_sleep(msecs);
	beep_off();
}

// スリープ
void pro100_sleep(UINT msecs)
{
	UINT64 tick;
	if (msecs == 0)
	{
		return;
	}

	tick = get_time () + msecs * 1000;

	while (get_time () <= tick);
}

// ページの確保
void *pro100_alloc_page(phys_t *ptr)
{
	void *vptr;
	void *vptr2;
	phys_t pptr;

	alloc_page(&vptr, &pptr);

	vptr2 = mapmem_hphys(pptr, PAGESIZE, MAPMEM_WRITE | MAPMEM_PCD | MAPMEM_PWT | 0/*MAPMEM_PAT*/);

	*ptr = pptr;

	return vptr2;
}

// ページの解放
void pro100_free_page(void *v, phys_t ptr)
{
	unmapmem(v, PAGESIZE);
	free_page_phys(ptr);
}

// CU のベースアドレスの初期化を実行
void pro100_init_cu_base_addr(PRO100_CTX *ctx)
{
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	if (ctx->cu_base_inited)
	{
		return;
	}

	ctx->cu_base_inited = true;

	pro100_wait_cu_ru_accepable(ctx);
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, 0, 4);
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0,
		PRO100_MAKE_CU_RU_COMMAND(PRO100_CU_CMD_LOAD_CU_BASE, PRO100_RU_CMD_NOOP),
		1);
	pro100_wait_cu_ru_accepable(ctx);
}

// RU のベースアドレスの初期化を実行
void pro100_init_ru_base_addr(PRO100_CTX *ctx)
{
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}
	if (ctx->ru_base_inited)
	{
		return;
	}

	ctx->ru_base_inited = true;

	pro100_wait_cu_ru_accepable(ctx);
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, 0, 4);
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0,
		PRO100_MAKE_CU_RU_COMMAND(PRO100_CU_CMD_NOOP, PRO100_RU_CMD_LOAD_RU_BASE),
		1);
	pro100_wait_cu_ru_accepable(ctx);
}

// パケットを受信したことにしてゲスト OS に渡す
void pro100_write_recv_packet(PRO100_CTX *ctx, void *buf, UINT size)
{
	PRO100_RFD rfd;
	phys_t ptr;
	// 引数チェック
	if (ctx == NULL || buf == NULL || size == 0)
	{
		return;
	}

	if (ctx->guest_rfd_current == 0)
	{
		// ゲスト OS が RFD のアドレスを指定していない
		return;
	}
/*
	if (ctx->guest_ru_suspended)
	{
		// ゲスト OS によって RU がサスペンドされている
		return;
	}*/

	// 現在の RFD バッファの内容をチェックする
	ptr = ctx->guest_rfd_current;
	pro100_mem_read(&rfd, ptr, sizeof(PRO100_RFD));

	if (rfd.eof)
	{
		// EOF ビットが有効になっているためこの RFD にはデータを受信できない
		return;
	}

	// RFD にデータを書き込む
	rfd.status = 0;
	rfd.sf = rfd.h = 0;

	SeCopy(rfd.data, buf, size);

	rfd.recved_bytes = size;
	rfd.f = 1;
	rfd.ok	= 1;
	rfd.c = 1;
	rfd.eof = 1;

	// ビットチェック
	if (rfd.el)
	{
		// これが最後の RFD である
		ctx->guest_rfd_current = ctx->guest_rfd_first = 0;
	}
	else
	{
		// 次の RFD のリンクアドレスを取得する
		ctx->guest_rfd_current = rfd.link_address;
		if (ctx->guest_rfd_current == 0xffffffff)
		{
			ctx->guest_rfd_current = 0;
		}

		if (rfd.s)
		{
			ctx->guest_ru_suspended = true;
		}
	}

	pro100_mem_write(ptr, &rfd, sizeof(PRO100_RFD));

	// 割り込み発生
	pro100_generate_int(ctx);
}

// RU をポーリングする
void pro100_poll_ru(PRO100_CTX *ctx)
{
	bool b = false;
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

LABEL_LOOP:

	pro100_init_ru_base_addr(ctx);

	if (ctx->host_ru_started == false)
	{
		// RU を開始する
		ctx->host_ru_started = true;

		pro100_wait_cu_ru_accepable(ctx);
		pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, (UINT)ctx->first_recv->rfd_ptr, 4);
		pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0,
			PRO100_MAKE_CU_RU_COMMAND(PRO100_CU_CMD_NOOP, PRO100_RU_CMD_START),
			1);
		pro100_flush(ctx);
		pro100_wait_cu_ru_accepable(ctx);
	}

	// 新しいパケットが到着しているかどうか確認する
	while (true)
	{
		if (ctx->current_recv->rfd->c)
		{
			UCHAR *data;
			UINT size;
			// パケットが到着した
			data = &ctx->current_recv->rfd->data[0];
			size = ctx->current_recv->rfd->recved_bytes;

			if (size >= 1 && size <= PRO100_MAX_PACKET_SIZE)
			{
				// パケット受信完了
#ifdef	PRO100_PASS_MODE
				pro100_write_recv_packet(ctx, data, size);
#else	// PRO100_PASS_MODE
				if (ctx->CallbackRecvPhyNic != NULL)
				{
					void *packet_data[1];
					UINT packet_size[1];

					packet_data[0] = data;
					packet_size[0] = size;
					ctx->CallbackRecvPhyNic(ctx, 1, packet_data, packet_size, ctx->CallbackRecvPhyNicParam, NULL);
				}
#endif	// PRO100_PASS_MODE
			}

			ctx->current_recv = ctx->current_recv->next_recv;

			if (ctx->current_recv == NULL)
			{
				// 最後の受信バッファまで受信が完了したのでバッファをリセットする
				pro100_init_recv_buffer(ctx);
				ctx->host_ru_started = false;
				b = true;
				break;
			}
		}
		else
		{
			break;
		}
	}

	if (b)
	{
		b = false;
		goto LABEL_LOOP;
	}
}

// CU にオペレーションを実行させる
void pro100_exec_cu_op(PRO100_CTX *ctx, PRO100_OP_BLOCK_MAX *op, UINT size)
{
	volatile PRO100_OP_BLOCK *b;
	PRO100_OP_BLOCK *b2;
	phys_t ptr;
	bool src_el, src_s, src_i;
	UINT src_link_addr;
	bool timeouted;
	UINT64 start_tick;
	// 引数チェック
	if (ctx == NULL || op == NULL)
	{
		return;
	}

	// メモリ確保
	b = pro100_alloc_page(&ptr);

	// 一時領域にコピー
	SeCopy((void *)b, op, size);

	// バックアップ
	src_el = b->el;
	src_s = b->s;
	src_i = b->i;
	src_link_addr = b->link_address;

	// フラグセット
	b->el = true;
	b->s = false;
	//b->ok = b->c = false;
	b->link_address = ptr;
	//b->i = false;

	pro100_init_cu_base_addr(ctx);

	pro100_wait_cu_ru_accepable(ctx);

	if (false)
	{
		char tmp[4096];

		SeBinToStrEx(tmp, sizeof(tmp), (void *)b, size);
		debugprint("%s\n", tmp);
	}

	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, (UINT)ptr, 4);
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0, 
		PRO100_MAKE_CU_RU_COMMAND(PRO100_CU_CMD_START, PRO100_RU_CMD_NOOP), 1);
	pro100_flush(ctx);

	// NIC がコマンドを完了するまで待機
	//debugprint("[");
	start_tick = get_time ();
	timeouted = true;
	while ((start_tick + 1000000ULL) >= get_time ())
	{
		if (b->c)
		{
			timeouted = false;
			break;
		}
	}
	//debugprint("%u] ", b->c);
	//b->c = true;

	if (false)
	{
		UINT t = pro100_read(ctx, PRO100_CSR_OFFSET_SCB_STATUS_WORD_0, 1);
		PRO100_SCB_STATUS_WORD_BIT *b = (PRO100_SCB_STATUS_WORD_BIT *)(void *)&t;

		debugprint("STATUS  CU=%u, RU=%u\n", b->cu_status, b->ru_status);
	}

	// 結果を書き戻す
	SeCopy(op, (void *)b, size);
	b2 = (PRO100_OP_BLOCK *)op;

	// バックアップを復元
	b2->el = src_el;
	b2->s = src_s;
	b2->i = src_i;
	b2->link_address = src_link_addr;

	// メモリ解放
	pro100_free_page((void *)b, ptr);

	if (timeouted && src_i)
	{
		//pro100_generate_int(ctx);
	}
}

// オペレーションのデータサイズの取得
UINT pro100_get_op_size(UINT op, void *data)
{
	UINT ret = sizeof(PRO100_OP_BLOCK);
	PRO100_OP_MCAST_ADDR_SETUP *mc = (PRO100_OP_MCAST_ADDR_SETUP *)data;

	switch (op)
	{
	case PRO100_CU_OP_NOP:
		break;

	case PRO100_CU_OP_IA_SETUP:
		ret += 6;
		break;

	case PRO100_CU_OP_CONFIG:
		ret += 22 + 9;
		break;

	case PRO100_CU_OP_MCAST_ADDR_SETUP:
		ret += mc->count + 2 + 2;
		break;

	case PRO100_CU_OP_LOAD_MICROCODE:
		ret += 256;
		break;

	case PRO100_CU_OP_DUMP:
		ret += 4;
		break;

	case PRO100_CU_OP_DIAG:
		break;
	}

	return ret;
}

// CU と RU が受付可能になるまで待機
void pro100_wait_cu_ru_accepable(PRO100_CTX *ctx)
{
	bool flag = false;
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	//debugprint("{B");
	while (true)
	{
		UINT t = pro100_read(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0, 1);

		if (t == 0)
		{
			break;
		}

		if (flag == false)
		{
			flag = true;

//			debugprint(" ** CU=%u, RU=%u **  \n",
//				PRO100_GET_CU_COMMAND(t), PRO100_GET_RU_COMMAND(t));
		}
	}
//	debugprint("} ");
}

// 物理的にパケットを送信する
void pro100_send_packet_to_line(PRO100_CTX *ctx, void *buf, UINT size)
{
	volatile PRO100_OP_BLOCK *b;
	PRO100_OP_TRANSMIT *t;
	phys_t ptr;
	// 引数チェック
	if (ctx == NULL || buf == NULL || size == 0)
	{
		return;
	}

	// メモリ確保
	b = pro100_alloc_page(&ptr);

	t = (PRO100_OP_TRANSMIT *)b;

	t->op_block.op = PRO100_CU_OP_TRANSMIT;
	t->op_block.transmit_flexible_mode = 0;
	t->op_block.transmit_raw_packet = 0;
	t->op_block.transmit_cid = 31;
	t->op_block.i = false;
	t->op_block.s = false;
	t->op_block.el = true;
	t->op_block.link_address = 0;
	t->tbd_array_address = 0xffffffff;
	t->data_bytes = size;
	t->threshold = 1;

	SeCopy(((UCHAR *)b) + sizeof(PRO100_OP_TRANSMIT) +
		(ctx->use_standard_txcb ? 0 : sizeof(PRO100_TBD) * 2), buf, size);

	if (false)
	{
		char tmp[8000];
		SeBinToStrEx(tmp, sizeof(tmp), (void *)b, size + sizeof(PRO100_OP_TRANSMIT));

		debugprint("%s\n\n", tmp);
	}

	pro100_init_cu_base_addr(ctx);
	pro100_wait_cu_ru_accepable(ctx);
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, (UINT)ptr, 4);
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0, 
		PRO100_MAKE_CU_RU_COMMAND(PRO100_CU_CMD_START, PRO100_RU_CMD_NOOP), 1);
	pro100_flush(ctx);

	//debugprint("1\n");
	while (b->c == false);
	//debugprint("2\n");

	pro100_free_page((void *)b, ptr);
}

// 送信しようとしているパケットを読み取る
UINT pro100_read_send_packet(PRO100_CTX *ctx, phys_t addr, void *buf)
{
	PRO100_OP_BLOCK_MAX b;
	PRO100_OP_TRANSMIT *t;
	UINT ret = 0;
	// 引数チェック
	if (ctx == NULL || addr == 0 || buf == NULL)
	{
		return 0;
	}

	// メモリから読み取る
	pro100_mem_read(&b, addr, sizeof(b));

	t = (PRO100_OP_TRANSMIT *)&b;

	if (t->op_block.transmit_flexible_mode == 0)
	{
		// Simplified モード
		ret = t->data_bytes;
		if (ret > PRO100_MAX_PACKET_SIZE)
		{
			// パケットサイズが大きすぎる
			return 0;
		}

		// パケットデータのコピー
		SeCopy(buf, ((UCHAR *)&b) + sizeof(PRO100_OP_TRANSMIT), ret);

		return ret;
	}
	else
	{
		UINT num_tbd = t->tbd_count;
		PRO100_TBD *tbd_array = SeZeroMalloc(num_tbd * sizeof(PRO100_TBD));
		UINT total_packet_size;
		UINT i;

		if (ctx->use_standard_txcb)
		{
			// Standard Flexible モード
			pro100_mem_read(tbd_array, (phys_t)t->tbd_array_address, num_tbd * sizeof(PRO100_TBD));
		}
		else
		{
			// Extended Flexible モード: 最初の 2 個を末尾から読む
			UINT num_tbd_ex = num_tbd;
			if (num_tbd_ex >= 2)
			{
				num_tbd_ex = 2;
			}
			SeCopy(tbd_array, ((UCHAR *)&b) + sizeof(PRO100_OP_TRANSMIT), num_tbd_ex * sizeof(PRO100_TBD));

			// 残りがある場合は TBD アレイアドレスから読み込む
			if ((num_tbd - num_tbd_ex) >= 1)
			{
				pro100_mem_read(((UCHAR *)tbd_array) + num_tbd_ex * sizeof(PRO100_TBD),
					(phys_t)t->tbd_array_address,
					(num_tbd - num_tbd_ex) * sizeof(PRO100_TBD));
			}
		}

		// TBD アレイアドレスを用いてパケットを読み込む
		total_packet_size = 0;

		for (i = 0;i < num_tbd;i++)
		{
			PRO100_TBD *tbd = &tbd_array[i];

			total_packet_size += tbd->size;
		}

		if (total_packet_size > PRO100_MAX_PACKET_SIZE)
		{
			// パケットサイズが大きすぎる
			ret = 0;
		}
		else
		{
			// パケットデータを読み込む
			UCHAR *current_ptr = buf;
			for (i = 0;i < num_tbd;i++)
			{
				PRO100_TBD *tbd = &tbd_array[i];

				pro100_mem_read(current_ptr, (phys_t)tbd->data_address, tbd->size);

				current_ptr += tbd->size;
			}

			ret = total_packet_size;
		}

		SeFree(tbd_array);

		return ret;
	}
}

// ゲスト OS が開始したオペレーションを処理する
void pro100_proc_guest_op(PRO100_CTX *ctx)
{
	phys_t ptr;
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}
	if (ctx->guest_cu_started == false || ctx->guest_cu_current_pointer == 0)
	{
		return;
	}

	ptr = ctx->guest_cu_current_pointer;

	// 終端まで読み取る
	while (true)
	{
		PRO100_OP_BLOCK_MAX b;

		if (ctx->guest_cu_suspended)
		{
			// サスペンド中の場合、前回の最後のオペレーションの S ビットがクリア
			// されたかどうか検査する
			pro100_mem_read(&b, ptr, sizeof(b));
			if (b.op_block.s)
			{
				// サスペンド継続中
				break;
			}

			ptr = ctx->guest_cu_next_pointer;

			ctx->guest_cu_suspended = false;
		}

		pro100_mem_read(&b, ptr, sizeof(b));

		if (b.op_block.op == PRO100_CU_OP_TRANSMIT)
		{
			// 送信処理
			PRO100_OP_TRANSMIT *t = (PRO100_OP_TRANSMIT *)&b;

			if (false)
			{
				debugprint("flexible_mode=%u, raw_packet=%u, cid=%u, array=0x%x, thres=%u, tcount=%u, size=%u\n",
					t->op_block.transmit_flexible_mode,
					t->op_block.transmit_raw_packet,
					t->op_block.transmit_cid,
					t->tbd_array_address,
					t->threshold,
					t->tbd_count,
					t->data_bytes);
			}

			//debugprint("SEND\n");

			if (false)
			{
				char tmp[4096];

				SeBinToStrEx(tmp, sizeof(tmp), &b, 24);
				debugprint("%s\n", tmp);
			}

			if (true)
			{
				UCHAR buf[PRO100_MAX_PACKET_SIZE];
				UINT packet_size = pro100_read_send_packet(ctx, ptr, buf);

#ifdef	PRO100_PASS_MODE
				pro100_write_recv_packet(ctx, buf, packet_size);
#else	// PRO100_PASS_MODE
				if (ctx->CallbackRecvVirtNic != NULL)
				{
					void *packet_data[1];
					UINT packet_sizes[1];

					packet_data[0] = buf;
					packet_sizes[0] = packet_size;
					ctx->CallbackRecvVirtNic(ctx, 1, packet_data, packet_sizes, ctx->CallbackRecvVirtNicParam, NULL);
				}
#endif	// PRO100_PASS_MODE
			}

			b.op_block.ok = b.op_block.c = true;
			b.op_block.transmit_overrun = false;

			pro100_mem_write(ptr, &b, sizeof(PRO100_OP_BLOCK) + sizeof(PRO100_OP_TRANSMIT));

			if (b.op_block.i)
			{
				pro100_generate_int(ctx);
			}
		}
		else
		{
			// 送信以外の処理
			UINT size = pro100_get_op_size(b.op_block.op, &b);

			//debugprint("0x%x: OP: %u  Size=%u\n", (UINT)ptr, b.op_block.op, size);

			switch (b.op_block.op)
			{
			case PRO100_CU_OP_IA_SETUP:
				// IA Setup
				SeCopy(ctx->mac_address, ((UCHAR *)&b) + sizeof(PRO100_OP_BLOCK), 6);
				pro100_init_vpn_client(ctx);
				break;

			case PRO100_CU_OP_CONFIG:
				// Configure
				ctx->use_standard_txcb = ((((UCHAR *)&b)[sizeof(PRO100_OP_BLOCK) + 6] & 0x10) ? true : false);
				break;
			}

			pro100_exec_cu_op(ctx, &b, size);

			pro100_mem_write(ptr, &b, size);
		}

		if (b.op_block.el)
		{
			// 終了フラグ
			ctx->guest_cu_started = false;
			ctx->guest_cu_current_pointer = 0;
			ctx->guest_cu_suspended = false;
			//debugprint("EL\n");
			pro100_generate_int(ctx);
			break;
		}
		if (b.op_block.s)
		{
			// サスペンドフラグ
			ctx->guest_cu_suspended = true;
			ctx->guest_cu_next_pointer = b.op_block.link_address;
			//debugprint("SUSPEND\n");
			pro100_generate_int(ctx);
			break;
		}

		ptr = b.op_block.link_address;
	}

	if (ctx->guest_cu_started)
	{
		ctx->guest_cu_current_pointer = ptr;
	}
}

// 物理メモリ読み込み
void pro100_mem_read(void *buf, phys_t addr, UINT size)
{
	// 引数チェック
	if (addr == 0 || buf == NULL || size == 0)
	{
		return;
	}

	mmio_gphys_access(addr, false, buf, size, MAPMEM_PCD | MAPMEM_PWT | 0/*MAPMEM_PAT*/);
}

// 物理メモリ書き込み
void pro100_mem_write(phys_t addr, void *buf, UINT size)
{
	// 引数チェック
	if (addr == 0 || buf == NULL || size == 0)
	{
		return;
	}

	mmio_gphys_access(addr, true, buf, size, MAPMEM_PCD | MAPMEM_PWT | 0/*MAPMEM_PAT*/);
}

// 書き込みフック
bool pro100_hook_write(PRO100_CTX *ctx, UINT offset, UINT data, UINT size)
{
	// 引数チェック
	if (ctx == NULL)
	{
		return false;
	}

	switch (offset)
	{
	case PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0:	// RU Command, CU Command
		if (size != 1)
		{
			debugprint("pro100_hook_write: PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0: BAD SIZE: %u\n", size);
		}
		if (size == 1)
		{
			UCHAR b = (UCHAR)data;
			UINT ru = PRO100_GET_RU_COMMAND(b);
			UINT cu = PRO100_GET_CU_COMMAND(b);
			char *s1 = NULL;
			char *s2 = NULL;

			s1 = pro100_get_ru_command_string(ru);
			s2 = pro100_get_cu_command_string(cu);

			if (s1 != NULL || s2 != NULL)
			{
				//debugprint("[%s, %s]  ", s1, s2);
			}

			switch (cu)
			{
			case PRO100_CU_CMD_NOOP:
				break;

			case PRO100_CU_CMD_START:
				//debugprint("GUEST PRO100_CU_CMD_START: 0x%x\n", (UINT)ctx->guest_last_general_pointer);
				ctx->guest_cu_started = true;
				ctx->guest_cu_suspended = false;
				ctx->guest_cu_start_pointer = (phys_t)ctx->guest_last_general_pointer;
				ctx->guest_cu_current_pointer = ctx->guest_cu_start_pointer;

				pro100_proc_guest_op(ctx);

				cu = PRO100_CU_CMD_NOOP;

				break;

			case PRO100_CU_CMD_RESUME:
				//debugprint("GUEST PRO100_CU_CMD_RESUME\n");
				pro100_proc_guest_op(ctx);

				cu = PRO100_CU_CMD_NOOP;

				break;

			case PRO100_CU_CMD_LOAD_CU_BASE:
				//debugprint("GUEST PRO100_CU_CMD_LOAD_CU_BASE: 0x%x\n", (UINT)ctx->guest_last_general_pointer);

				cu = PRO100_CU_CMD_NOOP;

				ctx->cu_base_inited = false;

				break;

			case PRO100_CU_CMD_LOAD_DUMP_ADDR:
				debugprint("GUEST PRO100_CU_CMD_LOAD_DUMP_ADDR: 0x%x\n", (UINT)ctx->guest_last_general_pointer);

				cu = PRO100_CU_CMD_NOOP;
				//pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, (UINT)ctx->guest_last_general_pointer, 4);

				ctx->guest_last_counter_pointer = ctx->guest_last_general_pointer;

				break;

			case PRO100_CU_CMD_DUMP_STAT:
			case PRO100_CU_CMD_DUMP_AND_RESET_STAT:
				debugprint(cu == PRO100_CU_CMD_DUMP_STAT ? "GUEST PRO100_CU_CMD_DUMP_STAT\n" : "GUEST PRO100_CU_CMD_DUMP_AND_RESET_STAT\n");

				if (ctx->guest_last_counter_pointer != 0)
				{
					UINT dummy_data[21];
					// ゲスト OS がカウンタデータのダンプを要求しているのでウソのデータを返す
					// (これをしないと Windows 版ドライバの一部で 2 秒間のビジーループによる待ちが発生してしまう)
					memset(dummy_data, 0, sizeof(dummy_data));

					dummy_data[16] = dummy_data[19] = dummy_data[20] = (cu == PRO100_CU_CMD_DUMP_STAT ? 0xa005 : 0xa007);

					pro100_mem_write((phys_t)ctx->guest_last_counter_pointer, dummy_data, sizeof(dummy_data));
				}
				else
				{
					debugprint("error: ctx->guest_last_counter_pointer == 0\n");
				}

				cu = PRO100_CU_CMD_NOOP;

				break;

			case PRO100_CU_CMD_HPQ_START:
				//debugprint("GUEST PRO100_CU_CMD_HPQ_START: 0x%x\n", (UINT)ctx->guest_last_general_pointer);

				cu = PRO100_CU_CMD_NOOP;

				break;

			case PRO100_CU_CMD_CU_STAT_RESUME:
				//debugprint("GUEST PRO100_CU_CMD_CU_STAT_RESUME\n");

				//pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, (UINT)ctx->guest_last_general_pointer, 4);
				//cu = PRO100_CU_CMD_NOOP;

				break;

			case PRO100_CU_CMD_HPQ_RESUME:
				//debugprint("GUEST PRO100_CU_CMD_HPQ_RESUME\n");

				cu = PRO100_CU_CMD_NOOP;

				break;

			default:
				printf("!!!! GUEST SEND UNKNOWN CU CMD: %u\n", cu);
				break;
			}

			switch (ru)
			{
			case PRO100_RU_CMD_NOOP:
				break;

			case PRO100_RU_CMD_START:
				//debugprint("GUEST PRO100_RU_CMD_START: 0x%x\n", (UINT)ctx->guest_last_general_pointer);

				ru = PRO100_CU_CMD_NOOP;

				// ゲスト OS が指定してきた受信バッファのポインタを記憶する
				ctx->guest_rfd_current = ctx->guest_rfd_first = (phys_t)ctx->guest_last_general_pointer;

				pro100_poll_ru(ctx);

				break;

			case PRO100_RU_CMD_RESUME:
				//debugprint("GUEST PRO100_RU_CMD_RESUME\n");

				ru = PRO100_CU_CMD_NOOP;

				ctx->guest_ru_suspended = false;

				pro100_poll_ru(ctx);

				break;

			case PRO100_RU_CMD_LOAD_HEADER_DATA_SIZE:
				//debugprint("GUEST PRO100_RU_CMD_LOAD_HEADER_DATA_SIZE: 0x%x\n", (UINT)ctx->guest_last_general_pointer);

				ru = PRO100_CU_CMD_NOOP;

				break;

			case PRO100_RU_CMD_LOAD_RU_BASE:
				//debugprint("GUEST PRO100_RU_CMD_LOAD_RU_BASE: 0x%x\n", (UINT)ctx->guest_last_general_pointer);

				ru = PRO100_CU_CMD_NOOP;

				ctx->ru_base_inited = false;

				break;

			default:
				printf("!!!! GUEST SEND UNKNOWN RU CMD: %u\n", ru);
				break;
			}

			ru = 0;

			b = PRO100_MAKE_CU_RU_COMMAND(cu, ru);

			if ((cu != 0 || ru != 0) && (cu == 0 || ru == 0))
			{
				debugprint("<P");
				pro100_wait_cu_ru_accepable(ctx);
				debugprint(">");
				pro100_write(ctx, PRO100_CSR_OFFSET_SCB_GENERAL_POINTER, (UINT)ctx->guest_last_general_pointer, 4);
				pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0, b, 1);
				debugprint("<Y");
				pro100_wait_cu_ru_accepable(ctx);
				debugprint(">");
			}

			return true;
		}
		break;

	case PRO100_CSR_OFFSET_SCB_COMMAND_WORD_1:	// 割り込み制御ビット
		if (size != 1)
		{
			debugprint("pro100_hook_write: PRO100_CSR_OFFSET_SCB_COMMAND_WORD_1: BAD SIZE: %u\n", size);
		}
		if (size == 1)
		{
			ctx->int_mask_guest_set = data;

			if (true)
			{
				//PRO100_INT_BIT *ib = (PRO100_INT_BIT *)&ctx->int_mask_guest_set;

				//debugprint("int mask: M=%u   0x%x\n", (UINT)ib->mask_all, (UINT)ctx->int_mask_guest_set);

				/*

				ib->mask_all = true;
				ib->fcp = ib->er = ib->rnr = ib->cna = ib->fr = ib->cx = 1;
				*/

				//pro100_beep((ib->mask_all == 0 ? 880 : 440), 200);

				pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_1,
					ctx->int_mask_guest_set, 4);

				return true;
			}
		}
		break;

	case PRO100_CSR_OFFSET_SCB_GENERAL_POINTER:	// 汎用ポインタ
		if (size != 4)
		{
			debugprint("pro100_hook_write: PRO100_CSR_OFFSET_SCB_GENERAL_POINTER: BAD SIZE: %u\n", size);
		}
		if (size == 4)
		{
			ctx->guest_last_general_pointer = data;
			//debugprint("GUEST WRITES POINTER: 0x%x\n", data);
		}
		return true;

	case PRO100_CSR_OFFSET_SCB_STATUS_WORD_1:	// STAT/ACK
		/*if (size == 1)
		{
			debugprint("<ACK>");
			if (data != 0)
			{
				pro100_write(ctx, PRO100_CSR_OFFSET_SCB_STATUS_WORD_1, 0xff, 1);
			}

			return true;
		}*/
		pro100_poll_ru(ctx);
		break;

	case PRO100_CSR_OFFSET_SCB_PORT:	// ポート
		break;

	case PRO100_CSR_OFFSET_SCB_MDI:		// MDI
		break;

	default:
		if (offset == 0 && size == 2)
		{
			UCHAR buf[4];
			*((UINT *)buf) = data;
			pro100_hook_write(ctx, 1, buf[1], 1);

			return true;
		}
		else
		{
			if (offset < 0x10)
			{
				printf("*** WRITE ACCESS TO   0x%x  size=%u   data=0x%x\n", offset, size, data);
			}
		}
		break;
	}

	return false;
}

// 読み取りフック
bool pro100_hook_read(PRO100_CTX *ctx, UINT offset, UINT size, UINT *data)
{
	// 引数チェック
	if (ctx == NULL)
	{
		return false;
	}

	switch (offset)
	{
	case PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0:	// コマンド実行状態
		if (size != 1)
		{
			debugprint("pro100_hook_read: PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0: BAD SIZE: %u\n", size);
		}
		if (size == 1)
		{
			UINT t = pro100_read(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_0, 1);

			t = PRO100_MAKE_CU_RU_COMMAND(0, 0);

			*data = t;

			return true;
		}
		break;

	case PRO100_CSR_OFFSET_SCB_STATUS_WORD_0:	// RU と CU のステータス
		if (size != 1)
		{
			if (size != 2)
			{
				debugprint("pro100_hook_read: PRO100_CSR_OFFSET_SCB_STATUS_WORD_0: BAD SIZE: %u\n", size);
			}
			else
			{
				UINT data1 = 0, data2 = 0;
				UCHAR data3[4];
				pro100_hook_read(ctx, PRO100_CSR_OFFSET_SCB_STATUS_WORD_0, 1, &data1);
				pro100_hook_read(ctx, PRO100_CSR_OFFSET_SCB_STATUS_WORD_1, 1, &data2);
				data3[0] = (UCHAR)data1;
				data3[1] = (UCHAR)data2;
				data3[2] = data3[3] = 0;

				*data = *((UINT *)data3);

				//debugprint("*data = 0x%x\n", *data);

				return true;
			}
		}
		if (size == 1)
		{
			UINT t = pro100_read(ctx, PRO100_CSR_OFFSET_SCB_STATUS_WORD_0, 1);
			PRO100_SCB_STATUS_WORD_BIT *sb = (PRO100_SCB_STATUS_WORD_BIT *)(void *)&t;

			sb->ru_status = 2;

			*data = t;
			return true;
		}
		break;

	case PRO100_CSR_OFFSET_SCB_STATUS_WORD_1:	// STAT/ACK
		pro100_poll_ru(ctx);

		if (size != 1)
		{
			debugprint("pro100_hook_read: PRO100_CSR_OFFSET_SCB_STATUS_WORD_1: BAD SIZE: %u\n", size);
		}
		if (size == 1)
		{
			UINT t;
			PRO100_STAT_ACK *sa;

			//debugprint("<INT>");

			pro100_proc_guest_op(ctx);

			t = pro100_read(ctx, offset, size);

			pro100_write(ctx, offset, t, 1);

			sa = (PRO100_STAT_ACK *)(void *)&t;

			sa->cna = sa->cx_tno = sa->fr = sa->rnr = sa->mdi = sa->swi = sa->fcp = 1;
			sa->rnr = 0;

			*data = t;

			return true;
		}
		break;

	case PRO100_CSR_OFFSET_SCB_COMMAND_WORD_1:	// 割り込み制御ビット
		if (size != 1)
		{
			debugprint("pro100_hook_read: PRO100_CSR_OFFSET_SCB_COMMAND_WORD_1: BAD SIZE: %u\n", size);
		}
		if (size == 1)
		{
			UINT t = ctx->int_mask_guest_set;
			PRO100_INT_BIT *bi = (PRO100_INT_BIT *)(void *)&t;
			bi->si = false;

			*data = t;

			return true;
		}
		break;

	case PRO100_CSR_OFFSET_SCB_GENERAL_POINTER:	// 汎用ポインタ
		if (size != 4)
		{
			debugprint("pro100_hook_read: PRO100_CSR_OFFSET_SCB_GENERAL_POINTER: BAD SIZE: %u\n", size);
		}
		if (size == 4)
		{
			*data = ctx->guest_last_general_pointer;
			return true;
		}
		break;

	default:
		if (offset < 0x10)
		{
			printf("*** READ ACCESS TO   0x%x  size=%u\n", offset, size);
		}
		break;
	}

	return false;
}

// 書き込み操作のフラッシュ
void pro100_flush(PRO100_CTX *ctx)
{
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	pro100_read(ctx, PRO100_CSR_OFFSET_SCB_STATUS_WORD_0, 1);
}

// 書き込み
void pro100_write(PRO100_CTX *ctx, UINT offset, UINT data, UINT size)
{
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	mmio_hphys_access((phys_t)ctx->csr_mm_addr + (phys_t)offset, true, &data, size, MAPMEM_PCD | MAPMEM_PWT | 0/*MAPMEM_PAT*/);
}

// 読み取り
UINT pro100_read(PRO100_CTX *ctx, UINT offset, UINT size)
{
	UINT data = 0;
	// 引数チェック
	if (ctx == NULL)
	{
		return 0;
	}

	mmio_hphys_access((phys_t)ctx->csr_mm_addr + (phys_t)offset, false,
		&data, size, MAPMEM_PCD | MAPMEM_PWT | 0/*MAPMEM_PAT*/);

	return data;
}

// 割り込みを発生させる
void pro100_generate_int(PRO100_CTX *ctx)
{
	PRO100_INT_BIT ib;
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	SeCopy(&ib, &ctx->int_mask_guest_set, sizeof(UINT));
	if (ib.mask_all != 0)
	{
		return;
	}

	ib.si = 1;

	//debugprint("*");
	pro100_write(ctx, PRO100_CSR_OFFSET_SCB_COMMAND_WORD_1, *((UINT *)(void *)&ib), 1);
}

// CSR レジスタ用 MMIO ハンドラ
int pro100_mm_handler(void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	int ret = 0;
	PRO100_CTX *ctx = (PRO100_CTX *)data;

	spinlock_lock (&ctx->lock);

	// 範囲チェック
	if (((UINT64)gphys >= (UINT64)ctx->csr_mm_addr) &&
		((UINT64)gphys < ((UINT64)ctx->csr_mm_addr) + (UINT64)PRO100_CSR_SIZE))
	{
		UINT offset = (UINT)((UINT64)gphys - (UINT64)ctx->csr_mm_addr);

		if (len == 1 || len == 2 || len == 4)
		{
			if (wr == 0)
			{
				UINT ret_data = 0;
				if (pro100_hook_read(ctx, offset, len, &ret_data) == false)
				{
					ret_data = pro100_read(ctx, offset, len);
				}

				if (len == 1)
				{
					*((UCHAR *)buf) = (UCHAR)ret_data;
				}
				else if (len == 2)
				{
					*((USHORT *)buf) = (USHORT)ret_data;
				}
				else if (len == 4)
				{
					*((UINT *)buf) = (UINT)ret_data;
				}
			}
			else
			{
				UINT data = 0;
				if (len == 1)
				{
					data = (UINT)(*((UCHAR *)buf));
				}
				else if (len == 2)
				{
					data = (UINT)(*((USHORT *)buf));
				}
				else if (len == 4)
				{
					data = (UINT)(*((UINT *)buf));
				}

				if (pro100_hook_write(ctx, offset, data, len) == false)
				{
					pro100_write(ctx, offset, data, len);
				}
			}

			ret = 1;
		}
	}

	spinlock_unlock (&ctx->lock);

	return ret;
}

// 割り込み制御ビットを文字列に変換 (デバッグ用)
void pro100_get_int_bit_string(char *str, UCHAR value)
{
	PRO100_INT_BIT *ib = (PRO100_INT_BIT *)&value;

	snprintf(str, 1024, "M=%u SI=%u FCP=%u ER=%u RNR=%u CNA=%u FR=%u CX=%u",
		ib->mask_all, ib->si, ib->fcp, ib->er, ib->rnr, ib->cna, ib->fr, ib->cx);
}

// STAT/ACK ビットを文字列に変換 (デバッグ用)
void pro100_get_stat_ack_string(char *str, UCHAR value)
{
	PRO100_STAT_ACK *sa = (PRO100_STAT_ACK *)&value;

	snprintf(str, 1024, "FCP=%u SWI=%u MDI=%u RNR=%u CNA=%u FR=%u CX=%u",
		sa->fcp, sa->swi, sa->mdi, sa->rnr, sa->cna, sa->fr, sa->cx_tno);
}

// CU コマンドを文字列に変換 (デバッグ用)
char *pro100_get_cu_command_string(UINT cu)
{
	char *s = NULL;

	switch (cu)
	{
	case PRO100_CU_CMD_START:
		s = "CU Start";
		break;

	case PRO100_CU_CMD_RESUME:
		s = "CU Resume";
		break;

	case PRO100_CU_CMD_HPQ_START:
		s = "CU HPQ Start";
		break;

	case PRO100_CU_CMD_LOAD_DUMP_ADDR:
		s = "Load Dump Addr";
		break;

	case PRO100_CU_CMD_DUMP_STAT:
		s = "Dump Stat";
		break;

	case PRO100_CU_CMD_LOAD_CU_BASE:
		s = "Load CU Base";
		break;

	case PRO100_CU_CMD_DUMP_AND_RESET_STAT:
		s = "Dump and Reset Stat";
		break;

	case PRO100_CU_CMD_CU_STAT_RESUME:
		s = "CU Stat Resume";
		break;

	case PRO100_CU_CMD_HPQ_RESUME:
		s = "CU HPQ Resume";
		break;

	case 8:		s = "Unknown 8";		break;
	case 9:		s = "Unknown 9";		break;
	case 12:	s = "Unknown 12";		break;
	case 13:	s = "Unknown 13";		break;
	case 14:	s = "Unknown 14";		break;
	case 15:	s = "Unknown 15";		break;
	}

	return s;
}

// RU コマンドを文字列に変換 (デバッグ用)
char *pro100_get_ru_command_string(UINT ru)
{
	char *s1 = NULL;

	switch (ru)
	{
	case PRO100_RU_CMD_START:
		s1 = "RU Start";
		break;

	case PRO100_RU_CMD_RESUME:
		s1 = "RU Resume";
		break;

	case PRO100_RU_CMD_RECV_DMA_REDIRECT:
		s1 = "Receive DMA Redirect";
		break;

	case PRO100_RU_CMD_ABORT:
		s1 = "RU Abort";
		break;

	case PRO100_RU_CMD_LOAD_HEADER_DATA_SIZE:
		s1 = "Load Header Data Size";
		break;

	case PRO100_RU_CMD_LOAD_RU_BASE:
		s1 = "Load RU Base";
		break;

	case PRO100_RU_CMD_RBD_RESUME:
		s1 = "RBD Resume";
		break;
	}

	return s1;
}

// I/O ハンドラ: 未使用
int pro100_io_handler(core_io_t io, union mem *data, void *arg)
{
	debugprint("IO port=%u, size=%u, dir=%u\n", (UINT)io.port, (UINT)io.size, (UINT)io.dir);

	return CORE_IO_RET_DEFAULT;
}

// PCI コンフィグレーションレジスタの読み込み処理
int
pro100_config_read (struct pci_device *dev, u8 iosize, u16 offset,
		    union mem *data)
{
	pci_handle_default_config_read (dev, iosize, offset, data);

	return CORE_IO_RET_DONE;
}

// PCI コンフィグレーションレジスタの書き込み処理
int
pro100_config_write (struct pci_device *dev, u8 iosize, u16 offset,
		     union mem *data)
{
	PRO100_CTX *ctx = (PRO100_CTX *)dev->host;
	UINT mode = 0;
	UINT addr = 0;

	if (iosize == 4)
	{
		switch (offset)
		{
		case PRO100_PCI_CONFIG_32_CSR_MMAP_ADDR_REG:
			if (data->dword != 0 && data->dword != 0xffffffff)
			{
				mode = 1;
			}
			break;

		case PRO100_PCI_CONFIG_32_CSR_IO_ADDR_REG:
			if (data->dword != 0 && data->dword != 0xffffffff)
			{
				mode = 2;
			}
			break;
		}
	}

	pci_handle_default_config_write (dev, iosize, offset, data);

	switch (mode)
	{
	case 1:
		addr = dev->config_space.regs32[PRO100_PCI_CONFIG_32_CSR_MMAP_ADDR_REG / sizeof(UINT)] & PCI_CONFIG_BASE_ADDRESS_MEMMASK;
		//if (addr < 0xF0000000)
		{
			if (ctx->csr_mm_addr != addr)
			{
				DWORD old_mm_addr = ctx->csr_mm_addr;
				// NIC の CSR レジスタのメモリアドレスが変更された
				ctx->csr_mm_addr = addr;

				if (old_mm_addr != 0)
				{
					if (ctx->csr_mm_handler != NULL)
					{
						// 古い MMIO ハンドラが登録されている場合は解除する
						mmio_unregister(ctx->csr_mm_handler);
					}
				}

				ctx->csr_mm_handler = mmio_register((phys_t)ctx->csr_mm_addr, 64,
					pro100_mm_handler, ctx);

				debugprint("vpn_pro100: mmio_register 0x%x\n", ctx->csr_mm_addr);
			}
		}
		break;

	case 2:
		if (true)
		{
			addr = dev->config_space.regs32[PRO100_PCI_CONFIG_32_CSR_IO_ADDR_REG / sizeof(UINT)] & PCI_CONFIG_BASE_ADDRESS_IOMASK;
			if (ctx->csr_io_addr != addr)
			{
				// NIC の CSR レジスタの I/O アドレスが変更された
				UINT old_io_addr = ctx->csr_io_addr;
				ctx->csr_io_addr = addr;

				if (old_io_addr == 0)
				{
					ctx->csr_io_handler = core_io_register_handler(ctx->csr_io_addr,
						PRO100_CSR_SIZE, pro100_io_handler, ctx,
						CORE_IO_PRIO_EXCLUSIVE, driver_name);
				}
				else
				{
					ctx->csr_io_handler = core_io_modify_handler(ctx->csr_io_handler,
						ctx->csr_io_addr, PRO100_CSR_SIZE);
				}
			}
		}
		break;
	}

	return CORE_IO_RET_DONE;
}

// 受信バッファの確保
void pro100_alloc_recv_buffer(PRO100_CTX *ctx)
{
	UINT i;
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	ctx->num_recv = PRO100_NUM_RECV_BUFFERS;
	ctx->recv = SeMalloc(sizeof(PRO100_RECV) * ctx->num_recv);

	for (i = 0;i < ctx->num_recv;i++)
	{
		PRO100_RECV *r = &ctx->recv[i];

		// メモリ確保
		r->rfd = (PRO100_RFD *)pro100_alloc_page(&r->rfd_ptr);
	}

	// データ構造の初期化
	pro100_init_recv_buffer(ctx);
}

// 受信バッファの初期化
void pro100_init_recv_buffer(PRO100_CTX *ctx)
{
	UINT i;
	// 引数チェック
	if (ctx == NULL)
	{
		return;
	}

	// リンクリスト構造の初期化
	for (i = 0;i < ctx->num_recv;i++)
	{
		PRO100_RECV *r = &ctx->recv[i];
		PRO100_RFD *rfd = r->rfd;

		SeZero(rfd, sizeof(PRO100_RFD));

		rfd->buffer_size = PRO100_MAX_PACKET_SIZE;

		if (i != (ctx->num_recv - 1))
		{
			r->next_recv = &ctx->recv[i + 1];
			rfd->s = rfd->el = false;
			rfd->link_address = (UINT)r->next_recv->rfd_ptr;
		}
		else
		{
			r->next_recv = NULL;
			rfd->link_address = 0;
			rfd->s = false;
			rfd->el	= true;
		}
	}

	ctx->first_recv = &ctx->recv[0];
	ctx->last_recv = &ctx->recv[ctx->num_recv - 1];
	ctx->current_recv = ctx->first_recv;
}

// 新しいデバイスの発見
void pro100_new(struct pci_device *dev)
{
	PRO100_CTX *ctx = SeZeroMalloc(sizeof(PRO100_CTX));

	debugprint ("pro100_new\n");

#ifdef VTD_TRANS
        if (iommu_detected) {
                add_remap(dev->address.bus_no ,dev->address.device_no ,dev->address.func_no,
                          vmm_start_inf() >> 12, (vmm_term_inf()-vmm_start_inf()) >> 12, PERM_DMA_RW) ;
        }
#endif // of VTD_TRANS

	ctx->dev = dev;
	ctx->net_handle = net_new_nic (dev->driver_options[0], false);
	spinlock_init (&ctx->lock);
	dev->host = ctx;
	dev->driver->options.use_base_address_mask_emulation = 1;

	pro100_alloc_recv_buffer(ctx);

	if (pro100_ctx == NULL)
	{
		pro100_ctx = ctx;
	}
	else
	{
		debugprint("Error: Two or more pro100 devices found.\n");
		pro100_beep(1234, 5000);
	}
}

static struct pci_driver vpn_pro100_driver =
{
	.name		= driver_name,
	.longname	= driver_longname,
	.driver_options	= "net",
	.device		= "id=8086:1229",
	.new		= pro100_new,
	.config_read	= pro100_config_read,
	.config_write	= pro100_config_write,
};

// 初期化
void pro100_init()
{
	debugprint("pro100_init() start.\n");

	pci_register_driver(&vpn_pro100_driver);

	debugprint("pro100_init() end.\n");
}


PCI_DRIVER_INIT(pro100_init);
