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

/* Note that we follow Linux system call convention */

#define DOSYSCALL0(rb, ra) \
	({ \
		register ulong ret asm ("x0"); \
		register ulong syscall_num asm ("x8"); \
		/* Avoid unused variable warning for some functions */ \
		(void)(ra); \
		syscall_num = (ulong)(rb); \
		asm volatile ("svc 0" \
			      : "=r" (ret) \
			      : "r" (syscall_num) \
			      : "memory", "cc"); \
		ra = ret; \
	})

#define DOSYSCALL1(rb, rs, ra) \
	({ \
		register ulong ret asm ("x0"); \
		register ulong arg0 asm ("x0"); \
		register ulong syscall_num asm ("x8"); \
		(void)(ra); \
		arg0 = (ulong)(rs); \
		syscall_num = (rb); \
		asm volatile ("svc 0" \
			      : "=r" (ret) \
			      : "r" (arg0), "r" (syscall_num) \
			      : "memory", "cc"); \
		ra = ret; \
	})

#define DOSYSCALL2(rb, rs, rd, ra) \
	({ \
		register ulong ret asm ("x0"); \
		register ulong arg0 asm ("x0"); \
		register ulong arg1 asm ("x1"); \
		register ulong syscall_num asm ("x8"); \
		(void)(ra); \
		arg0 = (ulong)(rs); \
		arg1 = (ulong)(rd); \
		syscall_num = (ulong)(rb); \
		asm volatile ("svc 0" \
			      : "=r" (ret) \
			      : "r" (arg0), "r" (arg1), "r" (syscall_num) \
			      : "memory", "cc"); \
		ra = ret; \
	})
