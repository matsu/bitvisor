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
// crypto library
// 
// Based on OpenSSL
// 
// By dnobori@cs.tsukuba.ac.jp

#include <core/mm.h>
#include <chelp.h>

#define UCHAR	unsigned char
#define UINT	unsigned int

// メモリ確保
void *chelp_malloc(unsigned long size)
{
	// 引数チェック
	if (size == 0)
	{
		size = 1;
	}

	return alloc (size);
}

// メモリ再確保
void *chelp_realloc(void *memblock, unsigned long size)
{
	// 引数チェック
	if (memblock == NULL)
	{
		return NULL;
	}
	if (size == 0)
	{
		size = 1;
	}

	return realloc (memblock, size);
}

// メモリ解放
void chelp_free(void *memblock)
{
	// 引数チェック
	if (memblock == NULL)
	{
		return;
	}

	free (memblock);
}

#define CHELP_QSORT_STACKSIZE	(sizeof(void *) * 8 - 2)

// クイックソートの実行
void chelp_qsort(void *base, UINT num, UINT width, int (*compare_function)(const void *, const void *))
{
	UCHAR *low;
	UCHAR *high;
	UCHAR *middle;
	UCHAR *low2;
	UCHAR *high2;
	UINT size;
	UCHAR *low_stack[CHELP_QSORT_STACKSIZE], *high_stack[CHELP_QSORT_STACKSIZE];
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
		chelp_swap(low, middle, width);
	}

	if (compare_function(low, high) > 0)
	{
		chelp_swap(low, high, width);
	}

	if (compare_function(middle, high) > 0)
	{
		chelp_swap(middle, high, width);
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

		chelp_swap(low2, high2, width);

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
void chelp_swap(UCHAR *a, UCHAR *b, UINT width)
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
void *chelp_bsearch(void *key, void *base, UINT num, UINT width, int (*compare_function)(const void *, const void *))
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

// メモリ検索
void *chelp_memchr(const void *buf, int chr, UINT count)
{
	// 引数チェック
	if (buf == NULL)
	{
		return NULL;
	}

	while ((count != 0) && (*(UCHAR *)buf != (UCHAR)chr))
	{
		buf = (UCHAR *)buf + 1;
		count--;
	}

	return (count ? (void *)buf : NULL);
}

// メモリ移動
void *chelp_memmove(void *dst, const void *src, UINT count)
{
	void *ret = dst;
	// 引数チェック
	if (dst == NULL || src == NULL || count == 0)
	{
		return NULL;
	}

	if (dst <= src || (UCHAR *)dst >= ((UCHAR *)src + count))
	{
		while (count--)
		{
			*(UCHAR *)dst = *(UCHAR *)src;
			dst = (UCHAR *)dst + 1;
			src = (UCHAR *)src + 1;
		}
	}
	else
	{
		dst = (UCHAR *)dst + count - 1;
		src = (UCHAR *)src + count - 1;

		while (count--)
		{
			*(UCHAR *)dst = *(UCHAR *)src;
			dst = (UCHAR *)dst - 1;
			src = (UCHAR *)src - 1;
		}
	}

	return ret;
}

// メモリ比較
int chelp_memcmp(const void *addr1, const void *addr2, UINT size)
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
void *chelp_memcpy(void *dst, const void *src, UINT size)
{
	UINT i;
	UCHAR *p1, *p2;
	// 引数チェック
	if (dst == NULL || src == NULL || size == 0)
	{
		return dst;
	}

	p1 = (UCHAR *)dst;
	p2 = (UCHAR *)src;

	for (i = 0;i < size;i++)
	{
		*(p1++) = *(p2++);
	}

	return dst;
}

// メモリセット
void *chelp_memset(void *dst, int c, UINT count)
{
	UINT i;
	UCHAR *p;
	// 引数チェック
	if (dst == NULL)
	{
		return dst;
	}

	p = (UCHAR *)dst;

	for (i = 0;i < count;i++)
	{
		*(p++) = (UCHAR)c;
	}

	return dst;
}

