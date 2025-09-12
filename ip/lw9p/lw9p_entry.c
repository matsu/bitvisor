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

#include <core/assert.h>
#include <core/mm.h>
#include <core/spinlock.h>
#include <string.h>
#include "lw9p.h"
#include "lwip/timeouts.h"
#include "lwip/sys.h"
#include "tcpip.h"

static int g_timer_running = 0;
static const u32 CHECK_INTERVAL = 500;
static const u32 CONNECT_TIMEOUT = 1500;
static u8 config_inited = 0;
static spinlock_t lw9p_global_lock;
static LIST1_DEFINE_HEAD (lw9p_session_t, s_session);

static void
lw9p_config_init (void)
{
	config_inited = 1;
	LIST1_HEAD_INIT (s_session);
	spinlock_init (&lw9p_global_lock);
}

static void
free_session (lw9p_session_t *s)
{
	if (!s)
		return;
	if (s->remote_filename) {
		free (s->remote_filename);
		s->remote_filename = NULL;
	}
	if (s->img_path) {
		free (s->img_path);
		s->img_path = NULL;
	}
	if (s->name) {
		free (s->name);
		s->name = NULL;
	}
	if (s->uname) {
		free (s->uname);
		s->uname = NULL;
	}
	free (s);
}

static char *
alloc_and_copy_str (const char *src)
{
	if (!src)
		return NULL;

	size_t len = strlen (src);

	if (len == 0)
		return NULL;

	char *dst = alloc (len + 1);
	if (!dst)
		return NULL;

	memcpy (dst, src, len + 1);

	return dst;
}

static lw9p_session_t *
create_session (char *name, char *filename, char *img_path, char *uname)
{
	lw9p_session_t *new_session = NULL;

	new_session = alloc (sizeof *new_session);
	memset (new_session, 0, sizeof *new_session);

	new_session->name = alloc_and_copy_str (name);
	if (!new_session->name)
		goto fail;

	new_session->img_path = alloc_and_copy_str (img_path);
	if (!new_session->img_path)
		goto fail;

	new_session->remote_filename = alloc_and_copy_str (filename);
	if (!new_session->remote_filename)
		goto fail;

	new_session->uname = alloc_and_copy_str (uname);
	if (!new_session->uname)
		goto fail;

	LIST1_PUSH (s_session, new_session);
	LW9P_DEBUG (LW9P_DBG,
		    ("new 9p session %s -> %s \n", new_session->name,
		     new_session->remote_filename));
	return new_session;

fail:
	LW9P_DEBUG (LW9P_SEVERE, ("new 9p name %s creation failed\n", name));
	if (new_session)
		free_session (new_session);

	return NULL;
}

static void
remove_session (lw9p_session_t *s)
{
	if (s == NULL)
		return;

	LW9P_DEBUG (LW9P_DBG, ("removing session %s \n", s->name));
	LIST1_DEL (s_session, s);
	free_session (s);
}

/*
 * This design relies on a single lwIP timer (via sys_timeout) and a linked
 * list of sessions. Whenever the timer fires, it scans the list for any
 * expired (CONNECT_TIMEOUT) connections, marks them failed, and stops if none
 * remain in CONNECTING. This approach centralizes timeout handling without
 * multiple timers.
 */
static void
global_timeout_handler (void *arg)
{
	u32 now;
	int still_connecting = 0;
	lw9p_session_t *s;

	now = sys_now ();
	LW9P_DEBUG (LW9P_DBG, ("global timeout start\n"));

	spinlock_lock (&lw9p_global_lock);
	LIST1_FOREACH (s_session, s) {
		if (lw9p_client_state (s) == CONNECTING) {
			still_connecting = 1;
			if ((now - s->timer_start_time) >= CONNECT_TIMEOUT) {
				set_client_state (s, CONNECT_FAILED);
				LW9P_DEBUG (LW9P_WARNING,
					    ("session %s timed out!\n",
					     s->name));
				lw9p_close (s, LW9P_RESULT_ERR_CONNECT);
			}
		} else if (lw9p_client_state (s) == AVAILABLE) {
			LW9P_DEBUG (LW9P_DBG,
				    ("session %s is AVAILABLE\n", s->name));
		}
	}
	spinlock_unlock (&lw9p_global_lock);

	/*
	 * If still_connecting == 1 => it indicates there is at least one
	 * session still in CONNECTING state. In this case, schedule another
	 * check. If no sessions remain in the CONNECTING state, stop the
	 * timer.
	 */
	if (still_connecting)
		sys_timeout (CHECK_INTERVAL, global_timeout_handler, NULL);
	else
		g_timer_running = 0;
}

void *
lw9p_session_init (const lw9p_session_params_t *params)
{
	lw9p_session_t *s;

	if (!config_inited)
		lw9p_config_init ();
	if (!params) {
		LW9P_DEBUG (LW9P_SEVERE, ("params is NULL\n"));
		return NULL;
	}

	if (!params->wr_done_fn) {
		LW9P_DEBUG (LW9P_SEVERE,
			    ("lw9p_session_init: NULL pointer in params\n"));
		return NULL;
	}
	spinlock_lock (&lw9p_global_lock);
	s = create_session (params->devname, params->filename,
			    params->img_path, params->uname);
	spinlock_unlock (&lw9p_global_lock);
	if (s == NULL)
		return NULL;
	LW9P_DEBUG (LW9P_DBG, ("lw9p_session_init: session %s \n", s->name));
	s->wr_done_fn = params->wr_done_fn;
	s->data_limit = params->data_limit_9p > 0 ? params->data_limit_9p :
		LW9P_DEFAULT_DATA_LIMIT;
	s->server_port = params->server_port;
	s->readonly = params->readonly;
	set_client_state (s, SESSION_INIT);
	IP4_ADDR (&s->server_ip, params->server_ip[0], params->server_ip[1],
		  params->server_ip[2], params->server_ip[3]);
	s->uid = params->uid;
	spinlock_init (&s->lw9p_lock);
	s->cb_arg = NULL;

	return s;
}

static void
lw9p_rw_data_sub (void *arg)
{
	lw9p_session_t *s = arg;
	if (s->state == LW9P_READY_FOR_RW) {
		s->state = LW9P_RW_QUEUED;
		s->rw_queuing = 0;
		lw9p_wr_data_chunk (s);
	} else {
		LW9P_DEBUG (LW9P_SEVERE, ("state not ready %d\n", s->state));
	}
}

void
lw9p_rw_data (void *arg, const lw9p_rw_params_t *params)
{
	lw9p_session_t *s = arg;
	/* This routine must not be called again before the previous
	 * request completes.  Not thread-safe.  A simple check is in
	 * place to detect obvious misuse. */
	if (++s->rw_queuing == 1) {
		s->start_pos = params->start_pos;
		s->req_bytes = params->req_bytes;
		s->is_write = params->is_write;
		s->cb_arg = params->cb_arg;
		s->buffer_ptr = params->buffer_ptr;
		tcpip_begin (lw9p_rw_data_sub, s);
	} else {
		LW9P_DEBUG (LW9P_SEVERE, ("rw_queueing!=0\n"));
	}
}

static void
lw9p_session_close_sub (void *arg)
{
	lw9p_session_t *s = arg;
	lw9p_close (s, LW9P_RESULT_ERR_MEDIA);
	spinlock_lock (&lw9p_global_lock);
	remove_session (s);
	spinlock_unlock (&lw9p_global_lock);
}

static void
lw9p_setup_timer (void *arg)
{
	lw9p_session_t *s = arg;
	if (!g_timer_running) {
		g_timer_running = 1;
		LW9P_DEBUG (LW9P_DBG, ("dev %s start timer\n", s->name));
		sys_timeout (CHECK_INTERVAL, global_timeout_handler, NULL);
	}
}

void
lw9p_session_close (void *arg)
{
	lw9p_session_t *s = arg;
	tcpip_begin (lw9p_session_close_sub, s);
}

void
lw9p_session_connect (void *arg)
{
	lw9p_session_t *s = arg;
	tcpip_begin (lw9p_connect, s);
}

void
lw9p_put_cb_arg_wait (void *arg, void *cb_arg)
{
	lw9p_session_t *s = arg;

	LW9P_DEBUG (LW9P_DBG, ("callback arg is written\n"));
	s->cb_arg = cb_arg;
	s->pending_req = 1;
	s->timer_start_time = sys_now ();

	tcpip_begin (lw9p_setup_timer, s);
}

client_state_enum
lw9p_client_state (void *arg)
{
	u8 state;
	lw9p_session_t *s = arg;

	if (s == NULL)
		return CONNECT_FAILED;

	spinlock_lock (&s->lw9p_lock);
	state = s->client_state;
	spinlock_unlock (&s->lw9p_lock);

	return state;
}

u64
lw9p_query_img_size (void *arg)
{
	u64 bytes;
	lw9p_session_t *s = arg;

	if (s == NULL)
		return 0;

	spinlock_lock (&s->lw9p_lock);
	bytes = s->image_size;
	spinlock_unlock (&s->lw9p_lock);

	return bytes;
}
