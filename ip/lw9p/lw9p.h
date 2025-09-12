/*
 * Copyright (c) 2025 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

#ifndef LW9P_H
#define LW9P_H

#include <core/spinlock.h>
#include <core/list.h>
#include <share/vmm_types.h>
#include <lw9p_common.h>
#include "lwip/ip.h"
#include "lwip/tcp.h"

#define LW9P_TRACE   0x01
#define LW9P_WARNING 0x02
#define LW9P_DBG     0x04
#define LW9P_SEVERE  0x08
/* size(4) + type(1) + tag(2) + fid(4) + offset(8) + count(4) */
#define LW9P_TREAD_TWRITE_HDR_SIZE 23
#define LW9P_HDR_LEN_BASE	   7 /* size(4) + type(1) + tag(2) */
#define RW_HDR_LEN_WITH_COUNT	   11 /* base(7) + count(4) */
#define RW_HDR_LEN_COUNT_FIELD	   4
#define LW9P_HDR_LEN_SIZE	   4
#define LW9P_HDR_LEN_FID	   4
/* 24 comes from IOHDRSZ in Plan 9's <fcall.h>. */
#define LW9P_MAX_HDR_SIZE	   24
#define LW9P_DEFAULT_DATA_LIMIT	   65536

#define LW9P_DEBUG_FLAG (LW9P_WARNING | LW9P_SEVERE)
#define LW9P_DEBUG(debug_level, Msg) \
	do { \
		if (LW9P_DEBUG_FLAG & debug_level) { \
			printf ("%s: ", #debug_level); \
			printf Msg; \
		} \
	} while (0)
#define min(a, b) (((a) < (b)) ? (a) : (b))

typedef enum {
	LW9P_RESULT_OK = 0,
	LW9P_RESULT_ERR_STATE_NO_SUPPORT, /* Unknown state */
	LW9P_RESULT_ERR_BUFFER, /* Buffer null or overflow */
	LW9P_RESULT_ERR_CONNECT, /* Connection to server failed */
	LW9P_RESULT_ERR_TCP_RECV, /* Error on receiving */
	LW9P_RESULT_ERR_TCP_WRITE, /* Error TCP WRITE */
	LW9P_RESULT_ERR_CLOSED, /* Internal network stack error */
	LW9P_RESULT_ERR_STATE, /* Unhandled 9p state */
	LW9P_RESULT_ERR_TWRITE_TREAD, /* Received ERR MSG state */
	LW9P_RESULT_ERR_PARSE, /* 9p data parse error */
	LW9P_RESULT_ERR_MEDIA, /* Media is removed */
} lw9p_results;

/** lw9p connection state */
typedef enum {
	LW9P_CLOSED = 0,
	LW9P_CONNECTED,
	LW9P_TVERSION_SENT,
	LW9P_TATTACH_SENT,
	LW9P_TGETATTR_SENT_0,
	LW9P_TWALK_SENT_0,
	LW9P_TWALK_SENT_1,
	LW9P_TOPEN_SENT,
	LW9P_TGETATTR_SENT_1,
	LW9P_READY_FOR_RW,
	LW9P_RW_QUEUED,
	LW9P_TREAD_SENT,
	LW9P_RREAD_CONTINUE,
	LW9P_TWRITE_SENT,
} lw9p_state_t;

/** lw9p session structure */
typedef struct lw9p_session lw9p_session_t;
struct lw9p_session {
	char *name;
	char *img_path;
	char *remote_filename;
	char *uname;
	u32 uid;
	ip_addr_t server_ip;
	u16 server_port;
	struct tcp_pcb *lw9p_pcb;
	client_state_enum client_state;
	bool readonly;
	bool is_write;
	u8 state;
	u8 rw_queuing;
	u16 rw_tag;
	u32 data_limit;
	u64 image_size;
	u64 start_pos;
	u32 req_bytes;
	u32 chunk_bytes;
	struct tag_list *tag;
	struct tag_list *tag_tail;
	struct send_data_list *send_data;
	struct send_data_list *send_data_tail;
	struct send_data_list *send_data_ack;
	void (*wr_done_fn) (void **arg);
	void *cb_arg;
	u32 timer_start_time;
	u8 pending_req;
	spinlock_t lw9p_lock;
	LIST1_DEFINE (lw9p_session_t);
	struct pbuf *cur_pbuf;
	u32 cur_offset;
	u32 rread_datasize;
	void *rread_databuf;
	void *buffer_ptr;
};

typedef enum {
	Rlerror = 7,
	TOpen = 12,
	ROpen = 13,
	TGetattr = 24,
	RGetattr = 25,
	TVersion = 100,
	RVersion = 101,
	TAttach = 104,
	RAttach = 105,
	TWalk = 110,
	RWalk = 111,
	TRead = 116,
	RRead = 117,
	TWrite = 118,
	RWrite = 119,
} LW9PMsgType;

typedef struct __attribute__ ((packed)) LW9PMsg {
	u32 size;
	u8 type;
	u16 tag;
	u8 payload[];
} LW9PMsg;

void lw9p_wr_data_chunk (lw9p_session_t *s);
void lw9p_connect (void *arg);
void lw9p_close (lw9p_session_t *s, int result);

static inline void
set_client_state (lw9p_session_t *s, u8 state)
{
	spinlock_lock (&s->lw9p_lock);
	s->client_state = state;
	spinlock_unlock (&s->lw9p_lock);
}

#endif /* LW9P_H */
