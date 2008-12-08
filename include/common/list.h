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

#ifndef _LIST_H_
#define _LIST_H_

struct list { void *next; };
#define LIST_DEFINE(listname) struct list listname
#define LIST_DEFINE_HEAD(listname) struct list listname##_head = { NULL }

#define LIST_INSERT(listname, new)			\
{							\
	new.listname.next = listname##_head.next;	\
	listname##_head.next = new;			\
}

#define LIST_APPEND(listname, new)			\
{							\
	typeof(new) p = listname##_head.next;		\
	if (p != NULL) {				\
		while (p->listname.next != NULL)	\
			p = p->listname.next;		\
		p->listname.next = new;			\
	} else						\
		listname##_head.next = new;		\
	new->listname.next = NULL;			\
}

#define LIST_FOREACH(listname, p) for (p = listname##_head.next; p != NULL; p = p->listname.next)

#define LIST_NEXT(listname, current) current->listname.next
#define LIST_HEAD(listname) listname##_head.next

#endif
