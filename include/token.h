/*
 * Copyright (c) 2010 Igel Co., Ltd
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

#ifndef _TOKEN_H
#define _TOKEN_H

struct token {
	char *start, *end, *next;
};

static char
get_token2 (char *p, struct token *t)
{
	int f = 0;

	while (*p == ' ' || *p == '\t')
		p++;
	t->start = p;
	t->end = p;		/* length 0 */
	for (;;) {
		switch (*p) {
		case '\\':
			if (*++p == '\0')
				goto err;
			goto notspc;
		case '"':
			f = !f;
			goto notspc;
		case '\0':
			if (!f) {
				t->next = p;
				break;
			}
			/* fall through */
		err:
			t->start = NULL;
			t->next = p;
			break;
		case '=':
		case ',':
		case '|':
			if (!f) {
				t->next = p + 1;
				break;
			}
			/* fall through */
		default:
		notspc:
			t->end = p + 1;
			/* fall through */
		case ' ':
			p++;
			continue;
		}
		return *p;
	}
}

static char
get_token1 (char *p, struct token *t)
{
	struct token tmp;
	char c;

	if (!p)
		p = "";
	c = get_token2 (p, &tmp);
	t->start = tmp.start;
	t->end = tmp.end;
	t->next = tmp.next;
	while (c == '|') {
		c = get_token2 (tmp.next, &tmp);
		t->end = tmp.end;
		t->next = tmp.next;
	}
	if (!tmp.start)
		t->start = NULL;
	return c;
}

static inline char
get_token (char *p, struct token *t)
{
	return get_token1 (p, t);
}

static int
match_token2 (char *str, char *p, char *end)
{
	for (;;) {
		if (p == end) {
			if (*str == '\0')
				return 1;
			return 0;
		}
		switch (*p) {
		case '"':
			break;
		case '*':
			while (*p == '*' && p != end)
				p++;
			while (*str != '\0') {
				if (match_token2 (str, p, end))
					return 1;
				str++;
			}
			continue;
		case '?':
			if (*str == '\0')
				return 0;
			str++;
			break;
		case '\0':
			/* this must not be executed */
			return 0;
		case '\\':
			p++;
			if (*p == '\0' || p == end) {
				/* this must not be executed */
				return 0;
			}
			/* fall through */
		default:
			if (*str == '\0' || *str != *p)
				return 0;
			str++;
			break;
		}
		p++;
	}
}

static int
match_token1 (char *str, struct token *t)
{
	struct token tmp;
	int len;
	char c;

	if (str == NULL)
		str = "";
	len = t->end - t->start;
	tmp.next = t->start;
	while (tmp.next - t->start < len) {
		c = get_token2 (tmp.next, &tmp);
		if (match_token2 (str, tmp.start, tmp.end))
			return 1;
		if (c != '|')
			break;
	}
	return 0;
}

static inline int
match_token (char *str, struct token *t)
{
	return match_token1 (str, t);
}

#endif
