/*
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

#include "string.h"
#include "sqlite3.h"
#include "vvfs.h"

#define NCACHE 4

struct cat_data {
	unsigned char *catbuf;
	v_rw_t *rw;
	void *rw_param;
	long long lba;
	int block_size_shift;
	int block_size;
	unsigned int ncatblk;
};

struct cache {
	struct cache *next;
	struct cache *next_dirty;
	unsigned char *buf;
	long long buf_lba;
};

struct v_filelist {
	struct v_filelist *next;
	const char *path;
	v_rw_t *rw;
	void *rw_param;
	struct cache cache[NCACHE];
	struct cache *c;
	struct cache *cref;
	struct cache *cdirty;
	struct cache *cfree;
	long long size;
	long long lastlba;
	long long lastwrittenlba;
	int block_size_shift;
	int block_size;
};

struct vFile {
	sqlite3_file base;
	struct v_filelist *v;
};

static struct v_filelist *v_filehead;

void
v_register (const char *name, int block_size_shift, v_rw_t *rw, void *rw_param)
{
	struct v_filelist *v;
	v = sqlite3_malloc (sizeof *v);
	v->next = v_filehead;
	v->path = name;
	v->block_size_shift = block_size_shift;
	v->block_size = 1 << block_size_shift;
	v->size = -1;
	v->lastlba = -1;
	v->lastwrittenlba = -1;
	v->cref = NULL;
	v->cdirty = NULL;
	v->cfree = NULL;
	for (int i = 0; i < NCACHE; i++) {
		v->cache[i].buf = sqlite3_malloc (1 << block_size_shift);
		v->cache[i].next = v->cfree;
		v->cfree = &v->cache[i];
	}
	v->c = NULL;
	v->rw = rw;
	v->rw_param = rw_param;
	v_filehead = v;
}

void *
cat_new (v_rw_t *rw, void *rw_param, int block_size_shift)
{
	struct cat_data *p;
	p = sqlite3_malloc (sizeof *p);
	p->catbuf = sqlite3_malloc (NCACHE << block_size_shift);
	p->rw = rw;
	p->rw_param = rw_param;
	p->block_size_shift = block_size_shift;
	p->block_size = 1 << block_size_shift;
	p->ncatblk = 0;
	return p;
}

int
cat_rw (void *rw_param, void *rbuf, const void *wbuf, int nblk, long long lba)
{
	struct cat_data *p = rw_param;
	if (p->ncatblk > 0) {
		if (wbuf && nblk == 1 && lba == p->lba + p->ncatblk &&
		    p->ncatblk < NCACHE)
			goto cat;
		int ret = p->rw (p->rw_param, NULL, p->catbuf, p->ncatblk,
				 p->lba);
		if (ret < 0)
			return ret;
		p->ncatblk = 0;
	}
	if (wbuf && nblk == 1) {
		p->lba = lba;
	cat:
		memcpy (&p->catbuf[p->ncatblk << p->block_size_shift], wbuf,
			p->block_size);
		p->ncatblk++;
		return 1;
	}
	return p->rw (p->rw_param, rbuf, wbuf, nblk, lba);
}

static int
v_sync (struct v_filelist *v)
{
	if (v->rw (v->rw_param, NULL, NULL, 0, 0) < 0)
		return -1;
	return 0;
}

static int
v_flush (struct v_filelist *v)
{
	struct cache *p = v->cdirty;
	if (!p)
		return 0;
	for (; p; p = p->next_dirty) {
		long long lba = p->buf_lba;
		int ret = v->rw (v->rw_param, NULL, p->buf, 1, lba);
		if (ret > 0 && v->lastwrittenlba < lba + ret - 1)
			v->lastwrittenlba = lba + ret - 1;
		if (ret != 1)
			return -1;
		v->cdirty = p->next_dirty;
	}
	return 0;
}

static int
v_rw_nblk (struct v_filelist *v, void *rbuf, const void *wbuf, int nblk,
	   long long lba)
{
	if (v_flush (v) < 0)
		return -1;
	if (wbuf) {
		for (struct cache *p = v->cref; p; p = p->next) {
			if (!p->next) {
				p->next = v->cfree;
				v->cfree = v->cref;
				v->c = NULL;
				break;
			}
		}
	}
	int ret = v->rw (v->rw_param, rbuf, wbuf, nblk, lba);
	if (wbuf)
		if (ret > 0 && v->lastwrittenlba < lba + ret - 1)
			v->lastwrittenlba = lba + ret - 1;
	return ret;
}

static int
v_cmove (struct v_filelist *v, long long lba)
{
	if (v->c && lba == v->c->buf_lba)
		return 0;	/* Fast path */
	for (struct cache **p = &v->cref; *p; p = &(*p)->next) {
		if (lba == (*p)->buf_lba) {
			/* Found */
			struct cache *q = *p;
			*p = q->next;
			q->next = v->cref;
			v->cref = q;
			v->c = q;
			return 0;
		}
		if (!(*p)->next && !v->cfree) {
			/* Full */
			if (v_flush (v) < 0)
				return -1;
			v->cfree = *p;
			*p = NULL;
			break;
		}
	}
	/* New */
	struct cache *p = v->cfree;
	if (!p)
		return -1;
	if ((v->lastwrittenlba < 0 || lba <= v->lastwrittenlba) &&
	    v->rw (v->rw_param, p->buf, NULL, 1, lba) != 1)
		return -1;
	p->buf_lba = lba;
	v->cfree = p->next;
	p->next = v->cref;
	v->cref = p;
	v->c = p;
	return 0;
}

static int
v_csetdirty (struct v_filelist *v, long long lba)
{
	if (!v->c || lba != v->c->buf_lba)
		return -1;
	struct cache **q;
	for (q = &v->cdirty; *q; q = &(*q)->next_dirty) {
		if (lba == (*q)->buf_lba)
			return 1;
		if (lba < (*q)->buf_lba)
			break;
	}
	struct cache *p = v->c;
	p->next_dirty = *q;
	*q = p;
	return 0;
}

static int
v_get_size (struct v_filelist *v)
{
	if (v_cmove (v, 0) < 0)
		return -1;
	long long size = v->c->buf[0] << 24 | v->c->buf[1] << 16 |
		v->c->buf[2] << 8 | v->c->buf[3];
	v->lastlba = (size + v->block_size - 1) >> v->block_size_shift;
	for (int n = 16;; n--) {
		if (v->rw (v->rw_param, NULL, NULL, 1, v->lastlba) >= 0) {
			v->size = size;
			if (v->lastwrittenlba < 0)
				v->lastwrittenlba = v->lastlba;
			return 0;
		}
		if (!v->lastlba)
			return -1;
		v->lastlba--;
		if (!n)
			v->lastlba = 0;
		size = v->lastlba << v->block_size_shift;
	}
}

static int
v_set_size (struct v_filelist *v, long long size)
{
	if (v->size >= 0 && size == v->size)
		return 0;
	if (v_cmove (v, 0) < 0)
		return -1;
	long long lastlba = (size + v->block_size - 1) >> v->block_size_shift;
	if (v->lastlba != lastlba) {
		if (v->rw (v->rw_param, NULL, NULL, 1, lastlba) < 0)
			return -1;
		v->lastlba = lastlba;
	}
	v->c->buf[0] = size >> 24;
	v->c->buf[1] = size >> 16;
	v->c->buf[2] = size >> 8;
	v->c->buf[3] = size;
	if (v_csetdirty (v, 0) < 0)
		return -1;
	v->size = size;
	if (v->lastwrittenlba > lastlba || v->lastwrittenlba < 0)
		v->lastwrittenlba = lastlba;
	return 0;
}

static int
v_rw (struct v_filelist *v, void *rbuf, const void *wbuf, int size,
      long long offset)
{
	if (v->size < 0 && v_get_size (v) < 0)
		return -1;
	if (wbuf && offset + size > v->size &&
	    v_set_size (v, offset + size) < 0)
		return -1;
	if (!rbuf && !wbuf && size == 1) {
		if (v_set_size (v, offset) < 0)
			return -1;
		return size;
	}
	if (!size) {
		if (v_flush (v) < 0)
			return -1;
		if (v_sync (v) < 0)
			return -1;
		return size;
	}
	if (!rbuf && !wbuf)
		return -1;
	int s0 = 0;
	if (!(offset & (v->block_size - 1)) && size > v->block_size) {
		int nblk = size >> v->block_size_shift;
		int ret = v_rw_nblk (v, rbuf, wbuf, nblk,
				     (offset >> v->block_size_shift) + 1);
		if (ret != nblk) {
			if (ret > 0)
				ret <<= v->block_size_shift;
			return ret;
		}
		int s = nblk << v->block_size_shift;
		if (wbuf)
			wbuf += s;
		else
			rbuf += s;
		offset += s;
		size -= s;
		if (!size)
			return s;
		s0 = s;
	}
	if (v_cmove (v, (offset >> v->block_size_shift) + 1) < 0)
		return -1;
	int s = v->block_size - (offset & (v->block_size - 1));
	if (s > size)
		s = size;
	if (wbuf) {
		memcpy (&v->c->buf[offset & (v->block_size - 1)], wbuf, s);
		if (v_csetdirty (v, (offset >> v->block_size_shift) + 1) < 0)
			return -1;
		wbuf += s;
	} else {
		memcpy (rbuf, &v->c->buf[offset & (v->block_size - 1)], s);
		rbuf += s;
	}
	offset += s;
	size -= s;
	int s1 = 0;
	if (size) {
		s1 = v_rw (v, rbuf, wbuf, size, offset);
		if (s1 < 0)
			return s1;
	}
	return s0 + s + s1;
}

static long long
v_getsize (struct v_filelist *v)
{
	if (v->size < 0 && v_get_size (v) < 0)
		return -1;
	return v->size;
}

static int
vClose (sqlite3_file *pFile)
{
	struct vFile *p = (void *)pFile;
	int size = v_rw (p->v, NULL, NULL, 0, 0);
	if (size == 0)
		return SQLITE_OK;
	return SQLITE_IOERR_FSYNC;
}

static int
vRead (sqlite3_file *pFile, void *zBuf, int iAmt, sqlite_int64 iOfst)
{
	struct vFile *p = (void *)pFile;
	int size = v_rw (p->v, zBuf, NULL, iAmt, iOfst);
	if (size == iAmt)
		return SQLITE_OK;
	if (size > 0)
		return SQLITE_IOERR_SHORT_READ;
	return SQLITE_IOERR_READ;
}

static int
vWrite (sqlite3_file *pFile, const void *zBuf, int iAmt, sqlite_int64 iOfst)
{
	struct vFile *p = (void *)pFile;
	int size = v_rw (p->v, NULL, zBuf, iAmt, iOfst);
	if (size == iAmt)
		return SQLITE_OK;
	return SQLITE_IOERR_WRITE;
}

static int
vTruncate (sqlite3_file *pFile, sqlite_int64 size)
{
	struct vFile *p = (void *)pFile;
	int success = v_rw (p->v, NULL, NULL, 1, size);
	if (success == 1)
		return SQLITE_OK;
	return SQLITE_IOERR_TRUNCATE;
}

static int
vSync (sqlite3_file *pFile, int flags)
{
	struct vFile *p = (void *)pFile;
	int size = v_rw (p->v, NULL, NULL, 0, 0);
	if (size == 0)
		return SQLITE_OK;
	return SQLITE_IOERR_FSYNC;
}

static int
vFileSize (sqlite3_file *pFile, sqlite_int64 *pSize)
{
	struct vFile *p = (void *)pFile;
	*pSize = v_getsize (p->v);
	return SQLITE_OK;
}

static int
vLock (sqlite3_file *pFile, int eLock)
{
	return SQLITE_OK;
}

static int
vUnlock (sqlite3_file *pFile, int eLock)
{
	return SQLITE_OK;
}

static int
vCheckReservedLock (sqlite3_file *pFile, int *pResOut)
{
	*pResOut = 0;
	return SQLITE_OK;
}

static int
vFileControl (sqlite3_file *pFile, int op, void *pArg)
{
	return SQLITE_NOTFOUND;
}

static int
vSectorSize (sqlite3_file *pFile)
{
	struct vFile *p = (void *)pFile;
	return p->v->block_size;
}

static int
vDeviceCharacteristics (sqlite3_file *pFile)
{
	return 0;
}

static int
vOpen (sqlite3_vfs *pVfs, const char *zPath, sqlite3_file *pFile, int flags,
       int *pOutFlags)
{
	struct v_filelist *v;
	if (zPath) {
		for (v = v_filehead; v; v = v->next) {
			if (!strcmp (zPath, v->path))
				goto found;
		}
	}
	return SQLITE_CANTOPEN;
found:;
	static const sqlite3_io_methods vio = {
		1,			/* iVersion */
		vClose,			/* xClose */
		vRead,			/* xRead */
		vWrite,			/* xWrite */
		vTruncate,		/* xTruncate */
		vSync,			/* xSync */
		vFileSize,		/* xFileSize */
		vLock,			/* xLock */
		vUnlock,		/* xUnlock */
		vCheckReservedLock,	/* xCheckReservedLock */
		vFileControl,		/* xFileControl */
		vSectorSize,		/* xSectorSize */
		vDeviceCharacteristics, /* xDeviceCharacteristics */
	};
	struct vFile *p = (void *)pFile;
	p->v = v;
	p->base.pMethods = &vio;
	if (pOutFlags)
		*pOutFlags = flags;
	return SQLITE_OK;
}

static int
vDelete (sqlite3_vfs *pVfs, const char *zPath, int dirSync)
{
	struct v_filelist *v;
	if (zPath) {
		for (v = v_filehead; v; v = v->next) {
			if (!strcmp (zPath, v->path)) {
				/* FIXME: Truncate only, not deleted... */
				if (v_rw (v, NULL, NULL, 1, 0) == 1 &&
				    v_rw (v, NULL, NULL, 0, 0) == 0)
					return SQLITE_OK;
				break;
			}
		}
	}
	return SQLITE_IOERR_DELETE;
}

static int
vAccess (sqlite3_vfs *pVfs, const char *zPath, int flags, int *pResOut)
{
	struct v_filelist *v;
	*pResOut = 0;
	if (zPath) {
		for (v = v_filehead; v; v = v->next) {
			if (!strcmp (zPath, v->path)) {
				*pResOut = 1;
				break;
			}
		}
	}
	return SQLITE_OK;
}

static int
vFullPathname (sqlite3_vfs *pVfs, const char *zPath, int nPathOut,
	       char *zPathOut)
{
	sqlite3_snprintf (nPathOut, zPathOut, "%s", zPath);
	return SQLITE_OK;
}

static void *
vDlOpen (sqlite3_vfs *pVfs, const char *zPath)
{
	return NULL;
}

static void
vDlError (sqlite3_vfs *pVfs, int nByte, char *zErrMsg)
{
	sqlite3_snprintf (nByte, zErrMsg, "not supported");
}

static void
(*vDlSym (sqlite3_vfs *pVfs, void *pH, const char *z)) (void)
{
	return NULL;
}

static void
vDlClose (sqlite3_vfs *pVfs, void *pHandle)
{
}

static int
vRandomness (sqlite3_vfs *pVfs, int nByte, char *zByte)
{
	return SQLITE_OK;
}

static int
vSleep (sqlite3_vfs *pVfs, int nMicro)
{
	return !!nMicro;
}

static int
vCurrentTime (sqlite3_vfs *pVfs, sqlite3_int64 *pTime)
{
	*pTime = 2459108;
	return SQLITE_OK;
}

sqlite3_vfs *
v_vfs (void)
{
	static sqlite3_vfs vvfs = {
		1,		       /* iVersion */
		sizeof (struct vFile), /* szOsFile */
		16,		       /* mxPathname */
		0,		       /* pNext */
		"v",		       /* zName */
		0,		       /* pAppData */
		vOpen,		       /* xOpen */
		vDelete,	       /* xDelete */
		vAccess,	       /* xAccess */
		vFullPathname,	       /* xFullPathname */
		vDlOpen,	       /* xDlOpen */
		vDlError,	       /* xDlError */
		vDlSym,		       /* xDlSym */
		vDlClose,	       /* xDlClose */
		vRandomness,	       /* xRandomness */
		vSleep,		       /* xSleep */
		vCurrentTime,	       /* xCurrentTime */
	};

	return &vvfs;
}
