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

// SeMemory.c
// 概要: メモリ管理

#define SE_INTERNAL
#include <Se/Se.h>


// Base64 エンコード
char *SeB64Encode(void *source, UINT len)
{
	BYTE *src;
	UINT i, j;
	char *set;
	UINT set_size;
	char *ret;

	set_size = (len + 8) * 2;
	set = SeZeroMalloc(set_size);

	src = (BYTE *)source;
	j = 0;
	i = 0;

	if (!len)
	{
		return 0;
	}
	while (true)
	{
		if (i >= len)
		{
			break;
		}
		if (set)
		{
			set[j] = SeB64CodeToChar((src[i]) >> 2);
		}
		if (i + 1 >= len)
		{
			if (set)
			{
				set[j + 1] = SeB64CodeToChar((src[i] & 0x03) << 4);
				set[j + 2] = '=';
				set[j + 3] = '=';
			}
			break;
		}
		if (set)
		{
			set[j + 1] = SeB64CodeToChar(((src[i] & 0x03) << 4) + ((src[i + 1] >> 4)));
		}
		if (i + 2 >= len)
		{
			if (set)
			{
				set[j + 2] = SeB64CodeToChar((src[i + 1] & 0x0f) << 2);
				set[j + 3] = '=';
			}
			break;
		}
		if (set)
		{
			set[j + 2] = SeB64CodeToChar(((src[i + 1] & 0x0f) << 2) + ((src[i + 2] >> 6)));
			set[j + 3] = SeB64CodeToChar(src[i + 2] & 0x3f);
		}
		i += 3;
		j += 4;
	}

	ret = SeCopyStr(set);
	SeFree(set);

	return ret;
}

// Base64 デコード
SE_BUF *SeB64Decode(char *source)
{
	UINT i, j;
	char a1, a2, a3, a4;
	char *src;
	UINT f1, f2, f3, f4;
	UINT len;
	UINT set_size;
	UCHAR *set;
	SE_BUF *b;
	UCHAR zero_char = 0;

	len = SeStrLen(source);
	src = source;
	i = 0;
	j = 0;

	set_size = (len + 2) * 2;
	set = SeZeroMalloc(set_size);

	while (true)
	{
		f1 = f2 = f3 = f4 = 0;
		if (i >= len)
		{
			break;
		}
		f1 = 1;
		a1 = SeB64CharToCode(src[i]);
		if (a1 == -1)
		{
			f1 = 0;
		}
		if (i >= len + 1)
		{
			a2 = 0;
		}
		else
		{
			a2 = SeB64CharToCode(src[i + 1]);
			f2 = 1;
			if (a2 == -1)
			{
				f2 = 0;
			}
		}
		if (i >= len + 2)
		{
			a3 = 0;
		}
		else
		{
			a3 = SeB64CharToCode(src[i + 2]);
			f3 = 1;
			if (a3 == -1)
			{
				f3 = 0;
			}
		}
		if (i >= len + 3)
		{
			a4 = 0;
		}
		else
		{
			a4 = SeB64CharToCode(src[i + 3]);
			f4 = 1;
			if (a4 == -1)
			{
				f4 = 0;
			}
		}
		if (f1 && f2)
		{
			if (set)
			{
				set[j] = (a1 << 2) + (a2 >> 4);
			}
			j++;
		}
		if (f2 && f3)
		{
			if (set)
			{
				set[j] = (a2 << 4) + (a3 >> 2);
			}
			j++;
		}
		if (f3 && f4)
		{
			if (set)
			{
				set[j] = (a3 << 6) + a4;
			}
			j++;
		}
		i += 4;
	}

	b = SeNewBuf();
	SeWriteBuf(b, set, j);
	SeWriteBuf(b, &zero_char, sizeof(zero_char));
	b->Size--;

	SeFree(set);

	SeSeekBuf(b, 0, 0);

	return b;
}

// Base64 - コードを文字に変換
char SeB64CodeToChar(BYTE c)
{
	BYTE r;
	r = '=';
	if (c <= 0x19)
	{
		r = c + 'A';
	}
	if (c >= 0x1a && c <= 0x33)
	{
		r = c - 0x1a + 'a';
	}
	if (c >= 0x34 && c <= 0x3d)
	{
		r = c - 0x34 + '0';
	}
	if (c == 0x3e)
	{
		r = '+';
	}
	if (c == 0x3f)
	{
		r = '/';
	}
	return r;
}

// Base64 - 文字をコードに変換
char SeB64CharToCode(char c)
{
	if (c >= 'A' && c <= 'Z')
	{
		return c - 'A';
	}
	if (c >= 'a' && c <= 'z')
	{
		return c - 'a' + 0x1a;
	}
	if (c >= '0' && c <= '9')
	{
		return c - '0' + 0x34;
	}
	if (c == '+')
	{
		return 0x3e;
	}
	if (c == '/')
	{
		return 0x3f;
	}
	if (c == '=')
	{
		return -1;
	}
	return 0;
}

// 指定したデータブロックがすべてゼロかどうか調べる
bool SeIsZero(void *data, UINT size)
{
	UINT i;
	UCHAR *c = (UCHAR *)data;
	// 引数チェック
	if (data == NULL || size == 0)
	{
		return true;
	}

	for (i = 0;i < size;i++)
	{
		if (c[i] != 0)
		{
			return false;
		}
	}

	return true;
}

// スタックの作成
SE_STACK *SeNewStack()
{
	SE_STACK *s;

	s = SeZeroMalloc(sizeof(SE_STACK));
	s->num_item = 0;
	s->num_reserved = SE_INIT_NUM_RESERVED;
	s->p = SeMalloc(sizeof(void *) * s->num_reserved);

	return s;
}

// スタックの解放
void SeFreeStack(SE_STACK *s)
{
	// 引数チェック
	if (s == NULL)
	{
		return;
	}

	// メモリ解放
	SeFree(s->p);
	SeFree(s);
}

// Push
void SePush(SE_STACK *s, void *p)
{
	UINT i;
	// 引数チェック
	if (s == NULL || p == NULL)
	{
		return;
	}

	i = s->num_item;
	s->num_item++;
	if (s->num_item > s->num_reserved)
	{
		s->num_reserved = s->num_reserved * 2;
		s->p = SeReAlloc(s->p, sizeof(void *) * s->num_reserved);
	}
	s->p[i] = p;
}

// Pop
void *SePop(SE_STACK *s)
{
	void *ret;
	// 引数チェック
	if (s == NULL)
	{
		return NULL;
	}
	if (s->num_item == 0)
	{
		return NULL;
	}

	ret = s->p[s->num_item - 1];
	s->num_item--;
	if ((s->num_item * 2) <= s->num_reserved)
	{
		if (s->num_reserved >= (SE_INIT_NUM_RESERVED * 2))
		{
			s->num_reserved = s->num_reserved / 2;
			s->p = SeReAlloc(s->p, sizeof(void *) * s->num_reserved);
		}
	}

	return ret;
}

// キューの作成
SE_QUEUE *SeNewQueue()
{
	SE_QUEUE *q;

	q = SeZeroMalloc(sizeof(SE_QUEUE));
	q->num_item = 0;
	q->fifo = SeNewFifo();

	return q;
}

// キューの解放
void SeFreeQueue(SE_QUEUE *q)
{
	// 引数チェック
	if (q == NULL)
	{
		return;
	}

	// メモリ解放
	SeFreeFifo(q->fifo);
	SeFree(q);
}

// 1 つ取得 (キューを減らさない)
void *SePeekNext(SE_QUEUE *q)
{
	void *p = NULL;
	// 引数チェック
	if (q == NULL)
	{
		return NULL;
	}

	if (q->num_item == 0)
	{
		// アイテム無し
		return NULL;
	}

	// FIFO から読み込む
	SePeekFifo(q->fifo, &p, sizeof(void *));

	return p;
}

// 1 つ取得
void *SeGetNext(SE_QUEUE *q)
{
	void *p = NULL;
	// 引数チェック
	if (q == NULL)
	{
		return NULL;
	}

	if (q->num_item == 0)
	{
		// アイテム無し
		return NULL;
	}

	// FIFO から読み込む
	SeReadFifo(q->fifo, &p, sizeof(void *));
	q->num_item--;

	return p;
}

// 次の Int を取得
UINT SeGetNextInt(SE_QUEUE *q)
{
	UINT *p;
	UINT ret;
	// 引数チェック
	if (q == NULL)
	{
		return 0;
	}

	p = SeGetNext(q);
	if (p == NULL)
	{
		return 0;
	}

	ret = *p;
	SeFree(p);

	return *p;
}

// キューに挿入
void SeInsertQueue(SE_QUEUE *q, void *p)
{
	// 引数チェック
	if (q == NULL || p == NULL)
	{
		return;
	}

	// FIFO に書き込む
	SeWriteFifo(q->fifo, &p, sizeof(void *));

	q->num_item++;
}

// キューに Int 型を挿入
void SeInsertQueueInt(SE_QUEUE *q, UINT value)
{
	UINT *p;
	// 引数チェック
	if (q == NULL)
	{
		return;
	}

	p = SeClone(&value, sizeof(UINT));

	SeInsertQueue(q, p);
}

// リスト内のポインタを置換する
bool SeReplaceListPointer(SE_LIST *o, void *oldptr, void *newptr)
{
	UINT i;
	// 引数チェック
	if (o == NULL || oldptr == NULL || newptr == NULL)
	{
		return false;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		void *p = SE_LIST_DATA(o, i);

		if (p == oldptr)
		{
			o->p[i] = newptr;
			return true;
		}
	}

	return false;
}

// ある文字列項目がリスト内に存在しているかどうか調べる
bool SeIsInListStr(SE_LIST *o, char *str)
{
	UINT i;
	// 引数チェック
	if (o == NULL || str == NULL)
	{
		return false;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		char *s = SE_LIST_DATA(o, i);

		if (SeStrCmpi(s, str) == 0)
		{
			return true;
		}
	}

	return false;
}

// ある項目がリスト内に存在するかどうか調べる
bool SeIsInList(SE_LIST *o, void *p)
{
	UINT i;
	// 引数チェック
	if (o == NULL || p == NULL)
	{
		return false;
	}

	for (i = 0;i < SE_LIST_NUM(o);i++)
	{
		void *q = SE_LIST_DATA(o, i);
		if (p == q)
		{
			return true;
		}
	}

	return false;
}

// 文字列をリストに挿入する
bool SeInsertStr(SE_LIST *o, char *str)
{
	// 引数チェック
	if (o == NULL || str == NULL)
	{
		return false;
	}

	if (SeSearch(o, str) == NULL)
	{
		SeInsert(o, str);

		return true;
	}

	return false;
}

// 文字列比較関数
int SeCompareStr(void *p1, void *p2)
{
	char *s1, *s2;
	if (p1 == NULL || p2 == NULL)
	{
		return 0;
	}
	s1 = *(char **)p1;
	s2 = *(char **)p2;

	return SeStrCmpi(s1, s2);
}

// ソートフラグの設定
void SeSetSortFlag(SE_LIST *o, bool sorted)
{
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	o->sorted = sorted;
}

// リストに比較関数をセットする
void SeSetCmp(SE_LIST *o, SE_CALLBACK_COMPARE *cmp)
{
	// 引数チェック
	if (o == NULL || cmp == NULL)
	{
		return;
	}

	if (o->cmp != cmp)
	{
		o->cmp = cmp;
		o->sorted = false;
	}
}

// リストのクローン
SE_LIST *SeCloneList(SE_LIST *o)
{
	SE_LIST *n = SeNewList(o->cmp);

	// メモリ再確保
	SeFree(n->p);
	n->p = SeToArray(o);
	n->num_item = n->num_reserved = SE_LIST_NUM(o);
	n->sorted = o->sorted;

	return n;
}

// リストを配列化する
void *SeToArray(SE_LIST *o)
{
	void *p;
	// 引数チェック
	if (o == NULL)
	{
		return NULL;
	}

	// メモリ確保
	p = SeMalloc(sizeof(void *) * SE_LIST_NUM(o));
	// コピー
	SeCopyToArray(o, p);

	return p;
}

// リストを配列にコピー
void SeCopyToArray(SE_LIST *o, void *p)
{
	// 引数チェック
	if (o == NULL || p == NULL)
	{
		return;
	}

	SeCopy(p, o->p, sizeof(void *) * o->num_item);
}

// リストからすべての要素の削除
void SeDeleteAll(SE_LIST *o)
{
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	o->num_item = 0;
	o->num_reserved = SE_INIT_NUM_RESERVED;
	o->p = SeReAlloc(o->p, sizeof(void *) * SE_INIT_NUM_RESERVED);
}

// リストから要素の削除
bool SeDelete(SE_LIST *o, void *p)
{
	UINT i, n;
	// 引数チェック
	if (o == NULL || p == NULL)
	{
		return false;
	}

	for (i = 0;i < o->num_item;i++)
	{
		if (o->p[i] == p)
		{
			break;
		}
	}
	if (i == o->num_item)
	{
		return false;
	}

	n = i;
	for (i = n;i < (o->num_item - 1);i++)
	{
		o->p[i] = o->p[i + 1];
	}
	o->num_item--;
	if ((o->num_item * 2) <= o->num_reserved)
	{
		if (o->num_reserved > (SE_INIT_NUM_RESERVED * 2))
		{
			o->num_reserved = o->num_reserved / 2;
			o->p = SeReAlloc(o->p, sizeof(void *) * o->num_reserved);
		}
	}

	return true;
}

// リストに項目を挿入
void SeInsert(SE_LIST *o, void *p)
{
	int low, high, middle;
	UINT pos;
	int i;
	// 引数チェック
	if (o == NULL || p == NULL)
	{
		return;
	}

	if (o->cmp == NULL)
	{
		// ソート関数が無い場合は単純に追加する
		SeAdd(o, p);
		return;
	}

	// ソートされていない場合は直ちにソートする
	if (o->sorted == false)
	{
		SeSort(o);
	}

	low = 0;
	high = SE_LIST_NUM(o) - 1;

	pos = INFINITE;

	while (low <= high)
	{
		int ret;

		middle = (low + high) / 2;
		ret = o->cmp(&(o->p[middle]), &p);

		if (ret == 0)
		{
			pos = middle;
			break;
		}
		else if (ret > 0)
		{
			high = middle - 1;
		}
		else
		{
			low = middle + 1;
		}
	}

	if (pos == INFINITE)
	{
		pos = low;
	}

	o->num_item++;
	if (o->num_item > o->num_reserved)
	{
		o->num_reserved *= 2;
		o->p = SeReAlloc(o->p, sizeof(void *) * o->num_reserved);
	}

	if (SE_LIST_NUM(o) >= 2)
	{
		for (i = (SE_LIST_NUM(o) - 2);i >= (int)pos;i--)
		{
			o->p[i + 1] = o->p[i];
		}
	}

	o->p[pos] = p;
}

// リストへの要素の追加
void SeAdd(SE_LIST *o, void *p)
{
	UINT i;
	// 引数チェック
	if (o == NULL || p == NULL)
	{
		return;
	}

	i = o->num_item;
	o->num_item++;

	if (o->num_item > o->num_reserved)
	{
		o->num_reserved = o->num_reserved * 2;
		o->p = SeReAlloc(o->p, sizeof(void *) * o->num_reserved);
	}

	o->p[i] = p;
	o->sorted = false;
}

// リストのソート
void SeSort(SE_LIST *o)
{
	// 引数チェック
	if (o == NULL || o->cmp == NULL)
	{
		return;
	}

	SeQSort(o->p, o->num_item, sizeof(void *), (int(*)(void *, void *))o->cmp);
	o->sorted = true;
}

// リストのサーチ
void *SeSearch(SE_LIST *o, void *target)
{
	void **ret;
	// 引数チェック
	if (o == NULL || target == NULL)
	{
		return NULL;
	}
	if (o->cmp == NULL)
	{
		return NULL;
	}

	// ソートのチェック
	if (o->sorted == false)
	{
		// 未ソートなのでソートを行う
		SeSort(o);
	}

	ret = (void **)SeBSearch(&target, o->p, o->num_item, sizeof(void *),
		(int(*)(void *, void *))o->cmp);

	if (ret != NULL)
	{
		return *ret;
	}
	else
	{
		return NULL;
	}
}

#define SE_QSORT_STACKSIZE	(sizeof(void *) * 8 - 2)

// クイックソートの実行
void SeQSort(void *base, UINT num, UINT width, int (*compare_function)(void *, void *))
{
	UCHAR *low;
	UCHAR *high;
	UCHAR *middle;
	UCHAR *low2;
	UCHAR *high2;
	UINT size;
	UCHAR *low_stack[SE_QSORT_STACKSIZE], *high_stack[SE_QSORT_STACKSIZE];
	int stack_pointer = 0;

	if (num <= 1)
	{
		return;
	}

	low = (UCHAR *)base;
	high = (UCHAR *)base + width * (num - 1);

LABEL_RECURSE:

	size = (UINT)(high - low) / width + 1;

	middle = low + (size / 2) * width;

	if (compare_function(low, middle) > 0)
	{
		SeFastSwap(low, middle, width);
	}

	if (compare_function(low, high) > 0)
	{
		SeFastSwap(low, high, width);
	}

	if (compare_function(middle, high) > 0)
	{
		SeFastSwap(middle, high, width);
	}

	low2 = low;
	high2 = high;

	while (true)
	{
		if (middle > low2)
		{
			do
			{
				low2 += width;
			}
			while (low2 < middle && compare_function(low2, middle) <= 0);
		}

		if (middle <= low2)
		{
			do
			{
				low2 += width;
			}
			while (low2 <= high && compare_function(low2, middle) <= 0);
		}

		do
		{
			high2 -= width;
		}
		while (high2 > middle && compare_function(high2, middle) > 0);

		if (high2 < low2)
		{
			break;
		}

		SeFastSwap(low2, high2, width);

		if (middle == high2)
		{
			middle = low2;
		}
	}

	high2 += width;

	if (middle < high2)
	{
		do
		{
			high2 -= width;
		}
		while (high2 > middle && compare_function(high2, middle) == 0);
	}

	if (middle >= high2)
	{
		do
		{
			high2 -= width;
		}
		while (high2 > low && compare_function(high2, middle) == 0);
	}

	if ((high2 - low) >= (high - low2))
	{
		if (low < high2)
		{
			low_stack[stack_pointer] = low;
			high_stack[stack_pointer] = high2;
			stack_pointer++;
		}

		if (low2 < high)
		{
			low = low2;
			goto LABEL_RECURSE;
		}
	}
	else
	{
		if (low2 < high)
		{
			low_stack[stack_pointer] = low2;
			high_stack[stack_pointer] = high;
			stack_pointer++;
		}

		if (low < high2)
		{
			high = high2;
			goto LABEL_RECURSE;
		}
	}

	stack_pointer--;
	if (stack_pointer >= 0)
	{
		low = low_stack[stack_pointer];
		high = high_stack[stack_pointer];

		goto LABEL_RECURSE;
	}
}

// スワップの実行
void SeFastSwap(UCHAR *a, UCHAR *b, UINT width)
{
	UCHAR tmp;
	// 引数チェック
	if (a == b)
	{
		return;
	}

	while (width--)
	{
		tmp = *a;
		*a++ = *b;
		*b++ = tmp;
	}
}

// バイナリサーチの実行
void *SeBSearch(void *key, void *base, UINT num, UINT width, int (*compare_function)(void *, void *))
{
	UCHAR *low = (UCHAR *)base;
	UCHAR *high = (UCHAR *)base + width * (num - 1);
	UCHAR *middle;
	UINT half;
	int ret;

	while (low <= high)
	{
		if ((half = (num / 2)) != 0)
		{
			middle = low + (((num % 2) != 0) ? half : (half - 1)) * width;
			ret = compare_function(key, middle);

			if (ret == 0)
			{
				return middle;
			}
			else if (ret < 0)
			{
				high = middle - width;
				num = ((num % 2) != 0) ? half : half - 1;
			}
			else
			{
				low = middle + width;
				num = half;
			}
		}
		else if (num != 0)
		{
			if (compare_function(key, low) == 0)
			{
				return low;
			}
			else
			{
				return NULL;
			}
		}
		else
		{
			break;
		}
	}

	return NULL;
}

// リストの解放
void SeFreeList(SE_LIST *o)
{
	// 引数チェック
	if (o == NULL)
	{
		return;
	}

	SeFree(o->p);
	SeFree(o);
}

// リストの作成
SE_LIST *SeNewList(SE_CALLBACK_COMPARE *cmp)
{
	SE_LIST *o;

	o = SeZeroMalloc(sizeof(SE_LIST));

	o->num_item = 0;
	o->num_reserved = SE_INIT_NUM_RESERVED;
	o->p = SeMalloc(sizeof(void *) * o->num_reserved);
	o->cmp = cmp;
	o->sorted = true;

	return o;
}

// FIFO のサイズ取得
UINT SeFifoSize(SE_FIFO *f)
{
	// 引数チェック
	if (f == NULL)
	{
		return 0;
	}

	return f->size;
}

// FIFO のクリア
void SeClearFifo(SE_FIFO *f)
{
	// 引数チェック
	if (f == NULL)
	{
		return;
	}

	f->size = f->pos = 0;
	f->memsize = SE_FIFO_INIT_MEM_SIZE;
	f->p = SeReAlloc(f->p, f->memsize);
}

// FIFO に書き込む
void SeWriteFifo(SE_FIFO *f, void *p, UINT size)
{
	UINT i, need_size;
	bool realloc_flag;
	// 引数チェック
	if (f == NULL || size == 0)
	{
		return;
	}

	i = f->size;
	f->size += size;
	need_size = f->pos + f->size;
	realloc_flag = false;

	// メモリ拡張
	while (need_size > f->memsize)
	{
		f->memsize = MAX(f->memsize, SE_FIFO_INIT_MEM_SIZE) * 3;
		realloc_flag = true;
	}

	if (realloc_flag)
	{
		f->p = SeReAlloc(f->p, f->memsize);
	}

	// データ書き込み
	if (p != NULL)
	{
		SeCopy((UCHAR *)f->p + f->pos + i, p, size);
	}
}

// FIFO から読み取る
UINT SeReadFifo(SE_FIFO *f, void *p, UINT size)
{
	UINT read_size;
	// 引数チェック
	if (f == NULL || size == 0)
	{
		return 0;
	}

	read_size = MIN(size, f->size);
	if (read_size == 0)
	{
		return 0;
	}
	if (p != NULL)
	{
		SeCopy(p, (UCHAR *)f->p + f->pos, read_size);
	}
	f->pos += read_size;
	f->size -= read_size;

	if (f->size == 0)
	{
		f->pos = 0;
	}

	// メモリの詰め直し
	if (f->pos >= SE_FIFO_INIT_MEM_SIZE &&
		f->memsize >= SE_FIFO_REALLOC_MEM_SIZE &&
		(f->memsize / 2) > f->size)
	{
		void *new_p;
		UINT new_size;

		new_size = MAX(f->memsize / 2, SE_FIFO_INIT_MEM_SIZE);
		new_p = SeMalloc(new_size);
		SeCopy(new_p, (UCHAR *)f->p + f->pos, f->size);

		SeFree(f->p);

		f->memsize = new_size;
		f->p = new_p;
		f->pos = 0;
	}

	return read_size;
}

// FIFO を peek する
UINT SePeekFifo(SE_FIFO *f, void *p, UINT size)
{
	UINT read_size;
	if (f == NULL || size == 0)
	{
		return 0;
	}

	read_size = MIN(size, f->size);
	if (read_size == 0)
	{
		return 0;
	}

	if (p != NULL)
	{
		SeCopy(p, (UCHAR *)f->p + f->pos, read_size);
	}

	return read_size;
}

// FIFO の解放
void SeFreeFifo(SE_FIFO *f)
{
	// 引数チェック
	if (f == NULL)
	{
		return;
	}

	SeFree(f->p);
	SeFree(f);
}

// FIFO の作成
SE_FIFO *SeNewFifo()
{
	SE_FIFO *f;

	// メモリ確保
	f = SeMalloc(sizeof(SE_FIFO));

	f->size = f->pos = 0;
	f->memsize = SE_FIFO_INIT_MEM_SIZE;
	f->p = SeMalloc(SE_FIFO_INIT_MEM_SIZE);

	return f;
}

// バッファから読み込み
UINT SeReadBuf(SE_BUF *b, void *buf, UINT size)
{
	UINT size_read;
	// 引数チェック
	if (b == NULL || size == 0)
	{
		return 0;
	}

	if (b->Buf == NULL)
	{
		SeZero(buf, size);
		return 0;
	}
	size_read = size;
	if ((b->Current + size) >= b->Size)
	{
		size_read = b->Size - b->Current;
		if (buf != NULL)
		{
			SeZero((UCHAR *)buf + size_read, size - size_read);
		}
	}

	if (buf != NULL)
	{
		SeCopy(buf, (UCHAR *)b->Buf + b->Current, size_read);
	}

	b->Current += size_read;

	return size_read;
}

// ダンプファイルからデータを読み込む
SE_BUF *SeReadDump(char *filename)
{
	SE_BUF *b;
	void *data;
	UINT data_size;
	// 引数チェック
	if (filename == NULL)
	{
		return NULL;
	}

	if (SeSysLoadData(filename, &data, &data_size) == false)
	{
		return NULL;
	}

	b = SeNewBuf();
	SeWriteBuf(b, data, data_size);
	SeSysFreeData(data);

	SeSeekBuf(b, 0, 0);

	return b;
}

// バッファ内容をファイルにダンプする
bool SeDumpBuf(SE_BUF *b, char *filename)
{
	// 引数チェック
	if (b == NULL || filename == NULL)
	{
		return false;
	}

	return SeSysSaveData(filename, b->Buf, b->Size);
}

// バッファに文字列を追記
void SeAddBufStr(SE_BUF *b, char *str)
{
	// 引数チェック
	if (b == NULL || str == NULL)
	{
		return;
	}

	SeWriteBuf(b, str, SeStrLen(str));
}

// バッファに文字列を書き込む
bool SeWriteBufStr(SE_BUF *b, char *str)
{
	UINT len;
	// 引数チェック
	if (b == NULL || str == NULL)
	{
		return false;
	}

	// 文字列長
	len = SeStrLen(str);
	if (SeWriteBufInt(b, len + 1) == false)
	{
		return false;
	}

	// 文字列本体
	SeWriteBuf(b, str, len);

	return true;
}

// バッファから文字列を読み込む
bool SeReadBufStr(SE_BUF *b, char *str, UINT size)
{
	UINT len;
	UINT read_size;
	// 引数チェック
	if (b == NULL || str == NULL || size == 0)
	{
		return false;
	}

	// 文字列長を読み込む
	len = SeReadBufInt(b);
	if (len == 0)
	{
		return false;
	}
	len--;
	if (len <= (size - 1))
	{
		size = len + 1;
	}

	read_size = MIN(len, (size - 1));

	// 文字列本体を読み込む
	if (SeReadBuf(b, str, read_size) != read_size)
	{
		return false;
	}
	if (read_size < len)
	{
		SeReadBuf(b, NULL, len - read_size);
	}
	str[len] = 0;

	return true;
}

// バッファに 64 bit 整数を書き込む
bool SeWriteBufInt64(SE_BUF *b, UINT64 value)
{
	// 引数チェック
	if (b == NULL)
	{
		return false;
	}

	value = SeEndian64(value);

	SeWriteBuf(b, &value, sizeof(UINT64));
	return true;
}

// バッファに整数を書き込む
bool SeWriteBufInt(SE_BUF *b, UINT value)
{
	// 引数チェック
	if (b == NULL)
	{
		return false;
	}

	value = SeEndian32(value);

	SeWriteBuf(b, &value, sizeof(UINT));
	return true;
}

// バッファから 64bit 整数を読み込む
UINT64 SeReadBufInt64(SE_BUF *b)
{
	UINT64 value;
	// 引数チェック
	if (b == NULL)
	{
		return 0;
	}

	if (SeReadBuf(b, &value, sizeof(UINT64)) != sizeof(UINT64))
	{
		return 0;
	}
	return SeEndian64(value);
}

// バッファから整数を読み込む
UINT SeReadBufInt(SE_BUF *b)
{
	UINT value;
	// 引数チェック
	if (b == NULL)
	{
		return 0;
	}

	if (SeReadBuf(b, &value, sizeof(UINT)) != sizeof(UINT))
	{
		return 0;
	}
	return SeEndian32(value);
}

// バッファサイズの調整
void SeAdjustBufSize(SE_BUF *b, UINT new_size)
{
	// 引数チェック
	if (b == NULL)
	{
		return;
	}

	if (b->SizeReserved >= new_size)
	{
		return;
	}

	while (b->SizeReserved < new_size)
	{
		b->SizeReserved = b->SizeReserved * 2;
	}
	b->Buf = SeReAlloc(b->Buf, b->SizeReserved);
}

// バッファのシーク
void SeSeekBuf(SE_BUF *b, UINT offset, int mode)
{
	UINT new_pos;
	// 引数チェック
	if (b == NULL)
	{
		return;
	}

	if (mode == 0)
	{
		// 絶対位置
		new_pos = offset;
	}
	else
	{
		if (mode > 0)
		{
			// 右へ移動
			new_pos = b->Current + offset;
		}
		else
		{
			// 左へ移動
			if (b->Current >= offset)
			{
				new_pos = b->Current - offset;
			}
			else
			{
				new_pos = 0;
			}
		}
	}
	b->Current = MAKESURE(new_pos, 0, b->Size);
}

// バッファの解放 (バッファ本体は解放しない)
void SeFreeBufWithoutBuffer(SE_BUF *b)
{
	// 引数チェック
	if (b == NULL)
	{
		return;
	}

	SeFree(b);
}

// バッファの解放
void SeFreeBuf(SE_BUF *b)
{
	// 引数チェック
	if (b == NULL)
	{
		return;
	}

	// メモリ解放
	SeFree(b->Buf);
	SeFree(b);
}

// バッファのうち残されている部分を読み込み
SE_BUF *SeReadRemainBuf(SE_BUF *b)
{
	UINT size;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	if (b->Size < b->Current)
	{
		return NULL;
	}

	size = b->Size - b->Current;

	return SeReadBufFromBuf(b, size);
}

// バッファの比較
bool SeCmpBuf(SE_BUF *b1, SE_BUF *b2)
{
	// 引数チェック
	if (b1 == NULL || b2 == NULL)
	{
		if (b1 == NULL && b2 == NULL)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	if (b1->Size != b2->Size)
	{
		return false;
	}

	if (SeCmp(b1->Buf, b2->Buf, b1->Size) != 0)
	{
		return false;
	}

	return true;
}

// バッファからバッファを読み込み
SE_BUF *SeReadBufFromBuf(SE_BUF *b, UINT size)
{
	SE_BUF *ret;
	UCHAR *data;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	data = SeMalloc(size);
	if (SeReadBuf(b, data, size) != size)
	{
		SeFree(data);
		return NULL;
	}

	ret = SeNewBuf();
	SeWriteBuf(ret, data, size);
	SeSeekBuf(ret, 0, 0);

	SeFree(data);

	return ret;
}

// バッファにバッファを書き込み
void SeWriteBufBuf(SE_BUF *b, SE_BUF *bb)
{
	// 引数チェック
	if (b == NULL || bb == NULL)
	{
		return;
	}

	SeWriteBuf(b, bb->Buf, bb->Size);
}

// バッファに 1 行書き込む
void SeWriteBufLine(SE_BUF *b, char *str)
{
	char *crlf = "\r\n";
	// 引数チェック
	if (b == NULL || str == NULL)
	{
		return;
	}

	SeWriteBuf(b, str, SeStrLen(str));
	SeWriteBuf(b, crlf, SeStrLen(crlf));
}

// バッファへ書き込み
void SeWriteBuf(SE_BUF *b, void *buf, UINT size)
{
	UINT new_size;
	// 引数チェック
	if (b == NULL || buf == NULL || size == 0)
	{
		return;
	}

	new_size = b->Current + size;
	if (new_size > b->Size)
	{
		// サイズを調整する
		SeAdjustBufSize(b, new_size);
	}
	if (b->Buf != NULL)
	{
		SeCopy((UCHAR *)b->Buf + b->Current, buf, size);
	}
	b->Current += size;
	b->Size = new_size;
}

// バッファのクローン
SE_BUF *SeCloneBuf(SE_BUF *b)
{
	SE_BUF *bb;
	// 引数チェック
	if (b == NULL)
	{
		return NULL;
	}

	bb = SeMemToBuf(b->Buf, b->Size);

	return bb;
}

// バッファのクリア
void SeClearBuf(SE_BUF *b)
{
	// 引数チェック
	if (b == NULL)
	{
		return;
	}

	b->Size = 0;
	b->Current = 0;
}

// メモリ領域をバッファに変換
SE_BUF *SeMemToBuf(void *data, UINT size)
{
	SE_BUF *b;
	// 引数チェック
	if (data == NULL && size != 0)
	{
		return NULL;
	}

	b = SeNewBuf();
	SeWriteBuf(b, data, size);
	SeSeekBuf(b, 0, 0);

	return b;
}

// 乱数バッファの作成
SE_BUF *SeRandBuf(UINT size)
{
	void *data = SeMalloc(size);
	SE_BUF *ret;

	SeRand(data, size);

	ret = SeMemToBuf(data, size);

	SeFree(data);

	return ret;
}

// バッファの作成
SE_BUF *SeNewBuf()
{
	SE_BUF *b;

	// メモリ確保
	b = SeMalloc(sizeof(SE_BUF));
	b->Buf = SeMalloc(SE_INIT_BUF_SIZE);
	b->Size = 0;
	b->Current = 0;
	b->SizeReserved = SE_INIT_BUF_SIZE;

	return b;
}

// 64 bit エンディアン変換
UINT64 SeEndian64(UINT64 value)
{
	if (SeIsLittleEndian())
	{
		return SeSwap64(value);
	}
	else
	{
		return value;
	}
}

// 32 bit エンディアン変換
UINT SeEndian32(UINT value)
{
	if (SeIsLittleEndian())
	{
		return SeSwap32(value);
	}
	else
	{
		return value;
	}
}

// 16 bit エンディアン変換
USHORT SeEndian16(USHORT value)
{
	if (SeIsLittleEndian())
	{
		return SeSwap16(value);
	}
	else
	{
		return value;
	}
}

// 任意のデータのスワップ
void SeSwap(void *buf, UINT size)
{
	UCHAR *tmp, *src;
	UINT i;
	// 引数チェック
	if (buf == NULL || size == 0)
	{
		return;
	}

	src = (UCHAR *)buf;
	tmp = SeMalloc(size);
	for (i = 0;i < size;i++)
	{
		tmp[size - i - 1] = src[i];
	}

	SeCopy(buf, tmp, size);
	SeFree(buf);
}

// 16bit スワップ
USHORT SeSwap16(USHORT value)
{
	USHORT r;

	((BYTE *)&r)[0] = ((BYTE *)&value)[1];
	((BYTE *)&r)[1] = ((BYTE *)&value)[0];

	return r;
}

// 32bit スワップ
UINT SeSwap32(UINT value)
{
	UINT r;

	((BYTE *)&r)[0] = ((BYTE *)&value)[3];
	((BYTE *)&r)[1] = ((BYTE *)&value)[2];
	((BYTE *)&r)[2] = ((BYTE *)&value)[1];
	((BYTE *)&r)[3] = ((BYTE *)&value)[0];

	return r;
}

// 64bit スワップ
UINT64 SeSwap64(UINT64 value)
{
	UINT64 r;

	((BYTE *)&r)[0] = ((BYTE *)&value)[7];
	((BYTE *)&r)[1] = ((BYTE *)&value)[6];
	((BYTE *)&r)[2] = ((BYTE *)&value)[5];
	((BYTE *)&r)[3] = ((BYTE *)&value)[4];
	((BYTE *)&r)[4] = ((BYTE *)&value)[3];
	((BYTE *)&r)[5] = ((BYTE *)&value)[2];
	((BYTE *)&r)[6] = ((BYTE *)&value)[1];
	((BYTE *)&r)[7] = ((BYTE *)&value)[0];

	return r;
}

// メモリ領域のクローン
void *SeClone(void *addr, UINT size)
{
	void *p;
	// 引数チェック
	if (addr == NULL)
	{
		return NULL;
	}

	p = SeMalloc(size);
	SeCopy(p, addr, size);

	return p;
}

// メモリを確保してゼロクリア
void *SeZeroMalloc(UINT size)
{
	void *p = SeMalloc(size);

	if (p == NULL)
	{
		return NULL;
	}

	SeZero(p, size);

	return p;
}

// メモリの確保
void *SeMalloc(UINT size)
{
	UINT real_size;
	void *p;
	void *ret;

	real_size = size + sizeof(UINT) * 2;

	p = SeSysMemoryAlloc(real_size);

	if (p == NULL)
	{
		SeSysLog(SE_LOG_FATAL, "Memory Allocation Failed.");
		return NULL;
	}

	((UINT *)p)[0] = 0x12345678;
	((UINT *)p)[1] = size;

	ret = (void *)(((UCHAR *)p) + sizeof(UINT) * 2);

	return ret;
}

// メモリの再確保
void *SeReAlloc(void *addr, UINT size)
{
	UINT real_size;
	void *real_addr;
	void *p;
	void *ret;
	// 引数チェック
	if (addr == NULL)
	{
		return NULL;
	}

	real_addr = (void *)(((UCHAR *)addr) - sizeof(UINT) * 2);
	real_size = size + sizeof(UINT) * 2;

	if (((UINT *)real_addr)[0] != 0x12345678)
	{
		SeSysLog(SE_LOG_FATAL, "Bad Memory Block.");
		return NULL;
	}

	p = SeSysMemoryReAlloc(real_addr, real_size);

	if (p == NULL)
	{
		SeSysLog(SE_LOG_FATAL, "Memory Allocation Failed.");
		return NULL;
	}

	((UINT *)p)[0] = 0x12345678;
	((UINT *)p)[1] = size;

	ret = (void *)(((UCHAR *)p) + sizeof(UINT) * 2);

	return ret;
}

// メモリブロックサイズの取得
UINT SeMemSize(void *addr)
{
	void *real_addr;
	// 引数チェック
	if (addr == NULL)
	{
		return 0;
	}

	real_addr = (void *)(((UCHAR *)addr) - sizeof(UINT) * 2);

	if (((UINT *)real_addr)[0] != 0x12345678)
	{
		SeSysLog(SE_LOG_FATAL, "Bad Memory Block.");
		return 0;
	}

	return ((UINT *)real_addr)[1];
}

// メモリの解放
void SeFree(void *addr)
{
	void *real_addr;
	// 引数チェック
	if (addr == NULL)
	{
		return;
	}

	real_addr = (void *)(((UCHAR *)addr) - sizeof(UINT) * 2);

	if (((UINT *)real_addr)[0] != 0x12345678)
	{
		SeSysLog(SE_LOG_FATAL, "Bad Memory Block.");
		return;
	}

	SeSysMemoryFree(real_addr);
}

// リトルエンディアンかどうか取得
bool SeIsLittleEndian()
{
	static UINT value = 0x00000001;
	UCHAR *c;

	c = (UCHAR *)(&value);

	return (*c == 0 ? false : true);
}

// ビッグエンディアンかどうか取得
bool SeIsBigEndian()
{
	return SeIsLittleEndian() ? false : true;
}

// メモリ比較 2
bool SeCmpEx(void *addr1, UINT size1, void *addr2, UINT size2)
{
	// 引数チェック
	if (addr1 == NULL || addr2 == NULL)
	{
		return false;
	}

	if (size1 != size2)
	{
		return false;
	}

	if (SeCmp(addr1, addr2, size1) != 0)
	{
		return false;
	}

	return true;
}

// メモリ比較
int SeCmp(void *addr1, void *addr2, UINT size)
{
	UINT i;
	UCHAR *p1, *p2;
	// 引数チェック
	if (addr1 == NULL || addr2 == NULL || size == 0)
	{
		return 0;
	}

	p1 = (UCHAR *)addr1;
	p2 = (UCHAR *)addr2;

	for (i = 0;i < size;i++)
	{
		if (*p1 > *p2)
		{
			return 1;
		}
		else if (*p1 < *p2)
		{
			return -1;
		}

		p1++;
		p2++;
	}

	return 0;
}

// メモリコピー
void SeCopy(void *dst, void *src, UINT size)
{
	UINT i;
	UCHAR *p1, *p2;
	// 引数チェック
	if (dst == NULL || src == NULL || size == 0)
	{
		return;
	}

	p1 = (UCHAR *)dst;
	p2 = (UCHAR *)src;

	for (i = 0;i < size;i++)
	{
		*(p1++) = *(p2++);
	}
}

// メモリクリア
void SeZero(void *addr, UINT size)
{
	UINT i;
	UCHAR *p;
	// 引数チェック
	if (addr == NULL || size == 0)
	{
		return;
	}

	p = (UCHAR *)addr;

	for (i = 0;i < size;i++)
	{
		*(p++) = 0;
	}
}


