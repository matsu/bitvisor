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

#include <arch/reboot.h>
#include <core/config.h>
#include <core/initfunc.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/process.h>
#include "acpi.h"
#include "ap.h"
#include "asm.h"
#include "callrealmode.h"
#include "panic.h"
#include "reboot.h"
#include "sleep.h"

static volatile bool rebooting = false;

void
handle_init_to_bsp (void)
{
	int d;

	if (config.vmm.auto_reboot == 1) {
		d = msgopen ("reboot");
		if (d >= 0) {
			msgsendint (d, 0);
			msgclose (d);
			printf ("Reboot failed.\n");
		} else {
			printf ("reboot not found.\n");
		}
	}
	if (config.vmm.auto_reboot)
		auto_reboot ();
	panic ("INIT signal to BSP");
}

bool
reboot_arch_rebooting (void)
{
	return rebooting;
}

void
reboot_arch_reboot (void)
{
	call_initfunc ("panic"); /* for disabling VT-x */
	acpi_reset ();
	rebooting = true;
	/*
	asm_outb (0x70, 0x0F);
	asm_outb (0x71, 0x00);
	asm_outb (0x70, 0x00);
	asm_outb (0x64, 0xFE);
	*/
	if (apic_available ()) {
		usleep (1 * 1000000);
		printf ("shutdown\n");
		asm_wridtr (0, 0);
		asm volatile ("int3");
	} else {
		printf ("call reboot\n");
		callrealmode_reboot ();
	}
	for (;;);
}
