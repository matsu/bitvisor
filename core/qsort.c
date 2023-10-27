/*
 * Copyright (c) 2023 Igel Co., Ltd
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

#include <core/string.h>
#include <core/qsort.h>

static void
swap (void *x, void *y, size_t size)
{
	char t[size];
	memcpy (t, x, size);
	memcpy (x, y, size);
	memcpy (y, t, size);
}

/* Quick sort algorithm.
 * "cmp(a, b)" is a compare function:
 * returns negative if a < b,
 * returns zero     if a == b,
 * returns positive if a > b.
 * This function is based on below article, see:
 * https://en.wikipedia.org/wiki/Quicksort
 */
void
qsort (void *base, size_t nmemb, size_t size,
       int (*cmp) (const void *, const void *))
{
	char (*p)[size], (*q)[size];
	size_t lo, hi, m, np, nq;

	if (nmemb <= 1)
		return;
	p = base;
	if (nmemb == 2) {
		if (cmp (&p[0], &p[1]) > 0)
			swap (&p[0], &p[1], size);
		return;
	}
	lo = 0;
	hi = nmemb - 1;
	m = (nmemb - 1) / 2;
	for (;; lo++, hi--) {
		while (hi != m && cmp (&p[hi], &p[m]) > 0)
			hi--;
		while (lo < hi && lo != m && cmp (&p[lo], &p[m]) < 0)
			lo++;
		if (lo >= hi) {
			q = &p[hi + 1];
			break;
		}
		swap (&p[lo], &p[hi], size);
		/* if m is swapped, update m to point to the swap dest. */
		if (lo == m)
			m = hi;
		else if (hi == m)
			m = lo;
	}
	np = q - p;
	nq = &p[nmemb] - q;
	qsort (p, np, size, cmp);
	qsort (q, nq, size, cmp);
}
