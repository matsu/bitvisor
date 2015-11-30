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
#include "assert.h"
#include "constants.h"
#include "elf.h"
#include "entry.h"
#include "initfunc.h"
#include "mm.h"
#include "msg.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "process.h"
#include "process_builtin.h"
#include "process_sysenter.h"
#include "seg.h"
#include "spinlock.h"
#include "string.h"
#include "types.h"

#define NUM_OF_SYSCALLS 32
#define NAMELEN 16
#define MAX_MSGLEN 16384

typedef ulong (*syscall_func_t) (ulong ip, ulong sp, ulong num, ulong si,
				 ulong di);

struct syscall_regs {
	ulong rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;
};

struct msgdsc_data {
	int pid;
	int gen;
	int dsc;
	void *func;
};

struct process_data {
	bool valid;
	phys_t mm_phys;
	int gen;
	int running;
	struct msgdsc_data msgdsc[NUM_OF_MSGDSC];
	bool exitflag;
	bool setlimit;
	int stacksize;
};

extern ulong volatile syscallstack asm ("%gs:gs_syscallstack");
static struct process_data process[NUM_OF_PID];
static spinlock_t process_lock;
static bool process_initialized = false;

static bool
is_range_valid (ulong addr, u32 len)
{
	if (addr >= 0x40000000)
		return false;
	if (addr + len >= 0x40000000)
		return false;
	if (addr < 0x00000000)
		return false;
	if (len >= 0x40000000)
		return false;
	return true;
}

static ulong
sys_nop (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	return 0;
}

static void
clearmsgdsc (struct msgdsc_data msgdsc[NUM_OF_MSGDSC])
{
	int i;

	for (i = 0; i < NUM_OF_MSGDSC; i++) {
		msgdsc[i].pid = 0;
		msgdsc[i].gen = 0;
		msgdsc[i].dsc = 0;
		msgdsc[i].func = NULL;
	}
}

#ifndef __x86_64__
static bool
sysenter_available (void)
{
	u32 a, b, c, d;
	unsigned int family, model, stepping;

	asm_cpuid (1, 0, &a, &b, &c, &d);
	if (d & CPUID_1_EDX_SEP_BIT) {
		family = (a & 0xF00) >> 8;
		model = (a & 0xF0) >> 4;
		stepping = (a & 0xF);
		if (family == 6 && model < 3 && stepping < 3)
			return false;
		return true;
	}
	return false;
}
#endif

static void
set_process64_msrs (void)
{
#ifdef __x86_64__
	asm_wrmsr32 (MSR_IA32_STAR, (u32) (ulong)syscall_entry_sysret64,
		     (SEG_SEL_CODE32U << 16) | SEG_SEL_CODE64);
	asm_wrmsr (MSR_IA32_LSTAR, (ulong)syscall_entry_sysret64);
	asm_wrmsr (MSR_AMD_CSTAR, (ulong)syscall_entry_sysret64);
	asm_wrmsr (MSR_IA32_FMASK,
		   RFLAGS_VM_BIT | RFLAGS_IF_BIT | RFLAGS_RF_BIT);
#endif
}

static void
setup_syscallentry (void)
{
#ifdef __x86_64__
	ulong efer;

	asm_wrmsr (MSR_IA32_SYSENTER_CS, 0);	/* Disable SYSENTER/SYSEXIT */
	set_process64_msrs ();
	asm_rdmsr (MSR_IA32_EFER, &efer);
	efer |= MSR_IA32_EFER_SCE_BIT;
	asm_wrmsr (MSR_IA32_EFER, efer);
#else
	if (sysenter_available ()) {
		asm_wrmsr (MSR_IA32_SYSENTER_CS, SEG_SEL_CODE32);
		asm_wrmsr (MSR_IA32_SYSENTER_EIP,
			   (ulong)syscall_entry_sysexit);
		asm_wrmsr (MSR_IA32_SYSENTER_ESP, currentcpu->tss32.esp0);
	}
#endif
}

static void
process_init_global (void)
{
	int i;

	for (i = 0; i < NUM_OF_PID; i++) {
		process[i].valid = false;
		process[i].gen = 1;
	}
	process[0].valid = true;
	clearmsgdsc (process[0].msgdsc);
	setup_syscallentry ();
	spinlock_init (&process_lock);
	process_initialized = true;
}

static void
process_init_ap (void)
{
	setup_syscallentry ();
}

static void
process_wakeup (void)
{
	setup_syscallentry ();
}

static void
load_bin (virt_t destvirt, uint destlen, void *src, uint srclen)
{
	mm_process_map_alloc (destvirt, destlen);
	memcpy ((void *)destvirt, src, srclen);
}

static ulong
process_load (void *bin)
{
	u8 *b;
	ELF_EHDR *ehdr;
	ELF_PHDR *phdr;
	unsigned int i;

	b = bin;
	if (b[0] != 0x7F && b[1] != 'E' && b[2] != 'L' && b[3] != 'F')
		return 0;
	ehdr = bin;
	phdr = (ELF_PHDR *)((u8 *)bin + ehdr->e_phoff);
	for (i = ehdr->e_phnum; i;
	     i--, phdr = (ELF_PHDR *)((u8 *)phdr + ehdr->e_phentsize)) {
		if (phdr->p_type == PT_LOAD) {
			load_bin (phdr->p_vaddr,
				  phdr->p_memsz,
				  (u8 *)bin + phdr->p_offset,
				  phdr->p_filesz);
		}
	}
	return ehdr->e_entry;
}

/* for internal use */
static int
_msgopen_2 (int pid, int mpid, int mgen, int mdesc)
{
	int i, r;

	spinlock_lock (&process_lock);
	for (i = 0; i < NUM_OF_MSGDSC; i++) {
		if (process[pid].msgdsc[i].pid == 0 &&
		    process[pid].msgdsc[i].gen == 0)
			goto found;
	}
	r = -1;
	goto ret;
found:
	process[pid].msgdsc[i].pid = mpid;
	process[pid].msgdsc[i].gen = mgen;
	process[pid].msgdsc[i].dsc = mdesc;
	r = i;
ret:
	spinlock_unlock (&process_lock);
	return r;
}

static int
process_new (int frompid, void *bin, int stacksize)
{
	int pid, gen;
	u64 phys;
	ulong rip;
	phys_t mm_phys;

	spinlock_lock (&process_lock);
	for (pid = 1; pid < NUM_OF_PID; pid++) {
		if (!process[pid].valid)
			goto found;
	}
err:
	spinlock_unlock (&process_lock);
	return -1;
found:
	if (mm_process_alloc (&phys) < 0) /* alloc page directories and init */
		goto err;
	process[pid].mm_phys = phys;
	process[pid].running = 0;
	process[pid].exitflag = false;
	process[pid].setlimit = false;
	process[pid].stacksize = PAGESIZE;
	if (stacksize > PAGESIZE)
		process[pid].stacksize = stacksize;
	gen = ++process[pid].gen;
	process[pid].valid = true;
	clearmsgdsc (process[pid].msgdsc);
	mm_phys = mm_process_switch (phys);
	if (!(rip = process_load (bin))) { /* load a program */
		printf ("process_load failed.\n");
		process[pid].valid = false;
		pid = 0;
	}
	/* for system calls */
#ifdef __x86_64__
	mm_process_map_shared_physpage (0x3FFFF000,
					sym_to_phys (processuser_syscall),
					false);
#else
	if (sysenter_available ())
		mm_process_map_shared_physpage (0x3FFFF000,
						sym_to_phys
						(processuser_sysenter),
						false);
	else
		mm_process_map_shared_physpage (0x3FFFF000,
						sym_to_phys
						(processuser_no_sysenter),
						false);
#endif
	process[pid].msgdsc[0].func = (void *)rip;
	mm_process_switch (mm_phys);
	spinlock_unlock (&process_lock);
	return _msgopen_2 (frompid, pid, gen, 0);
}

static void
callret (int retval)
{
	asm_wrrsp_and_ret (syscallstack, retval);
}

void
process_kill (bool (*func) (void *data), void *data)
{
	if (!process_initialized) /* process not ready. */
		return;
	if (!syscallstack)	/* can't kill */
		return;
	if (func && !func (data))
		return;
	process[currentcpu->pid].exitflag = true;
	callret (-1);
}

/* free any resources of a process */
/* CR3 must be the process's one */
/* process_lock must be locked */
static void
cleanup (int pid, phys_t mm_phys)
{
	ASSERT (process[pid].valid);
	ASSERT (process[pid].running == 0);
	process[pid].gen++;
	msg_unregisterall (pid);
	mm_process_unmapall ();
	mm_process_switch (mm_phys);
	mm_process_free (process[pid].mm_phys);
	process[pid].valid = false;
}

static void
release_process64_msrs (void *data)
{
}

bool
own_process64_msrs (void (*func) (void *data), void *data)
{
	struct pcpu *cpu = currentcpu;

	if (cpu->release_process64_msrs == func &&
	    cpu->release_process64_msrs_data == data)
		return false;
	if (cpu->release_process64_msrs)
		cpu->release_process64_msrs (cpu->release_process64_msrs_data);
	cpu->release_process64_msrs = func;
	cpu->release_process64_msrs_data = data;
	return true;
}

/* pid, func=pointer to the function of the process,
   sp=stack pointer of the process */
/* process_lock must be locked */
static int
call_msgfunc0 (int pid, void *func, ulong sp)
{
	ulong ax, bx, cx, dx, si, di;
	int oldpid;

	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	ASSERT (process[pid].valid);
	if (pid == 0) {
		panic ("call_msgfunc0 can't call kernel");
	}
	oldpid = currentcpu->pid;
	currentcpu->pid = pid;
	process[pid].running++;
	spinlock_unlock (&process_lock);
	if (own_process64_msrs (release_process64_msrs, NULL))
		set_process64_msrs ();
	asm volatile (
#ifdef __x86_64__
		" pushq %%rbp \n"
		" pushq %8 \n"
		" pushq $1f \n"
		" movq %%rsp,%8 \n"
		" jmp ret_to_user64 \n"
		"1: \n"
		" popq %8 \n"
		" popq %%rbp \n"
#else
		" push %%ebp \n"
		" pushl %8 \n"

		" push $1f \n"
		" mov %%esp,%8 \n"
		" jmp ret_to_user32 \n"
		"1: \n"
		" popl %8 \n"
		" pop %%ebp \n"
#endif
		: "=a" (ax)
		, "=b" (bx)
		, "=c" (cx)
		, "=d" (dx)
		, "=S" (si)
		, "=D" (di)
		: "c" (sp)
		, "d" ((ulong)func)
		, "m" (syscallstack)
		: "memory", "cc"
#ifdef __x86_64__
		, "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
#endif
		);
	spinlock_lock (&process_lock);
	process[pid].running--;
	currentcpu->pid = oldpid;
	return (int)ax;
}

/* pid, gen, desc, arg=arguments, len=length of the arguments (bytes) */
static int
call_msgfunc1 (int pid, int gen, int desc, void *arg, int len,
	       struct msgbuf *buf, int bufcnt)
{
	phys_t mm_phys;
	ulong sp, sp2, buf_sp;
	int r = -1;
	struct msgbuf buf_user[MAXNUM_OF_MSGBUF];
	void *curstk;
	int (*func) (int, int, struct msgbuf *, int);
	int i;
	long tmp;
	int stacksize;

	asm_rdrsp ((ulong *)&curstk);
	if ((u8 *)curstk - (u8 *)currentcpu->stackaddr < VMM_MINSTACKSIZE) {
		printf ("msg: not enough stack space available for VMM\n");
		return r;
	}
	spinlock_lock (&process_lock);
	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	if (!process[pid].valid)
		goto ret;
	ASSERT (desc >= 0);
	ASSERT (desc < NUM_OF_MSGDSC);
	if (process[pid].gen != gen)	
		goto ret;
	if (process[pid].msgdsc[desc].func == NULL)
		goto ret;
	if (pid == 0) {
		ASSERT (len == sizeof (long) * 2);
		func = (int (*)(int, int, struct msgbuf *, int))
			process[pid].msgdsc[desc].func;
		spinlock_unlock (&process_lock);
		r = func (((long *)arg)[0], ((long *)arg)[1], buf, bufcnt);
		return r;
	}
	if (bufcnt > MAXNUM_OF_MSGBUF)
		goto ret;
	mm_phys = mm_process_switch (process[pid].mm_phys);
	for (i = 0; i < bufcnt; i++) {
		if (buf[i].premap_handle) {
			tmp = (long)buf[i].base - buf[i].premap_handle;
			buf_user[i].base = (void *)tmp;
		} else {
			buf_user[i].base = mm_process_map_shared (mm_phys,
								  buf[i].base,
								  buf[i].len,
								  !!buf[i].rw,
								  false);
		}
		if (!buf_user[i].base) {
			bufcnt = i;
			goto mapfail;
		}
		buf_user[i].len = buf[i].len;
		buf_user[i].rw = buf[i].rw;
		buf_user[i].premap_handle = 0;
	}
	stacksize = process[pid].stacksize;
	sp2 = mm_process_map_stack (stacksize, process[pid].setlimit, true);
	if (!sp2) {
		printf ("cannot allocate stack for process\n");
		goto mapfail;
	}
	sp = sp2;
	for (i = bufcnt; i-- > 0;) {
		sp -= sizeof buf_user[i];
		memcpy ((void *)sp, &buf_user[i], sizeof buf_user[i]);
	}
	buf_sp = sp;
#ifdef __x86_64__
	/* for %r8 and %r9 */
	sp -= sizeof (ulong);
	*(ulong *)sp = 0;
	sp -= sizeof (ulong);
	*(ulong *)sp = 0;
#endif
	sp -= sizeof (ulong);
	*(ulong *)sp = bufcnt;
	sp -= sizeof (ulong);
	*(ulong *)sp = buf_sp;
	sp -= len;
	memcpy ((void *)sp, arg, len);
	sp -= sizeof (ulong);
	*(ulong *)sp = 0x3FFFF100;
	r = call_msgfunc0 (pid, process[pid].msgdsc[desc].func, sp);
	mm_process_unmap_stack (sp2, stacksize);
mapfail:
	for (i = 0; i < bufcnt; i++) {
		if (buf[i].premap_handle)
			continue;
		mm_process_unmap ((virt_t)buf_user[i].base, buf_user[i].len);
	}
	if (process[pid].running == 0 && process[pid].exitflag)
		cleanup (pid, mm_phys);
	mm_process_switch (mm_phys);
ret:
	spinlock_unlock (&process_lock);
	return r;
}

/* for internal use */
static void *
_msgsetfunc (int pid, int desc, void *func)
{
	void *oldfunc;

	spinlock_lock (&process_lock);
	oldfunc = process[pid].msgdsc[desc].func;
	process[pid].msgdsc[desc].func = func;
	spinlock_unlock (&process_lock);
	return oldfunc;
}

/* for kernel (VMM) */
void *
msgsetfunc (int desc, void *func)
{
	return _msgsetfunc (0, desc, func);
}

/* si=desc di=func */
ulong
sys_msgsetfunc (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	if (process[currentcpu->pid].setlimit)
		return (ulong)-1L;
	if (si >= 0 && si < NUM_OF_MSGDSC)
		return (ulong)_msgsetfunc (currentcpu->pid, si, (void *)di);
	return 0;
}

/* for internal use*/
static int
_msgregister (int pid, char *name, void *func)
{
	int r, i;

	spinlock_lock (&process_lock);
	for (i = 0; i < NUM_OF_MSGDSC; i++) {
		if (!process[pid].msgdsc[i].func)
			goto found;
	}
	r = -1;
	goto ret;
found:
	process[pid].msgdsc[i].func = func;
	if (msg_register (name, pid, process[pid].gen, i) < 0) {
		process[pid].msgdsc[i].func = NULL;
		r = -1;
	} else {
		r = i;
	}
ret:
	spinlock_unlock (&process_lock);
	return r;
}

/* for kernel (VMM) */
int
msgregister (char *name, void *func)
{
	return _msgregister (0, name, func);
}

/* si=name di=func */
ulong
sys_msgregister (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	char name[MSG_NAMELEN];

	if (!is_range_valid (si, MSG_NAMELEN))
		return (ulong)-1L;
	if (process[currentcpu->pid].setlimit)
		return (ulong)-1L;
	memcpy (name, (void *)si, MSG_NAMELEN);
	name[MSG_NAMELEN - 1] = '\0';
	return _msgregister (currentcpu->pid, name, (void *)di);
}

/* for internal use */
static int
_msgopen (int pid, char *name)
{
	int mpid, mgen, mdesc;

	if (msg_findname (name, &mpid, &mgen, &mdesc))
		return -1;
	return _msgopen_2 (pid, mpid, mgen, mdesc);
}

/* for kernel (VMM) */
int
msgopen (char *name)
{
	return _msgopen (0, name);
}

/* si=name */
ulong
sys_msgopen (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	char name[MSG_NAMELEN];

	if (!is_range_valid (si, MSG_NAMELEN))
		return (ulong)-1L;
	if (process[currentcpu->pid].setlimit)
		return (ulong)-1L;
	memcpy (name, (void *)si, MSG_NAMELEN);
	name[MSG_NAMELEN - 1] = '\0';
	return _msgopen (currentcpu->pid, name);
}

/* for internal use */
static int
_msgclose (int pid, int desc)
{
	if (desc >= 0 && desc < NUM_OF_MSGDSC) {
		spinlock_lock (&process_lock);
		process[pid].msgdsc[desc].pid = 0;
		process[pid].msgdsc[desc].gen = 0;
		spinlock_unlock (&process_lock);
		return 0;
	}
	return -1;
}

/* for kernel (VMM) */
int
msgclose (int desc)
{
	return _msgclose (0, desc);
}

/* si=desc */
ulong
sys_msgclose (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	return _msgclose (currentcpu->pid, si);
}

/* for internal use */
static int
_msgsendint (int pid, int desc, int data)
{
	int mpid, mgen, mdesc;
	long d[2];

	d[0] = MSG_INT;
	d[1] = data;
	spinlock_lock (&process_lock);
	mpid = process[pid].msgdsc[desc].pid;
	mgen = process[pid].msgdsc[desc].gen;
	mdesc = process[pid].msgdsc[desc].dsc;
	spinlock_unlock (&process_lock);
	return call_msgfunc1 (mpid, mgen, mdesc, d, sizeof (d), NULL, 0);
}

/* for kernel (VMM) */
int
msgsendint (int desc, int data)
{
	return _msgsendint (0, desc, data);
}

/* si=desc di=data */
ulong
sys_msgsendint (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	return _msgsendint (currentcpu->pid, si, di);
}

/* si=retval */
ulong
sys_msgret (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	callret ((int)si);
	return 0;		/* Not reached. */
}

/* for internal use */
static int
_msgsenddesc (int frompid, int todesc, int senddesc)
{
	int topid, togen, i, r = -1;

	if (senddesc < 0 || senddesc >= NUM_OF_MSGDSC)
		return -1;
	if (todesc < 0 || todesc >= NUM_OF_MSGDSC)
		return -1;
	spinlock_lock (&process_lock);
	topid = process[frompid].msgdsc[todesc].pid;
	togen = process[frompid].msgdsc[todesc].gen;
	ASSERT (topid >= 0);
	ASSERT (topid < NUM_OF_PID);
	if (!process[topid].valid)
		goto ret;
	if (process[topid].gen != togen)
		goto ret;
	for (i = 0; i < NUM_OF_MSGDSCRECV; i++) {
		if (process[topid].msgdsc[i].pid == 0 &&
		    process[topid].msgdsc[i].gen == 0)
			goto found;
	}
	goto ret;
found:
	process[topid].msgdsc[i].pid = process[frompid].msgdsc[senddesc].pid;
	process[topid].msgdsc[i].gen = process[frompid].msgdsc[senddesc].gen;
	process[topid].msgdsc[i].dsc = process[frompid].msgdsc[senddesc].dsc;
	r = i;
ret:
	spinlock_unlock (&process_lock);
	return r;
}

/* for kernel (VMM) */
int
msgsenddesc (int desc, int data)
{
	return _msgsenddesc (0, desc, data);
}

/* si=desc di=data */
ulong
sys_msgsenddesc (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	if (process[currentcpu->pid].setlimit)
		return (ulong)-1L;
	return _msgsenddesc (currentcpu->pid, si, di);
}

static void *
_builtin_find (char *name, int *stacksize)
{
	struct process_builtin_data *p;

	for (p = __process_builtin; p != __process_builtin_end; p++) {
		if (strcmp (name, p->name) == 0) {
			*stacksize = p->stacksize;
			return p->bin;
		}
	}
	return NULL;
}

static int
_newprocess (int frompid, char *name)
{
	void *bin = NULL;
	int stacksize = 0;

	if (!bin)
		bin = _builtin_find (name, &stacksize);
	if (!bin)
		return -1;
	return process_new (frompid, bin, stacksize);
}

int
newprocess (char *name)
{
	return _newprocess (0, name);
}

/* si=name */
ulong
sys_newprocess (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	char name[PROCESS_NAMELEN];

	if (process[currentcpu->pid].setlimit)
		return (ulong)-1L;
	memcpy (name, (void *)si, PROCESS_NAMELEN);
	name[PROCESS_NAMELEN - 1] = '\0';
	return (ulong)_newprocess (currentcpu->pid, name);
}

/* for internal use */
static int
_msgsendbuf (int pid, int desc, int data, struct msgbuf *buf, int bufcnt)
{
	int mpid, mgen, mdesc;
	long d[2];

	d[0] = MSG_BUF;
	d[1] = data;
	spinlock_lock (&process_lock);
	mpid = process[pid].msgdsc[desc].pid;
	mgen = process[pid].msgdsc[desc].gen;
	mdesc = process[pid].msgdsc[desc].dsc;
	spinlock_unlock (&process_lock);
	return call_msgfunc1 (mpid, mgen, mdesc, d, sizeof (d), buf, bufcnt);
}

/* for kernel (VMM) */
int
msgsendbuf (int desc, int data, struct msgbuf *buf, int bufcnt)
{
	return _msgsendbuf (0, desc, data, buf, bufcnt);
}

/* si=desc di=data sp=others */
ulong
sys_msgsendbuf (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	struct msgsendbuf_args {
		int data;
		int bufcnt;
		struct msgbuf *buf;
	} *q = (struct msgsendbuf_args *)di, arg;
	struct msgbuf buf[MAXNUM_OF_MSGBUF];
	int i;

	if (!is_range_valid (di, sizeof *q))
		return (ulong)-1L;
	/* we need to copy the structure to kernel memory before
	   checking it is valid because a process may modify it at the
	   same time. */
	memcpy (&arg, q, sizeof arg);
	if (arg.bufcnt < 0)
		return (ulong)-1L;
	if (!is_range_valid ((ulong)arg.buf, sizeof *arg.buf * arg.bufcnt))
		return (ulong)-1L;
	if (arg.bufcnt > MAXNUM_OF_MSGBUF)
		return (ulong)-1L;
	memcpy (buf, arg.buf, sizeof *arg.buf * arg.bufcnt);
	for (i = 0; i < arg.bufcnt; i++) {
		if (!buf[i].len)
			return (ulong)-1L;
		if (!is_range_valid ((ulong)buf[i].base, buf[i].len))
			return (ulong)-1L;
		buf[i].premap_handle = 0;
	}
	return _msgsendbuf (currentcpu->pid, si, arg.data, buf, arg.bufcnt);
}

static int
_msgunregister (int pid, int desc)
{
	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	if (desc < 0 || desc >= NUM_OF_MSGDSC)
		return -1;
	process[pid].msgdsc[desc].func = NULL;
	return msg_unregister2 (pid, desc);
}

/* for kernel (VMM) */
int
msgunregister (int desc)
{
	return _msgunregister (0, desc);
}

/* si=desc */
ulong
sys_msgunregister (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	return _msgunregister (currentcpu->pid, (int)si);
}

void
exitprocess (int retval)
{
	panic ("exitprocess inside VMM.");
}

/* si=retval */
ulong
sys_exitprocess (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	if (!syscallstack)
		panic ("sys_exitprocess failed.");
	process[currentcpu->pid].exitflag = true;
	callret ((int)si);
	return 0;		/* Not reached. */
}

/* si=stack size di=maximum stack size */
static ulong
sys_setlimit (ulong ip, ulong sp, ulong num, ulong si, ulong di)
{
	int r = -1;
	virt_t tmp;

	spinlock_lock (&process_lock);
	if (process[currentcpu->pid].setlimit)
		goto ret;
	if (si < PAGESIZE)
		si = PAGESIZE;
	if (di < PAGESIZE)
		di = PAGESIZE;
	tmp = mm_process_map_stack (di, false, false);
	if (!tmp)
		goto ret;
	r = mm_process_unmap_stack (tmp, di);
	if (r) {
		spinlock_unlock (&process_lock);
		panic ("unmap stack failed");
	}
	process[currentcpu->pid].setlimit = true;
	process[currentcpu->pid].stacksize = si;
ret:
	spinlock_unlock (&process_lock);
	return (ulong)r;
}

long
msgpremapbuf (int desc, struct msgbuf *buf)
{
	phys_t mm_phys;
	int topid, togen;
	void *base_user = NULL;

	spinlock_lock (&process_lock);
	topid = process[0].msgdsc[desc].pid;
	togen = process[0].msgdsc[desc].gen;
	if (topid == 0)
		goto ret;
	ASSERT (topid >= 0);
	ASSERT (topid < NUM_OF_PID);
	if (!process[topid].valid)
		goto ret;
	if (process[topid].gen != togen)	
		goto ret;
	mm_phys = mm_process_switch (process[topid].mm_phys);
	base_user = mm_process_map_shared (mm_phys, buf->base, buf->len,
					   !!buf->rw, true);
	mm_process_switch (mm_phys);
ret:
	spinlock_unlock (&process_lock);
	if (base_user)
		return (long)buf->base - (long)base_user;
	else
		return 0;
}

static syscall_func_t syscall_table[NUM_OF_SYSCALLS] = {
	NULL,			/* 0 */
	sys_nop,
	NULL,
	sys_msgsetfunc,
	sys_msgregister,
	sys_msgopen,		/* 5 */
	sys_msgclose,
	sys_msgsendint,
	sys_msgret,
	sys_msgsenddesc,
	sys_newprocess,		/* 10 */
	sys_msgsendbuf,
	sys_msgunregister,
	sys_exitprocess,
	sys_setlimit,
};

__attribute__ ((regparm (1))) void
process_syscall (struct syscall_regs *regs)
{
	if (regs->rbx < NUM_OF_SYSCALLS && syscall_table[regs->rbx]) {
		regs->rax = syscall_table[regs->rbx] (regs->rdx, regs->rcx,
						      regs->rbx, regs->rsi,
						      regs->rdi);
		if (own_process64_msrs (release_process64_msrs, NULL))
			set_process64_msrs ();
		return;
	}
	printf ("Bad system call.\n");
	printf ("rax:0x%lX rbx(num):0x%lX rcx(rsp):0x%lX rdx(eip):0x%lX\n",
		regs->rax, regs->rbx, regs->rcx, regs->rdx);
	printf ("rsp:0x%lX rbp:0x%lX rsi:0x%lX rdi:0x%lX\n",
		regs->rsp, regs->rbp, regs->rsi, regs->rdi);
	process_kill (NULL, NULL);
	panic ("process_kill failed.");
}

INITFUNC ("global3", process_init_global);
INITFUNC ("ap0", process_init_ap);
INITFUNC ("wakeup0", process_wakeup);
