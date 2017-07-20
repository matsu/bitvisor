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
#include "assert.h"
#include "callrealmode.h"
#include "calluefi.h"
#include "config.h"
#include "convert.h"
#include "current.h"
#include "debug.h"
#include "initfunc.h"
#include "keyboard.h"
#include "linkage.h"
#include "loadbootsector.h"
#include "main.h"
#include "mm.h"
#include "multiboot.h"
#include "osloader.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h" 
#include "regs.h"
#include "sleep.h"
#include "string.h"
#include "svm.h"
#include "svm_init.h"
#include "types.h"
#include "uefi.h"
#include "uefi_param_ext.h"
#include "vcpu.h"
#include "vmmcall.h"
#include "vmmcall_boot.h"
#include "vramwrite.h"
#include "vt.h"
#include "vt_init.h"
#include <share/uefi_boot.h>

static struct multiboot_info mi;
static u32 minios_startaddr;
static u32 minios_paramsaddr;
static u8 minios_params[OSLOADER_BOOTPARAMS_SIZE];
static void *bios_data_area;
static int shiftkey;
static u8 imr_master, imr_slave;

static struct uuid pass_auth_uuid = UEFI_BITVISOR_PASS_AUTH_UUID;
static struct uuid cpu_type_uuid  = UEFI_BITVISOR_CPU_TYPE_UUID;

static void
print_boot_msg (void)
{
	printf ("Starting BitVisor...\n");
	printf ("Copyright (c) 2007, 2008 University of Tsukuba\n");
	printf ("All rights reserved.\n");
}

static void
print_startvm_msg (void)
{
	printf ("Starting a virtual machine.\n");
}

static void
load_drivers (void)
{
	printf ("Loading drivers.\n");
	call_initfunc ("driver");
}

static u8
detect_bios_boot_device (struct multiboot_info *mi)
{
	if (mi->flags.boot_device) {
		return mi->boot_device.drive;
	} else {
		printf ("BIOS boot device detection failed. Using 0x80.\n");
		return 0x80;
	}
}

static void
copy_minios (void)
{
	struct multiboot_modules *q;

	minios_startaddr = 0;
	if (mi.flags.mods && mi.mods_count) {
		q = mapmem (MAPMEM_HPHYS, mi.mods_addr, sizeof *q * 2);
		ASSERT (q);
		minios_paramsaddr = callrealmode_endofcodeaddr ();
		if (mi.mods_count >= 2)
			minios_startaddr = load_minios (q[0].mod_start,
							q[0].mod_end -
							q[0].mod_start,
							q[1].mod_start,
							q[1].mod_end -
							q[1].mod_start,
							minios_paramsaddr,
							minios_params);
		else
			minios_startaddr = load_minios (q[0].mod_start,
							q[0].mod_end -
							q[0].mod_start, 0, 0,
							minios_paramsaddr,
							minios_params);
		unmapmem (q, sizeof *q * 2);
	} else {
		printf ("Module not found.\n");
	}
}

/* the head 640KiB area is saved by save_bios_data_area and */
/* restored by reinitialize_vm. */
/* this function clears other RAM space that may contain sensitive data. */
static void
clear_guest_pages (void)
{
	u64 base, len;
	u32 type;
	u32 n, nn;
	static const u32 maxlen = 0x100000;
	void *p;

	n = 0;
	for (nn = 1; nn; n = nn) {
		nn = getfakesysmemmap (n, &base, &len, &type);
		if (type != SYSMEMMAP_TYPE_AVAILABLE)
			continue;
		if (base < 0x100000) /* < 1MiB */
			continue;
		if (base + len <= 0x100000) /* < 1MiB */
			continue;
		while (len >= maxlen) {
			p = mapmem (MAPMEM_HPHYS | MAPMEM_WRITE, base, maxlen);
			ASSERT (p);
			memset (p, 0, maxlen);
			unmapmem (p, maxlen);
			base += maxlen;
			len -= maxlen;
		}
		if (len > 0) {
			p = mapmem (MAPMEM_HPHYS, base, len);
			ASSERT (p);
			memset (p, 0, len);
			unmapmem (p, len);
		}
	}
}

/* make CPU's virtualization extension usable */
static void
virtualization_init_pcpu (void)
{
	currentcpu->fullvirtualize = FULLVIRTUALIZE_NONE;
	if (vt_available ()) {
		if (currentcpu->cpunum == 0)
			printf ("Using VMX.\n");
		vt_init ();
		currentcpu->fullvirtualize = FULLVIRTUALIZE_VT;
	}
	if (svm_available ()) {
		if (currentcpu->cpunum == 0)
			printf ("Using SVM.\n");
		svm_init ();
		currentcpu->fullvirtualize = FULLVIRTUALIZE_SVM;
	}
}

/* set current vcpu for full virtualization */
static void
set_fullvirtualize (void)
{
	switch (currentcpu->fullvirtualize) {
	case FULLVIRTUALIZE_NONE:
		panic ("Fatal error: This processor does not support"
		       " Intel VT or AMD Virtualization");
		break;
	case FULLVIRTUALIZE_VT:
		vmctl_vt_init ();
		break;
	case FULLVIRTUALIZE_SVM:
		vmctl_svm_init ();
		break;
	}
}

static void
initregs (void)
{
	current->vmctl.reset ();
	current->vmctl.write_general_reg (GENERAL_REG_RAX, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RCX, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RDX, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RBX, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RSP, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RBP, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RSI, 0);
	current->vmctl.write_general_reg (GENERAL_REG_RDI, 0);
	current->vmctl.write_ip (0);
	current->vmctl.write_flags (RFLAGS_ALWAYS1_BIT);
	current->vmctl.write_idtr (0, 0x3FF);
}

static void
sync_cursor_pos (void)
{
	unsigned int row, col;

	vramwrite_get_cursor_pos (&col, &row);
	callrealmode_setcursorpos (0, row, col);
}

static void
save_bios_data_area (void)
{
	void *p;

	bios_data_area = alloc (0xA0000);
	p = mapmem_hphys (0, 0xA0000, 0);
	ASSERT (p);
	memcpy (bios_data_area, p, 0xA0000);
	unmapmem (p, 0xA0000);
}

static void
bsp_reinitialize_devices (void)
{
	/* clear screen */
	vramwrite_clearscreen ();
	/* printf ("init pic\n"); */
	asm_outb (0x20, 0x11);
	asm_outb (0x21, 0x8);
	asm_outb (0x21, 0x4);
	asm_outb (0x21, 0x1);
	asm_outb (0xA0, 0x11);
	asm_outb (0xA1, 0x70);
	asm_outb (0xA1, 0x2);
	asm_outb (0xA1, 0x1);
	asm_outb (0x21, imr_master);
	asm_outb (0xA1, imr_slave);
	keyboard_flush ();
	/* printf ("init pit\n"); */
	sleep_set_timer_counter ();
	/* printf ("sleep 1 sec\n"); */
	usleep (1000000);
	/* printf ("Starting\n"); */
}

static void
get_tmpbuf (u32 *tmpbufaddr, u32 *tmpbufsize)
{
	u32 n, type;
	u64 base, len;

	*tmpbufaddr = callrealmode_endofcodeaddr ();
	n = 0;
	do {
		n = getfakesysmemmap (n, &base, &len, &type);
		if (type != SYSMEMMAP_TYPE_AVAILABLE)
			continue;
		if (base > *tmpbufaddr)
			continue;
		if (base + len <= *tmpbufaddr)
			continue;
		if (base + len > 0xA0000)
			len = 0xA0000 - base;
		*tmpbufsize = base + len - *tmpbufaddr;
		return;
	} while (n);
	panic ("tmpbuf not found");
}

static void
process_cpu_type_ext (void)
{
	phys_t cpu_type_ext_addr = uefi_param_ext_get_phys (&cpu_type_uuid);
	if (cpu_type_ext_addr) {
		struct cpu_type_param_ext *cpu_ext;

		cpu_ext = mapmem_hphys (cpu_type_ext_addr, sizeof *cpu_ext,
					MAPMEM_WRITE);
		switch (currentcpu->fullvirtualize) {
		case FULLVIRTUALIZE_VT:
			cpu_ext->type = CPU_TYPE_INTEL;
			break;
		case FULLVIRTUALIZE_SVM:
			cpu_ext->type = CPU_TYPE_AMD;
			break;
		default:
			panic ("Unknown CPU type");
			break;
		}
		unmapmem (cpu_ext, sizeof *cpu_ext);
	}
}

static void
bsp_init_thread (void *args)
{
	u32 tmpbufaddr, tmpbufsize;
	u8 bios_boot_drive = 0;
	void *p;

	if (!uefi_booted)
		bios_boot_drive = detect_bios_boot_device (&mi);
	save_bios_data_area ();
	callrealmode_usevcpu (current);
	if (minios_startaddr) {
		asm_inb (0x21, &imr_master);
		asm_inb (0xA1, &imr_slave);
		p = mapmem_hphys (minios_paramsaddr, OSLOADER_BOOTPARAMS_SIZE,
				  MAPMEM_WRITE);
		ASSERT (p);
		memcpy (p, minios_params, OSLOADER_BOOTPARAMS_SIZE);
		unmapmem (p, OSLOADER_BOOTPARAMS_SIZE);
		callrealmode_startkernel32 (minios_paramsaddr,
					    minios_startaddr);
		bsp_reinitialize_devices ();
		/* reinitialize_vm */
		p = mapmem_hphys (0, 0xA0000, MAPMEM_WRITE);
		ASSERT (p);
		memcpy (p, bios_data_area, 0xA0000);
		unmapmem (p, 0xA0000);
		clear_guest_pages ();
		call_initfunc ("config0");
		load_drivers ();
		call_initfunc ("config1");
	} else if (!uefi_booted) {
		load_drivers ();
		get_tmpbuf (&tmpbufaddr, &tmpbufsize);
		load_bootsector (bios_boot_drive, tmpbufaddr, tmpbufsize);
		sync_cursor_pos ();
	}
	initregs ();
	if (uefi_booted) {
		process_cpu_type_ext ();
		copy_uefi_bootcode ();
		if (uefi_param_ext_get_phys (&pass_auth_uuid))
			vmmcall_boot_continue();
		call_initfunc ("config0");
		load_drivers ();
		call_initfunc ("config1");
	} else {
		copy_bootsector ();
	}
}

static void
create_pass_vm (void)
{
	bool bsp = false;
	static struct vcpu *vcpu0;

	if (currentcpu->cpunum == 0)
		bsp = true;
	sync_all_processors ();
	if (bsp) {
		load_new_vcpu (NULL);
		vcpu0 = current;
	}
	sync_all_processors ();
	if (!bsp)
		load_new_vcpu (vcpu0);
	set_fullvirtualize ();
	sync_all_processors ();
	current->vmctl.vminit ();
	call_initfunc ("pass");
	sync_all_processors ();
	if (bsp) {
		vmmcall_boot_enable (bsp_init_thread, NULL);
	} else {
		initregs ();
		current->vmctl.init_signal ();
	}
	current->vmctl.enable_resume ();
	current->initialized = true;
	sync_all_processors ();
	if (bsp)
		print_startvm_msg ();
	currentcpu->pass_vm_created = true;
#ifdef DEBUG_GDB
	if (!bsp)
		for (;;)
			asm_cli_and_hlt ();
#endif
	current->vmctl.start_vm ();
	panic ("VM stopped.");
}

static void
wait_for_create_pass_vm (void)
{
	sync_all_processors ();
	sync_all_processors ();
	sync_all_processors ();
	sync_all_processors ();
	sync_all_processors ();
}

void
resume_vm (u32 wake_addr)
{
	current->vmctl.resume ();
	initregs ();
	if (currentcpu->cpunum == 0) {
		current->vmctl.write_realmode_seg (SREG_SS, 0);
		current->vmctl.write_general_reg (GENERAL_REG_RSP, 0x1000);
		current->vmctl.write_realmode_seg (SREG_CS, wake_addr >> 4);
		current->vmctl.write_ip (wake_addr & 0xF);
	} else {
		current->vmctl.init_signal ();
#ifdef DEBUG_GDB
		for (;;)
			asm_cli_and_hlt ();
#endif
	}
	current->vmctl.start_vm ();
	panic ("VM stopped.");
}

static void
get_shiftflags (void)
{
#ifdef SHIFT_KEY_DEBUG
	if (uefi_booted)
		shiftkey = 0;
	else
		shiftkey = callrealmode_getshiftflags ();
#else
	shiftkey = 0;
#endif
}

static void
debug_on_shift_key (void)
{
	int d;

	d = newprocess ("init");
	debug_msgregister ();
	msgsendint (d,
		    !!((shiftkey & GETSHIFTFLAGS_RSHIFT_BIT) ||
		       (shiftkey & GETSHIFTFLAGS_LSHIFT_BIT)));
	debug_msgunregister ();
	msgclose (d);
}

static void
call_parallel (void)
{
	static struct {
		char *name;
		ulong not_called;
	} paral[] = {
		{ "paral0", 1 },
		{ "paral1", 1 },
		{ "paral2", 1 },
		{ "paral3", 1 },
		{ NULL, 0 }
	};
	int i;

	for (i = 0; paral[i].name; i++) {
		if (asm_lock_ulong_swap (&paral[i].not_called, 0))
			call_initfunc (paral[i].name);
	}
}

static void
ap_proc (void)
{
	call_initfunc ("ap");
	call_parallel ();
	call_initfunc ("pcpu");
}

static void
bsp_proc (void)
{
	call_initfunc ("bsp");
	call_parallel ();
	call_initfunc ("pcpu");
}

asmlinkage void
vmm_main (struct multiboot_info *mi_arg)
{
	uefi_booted = !mi_arg;
	if (!uefi_booted)
		memcpy (&mi, mi_arg, sizeof (struct multiboot_info));
	initfunc_init ();
	call_initfunc ("global");
	start_all_processors (bsp_proc, ap_proc);
}

INITFUNC ("pcpu2", virtualization_init_pcpu);
INITFUNC ("pcpu5", create_pass_vm);
INITFUNC ("dbsp5", wait_for_create_pass_vm);
INITFUNC ("bsp0", debug_on_shift_key);
INITFUNC ("global1", print_boot_msg);
INITFUNC ("global3", copy_minios);
INITFUNC ("global3", get_shiftflags);
