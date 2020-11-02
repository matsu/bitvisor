/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2009 Igel Co., Ltd
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

/* Usage via dbgsh:

[Memory] the database is always empty on start
------------------------------------------------------------
> sqliteexample
Memory(m) or storage_io(s)? m
sqliteexample> create table foo(bar)
sqliteexample> insert into foo(bar) values(1)
sqliteexample> select * from foo
bar = 1

sqliteexample>
------------------------------------------------------------

[Storage_io] the database is persistent on a storage
------------------------------------------------------------
> sqliteexample
Memory(m) or storage_io(s)? s
id 1
Number of devices 4
Device number? 1
Start LBA? 0
End LBA? 20479
Device 1 LBA 0-10239,10240-20479 Storage size 175913280995328 (may be incorrect)
Continue(y/n)? y
sqliteexample> select count(*) from a
count(*) = 52

sqliteexample> insert into a(a) values(2)
sqliteexample> select count(*) from a
count(*) = 53

sqliteexample>
> sqliteexample
Memory(m) or storage_io(s)? s
id 2
Number of devices 4
Device number? 1
Start LBA? 0
End LBA? 20479
Device 1 LBA 0-10239,10240-20479 Storage size 175913280995328 (may be incorrect)
Continue(y/n)? y
sqliteexample> select count(*) from a
count(*) = 53

sqliteexample> select * from a where a=2
a = 2
b = NULL
c = NULL

sqliteexample>
------------------------------------------------------------ */

#include <lib_lineinput.h>
#include <lib_printf.h>
#include <lib_stdlib.h>
#include <lib_string.h>
#include <lib_syscalls.h>
#include <lib_storage_io.h>
#include "assert.h"
#include "sqlite3.h"
#include "vvfs.h"

struct stordata {
	int id;
	int dev;
	long long start;
	long long size;
	long long *lastblk[2];
};

struct revdata {
	long long size;
	v_rw_t *rw;
	void *rw_param;
	long long *lastblk[2];
};

static int waitd;
static char waitflag;

int
sqlite3_os_init (void)
{
	return SQLITE_OK;
}

int
sqlite3_os_end (void)
{
	return SQLITE_OK;
}

static int
callback (void *NotUsed, int argc, char **argv, char **azColName)
{
	int i;
	for(i = 0; i < argc; i++)
		printf ("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	printf ("\n");
	return 0;
}

static int
mem_rw (void *rw_param, void *rbuf, const void *wbuf, int nblk, long long lba)
{
	static char storage1[524288];
	static char storage2[524288];
	char *storage = rw_param ? storage2 : storage1;
	int size = rw_param ? sizeof storage2 : sizeof storage1;
	if (!nblk)
		return 0;	/* Flush */
	if (lba >= size >> 12)
		return -1;
	if (!rbuf && !wbuf && nblk == 1) /* Set last LBA */
		return 0;
	if (lba + nblk > size >> 12)
		nblk = (size >> 12) - lba;
	if (rbuf) {
		memcpy (rbuf, &storage[lba << 12], nblk << 12);
		return nblk;
	}
	if (wbuf) {
		memcpy (&storage[lba << 12], (void *)wbuf, nblk << 12);
		return nblk;
	}
	return -1;
}

static void
callback_rw (void *data, int len)
{
	int *p = data;
	if (len > 0)
		*p = 1;
	waitflag = 1;
}

static int
stor_rw (void *rw_param, void *rbuf, const void *wbuf, int nblk, long long lba)
{
	struct stordata *p = rw_param;
	if (!nblk)
		return 0;	/* Flush: not implemented */
	if (lba >= p->size)
		return -1;
	if (!rbuf && !wbuf && nblk == 1) { /* Set last LBA */
		if (lba + 1 + *p->lastblk[1] + 1 < p->size) {
			*p->lastblk[0] = lba;
			return 0;
		}
		return -1;
	}
	if (lba + nblk > p->size)
		nblk = p->size - lba;
	int ok = 0;
	if (rbuf) {
		waitflag = 0;
		if (storage_io_aread (p->id, p->dev, rbuf, nblk << 12,
				      (lba << 12) + p->start,
				      callback_rw, &ok) < 0)
			return -1;
	} else if (wbuf) {
		waitflag = 0;
		if (storage_io_awrite (p->id, p->dev, (void *)wbuf, nblk << 12,
				       (lba << 12) + p->start,
				       callback_rw, &ok) < 0)
			return -1;
	}
	if (rbuf || wbuf) {
		struct msgbuf mbuf;
		setmsgbuf (&mbuf, &waitflag, sizeof waitflag, 0);
		msgsendbuf (waitd, 0, &mbuf, 1);
		if (ok)
			return nblk;
	}
	return -1;
}

static int
rev_rw (void *rw_param, void *rbuf, const void *wbuf, int nblk, long long lba)
{
	struct revdata *r = rw_param;
	if (rbuf || wbuf) {
		for (int i = 0; i < nblk; i++) {
			long long r_lba = r->size - lba - nblk + i;
			void *r_rbuf = NULL;
			const void *r_wbuf = NULL;
			if (rbuf)
				r_rbuf = rbuf + ((nblk - i - 1) << 12);
			if (wbuf)
				r_wbuf = wbuf + ((nblk - i - 1) << 12);
			int ret = r->rw (r->rw_param, r_rbuf, r_wbuf, 1,
					 r_lba);
			if (ret < 0)
				return ret;
		}
		return nblk;
	}
	if (!rbuf && !wbuf && nblk == 1) { /* Set last LBA */
		if (lba + 1 + *r->lastblk[1] + 1 < r->size) {
			*r->lastblk[0] = lba;
			return 0;
		}
		return -1;
	}
	return r->rw (r->rw_param, rbuf, wbuf, nblk, lba);
}

static void
callback_size (void *data, long long size)
{
	long long *p = data;
	*p = size;
	waitflag = 1;
}

static int
sqltest (void)
{
	sqlite3 *db;
	if (sqlite3_open ("a", &db) != SQLITE_OK) {
		printf ("Can't open database: %s\n", sqlite3_errmsg (db));
		sqlite3_close (db);
		return 1;
	}
	for (;;) {
		static char buf[256];
		printf ("sqliteexample> ");
		lineinput (buf, 256);
		if (!buf[0]) {
			sqlite3_close (db);
			return 0;
		}
		char *zErrMsg = NULL;
		int rc = sqlite3_exec (db, buf, callback, 0, &zErrMsg);
		if (rc != SQLITE_OK) {
			printf ("SQL error: %s\n", zErrMsg);
			sqlite3_free (zErrMsg);
		}
	}
}

int
_start (int a1, int a2)
{
	static char heap[1048576] __attribute__ ((aligned (8)));
	sqlite3_config (SQLITE_CONFIG_HEAP, heap, sizeof heap, 32);
	if (sqlite3_initialize () != SQLITE_OK) {
		printf ("sqlite3_initialize failed\n");
		return 1;
	}
	sqlite3_vfs_register (v_vfs(), 1);
	printf ("Memory(m) or storage_io(s)? ");
	static char buf[256];
	lineinput (buf, sizeof buf);
	if (!strcmp (buf, "m")) {
		v_register ("a", 12, mem_rw, NULL);
		v_register ("a-journal", 12, mem_rw, mem_rw);
		exitprocess (sqltest ());
	}
	if (strcmp (buf, "s"))
		exitprocess (0);
	int id = storage_io_init ();
	printf ("id %d\n", id);
	printf ("Number of devices %d\n", storage_io_get_num_devices (id));
	printf ("Device number? ");
	lineinput (buf, sizeof buf);
	char *p;
	int dev = (int)strtol (buf, &p, 0);
	if (p == buf)
		exitprocess (0);
	printf ("Start LBA? ");
	lineinput (buf, sizeof buf);
	long start = strtol (buf, &p, 0);
	if (p == buf)
		exitprocess (0);
	printf ("End LBA? ");
	lineinput (buf, sizeof buf);
	long end = strtol (buf, &p, 0);
	if (p == buf)
		exitprocess (0);
	if (end < start)
		exitprocess (0);
	printf ("Device %d LBA %ld-%ld Storage size ", dev, start, end);
	waitd = msgopen ("wait");
	if (waitd < 0) {
		printf ("msgopen \"wait\" failed\n");
		exitprocess (1);
	}
	long long size = 0;
	waitflag = 0;
	if (storage_io_aget_size (id, dev, callback_size, &size) < 0) {
		printf ("storage_io_aget_size failed\n");
		exitprocess (1);
	}
	struct msgbuf mbuf;
	setmsgbuf (&mbuf, &waitflag, sizeof waitflag, 0);
	msgsendbuf (waitd, 0, &mbuf, 1);
	printf ("%lld (may be incorrect)\n", size);
	printf ("Continue(y/n)? ");
	lineinput (buf, sizeof buf);
	if (!strcmp (buf, "y")) {
		long long lastblk1 = 0;
		long long lastblk2 = 0;
		struct stordata s = {
			.id = id,
			.dev = dev,
			.start = start << 9,
			.size = (end - start + 1) >> 3,
			.lastblk = {
				&lastblk1,
				&lastblk2,
			},
		};
		struct cat_data *sc = cat_new (stor_rw, &s, 12);
		v_register ("a", 12, cat_rw, sc);
		struct revdata r = {
			.size = s.size,
			.rw = cat_rw,
			.rw_param = sc,
			.lastblk = {
				&lastblk2,
				&lastblk1,
			},
		};
		struct cat_data *rc = cat_new (rev_rw, &r, 12);
		v_register ("a-journal", 12, cat_rw, rc);
		exitprocess (sqltest ());
	}
	exitprocess (0);
	return 0;
}
