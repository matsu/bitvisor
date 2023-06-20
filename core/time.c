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

#include <builtin.h>
#include <core/currentcpu.h>
#include "acpi.h"
#include "ap.h"
#include "arith.h"
#include "asm.h"
#include "calluefi.h"
#include "comphappy.h"
#include "config.h"
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
#include "uefi.h"
#include "vmmcall_status.h"

static u64 boot_init_time;
static u64 boot_preposition_time;
static u64 lastcputime;
static u64 lastacpitime;
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
get_cpu_time (void)
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
get_acpi_time (u64 *r)
{
	u32 tmr, oldtmr;
	u64 now, tmp[2], oldnow;

	VAR_IS_INITIALIZED (oldnow);
	atomic_cmpxchg64 (&lastacpitime, &oldnow, oldnow);
	if (!get_acpi_time_raw (&tmr))
		return false;
	oldtmr = oldnow & 16777215;
	now = oldnow;
	if (tmr != oldtmr) {
		if (tmr < oldtmr)
			tmr += 16777216;
		now += tmr - oldtmr;
		atomic_cmpxchg64 (&lastacpitime, &oldnow, now);
	}
	mpumul_64_64 (now, 1000000ULL, tmp); /* tmp = now * 1000000 */
	mpudiv_128_32 (tmp, 3579545U, tmp); /* tmp = tmp / 3579545 */
	*r = tmp[0];
	return true;
}

u64
get_time (void)
{
	u64 ret;
	bool ok;

	ok = false;
#ifdef ACPI_TIME_SOURCE
	/* prefer invariant TSC */
	if (!currentcpu->use_invariant_tsc)
		ok = get_acpi_time (&ret);
#endif
	if (!ok)
		ret = get_cpu_time ();
	return ret;
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

static void
time_record_boot_time_uefi (void)
{
	u16 year;
	u8 month;
	u8 day;
	u8 hour;
	u8 minute;
	u8 second;
	int y, m, y400r, y100r, y100q, y4r, y4q, ret;
	long long y400q, d;

	boot_preposition_time = get_time ();
	ret = call_uefi_get_time (&year, &month, &day, &hour, &minute,
				  &second);
	if (!ret) {
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
		boot_init_time = d * 86400 + hour * 3600 + minute * 60 +
				 second;
	}
}

static void
time_init_pcpu (void)
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
	if (uefi_booted && cpu == 0)
		/* Using UEFI runtime syscall to store
		 * the boot time in EPOCH format */
		time_record_boot_time_uefi ();

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

static int
time_msghandler (int m, int c)
{
	if (m == 0)
		printf ("time %llu\n", get_time ());
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

static void
time_init_global (void)
{
	lastcputime = 0;
	lastacpitime = 0;
	rw_spinlock_init (&initsync);
}

void
get_epoch_time (long long *second, int *microsecond)
{
	u64 time[2] = { get_time () - boot_preposition_time, 0 };
	u64 sec[2];
	*microsecond = mpudiv_128_32 (time, 1000000, sec);
	*second = boot_init_time + sec[0];
}

static int
epochtime_msghandler (int m, int c, struct msgbuf *buf, int bufcnt)
{
	int microsecond_value;
	int *microsecond_ptr;
	long long *second_ptr;

	if (m != MSG_BUF || (bufcnt != 1 && bufcnt != 2))
		return -1;
	if (buf[0].len != sizeof (long long))
		return -1;
	second_ptr = buf[0].base;
	if (bufcnt == 2) {
		if (buf[1].len != sizeof (int))
			return -1;
		microsecond_ptr = buf[1].base;
	} else {
		microsecond_ptr = &microsecond_value;
	}
	get_epoch_time (second_ptr, microsecond_ptr);
	return 0;
}

static void
epochtime_init_msg (void)
{
	msgregister ("epochtime", epochtime_msghandler);
}

INITFUNC ("global3", time_init_global);
INITFUNC ("paral01", time_init_global_status);
INITFUNC ("pcpu4", time_init_pcpu);
INITFUNC ("dbsp4", time_init_dbsp);
INITFUNC ("msg0", time_init_msg);
INITFUNC ("wakeup0", time_wakeup);
INITFUNC ("msg1", epochtime_init_msg);
