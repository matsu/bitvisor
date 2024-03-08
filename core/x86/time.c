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

#include <arch/time.h>
#include <builtin.h>
#include <core/arith.h>
#include <core/config.h>
#include <core/currentcpu.h>
#include <core/initfunc.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/sleep.h>
#include <core/spinlock.h>
#include <core/time.h>
#include "../vmmcall_status.h"
#include "ap.h"
#include "asm.h"
#include "constants.h"
#include "convert.h"
#include "pcpu.h"

static u64 lastcputime;
static rw_spinlock_t initsync;

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

static u64
get_cpu_time_raw (void)
{
	u32 tsc_l, tsc_h;
	u64 tsc;

	asm_rdtsc (&tsc_l, &tsc_h);
	conv32to64 (tsc_l, tsc_h, &tsc);
	return tsc;
}

u64
time_arch_get_cpu_time (void)
{
	u64 tsc, time;
	u64 lasttime = lasttime;

	atomic_cmpxchg64 (&lastcputime, &lasttime, lasttime);
	tsc = get_cpu_time_raw ();
	time = tsc_to_time (tsc - currentcpu->tsc, currentcpu->hz);
	time += currentcpu->timediff;
	if (lasttime <= time) {
		atomic_cmpxchg64 (&lastcputime, &lasttime, time);
	} else {
		currentcpu->timediff += lasttime - time;
		time = lasttime;
	}
	return time;
}

bool
time_arch_override_acpi_time (void)
{
	/* prefer invariant TSC */
	return currentcpu->use_invariant_tsc;
}

static void
time_init_dbsp (void)
{
	rw_spinlock_lock_ex (&initsync);
	sync_all_processors ();
	usleep (1000000 >> 4);
	sync_all_processors ();
	/* Update lastcputime */
	u64 time = get_cpu_time ();
	rw_spinlock_unlock_ex (&initsync);
	printf ("Time: %llu\n", time);
}

void
time_arch_init_pcpu (void)
{
	u32 a, b, c, d;
	u32 tsc1_l, tsc1_h;
	u32 tsc2_l, tsc2_h;
	u64 tsc1, tsc2, count;
	int cpu;

	cpu = currentcpu_get_id ();
	asm_cpuid (1, 0, &a, &b, &c, &d);
	if (!(d & CPUID_1_EDX_TSC_BIT))
		panic ("Processor %d does not support TSC", cpu);
	currentcpu->use_invariant_tsc = false;
	if (!config.vmm.ignore_tsc_invariant) {
		asm_cpuid (CPUID_EXT_0, 0, &a, &b, &c, &d);
		if (a >= CPUID_EXT_7) {
			asm_cpuid (CPUID_EXT_7, 0, &a, &b, &c, &d);
			if (d & CPUID_EXT_7_EDX_TSCINVARIANT_BIT)
				currentcpu->use_invariant_tsc = true;
		}
	}
	sync_all_processors ();
	asm_rdtsc (&tsc1_l, &tsc1_h);
	if (cpu == 0)
		usleep (1000000 >> 4);
	sync_all_processors ();
	asm_rdtsc (&tsc2_l, &tsc2_h);
	conv32to64 (tsc1_l, tsc1_h, &tsc1);
	conv32to64 (tsc2_l, tsc2_h, &tsc2);
	/* Set timediff value to:
	 * - Zero for all processors on BIOS environment
	 * - Zero for BSP on UEFI environment
	 * - Current time of BSP for APs on UEFI environment
	 */
	u64 lasttime = lasttime;
	rw_spinlock_lock_sh (&initsync);
	atomic_cmpxchg64 (&lastcputime, &lasttime, lasttime);
	rw_spinlock_unlock_sh (&initsync);
	count = (tsc2 - tsc1) << 4;
	printf ("Processor %d %llu Hz%s\n", cpu, count,
		currentcpu->use_invariant_tsc ? " (Invariant TSC)" : "");
	currentcpu->tsc = tsc2;
	currentcpu->hz = count;
	currentcpu->timediff = lasttime;
}

static void
time_wakeup (void)
{
	u32 tsc_l, tsc_h;

	/* Read current TSC again to keep TSC >= currentcpu->tsc */
	asm_rdtsc (&tsc_l, &tsc_h);
	conv32to64 (tsc_l, tsc_h, &currentcpu->tsc);
	/* Update timediff */
	u64 lasttime = lasttime;
	atomic_cmpxchg64 (&lastcputime, &lasttime, lasttime);
	currentcpu->timediff = lasttime;
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
		  " lastcputime: %llu\n"
		  " timediff: %llu\n"
		  , currentcpu->cpunum
		  , currentcpu->hz
		  , currentcpu->tsc
		  , lastcputime
		  , currentcpu->timediff);
	return buf;
}

static void
time_init_global_status (void)
{
	register_status_callback (time_status);
}

void
time_arch_init_global (void)
{
	lastcputime = 0;
	rw_spinlock_init (&initsync);
}

void
time_arch_record_boot_time (u64 *boot_init_time, u64 *boot_preposition_time)
{
	enum {
		SECONDS,
		MINUTES,
		HOURS,
		DAY,
		MONTH,
		YEAR,
		CENTURY,
	};
	static const u8 rtcreg[] = {
		0x00,
		0x02,
		0x04,
		0x07,
		0x08,
		0x09,
		0x32
	};
	u8 rtcdata[sizeof rtcreg];
	for (int i = 0; i < sizeof rtcreg; i++) {
		asm_outb (0x70, rtcreg[i]);
		u8 tmp;
		asm_inb (0x71, &tmp);
		if (tmp > 0x99 || (tmp & 0xF) > 9)
			tmp = 0;
		rtcdata[i] = ((tmp >> 4) * 10) + (tmp & 0xF);
	}
	*boot_preposition_time = get_time ();
	u16 year = rtcdata[YEAR];
	if (rtcdata[CENTURY] >= 19 && rtcdata[CENTURY] <= 21)
		year += rtcdata[CENTURY] * 100;
	if (year < 100)
		year += 1900;
	if (year < 1980)
		year += 100;
	u8 month = rtcdata[MONTH];
	u8 day = rtcdata[DAY];
	u8 hour = rtcdata[HOURS];
	u8 minute = rtcdata[MINUTES];
	u8 second = rtcdata[SECONDS];
	int y, m, y400r, y100r, y100q, y4r, y4q;
	long long y400q, d;
	y = year - 1900 - 100;
	m = month - 1;
	if (m >= 2) {
		m -= 2;
	} else {
		m += 10;
		y--;
	}
	y400r = y % 400;
	if (y < 0) {
		y400r += 400;
		y -= 400;
	}
	y400q = y / 400;
	y100r = y400r % 100;
	y100q = y400r / 100;
	y4r = y100r % 4;
	y4q = y100r / 4;
	d = (y400q * (365 * 400 + 97) + y100q * (365 * 100 + 24) +
	     y4q * (365 * 4 + 1) + y4r * 365 + (m * 306 + 5) / 10 +
	     day - 1 + 60 + 365 * 30 + 7);
	*boot_init_time = d * 86400 + hour * 3600 + minute * 60 + second;
}

INITFUNC ("paral01", time_init_global_status);
INITFUNC ("dbsp4", time_init_dbsp);
INITFUNC ("wakeup0", time_wakeup);
