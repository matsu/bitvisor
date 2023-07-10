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

#include <arch/currentcpu.h>
#include <arch/process.h>
#include <arch/vmm_mem.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/process.h>
#include <core/spinlock.h>
#include <core/string.h>
#include <core/types.h>
#include "assert.h"
#include "constants.h"
#include "elf.h"
#include "initfunc.h"
#include "list.h"
#include "mm.h"
#include "msg.h"
#include "process.h"
#include "process_builtin.h"

#define NUM_OF_SYSCALLS 32
#define NAMELEN 16
#define MAX_MSGLEN 16384

struct msgdsc_data {
	int pid;
	int gen;
	int dsc;
	void *func;
};

struct process_data {
	bool valid;
	struct mm_arch_proc_desc *mm_proc_desc;
	int gen;
	int running;
	struct msgdsc_data msgdsc[NUM_OF_MSGDSC];
	bool exitflag;
	bool setlimit;
	int stacksize;
};

struct ro_segment_vec {
	phys_t phys;
	signed char size1;
} __attribute__ ((packed));

struct ro_segments {
	struct ro_segments *next;
	u32 vaddr;
	u8 veccnt;
	bool exec;
	struct ro_segment_vec vec[];
} __attribute__ ((packed));

struct rw_segment_vec {
	void *buf;
	signed char size1;
} __attribute__ ((packed));

struct rw_segments {
	struct rw_segments *next;
	u32 vaddr;
	u32 vsize;
	u8 veccnt;
	struct rw_segment_vec vec[];
} __attribute__ ((packed));

struct processbin {
	LIST1_DEFINE (struct processbin);
	struct ro_segments *ro;
	struct rw_segments *rw;
	ulong entry;
	int stacksize;
	char name[];
} __attribute__ ((packed));

static struct process_data process[NUM_OF_PID];
static spinlock_t process_lock;
static bool process_initialized = false;
static LIST1_DEFINE_HEAD (struct processbin, processbin_list);

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
sys_nop (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
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

static void
add_segment_ro_copy (virt_t srcstart, virt_t srcend, void *src,
		     virt_t start, virt_t end, void *dest)
{
	if (end <= srcstart || srcend <= start) {
		memset (dest, 0, end - start);
		return;
	}
	/* end > srcstart && srcend > start */
	if (srcstart > start) {
		memset (dest, 0, srcstart - start);
		dest += srcstart - start;
		start = srcstart;
	}
	/* start >= srcstart */
	if (end <= srcend) {
		memcpy (dest, src + (start - srcstart), end - start);
		return;
	}
	/* end > srcend */
	memcpy (dest, src + (start - srcstart), srcend - start);
	dest += srcend - start;
	start = srcend;
	memset (dest, 0, end - start);
}

static unsigned int
get_vec_size (signed char size1)
{
	return size1 < 0 ? -size1 : 1 << size1;
}

static void
add_segment_ro (virt_t destvirt, uint destlen, void *src, uint srclen,
		bool exec, struct ro_segments ***next)
{
	if (srclen > destlen)
		srclen = destlen;
	virt_t destend = destvirt + destlen;
	virt_t destsrcstart = destvirt;
	virt_t destsrcend = destsrcstart + srclen;
	if (destvirt & PAGESIZE_MASK)
		destvirt -= destvirt & PAGESIZE_MASK;
	if (destend & PAGESIZE_MASK)
		destend += PAGESIZE - (destend & PAGESIZE_MASK);
	struct ro_segments *p = alloc (sizeof *p);
	p->vaddr = destvirt;
	p->next = NULL;
	int veccnt = 0;
	while (destvirt < destend) {
		signed char size1 = PAGESIZE_SHIFT;
		while (get_vec_size (size1) * 2 <= destend - destvirt)
			size1++;
		unsigned int len = get_vec_size (size1);
		phys_t phys;
		void *buf;
		alloc_pages (&buf, &phys, len / PAGESIZE);
		add_segment_ro_copy (destsrcstart, destsrcend, src, destvirt,
				     destvirt + len, buf);
		p = realloc (p, sizeof *p + sizeof (p->vec[0]) * (veccnt + 1));
		p->vec[veccnt].phys = phys;
		p->vec[veccnt].size1 = size1;
		veccnt++;
		destvirt += len;
	}
	p->veccnt = veccnt;
	p->exec = exec;
	**next = p;
	*next = &p->next;
}

static void
add_segment_rw (virt_t destvirt, uint destlen, void *src, uint srclen,
		struct rw_segments ***next)
{
	if (srclen > destlen)
		srclen = destlen;
	struct rw_segments *p = alloc (sizeof *p);
	p->vaddr = destvirt;
	p->vsize = destlen;
	p->next = NULL;
	int veccnt = 0;
	while (srclen > 0) {
		signed char size1;
		if (srclen <= 64) {
			size1 = -srclen;
		} else {
			size1 = 6;
			while (get_vec_size (size1) * 2 <= srclen)
				size1++;
		}
		unsigned int len = get_vec_size (size1);
		void *buf = alloc (len);
		memcpy (buf, src, len);
		p = realloc (p, sizeof *p + sizeof (p->vec[0]) * (veccnt + 1));
		p->vec[veccnt].buf = buf;
		p->vec[veccnt].size1 = size1;
		veccnt++;
		src += len;
		srclen -= len;
	}
	p->veccnt = veccnt;
	**next = p;
	*next = &p->next;
}

struct processbin *
processbin_add (char *name, void *bin, ulong len, int stacksize)
{
	u8 *b;
	u8 *phdrb;
	ELF_EHDR *ehdr;
	ELF_PHDR *phdr;
	unsigned int i;
	struct processbin *pb;
	struct ro_segments *ro = NULL;
	struct ro_segments **ro_next = &ro;
	struct rw_segments *rw = NULL;
	struct rw_segments **rw_next = &rw;
	virt_t proc_end_virt;

	b = bin;
	if (sizeof *ehdr > len)
		return NULL;
	if (b[0] != 0x7F && b[1] != 'E' && b[2] != 'L' && b[3] != 'F')
		return NULL;
	ehdr = bin;
	phdrb = b + ehdr->e_phoff;
	phdr = (ELF_PHDR *)phdrb;
	proc_end_virt = vmm_mem_proc_end_virt ();
	for (i = ehdr->e_phnum; i && phdrb - b + sizeof *phdr <= len;
	     i--, phdr = (ELF_PHDR *)(phdrb += ehdr->e_phentsize)) {
		if (phdr->p_type != PT_LOAD)
			continue;
		if (phdr->p_vaddr >= proc_end_virt)
			continue;
		if (phdr->p_vaddr + phdr->p_memsz > proc_end_virt)
			continue;
		if (phdr->p_offset >= len)
			continue;
		if (phdr->p_offset + phdr->p_filesz > len)
			continue;
		if (phdr->p_flags & PF_W)
			add_segment_rw (phdr->p_vaddr, phdr->p_memsz,
					b + phdr->p_offset, phdr->p_filesz,
					&rw_next);
		else
			add_segment_ro (phdr->p_vaddr, phdr->p_memsz,
					b + phdr->p_offset, phdr->p_filesz,
					!!(phdr->p_flags & PF_X), &ro_next);
	}
	int namelen = strlen (name) + 1;
	pb = alloc (sizeof *pb + namelen);
	memcpy (pb->name, name, namelen);
	pb->ro = ro;
	pb->rw = rw;
	pb->entry = ehdr->e_entry;
	pb->stacksize = stacksize;
	LIST1_ADD (processbin_list, pb);
	return pb;
}

static void
builtin_loadall (void)
{
	struct process_builtin_data *p;

	for (p = __process_builtin; p != __process_builtin_end; p++)
		processbin_add (p->name, p->bin, p->len, p->stacksize);
}

static void
builtin_free (void)
{
	struct process_builtin_data *p;
	void *start = NULL;
	void *end = NULL;

	for (p = __process_builtin; p != __process_builtin_end; p++) {
		if (end != p->name) {
			if (end)
				free_pages_range (start, end);
			start = p->name;
		}
		end = p->name + strlen (p->name) + 1;
		if (end != p->bin) {
			free_pages_range (start, end);
			start = p->bin;
		}
		end = p->bin + p->len;
	}
	if (end)
		free_pages_range (start, end);
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
	process_arch_setup ();
	spinlock_init (&process_lock);
	LIST1_HEAD_INIT (processbin_list);
	process_initialized = true;
	builtin_loadall ();
	builtin_free ();
}

static ulong
processbin_load (struct processbin *bin,
		 struct mm_arch_proc_desc *mm_proc_desc)
{
	for (struct rw_segments *p = bin->rw; p; p = p->next) {
		ulong vaddr = p->vaddr;
		mm_process_map_alloc (mm_proc_desc, vaddr, p->vsize);
		for (u32 i = 0; i < p->veccnt; i++) {
			unsigned int len = get_vec_size (p->vec[i].size1);
			memcpy ((void *)vaddr, p->vec[i].buf, len);
			vaddr += len;
		}
	}
	for (struct ro_segments *p = bin->ro; p; p = p->next) {
		ulong vaddr = p->vaddr;
		for (int i = 0; i < p->veccnt; i++) {
			phys_t phys = p->vec[i].phys;
			unsigned int len = get_vec_size (p->vec[i].size1);
			u32 size = len / PAGESIZE;
			bool exec = p->exec;
			for (u32 i = 0; i < size; i++) {
				mm_process_map_shared_physpage (mm_proc_desc,
								vaddr, phys,
								false, exec);
				vaddr += PAGESIZE;
				phys += PAGESIZE;
			}
		}
	}
	return bin->entry;
}

/* for internal use */
static int
_msgopen_2 (int pid, int mpid, int mgen, int mdesc)
{
	int i, r;

	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
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
process_new (int frompid, struct processbin *bin)
{
	int pid, gen;
	ulong rip;
	struct mm_arch_proc_desc *mm_proc_desc, *old_mm_proc_desc;
	int stacksize = bin->stacksize;

	spinlock_lock (&process_lock);
	for (pid = 1; pid < NUM_OF_PID; pid++) {
		if (!process[pid].valid)
			goto found;
	}
err:
	spinlock_unlock (&process_lock);
	return -1;
found:
	/* alloc page directories and init */
	if (mm_process_alloc (&mm_proc_desc, pid) < 0)
		goto err;
	process[pid].mm_proc_desc = mm_proc_desc;
	process[pid].running = 0;
	process[pid].exitflag = false;
	process[pid].setlimit = false;
	process[pid].stacksize = PAGESIZE;
	if (stacksize > PAGESIZE)
		process[pid].stacksize = stacksize;
	gen = ++process[pid].gen;
	process[pid].valid = true;
	clearmsgdsc (process[pid].msgdsc);
	old_mm_proc_desc = mm_process_switch (mm_proc_desc);
	/* load a program */
	if (!(rip = processbin_load (bin, mm_proc_desc))) {
		printf ("processbin_load failed.\n");
		process[pid].valid = false;
		pid = 0;
	}
	/* for system calls */
	mm_process_map_shared_physpage (mm_proc_desc, 0x3FFFF000,
					processuser_arch_syscall_phys (),
					false, true);
	process[pid].msgdsc[0].func = (void *)rip;
	mm_process_switch (old_mm_proc_desc);
	spinlock_unlock (&process_lock);
	return _msgopen_2 (frompid, pid, gen, 0);
}

void
process_kill (bool (*func) (void *data), void *data)
{
	if (!process_initialized) /* process not ready. */
		return;
	if (!process_arch_syscallstack_ok ())	/* can't kill */
		return;
	if (func && !func (data))
		return;
	process[currentcpu_get_pid ()].exitflag = true;
	process_arch_callret (-1);
}

/* free any resources of a process */
/* Page table must be the process's one */
/* process_lock must be locked */
static void
cleanup (int pid, struct mm_arch_proc_desc *mm_proc_desc_callee,
	 struct mm_arch_proc_desc *mm_proc_desc_caller)
{
	ASSERT (process[pid].valid);
	ASSERT (process[pid].running == 0);
	process[pid].gen++;
	msg_unregisterall (pid);
	mm_process_unmapall (mm_proc_desc_callee);
	mm_process_switch (mm_proc_desc_caller);
	mm_process_free (mm_proc_desc_callee);
	process[pid].valid = false;
	process[pid].mm_proc_desc = NULL;
}

/* pid, func=pointer to the function of the process,
   sp=stack pointer of the process */
/* process_lock must be locked */
static int
call_msgfunc0 (int pid, void *func, ulong sp, void *arg, int len,
	       ulong buf, int bufcnt)
{
	int oldpid, ret;

	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	ASSERT (process[pid].valid);
	if (pid == 0) {
		panic ("call_msgfunc0 can't call kernel");
	}
	oldpid = currentcpu_get_pid ();
	currentcpu_set_pid (pid);
	process[pid].running++;
	spinlock_unlock (&process_lock);
	ret = process_arch_exec (func, sp, arg, len, buf, bufcnt);
	spinlock_lock (&process_lock);
	process[pid].running--;
	currentcpu_set_pid (oldpid);
	return ret;
}

/* pid, gen, desc, arg=arguments, len=length of the arguments (bytes) */
static int
call_msgfunc1 (int pid, int gen, int desc, void *arg, int len,
	       struct msgbuf *buf, int bufcnt)
{
	struct mm_arch_proc_desc *mm_proc_desc_callee, *mm_proc_desc_caller;
	ulong sp, sp2;
	int r = -1;
	struct msgbuf buf_user[MAXNUM_OF_MSGBUF];
	int (*func) (int, int, struct msgbuf *, int);
	int i;
	long tmp;
	int stacksize;

	if (currentcpu_vmm_stack_full ()) {
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
	mm_proc_desc_callee = process[pid].mm_proc_desc;
	mm_proc_desc_caller = mm_process_switch (mm_proc_desc_callee);
	for (i = 0; i < bufcnt; i++) {
		if (buf[i].premap_handle) {
			tmp = (long)buf[i].base - buf[i].premap_handle;
			buf_user[i].base = (void *)tmp;
		} else {
			buf_user[i].base = mm_process_map_shared
					   (mm_proc_desc_callee,
					    mm_proc_desc_caller, buf[i].base,
					    buf[i].len, !!buf[i].rw, false);
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
	sp2 = mm_process_map_stack (mm_proc_desc_callee, stacksize,
				    process[pid].setlimit, true);
	if (!sp2) {
		printf ("cannot allocate stack for process\n");
		goto mapfail;
	}
	sp = sp2;
	for (i = bufcnt; i-- > 0;) {
		sp -= sizeof buf_user[i];
		memcpy ((void *)sp, &buf_user[i], sizeof buf_user[i]);
	}
	r = call_msgfunc0 (pid, process[pid].msgdsc[desc].func, sp, arg, len,
			   sp /* sp is currently pointing buf */, bufcnt);
	mm_process_unmap_stack (mm_proc_desc_callee, sp2, stacksize);
mapfail:
	for (i = 0; i < bufcnt; i++) {
		if (buf[i].premap_handle)
			continue;
		mm_process_unmap (mm_proc_desc_callee,
				  (virt_t)buf_user[i].base, buf_user[i].len);
	}
	if (process[pid].running == 0 && process[pid].exitflag)
		cleanup (pid, mm_proc_desc_callee, mm_proc_desc_caller);
	mm_process_switch (mm_proc_desc_caller);
ret:
	spinlock_unlock (&process_lock);
	return r;
}

/* for internal use */
static void *
_msgsetfunc (int pid, int desc, void *func)
{
	void *oldfunc;

	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	ASSERT (desc >= 0);
	ASSERT (desc < NUM_OF_MSGDSC);
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

/* arg0=desc arg1=func */
ulong
sys_msgsetfunc (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	int pid;

	if (!is_range_valid (arg1, 1))
		return (ulong)-1L;
	pid = currentcpu_get_pid ();
	if (process[pid].setlimit)
		return (ulong)-1L;
	if (arg0 >= 0 && arg0 < NUM_OF_MSGDSC)
		return (ulong)_msgsetfunc (pid, arg0, (void *)arg1);
	return 0;
}

/* for internal use*/
static int
_msgregister (int pid, char *name, void *func)
{
	int r, i;

	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	spinlock_lock (&process_lock);
	for (i = 0; i < NUM_OF_MSGDSC; i++) {
		if (!process[pid].msgdsc[i].func)
			goto found;
	}
	r = -1;
	goto ret;
found:
	process[pid].msgdsc[i].func = func;
	if (name && msg_register (name, pid, process[pid].gen, i) < 0) {
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

/* arg0=name arg1=func */
ulong
sys_msgregister (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	char name[MSG_NAMELEN];
	char *pname = NULL;
	int pid;

	if (arg0 && !is_range_valid (arg0, MSG_NAMELEN))
		return (ulong)-1L;
	if (!is_range_valid (arg1, 1))
		return (ulong)-1L;
	pid = currentcpu_get_pid ();
	if (process[pid].setlimit)
		return (ulong)-1L;
	if (arg0) {
		snprintf (name, sizeof name, "%s", (char *)arg0);
		name[MSG_NAMELEN - 1] = '\0';
		pname = name;
	}
	return _msgregister (pid, pname, (void *)arg1);
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

/* arg0=name */
ulong
sys_msgopen (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	char name[MSG_NAMELEN];
	int pid;

	if (!is_range_valid (arg0, MSG_NAMELEN))
		return (ulong)-1L;
	pid = currentcpu_get_pid ();
	if (process[pid].setlimit)
		return (ulong)-1L;
	snprintf (name, sizeof name, "%s", (char *)arg0);
	name[MSG_NAMELEN - 1] = '\0';
	return _msgopen (pid, name);
}

/* for internal use */
static int
_msgclose (int pid, int desc)
{
	if (desc >= 0 && desc < NUM_OF_MSGDSC) {
		ASSERT (pid >= 0);
		ASSERT (pid < NUM_OF_PID);
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

/* arg0=desc */
ulong
sys_msgclose (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	return _msgclose (currentcpu_get_pid (), arg0);
}

/* for internal use */
static int
_msgsendint (int pid, int desc, int data)
{
	int mpid, mgen, mdesc;
	long d[2];

	d[0] = MSG_INT;
	d[1] = data;
	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	ASSERT (desc >= 0);
	ASSERT (desc < NUM_OF_MSGDSC);
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

/* arg0=desc arg1=data */
ulong
sys_msgsendint (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	if (arg0 >= NUM_OF_MSGDSC)
		return (ulong)-1L;
	return _msgsendint (currentcpu_get_pid (), arg0, arg1);
}

/* arg0=retval */
ulong
sys_msgret (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	process_arch_callret ((int)arg0);
	return 0;		/* Not reached. */
}

/* for internal use */
static int
_msgsenddesc (int frompid, int todesc, int senddesc)
{
	int topid, togen, i, r = -1;
	int mpid, mgen, mdesc;
	int myfunc = 0;
	int todsc;

	if (senddesc & MSGSENDDESC_MYFUNC) {
		senddesc -= MSGSENDDESC_MYFUNC;
		myfunc = 1;
	}
	if (senddesc < 0 || senddesc >= NUM_OF_MSGDSC)
		return -1;
	if (todesc < 0 || todesc >= NUM_OF_MSGDSC)
		return -1;
	if (frompid < 0 || frompid >= NUM_OF_PID)
		return -1;
	spinlock_lock (&process_lock);
	topid = process[frompid].msgdsc[todesc].pid;
	togen = process[frompid].msgdsc[todesc].gen;
	todsc = process[frompid].msgdsc[todesc].dsc;
	ASSERT (topid >= 0);
	ASSERT (topid < NUM_OF_PID);
	if (!process[topid].valid)
		goto ret;
	if (process[topid].gen != togen)
		goto ret;
	if (!topid || todsc)
		goto ret;
	for (i = 0; i < NUM_OF_MSGDSCRECV; i++) {
		if (process[topid].msgdsc[i].pid == 0 &&
		    process[topid].msgdsc[i].gen == 0)
			goto found;
	}
	goto ret;
found:
	if (myfunc) {
		mpid = frompid;
		mgen = process[frompid].gen;
		mdesc = senddesc;
	} else {
		mpid = process[frompid].msgdsc[senddesc].pid;
		mgen = process[frompid].msgdsc[senddesc].gen;
		mdesc = process[frompid].msgdsc[senddesc].dsc;
	}
	process[topid].msgdsc[i].pid = mpid;
	process[topid].msgdsc[i].gen = mgen;
	process[topid].msgdsc[i].dsc = mdesc;
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

/* arg0=desc arg1=data */
ulong
sys_msgsenddesc (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	int pid = currentcpu_get_pid ();
	if (process[pid].setlimit)
		return (ulong)-1L;
	return _msgsenddesc (pid, arg0, arg1);
}

static int
_newprocess (int frompid, char *name)
{
	struct processbin *bin;
	LIST1_FOREACH (processbin_list, bin)
		if (!strcmp (bin->name, name))
			return process_new (frompid, bin);
	return -1;
}

int
newprocess (char *name)
{
	return _newprocess (0, name);
}

/* arg0=name */
ulong
sys_newprocess (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	char name[PROCESS_NAMELEN];
	int pid;

	if (!is_range_valid (arg0, 1))
		return (ulong)-1L;
	pid = currentcpu_get_pid ();
	if (process[pid].setlimit)
		return (ulong)-1L;
	snprintf (name, sizeof name, "%s", (char *)arg0);
	name[PROCESS_NAMELEN - 1] = '\0';
	return (ulong)_newprocess (pid, name);
}

/* for internal use */
static int
_msgsendbuf (int pid, int desc, int data, struct msgbuf *buf, int bufcnt)
{
	int mpid, mgen, mdesc;
	long d[2];

	ASSERT (pid >= 0);
	ASSERT (pid < NUM_OF_PID);
	ASSERT (desc >= 0);
	ASSERT (desc < NUM_OF_MSGDSC);
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

/* arg0=desc arg1=data sp=others */
ulong
sys_msgsendbuf (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	struct msgsendbuf_args {
		int data;
		int bufcnt;
		struct msgbuf *buf;
	} *q = (struct msgsendbuf_args *)arg1, arg;
	struct msgbuf buf[MAXNUM_OF_MSGBUF];
	int i;

	if (!is_range_valid (arg1, sizeof *q))
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
	if (arg0 >= NUM_OF_MSGDSC)
		return (ulong)-1L;
	memcpy (buf, arg.buf, sizeof *arg.buf * arg.bufcnt);
	for (i = 0; i < arg.bufcnt; i++) {
		if (!buf[i].len)
			return (ulong)-1L;
		if (!is_range_valid ((ulong)buf[i].base, buf[i].len))
			return (ulong)-1L;
		buf[i].premap_handle = 0;
	}
	return _msgsendbuf (currentcpu_get_pid (), arg0, arg.data, buf,
			    arg.bufcnt);
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

/* arg0=desc */
ulong
sys_msgunregister (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	return _msgunregister (currentcpu_get_pid (), (int)arg0);
}

void
exitprocess (int retval)
{
	panic ("exitprocess inside VMM.");
}

/* arg0=retval */
ulong
sys_exitprocess (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	if (!process_arch_syscallstack_ok ())
		panic ("sys_exitprocess failed.");
	process[currentcpu_get_pid ()].exitflag = true;
	process_arch_callret ((int)arg0);
	return 0;		/* Not reached. */
}

/* arg0=stack size arg1=maximum stack size */
static ulong
sys_setlimit (ulong ip, ulong sp, ulong num, ulong arg0, ulong arg1)
{
	int r = -1;
	int pid;
	virt_t tmp;
	struct mm_arch_proc_desc *mm_proc_desc;

	spinlock_lock (&process_lock);
	pid = currentcpu_get_pid ();
	if (process[pid].setlimit)
		goto ret;
	if (arg0 < PAGESIZE)
		arg0 = PAGESIZE;
	if (arg1 < PAGESIZE)
		arg1 = PAGESIZE;
	mm_proc_desc = process[pid].mm_proc_desc;
	tmp = mm_process_map_stack (mm_proc_desc, arg1, false, false);
	if (!tmp)
		goto ret;
	r = mm_process_unmap_stack (mm_proc_desc, tmp, arg1);
	if (r) {
		spinlock_unlock (&process_lock);
		panic ("unmap stack failed");
	}
	process[pid].setlimit = true;
	process[pid].stacksize = arg0;
ret:
	spinlock_unlock (&process_lock);
	return (ulong)r;
}

long
msgpremapbuf (int desc, struct msgbuf *buf)
{
	struct mm_arch_proc_desc *mm_proc_desc_callee, *mm_proc_desc_caller;
	int topid, togen;
	void *base_user = NULL;

	ASSERT (desc >= 0);
	ASSERT (desc < NUM_OF_MSGDSC);
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
	mm_proc_desc_callee = process[topid].mm_proc_desc;
	mm_proc_desc_caller = mm_process_switch (mm_proc_desc_callee);
	base_user = mm_process_map_shared (mm_proc_desc_callee,
					   mm_proc_desc_caller, buf->base,
					   buf->len, !!buf->rw, false);
	mm_process_switch (mm_proc_desc_caller);
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

syscall_func_t
process_get_syscall_func (uint num)
{
	if (num < NUM_OF_SYSCALLS && syscall_table[num])
		return syscall_table[num];
	return NULL;
}

INITFUNC ("global3", process_init_global);
