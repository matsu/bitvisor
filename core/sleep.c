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

#include "asm.h"
#include "constants.h"
#include "initfunc.h"
#include "process.h"
#include "sleep.h"
#include "time.h"
#include "uefi.h"

void
waitcycles (u32 d, u32 a)
{
	u32 a0, d0, aa, dd, a1, d1;

	asm_rdtsc (&a0, &d0);
	a1 = a0 + a;
	d1 = d0 + d + (a1 < a0);
	do
		asm_rdtsc (&aa, &dd);
	while (dd < d1 || (dd == d1 && aa < a1));
}

/* read count of timer 0 */
static u16
get_timer0 (void)
{
	u16 count;
	u8 l, h;

	asm_outb (PIT_CONTROL,
		  PIT_CONTROL_BINARY |
		  PIT_CONTROL_MODE2 |
		  PIT_CONTROL_LATCH |
		  PIT_CONTROL_COUNTER0);
	asm_inb (PIT_COUNTER0, &l);
	asm_inb (PIT_COUNTER0, &h);
	count = l | (h << 8);
	return count;
}

/* busyloop for usec microseconds */
void
usleep (u32 usec)
{
	u32 count, remainder, counter0, tmp, diff;
	u64 acpitime, acpitime2;

	if (get_acpi_time (&acpitime)) {
		while (get_acpi_time (&acpitime2))
			if (acpitime2 - acpitime >= usec)
				return;
	}
	while (usec > 3599591843U) {
		usleep (3599591843U);
		usec -= 3599591843U;
	}
	asm_mul_and_div (14318181, usec, 12000000, &count, &remainder);
	if (remainder >= 12000000 / 2)
		count++;
	counter0 = get_timer0 ();
	while (count) {
		do
			tmp = get_timer0 ();
		while (counter0 == tmp);
		if (counter0 > tmp) {
			diff = counter0 - tmp;
			if (count < tmp)
				diff = count;
		} else {
			diff = 1;
		}
		count -= diff;
		counter0 = tmp;
	}
}

void
sleep_set_timer_counter (void)
{
	/* set PIT counter 0 to 65536 */
	asm_outb (PIT_CONTROL,
		  PIT_CONTROL_BINARY |
		  PIT_CONTROL_MODE2 |
		  PIT_CONTROL_16BIT |
		  PIT_CONTROL_COUNTER0);
	asm_outb (PIT_COUNTER0, 0);
	asm_outb (PIT_COUNTER0, 0);
}

static int
usleep_msghandler (int m, int c)
{
	if (m == MSG_INT) {
		usleep (c);
		return 0;
	}
	return -1;
}

static void
sleep_init_global (void)
{
	if (!uefi_booted)
		sleep_set_timer_counter ();
}

static void
sleep_init_msg (void)
{
	msgregister ("usleep", usleep_msghandler);
}

INITFUNC ("global0", sleep_init_global);
INITFUNC ("msg0", sleep_init_msg);
