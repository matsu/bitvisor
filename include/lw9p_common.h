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

#ifndef LW9P_COMMON_H
#define LW9P_COMMON_H

#include <share/vmm_types.h>

typedef enum {
	AVAILABLE = 0,
	BUSY_TRANSFER,
	SESSION_INIT,
	CONNECT_FAILED,
	CONNECTING,
} client_state_enum;

typedef struct {
	char *devname;
	void (*wr_done_fn) (void **arg);
	char *filename;
	char *img_path;
	u8 *server_ip;
	u16 server_port;
	bool readonly;
	u32 uid;
	char *uname;
	u32 data_limit_9p;
} lw9p_session_params_t;

typedef struct {
	u64 start_pos;
	u32 req_bytes;
	bool is_write;
	void *cb_arg;
	void *buffer_ptr;
} lw9p_rw_params_t;

void *lw9p_session_init (const lw9p_session_params_t *params);
void lw9p_rw_data (void *arg, const lw9p_rw_params_t *params);
void lw9p_session_close (void *arg);
void lw9p_session_connect (void *arg);
void lw9p_put_cb_arg_wait (void *arg, void *cb_arg);
client_state_enum lw9p_client_state (void *arg);
u64 lw9p_query_img_size (void *arg);

#endif /* LW9P_COMMON_H */
