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

#include <core/mm.h>
#include "lw9p.h"
#include "lw9p_helper.h"

typedef void send_data_done_t (lw9p_session_t *s, void *buffer, u32 len);

struct send_data_list {
	struct send_data_list *next;
	void *buffer;
	u32 total_len;
	u32 sent_len;
	u32 ack_len;
	send_data_done_t *done;
};

struct tag_list {
	struct tag_list *next;
	void *buffer;
	u32 len;
	u16 tag;
};

static void
tag_append (lw9p_session_t *s, void *buffer, u32 len, u16 tag)
{
	struct tag_list *p = mem_malloc (sizeof *p);
	if (!p) {
		LW9P_DEBUG (LW9P_SEVERE, ("%s: mem_malloc failed\n",
					  __func__));
		return;
	}
	p->next = NULL;
	p->buffer = buffer;
	p->len = len;
	p->tag = tag;
	if (s->tag)
		s->tag_tail->next = p;
	else
		s->tag = p;
	s->tag_tail = p;
}

static void *
tag_delete (lw9p_session_t *s, u32 len, u16 tag)
{
	struct tag_list *p = s->tag;
	if (!p)
		goto not_found;
	if (p->tag == tag) {
		s->tag = p->next;
		goto found;
	}
	while (p->next) {
		struct tag_list *prev = p;
		p = p->next;
		if (p->tag == tag) {
			prev->next = p->next;
			if (!p->next)
				s->tag_tail = prev;
			goto found;
		}
	}
not_found:
	LW9P_DEBUG (LW9P_SEVERE, ("%s: not found\n", __func__));
	return NULL;
found:;
	void *buffer = p->buffer;
	if (p->len != len) {
		/* The server returned different Rread/Rwrite length
		 * from Tread/Twrite length.  Return NULL to avoid
		 * buffer corruption/overflow. */
		LW9P_DEBUG (LW9P_SEVERE, ("%s: invalid len\n", __func__));
		buffer = NULL;
	}
	mem_free (p);
	return buffer;
}

static void
send_data_enqueue (lw9p_session_t *s, void *buffer, u32 len,
		   send_data_done_t *done)
{
	struct send_data_list *p = mem_malloc (sizeof *p);
	if (!p) {
		LW9P_DEBUG (LW9P_SEVERE, ("%s: mem_malloc failed\n",
					  __func__));
		return;
	}
	p->next = NULL;
	p->buffer = buffer;
	p->total_len = len;
	p->sent_len = 0;
	p->ack_len = 0;
	p->done = done;
	if (s->send_data) {
		s->send_data_tail->next = p;
	} else {
		s->send_data = p;
		s->send_data_ack = p;
	}
	s->send_data_tail = p;
}

static void
send_data_ack (lw9p_session_t *s, u32 len)
{
	while (len > 0) {
		struct send_data_list *p = s->send_data_ack;
		if (!p) {
			LW9P_DEBUG (LW9P_SEVERE, ("%s: list empty\n",
						  __func__));
			return;
		}
		u32 remaining = p->total_len - p->ack_len;
		u32 plen = len < remaining ? len : remaining;
		if (p->ack_len + plen > p->sent_len) {
			LW9P_DEBUG (LW9P_SEVERE, ("%s: len too big\n",
						  __func__));
			return;
		}
		p->ack_len += plen;
		len -= plen;
		if (p->ack_len == p->total_len) {
			if (p->done)
				p->done (s, p->buffer, p->total_len);
			s->send_data_ack = p->next;
			mem_free (p);
		}
	}
}

static void
send_data_free_all (lw9p_session_t *s)
{
	s->send_data = NULL;
	for (;;) {
		struct send_data_list *p = s->send_data_ack;
		if (!p)
			return;
		if (p->done)
			p->done (s, p->buffer, p->total_len);
		s->send_data_ack = p->next;
		mem_free (p);
	}
}

static void
lw9p_send_data (lw9p_session_t *s)
{
	u32 tcpsndbuf, bytes_to_send;
	u32 remaining;
	err_t err = ERR_OK;

again:
	/* Edge case if connection is just lost */
	if (lw9p_client_state (s) == CONNECT_FAILED)
		return;

	tcpsndbuf = tcp_sndbuf (s->lw9p_pcb);
	LW9P_DEBUG (LW9P_DBG, ("lw9p_send_data tcpsndbuf %u\n", tcpsndbuf));
	if (tcpsndbuf == 0) {
		LW9P_DEBUG (LW9P_WARNING, ("No send buffer available\n"));
		return;
	}

	struct send_data_list *p = s->send_data;
	if (!p)
		return;
	remaining = p->total_len - p->sent_len;
	bytes_to_send = (remaining > tcpsndbuf) ? tcpsndbuf : remaining;
	bool filling_sndbuf = remaining >= tcpsndbuf;
	bool more = remaining > tcpsndbuf || p->next;

	err = tcp_write (s->lw9p_pcb, p->buffer + p->sent_len, bytes_to_send,
			 more ? TCP_WRITE_FLAG_MORE : 0);
	if (err == ERR_MEM)
		return;

	LW9P_DEBUG (LW9P_DBG,
		    ("tcp_write ptr %p, sending %u, remain %u total=%u\n",
		     p->buffer + p->sent_len, bytes_to_send, remaining,
		     p->total_len));
	if (err != ERR_OK) {
		LW9P_DEBUG (LW9P_SEVERE, ("tcp_write error %d\n", err));
		send_data_free_all (s);
		return;
	}

	p->sent_len += bytes_to_send;
	if (p->sent_len == p->total_len)
		s->send_data = p->next;

	if (filling_sndbuf || !more) {
		/*
		 * We've exactly filled the TCP send buffer with this write.
		 * Call tcp_output() immediately to flush the data onto the
		 * wire and free up space in the send buffer once the remote
		 * side acknowledges.This step can also help avoid send delay.
		 */
		err = tcp_output (s->lw9p_pcb);
		if (err != ERR_OK)
			LW9P_DEBUG (LW9P_SEVERE, ("tcp_output err %d\n", err));
	} else {
		goto again;
	}
}

static err_t
lw9p_sent_cb (void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	lw9p_session_t *s = (lw9p_session_t *)arg;

	LW9P_DEBUG (LW9P_DBG, ("lw9p_sent_cb: acknowledged %u bytes\n", len));
	send_data_ack (s, len);
	/* If there is still data remaining, continue sending */
	if (s->lw9p_pcb && s->send_data)
		lw9p_send_data (s);

	return ERR_OK;
}

static void
free_9p_msg (lw9p_session_t *s, void *buffer, u32 len)
{
	mem_free (buffer);
}

static lw9p_results
send_9p_msg (lw9p_session_t *s, LW9PMsg *msg)
{
	if (!msg)
		return LW9P_RESULT_ERR_BUFFER;
	send_data_enqueue (s, msg, msg->size, free_9p_msg);
	err_t err = lw9p_sent_cb (s, s->lw9p_pcb, 0);

	return err == ERR_OK ? LW9P_RESULT_OK : LW9P_RESULT_ERR_TCP_WRITE;
}

static u16
rw_tag_next (lw9p_session_t *s)
{
	u16 tag = s->rw_tag;
	s->rw_tag = tag + 1 < 65535 ? tag + 1 : 0;
	return tag;
}

static lw9p_results
sending_Tread (lw9p_session_t *s)
{
	while (s->req_bytes > 0) {
		u16 rw_tag = rw_tag_next (s);
		LW9PMsg *msg = lw9p_helper_create_TRead_msg (s, rw_tag);
		tag_append (s, s->buffer_ptr, s->chunk_bytes, rw_tag);
		s->start_pos += s->chunk_bytes;
		s->buffer_ptr += s->chunk_bytes;
		s->req_bytes -= s->chunk_bytes;
		send_data_enqueue (s, msg, msg->size, free_9p_msg);
	}
	s->state = LW9P_TREAD_SENT;
	err_t err = lw9p_sent_cb (s, s->lw9p_pcb, 0);
	return err == ERR_OK ? LW9P_RESULT_OK : LW9P_RESULT_ERR_TCP_WRITE;
}

static lw9p_results
sending_Twrite (lw9p_session_t *s)
{
	while (s->req_bytes > 0) {
		u16 rw_tag = rw_tag_next (s);
		LW9PMsg *msg = lw9p_helper_create_TWrite_msg (s, rw_tag);
		tag_append (s, s->buffer_ptr, s->chunk_bytes, rw_tag);
		send_data_enqueue (s, msg, LW9P_TREAD_TWRITE_HDR_SIZE,
				   free_9p_msg);
		send_data_enqueue (s, s->buffer_ptr, s->chunk_bytes, NULL);
		s->start_pos += s->chunk_bytes;
		s->buffer_ptr += s->chunk_bytes;
		s->req_bytes -= s->chunk_bytes;
	}
	s->state = LW9P_TWRITE_SENT;
	err_t err = lw9p_sent_cb (s, s->lw9p_pcb, 0);
	return err == ERR_OK ? LW9P_RESULT_OK : LW9P_RESULT_ERR_TCP_WRITE;
}

static lw9p_results
lw9p_send_rw_data (lw9p_session_t *s)
{
	set_client_state (s, BUSY_TRANSFER);
	return s->is_write ? sending_Twrite (s) : sending_Tread (s);
}

void
lw9p_wr_data_chunk (lw9p_session_t *s)
{
	if (s->req_bytes > 0) {
		int result = lw9p_send_rw_data (s);
		if (result != LW9P_RESULT_OK) {
			LW9P_DEBUG (LW9P_SEVERE,
				    ("rw data err result %d\n", result));
			lw9p_close (s, result);
		}
	} else {
		set_client_state (s, AVAILABLE);
		LW9P_DEBUG (LW9P_TRACE, ("9P transfer finished\n"));
		if (s->cb_arg)
			s->wr_done_fn (&s->cb_arg);
		else
			LW9P_DEBUG (LW9P_WARNING, ("cb_arg is NULL?\n"));
	}
}

static err_t
lw9p_pcb_close (lw9p_session_t *s)
{
	err_t error;

	tcp_err (s->lw9p_pcb, NULL);
	tcp_recv (s->lw9p_pcb, NULL);
	tcp_sent (s->lw9p_pcb, NULL);
	error = tcp_close (s->lw9p_pcb);

	if (error != ERR_OK) {
		LW9P_DEBUG (LW9P_SEVERE, ("pcb close failure\n"));
		tcp_abort (s->lw9p_pcb);
	}
	s->lw9p_pcb = NULL;

	return error;
}

static lw9p_results
receive_RWrite (lw9p_session_t *s)
{
	if (!s->tag) {
		s->state = LW9P_READY_FOR_RW;
		lw9p_wr_data_chunk (s);
	}
	return LW9P_RESULT_OK;
}

static void
cur_pbuf_move_forward (lw9p_session_t *s, struct pbuf *new_pbuf,
		       u16_t new_offset)
{
	if (new_pbuf != s->cur_pbuf) {
		pbuf_ref (new_pbuf);
		pbuf_free (s->cur_pbuf);
		s->cur_pbuf = new_pbuf;
	}
	s->cur_offset = new_offset;
}

static lw9p_results
receive_RRead (lw9p_session_t *s)
{
	u32 offset = s->cur_offset;
	struct pbuf *p = s->cur_pbuf;
	while (p) {
		if (offset >= p->len && offset > 0) {
			offset -= p->len;
			p = p->next;
			continue;
		}
		if (!s->rread_datasize)
			break;
		void *payload = p->payload + offset;
		u16_t len = p->len - offset;
		if (len > s->rread_datasize)
			len = s->rread_datasize;
		memcpy (s->rread_databuf, payload, len);
		s->rread_databuf += len;
		s->rread_datasize -= len;
		offset += len;
	}
	if (p) {
		cur_pbuf_move_forward (s, p, offset);
	} else {
		pbuf_free (s->cur_pbuf);
		s->cur_pbuf = NULL;
	}
	if (s->rread_datasize > 0) {
		s->state = LW9P_RREAD_CONTINUE;
		return LW9P_RESULT_OK;
	}
	if (!s->tag) {
		s->state = LW9P_READY_FOR_RW;
		lw9p_wr_data_chunk (s);
	} else {
		s->state = LW9P_TREAD_SENT;
	}
	return LW9P_RESULT_OK;
}

void
lw9p_close (lw9p_session_t *s, int reason)
{
	LW9P_DEBUG (LW9P_DBG,
		    ("lw9p_close: session %s reason=0x%x cb_arg=%p state=%d\n",
		     s->name, reason, s->cb_arg, s->state));
	s->state = LW9P_CLOSED;
	set_client_state (s, CONNECT_FAILED);

	if (s->lw9p_pcb)
		lw9p_pcb_close (s);
	else
		LW9P_DEBUG (LW9P_WARNING, ("lw9p close no pcb\n"));

	if (s->cb_arg) {
		LW9P_DEBUG (LW9P_DBG,
			    ("Clean the pending arg %s!\n", s->name));
		s->wr_done_fn (&s->cb_arg);
	}
}

static void
cur_pbuf_append (lw9p_session_t *s, struct pbuf *p)
{
	if (s->cur_pbuf) {
		pbuf_chain (s->cur_pbuf, p);
	} else {
		pbuf_ref (p);
		s->cur_pbuf = p;
		s->cur_offset = 0;
	}
}

static void
cur_pbuf_consume (lw9p_session_t *s, u32 consume_len)
{
	if (!consume_len)
		return;
	if (!s->cur_pbuf) {
		LW9P_DEBUG (LW9P_SEVERE, ("%s: pbuf NULL\n", __func__));
		return;
	}
	if (consume_len > s->cur_pbuf->tot_len) {
		LW9P_DEBUG (LW9P_SEVERE, ("%s: incorrect len %u > %u\n",
					  __func__, consume_len,
					  s->cur_pbuf->tot_len));
		return;
	}
	if (consume_len == s->cur_pbuf->tot_len) {
		pbuf_free (s->cur_pbuf);
		s->cur_pbuf = NULL;
	}
	u16_t new_offset;
	struct pbuf *new_pbuf = pbuf_skip (s->cur_pbuf, s->cur_offset +
					   consume_len, &new_offset);
	cur_pbuf_move_forward (s, new_pbuf, new_offset);
}

static LW9PMsgType
lw9p_parse_msg (lw9p_session_t *s)
{
	struct pbuf *p = s->cur_pbuf;
	LW9PMsg m;
	if (p->tot_len - s->cur_offset < sizeof m)
		return -1;
	pbuf_copy_partial (p, &m, sizeof m, s->cur_offset);
	u32 consume_len;
#define SET_CONSUME_LEN(len) do { \
		consume_len = len; \
		if (p->tot_len - s->cur_offset < consume_len) \
			return -1; \
	} while (0)
	switch (m.type) {
	case RVersion:
		SET_CONSUME_LEN (m.size);
		if (m.size < LW9P_HDR_LEN_BASE + 4 + 2 + 8)
			goto reterr;
		u32 server_msize;
		pbuf_copy_partial (p, &server_msize, sizeof server_msize,
				   s->cur_offset + LW9P_HDR_LEN_BASE);
		if (server_msize <
		    s->data_limit + LW9P_MAX_HDR_SIZE) {
			if (server_msize <= LW9P_MAX_HDR_SIZE)
				goto reterr;
			LW9P_DEBUG (LW9P_TRACE,
				    ("msize client %u server %u\n",
				     s->data_limit + LW9P_MAX_HDR_SIZE,
				     server_msize));
			s->data_limit = server_msize - LW9P_MAX_HDR_SIZE;
		}
		/* The string "9P2000.L" is 8 bytes long */
		char val_char[9];
		pbuf_copy_partial (p, val_char, sizeof val_char,
				   s->cur_offset + LW9P_HDR_LEN_BASE + 4 + 2 +
				   8);
		val_char[8] = '\0';
		LW9P_DEBUG (LW9P_TRACE, ("Got RVersion %s\n", val_char));
		break;
	case RGetattr:
		SET_CONSUME_LEN (m.size);
		LW9P_DEBUG (LW9P_TRACE,
			    ("Got RGetattr, m.size=%u\n", m.size));
		if (m.size >= 64) {
			u64 val_64;
			pbuf_copy_partial (p, &val_64, sizeof val_64,
					   s->cur_offset + 56);
			LW9P_DEBUG (LW9P_TRACE, ("filesize %llu\n", val_64));
			spinlock_lock (&s->lw9p_lock);
			s->image_size = val_64;
			spinlock_unlock (&s->lw9p_lock);
		} else {
			LW9P_DEBUG (LW9P_TRACE,
				    ("RGetattr: no filesize, short len=%u\n",
				     m.size));
		}
		break;
	case RWrite:
		SET_CONSUME_LEN (m.size);
		u32 rwrite_datasize;
		pbuf_copy_partial (p, &rwrite_datasize, sizeof rwrite_datasize,
				   s->cur_offset + LW9P_HDR_LEN_BASE);
		if (!tag_delete (s, rwrite_datasize, m.tag))
			LW9P_DEBUG (LW9P_SEVERE, ("Invalid Rwrite\n"));
		LW9P_DEBUG (LW9P_TRACE, ("Got RWrite\n"));
		break;
	case Rlerror:
		SET_CONSUME_LEN (m.size);
		LW9P_DEBUG (LW9P_SEVERE, ("Got Rlerror \n"));
		break;
	case RAttach:
		SET_CONSUME_LEN (m.size);
		LW9P_DEBUG (LW9P_TRACE, ("Got RAttach Qid Path \n"));
		break;
	case RWalk:
		SET_CONSUME_LEN (m.size);
		LW9P_DEBUG (LW9P_TRACE, ("Got RWalk\n"));
		break;
	case ROpen:
		SET_CONSUME_LEN (m.size);
		LW9P_DEBUG (LW9P_TRACE, ("Got ROpen\n"));
		break;
	case RRead:
		SET_CONSUME_LEN (RW_HDR_LEN_WITH_COUNT);
		pbuf_copy_partial (p, &s->rread_datasize,
				   sizeof s->rread_datasize,
				   s->cur_offset + LW9P_HDR_LEN_BASE);
		LW9P_DEBUG (LW9P_TRACE, ("Got Rread\n"));
		if (s->rread_datasize + RW_HDR_LEN_WITH_COUNT != m.size) {
			LW9P_DEBUG (LW9P_SEVERE, ("Incorrect Rread size\n"));
			goto reterr;
		}
		s->rread_databuf = tag_delete (s, s->rread_datasize, m.tag);
		if (!s->rread_databuf) {
			LW9P_DEBUG (LW9P_SEVERE, ("Invalid Rread\n"));
			goto reterr;
		}
		break;
	default:
		LW9P_DEBUG (LW9P_SEVERE, ("Unknown lw9p msg %d\n", m.type));
	reterr:
		pbuf_free (p);
		s->cur_pbuf = NULL;
		return -1;
	}
#undef SET_CONSUME_LEN
	cur_pbuf_consume (s, consume_len);
	return m.type;
}

/* Main client state machine for receiving */
static void
lw9p_recv_process (lw9p_session_t *s, struct tcp_pcb *tpcb, struct pbuf *pb)
{
	int msg_type = -1;
	int result = LW9P_RESULT_ERR_STATE;

	if (lw9p_client_state (s) == CONNECT_FAILED)
		goto fail;

	if (pb)
		cur_pbuf_append (s, pb);
look_at_next:
	if (s->state != LW9P_RREAD_CONTINUE) {
		msg_type = lw9p_parse_msg (s);
		LW9P_DEBUG (LW9P_TRACE,
			    ("msg type: %d state %d\n", msg_type, s->state));
		if (msg_type < 0) {
			if (s->cur_pbuf)
				return;
			result = LW9P_RESULT_ERR_PARSE;
			goto fail;
		}
	}
	if (lw9p_client_state (s) == BUSY_TRANSFER) {
		switch (s->state) {
		case LW9P_TREAD_SENT:
			if (msg_type != RRead)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: RRead\n"));
			result = receive_RRead (s);
			if (result != LW9P_RESULT_OK)
				goto fail;
			break;
		case LW9P_RREAD_CONTINUE:
			LW9P_DEBUG (LW9P_TRACE, ("state: RRead continue\n"));
			result = receive_RRead (s);
			if (result != LW9P_RESULT_OK)
				goto fail;
			break;
		case LW9P_TWRITE_SENT:
			if (msg_type != RWrite)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: Rwrite\n"));
			result = receive_RWrite (s);
			if (result != LW9P_RESULT_OK)
				goto fail;
			break;
		default:
			result = LW9P_RESULT_ERR_STATE_NO_SUPPORT;
			LW9P_DEBUG (LW9P_SEVERE,
				    ("BUSY_TRANSFER unhandled state (%d)\n",
				     s->state));
			goto fail;
		}
	} else {
		LW9PMsg *out_msg;

		switch (s->state) {
		case LW9P_TVERSION_SENT:
			if (msg_type != RVersion)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: RVersion\n"));
			out_msg = lw9p_helper_create_TAttach_msg (s);
			result = send_9p_msg (s, out_msg);
			if (result != LW9P_RESULT_OK)
				goto fail;
			s->state = LW9P_TATTACH_SENT;
			break;
		case LW9P_TATTACH_SENT:
			if (msg_type != RAttach)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: Rattach1\n"));
			out_msg = lw9p_helper_create_TGetattr_msg (0);
			result = send_9p_msg (s, out_msg);
			if (result != LW9P_RESULT_OK)
				goto fail;
			s->state = LW9P_TGETATTR_SENT_0;
			break;
		case LW9P_TGETATTR_SENT_0:
			if (msg_type != RGetattr)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: RGetattr\n"));
			out_msg = lw9p_helper_create_TWalk_msg (s, 0);
			result = send_9p_msg (s, out_msg);
			if (result != LW9P_RESULT_OK)
				goto fail;
			s->state = LW9P_TWALK_SENT_0;
			break;
		case LW9P_TWALK_SENT_0:
			if (msg_type != RWalk)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: Rwalk phase 0\n"));
			out_msg = lw9p_helper_create_TWalk_msg (s, 1);
			result = send_9p_msg (s, out_msg);
			if (result != LW9P_RESULT_OK)
				goto fail;
			s->state = LW9P_TWALK_SENT_1;
			break;
		case LW9P_TWALK_SENT_1:
			if (msg_type != RWalk)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: Rwalk phase 1\n"));
			out_msg = lw9p_helper_create_TOpen_msg (s);
			result = send_9p_msg (s, out_msg);
			if (result != LW9P_RESULT_OK)
				goto fail;
			s->state = LW9P_TOPEN_SENT;
			break;
		case LW9P_TOPEN_SENT:
			if (msg_type != ROpen)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: Ropen\n"));
			out_msg = lw9p_helper_create_TGetattr_msg (2);
			result = send_9p_msg (s, out_msg);
			if (result != LW9P_RESULT_OK)
				goto fail;
			s->state = LW9P_TGETATTR_SENT_1;
			break;
		case LW9P_TGETATTR_SENT_1:
			if (msg_type != RGetattr)
				goto fail;
			LW9P_DEBUG (LW9P_TRACE, ("state: RGetattr phase1\n"));
			s->state = LW9P_READY_FOR_RW;
			set_client_state (s, AVAILABLE);
			result = LW9P_RESULT_OK;
			/* There is a pending reading request */
			if (s->cb_arg && s->pending_req) {
				LW9P_DEBUG (LW9P_TRACE,
					    ("Clean pending req %s\n",
					     s->name));
				s->pending_req = 0;
				s->wr_done_fn (&s->cb_arg);
			}
			break;
		default:
			result = LW9P_RESULT_ERR_STATE_NO_SUPPORT;
			LW9P_DEBUG (LW9P_SEVERE,
				    ("Unknown state (%d)\n", s->state));
			goto fail;
		}
	}
	if (s->cur_pbuf)
		goto look_at_next;
	return;
fail:
	if (s->cur_pbuf) {
		pbuf_free (s->cur_pbuf);
		s->cur_pbuf = NULL;
	}
	lw9p_close (s, result);
}

/* Handle connection incoming data */
static err_t
lw9p_recv (void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	lw9p_session_t *s = (lw9p_session_t *)arg;

	if (err == ERR_OK) {
		if (p) {
			lw9p_recv_process (s, tpcb, p);
			tcp_recved (tpcb, p->tot_len);
			pbuf_free (p);
		} else {
			LW9P_DEBUG (LW9P_WARNING,
				    ("connection closed by remote host\n"));
			lw9p_close (s, LW9P_RESULT_ERR_CLOSED);
		}
	} else {
		LW9P_DEBUG (LW9P_SEVERE, ("failed to receive (%x)\n", err));
		lw9p_close (s, LW9P_RESULT_ERR_TCP_RECV);
	}
	return err;
}

static void
lw9p_err (void *arg, err_t err)
{
	if (arg != NULL) {
		lw9p_session_t *s = (lw9p_session_t *)arg;
		LW9P_DEBUG (LW9P_SEVERE,
			    ("fail connect to server (%x)\n", (err)));
		s->lw9p_pcb = NULL;
		lw9p_close (s, LW9P_RESULT_ERR_CONNECT);
	}
}

static err_t
lw9p_connected (void *arg, struct tcp_pcb *tpcb, err_t err)
{
	lw9p_session_t *s = (lw9p_session_t *)arg;

	if (err == ERR_OK) {
		LW9P_DEBUG (LW9P_TRACE, ("connected to server\n"));

		LW9PMsg *out_msg = lw9p_helper_create_TVersion_msg (s);
		if (!out_msg) {
			LW9P_DEBUG (LW9P_WARNING,
				    ("Failed to create TVersion Msg\n"));
			return ERR_MEM;
		}

		u8 sendErr = send_9p_msg (s, out_msg);
		if (sendErr != LW9P_RESULT_OK) {
			LW9P_DEBUG (LW9P_WARNING,
				    ("Failed to send TVersion Msg\n"));
		} else
			s->state = LW9P_TVERSION_SENT;
	} else {
		LW9P_DEBUG (LW9P_WARNING,
			    ("err %d lwip connect (%s)\n", err,
			     lwip_strerr (err)));
	}
	return err;
}

/* Open a tcp session */
void
lw9p_connect (void *arg)
{
	err_t error;
	lw9p_session_t *s = arg;

	set_client_state (s, CONNECTING);

	s->lw9p_pcb = tcp_new ();
	if (!s->lw9p_pcb) {
		LW9P_DEBUG (LW9P_SEVERE, ("cannot alloc lw9p_pcb\n"));
		return;
	}

	tcp_arg (s->lw9p_pcb, s);
	tcp_err (s->lw9p_pcb, lw9p_err);
	tcp_recv (s->lw9p_pcb, lw9p_recv);
	tcp_sent (s->lw9p_pcb, lw9p_sent_cb);

	error = tcp_connect (s->lw9p_pcb, &s->server_ip, s->server_port,
			     lw9p_connected);
	if (error != ERR_OK) {
		LW9P_DEBUG (LW9P_SEVERE,
			    ("cannot connect lw9p_pcb (%s)\n",
			     lwip_strerr (error)));
		lw9p_close (s, LW9P_RESULT_ERR_CONNECT);
	}
}
