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

#include <core/initfunc.h>
#include <core/io.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include <core/time.h>
#include "acpi.h"
#include "acpi_constants.h"
#include "assert.h"
#include "constants.h"
#include "current.h"
#include "mm.h"
#include "wakeup.h"

static bool
acpi_pm1_sleep (u32 v)
{
	u32 new_waking_vector;
	u32 old_waking_vector;
	bool error;

	if (!acpi_dsdt_pm1_cnt_slp_typx_check (v))
		return false;
	ASSERT (acpi_get_waking_vector (&old_waking_vector, &error));
	if (error) /* In case of FACS error */
		return true;
	new_waking_vector = prepare_for_sleep (old_waking_vector);
	acpi_set_waking_vector (new_waking_vector, old_waking_vector);
	get_cpu_time ();	/* update lastcputime */
	/* Flush all write back caches including the internal caches
	   on the other processors, or the processors will lose them
	   and the VMM will not work correctly. */
	mm_flush_wb_cache ();
	acpi_reg_pm1a_cnt_write (v);
	cancel_sleep ();
	return true;
}

static enum ioact
acpi_io_monitor (enum iotype type, u32 port, void *data)
{
	u32 v;
	u32 pm1a_cnt_ioaddr;

	if (acpi_reg_get_pm1a_cnt_ioaddr (&pm1a_cnt_ioaddr) &&
	    port == pm1a_cnt_ioaddr) {
		switch (type) {
		case IOTYPE_OUTB:
			v = *(u8 *)data;
			break;
		case IOTYPE_OUTW:
			v = *(u16 *)data;
			break;
		case IOTYPE_OUTL:
			v = *(u32 *)data;
			break;
		default:
			goto def;
		}
		if (v & ACPI_PM1_CNT_SLP_EN_BIT)
			if (acpi_pm1_sleep (v))
				return IOACT_CONT;
		goto def;
	}
def:
	return do_io_default (type, port, data);
}

static enum ioact
acpi_smi_monitor (enum iotype type, u32 port, void *data)
{
	u32 smi_cmd;

	if (current->acpi.smi_hook_disabled)
		panic ("SMI monitor called while SMI hook is disabled");
	current->vmctl.paging_map_1mb ();
	ASSERT (acpi_reg_get_smi_cmd_ioaddr (&smi_cmd));
	current->vmctl.iopass (smi_cmd, true);
	current->acpi.smi_hook_disabled = true;
	return IOACT_RERUN;
}

void
acpi_smi_hook (void)
{
	u32 smi_cmd;

	if (!current->vcpu0->acpi.iopass)
		return;
	if (current->acpi.smi_hook_disabled) {
		ASSERT (acpi_reg_get_smi_cmd_ioaddr (&smi_cmd));
		current->vmctl.iopass (smi_cmd, false);
		current->acpi.smi_hook_disabled = false;
	}
}

static void
acpi_iohook (void)
{
	u32 pm1a_cnt_ioaddr, smi_cmd;

	if (acpi_reg_get_pm1a_cnt_ioaddr (&pm1a_cnt_ioaddr))
		set_iofunc (pm1a_cnt_ioaddr, acpi_io_monitor);
	if (acpi_reg_get_smi_cmd_ioaddr (&smi_cmd)) {
		current->vcpu0->acpi.iopass = true;
		set_iofunc (smi_cmd, acpi_smi_monitor);
	}
}

INITFUNC ("iohook1", acpi_iohook);
