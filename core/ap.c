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
#include "asm.h"
#include "assert.h"
#include "constants.h"
#include "entry.h"
#include "int.h"
#include "linkage.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "seg.h"
#include "sleep.h"
#include "spinlock.h"
#include "string.h"

#define APINIT_ADDR		((APINIT_SEGMENT << 4) + APINIT_OFFSET)
#define APINIT_SIZE		(cpuinit_end - cpuinit_start)
#define APINIT_POINTER(n)	((void *)(apinit + ((u8 *)&n - cpuinit_start)))
#define ICR_MODE_NMI		0x400
#define ICR_MODE_INIT		0x500
#define ICR_MODE_STARTUP	0x600
#define ICR_STATUS_BIT		0x1000
#define ICR_STATUS_IDLE		0x0000
#define ICR_STATUS_PENDING	0x1000
#define ICR_LEVEL_BIT		0x4000
#define ICR_LEVEL_DEASSERT	0x0000
#define ICR_LEVEL_ASSERT	0x4000
#define ICR_TRIGGER_BIT		0x8000
#define ICR_TRIGGER_EDGE	0x0000
#define ICR_TRIGGER_LEVEL	0x8000
#define ICR_DEST_OTHER		0xC0000

static void ap_start (void);

static volatile int num_of_processors; /* number of application processors  */
static void (*initproc_bsp) (void), (*initproc_ap) (void);
static spinlock_t ap_lock;
static void *newstack_tmp;
static spinlock_t sync_lock;
static u32 sync_id;
static volatile u32 sync_count;
static spinlock_t *apinitlock;

/* this function is called after starting AP and switching a stack */
/* unlock the spinlock because the stack is switched */
static asmlinkage void
apinitproc1 (void)
{
	void (*proc) (void);
	int n;
	void *newstack;

	newstack = newstack_tmp;
	spinlock_unlock (apinitlock);
	spinlock_lock (&ap_lock);
	num_of_processors++;
	n = num_of_processors;
	proc = initproc_ap;
	printf ("Processor %d (AP)\n", num_of_processors);
	spinlock_unlock (&ap_lock);
	segment_init_ap (n);
	currentcpu->stackaddr = newstack;
	int_init_ap ();
	proc ();
}

static asmlinkage void
bspinitproc1 (void)
{
	ap_start ();
	initproc_bsp ();
	panic ("bspinitproc1");
}

/* this function is called on AP by entry.s */
/* other APs are waiting for spinlock */
/* the stack is cpuinit_tmpstack defined in entry.s */
/* this function allocates a stack and calls apinitproc1 with the new stack */
asmlinkage void
apinitproc0 (void)
{
	void *newstack;

	alloc_pages (&newstack, NULL, VMM_STACKSIZE / PAGESIZE);
	newstack_tmp = newstack;
	asm_wrrsp_and_jmp ((ulong)newstack + VMM_STACKSIZE, apinitproc1);
}

static bool
apic_available (void)
{
	u32 a, b, c, d;
	u64 tmp;

	asm_cpuid (1, 0, &a, &b, &c, &d);
	if (!(d & CPUID_1_EDX_APIC_BIT))
		return false;
	asm_rdmsr64 (MSR_IA32_APIC_BASE_MSR, &tmp);
	if (!(tmp & MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT))
		return false;
	return true;
}

static void
apic_wait_for_idle (volatile u32 *apic_icr)
{
	while ((*apic_icr & ICR_STATUS_BIT) != ICR_STATUS_IDLE);
}

static void
apic_assert_init (volatile u32 *apic_icr)
{
	apic_wait_for_idle (apic_icr);
	*apic_icr = ICR_DEST_OTHER | ICR_TRIGGER_LEVEL | ICR_MODE_INIT |
		ICR_LEVEL_ASSERT;
}

static void
apic_deassert_init (volatile u32 *apic_icr)
{
	apic_wait_for_idle (apic_icr);
	*apic_icr = ICR_DEST_OTHER | ICR_TRIGGER_LEVEL | ICR_MODE_INIT;
}

static void
apic_send_startup_ipi (volatile u32 *apic_icr)
{
	apic_wait_for_idle (apic_icr);
	*apic_icr = ICR_DEST_OTHER | ICR_LEVEL_ASSERT | ICR_MODE_STARTUP |
		(APINIT_ADDR >> 12);
}

static void
ap_start (void)
{
	const u32 apic_icr_phys = 0xFEE00300;
	volatile u32 *apic_icr, *num;
	u8 *apinit;
	u32 tmp;
	int i;

	printf ("Processor 0 (BSP)\n");
	spinlock_init (&sync_lock);
	sync_id = 0;
	sync_count = 0;
	num_of_processors = 0;
	spinlock_init (&ap_lock);
	apinit = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, APINIT_ADDR,
			 APINIT_SIZE);
	ASSERT (apinit);
	memcpy (apinit, cpuinit_start, APINIT_SIZE);
	num = (volatile u32 *)APINIT_POINTER (apinit_procs);
	apinitlock = (spinlock_t *)APINIT_POINTER (apinit_lock);
	*num = 0;
	spinlock_init (apinitlock);
	apic_icr = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE | MAPMEM_PWT |
			   MAPMEM_PCD, apic_icr_phys, sizeof *apic_icr);
	ASSERT (apic_icr);
	if (apic_available ()) {
		apic_assert_init (apic_icr);
		usleep (200000);
		apic_deassert_init (apic_icr);
		usleep (200000);
		for (i = 0; i < 3; i++) {
			apic_send_startup_ipi (apic_icr);
			usleep (200000);
		}
		for (;;) {
			spinlock_lock (&ap_lock);
			tmp = num_of_processors;
			spinlock_unlock (&ap_lock);
			if (*num == tmp)
				break;
			usleep (1000000);
		}
	}
	unmapmem ((void *)apic_icr, sizeof *apic_icr);
	unmapmem ((void *)apinit, APINIT_SIZE);
}

static void
bsp_continue (asmlinkage void (*initproc_arg) (void))
{
	void *newstack;

	alloc_pages (&newstack, NULL, VMM_STACKSIZE / PAGESIZE);
	currentcpu->stackaddr = newstack;
	asm_wrrsp_and_jmp ((ulong)newstack + VMM_STACKSIZE, initproc_arg);
}

void
sync_all_processors (void)
{
	u32 id = 0;
	bool ret = false;

	spinlock_lock (&sync_lock);
	asm_lock_cmpxchgl (&sync_id, &id, id);
	sync_count++;
	if (sync_count == num_of_processors + 1) {
		sync_count = 0;
		asm_lock_incl (&sync_id);
		ret = true;
	}
	spinlock_unlock (&sync_lock);
	while (!ret) {
		ret = asm_lock_cmpxchgl (&sync_id, &id, id);
		asm_pause ();
	}
}

void
start_all_processors (void (*bsp_initproc) (void), void (*ap_initproc) (void))
{
	initproc_bsp = bsp_initproc;
	initproc_ap = ap_initproc;
	bsp_continue (bspinitproc1);
}
