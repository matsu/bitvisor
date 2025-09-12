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
#include <string.h>
#include "lw9p.h"
#include "lw9p_helper.h"

#define WRITE_U16_OFFSET(val) \
	do { \
		*(u16 *)(msg->payload + offset) = (val); \
		offset += 2; \
	} while (0)

#define WRITE_U32_OFFSET(val) \
	do { \
		*(u32 *)(msg->payload + offset) = (val); \
		offset += 4; \
	} while (0)

#define WRITE_U64_OFFSET(val) \
	do { \
		*(u64 *)(msg->payload + offset) = (val); \
		offset += 8; \
	} while (0)

#define WRITE_MEM_OFFSET(src, len) \
	do { \
		memcpy (msg->payload + offset, (src), (len)); \
		offset += (len); \
	} while (0)

static LW9PMsg *
alloc_and_init_msg (uint8_t type, size_t total_size)
{
	LW9PMsg *msg = mem_malloc (total_size);
	if (!msg)
		return NULL;
	memset (msg, 0, total_size);

	msg->size = total_size;
	msg->type = type;
	msg->tag = 0;
	return msg;
}

LW9PMsg *
lw9p_helper_create_TVersion_msg (lw9p_session_t *s)
{
	u16 ver_len;
	size_t total_size;
	size_t offset = 0;

	ver_len = strlen ("9P2000.L");
	/* header: 7 bytes, msize: 4 bytes, version length: 2 bytes */
	total_size = LW9P_HDR_LEN_BASE + 4 + 2 + ver_len;

	LW9PMsg *msg = alloc_and_init_msg (TVersion, total_size);
	msg->tag = ~0;		/* NOTAG */

	WRITE_U32_OFFSET (s->data_limit + LW9P_MAX_HDR_SIZE);
	WRITE_U16_OFFSET (ver_len);
	WRITE_MEM_OFFSET ("9P2000.L", ver_len);

	return msg;
}

LW9PMsg *
lw9p_helper_create_TAttach_msg (lw9p_session_t *s)
{
	u16 uname_len;
	u16 aname_len;
	size_t total_size;
	size_t offset = 0;

	uname_len = strlen (s->uname);
	aname_len = strlen (s->img_path);
	/* header: 7 bytes, fid: 4 bytes, afid: 4 bytes, uname_len: 2 bytes,
	   uname string, aname_len: 2bytes, aname string, uid: bytes */
	total_size = LW9P_HDR_LEN_BASE + LW9P_HDR_LEN_FID + 4 + 2 + uname_len +
		2 + aname_len + 4;

	LW9PMsg *msg = alloc_and_init_msg (TAttach, total_size);

	WRITE_U32_OFFSET (0); /* fid */
	WRITE_U32_OFFSET (0xffffffff); /* afid */
	WRITE_U16_OFFSET (uname_len); /* uname */
	WRITE_MEM_OFFSET (s->uname, uname_len);
	WRITE_U16_OFFSET (aname_len); /* aname */
	WRITE_MEM_OFFSET (s->img_path, aname_len);
	WRITE_U32_OFFSET (s->uid); /* uid */

	return msg;
}

LW9PMsg *
lw9p_helper_create_TWalk_msg (lw9p_session_t *s, u8 phase)
{
	u16 fn_len;
	size_t total_size;
	size_t offset = 0;

	fn_len = strlen (s->remote_filename);

	/* Phase 0 (initial stage): header: 7 bytes, fid: 4 bytes, newfid:
	 * 4 bytes, nwname: 2 bytes, u16: 2 bytes
	 * Phase 1 (subsequent stage): header: 7 bytes, fid: 4 bytes,
	 * newfid: 4bytes, nwname: 2 bytes */
	total_size = LW9P_HDR_LEN_BASE + LW9P_HDR_LEN_FID + 4 + 2;
	if (phase == 0)
		total_size += 2 + fn_len;

	LW9PMsg *msg = alloc_and_init_msg (TWalk, total_size);

	if (phase == 0) {
		WRITE_U32_OFFSET (0); /* fid */
		WRITE_U32_OFFSET (1); /* newfid */
		WRITE_U16_OFFSET (1); /* nwname */
		WRITE_U16_OFFSET (fn_len);
		WRITE_MEM_OFFSET (s->remote_filename, fn_len);
	} else {
		WRITE_U32_OFFSET (1); /* fid */
		WRITE_U32_OFFSET (2); /* newfid */
		WRITE_U16_OFFSET (0); /* nwname */
	}
	return msg;
}

LW9PMsg *
lw9p_helper_create_TRead_msg (lw9p_session_t *s, u16 tag)
{
	size_t total_size;
	size_t offset = 0;

	s->chunk_bytes = min (s->req_bytes, s->data_limit);
	total_size = LW9P_TREAD_TWRITE_HDR_SIZE;

	LW9PMsg *msg = alloc_and_init_msg (TRead, total_size);
	msg->tag = tag;

	WRITE_U32_OFFSET (1); /* fid */
	WRITE_U64_OFFSET (s->start_pos); /* file offset */
	WRITE_U32_OFFSET (s->chunk_bytes); /* request bytes */

	return msg;
}

LW9PMsg *
lw9p_helper_create_TWrite_msg (lw9p_session_t *s, u16 tag)
{
	size_t offset = 0;

	s->chunk_bytes = min (s->req_bytes, s->data_limit);
	/* Only the header size is allocated, but msg->size includes the size
	   of the data to be sent later. */
	LW9PMsg *msg = alloc_and_init_msg (TWrite, LW9P_TREAD_TWRITE_HDR_SIZE);
	msg->tag = tag;
	msg->size += s->chunk_bytes;

	WRITE_U32_OFFSET (1); /* fid */
	WRITE_U64_OFFSET (s->start_pos); /* file offset */
	WRITE_U32_OFFSET (s->chunk_bytes);

	return msg;
}

LW9PMsg *
lw9p_helper_create_TOpen_msg (lw9p_session_t *s)
{
	size_t total_size;
	size_t offset = 0;

	/* header: 7 bytes, fid: 4 bytes, flag: 4 bytes */
	total_size = LW9P_HDR_LEN_BASE + LW9P_HDR_LEN_FID + 4;

	LW9PMsg *msg = alloc_and_init_msg (TOpen, total_size);

	WRITE_U32_OFFSET (1); /* fid */
	WRITE_U32_OFFSET (s->readonly ? 0 : 2); /* mode */

	return msg;
}

LW9PMsg *
lw9p_helper_create_TGetattr_msg (u8 fid)
{
	size_t total_size;
	size_t offset = 0;

	/* header: 7 bytes, fid: 4 bytes, getattr_flags: 8 bytes */
	total_size = LW9P_HDR_LEN_BASE + LW9P_HDR_LEN_FID + 8;

	LW9PMsg *msg = alloc_and_init_msg (TGetattr, total_size);

	WRITE_U32_OFFSET (fid); /* fid */
	WRITE_U64_OFFSET (0x07ff); /* getattr_flags */

	return msg;
}
