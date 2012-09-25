/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#include <lib_lineinput.h>
#include <lib_syscalls.h>

#define NUM_OF_CH 8192
#define NUM_OF_ST 128
#define LINEMAX 1024
#define TYPE1 unsigned short
#define TYPE2 unsigned char
#define T1MAX 0xFFFF
#define T2MAX 0xFF

struct lineinput_char {
	TYPE1 prev, next;
	char c;
};

struct lineinput_string {
	TYPE2 prev, next;
	TYPE1 ch;
	short n;
};

static struct lineinput_char ch[NUM_OF_CH];
static struct lineinput_string st[NUM_OF_ST];
static TYPE1 ch_free;
static TYPE2 st_free, st_history;
static int lineinputinit = 0;

static TYPE1
ch_get (TYPE1 *head)
{
	TYPE1 r;

	r = *head;
	if (r != T1MAX) {
		ch[r].prev = T1MAX;
		*head = ch[r].next;
	}
	return r;
}

static void
ch_put (TYPE1 *head, TYPE1 i)
{
	if (*head == T1MAX) {
		ch[i].prev = T1MAX;
		ch[i].next = T1MAX;
		*head = i;
	} else {
		ch[i].prev = T1MAX;
		ch[i].next = *head;
		ch[*head].prev = i;
		*head = i;
	}
}

static TYPE2
st_get (TYPE2 *head)
{
	TYPE2 r;

	r = *head;
	if (r != T2MAX) {
		st[r].prev = T2MAX;
		*head = st[r].next;
	}
	return r;
}

static void
st_put (TYPE2 *head, TYPE2 i)
{
	if (*head == T2MAX) {
		st[i].prev = T2MAX;
		st[i].next = T2MAX;
		*head = i;
	} else {
		st[i].prev = T2MAX;
		st[i].next = *head;
		st[*head].prev = i;
		*head = i;
	}
}

static void
lineinput_init (void)
{
	int i;

	ch_free = T1MAX;
	for (i = 0; i < NUM_OF_CH; i++)
		ch_put (&ch_free, i);
	st_free = T2MAX;
	for (i = 0; i < NUM_OF_ST; i++) {
		st[i].ch = T1MAX;
		st_put (&st_free, i);
	}
	st_history = T2MAX;
	lineinputinit = 1;
}

static void
drop_old_history (void)
{
	int i;

	for (i = st_history; i != T2MAX; i = st[i].next) {
		if (st[i].next == T2MAX)
			goto found;
	}
	return;
found:
	if (st[i].prev == T2MAX)
		st_history = T2MAX;
	else
		st[st[i].prev].next = T2MAX;
	while (st[i].ch != T1MAX)
		ch_put (&ch_free, ch_get (&st[i].ch));
	st_put (&st_free, i);
}

int
lineinput_desc (int kbd, int dsp, char *buf, int len)
{
	TYPE2 st_i, st_new;
	TYPE1 ch_cursor, ch_tmp;
	char c;
	int i;
#	define GET_CH_TMP() do { \
		ch_tmp = ch_get (&ch_free); \
		while (ch_tmp == T1MAX) { \
			drop_old_history (); \
			ch_tmp = ch_get (&ch_free); \
		} \
	} while (0)
	enum { DEL_OFF, DEL_U, DEL_K } delmode;

	if (!lineinputinit)
		lineinput_init ();
	if (len == 0)
		return 0;
	st_i = st_get (&st_free);
	if (st_i == T2MAX) {
		drop_old_history ();
		st_i = st_get (&st_free);
		if (st_i == T2MAX)
			return 0;
	}
	st_put (&st_history, st_i);
	GET_CH_TMP ();
	ch[ch_tmp].c = '\0';
	ch[ch_tmp].prev = ch[ch_tmp].next = T1MAX;
	ch_cursor = st[st_i].ch = ch_tmp;
	st_new = st_i;
	st[st_i].n = 0;
inputloop_deldone:
	delmode = DEL_OFF;
inputloop:
	switch (delmode) {
	case DEL_U:
		c = '\b';
		break;
	case DEL_K:
		c = 'd' & 31;
		break;
	default:
		c = msgsendint (kbd, 0);
	}
	if (c >= ' ' && c <= '~') {
		if (st[st_i].n < LINEMAX) {
			GET_CH_TMP ();
			ch[ch_tmp].c = c;
			ch[ch_tmp].prev = ch[ch_cursor].prev;
			ch[ch_tmp].next = ch_cursor;
			if (ch[ch_cursor].prev == T1MAX)
				st[st_i].ch = ch_tmp;
			else
				ch[ch[ch_cursor].prev].next = ch_tmp;
			ch[ch_cursor].prev = ch_tmp;
			msgsendint (dsp, c);
			st[st_i].n++;
		}
	prnt:
		for (ch_tmp = ch_cursor; ch_tmp != T1MAX;
		     ch_tmp = ch[ch_tmp].next) {
			if (ch[ch_tmp].next == T1MAX)
				break;
			msgsendint (dsp, ch[ch_tmp].c);
		}
		if (ch_tmp != T1MAX)
			msgsendint (dsp, ' ');
		for (ch_tmp = ch_cursor; ch_tmp != T1MAX;
		     ch_tmp = ch[ch_tmp].next)
			msgsendint (dsp, '\b');
		goto inputloop;
	}
	switch (c) {
	case 'f' & 31:
		ch_tmp = ch[ch_cursor].next;
		if (ch_tmp == T1MAX)
			goto inputloop;
		msgsendint (dsp, ch[ch_cursor].c);
		ch_cursor = ch_tmp;
		goto inputloop;
	case 'k' & 31:
		delmode = DEL_K;
		goto inputloop;
	case 'u' & 31:
		delmode = DEL_U;
		goto inputloop;
	case '\b':
	case 127:
	case 'b' & 31:
		ch_tmp = ch[ch_cursor].prev;
		if (ch_tmp == T1MAX)
			goto inputloop_deldone;
		ch_cursor = ch_tmp;
		msgsendint (dsp, '\b');
		if (c == ('b' & 31))
			goto inputloop;
	case 'd' & 31:
		if (ch[ch_cursor].next == T1MAX)
			goto inputloop_deldone;
		ch[ch[ch_cursor].next].prev = ch[ch_cursor].prev;
		ch_tmp = ch[ch_cursor].next;
		if (ch[ch_cursor].prev == T1MAX)
			st[st_i].ch = ch_tmp;
		else
			ch[ch[ch_cursor].prev].next = ch_tmp;
		ch_put (&ch_free, ch_cursor);
		ch_cursor = ch_tmp;
		st[st_i].n--;
		goto prnt;
	case '\n':
	case '\r':
		goto inputend;
	case 'p' & 31:
	case 'n' & 31:
	case 'a' & 31:
		while (ch[ch_cursor].prev != T1MAX) {
			msgsendint (dsp, '\b');
			ch_cursor = ch[ch_cursor].prev;
		}
		if (c == ('a' & 31))
			goto inputloop;
		while (ch[ch_cursor].next != T1MAX) {
			msgsendint (dsp, ' ');
			ch_cursor = ch[ch_cursor].next;
		}
		while (ch[ch_cursor].prev != T1MAX) {
			msgsendint (dsp, '\b');
			ch_cursor = ch[ch_cursor].prev;
		}
		if (c == ('p' & 31) && st[st_i].next != T2MAX)
			st_i = st[st_i].next;
		if (c == ('n' & 31) && st[st_i].prev != T2MAX)
			st_i = st[st_i].prev;
		ch_cursor = st[st_i].ch;
	case 'e' & 31:
		while (ch[ch_cursor].next != T1MAX) {
			msgsendint (dsp, ch[ch_cursor].c);
			ch_cursor = ch[ch_cursor].next;
		}
		goto inputloop;
	}
	goto inputloop;
inputend:
	for (ch_tmp = ch_cursor; ch_tmp != T1MAX;
	     ch_tmp = ch[ch_tmp].next) {
		if (ch[ch_tmp].next == T1MAX)
			break;
		msgsendint (dsp, ch[ch_tmp].c);
	}
	if (ch_tmp != T1MAX)
		msgsendint (dsp, ' ');
	msgsendint (dsp, '\b');
	msgsendint (dsp, '\n');
	i = 0;
	for (ch_tmp = st[st_i].ch; ch_tmp != T1MAX && len > 0;
	     ch_tmp = ch[ch_tmp].next, len--)
		buf[i++] = ch[ch_tmp].c;
	if (i > 0)
		i--;
	if (len == 0 && buf[i] != '\0')
		buf[i] = '\0';
	if (st[st_i].prev != T2MAX) {
		st[st[st_i].prev].next = st[st_i].next;
		if (st[st_i].next != T2MAX)
			st[st[st_i].next].prev = st[st_i].prev;
		st[st_i].prev = T2MAX;
		st[st_i].next = st_history;
		st[st_history].prev = st_i;
		st_history = st_i;
	}
	if (ch[st[st_new].ch].c == '\0') {
		if (st[st_new].prev == T2MAX)
			st_history = st[st_new].next;
		else
			st[st[st_new].prev].next = st[st_new].next;
		if (st[st_new].next != T2MAX)
			st[st[st_new].next].prev = st[st_new].prev;
		st_put (&st_free, st_new);		
	}
	return i;
}

int
lineinput (char *buf, int len)
{
	return lineinput_desc (0, 1, buf, len);
}
