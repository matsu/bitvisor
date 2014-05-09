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
#include "beep.h"
#include "cache.h"
#include "entry.h"
#include "initfunc.h"
#include "main.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "sleep.h"
#include "spinlock.h"
#include "string.h"
#include "wakeup_entry.h"

static unsigned int wakeup_cpucount;
static spinlock_t wakeup_cpucount_lock;
static u32 waking_vector;

static bool
get_suspend_lock_pcpu (struct pcpu *p, void *q)
{
	if (p != q)
		spinlock_lock (&p->suspend_lock);
	return false;
}

static void
get_suspend_lock (void)
{
	pcpu_list_foreach (get_suspend_lock_pcpu, currentcpu);
}

static bool
release_suspend_lock_pcpu (struct pcpu *p, void *q)
{
	if (p != q)
		spinlock_unlock (&p->suspend_lock);
	return false;
}

static void
release_suspend_lock (void)
{
	pcpu_list_foreach (release_suspend_lock_pcpu, currentcpu);
}

static void
wakeup_prepare (void)
{
	ulong tmp;

	/* All processors load the current GDTR temporarily
	   when they wake up. */
	wakeup_entry_gdtr.limit = 0xFFFF;
	tmp = (ulong)currentcpu->segdesctbl;
	wakeup_entry_gdtr.base = tmp;
	asm_rdcr4 (&tmp);
	wakeup_entry_cr4 = tmp;
	wakeup_entry_cr3 = vmm_base_cr3;
	asm_rdcr0 (&tmp);
	wakeup_entry_cr0 = tmp;
}

u32
prepare_for_sleep (u32 firmware_waking_vector)
{
	u8 *p;
	int wakeup_entry_len;

	/* Get the suspend-lock to make other processors stopping or staying
	   in the guest mode. */
	get_suspend_lock ();

	/* Now the VMM is executed by the current processor only.
	   Call suspend functions. */
	call_initfunc ("suspend");

	/* Initialize variables used by wakeup functions */
	wakeup_cpucount = 0;
	spinlock_init (&wakeup_cpucount_lock);
	waking_vector = firmware_waking_vector;
	wakeup_prepare ();

	/* Copy the wakeup_entry code. */
	wakeup_entry_len = wakeup_entry_end - wakeup_entry_start;
	p = mapmem_hphys (wakeup_entry_addr, wakeup_entry_len, MAPMEM_WRITE);
	memcpy (p, wakeup_entry_start, wakeup_entry_len);
	unmapmem (p, wakeup_entry_len);
	return wakeup_entry_addr;
}

void
cancel_sleep (void)
{
	release_suspend_lock ();
	call_initfunc ("resume");
}

asmlinkage void *
wakeup_main (void)
{
	u8 *stack;

	spinlock_lock (&wakeup_cpucount_lock);
	if (!wakeup_cpucount++)
		segment_wakeup (true);
	else
		segment_wakeup (false);
	spinlock_unlock (&wakeup_cpucount_lock);
	free (currentcpu->stackaddr);
	stack = alloc (VMM_STACKSIZE);
	currentcpu->stackaddr = stack;
	return stack + VMM_STACKSIZE;
}

static bool
wakeup_ap_loopcond (void *data)
{
	int cpucount;

	spinlock_lock (&wakeup_cpucount_lock);
	cpucount = wakeup_cpucount;
	spinlock_unlock (&wakeup_cpucount_lock);
	if (cpucount == num_of_processors + 1)
		return false;
	else
		return true;
}

static void
wakeup_ap (void)
{
	u8 buf[5];
	u8 *p;

	/* Do nothing if no APs were started before suspend.  It is
	 * true when there is only one logical processor for real or
	 * the guest OS uses BSP only on UEFI systems. */
	if (num_of_processors + 1 == 1)
		return;
	/* Put a "ljmpw" instruction to the physical address 0 to
	   avoid the alignment restriction of the SIPI. */
	p = mapmem_hphys (0, 5, MAPMEM_WRITE);
	memcpy (buf, p, 5);
	p[0] = 0xEA;		/* ljmpw */
	p[1] = wakeup_entry_addr & 0xF;
	p[2] = 0;
	p[3] = wakeup_entry_addr >> 4;
	p[4] = wakeup_entry_addr >> 12;
	ap_start_addr (0, wakeup_ap_loopcond, NULL);
	memcpy (p, buf, 5);
	unmapmem (p, 5);
}

asmlinkage void
wakeup_cont (void)
{
	static rw_spinlock_t wakeup_wait_lock;

	asm_wrcr3 (currentcpu->cr3);
	call_initfunc ("wakeup");
	if (!currentcpu->cpunum) {
		call_initfunc ("resume");
		rw_spinlock_init (&wakeup_wait_lock);
		rw_spinlock_lock_ex (&wakeup_wait_lock);
		wakeup_ap ();
		rw_spinlock_unlock_ex (&wakeup_wait_lock);
	} else {
		rw_spinlock_lock_sh (&wakeup_wait_lock);
		rw_spinlock_unlock_sh (&wakeup_wait_lock);
	}
	update_mtrr_and_pat ();
	resume_vm (waking_vector);
	panic ("resume_vm failed.");
}

void
wakeup_init (void)
{
	wakeup_entry_addr =
		alloc_realmodemem (wakeup_entry_end - wakeup_entry_start);
}
