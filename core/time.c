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
#include <core/printf.h>
#include <core/process.h>
#include <core/sleep.h>
#include <core/time.h>
#include "acpi.h"
#include "calluefi.h"
#include "comphappy.h"
#include "time.h"
#include "uefi.h"

static u64 boot_init_time;
static u64 boot_preposition_time;
static u64 lastacpitime;

u64
get_cpu_time (void)
{
	return time_arch_get_cpu_time ();
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
	if (!time_arch_override_acpi_time ())
		ok = get_acpi_time (&ret);
#endif
	if (!ok)
		ret = get_cpu_time ();
	return ret;
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
	time_arch_init_pcpu ();
	if (uefi_booted && currentcpu_get_id () == 0)
		/* Using UEFI runtime syscall to store
		 * the boot time in EPOCH format */
		time_record_boot_time_uefi ();

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

static void
time_init_global (void)
{
	lastacpitime = 0;
	time_arch_init_global ();
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
INITFUNC ("pcpu5", time_init_pcpu);
INITFUNC ("msg0", time_init_msg);
INITFUNC ("msg1", epochtime_init_msg);
