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

enum {
	MSG_INT,
	MSG_BUF,
};

struct msgbuf {
	void *base;
	unsigned int len;
	int rw;
	long premap_handle;
};

static inline void
setmsgbuf_premap (struct msgbuf *mbuf, void *base, unsigned int len, int rw,
		  long premap_handle)
{
	mbuf->base = base;
	mbuf->len = len;
	mbuf->rw = rw;
	mbuf->premap_handle = premap_handle;
}

static inline void
setmsgbuf (struct msgbuf *mbuf, void *base, unsigned int len, int rw)
{
	setmsgbuf_premap (mbuf, base, len, rw, 0);
}

static inline long
msgpremapbuf (int desc, struct msgbuf *buf)
{
	return 0;
}

void nop (void);
void *msgsetfunc (int desc, void *func);
int msgregister (char *name, void *func);
int msgopen (char *name);
int msgclose (int desc);
int msgsendint (int desc, int data);
int msgsenddesc (int desc, int data);
int newprocess (char *name);
int msgsendbuf (int desc, int data, struct msgbuf *buf, int bufcnt);
int msgunregister (int desc);
void exitprocess (int retval);
int setlimit (int stacksize, int maxstacksize);
