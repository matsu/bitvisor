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

#include "lib_syscalls.h"

typedef unsigned long ulong;

#define CALLADDR		0x3FFFF000
#define SYS_NOP			1
#define SYS_MSGSETFUNC		3
#define SYS_MSGREGISTER		4
#define SYS_MSGOPEN		5
#define SYS_MSGCLOSE		6
#define SYS_MSGSENDINT		7
#define SYS_MSGRET		8
#define SYS_MSGSENDDESC		9
#define SYS_NEWPROCESS		10
#define SYS_MSGSENDBUF		11
#define SYS_MSGUNREGISTER	12
#define SYS_EXITPROCESS		13
#define SYS_SETLIMIT		14

#ifdef __x86_64__
#	define DOSYSCALL0(rb, ra) asm volatile \
		("syscall" : "=a" (ra) \
			    : "b" ((ulong)rb) \
			    : "memory", "cc", "%rcx", "%rdx" \
			    , "%r8", "%r9", "%r10", "%r11", "%r12", "%r13" \
			    , "%r14", "%r15")
#	define DOSYSCALL1(rb, rs, ra) asm volatile \
		("syscall" : "=a" (ra) \
			    : "b" ((ulong)rb) \
			    , "S" ((ulong)rs) \
			    : "memory", "cc", "%rcx", "%rdx" \
			    , "%r8", "%r9", "%r10", "%r11", "%r12", "%r13" \
			    , "%r14", "%r15")
#	define DOSYSCALL2(rb, rs, rd, ra) asm volatile \
		("syscall" : "=a" (ra) \
			    : "b" ((ulong)rb) \
			    , "S" ((ulong)rs), "D" ((ulong)rd) \
			    : "memory", "cc", "%rcx", "%rdx" \
			    , "%r8", "%r9", "%r10", "%r11", "%r12", "%r13" \
			    , "%r14", "%r15")
#else
#	define DOSYSCALL0(rb, ra) asm volatile \
		("call *%1" : "=a" (ra) \
			    : "0" (CALLADDR), "b" (rb) \
			    : "memory", "cc", "%ecx", "%edx")
#	define DOSYSCALL1(rb, rs, ra) asm volatile \
		("call *%1" : "=a" (ra) \
			    : "0" (CALLADDR), "b" (rb), "S" (rs) \
			    : "memory", "cc", "%ecx", "%edx")
#	define DOSYSCALL2(rb, rs, rd, ra) asm volatile \
		("call *%1" : "=a" (ra) \
			    : "0" (CALLADDR), "b" (rb), "S" (rs), "D" (rd) \
			    : "memory", "cc", "%ecx", "%edx")
#endif

void
nop (void)
{
	ulong tmp;

	DOSYSCALL0 (SYS_NOP, tmp);
}

void *
msgsetfunc (int desc, void *func)
{
	ulong tmp;

	DOSYSCALL2 (SYS_MSGSETFUNC, desc, func, tmp);
	return (void *)tmp;
}

int
msgregister (char *name, void *func)
{
	ulong tmp;

	DOSYSCALL2 (SYS_MSGREGISTER, name, func, tmp);
	return (int)tmp;
}

int
msgopen (char *name)
{
	ulong tmp;

	DOSYSCALL1 (SYS_MSGOPEN, name, tmp);
	return (int)tmp;
}

int
msgclose (int desc)
{
	ulong tmp;

	DOSYSCALL1 (SYS_MSGCLOSE, desc, tmp);
	return (int)tmp;
}

int
msgsendint (int desc, int data)
{
	ulong tmp;

	DOSYSCALL2 (SYS_MSGSENDINT, desc, data, tmp);
	return (int)tmp;
}

int
msgsenddesc (int desc, int data)
{
	ulong tmp;

	DOSYSCALL2 (SYS_MSGSENDDESC, desc, data, tmp);
	return (int)tmp;
}

int
newprocess (char *name)
{
	ulong tmp;

	DOSYSCALL1 (SYS_NEWPROCESS, name, tmp);
	return (int)tmp;
}

int
msgsendbuf (int desc, int data, struct msgbuf *buf, int bufcnt)
{
	ulong tmp;
	struct msgsendbuf_args {
		int data;
		int bufcnt;
		struct msgbuf *buf;
	} a;

	a.data = data;
	a.buf = buf;
	a.bufcnt = bufcnt;
	DOSYSCALL2 (SYS_MSGSENDBUF, desc, &a, tmp);
	return (int)tmp;
}

int
msgunregister (int desc)
{
	ulong tmp;

	DOSYSCALL1 (SYS_MSGUNREGISTER, desc, tmp);
	return (int)tmp;
}

void
exitprocess (int retval)
{
	ulong tmp;

	DOSYSCALL1 (SYS_EXITPROCESS, retval, tmp);
}

int
setlimit (int stacksize, int maxstacksize)
{
	ulong tmp;

	DOSYSCALL2 (SYS_SETLIMIT, stacksize, maxstacksize, tmp);
	return (int)tmp;
}
