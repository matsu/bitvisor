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
#include "current.h"
#include "initfunc.h"
#include "localapic.h"
#include "mm.h"
#include "mmio.h"
#include "msr_pass.h"
#include "panic.h"

#define APIC_BASE	0xFEE00000
#define APIC_LEN	0x1000
#define APIC_ID		0xFEE00020
#define APIC_ID_AID_SHIFT	24
#define APIC_ICR_LOW	0xFEE00300
#define APIC_ICR_LOW_VEC_MASK	0xFF
#define APIC_ICR_LOW_MT_MASK	0x700
#define APIC_ICR_LOW_MT_STARTUP	0x600
#define APIC_ICR_LOW_MT_INIT	0x500
#define APIC_ICR_LOW_DM_LOGICAL_BIT	0x800
#define APIC_ICR_LOW_DSH_MASK	0xC0000
#define APIC_ICR_LOW_DSH_DEST	0x00000
#define APIC_ICR_LOW_DSH_SELF	0x40000
#define APIC_ICR_LOW_DSH_ALL	0x80000
#define APIC_ICR_LOW_DSH_OTHERS	0xC0000
#define APIC_ICR_HIGH	0xFEE00310
#define APIC_ICR_HIGH_DES_SHIFT	24

struct do_startup_data {
	struct vcpu *vcpu0;
	u32 sipi_vector, apic_id;
	u32 broadcast_id;
};

static void (*ap_start) (void);
static void *mmio_handle;

static bool
match_apic_id (struct vcpu *p, struct do_startup_data *d)
{
	if (d->apic_id == d->broadcast_id)
		return true;
	if (d->broadcast_id == 0xFFFFFFFF &&
	    p->localapic.x2apic_id != 0xFFFFFFFF) {
		if (p->localapic.x2apic_id == d->apic_id)
			return true;
	} else {
		if (p->localapic.apic_id == d->apic_id)
			return true;
	}
	return false;
}

static bool
do_startup (struct vcpu *p, void *q)
{
	struct do_startup_data *d;
	u32 sipi_vector;

	d = q;
	if (p->vcpu0 != d->vcpu0)
		return false;
	if (match_apic_id (p, d)) {
		sipi_vector = p->localapic.sipi_vector;
		asm_lock_cmpxchgl (&p->localapic.sipi_vector, &sipi_vector,
				   d->sipi_vector);
	}
	return false;
}

static void
apic_startup (u32 icr_low, u32 apic_id, u32 broadcast_id)
{
	struct do_startup_data d;

	switch (icr_low & APIC_ICR_LOW_DSH_MASK) {
	case APIC_ICR_LOW_DSH_DEST:
		if (icr_low & APIC_ICR_LOW_DM_LOGICAL_BIT)
			panic ("Start-up IPI with a logical ID destination"
			       " is not yet supported");
		d.apic_id = apic_id;
		d.broadcast_id = broadcast_id;
		break;
	case APIC_ICR_LOW_DSH_SELF:
		panic ("Delivering start-up IPI to self");
	case APIC_ICR_LOW_DSH_ALL:
		panic ("Delivering start-up IPI to all including self");
	case APIC_ICR_LOW_DSH_OTHERS:
		d.apic_id = broadcast_id;
		d.broadcast_id = broadcast_id;
		break;
	}
	d.vcpu0 = current->vcpu0;
	d.sipi_vector = icr_low & APIC_ICR_LOW_VEC_MASK;
	vcpu_list_foreach (do_startup, &d);
}

static int
handle_ap_start (u32 icr_low)
{
	switch (icr_low & APIC_ICR_LOW_MT_MASK) {
	case APIC_ICR_LOW_MT_INIT:
	case APIC_ICR_LOW_MT_STARTUP:
		ap_start ();
		ap_start = NULL;
		call_initfunc ("dbsp");
		if (!current->vcpu0->localapic.registered) {
			msr_pass_hook_x2apic_icr (0);
			mmio_unregister (mmio_handle);
			mmio_handle = NULL;
			return 0;
		}
	}
	return 1;
}

static int
mmio_apic (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 f)
{
	u32 *apic_icr_low, *apic_icr_high;

	if (!wr)
		return 0;
	if (gphys == APIC_ICR_LOW) {
		if (len != 4)
			panic ("APIC_ICR len %u", len);
	} else if (gphys > APIC_ICR_LOW) {
		if (gphys < APIC_ICR_LOW + 4)
			panic ("APIC_ICR gphys 0x%llX", gphys);
		else
			return 0;
	} else {
		if (gphys + len > APIC_ICR_LOW)
			panic ("APIC_ICR gphys 0x%llX len %u", gphys, len);
		else
			return 0;
	}

	apic_icr_low = buf;
	if (ap_start) {
		if (!handle_ap_start (*apic_icr_low))
			return 0;
	}
	switch (*apic_icr_low & APIC_ICR_LOW_MT_MASK) {
	default:
		return 0;
	case APIC_ICR_LOW_MT_STARTUP:
		apic_icr_high = mapmem_hphys (APIC_ICR_HIGH,
					      sizeof *apic_icr_high,
					      MAPMEM_PCD | MAPMEM_PWT);
		 /* APIC ID 0xFF means a broadcast */
		apic_startup (*apic_icr_low,
			      *apic_icr_high >> APIC_ICR_HIGH_DES_SHIFT, 0xFF);
		unmapmem (apic_icr_high, sizeof *apic_icr_high);
		return 0;
	}
	return 0;
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

static u32
get_x2apic_id (void)
{
	u32 a, b, c, d;

	asm_cpuid (0xB, 0, &a, &b, &c, &d);
	return d;
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

u32
localapic_wait_for_sipi (void)
{
	u32 *apic_id, sipi_vector;

	current->localapic.x2apic_id = 0xFFFFFFFF;
	if (is_x2apic_supported ()) {
		current->localapic.x2apic_id = get_x2apic_id ();
		if (is_x2apic_enabled ()) {
			/* Skip use of the memory mapped interface
			 * which is not available while x2APIC is
			 * enabled. */
			current->localapic.apic_id =
				current->localapic.x2apic_id & 0xFF;
			goto x2apic_enabled;
		}
	}
	apic_id = mapmem_hphys (APIC_ID, sizeof *apic_id,
				MAPMEM_PCD | MAPMEM_PWT);
	current->localapic.apic_id = *apic_id >> APIC_ID_AID_SHIFT;
	unmapmem (apic_id, sizeof *apic_id);
x2apic_enabled:
	sipi_vector = current->localapic.sipi_vector;
	asm_lock_cmpxchgl (&current->localapic.sipi_vector, &sipi_vector, ~0U);
	do {
		asm_pause ();
		asm_lock_cmpxchgl (&current->localapic.sipi_vector,
				   &sipi_vector, ~0U);
	} while (sipi_vector == ~0U);
	return sipi_vector;
}

void
localapic_change_base_msr (u64 msrdata)
{
	if (msrdata & MSR_IA32_APIC_BASE_MSR_ENABLE_X2APIC_BIT) {
		/* current->vcpu0->localapic.registered == true on AMD
		 * multiprocessor/multicore environment */
		/* ap_start != NULL until ap_start is called on UEFI
		 * environment */
		if (current->vcpu0->localapic.registered || ap_start)
			msr_pass_hook_x2apic_icr (1);
	}
	if (!current->vcpu0->localapic.registered)
		return;
	if (!(msrdata & MSR_IA32_APIC_BASE_MSR_APIC_GLOBAL_ENABLE_BIT))
		return;
	if ((msrdata & 0xFFFFFFFF00000000ULL) ||
	    (msrdata & MSR_IA32_APIC_BASE_MSR_APIC_BASE_MASK) != APIC_BASE)
		panic ("localapic_wait_for_sipi: Bad APIC base 0x%llX",
		       msrdata);
}

void
localapic_mmio_register (void)
{
	void *handle = NULL;

	if (!current->vcpu0->localapic.registered) {
		if (!current->vcpu0->localapic.delayed_ap_start)
			handle = mmio_register (APIC_BASE, APIC_LEN, mmio_apic,
						NULL);
		if (handle)
			mmio_handle = handle;
		current->vcpu0->localapic.registered = true;
	}
}

void
localapic_delayed_ap_start (void (*func) (void))
{
	ap_start = func;
}

void
localapic_x2apic_icr (u64 msrdata)
{
	if (!current->vcpu0->localapic.registered && !ap_start) {
		msr_pass_hook_x2apic_icr (0);
		return;
	}
	if (ap_start) {
		if (!handle_ap_start (msrdata))
			return;
	}
	switch (msrdata & APIC_ICR_LOW_MT_MASK) {
	case APIC_ICR_LOW_MT_STARTUP:
		 /* APIC ID 0xFFFFFFFF means a broadcast */
		apic_startup (msrdata, msrdata >> 32, 0xFFFFFFFF);
	}
}

static void
localapic_init (void)
{
	if (current == current->vcpu0)
		current->localapic.registered = false;
}

static void
localapic_init2 (void)
{
	void *handle = NULL;

	if (is_x2apic_supported () && is_x2apic_enabled ()) {
		/* x2APIC is enabled by firmware */
		msr_pass_hook_x2apic_icr (1);
	}
	if (!ap_start || current != current->vcpu0)
		return;
	if (!current->localapic.registered)
		handle = mmio_register (APIC_BASE, APIC_LEN, mmio_apic, NULL);
	if (handle)
		mmio_handle = handle;
	current->localapic.delayed_ap_start = true;
}

INITFUNC ("vcpu0", localapic_init);
INITFUNC ("pass5", localapic_init2);
