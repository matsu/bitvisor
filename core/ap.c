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
#include "localapic.h"
#include "mm.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "seg.h"
#include "sleep.h"
#include "spinlock.h"
#include "string.h"
#include "thread.h"
#include "uefi.h"

#define APINIT_SIZE		(cpuinit_end - cpuinit_start)
#define APINIT_POINTER(n)	((void *)(apinit + ((u8 *)&n - cpuinit_start)))
#define ICR_MODE_MASK		0x700
#define ICR_MODE_FIXED		0x000
#define ICR_MODE_LOWEST_PRIORITY 0x100
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
#define ICR_DEST_ALL		0x80000
#define ICR_DEST_SELF		0x40000
#define SVR_APIC_ENABLED	0x100

struct local_apic_registers {
	u32 reserved00[4];	/* 0x000 */
	u32 reserved01[4];	/* 0x010 */
	u32 local_apic_id;	/* 0x020 */
	u32 reserved02[3];
	u32 local_apic_version; /* 0x030 */
	u32 reserved03[3];
	u32 reserved04[4];	/* 0x040 */
	u32 reserved05[4];	/* 0x050 */
	u32 reserved06[4];	/* 0x060 */
	u32 reserved07[4];	/* 0x070 */
	u32 task_priority;	/* 0x080 */
	u32 reserved08[3];
	u32 arbitration_priority; /* 0x090 */
	u32 reserved09[3];
	u32 processor_priority; /* 0x0A0 */
	u32 reserved0a[3];
	u32 eoi;		/* 0x0B0 */
	u32 reserved0b[3];
	u32 remote_read;	/* 0x0C0 */
	u32 reserved0c[3];
	u32 logical_destination; /* 0x0D0 */
	u32 reserved0d[3];
	u32 destination_format;	/* 0x0E0 */
	u32 reserved0e[3];
	u32 spurious_interrupt_vector; /* 0x0F0 */
	u32 reserved0f[3];
	u32 in_service_0;	/* 0x100 */
	u32 reserved10[3];
	u32 in_service_1;	/* 0x110 */
	u32 reserved11[3];
	u32 in_service_2;	/* 0x120 */
	u32 reserved12[3];
	u32 in_service_3;	/* 0x130 */
	u32 reserved13[3];
	u32 in_service_4;	/* 0x140 */
	u32 reserved14[3];
	u32 in_service_5;	/* 0x150 */
	u32 reserved15[3];
	u32 in_service_6;	/* 0x160 */
	u32 reserved16[3];
	u32 in_service_7;	/* 0x170 */
	u32 reserved17[3];
	u32 trigger_mode_0;	/* 0x180 */
	u32 reserved18[3];
	u32 trigger_mode_1;	/* 0x190 */
	u32 reserved19[3];
	u32 trigger_mode_2;	/* 0x1A0 */
	u32 reserved1a[3];
	u32 trigger_mode_3;	/* 0x1B0 */
	u32 reserved1b[3];
	u32 trigger_mode_4;	/* 0x1C0 */
	u32 reserved1c[3];
	u32 trigger_mode_5;	/* 0x1D0 */
	u32 reserved1d[3];
	u32 trigger_mode_6;	/* 0x1E0 */
	u32 reserved1e[3];
	u32 trigger_mode_7;	/* 0x1F0 */
	u32 reserved1f[3];
	u32 interrupt_request_0; /* 0x200 */
	u32 reserved20[3];
	u32 interrupt_request_1; /* 0x210 */
	u32 reserved21[3];
	u32 interrupt_request_2; /* 0x220 */
	u32 reserved22[3];
	u32 interrupt_request_3; /* 0x230 */
	u32 reserved23[3];
	u32 interrupt_request_4; /* 0x240 */
	u32 reserved24[3];
	u32 interrupt_request_5; /* 0x250 */
	u32 reserved25[3];
	u32 interrupt_request_6; /* 0x260 */
	u32 reserved26[3];
	u32 interrupt_request_7; /* 0x270 */
	u32 reserved27[3];
	u32 error_status;	/* 0x280 */
	u32 reserved28[3];
	u32 reserved29[4];	/* 0x290 */
	u32 reserved2a[4];	/* 0x2A0 */
	u32 reserved2b[4];	/* 0x2B0 */
	u32 reserved2c[4];	/* 0x2C0 */
	u32 reserved2d[4];	/* 0x2D0 */
	u32 reserved2e[4];	/* 0x2E0 */
	u32 lvt_corrected_machine_check_interrupt; /* 0x2F0 */
	u32 reserved2f[3];
	u32 interrupt_command_0; /* 0x300 */
	u32 reserved30[3];
	u32 interrupt_command_1; /* 0x310 */
	u32 reserved31[3];
	u32 lvt_timer;		/* 0x320 */
	u32 reserved32[3];
	u32 lvt_thermal_sensor;	/* 0x330 */
	u32 reserved33[3];
	u32 lvt_performance_monitoring_counters; /* 0x340 */
	u32 reserved34[3];
	u32 lvt_lint0;		/* 0x350 */
	u32 reserved35[3];
	u32 lvt_lint1;		/* 0x360 */
	u32 reserved36[3];
	u32 lvt_error;		/* 0x370 */
	u32 reserved37[3];
	u32 initial_count;	/* 0x380 */
	u32 reserved38[3];
	u32 current_count;	/* 0x390 */
	u32 reserved39[3];
	u32 reserved3a[4];	/* 0x3A0 */
	u32 reserved3b[4];	/* 0x3B0 */
	u32 reserved3c[4];	/* 0x3C0 */
	u32 reserved3d[4];	/* 0x3D0 */
	u32 divide_configuration; /* 0x3E0 */
	u32 reserved3e[3];
	u32 reserved3f[4];	/* 0x3F0 */
};

struct icr {
	unsigned int vector : 8;
	unsigned int delivery_mode : 3;
	unsigned int destination_mode : 1;
	unsigned int reserved1 : 2;
	unsigned int level : 1;
	unsigned int trigger_mode : 1;
	unsigned int reserved2 : 2;
	unsigned int destination_shorthand : 2;
	unsigned int reserved3 : 12;
	unsigned int destination : 32;
};

static void ap_start (void);

volatile int num_of_processors; /* number of application processors */
static void (*initproc_bsp) (void), (*initproc_ap) (void);
static spinlock_t ap_lock;
static void *newstack_tmp;
static spinlock_t sync_lock;
static u32 sync_id;
static volatile u32 sync_count;
static spinlock_t *apinitlock;
static u32 apinit_addr;
static bool ap_started;
static struct local_apic_registers *lar;
static const u64 apic_base = 0xFEE00000;

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
	spinlock_unlock (&ap_lock);
	segment_init_ap (n);
	currentcpu->stackaddr = newstack;
	int_init_ap ();
	/* Load segments and interrupts as soon as possible, before
	 * using printf(), because printf() might use complex network
	 * functions on UEFI systems. */
	printf ("Processor %d (AP)\n", n);
	proc ();
}

static asmlinkage void
bspinitproc1 (void)
{
	printf ("Processor 0 (BSP)\n");
	spinlock_init (&sync_lock);
	sync_id = 0;
	sync_count = 0;
	num_of_processors = 0;
	spinlock_init (&ap_lock);

	if (!uefi_booted) {
		apinit_addr = 0xF000;
		ap_start ();
	} else {
		apinit_addr = alloc_realmodemem (APINIT_SIZE + 15);
		ASSERT (apinit_addr >= 0x1000);
		while ((apinit_addr & 0xF) != (APINIT_OFFSET & 0xF))
			apinit_addr++;
		ASSERT (apinit_addr > APINIT_OFFSET);
		localapic_delayed_ap_start (ap_start);
	}
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
	newstack_tmp = alloc (VMM_STACKSIZE);
	asm_wrrsp_and_jmp ((ulong)newstack_tmp + VMM_STACKSIZE, apinitproc1);
}

bool
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

static bool
is_x2apic_supported (void)
{
	u32 a, b, c, d;

	asm_cpuid (CPUID_1, 0, &a, &b, &c, &d);
	if (c & CPUID_1_ECX_X2APIC_BIT)
		return true;
	return false;
}

static bool
is_x2apic_enabled (void)
{
	u64 apic_base_msr;

	asm_rdmsr64 (MSR_IA32_APIC_BASE_MSR, &apic_base_msr);
	if (!(apic_base_msr & MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT))
		return false;
	if (apic_base_msr & MSR_IA32_APIC_BASE_MSR_ENABLE_X2APIC_BIT)
		return true;
	return false;
}

static void
write_icr (volatile u32 *apic_icr, u32 value)
{
	if (is_x2apic_supported () && is_x2apic_enabled ())
		asm_wrmsr64 (MSR_IA32_X2APIC_ICR, value);
	else
		*apic_icr = value;
}

static u32
read_svr (volatile u32 *apic_svr)
{
	u32 value, dummy;

	if (is_x2apic_supported () && is_x2apic_enabled ())
		asm_rdmsr32 (MSR_IA32_X2APIC_SIVR, &value, &dummy);
	else
		value = *apic_svr;
	return value;
}

static void
write_svr (volatile u32 *apic_svr, u32 value)
{
	if (is_x2apic_supported () && is_x2apic_enabled ())
		asm_wrmsr64 (MSR_IA32_X2APIC_SIVR, value);
	else
		*apic_svr = value;
}

static void
apic_wait_for_idle (volatile u32 *apic_icr)
{
	if (is_x2apic_supported () && is_x2apic_enabled ())
		return;
	while ((*apic_icr & ICR_STATUS_BIT) != ICR_STATUS_IDLE);
}

static void
apic_send_init (volatile u32 *apic_icr)
{
	apic_wait_for_idle (apic_icr);
	write_icr (apic_icr, ICR_DEST_OTHER | ICR_TRIGGER_EDGE |
		   ICR_LEVEL_ASSERT | ICR_MODE_INIT);
}

static void
apic_send_startup_ipi (volatile u32 *apic_icr, u32 addr)
{
	apic_wait_for_idle (apic_icr);
	write_icr (apic_icr, ICR_DEST_OTHER | ICR_TRIGGER_EDGE |
		   ICR_LEVEL_ASSERT | ICR_MODE_STARTUP | addr);
}

static void
apic_send_nmi (volatile u32 *apic_icr)
{
	apic_wait_for_idle (apic_icr);
	write_icr (apic_icr, ICR_DEST_OTHER | ICR_TRIGGER_EDGE |
		   ICR_LEVEL_ASSERT | ICR_MODE_NMI);
	apic_wait_for_idle (apic_icr);
}

void
ap_start_addr (u8 addr, bool (*loopcond) (void *data), void *data)
{
	volatile u32 *apic_icr;

	if (!apic_available ())
		return;
	ASSERT (lar);
	apic_icr = &lar->interrupt_command_0;
	apic_send_init (apic_icr);
	usleep (10000);
	while (loopcond (data)) {
		apic_send_startup_ipi (apic_icr, addr);
		usleep (200000);
	}
}

static bool
ap_start_loopcond (void *data)
{
	int *p;

	p = data;
	return (*p)++ < 3;
}

static void
ap_start (void)
{
	volatile u32 *num;
	u8 *apinit;
	u32 tmp;
	int i;
	u8 buf[5];
	u8 *p;
	u32 apinit_segment;

	apinit_segment = (apinit_addr - APINIT_OFFSET) >> 4;
	/* Put a "ljmpw" instruction to the physical address 0 */
	p = mapmem_hphys (0, 5, MAPMEM_WRITE);
	memcpy (buf, p, 5);
	p[0] = 0xEA;		/* ljmpw */
	p[1] = APINIT_OFFSET & 0xFF;
	p[2] = APINIT_OFFSET >> 8;
	p[3] = apinit_segment & 0xFF;
	p[4] = apinit_segment >> 8;
	apinit = mapmem_hphys (apinit_addr, APINIT_SIZE, MAPMEM_WRITE);
	ASSERT (apinit);
	memcpy (apinit, cpuinit_start, APINIT_SIZE);
	num = (volatile u32 *)APINIT_POINTER (apinit_procs);
	apinitlock = (spinlock_t *)APINIT_POINTER (apinit_lock);
	*num = 0;
	spinlock_init (apinitlock);
	i = 0;
	ap_start_addr (0, ap_start_loopcond, &i);
	for (;;) {
		spinlock_lock (&ap_lock);
		tmp = num_of_processors;
		spinlock_unlock (&ap_lock);
		if (*num == tmp)
			break;
		usleep (1000000);
	}
	unmapmem ((void *)apinit, APINIT_SIZE);
	memcpy (p, buf, 5);
	unmapmem (p, 5);
	ap_started = true;
}

static void
bsp_continue (asmlinkage void (*initproc_arg) (void))
{
	void *newstack;

	newstack = alloc (VMM_STACKSIZE);
	currentcpu->stackaddr = newstack;
	asm_wrrsp_and_jmp ((ulong)newstack + VMM_STACKSIZE, initproc_arg);
}

void
panic_wakeup_all (void)
{
	u64 tmp;
	volatile u32 *apic_icr;

	if (!apic_available ())
		return;
	if (!ap_started)
		return;
	asm_rdmsr64 (MSR_IA32_APIC_BASE_MSR, &tmp);
	if (!(tmp & MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT))
		return;
	tmp &= ~MSR_IA32_APIC_BASE_MSR_APIC_BASE_MASK;
	tmp |= apic_base;
	asm_wrmsr64 (MSR_IA32_APIC_BASE_MSR, tmp);

	if (!lar)
		return;
	apic_icr = &lar->interrupt_command_0;
	apic_send_nmi (apic_icr);
}

void
disable_apic (void)
{
	u64 tmp;
	volatile u32 *apic_svr;

	if (!apic_available ())
		return;
	asm_rdmsr64 (MSR_IA32_APIC_BASE_MSR, &tmp);
	if (!(tmp & MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT))
		return;
	tmp &= ~MSR_IA32_APIC_BASE_MSR_APIC_BASE_MASK;
	tmp |= apic_base;
	asm_wrmsr64 (MSR_IA32_APIC_BASE_MSR, tmp);

	if (!lar)
		return;
	apic_svr = &lar->spurious_interrupt_vector;
	write_svr (apic_svr, read_svr (apic_svr) & ~SVR_APIC_ENABLED);
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
	lar = mapmem_hphys (apic_base, sizeof *lar, MAPMEM_WRITE | MAPMEM_PWT |
			    MAPMEM_PCD);
	initproc_bsp = bsp_initproc;
	initproc_ap = ap_initproc;
	bsp_continue (bspinitproc1);
}

void
self_ipi (int intnum)
{
	volatile u32 *apic_icr;

	if (intnum < 0x10 || intnum > 0xFF)
		return;
	if (!apic_available ())
		return;
	if (is_x2apic_supported () && is_x2apic_enabled ()) {
		asm_wrmsr32 (MSR_IA32_X2APIC_SELF_IPI, intnum, 0);
		return;
	}
	if (!lar)
		return;
	apic_icr = &lar->interrupt_command_0;
	while ((*apic_icr & ICR_STATUS_BIT) != ICR_STATUS_IDLE);
	*apic_icr = ICR_DEST_SELF | ICR_TRIGGER_EDGE | ICR_LEVEL_ASSERT |
		ICR_MODE_FIXED | intnum;
}

void
send_ipi (u64 icr)
{
	volatile u32 *apic_icr;
	volatile u32 *apic_icr_high;

	if (!~icr)		/* Special invalid value used by
				 * msi_to_icr(). */
		return;
	if ((icr & ICR_MODE_MASK) == ICR_MODE_LOWEST_PRIORITY)
		/* Lowest Priority should be avoided in xAPIC mode or
		 * is reserved in x2APIC mode. */
		icr = (icr & ~ICR_MODE_MASK) | ICR_MODE_FIXED;
	if (!apic_available ())
		return;
	if (is_x2apic_supported () && is_x2apic_enabled ()) {
		asm_wrmsr64 (MSR_IA32_X2APIC_ICR, icr);
		return;
	}
	if (!lar)
		return;
	apic_icr = &lar->interrupt_command_0;
	apic_icr_high = &lar->interrupt_command_1;
	while ((*apic_icr & ICR_STATUS_BIT) != ICR_STATUS_IDLE);
	*apic_icr_high = icr >> 32;
	*apic_icr = icr;
}

void
eoi (void)
{
	volatile u32 *apic_eoi;

	if (!apic_available ())
		return;
	if (is_x2apic_supported () && is_x2apic_enabled ()) {
		asm_wrmsr32 (MSR_IA32_X2APIC_EOI, 0, 0);
		return;
	}
	if (!lar)
		return;
	apic_eoi = &lar->eoi;
	*apic_eoi = 0;
}

u64
msi_to_icr (u32 maddr, u32 mupper, u16 mdata)
{
	union {
		u64 v;
		struct icr b;
	} ret;

	ret.v = ~0ULL;
	if ((maddr & 0xFFF00000) == 0xFEE00000 && !mupper) {
		ret.v = 0;
		ret.b.vector = mdata;
		ret.b.delivery_mode = mdata >> 8;
		ret.b.destination_mode = maddr >> 2;
		ret.b.level = mdata >> 14;
		ret.b.trigger_mode = mdata >> 15;
		ret.b.destination_shorthand = 0; /* No shorthand */
		ret.b.destination = maddr >> 12 << 24;
		if (apic_available () && is_x2apic_supported () &&
		    is_x2apic_enabled ())
			ret.b.destination >>= 24;
	}
	return ret.v;
}
