/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (C) 2007, 2008 
 *      National Institute of Information and Communications Technology
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

// Secure VM Project
// VPN Client Module (IPsec Driver) Source Code
// 
// Developed by Daiyuu Nobori (dnobori@cs.tsukuba.ac.jp)

// SeMemory.h
// 概要: SeMemory.c のヘッダ

#ifndef	SEMEMORY_H
#define SEMEMORY_H

// 定数
#define	SE_INIT_BUF_SIZE			10240
#define	SE_FIFO_INIT_MEM_SIZE		4096
#define	SE_FIFO_REALLOC_MEM_SIZE	(65536 * 10)	// 絶妙な値
#define	SE_INIT_NUM_RESERVED		32

// バッファ
struct SE_BUF
{
	void *Buf;
	UINT Size;
	UINT SizeReserved;
	UINT Current;
};

// FIFO
struct SE_FIFO
{
	void *p;
	UINT pos, size, memsize;
};

// リスト
struct SE_LIST
{
	UINT num_item, num_reserved;
	void **p;
	SE_CALLBACK_COMPARE *cmp;
	bool sorted;
};

// キュー
struct SE_QUEUE
{
	UINT num_item;
	SE_FIFO *fifo;
};

// スタック
struct SE_STACK
{
	UINT num_item, num_reserved;
	void **p;
};

// マクロ
#define	SE_LIST_DATA(o, i)		(((o) != NULL) ? ((o)->p[(i)]) : NULL)
#define	SE_LIST_NUM(o)			(((o) != NULL) ? (o)->num_item : 0)

#if	0
#define SE_GETARG(ret, start, index)		\
{											\
	UCHAR *pointer = (UCHAR *)(&start);		\
	void **pointer2;						\
	UINT index_copy = (index);				\
	pointer += (UINT)(sizeof(void *) * (index_copy + 1));	\
	pointer2 = (void **)pointer;			\
	ret = *pointer2;						\
}
#define SE_BUILD_ARGLIST(start)				\
	((void **)(&start)) + 1
#endif

// 関数プロトタイプ
void SeZero(void *addr, UINT size);
void SeCopy(void *dst, void *src, UINT size);
int SeCmp(void *addr1, void *addr2, UINT size);
bool SeCmpEx(void *addr1, UINT size1, void *addr2, UINT size2);
bool SeIsLittleEndian();
bool SeIsBigEndian();
void *SeMalloc(UINT size);
void *SeReAlloc(void *addr, UINT size);
void SeFree(void *addr);
void *SeZeroMalloc(UINT size);
UINT SeMemSize(void *addr);
void *SeClone(void *addr, UINT size);
UINT64 SeSwap64(UINT64 value);
UINT SeSwap32(UINT value);
USHORT SeSwap16(USHORT value);
void SeSwap(void *buf, UINT size);
UINT64 SeEndian64(UINT64 value);
UINT SeEndian32(UINT value);
USHORT SeEndian16(USHORT value);
bool SeIsZero(void *data, UINT size);
char *SeB64Encode(void *source, UINT len);
SE_BUF *SeB64Decode(char *source);
char SeB64CodeToChar(BYTE c);
char SeB64CharToCode(char c);

SE_BUF *SeNewBuf();
SE_BUF *SeMemToBuf(void *data, UINT size);
SE_BUF *SeRandBuf(UINT size);
SE_BUF *SeCloneBuf(SE_BUF *b);
void SeClearBuf(SE_BUF *b);
void SeWriteBuf(SE_BUF *b, void *buf, UINT size);
void SeWriteBufBuf(SE_BUF *b, SE_BUF *bb);
UINT SeReadBuf(SE_BUF *b, void *buf, UINT size);
SE_BUF *SeReadBufFromBuf(SE_BUF *b, UINT size);
SE_BUF *SeReadRemainBuf(SE_BUF *b);
void SeAdjustBufSize(SE_BUF *b, UINT new_size);
void SeSeekBuf(SE_BUF *b, UINT offset, int mode);
void SeFreeBuf(SE_BUF *b);
void SeFreeBufWithoutBuffer(SE_BUF *b);
UINT SeReadBufInt(SE_BUF *b);
UINT64 SeReadBufInt64(SE_BUF *b);
bool SeWriteBufInt(SE_BUF *b, UINT value);
bool SeWriteBufInt64(SE_BUF *b, UINT64 value);
bool SeReadBufStr(SE_BUF *b, char *str, UINT size);
bool SeWriteBufStr(SE_BUF *b, char *str);
void SeWriteBufLine(SE_BUF *b, char *str);
void SeAddBufStr(SE_BUF *b, char *str);
bool SeDumpBuf(SE_BUF *b, char *filename);
SE_BUF *SeReadDump(char *filename);
bool SeCmpBuf(SE_BUF *b1, SE_BUF *b2);

SE_FIFO *SeNewFifo();
void SeFreeFifo(SE_FIFO *f);
UINT SePeekFifo(SE_FIFO *f, void *p, UINT size);
UINT SeReadFifo(SE_FIFO *f, void *p, UINT size);
void SeWriteFifo(SE_FIFO *f, void *p, UINT size);
void SeClearFifo(SE_FIFO *f);
UINT SeFifoSize(SE_FIFO *f);

SE_LIST *SeNewList(SE_CALLBACK_COMPARE *cmp);
void SeFreeList(SE_LIST *o);
void *SeSearch(SE_LIST *o, void *target);
void *SeBSearch(void *key, void *base, UINT num, UINT width, int (*compare_function)(void *, void *));
void SeSort(SE_LIST *o);
void SeQSort(void *base, UINT num, UINT width, int (*compare_function)(void *, void *));
void SeFastSwap(UCHAR *a, UCHAR *b, UINT width);
void SeAdd(SE_LIST *o, void *p);
void SeInsert(SE_LIST *o, void *p);
bool SeDelete(SE_LIST *o, void *p);
void SeDeleteAll(SE_LIST *o);
void SeCopyToArray(SE_LIST *o, void *p);
void *SeToArray(SE_LIST *o);
SE_LIST *SeCloneList(SE_LIST *o);
void SeSetCmp(SE_LIST *o, SE_CALLBACK_COMPARE *cmp);
void SeSetSortFlag(SE_LIST *o, bool sorted);
int SeCompareStr(void *p1, void *p2);
bool SeInsertStr(SE_LIST *o, char *str);
bool SeIsInList(SE_LIST *o, void *p);
bool SeIsInListStr(SE_LIST *o, char *str);
bool SeReplaceListPointer(SE_LIST *o, void *oldptr, void *newptr);

SE_QUEUE *SeNewQueue();
void SeFreeQueue(SE_QUEUE *q);
void *SeGetNext(SE_QUEUE *q);
void *SePeekNext(SE_QUEUE *q);
UINT SeGetNextInt(SE_QUEUE *q);
void SeInsertQueue(SE_QUEUE *q, void *p);
void SeInsertQueueInt(SE_QUEUE *q, UINT value);

SE_STACK *SeNewStack();
void SeFreeStack(SE_STACK *s);
void SePush(SE_STACK *s, void *p);
void *SePop(SE_STACK *s);



#endif	// SEMEMORY_H

