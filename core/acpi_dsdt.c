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

/* For debugging:
   cc -DDEBUG_APP -DDEBUG_DUMP core/acpi_dsdt.c && ./a.out < dsdt.dat */
#ifndef DEBUG_APP
#include "acpi_dsdt.h"
#include "assert.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "stdarg.h"
#include "string.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <err.h>

typedef enum { false, true } bool;
#define alloc malloc
#define u32 uint32_t
#define u8 uint8_t
#define ulong unsigned long
#define ASSERT assert
#define MAPMEM_WRITE 0
#define panic(...) errx (1, __VA_ARGS__)
static void parser (unsigned char *start, unsigned char *end, bool);
static unsigned char buf[1048576];

static void *
mapmem_hphys (ulong a, int b, int c)
{
	return NULL;
}

static void
unmapmem (void *a, int b)
{
}

int
main (int argc, char **argv)
{
	int size, i, j;
	extern unsigned char acpi_dsdt_system_state[6][5];

	size = fread (buf, 1, sizeof buf, stdin);
	if (size <= 0) {
		perror ("fread");
		exit (1);
	}
	parser (buf, buf + size, true);
	for (i = 0; i < 6; i++) {
		printf ("%d: ", i);
		for (j = 1; j <= acpi_dsdt_system_state[i][0]; j++) {
			if (j > 1)
				printf (", ");
			printf ("%d", acpi_dsdt_system_state[i][j]);
		}
		printf ("\n");
	}
	exit (0);
}
#endif

#define PKGENDLEN 16
#define DATALEN 64

/* --- */
enum elementid {
#define TERM(x) x
#include "acpi_dsdt_term.h"
#undef TERM
};
/* --- */

struct pathlist {
	struct pathlist *next;
	enum elementid element;
	int namelen, nameflag;
	int ref;
};

struct buflist {
	struct buflist *next;
	enum elementid element;
	int ref;
};

struct limitlist {
	struct limitlist *next;
	unsigned char *pkgend;
	int ref;
};

struct breaklist {
	struct breaklist *next;
	char name[DATALEN];
	int namelen;
};

struct parsedatalist {
	struct parsedatalist *next;
	unsigned char *pkglen_c;
	unsigned int pkglen;
	unsigned char system_state[6][5];
	unsigned char *system_state_name[6];
	int datalen;
	unsigned char data[DATALEN];
	unsigned char *datac;
	struct limitlist *limithead;
	enum elementid pathelement;
	struct pathlist *pathhead;
	struct buflist *bufhead;
};

struct parsedata {
	struct parsedatalist *head, *cur, *ok;
	unsigned char *start, *end, *c;
	struct breaklist *breakhead;
	int breakfind;
	int progress, progresschar;
};

unsigned char acpi_dsdt_system_state[6][5];

static void *
parsealloc (int s)
{
	void *p;

	p = alloc (s);
	return p;
}

static void
parsefree (void *p)
{
	free (p);
}

static void
error (char *str)
{
	panic ("%s", str);
}

static void
replace_byte (struct parsedata *d, unsigned char *p, unsigned char c)
{
	d->start[9] += *p - c;
	*p = c;
}

static void
printname (char *name, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (!(i & 3))
			printf ("%c", i ? '.' : '\\');
		printf ("%c", name[i]);
	}
}

static void
addpathlist (struct pathlist **pathhead, enum elementid element)
{
	struct pathlist *newpath;

	newpath = parsealloc (sizeof (*newpath));
	newpath->next = *pathhead;
	newpath->element = element;
	newpath->namelen = *pathhead ? (*pathhead)->namelen : 0;
	newpath->nameflag = 0;
	newpath->ref = 1;
	*pathhead = newpath;
}

static void
addbuflist (struct buflist **bufhead, enum elementid element)
{
	struct buflist *newbuf;

	newbuf = parsealloc (sizeof (*newbuf));
	newbuf->next = *bufhead;
	newbuf->element = element;
	newbuf->ref = 1;
	*bufhead = newbuf;
}

static void
pushpkgend (struct limitlist **limithead, unsigned char *pkgend)
{
	struct limitlist *newlimit;

	newlimit = parsealloc (sizeof (*newlimit));
	newlimit->next = *limithead;
	newlimit->pkgend = pkgend;
	newlimit->ref = 1;
	*limithead = newlimit;
}

static unsigned char *
poppkgend (struct limitlist **limithead, int pollflag)
{
	struct limitlist *curlimit;
	unsigned char *r;

	curlimit = *limithead;
	if (!curlimit)
		return NULL;
	r = curlimit->pkgend;
	if (pollflag)
		return r;
	curlimit->ref--;
	*limithead = curlimit->next;
	if (!curlimit->ref)
		parsefree (curlimit);
	else if (*limithead)
		(*limithead)->ref++;
	return r;
}

static void
parsefreepathlist (struct pathlist **pathhead)
{
	struct pathlist *p;

	while (*pathhead && !--(*pathhead)->ref) {
		p = *pathhead;
		*pathhead = p->next;
		parsefree (p);
	}
	*pathhead = NULL;
}

static void
parsefreebuflist (struct buflist **bufhead)
{
	struct buflist *p;

	while (*bufhead && !--(*bufhead)->ref) {
		p = *bufhead;
		*bufhead = p->next;
		parsefree (p);
	}
	*bufhead = NULL;
}

static void
parsefreelimitlist (struct limitlist **limithead)
{
	struct limitlist *p;

	while (*limithead && !--(*limithead)->ref) {
		p = *limithead;
		*limithead = p->next;
		parsefree (p);
	}
	*limithead = NULL;
}

static void
parsefreebreaklist (struct breaklist **breakhead)
{
	struct breaklist *p;

	while (*breakhead) {
		p = *breakhead;
		printf ("Disable ");
		printname (p->name, p->namelen);
		printf ("\n");
		*breakhead = p->next;
		parsefree (p);
	}
	*breakhead = NULL;
}

static void
addbufsub (struct parsedata *d, enum elementid *tmp, int n)
{
	struct parsedatalist *q;
	int i, j, f;

	q = parsealloc (sizeof *q);
	/* copy path */
	f = 0;
	q->pathhead = d->cur->pathhead;
	if (q->pathhead)
		q->pathhead->ref++;
	if (!q->pathhead || q->pathhead->element != d->cur->pathelement) {
		addpathlist (&q->pathhead, d->cur->pathelement);
		f = 1;
	}
	/* copy buf */
	q->bufhead = d->cur->bufhead;
	if (q->bufhead)
		q->bufhead->ref++;
	if (f)
		addbuflist (&q->bufhead, OK);
	for (i = n; i > 0; i--)
		addbuflist (&q->bufhead, tmp[i - 1]);
	/* copy limit */
	q->limithead = d->cur->limithead;
	if (q->limithead)
		q->limithead->ref++;
	/* copy data */
	for (i = 0; i < d->cur->datalen; i++)
		q->data[i] = d->cur->data[i];
	q->datalen = d->cur->datalen;
	q->datac = d->cur->datac;
	/* copy other data */
	for (i = 0; i < 6; i++) {
		for (j = 0; j < 5; j++)
			q->system_state[i][j] = d->cur->system_state[i][j];
		q->system_state_name[i] = d->cur->system_state_name[i];
	}
	/* initialize other data */
	q->next = d->head;
	d->head = q;
}

static void
addbuf (struct parsedata *d, ...)
{
	va_list ap;
	enum elementid element, tmp[32];
	int i;

	va_start (ap, d);
	i = 0;
	while ((element = va_arg (ap, enum elementid)) != OK) {
		if (i >= 32)
			error ("i >= 32");
		tmp[i++] = element;
	}
	va_end (ap);
	addbufsub (d, tmp, i);
}

static void
addbuf2 (struct parsedata *d, enum elementid element0, enum elementid element,
	 int n)
{
	enum elementid tmp[32];
	int i;

	if (n >= 32)
		error ("n >= 32");
	tmp[0] = element0;
	for (i = 0; i < n; i++)
		tmp[i + 1] = element;
	addbufsub (d, tmp, n + 1);
}

static struct parsedatalist *
getbuf (struct parsedata *d)
{
	if (d->head == NULL) {
		d->head = d->ok;
		d->ok = NULL;
		d->c++;
		if (d->progress && ((d->c - d->start) % d->progress) == 0)
			printf ("%c", d->progresschar);
	}
	if (d->head == NULL)
		return NULL;
	d->cur = d->head;
	d->head = d->head->next;
	return d->cur;
}

static int
eqlist (struct parsedatalist *p, struct parsedatalist *q)
{
	struct buflist *bufp1, *bufp2;
	struct limitlist *limitp1, *limitp2;

	bufp1 = p->bufhead;
	bufp2 = q->bufhead;
	while (bufp1 != bufp2) {
		if (!bufp1 || !bufp2)
			return 0;
		if (bufp1->element != bufp2->element)
			return 0;
		bufp1 = bufp1->next;
		bufp2 = bufp2->next;
	}
	limitp1 = p->limithead;
	limitp2 = q->limithead;
	while (limitp1 != limitp2) {
		if (!limitp1 || !limitp2)
			return 0;
		if (limitp1->pkgend != limitp2->pkgend)
			return 0;
		limitp1 = limitp1->next;
		limitp2 = limitp2->next;
	}
	/* path and data may be different. it's ok. */
	return 1;
}

static struct pathlist *
pathsearch (struct parsedata *d, enum elementid element, struct pathlist *end)
{
	struct pathlist *path;

	for (path = d->cur->pathhead; path && path != end; path = path->next)
		if (path->element == element)
			return path;
	return NULL;
}

static int
getname (struct parsedata *d, char *buf, int len)
{
	char c;
	int i, j;

	for (i = j = 0; i < d->cur->datalen; i++) {
		c = (char)d->cur->data[i];
		switch (c) {
		case '\\':
			j = 0;
			break;
		case '^':
			if (j >= 4)
				j -= 4;
			else
				j = 0;
			break;
		default:
			if (j < len)
				buf[j++] = c;
		}
	}
	return j;
}

static int
namecmp_system_state (struct parsedata *d)
{
	int s, len;
	char tmp[DATALEN];

	len = getname (d, tmp, sizeof tmp);
	if (len != 4)
		s = -1;
	else if (!memcmp (tmp, "_S0_", 4))
		s = 0;
	else if (!memcmp (tmp, "_S1_", 4))
		s = 1;
	else if (!memcmp (tmp, "_S2_", 4))
		s = 2;
	else if (!memcmp (tmp, "_S3_", 4))
		s = 3;
	else if (!memcmp (tmp, "_S4_", 4))
		s = 4;
	else if (!memcmp (tmp, "_S5_", 4))
		s = 5;
	else
		s = -1;
	return s;
}

static void
save_system_state (struct parsedata *d, struct pathlist *path)
{
	int s;
	struct parsedatalist *c;

	if (path->element != AML_DefName)
		return;
	c = d->cur;
	s = namecmp_system_state (d);
	if (s < 0)
		return;
	if ((path = pathsearch (d, AML_DefPackage, path)) &&
	    (path = pathsearch (d, AML_PackageElementList, path)) &&
	    (path = pathsearch (d, AML_PackageElement, path))) {
		switch (c->pathhead->element) {
		case AML_ByteData:
		case AML_ZeroOp:
		case AML_OneOp:
		case AML_OnesOp:
			if (c->system_state[s][0] >= 5)
				return;
			c->system_state[s][0]++;
			if (c->system_state[s][0] >= 5)
				return;
			c->system_state[s][c->system_state[s][0]] = *d->c;
			c->system_state_name[s] = c->datac;
		default:
			break;
		}
	}
}

static void
addbreak (struct parsedata *d, char *name, int len)
{
	struct breaklist *p;

	for (p = d->breakhead; p; p = p->next) {
		if (p->namelen == len && !memcmp (p->name, name, len))
			return;
	}
	p = parsealloc (sizeof (*p));
	p->next = d->breakhead;
	memcpy (p->name, name, len);
	p->namelen = len;
	d->breakhead = p;
}

static void
find_pnp0501 (struct parsedata *d, struct pathlist *path)
{
	int len;
	char tmp[DATALEN];
	struct pathlist *path2;

	if (path->element != AML_DefName)
		return;
	len = getname (d, tmp, DATALEN);
	if (len < 4 || memcmp (&tmp[len - 4], "_HID", 4))
		return;
	path2 = pathsearch (d, AML_DWordPrefix, path);
	if (!path2)
		return;
	if ((d->end - d->c > 4) &&
	    *(unsigned int *)(d->c + 1) == 0x0105D041) { /* PNP0501 */
		memcpy (&tmp[len - 4], "_DIS", 4);
		addbreak (d, tmp, len);
	}
}

static void
break_method (struct parsedata *d, struct pathlist *path)
{
	int len;
	char tmp[DATALEN];
	struct breaklist *p;

	if (!d->breakhead)
		return;
	if (path->element != AML_DefMethod)
		return;
	if (!pathsearch (d, AML_ByteList, path))
		return;
	len = getname (d, tmp, DATALEN);
	for (p = d->breakhead; p; p = p->next) {
		if (p->namelen == len && !memcmp (p->name, tmp, len)) {
			replace_byte (d, d->c, 0xA3); /* 0xA3=noop */
			break;
		}
	}
}

static void
parseok_def (struct parsedata *d, struct pathlist *path)
{
	struct pathlist *path2;

	if ((path2 = pathsearch (d, AML_NameString, path)) &&
	    path2->next == path) {
		d->cur->datalen = path->namelen;
		if (!pathsearch (d, AML_NameSeg, path2) &&
		    !pathsearch (d, AML_RootChar, path2) &&
		    !pathsearch (d, AML_PrefixPath, path2))
			return;
		if (pathsearch (d, AML_NullName, path2))
			return;
		if (d->cur->datalen < DATALEN - 2) {
			if (!path->nameflag)
				path->nameflag = 1;
			else if (path->nameflag == 2)
				error ("nameflag == 2");
			d->cur->data[d->cur->datalen++] = *d->c;
			d->cur->datac = d->c;
			path->namelen = d->cur->datalen;
		}
	} else {
		if (path->nameflag == 1)
			path->nameflag = 2;
		if (d->breakfind) {
			find_pnp0501 (d, path);
		} else {
			save_system_state (d, path);
			break_method (d, path);
		}
	}
}

#ifdef DEBUG_DUMP
static char *term[] = {
#define TERM(x) #x
#include "acpi_dsdt_term.h"
#undef TERM
};

static void
ppath (struct pathlist *p)
{
	if (p->next)
		ppath (p->next);
	printf (" %s", term[p->element]);
}

static void
debug_dump (struct parsedata *d)
{
	int len;
	char tmp[DATALEN];

	printf ("%5x[%02x]:", d->c - d->start, *d->c);
	ppath (d->cur->pathhead);
	printf (" ");
	len = getname (d, tmp, sizeof tmp);
	printname (tmp, len);
	printf ("\n");
}
#else
static void
debug_dump (struct parsedata *d)
{
}
#endif

static void
parseok (struct parsedata *d)
{
	struct pathlist *path, *tmp;

	debug_dump (d);
	path = NULL;
	path = (tmp = pathsearch (d, AML_DefScope, path)) ? tmp : path;
	path = (tmp = pathsearch (d, AML_DefDevice, path)) ? tmp : path;
	path = (tmp = pathsearch (d, AML_DefMethod, path)) ? tmp : path;
	path = (tmp = pathsearch (d, AML_DefName, path)) ? tmp : path;
	if (path)
		parseok_def (d, path);
}

static struct parsedatalist *
parsemain (struct parsedata *d)
{
	struct parsedatalist *q, *r;
	struct pathlist *curpath;
	struct buflist *curbuf;
	enum elementid element;
	unsigned char *tmpc;
	int i;

	r = NULL;
loop:
	if (r == NULL) {
		if (getbuf (d) == NULL) {
#ifdef ACPI_IGNORE_ERROR
			printf ("getbuf error\n");
			return NULL;
#else
			error ("getbuf");
#endif
		}
		if (d->c == d->end) {
			r = d->cur;
			goto loop;
		}
	} else {
		if (getbuf (d) == NULL)
			return r;
		goto err;
	}
loop2:
	curbuf = d->cur->bufhead;
	if (!curbuf)
		goto err;
	element = curbuf->element;
	curbuf->ref--;
	d->cur->bufhead = curbuf->next;
	if (!curbuf->ref)
		parsefree (curbuf);
	else if (d->cur->bufhead)
		d->cur->bufhead->ref++;
	d->cur->pathelement = element;
	switch (element) {
/* --- */
	case AML_AMLCode:
		/* ThinkPad L570 has a DSDT that contains unexpected
		 * OneOp in TermList.  TermList2 has OneOp for
		 * workaround. */
		addbuf (d, AML_DefBlockHdr, AML_TermList2, OK);
		break;
	case AML_DefBlockHdr:
		addbuf (d, AML_TableSignature, AML_TableLength, 
			AML_SpecCompliance, AML_CheckSum, AML_OemID, 
			AML_OemTableID, AML_OemRevision, AML_CreatorID, 
			AML_CreatorRevision, OK);
		break;
	case AML_TableSignature:
		addbuf (d, AML_DWordData, OK);
		break;
	case AML_TableLength:
		addbuf (d, AML_DWordData, OK);
		break;
	case AML_SpecCompliance:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_CheckSum:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_OemID:
		addbuf (d, AML_ByteData, AML_ByteData, 
			AML_ByteData, AML_ByteData, AML_ByteData, 
			AML_ByteData, OK);
		break;
	case AML_OemTableID:
		addbuf (d, AML_ByteData, AML_ByteData, 
			AML_ByteData, AML_ByteData, AML_ByteData, 
			AML_ByteData, AML_ByteData, AML_ByteData, OK);
		break;
	case AML_OemRevision:
		addbuf (d, AML_DWordData, OK);
		break;
	case AML_CreatorID:
		addbuf (d, AML_DWordData, OK);
		break;
	case AML_CreatorRevision:
		addbuf (d, AML_DWordData, OK);
		break;
	case AML_LeadNameChar:
		addbuf (d, AML_0x41_TO_0x5A, OK);
		addbuf (d, AML_0x5F, OK);
		break;
	case AML_DigitChar:
		addbuf (d, AML_0x30_TO_0x39, OK);
		break;
	case AML_NameChar:
		addbuf (d, AML_DigitChar, OK);
		addbuf (d, AML_LeadNameChar, OK);
		break;
	case AML_RootChar:
		addbuf (d, AML_0x5C, OK);
		break;
	case AML_ParentPrefixChar:
		addbuf (d, AML_0x5E, OK);
		break;
	case AML_NameSeg:
		addbuf (d, AML_LeadNameChar, AML_NameChar, 
			AML_NameChar, AML_NameChar, OK);
		break;
	case AML_NameString:
		addbuf (d, AML_RootChar, AML_NamePath, OK);
		addbuf (d, AML_PrefixPath, AML_NamePath, OK);
		break;
	case AML_PrefixPath:
		addbuf (d, AML_Nothing, OK);
		addbuf (d, AML_0x5E, AML_PrefixPath, OK);
		break;
	case AML_NamePath:
		addbuf (d, AML_NameSeg, OK);
		addbuf (d, AML_DualNamePath, OK);
		addbuf (d, AML_MultiNamePath, OK);
		addbuf (d, AML_NullName, OK);
		break;
	case AML_DualNamePath:
		addbuf (d, AML_DualNamePrefix, AML_NameSeg, 
			AML_NameSeg, OK);
		break;
	case AML_DualNamePrefix:
		addbuf (d, AML_0x2E, OK);
		break;
	case AML_MultiNamePrefix:
		addbuf (d, AML_0x2F, OK);
		break;
	case AML_SimpleName:
		addbuf (d, AML_NameString, OK);
		addbuf (d, AML_ArgObj, OK);
		addbuf (d, AML_LocalObj, OK);
		break;
	case AML_SuperName:
		addbuf (d, AML_SimpleName, OK);
		addbuf (d, AML_DebugObj, OK);
		addbuf (d, AML_Type6Opcode, OK);
		break;
	case AML_NullName:
		addbuf (d, AML_0x00, OK);
		break;
	case AML_Target:
		addbuf (d, AML_SuperName, OK);
		addbuf (d, AML_NullName, OK);
		break;
	case AML_ComputationalData:
		addbuf (d, AML_ByteConst, OK);
		addbuf (d, AML_WordConst, OK);
		addbuf (d, AML_DWordConst, OK);
		addbuf (d, AML_QWordConst, OK);
		addbuf (d, AML_String, OK);
		addbuf (d, AML_ConstObj, OK);
		addbuf (d, AML_RevisionOp, OK);
		addbuf (d, AML_DefBuffer, OK);
		break;
	case AML_DataObject:
		addbuf (d, AML_ComputationalData, OK);
		addbuf (d, AML_DefPackage, OK);
		addbuf (d, AML_DefVarPackage, OK);
		break;
	case AML_DataRefObject:
		addbuf (d, AML_DataObject, OK);
		addbuf (d, AML_ObjectReference, OK);
		addbuf (d, AML_DDBHandle, OK);
		break;
	case AML_ByteConst:
		addbuf (d, AML_BytePrefix, AML_ByteData, OK);
		break;
	case AML_BytePrefix:
		addbuf (d, AML_0x0A, OK);
		break;
	case AML_WordConst:
		addbuf (d, AML_WordPrefix, AML_WordData, OK);
		break;
	case AML_WordPrefix:
		addbuf (d, AML_0x0B, OK);
		break;
	case AML_DWordConst:
		addbuf (d, AML_DWordPrefix, AML_DWordData, OK);
		break;
	case AML_DWordPrefix:
		addbuf (d, AML_0x0C, OK);
		break;
	case AML_QWordConst:
		addbuf (d, AML_QWordPrefix, AML_QWordData, OK);
		break;
	case AML_QWordPrefix:
		addbuf (d, AML_0x0E, OK);
		break;
	case AML_String:
		addbuf (d, AML_StringPrefix, AML_AsciiCharList, 
			AML_NullChar, OK);
		break;
	case AML_StringPrefix:
		addbuf (d, AML_0x0D, OK);
		break;
	case AML_ConstObj:
		addbuf (d, AML_ZeroOp, OK);
		addbuf (d, AML_OneOp, OK);
		addbuf (d, AML_OnesOp, OK);
		break;
	case AML_ByteList:
		addbuf (d, AML_Nothing, OK);
		addbuf (d, AML_ByteData, AML_ByteList, OK);
		break;
	case AML_ByteData:
		addbuf (d, AML_0x00_TO_0xFF, OK);
		break;
	case AML_AsciiCharList:
		addbuf (d, AML_Nothing, OK);
		addbuf (d, AML_AsciiChar, AML_AsciiCharList, OK);
		break;
	case AML_AsciiChar:
		addbuf (d, AML_0x01_TO_0x7F, OK);
		break;
	case AML_NullChar:
		addbuf (d, AML_0x00, OK);
		break;
	case AML_ZeroOp:
		addbuf (d, AML_0x00, OK);
		break;
	case AML_OneOp:
		addbuf (d, AML_0x01, OK);
		break;
	case AML_OnesOp:
		addbuf (d, AML_0xFF, OK);
		break;
	case AML_RevisionOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x30, OK);
		break;
	case AML_ExtOpPrefix:
		addbuf (d, AML_0x5B, OK);
		break;
	case AML_Object:
		addbuf (d, AML_NameSpaceModifierObj, OK);
		addbuf (d, AML_NamedObj, OK);
		break;
	case AML_TermObj:
		addbuf (d, AML_Object, OK);
		addbuf (d, AML_Type1Opcode, OK);
		addbuf (d, AML_Type2Opcode, OK);
		break;
	case AML_TermList:
		addbuf (d, AML_Nothing, OK);
		addbuf (d, AML_TermObj, AML_TermList, OK);
		break;
	case AML_TermArg:
		addbuf (d, AML_Type2Opcode, OK);
		addbuf (d, AML_DataObject, OK);
		addbuf (d, AML_ArgObj, OK);
		addbuf (d, AML_LocalObj, OK);
		break;
	case AML_MethodInvocation:
		/* NameString2 is same as NameString except NullName.
		 * If NameString is used here, multiple ZeroOp bytes
		 * are parsed as NullName for NameString, and, ZeroOp
		 * and another MethodInvocation for TermArgList.  To
		 * avoid the behavior which may take a long time,
		 * NullName is specially handled in MethodInvocation2
		 * and goto err if MethodInvocation2 already exists in
		 * path. */
		if (pathsearch (d, AML_MethodInvocation2, NULL))
			goto err;
		addbuf (d, AML_NameString2, AML_TermArgList, OK);
		addbuf (d, AML_MethodInvocation2, OK);
		break;
	case AML_TermArgList:
		addbuf (d, AML_Nothing, OK);
		addbuf (d, AML_TermArg, AML_TermArgList, OK);
		break;
	case AML_NameSpaceModifierObj:
		addbuf (d, AML_DefAlias, OK);
		addbuf (d, AML_DefName, OK);
		addbuf (d, AML_DefScope, OK);
		break;
	case AML_DefAlias:
		addbuf (d, AML_AliasOp, AML_NameString, 
			AML_NameString, OK);
		break;
	case AML_AliasOp:
		addbuf (d, AML_0x06, OK);
		break;
	case AML_DefName:
		addbuf (d, AML_NameOp, AML_NameString, 
			AML_DataRefObject, OK);
		break;
	case AML_NameOp:
		addbuf (d, AML_0x08, OK);
		break;
	case AML_DefScope:
		addbuf (d, AML_ScopeOp, AML_PkgLength, 
			AML_NameString, AML_TermList, AML_PkgEND, OK);
		break;
	case AML_ScopeOp:
		addbuf (d, AML_0x10, OK);
		break;
	case AML_NamedObj:
		addbuf (d, AML_DefBankField, OK);
		addbuf (d, AML_DefCreateBitField, OK);
		addbuf (d, AML_DefCreateByteField, OK);
		addbuf (d, AML_DefCreateDWordField, OK);
		addbuf (d, AML_DefCreateField, OK);
		addbuf (d, AML_DefCreateQWordField, OK);
		addbuf (d, AML_DefCreateWordField, OK);
		addbuf (d, AML_DefDataRegion, OK);
		/* DefDevice, DefEvent, DefField, DefIndexField,
		 * DefMethod and DefMutex were removed from NamedObj
		 * in ACPI spec 5.0, but actually they are needed. */
		addbuf (d, AML_DefDevice, OK);
		addbuf (d, AML_DefEvent, OK);
		addbuf (d, AML_DefExternal, OK);
		addbuf (d, AML_DefField, OK);
		addbuf (d, AML_DefIndexField, OK);
		addbuf (d, AML_DefMethod, OK);
		addbuf (d, AML_DefMutex, OK);
		addbuf (d, AML_DefOpRegion, OK);
		addbuf (d, AML_DefPowerRes, OK);
		addbuf (d, AML_DefProcessor, OK);
		addbuf (d, AML_DefThermalZone, OK);
		break;
	case AML_DefBankField:
		addbuf (d, AML_BankFieldOp, AML_PkgLength, 
			AML_NameString, AML_NameString, AML_BankValue, 
			AML_FieldFlags, AML_FieldList, AML_PkgEND, OK);
		break;
	case AML_BankFieldOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x87, OK);
		break;
	case AML_BankValue:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_FieldFlags:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_FieldList:
		addbuf (d, AML_Nothing, OK);
		addbuf (d, AML_FieldElement, AML_FieldList, OK);
		break;
	case AML_NamedField:
		addbuf (d, AML_NameSeg, AML_PkgLength, 
			AML_PkgIGNORE, OK);
		break;
	case AML_ReservedField:
		addbuf (d, AML_0x00, AML_PkgLength, 
			AML_PkgIGNORE, OK);
		break;
	case AML_AccessField:
		addbuf (d, AML_0x01, AML_AccessType, 
			AML_AccessAttrib, OK);
		break;
	case AML_AccessType:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_AccessAttrib:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_ConnectField:
		addbuf (d, AML_0x02, AML_NameString, OK);
		/* BufferData appeared in ACPI spec 5.0 but it still
		 * has been undefined in ACPI spec 6.2.  BufData is
		 * just guess. */
		addbuf (d, AML_0x02, /* AML_BufferData */AML_BufData, OK);
		break;
	case AML_DefCreateBitField:
		addbuf (d, AML_CreateBitFieldOp, AML_SourceBuff, 
			AML_BitIndex, AML_NameString, OK);
		break;
	case AML_CreateBitFieldOp:
		addbuf (d, AML_0x8D, OK);
		break;
	case AML_SourceBuff:
		addbuf (d, AML_TermArg, /* => Buffer */OK);
		break;
	case AML_BitIndex:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefCreateByteField:
		addbuf (d, AML_CreateByteFieldOp, AML_SourceBuff, 
			AML_ByteIndex, AML_NameString, OK);
		break;
	case AML_CreateByteFieldOp:
		addbuf (d, AML_0x8C, OK);
		break;
	case AML_ByteIndex:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefCreateDWordField:
		addbuf (d, AML_CreateDWordFieldOp, 
			AML_SourceBuff, AML_ByteIndex, AML_NameString, OK);
		break;
	case AML_CreateDWordFieldOp:
		addbuf (d, AML_0x8A, OK);
		break;
	case AML_DefCreateField:
		addbuf (d, AML_CreateFieldOp, AML_SourceBuff, 
			AML_BitIndex, AML_NumBits, AML_NameString, OK);
		break;
	case AML_CreateFieldOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x13, OK);
		break;
	case AML_NumBits:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefCreateQWordField:
		addbuf (d, AML_CreateQWordFieldOp, 
			AML_SourceBuff, AML_ByteIndex, AML_NameString, OK);
		break;
	case AML_CreateQWordFieldOp:
		addbuf (d, AML_0x8F, OK);
		break;
	case AML_DefCreateWordField:
		addbuf (d, AML_CreateWordFieldOp, AML_SourceBuff, 
			AML_ByteIndex, AML_NameString, OK);
		break;
	case AML_CreateWordFieldOp:
		addbuf (d, AML_0x8B, OK);
		break;
	case AML_DefDataRegion:
		addbuf (d, AML_DataRegionOp, AML_NameString, 
			AML_TermArg, AML_TermArg, AML_TermArg, OK);
		break;
	case AML_DataRegionOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x88, OK);
		break;
	case AML_DeviceOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x82, OK);
		break;
	case AML_DefEvent:
		addbuf (d, AML_EventOp, AML_NameString, OK);
		break;
	case AML_EventOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x02, OK);
		break;
	case AML_DefExternal:
		addbuf (d, AML_ExternalOp, AML_NameString, AML_ObjectType,
			AML_ArgumentCount, OK);
		break;
	case AML_ExternalOp:
		addbuf (d, AML_0x15, OK);
		break;
	case AML_ObjectType:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_ArgumentCount:
		addbuf (d, AML_ByteData /* 0 - 7 */, OK);
		break;
	case AML_DefField:
		addbuf (d, AML_FieldOp, AML_PkgLength, 
			AML_NameString, AML_FieldFlags, AML_FieldList, 
			AML_PkgEND, OK);
		break;
	case AML_FieldOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x81, OK);
		break;
	case AML_DefIndexField:
		addbuf (d, AML_IndexFieldOp, AML_PkgLength, 
			AML_NameString, AML_NameString, AML_FieldFlags, 
			AML_FieldList, AML_PkgEND, OK);
		break;
	case AML_IndexFieldOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x86, OK);
		break;
	case AML_MethodOp:
		addbuf (d, AML_0x14, OK);
		break;
	case AML_MethodFlags:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_DefMutex:
		addbuf (d, AML_MutexOp, AML_NameString, 
			AML_SyncFlags, OK);
		break;
	case AML_MutexOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x01, OK);
		break;
	case AML_SyncFlags:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_DefOpRegion:
		addbuf (d, AML_OpRegionOp, AML_NameString, 
			AML_RegionSpace, AML_RegionOffset, 
			AML_RegionLen, OK);
		break;
	case AML_OpRegionOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x80, OK);
		break;
	case AML_RegionSpace:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_RegionOffset:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_RegionLen:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefPowerRes:
		addbuf (d, AML_PowerResOp, AML_PkgLength, 
			AML_NameString, AML_SystemLevel, 
			AML_ResourceOrder, AML_TermList, AML_PkgEND, OK);
		break;
	case AML_PowerResOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x84, OK);
		break;
	case AML_SystemLevel:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_ResourceOrder:
		addbuf (d, AML_WordData, OK);
		break;
	case AML_DefProcessor:
		addbuf (d, AML_ProcessorOp, AML_PkgLength, 
			AML_NameString, AML_ProcID, AML_PblkAddr, 
			AML_PblkLen, AML_TermList, AML_PkgEND, OK);
		break;
	case AML_ProcessorOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x83, OK);
		break;
	case AML_ProcID:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_PblkAddr:
		addbuf (d, AML_DWordData, OK);
		break;
	case AML_PblkLen:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_DefThermalZone:
		addbuf (d, AML_ThermalZoneOp, AML_PkgLength, 
			AML_NameString, AML_TermList, AML_PkgEND, OK);
		break;
	case AML_ThermalZoneOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x85, OK);
		break;
	case AML_ExtendedAccessField:
		/* AccessLength appeared in ACPI spec 5.0 but it still
		 * has been undefined in ACPI spec 6.2.  ByteData is
		 * just guess. */
		addbuf (d, AML_0x03, AML_AccessType, AML_ExtendedAccessAttrib,
			/* AML_AccessLength */AML_ByteData, OK);
		break;
	case AML_ExtendedAccessAttrib:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_FieldElement:
		addbuf (d, AML_NamedField, OK);
		addbuf (d, AML_ReservedField, OK);
		addbuf (d, AML_AccessField, OK);
		addbuf (d, AML_ExtendedAccessField, OK);
		addbuf (d, AML_ConnectField, OK);
		break;
	case AML_Type1Opcode:
		addbuf (d, AML_DefBreak, OK);
		addbuf (d, AML_DefBreakPoint, OK);
		addbuf (d, AML_DefContinue, OK);
		addbuf (d, AML_DefFatal, OK);
		addbuf (d, AML_DefIfElse, OK);
		addbuf (d, AML_DefLoad, OK);
		addbuf (d, AML_DefNoop, OK);
		addbuf (d, AML_DefNotify, OK);
		addbuf (d, AML_DefRelease, OK);
		addbuf (d, AML_DefReset, OK);
		addbuf (d, AML_DefReturn, OK);
		addbuf (d, AML_DefSignal, OK);
		addbuf (d, AML_DefSleep, OK);
		addbuf (d, AML_DefStall, OK);
		addbuf (d, AML_DefUnload, OK);
		addbuf (d, AML_DefWhile, OK);
		break;
	case AML_DefBreak:
		addbuf (d, AML_BreakOp, OK);
		break;
	case AML_BreakOp:
		addbuf (d, AML_0xA5, OK);
		break;
	case AML_DefBreakPoint:
		addbuf (d, AML_BreakPointOp, OK);
		break;
	case AML_BreakPointOp:
		addbuf (d, AML_0xCC, OK);
		break;
	case AML_DefContinue:
		addbuf (d, AML_ContinueOp, OK);
		break;
	case AML_ContinueOp:
		addbuf (d, AML_0x9F, OK);
		break;
	case AML_DefElse:
		addbuf (d, AML_Nothing, OK);
		addbuf (d, AML_ElseOp, AML_PkgLength, 
			AML_TermList, AML_PkgEND, OK);
		break;
	case AML_ElseOp:
		addbuf (d, AML_0xA1, OK);
		break;
	case AML_DefFatal:
		addbuf (d, AML_FatalOp, AML_FatalType, 
			AML_FatalCode, AML_FatalArg, OK);
		break;
	case AML_FatalOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x32, OK);
		break;
	case AML_FatalType:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_FatalCode:
		addbuf (d, AML_DWordData, OK);
		break;
	case AML_FatalArg:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefIfElse:
		addbuf (d, AML_IfOp, AML_PkgLength, 
			AML_Predicate, AML_TermList, AML_PkgEND, 
			AML_DefElse, OK);
		break;
	case AML_IfOp:
		addbuf (d, AML_0xA0, OK);
		break;
	case AML_Predicate:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefLoad:
		addbuf (d, AML_LoadOp, AML_NameString, 
			AML_DDBHandleObject, OK);
		break;
	case AML_LoadOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x20, OK);
		break;
	case AML_DDBHandleObject:
		addbuf (d, AML_SuperName, OK);
		break;
	case AML_DefNoop:
		addbuf (d, AML_NoopOp, OK);
		break;
	case AML_NoopOp:
		addbuf (d, AML_0xA3, OK);
		break;
	case AML_DefNotify:
		addbuf (d, AML_NotifyOp, AML_NotifyObject, 
			AML_NotifyValue, OK);
		break;
	case AML_NotifyOp:
		addbuf (d, AML_0x86, OK);
		break;
	case AML_NotifyValue:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefRelease:
		addbuf (d, AML_ReleaseOp, AML_MutexObject, OK);
		break;
	case AML_ReleaseOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x27, OK);
		break;
	case AML_MutexObject:
		addbuf (d, AML_SuperName, OK);
		break;
	case AML_DefReset:
		addbuf (d, AML_ResetOp, AML_EventObject, OK);
		break;
	case AML_ResetOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x26, OK);
		break;
	case AML_EventObject:
		addbuf (d, AML_SuperName, OK);
		break;
	case AML_DefReturn:
		addbuf (d, AML_ReturnOp, AML_ArgObject, OK);
		break;
	case AML_ReturnOp:
		addbuf (d, AML_0xA4, OK);
		break;
	case AML_ArgObject:
		addbuf (d, AML_TermArg, /* => DataRefObject */OK);
		break;
	case AML_DefSignal:
		addbuf (d, AML_SignalOp, AML_EventObject, OK);
		break;
	case AML_SignalOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x24, OK);
		break;
	case AML_DefSleep:
		addbuf (d, AML_SleepOp, AML_MsecTime, OK);
		break;
	case AML_SleepOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x22, OK);
		break;
	case AML_MsecTime:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefStall:
		addbuf (d, AML_StallOp, AML_UsecTime, OK);
		break;
	case AML_StallOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x21, OK);
		break;
	case AML_UsecTime:
		addbuf (d, AML_TermArg, /* => ByteData */OK);
		break;
	case AML_DefUnload:
		addbuf (d, AML_UnloadOp, AML_DDBHandleObject, OK);
		break;
	case AML_UnloadOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x2A, OK);
		break;
	case AML_DefWhile:
		addbuf (d, AML_WhileOp, AML_PkgLength, 
			AML_Predicate, AML_TermList, AML_PkgEND, OK);
		break;
	case AML_WhileOp:
		addbuf (d, AML_0xA2, OK);
		break;
	case AML_Type2Opcode:
		addbuf (d, AML_DefAcquire, OK);
		addbuf (d, AML_DefAdd, OK);
		addbuf (d, AML_DefAnd, OK);
		addbuf (d, AML_DefBuffer, OK);
		addbuf (d, AML_DefConcat, OK);
		addbuf (d, AML_DefConcatRes, OK);
		addbuf (d, AML_DefCondRefOf, OK);
		addbuf (d, AML_DefCopyObject, OK);
		addbuf (d, AML_DefDecrement, OK);
		addbuf (d, AML_DefDerefOf, OK);
		addbuf (d, AML_DefDivide, OK);
		addbuf (d, AML_DefFindSetLeftBit, OK);
		addbuf (d, AML_DefFindSetRightBit, OK);
		addbuf (d, AML_DefFromBCD, OK);
		addbuf (d, AML_DefIncrement, OK);
		addbuf (d, AML_DefIndex, OK);
		addbuf (d, AML_DefLAnd, OK);
		addbuf (d, AML_DefLEqual, OK);
		addbuf (d, AML_DefLGreater, OK);
		addbuf (d, AML_DefLGreaterEqual, OK);
		addbuf (d, AML_DefLLess, OK);
		addbuf (d, AML_DefLLessEqual, OK);
		addbuf (d, AML_DefMid, OK);
		addbuf (d, AML_DefLNot, OK);
		addbuf (d, AML_DefLNotEqual, OK);
		addbuf (d, AML_DefLoadTable, OK);
		addbuf (d, AML_DefLOr, OK);
		addbuf (d, AML_DefMatch, OK);
		addbuf (d, AML_DefMod, OK);
		addbuf (d, AML_DefMultiply, OK);
		addbuf (d, AML_DefNAnd, OK);
		addbuf (d, AML_DefNOr, OK);
		addbuf (d, AML_DefNot, OK);
		addbuf (d, AML_DefObjectType, OK);
		addbuf (d, AML_DefOr, OK);
		addbuf (d, AML_DefPackage, OK);
		addbuf (d, AML_DefVarPackage, OK);
		addbuf (d, AML_DefRefOf, OK);
		addbuf (d, AML_DefShiftLeft, OK);
		addbuf (d, AML_DefShiftRight, OK);
		addbuf (d, AML_DefSizeOf, OK);
		addbuf (d, AML_DefStore, OK);
		addbuf (d, AML_DefSubtract, OK);
		addbuf (d, AML_DefTimer, OK);
		addbuf (d, AML_DefToBCD, OK);
		addbuf (d, AML_DefToBuffer, OK);
		addbuf (d, AML_DefToDecimalString, OK);
		addbuf (d, AML_DefToHexString, OK);
		addbuf (d, AML_DefToInteger, OK);
		addbuf (d, AML_DefToString, OK);
		addbuf (d, AML_DefWait, OK);
		addbuf (d, AML_DefXOr, OK);
		addbuf (d, AML_MethodInvocation, OK);
		break;
	case AML_Type6Opcode:
		addbuf (d, AML_DefRefOf, OK);
		addbuf (d, AML_DefDerefOf, OK);
		addbuf (d, AML_DefIndex, OK);
		/* UserTermObj was apparently replaced to
		 * MethodInvocation in ACPI spec 5.0 but Type6Opcode
		 * still has UserTermObj in ACPI spec 6.2.  It must be
		 * MethodInvocation now. */
		addbuf (d, /* AML_UserTermObj */AML_MethodInvocation, OK);
		break;
	case AML_DefAcquire:
		addbuf (d, AML_AcquireOp, AML_MutexObject, 
			AML_Timeout, OK);
		break;
	case AML_AcquireOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x23, OK);
		break;
	case AML_Timeout:
		addbuf (d, AML_WordData, OK);
		break;
	case AML_DefAdd:
		addbuf (d, AML_AddOp, AML_Operand, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_AddOp:
		addbuf (d, AML_0x72, OK);
		break;
	case AML_Operand:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefAnd:
		addbuf (d, AML_AndOp, AML_Operand, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_AndOp:
		addbuf (d, AML_0x7B, OK);
		break;
	case AML_DefBuffer:
		addbuf (d, AML_BufferOp, AML_PkgLength, 
			AML_BufferSize, AML_ByteList, AML_PkgEND, OK);
		break;
	case AML_BufferOp:
		addbuf (d, AML_0x11, OK);
		break;
	case AML_BufferSize:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefConcat:
		addbuf (d, AML_ConcatOp, AML_Data, AML_Data, 
			AML_Target, OK);
		break;
	case AML_ConcatOp:
		addbuf (d, AML_0x73, OK);
		break;
	case AML_Data:
		addbuf (d, AML_TermArg, /* => ComputationalData */OK);
		break;
	case AML_DefConcatRes:
		addbuf (d, AML_ConcatResOp, AML_BufData, 
			AML_BufData, AML_Target, OK);
		break;
	case AML_ConcatResOp:
		addbuf (d, AML_0x84, OK);
		break;
	case AML_BufData:
		addbuf (d, AML_TermArg, /* => Buffer */OK);
		break;
	case AML_DefCondRefOf:
		addbuf (d, AML_CondRefOfOp, AML_SuperName, 
			AML_Target, OK);
		break;
	case AML_CondRefOfOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x12, OK);
		break;
	case AML_DefCopyObject:
		addbuf (d, AML_CopyObjectOp, AML_TermArg, 
			AML_SimpleName, OK);
		break;
	case AML_CopyObjectOp:
		addbuf (d, AML_0x9D, OK);
		break;
	case AML_DefDecrement:
		addbuf (d, AML_DecrementOp, AML_SuperName, OK);
		break;
	case AML_DecrementOp:
		addbuf (d, AML_0x76, OK);
		break;
	case AML_DefDerefOf:
		addbuf (d, AML_DerefOfOp, AML_ObjReference, OK);
		break;
	case AML_DerefOfOp:
		addbuf (d, AML_0x83, OK);
		break;
	case AML_ObjReference:
		addbuf (d, AML_TermArg, /* => ObjectReference */OK);
		addbuf (d, AML_String, OK);
		break;
	case AML_DefDivide:
		addbuf (d, AML_DivideOp, AML_Dividend, 
			AML_Divisor, AML_Remainder, AML_Quotient, OK);
		break;
	case AML_DivideOp:
		addbuf (d, AML_0x78, OK);
		break;
	case AML_Dividend:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_Divisor:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_Remainder:
		addbuf (d, AML_Target, OK);
		break;
	case AML_Quotient:
		addbuf (d, AML_Target, OK);
		break;
	case AML_DefFindSetLeftBit:
		addbuf (d, AML_FindSetLeftBitOp, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_FindSetLeftBitOp:
		addbuf (d, AML_0x81, OK);
		break;
	case AML_DefFindSetRightBit:
		addbuf (d, AML_FindSetRightBitOp, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_FindSetRightBitOp:
		addbuf (d, AML_0x82, OK);
		break;
	case AML_DefFromBCD:
		addbuf (d, AML_FromBCDOp, AML_BCDValue, 
			AML_Target, OK);
		break;
	case AML_FromBCDOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x28, OK);
		break;
	case AML_BCDValue:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefIncrement:
		addbuf (d, AML_IncrementOp, AML_SuperName, OK);
		break;
	case AML_IncrementOp:
		addbuf (d, AML_0x75, OK);
		break;
	case AML_DefIndex:
		addbuf (d, AML_IndexOp, AML_BuffPkgStrObj, 
			AML_IndexValue, AML_Target, OK);
		break;
	case AML_IndexOp:
		addbuf (d, AML_0x88, OK);
		break;
	case AML_IndexValue:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefLAnd:
		addbuf (d, AML_LandOp, AML_Operand, AML_Operand, OK);
		break;
	case AML_LandOp:
		addbuf (d, AML_0x90, OK);
		break;
	case AML_DefLEqual:
		addbuf (d, AML_LequalOp, AML_Operand, 
			AML_Operand, OK);
		break;
	case AML_LequalOp:
		addbuf (d, AML_0x93, OK);
		break;
	case AML_DefLGreater:
		addbuf (d, AML_LgreaterOp, AML_Operand, 
			AML_Operand, OK);
		break;
	case AML_LgreaterOp:
		addbuf (d, AML_0x94, OK);
		break;
	case AML_DefLGreaterEqual:
		addbuf (d, AML_LgreaterEqualOp, AML_Operand, 
			AML_Operand, OK);
		break;
	case AML_LgreaterEqualOp:
		addbuf (d, AML_LnotOp, AML_LlessOp, OK);
		break;
	case AML_DefLLess:
		addbuf (d, AML_LlessOp, AML_Operand, AML_Operand, OK);
		break;
	case AML_LlessOp:
		addbuf (d, AML_0x95, OK);
		break;
	case AML_DefLLessEqual:
		addbuf (d, AML_LlessEqualOp, AML_Operand, 
			AML_Operand, OK);
		break;
	case AML_LlessEqualOp:
		addbuf (d, AML_LnotOp, AML_LgreaterOp, OK);
		break;
	case AML_DefLNot:
		addbuf (d, AML_LnotOp, AML_Operand, OK);
		break;
	case AML_LnotOp:
		addbuf (d, AML_0x92, OK);
		break;
	case AML_DefLNotEqual:
		addbuf (d, AML_LnotEqualOp, AML_Operand, 
			AML_Operand, OK);
		break;
	case AML_LnotEqualOp:
		addbuf (d, AML_LnotOp, AML_LequalOp, OK);
		break;
	case AML_DefLoadTable:
		addbuf (d, AML_LoadTableOp, AML_TermArg, 
			AML_TermArg, AML_TermArg, AML_TermArg, 
			AML_TermArg, AML_TermArg, OK);
		break;
	case AML_LoadTableOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x1F, OK);
		break;
	case AML_DefLOr:
		addbuf (d, AML_LorOp, AML_Operand, AML_Operand, OK);
		break;
	case AML_LorOp:
		addbuf (d, AML_0x91, OK);
		break;
	case AML_DefMatch:
		addbuf (d, AML_MatchOp, AML_SearchPkg, 
			AML_MatchOpcode, AML_Operand, AML_MatchOpcode, 
			AML_Operand, AML_StartIndex, OK);
		break;
	case AML_MatchOp:
		addbuf (d, AML_0x89, OK);
		break;
	case AML_SearchPkg:
		addbuf (d, AML_TermArg, /* => Package */OK);
		break;
	case AML_MatchOpcode:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_StartIndex:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefMid:
		addbuf (d, AML_MidOp, AML_MidObj, AML_TermArg, 
			AML_TermArg, AML_Target, OK);
		break;
	case AML_MidOp:
		addbuf (d, AML_0x9E, OK);
		break;
	case AML_MidObj:
		addbuf (d, AML_TermArg, /* => Buffer */OK);
		addbuf (d, AML_String, OK);
		break;
	case AML_DefMod:
		addbuf (d, AML_ModOp, AML_Dividend, AML_Divisor, 
			AML_Target, OK);
		break;
	case AML_ModOp:
		addbuf (d, AML_0x85, OK);
		break;
	case AML_DefMultiply:
		addbuf (d, AML_MultiplyOp, AML_Operand, 
			AML_Operand, AML_Target, OK);
		break;
	case AML_MultiplyOp:
		addbuf (d, AML_0x77, OK);
		break;
	case AML_DefNAnd:
		addbuf (d, AML_NandOp, AML_Operand, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_NandOp:
		addbuf (d, AML_0x7C, OK);
		break;
	case AML_DefNOr:
		addbuf (d, AML_NorOp, AML_Operand, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_NorOp:
		addbuf (d, AML_0x7E, OK);
		break;
	case AML_DefNot:
		addbuf (d, AML_NotOp, AML_Operand, AML_Target, OK);
		break;
	case AML_NotOp:
		addbuf (d, AML_0x80, OK);
		break;
	case AML_DefObjectType:
		addbuf (d, AML_ObjectTypeOp, AML_SuperName, OK);
		break;
	case AML_ObjectTypeOp:
		addbuf (d, AML_0x8E, OK);
		break;
	case AML_DefOr:
		addbuf (d, AML_OrOp, AML_Operand, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_OrOp:
		addbuf (d, AML_0x7D, OK);
		break;
	case AML_DefPackage:
		/* iMac (Retina 5K, 27-inch, Late 2015) has an SSDT
		 * "Cpu0Ist" that contains wrong value in PkgLength.
		 * Currently no workaround is found.  Use
		 * CONFIG_ACPI_IGNORE_ERROR to ignore errors. */
		addbuf (d, AML_PackageOp, AML_PkgLength, 
			AML_NumElements, AML_PackageElementList, 
			AML_PkgEND, OK);
		break;
	case AML_PackageOp:
		addbuf (d, AML_0x12, OK);
		break;
	case AML_DefVarPackage:
		addbuf (d, AML_VarPackageOp, AML_PkgLength, 
			AML_VarNumElements, AML_PackageElementList, 
			AML_PkgEND, OK);
		break;
	case AML_VarPackageOp:
		addbuf (d, AML_0x13, OK);
		break;
	case AML_NumElements:
		addbuf (d, AML_ByteData, OK);
		break;
	case AML_VarNumElements:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_PackageElement:
		addbuf (d, AML_DataRefObject, OK);
		addbuf (d, AML_NameString, OK);
		break;
	case AML_DefRefOf:
		addbuf (d, AML_RefOfOp, AML_SuperName, OK);
		break;
	case AML_RefOfOp:
		addbuf (d, AML_0x71, OK);
		break;
	case AML_DefShiftLeft:
		addbuf (d, AML_ShiftLeftOp, AML_Operand, 
			AML_ShiftCount, AML_Target, OK);
		break;
	case AML_ShiftLeftOp:
		addbuf (d, AML_0x79, OK);
		break;
	case AML_ShiftCount:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_DefShiftRight:
		addbuf (d, AML_ShiftRightOp, AML_Operand, 
			AML_ShiftCount, AML_Target, OK);
		break;
	case AML_ShiftRightOp:
		addbuf (d, AML_0x7A, OK);
		break;
	case AML_DefSizeOf:
		addbuf (d, AML_SizeOfOp, AML_SuperName, OK);
		break;
	case AML_SizeOfOp:
		addbuf (d, AML_0x87, OK);
		break;
	case AML_DefStore:
		addbuf (d, AML_StoreOp, AML_TermArg, 
			AML_SuperName, OK);
		break;
	case AML_StoreOp:
		addbuf (d, AML_0x70, OK);
		break;
	case AML_DefSubtract:
		addbuf (d, AML_SubtractOp, AML_Operand, 
			AML_Operand, AML_Target, OK);
		break;
	case AML_SubtractOp:
		addbuf (d, AML_0x74, OK);
		break;
	case AML_DefTimer:
		addbuf (d, AML_TimerOp, OK);
		break;
	case AML_TimerOp:
		addbuf (d, AML_0x5B, AML_0x33, OK);
		break;
	case AML_DefToBCD:
		addbuf (d, AML_ToBCDOp, AML_Operand, AML_Target, OK);
		break;
	case AML_ToBCDOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x29, OK);
		break;
	case AML_DefToBuffer:
		addbuf (d, AML_ToBufferOp, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_ToBufferOp:
		addbuf (d, AML_0x96, OK);
		break;
	case AML_DefToDecimalString:
		addbuf (d, AML_ToDecimalStringOp, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_ToDecimalStringOp:
		addbuf (d, AML_0x97, OK);
		break;
	case AML_DefToHexString:
		addbuf (d, AML_ToHexStringOp, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_ToHexStringOp:
		addbuf (d, AML_0x98, OK);
		break;
	case AML_DefToInteger:
		addbuf (d, AML_ToIntegerOp, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_ToIntegerOp:
		addbuf (d, AML_0x99, OK);
		break;
	case AML_DefToString:
		addbuf (d, AML_ToStringOp, AML_TermArg, 
			AML_LengthArg, AML_Target, OK);
		break;
	case AML_LengthArg:
		addbuf (d, AML_TermArg, /* => Integer */OK);
		break;
	case AML_ToStringOp:
		addbuf (d, AML_0x9C, OK);
		break;
	case AML_DefWait:
		addbuf (d, AML_WaitOp, AML_EventObject, 
			AML_Operand, OK);
		break;
	case AML_WaitOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x25, OK);
		break;
	case AML_DefXOr:
		addbuf (d, AML_XorOp, AML_Operand, AML_Operand, 
			AML_Target, OK);
		break;
	case AML_XorOp:
		addbuf (d, AML_0x7F, OK);
		break;
	case AML_ArgObj:
		addbuf (d, AML_Arg0Op, OK);
		addbuf (d, AML_Arg1Op, OK);
		addbuf (d, AML_Arg2Op, OK);
		addbuf (d, AML_Arg3Op, OK);
		addbuf (d, AML_Arg4Op, OK);
		addbuf (d, AML_Arg5Op, OK);
		addbuf (d, AML_Arg6Op, OK);
		break;
	case AML_Arg0Op:
		addbuf (d, AML_0x68, OK);
		break;
	case AML_Arg1Op:
		addbuf (d, AML_0x69, OK);
		break;
	case AML_Arg2Op:
		addbuf (d, AML_0x6A, OK);
		break;
	case AML_Arg3Op:
		addbuf (d, AML_0x6B, OK);
		break;
	case AML_Arg4Op:
		addbuf (d, AML_0x6C, OK);
		break;
	case AML_Arg5Op:
		addbuf (d, AML_0x6D, OK);
		break;
	case AML_Arg6Op:
		addbuf (d, AML_0x6E, OK);
		break;
	case AML_LocalObj:
		addbuf (d, AML_Local0Op, OK);
		addbuf (d, AML_Local1Op, OK);
		addbuf (d, AML_Local2Op, OK);
		addbuf (d, AML_Local3Op, OK);
		addbuf (d, AML_Local4Op, OK);
		addbuf (d, AML_Local5Op, OK);
		addbuf (d, AML_Local6Op, OK);
		addbuf (d, AML_Local7Op, OK);
		break;
	case AML_Local0Op:
		addbuf (d, AML_0x60, OK);
		break;
	case AML_Local1Op:
		addbuf (d, AML_0x61, OK);
		break;
	case AML_Local2Op:
		addbuf (d, AML_0x62, OK);
		break;
	case AML_Local3Op:
		addbuf (d, AML_0x63, OK);
		break;
	case AML_Local4Op:
		addbuf (d, AML_0x64, OK);
		break;
	case AML_Local5Op:
		addbuf (d, AML_0x65, OK);
		break;
	case AML_Local6Op:
		addbuf (d, AML_0x66, OK);
		break;
	case AML_Local7Op:
		addbuf (d, AML_0x67, OK);
		break;
	case AML_DebugObj:
		addbuf (d, AML_DebugOp, OK);
		break;
	case AML_DebugOp:
		addbuf (d, AML_ExtOpPrefix, AML_0x31, OK);
		break;
/* --- */
#define OKIF(COND) if (COND) goto ok
	case AML_0x00: OKIF (*d->c == 0x00); goto err;
	case AML_0x00_TO_0xFF: goto ok;
	case AML_0x01: OKIF (*d->c == 0x01); goto err;
	case AML_0x01_TO_0x7F: OKIF (*d->c >= 0x01 && *d->c <= 0x7F); goto err;
	case AML_0x02: OKIF (*d->c == 0x02); goto err;
	case AML_0x03: OKIF (*d->c == 0x03); goto err;
	case AML_0x06: OKIF (*d->c == 0x06); goto err;
	case AML_0x08: OKIF (*d->c == 0x08); goto err;
	case AML_0x0A: OKIF (*d->c == 0x0A); goto err;
	case AML_0x0B: OKIF (*d->c == 0x0B); goto err;
	case AML_0x0C: OKIF (*d->c == 0x0C); goto err;
	case AML_0x0D: OKIF (*d->c == 0x0D); goto err;
	case AML_0x0E: OKIF (*d->c == 0x0E); goto err;
	case AML_0x10: OKIF (*d->c == 0x10); goto err;
	case AML_0x11: OKIF (*d->c == 0x11); goto err;
	case AML_0x12: OKIF (*d->c == 0x12); goto err;
	case AML_0x13: OKIF (*d->c == 0x13); goto err;
	case AML_0x14: OKIF (*d->c == 0x14); goto err;
	case AML_0x15: OKIF (*d->c == 0x15); goto err;
	case AML_0x1F: OKIF (*d->c == 0x1F); goto err;
	case AML_0x20: OKIF (*d->c == 0x20); goto err;
	case AML_0x21: OKIF (*d->c == 0x21); goto err;
	case AML_0x22: OKIF (*d->c == 0x22); goto err;
	case AML_0x23: OKIF (*d->c == 0x23); goto err;
	case AML_0x24: OKIF (*d->c == 0x24); goto err;
	case AML_0x25: OKIF (*d->c == 0x25); goto err;
	case AML_0x26: OKIF (*d->c == 0x26); goto err;
	case AML_0x27: OKIF (*d->c == 0x27); goto err;
	case AML_0x28: OKIF (*d->c == 0x28); goto err;
	case AML_0x29: OKIF (*d->c == 0x29); goto err;
	case AML_0x2A: OKIF (*d->c == 0x2A); goto err;
	case AML_0x2E: OKIF (*d->c == 0x2E); goto err;
	case AML_0x2F: OKIF (*d->c == 0x2F); goto err;
	case AML_0x30: OKIF (*d->c == 0x30); goto err;
	case AML_0x30_TO_0x39: OKIF (*d->c >= 0x30 && *d->c <= 0x39); goto err;
	case AML_0x31: OKIF (*d->c == 0x31); goto err;
	case AML_0x32: OKIF (*d->c == 0x32); goto err;
	case AML_0x33: OKIF (*d->c == 0x33); goto err;
	case AML_0x41_TO_0x5A: OKIF (*d->c >= 0x41 && *d->c <= 0x5A); goto err;
	case AML_0x5B: OKIF (*d->c == 0x5B); goto err;
	case AML_0x5C: OKIF (*d->c == 0x5C); goto err;
	case AML_0x5E: OKIF (*d->c == 0x5E); goto err;
	case AML_0x5F: OKIF (*d->c == 0x5F); goto err;
	case AML_0x60: OKIF (*d->c == 0x60); goto err;
	case AML_0x61: OKIF (*d->c == 0x61); goto err;
	case AML_0x62: OKIF (*d->c == 0x62); goto err;
	case AML_0x63: OKIF (*d->c == 0x63); goto err;
	case AML_0x64: OKIF (*d->c == 0x64); goto err;
	case AML_0x65: OKIF (*d->c == 0x65); goto err;
	case AML_0x66: OKIF (*d->c == 0x66); goto err;
	case AML_0x67: OKIF (*d->c == 0x67); goto err;
	case AML_0x68: OKIF (*d->c == 0x68); goto err;
	case AML_0x69: OKIF (*d->c == 0x69); goto err;
	case AML_0x6A: OKIF (*d->c == 0x6A); goto err;
	case AML_0x6B: OKIF (*d->c == 0x6B); goto err;
	case AML_0x6C: OKIF (*d->c == 0x6C); goto err;
	case AML_0x6D: OKIF (*d->c == 0x6D); goto err;
	case AML_0x6E: OKIF (*d->c == 0x6E); goto err;
	case AML_0x70: OKIF (*d->c == 0x70); goto err;
	case AML_0x71: OKIF (*d->c == 0x71); goto err;
	case AML_0x72: OKIF (*d->c == 0x72); goto err;
	case AML_0x73: OKIF (*d->c == 0x73); goto err;
	case AML_0x74: OKIF (*d->c == 0x74); goto err;
	case AML_0x75: OKIF (*d->c == 0x75); goto err;
	case AML_0x76: OKIF (*d->c == 0x76); goto err;
	case AML_0x77: OKIF (*d->c == 0x77); goto err;
	case AML_0x78: OKIF (*d->c == 0x78); goto err;
	case AML_0x79: OKIF (*d->c == 0x79); goto err;
	case AML_0x7A: OKIF (*d->c == 0x7A); goto err;
	case AML_0x7B: OKIF (*d->c == 0x7B); goto err;
	case AML_0x7C: OKIF (*d->c == 0x7C); goto err;
	case AML_0x7D: OKIF (*d->c == 0x7D); goto err;
	case AML_0x7E: OKIF (*d->c == 0x7E); goto err;
	case AML_0x7F: OKIF (*d->c == 0x7F); goto err;
	case AML_0x80: OKIF (*d->c == 0x80); goto err;
	case AML_0x81: OKIF (*d->c == 0x81); goto err;
	case AML_0x82: OKIF (*d->c == 0x82); goto err;
	case AML_0x83: OKIF (*d->c == 0x83); goto err;
	case AML_0x84: OKIF (*d->c == 0x84); goto err;
	case AML_0x85: OKIF (*d->c == 0x85); goto err;
	case AML_0x86: OKIF (*d->c == 0x86); goto err;
	case AML_0x87: OKIF (*d->c == 0x87); goto err;
	case AML_0x88: OKIF (*d->c == 0x88); goto err;
	case AML_0x89: OKIF (*d->c == 0x89); goto err;
	case AML_0x8A: OKIF (*d->c == 0x8A); goto err;
	case AML_0x8B: OKIF (*d->c == 0x8B); goto err;
	case AML_0x8C: OKIF (*d->c == 0x8C); goto err;
	case AML_0x8D: OKIF (*d->c == 0x8D); goto err;
	case AML_0x8E: OKIF (*d->c == 0x8E); goto err;
	case AML_0x8F: OKIF (*d->c == 0x8F); goto err;
	case AML_0x90: OKIF (*d->c == 0x90); goto err;
	case AML_0x91: OKIF (*d->c == 0x91); goto err;
	case AML_0x92: OKIF (*d->c == 0x92); goto err;
	case AML_0x93: OKIF (*d->c == 0x93); goto err;
	case AML_0x94: OKIF (*d->c == 0x94); goto err;
	case AML_0x95: OKIF (*d->c == 0x95); goto err;
	case AML_0x96: OKIF (*d->c == 0x96); goto err;
	case AML_0x97: OKIF (*d->c == 0x97); goto err;
	case AML_0x98: OKIF (*d->c == 0x98); goto err;
	case AML_0x99: OKIF (*d->c == 0x99); goto err;
	case AML_0x9C: OKIF (*d->c == 0x9C); goto err;
	case AML_0x9D: OKIF (*d->c == 0x9D); goto err;
	case AML_0x9E: OKIF (*d->c == 0x9E); goto err;
	case AML_0x9F: OKIF (*d->c == 0x9F); goto err;
	case AML_0xA0: OKIF (*d->c == 0xA0); goto err;
	case AML_0xA1: OKIF (*d->c == 0xA1); goto err;
	case AML_0xA2: OKIF (*d->c == 0xA2); goto err;
	case AML_0xA3: OKIF (*d->c == 0xA3); goto err;
	case AML_0xA4: OKIF (*d->c == 0xA4); goto err;
	case AML_0xA5: OKIF (*d->c == 0xA5); goto err;
	case AML_0xCC: OKIF (*d->c == 0xCC); goto err;
	case AML_0xFF: OKIF (*d->c == 0xFF); goto err;
#undef OKIF
	case AML_BuffPkgStrObj:
		addbuf (d, AML_TermArg, /* => Buffer, Package or String */OK);
		break;
	case AML_DDBHandle:
		goto err;
		addbuf (d, AML_TermArg, OK); /* FIXME: correct? */
		break;
	case AML_DefDevice:
		/* D34010WYKH 2014-01 has ZeroOp in TermList of
		 * Device().  That is parsed in MethodInvocation. */
		addbuf (d, AML_DeviceOp, AML_PkgLength, 
			AML_NameString, AML_TermList, AML_PkgEND, OK);
		break;
	case AML_DefMethod:
		addbuf (d, AML_MethodOp, AML_PkgLength, 
			AML_NameString, AML_MethodFlags, AML_ByteList, 
			AML_PkgEND, OK);
		break;
	case AML_DWordData:	/* WordData[0:15] WordData[16:31] */
		addbuf (d, AML_WordData, AML_WordData, OK);
		break;
	case AML_MultiNamePath:
		addbuf (d, AML_MultiNamePrefix, AML_SegCount, OK);
		break;
	case AML_Nothing:
		goto loop2;
	case AML_NotifyObject:
		addbuf (d, AML_SuperName, OK);
		/* => ThermalZone | Processor | Device */
		break;
	case AML_NameString2:
		/* NameString without NullName in NamePath */
		addbuf (d, AML_RootChar, AML_NameSeg, OK);
		addbuf (d, AML_RootChar, AML_DualNamePath, OK);
		addbuf (d, AML_RootChar, AML_MultiNamePath, OK);
		addbuf (d, AML_PrefixPath, AML_NameSeg, OK);
		addbuf (d, AML_PrefixPath, AML_DualNamePath, OK);
		addbuf (d, AML_PrefixPath, AML_MultiNamePath, OK);
		break;
	case AML_ObjectReference:
		addbuf (d, AML_TermArg, OK); /* FIXME: correct? */
		break;
	case AML_PackageElementList:
		if (namecmp_system_state (d) < 0) {
			/* ignore uninteresting data */
			addbuf (d, AML_ByteList, OK);
		} else {
			addbuf (d, AML_Nothing, OK);
			addbuf (d, AML_PackageElement, 
				AML_PackageElementList, OK);
		}
		break;
	case AML_PkgLength:
		switch (*d->c & 0xC0) {
		case 0xC0:
			addbuf (d, AML_PkgLength_0, AML_PkgLength_1,
				AML_PkgLength_2, AML_PkgLength_3,
				AML_PkgLength_PUSH, OK);
			break;
		case 0x80:
			addbuf (d, AML_PkgLength_0, AML_PkgLength_1,
				AML_PkgLength_2, AML_PkgLength_PUSH, OK);
			break;
		case 0x40:
			addbuf (d, AML_PkgLength_0, AML_PkgLength_1,
				AML_PkgLength_PUSH, OK);
			break;
		case 0:
			addbuf (d, AML_PkgLength_0, AML_PkgLength_PUSH, OK);
		}
		break;
	case AML_PkgLength_0:
		d->cur->pkglen_c = d->c;
		d->cur->pkglen = *d->c;
		goto ok;
	case AML_PkgLength_1:
		d->cur->pkglen &= 0xF;
		d->cur->pkglen |= ((unsigned int)*d->c) << 4;
		goto ok;
	case AML_PkgLength_2:
		d->cur->pkglen |= ((unsigned int)*d->c) << 12;
		goto ok;
	case AML_PkgLength_3:
		d->cur->pkglen |= ((unsigned int)*d->c) << 20;
		goto ok;
	case AML_PkgLength_PUSH:
		pushpkgend (&d->cur->limithead,
			    d->cur->pkglen_c + d->cur->pkglen);
		goto loop2;
	case AML_PkgIGNORE:
		if (poppkgend (&d->cur->limithead, 0) == NULL)
			error ("pkgignore underflow");
		goto loop2;
	case AML_PkgEND:
		tmpc = poppkgend (&d->cur->limithead, 0);
		if (tmpc == NULL)
			error ("pkgend underflow");
		if (tmpc == d->c)
			goto loop2;
		goto err;
	case AML_QWordData:	/* DwordData[0:31] DwordData[32:63] */
		addbuf (d, AML_DWordData, AML_DWordData, OK);
		break;
	case AML_SegCount:
		/* addbuf (d, AML_ByteData, OK); */
		addbuf2 (d, AML_SegCount_OK, AML_NameSeg, *d->c);
		break;
	case AML_SegCount_OK:
		goto ok;
	case AML_WordData:	/* ByteData[0:7] ByteData[8:15] */
		addbuf (d, AML_ByteData, AML_ByteData, OK);
		break;
	case AML_MethodInvocation2:
		/* NameString with NullName in NamePath and method
		 * arguments */
		addbuf (d, AML_RootChar, AML_NullName, AML_TermArgList, OK);
		addbuf (d, AML_PrefixPath, AML_NullName, AML_TermArgList, OK);
		break;
	case AML_TermList2:
		addbuf (d, AML_Nothing, OK);
 		addbuf (d, AML_TermObj, AML_TermList2, OK);
		/* OneOp is a workaround for ThinkPad L570. */
		addbuf (d, AML_OneOp, AML_TermList2, OK);
		break;
	case OK:
		if (!d->cur->pathhead)
			error ("path underflow");
		curpath = d->cur->pathhead;
		curpath->ref--;
		d->cur->pathhead = curpath->next;
		if (!curpath->ref)
			parsefree (curpath);
		else if (d->cur->pathhead)
			d->cur->pathhead->ref++;
		goto loop2;
	default:
		error ("?");
	ok:
		if (poppkgend (&d->cur->limithead, 1) == d->c)
			goto err;
		for (q = d->ok; q; q = q->next) {
			if (eqlist (d->cur, q)) {
				/* workaround for some DSDT */
				for (i = 0; i < 6; i++)
					if (d->cur->system_state[i][0] >
					    q->system_state[i][0])
						break;
				if (i == 6)
					goto err;
			}
		}
		parseok (d);
		d->cur->next = d->ok;
		d->ok = d->cur;
		goto loop;
	}
err:
	parsefreepathlist (&d->cur->pathhead);
	parsefreebuflist (&d->cur->bufhead);
	parsefreelimitlist (&d->cur->limithead);
	parsefree (d->cur);
	goto loop;
}

static void
parser (unsigned char *start, unsigned char *end, bool print_progress)
{
	struct parsedata d;
	struct parsedatalist *q;
	int i, j;
	int search_device;

	search_device = 0;
#ifdef TTY_SERIAL
	search_device = 1;
#endif
	d.breakhead = NULL;
	d.progress = print_progress ? ((end - start) + 25) / 50 : 0;
	d.progresschar = 'o';
	d.start = start;
	d.end = end;

	if (d.progress) {
		for (i = 0, j = end - start; i < j; i += d.progress)
			printf (".");
		for (i = 0, j = end - start; i < j; i += d.progress)
			printf ("\b");
	}
	if (search_device) {
		d.breakfind = 1;
		d.c = start;
		d.ok = NULL;
		d.head = parsealloc (sizeof *d.head);
		for (i = 0; i <= 5; i++) {
			d.head->system_state[i][0] = 0;
			d.head->system_state_name[i] = NULL;
		}
		d.head->datalen = 0;
		d.head->bufhead = NULL;
		d.head->pathelement = OK;
		d.head->pathhead = NULL;
		d.head->next = NULL;
		addbuflist (&d.head->bufhead, AML_AMLCode);
		d.head->limithead = NULL;
		q = parsemain (&d);
		if (d.progress)
			printf ("%c", d.progresschar);
		if (q) {
			parsefreepathlist (&q->pathhead);
			parsefreebuflist (&q->bufhead);
			parsefreelimitlist (&q->limithead);
			parsefree (q);
		}
		if (d.progress) {
			for (i = 0, j = end - start; i < j; i += d.progress)
				printf ("\b");
		}
		d.progresschar = 'O';
	}
	d.breakfind = 0;
	d.c = start;
	d.ok = NULL;
	d.head = parsealloc (sizeof *d.head);
	for (i = 0; i <= 5; i++) {
		d.head->system_state[i][0] = 0;
		d.head->system_state_name[i] = NULL;
	}
	d.head->datalen = 0;
	d.head->bufhead = NULL;
	d.head->pathelement = OK;
	d.head->pathhead = NULL;
	d.head->next = NULL;
	addbuflist (&d.head->bufhead, AML_AMLCode);
	d.head->limithead = NULL;
	q = parsemain (&d);
	if (d.progress)
		printf ("%c\n", d.progresschar);
	if (!q)
		goto error;
#ifdef DISABLE_SLEEP
	if (q->system_state_name[2] && *q->system_state_name[2] == '_') {
		replace_byte (&d, q->system_state_name[2], 'D');
		printf ("Disable ACPI S2\n");
	}
	if (q->system_state_name[3] && *q->system_state_name[3] == '_') {
		replace_byte (&d, q->system_state_name[3], 'D');
		printf ("Disable ACPI S3\n");
	}
#endif
	for (i = 0; i < 6; i++) {
		if (!q->system_state[i][0])
			continue;
		for (j = 0; j < 5; j++)
			acpi_dsdt_system_state[i][j] = q->system_state[i][j];
	}
	parsefreepathlist (&q->pathhead);
	parsefreebuflist (&q->bufhead);
	parsefreelimitlist (&q->limithead);
	parsefree (q);
error:
	parsefreebreaklist (&d.breakhead);
}

void
acpi_dsdt_parse (ulong dsdt)
{
	u32 *p;
	u8 *q;
	u32 len;

	p = mapmem_hphys (dsdt, 8, 0);
	ASSERT (p);
	if (memcmp ((void *)p, "DSDT", 4))
		panic ("DSDT broken");
	len = p[1];
	unmapmem (p, 8);
	q = mapmem_hphys (dsdt, len, MAPMEM_WRITE);
	ASSERT (q);
	parser (q, q + len, true);
	unmapmem (q, len);
}

void
acpi_ssdt_parse (u8 *ssdt, u32 len)
{
	parser (ssdt, ssdt + len, false);
}
