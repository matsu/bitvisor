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

#ifndef _ARRAYLIST_H_
#define _ARRAYLIST_H_

#define ARRAYLIST_DEFAULT_NUM 128
#define ARRAYLIST_DEFINE(listname)
#define ARRAYLIST_DEFINE_HEAD_NUM(listname, maxnum) int listname##_num = 0; int listname##_max = maxnum; void *listname##_array[maxnum] = {NULL};
#define ARRAYLIST_DEFINE_HEAD(listname) ARRAYLIST_DEFINE_HEAD_NUM(listname, ARRAYLIST_DEFAULT_NUM)

#define ARRAYLIST_APPEND(listname, new)				\
	{							\
		if (listname##_num >= listname##_max)		\
			panic("too many arraylist items\n");	\
		listname##_array[listname##_num++] = new;	\
	}

#define ARRAYLIST_REMOVE_INDEX(listname, index)				\
	{								\
		int __i;						\
		for (__i = index; __i < listname##_num; __i++)		\
			listname##_array[__i] = listname##_array[__i+1]; \
		listname##_num--;					\
	}

#define ARRAYLIST_REMOVE(listname, old)					\
	{								\
		int __i;						\
		for (__i = 0; __i < listname##_num; __i++)		\
			if (listname##_array[i] == old)			\
				ARRAYLIST_REMOVE_INDEX(listname, __i);	\
	}

#define ARRAYLIST_FOREACH(listname, p, i) \
	for (i = 0, p = listname##_array[0]; i < listname##_num; i++, p = listname##_array[i])

#endif
