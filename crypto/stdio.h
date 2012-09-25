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

#define EOF (-1)
#define stdin NULL

static inline char *
fgets (char *s, int size, FILE *fp)
{
	return NULL;
}

static inline int
fputs (const char *s, FILE *fp)
{
	return EOF;
}

static inline int
fflush (FILE *s)
{
	return EOF;
}

static inline int
fprintf (FILE *fp, const char *format, ...)
{
	return -1;
}

static inline int
feof (FILE *fp)
{
	return -1;
}

static inline int
ferror (FILE *fp)
{
	return -1;
}

static inline FILE *
fopen (const char *path, const char *mode)
{
	return NULL;
}

static inline int
fileno (FILE *fp)
{
	return -1;
}

static inline int
fclose (FILE *fp)
{
	return EOF;
}

static inline int
sscanf (const char *str, const char *format, ...)
{
	return EOF;
}

static inline size_t
fwrite (const void *buf, size_t size, size_t num, FILE *fp)
{
	return 0;
}
