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

#include "ap.h"
#include "arith.h"
#include "asm.h"
#include "constants.h"
#include "convert.h"
#include "initfunc.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h"
#include "sleep.h"
#include "spinlock.h"
#include "time.h"
#include "vmmcall_status.h"

static u64 volatile lasttime;
static spinlock_t time_lock;

static u64
tsc_to_time (u64 tsc, u64 hz)
{
	u64 tmp[2];

	while (hz > 0xFFFFFFFFULL) {
		tsc >>= 1;
		hz >>= 1;
	}
	mpumul_64_64 (tsc, 1000000ULL, tmp); /* tmp = tsc * 1000000 */
	mpudiv_128_32 (tmp, (u32)hz, tmp); /* tmp = tmp / hz */
	return tmp[0];
}

u64
get_cpu_time (void)
{
	u32 tsc_l, tsc_h;
	u64 tsc, time;

	asm_rdtsc (&tsc_l, &tsc_h);
	conv32to64 (tsc_l, tsc_h, &tsc);
	time = tsc_to_time (tsc - currentcpu->tsc, currentcpu->hz);
	time += currentcpu->timediff;
	spinlock_lock (&time_lock);
	if (lasttime <= time) {
		lasttime = time;
	} else {
		currentcpu->timediff += lasttime - time;
		time = lasttime;
	}
	spinlock_unlock (&time_lock);
	return time;
}

static void
time_init_pcpu (void)
{
	u32 a, b, c, d;
	u32 tsc1_l, tsc1_h;
	u32 tsc2_l, tsc2_h;
	u64 tsc1, tsc2, count;

	asm_cpuid (1, 0, &a, &b, &c, &d);
	if (!(d & CPUID_1_EDX_TSC_BIT))
		panic ("Processor %d does not support TSC",
		       currentcpu->cpunum);
	sync_all_processors ();
	asm_rdtsc (&tsc1_l, &tsc1_h);
	if (currentcpu->cpunum == 0)
		usleep (1000000);
	sync_all_processors ();
	asm_rdtsc (&tsc2_l, &tsc2_h);
	conv32to64 (tsc1_l, tsc1_h, &tsc1);
	conv32to64 (tsc2_l, tsc2_h, &tsc2);
	count = tsc2 - tsc1;
	printf ("Processor %d %llu Hz\n", currentcpu->cpunum, count);
	currentcpu->tsc = tsc1;
	currentcpu->hz = count;
	currentcpu->timediff = 0;
}

static int
time_msghandler (int m, int c)
{
	if (m == 0)
		printf ("time %llu\n", get_cpu_time ());
	return 0;
}

static void
time_init_msg (void)
{
	if (false)		/* DEBUG */
		msgregister ("time", time_msghandler);
}

static char *
time_status (void)
{
	static char buf[1024];

	snprintf (buf, 1024,
		  "time:\n"
		  " cpu: %d\n"
		  " hz: %llu\n"
		  " tsc: %llu\n"
		  " lasttime: %llu\n"
		  " timediff: %llu\n"
		  , currentcpu->cpunum
		  , currentcpu->hz
		  , currentcpu->tsc
		  , lasttime
		  , currentcpu->timediff);
	return buf;
}

static void
time_init_global_status (void)
{
	register_status_callback (time_status);
}

static void
time_init_global (void)
{
	spinlock_init (&time_lock);
	lasttime = 0;
}

INITFUNC ("global3", time_init_global);
INITFUNC ("global4", time_init_global_status);
INITFUNC ("pcpu3", time_init_pcpu);
INITFUNC ("msg0", time_init_msg);
