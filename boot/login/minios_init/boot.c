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

#define _XOPEN_SOURCE 500
#define _BSD_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include <fcntl.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "boot.h"

#define KEXEC_ARCH_386 (3 << 16)

extern char prog[], progend[];
extern unsigned int eax;

struct kexecseg boot = {
	/* 0x00100000: mov $0, %eax */
	0xB8, 0,
	/* 0x00100005: mov $0x00100010, %ebx */
	0xBB, 0x00100010,
	/* 0x0010000A: nop; nop; nop */
	{
		0x90, 0x90, 0x90,
	},
	/* 0x0010000D: int3 */
	0xCC,
	/* 0x0010000E: jmp $ */
	{
		0xEB, 0xFE,
	},
	/* 0x00100010- config */
};

struct kexec_segment {
	void *buf;
	size_t bufsz;
	unsigned long mem;
	size_t memsz;
};

struct config_data config;

static volatile int sigillcount;

void
boot_guest_cont (u8 *vmmcall, int n)
{
	int r;
	struct kexec_segment s;

	config.len = sizeof config;
	memcpy (&boot.config, &config, sizeof config);
	boot.vmmcall[0] = vmmcall[0];
	boot.vmmcall[1] = vmmcall[1];
	boot.vmmcall[2] = vmmcall[2];
	boot.eax = n;
	s.buf = &boot;
	s.bufsz = sizeof boot & ~4095;
	s.mem = 0x100000;
	s.memsz = s.bufsz;
	r = syscall (__NR_kexec_load, 0x100000, 1, &s, KEXEC_ARCH_386);
	if (r) {
		perror ("kexec_load");
		return;
	}
	r = syscall (__NR_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2C,
		     LINUX_REBOOT_CMD_KEXEC, NULL);
	perror ("reboot");
}

static void
vmmcall_error (void)
{
	printf ("vmmcall failed\n");
	exit (1);
}

static void
sigill (int sig)
{
	int n;
	u8 vmmcall[] = {
		0x0F, 0x01, 0xD9,
	};

	printf ("exception\n");
	if (sigillcount++ == 0) {
		printf ("trying vmmcall\n");
		asm volatile ("vmmcall" : "=a" (n) : "a" (0), "b" ("boot"));
		printf ("good. n=%d\n", n);
		if (n)
			boot_guest_cont (vmmcall, n);
	}
	vmmcall_error ();
}

void
boot_guest (void)
{
	int n;
	u8 vmcall[] = {
		0x0F, 0x01, 0xC1,
	};

	sigillcount = 0;
	signal (SIGILL, sigill);
	printf ("trying vmcall\n");
	asm volatile ("vmcall" : "=a" (n) : "a" (0), "b" ("boot"));
	printf ("good. n=%d\n", n);
	if (n)
		boot_guest_cont (vmcall, n);
	vmmcall_error ();
}
