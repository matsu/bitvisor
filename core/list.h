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

#ifndef _CORE_LIST_H
#define _CORE_LIST_H

#include "types.h"

/* usage:
   struct foo {
     LIST1_DEFINE (struct foo);
     int bar;
   };
   LIST1_DEFINE_HEAD (struct foo, list1_foo);
   func () {
     struct foo f[3], *p;
     LIST1_HEAD_INIT (list1_foo);
     LIST1_PUSH (list1_foo, &f[0]);
     LIST1_PUSH (list1_foo, &f[1]);
     p = LIST1_POP (list1_foo);
     LIST1_DEL (list1_foo, &f[0]);
   }
*/

#define LIST1_DEFINE(type) type **pnext, *next
#define LIST1_DEFINE_HEAD(type, name) struct { type **pnext, *next; } name
#define LIST1_DEFINE_HEAD_INIT(type, name) \
	struct { type **pnext, *next; } name = { &name.next, NULL }
#define LIST1_HEAD_INIT(name) *(name.pnext = &name.next) = NULL
#define LIST1_NEXTPNEXT(head, data) \
	((data)->next ? &(data)->next->pnext : &(head).pnext)
#define LIST1_ADD(head, data) \
	list1_insert ((head).pnext, (data), &(data)->pnext, &(data)->next, \
		      NULL, &(head).pnext)
#define LIST1_PUSH(head, data) \
	list1_insert (&(head).next, (data), &(data)->pnext, &(data)->next, \
		      (head).next, LIST1_NEXTPNEXT (head, &head))
#define LIST1_DEL(head, data) \
	list1_delete ((data)->pnext, (data)->next, \
		      LIST1_NEXTPNEXT (head, data), (data))
#define LIST1_POP(head) \
	((head).next ? LIST1_DEL (head, (head).next) : NULL)
#define LIST1_FOREACH(head, var) \
	for (var = (head).next; var; var = var->next)
#define LIST1_INSERT(head, nextd, data) \
	((nextd) ? list1_insert ((nextd)->pnext, (data), &(data)->pnext, \
				 &(data)->next, (nextd), &(nextd)->pnext) \
		 : LIST1_ADD ((head), (data)))

/* p{p_pn,p_n} -> n{n_pn,n_n} */
/* p{p_pn,p_n} -> d{d_pn,d_n} -> n{n_pn,n_n} */
static inline void
list1_insert (void *p_n2, void *d, void *d_pn2, void *d_n2, void *n,
	      void *n_pn2)
{
	void **p_n = p_n2, ***d_pn = d_pn2, **d_n = d_n2, ***n_pn = n_pn2;

	*p_n = d;
	*d_pn = p_n;
	*d_n = n;
	*n_pn = d_n;
}

/* p{p_pn,p_n=*d_pn} -> d{d_pn,d_n} -> n=d_n{n_pn=&d_n,n_n} */
/* p{p_pn,p_n=n} -> n{n_pn=&p_n,n_n} */
static inline void *
list1_delete (void *p_n2, void *n, void *n_pn2, void *d)
{
	void **p_n = p_n2, ***n_pn = n_pn2;

	*p_n = n;
	*n_pn = p_n;
	return d;
}

#endif
