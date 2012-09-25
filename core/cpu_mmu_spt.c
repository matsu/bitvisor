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

/* MMU emulation, Shadow Page Tables (SPT) */

#include "acpi.h"
#include "asm.h"
#include "constants.h"
#include "cpu_mmu.h"
#include "cpu_mmu_spt.h"
#include "current.h"
#include "initfunc.h"
#include "mm.h"
#include "mmio.h"
#include "panic.h"
#include "pcpu.h"
#include "printf.h"
#include "spinlock.h"
#include "string.h"
#include "vmmcall_status.h"

struct map_page_data1 {
	unsigned int write : 1;
	unsigned int user : 1;
	unsigned int wp : 1;
	unsigned int pwt : 1;
	unsigned int pcd : 1;
	unsigned int pat : 1;
};

struct map_page_data2 {
	unsigned int rw : 1;
	unsigned int us : 1;
	unsigned int nx : 1;
};

#define GFN_GLOBAL	0xFFFFFFFFFFFFFFFEULL
#define GFN_UNUSED	0xFFFFFFFFFFFFFFFFULL

static bool guest64 (void);
static void update_cr3 (void);
static void invalidate_page (ulong virtual_addr);
static void map_page (u64 v, struct map_page_data1 m1,
		      struct map_page_data2 m2[5], u64 gfns[5], int glvl);
static bool extern_mapsearch (struct vcpu *p, phys_t start, phys_t end);
static void init_global (void);
static void init_pcpu (void);
static int init_vcpu (void);

#ifdef CPU_MMU_SPT_1
static void
get_cr0_cr3_cr4_and_efer (ulong *cr0, ulong *cr3, ulong *cr4, u64 *efer)
{
	current->vmctl.read_control_reg (CONTROL_REG_CR0, cr0);
	current->vmctl.read_control_reg (CONTROL_REG_CR3, cr3);
	current->vmctl.read_control_reg (CONTROL_REG_CR4, cr4);
	current->vmctl.read_msr (MSR_IA32_EFER, efer);
}

static void
invalidate_page (ulong v)
{
	pmap_t p;
	u64 tmp;

	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	pmap_seek (&p, v, 2);
	tmp = pmap_read (&p);
	if (tmp & PDE_P_BIT) {
		if (tmp & PDE_AVAILABLE1_BIT) {
			pmap_write (&p, tmp & ~PDE_AVAILABLE1_BIT, 0xFFF);
			pmap_setlevel (&p, 1);
			pmap_clear (&p);
			if (current->spt.levels >= 3) {
				pmap_seek (&p, v ^ PAGESIZE2M, 2);
				tmp = pmap_read (&p);
				if (tmp & PDE_P_BIT) {
					pmap_write (&p,
						    tmp & ~PDE_AVAILABLE1_BIT,
						    0xFFF);
					pmap_setlevel (&p, 1);
					pmap_clear (&p);
				}
			}
		} else {
			pmap_setlevel (&p, 1);
			tmp = pmap_read (&p);
			pmap_write (&p, 0, 0xFFF);
		}
	}
	pmap_close (&p);
}

static void
map_page (u64 v, struct map_page_data1 m1, struct map_page_data2 m2[5],
	  u64 gfns[5], int glvl)
{
	u64 hphys;
	bool fakerom;
	pmap_t p;
	u64 tmp;
	int l;
	ulong cr0;

	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr0);
	if (!(cr0 & CR0_WP_BIT)) {
		if (!m1.user && m1.write) {
			m2[0].us = 0;
			m2[0].rw = 1;
		}
	}
	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	pmap_seek (&p, v, 1);
	tmp = pmap_read (&p);
	for (; (l = pmap_getreadlevel (&p)) > 1; tmp = pmap_read (&p)) {
		pmap_setlevel (&p, l);
		if (current->spt.cnt == NUM_OF_SPTTBL) {
			pmap_setlevel (&p, current->spt.levels);
			pmap_clear (&p);
			current->spt.cnt = 0;
			continue;
		}
		pmap_write (&p, current->spt.tbl_phys[current->spt.cnt++] |
			    PDE_P_BIT, PDE_P_BIT);
		pmap_setlevel (&p, l - 1);
		pmap_clear (&p);
		pmap_setlevel (&p, 1);
	}
	if (glvl > 1 && (gfns[1] == GFN_GLOBAL || gfns[1] == GFN_UNUSED)) {
		pmap_setlevel (&p, 2);
		tmp = pmap_read (&p);
		if (!(tmp & PDE_AVAILABLE1_BIT))
			pmap_write (&p, tmp | PDE_AVAILABLE1_BIT, 0xFFF);
		pmap_setlevel (&p, 1);
	}
	hphys = current->gmm.gp2hp (gfns[0] << PAGESIZE_SHIFT, &fakerom);
	if (fakerom && m1.write)
		panic ("Writing to VMM memory.");
	if (fakerom)
		m2[0].rw = 0;
	pmap_write (&p, hphys | PTE_P_BIT |
		    (m2[0].rw ? PTE_RW_BIT : 0) |
		    (m2[0].us ? PTE_US_BIT : 0) |
		    (m2[0].nx ? PTE_NX_BIT : 0) |
		    (m1.pwt ? PTE_PWT_BIT : 0) |
		    (m1.pcd ? PTE_PCD_BIT : 0) |
		    (m1.pat ? PTE_PAT_BIT : 0),
		    PTE_P_BIT | PTE_RW_BIT | PTE_US_BIT |
		    PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT);
	pmap_close (&p);
}

static bool
extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
#ifdef CPU_MMU_SPT_USE_PAE
	u64 *e, tmp, mask = p->pte_addr_mask;
	unsigned int n = 512;
#else
	u32 *e, tmp, mask = PTE_ADDR_MASK;
	unsigned int n = 1024;
#endif
	unsigned int i, j;

	start &= ~PAGESIZE_MASK;
	end |= PAGESIZE_MASK;
	for (i = 0; i < p->spt.cnt; i++) {
		e = p->spt.tbl[i];
		for (j = 0; j < n; j++) {
			tmp = e[j] & mask;
			if ((e[j] & PTE_P_BIT) && start <= tmp && tmp <= end)
				return true;
		}
	}
	return false;
}

static void
update_cr3 (void)
{
	pmap_t p;

#ifdef CPU_MMU_SPT_USE_PAE
	current->spt.levels = guest64 () ? 4 : 3;
#else
	current->spt.levels = 2;
#endif
	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	pmap_clear (&p);
	pmap_close (&p);
	current->spt.cnt = 0;
	current->vmctl.spt_setcr3 (current->spt.cr3tbl_phys);
}

static void
clear_all (void)
{
	pmap_t p;

	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	pmap_clear (&p);
	pmap_close (&p);
	current->spt.cnt = 0;
	current->vmctl.spt_setcr3 (current->spt.cr3tbl_phys);
}

static bool
spt_tlbflush (void)
{
	return true;
}

static void
init_global (void)
{
}

static void
init_pcpu (void)
{
}

static int
init_vcpu (void)
{
	int i;

	alloc_page (&current->spt.cr3tbl, &current->spt.cr3tbl_phys);
	for (i = 0; i < NUM_OF_SPTTBL; i++)
		alloc_page (&current->spt.tbl[i], &current->spt.tbl_phys[i]);
	current->spt.cnt = 0;
	memset (current->spt.cr3tbl, 0, PAGESIZE);
	return 0;
}
#endif /* CPU_MMU_SPT_1 */

#ifdef CPU_MMU_SPT_2
#include "list.h"

/* key:
   bit 63-12: gfn
   bit 11,10: 2to3
   bit 9-7:   glevels (not largepage)
   bit 6-4:   level */

#define KEY_GFNMASK	0xFFFFFFFFFFFFF000ULL
#define KEY_GFN_SHIFT	12
#define KEY_2TO3_SHIFT	10
#define KEY_LARGEPAGE	0x40
#define KEY_GLVL2	(2 << 7)
#define KEY_GLVL3	(3 << 7)
#define KEY_GLVL4	(4 << 7)
#define KEY_LPMASK	0xFFFFFFFFFFE00000ULL

struct sptlist {
	LIST1_DEFINE (struct sptlist);
	struct cpu_mmu_spt_data *spt;
};

static LIST1_DEFINE_HEAD (struct sptlist, list1_spt);

static u32 stat_mapcnt = 0;
static u32 stat_cr3cnt = 0;
static u32 stat_invlpgcnt = 0;
static u32 stat_wpcnt = 0;
static u32 stat_ptfoundcnt = 0;
static u32 stat_ptfullcnt = 0;
static u32 stat_ptnewcnt = 0;
static u32 stat_pdfoundcnt = 0;
static u32 stat_pdfullcnt = 0;
static u32 stat_pdnewcnt = 0;
static u32 stat_ptgoodcnt = 0;
static u32 stat_pthitcnt = 0;
static u32 stat_ptnew2cnt = 0;
static u32 stat_pdgoodcnt = 0;
static u32 stat_pdhitcnt = 0;
static u32 stat_pdnew2cnt = 0;

static void
get_cr0_cr3_cr4_and_efer (ulong *cr0, ulong *cr3, ulong *cr4, u64 *efer)
{
	current->vmctl.read_control_reg (CONTROL_REG_CR0, cr0);
	current->vmctl.read_control_reg (CONTROL_REG_CR3, cr3);
	current->vmctl.read_control_reg (CONTROL_REG_CR4, cr4);
	current->vmctl.read_msr (MSR_IA32_EFER, efer);
}

static void
invalidate_page (ulong v)
{
	pmap_t p;
	u64 tmp;

	STATUS_UPDATE (asm_lock_incl (&stat_invlpgcnt));
	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	pmap_seek (&p, v, 2);
	tmp = pmap_read (&p);
	if (tmp & PDE_P_BIT) {
		if (tmp & PDE_AVAILABLE1_BIT) {
			pmap_write (&p, 0, 0xFFF);
			if (current->spt.levels >= 3) {
				pmap_seek (&p, v ^ PAGESIZE2M, 2);
				tmp = pmap_read (&p);
				if (tmp & PDE_P_BIT)
					pmap_write (&p, 0, 0xFFF);
			}
		} else {
			pmap_setlevel (&p, 1);
			tmp = pmap_read (&p);
			pmap_write (&p, 0, 0xFFF);
		}
	}
	pmap_close (&p);
}

static u64
pte_and (u64 *pte, u64 mask)
{
	u64 oldpte = 0, newpte = 0;

	while (asm_lock_cmpxchgq (pte, &oldpte, newpte))
		newpte = oldpte & mask;
	return newpte;
}

static bool
update_rwmap (u64 gfn, void *pte)
{
	unsigned int i, j, k, kk;
	bool r = false;

	spinlock_lock (&current->spt.rwmap_lock);
	i = current->spt.rwmap_fail;
	j = current->spt.rwmap_normal;
	k = current->spt.rwmap_free;
	if (i != j)
		r = true;
	while (i != j) {
		pte_and (current->spt.rwmap[i].pte, ~(PTE_D_BIT | PTE_RW_BIT));
		i = (i + 1) % NUM_OF_SPTRWMAP;
	}
	/* i == j */
	if (pte != NULL) {
		while (j != k) {
			if (current->spt.rwmap[j].gfn == gfn &&
			    current->spt.rwmap[j].pte == pte)
				goto skip;
			j = (j + 1) % NUM_OF_SPTRWMAP;
		}
		kk = (k + 1) % NUM_OF_SPTRWMAP;
		if (kk == i) {
			pte_and (current->spt.rwmap[i].pte,
				 ~(PTE_D_BIT | PTE_RW_BIT));
			i = j = (i + 1) % NUM_OF_SPTRWMAP;
			current->spt.rwmap_normal = j;
		}
		current->spt.rwmap[k].gfn = gfn;
		current->spt.rwmap[k].pte = pte;
		current->spt.rwmap_free = kk;
	}
skip:
	current->spt.rwmap_fail = i;
	spinlock_unlock (&current->spt.rwmap_lock);
	return r;
}

static void
swap_shadow (struct cpu_mmu_spt_shadow *shadow, unsigned int i, unsigned int j)
{
	struct cpu_mmu_spt_shadow shadowi, shadowj;

	shadowi.virt = shadow[i].virt;
	shadowi.phys = shadow[i].phys;
	shadowi.key = shadow[i].key;
	shadowi.cleared = shadow[i].cleared;
	shadowj.virt = shadow[j].virt;
	shadowj.phys = shadow[j].phys;
	shadowj.key = shadow[j].key;
	shadowj.cleared = shadow[j].cleared;
	shadow[i].virt = shadowj.virt;
	shadow[i].phys = shadowj.phys;
	shadow[i].key = shadowj.key;
	shadow[i].cleared = shadowj.cleared;
	shadow[j].virt = shadowi.virt;
	shadow[j].phys = shadowi.phys;
	shadow[j].key = shadowi.key;
	shadow[j].cleared = shadowi.cleared;
}

static void
swap_rwmap (struct cpu_mmu_spt_rwmap *rwmap, unsigned int i, unsigned int j)
{
	struct cpu_mmu_spt_rwmap rwmapi, rwmapj;

	rwmapi.gfn = rwmap[i].gfn;
	rwmapi.pte = rwmap[i].pte;
	rwmapj.gfn = rwmap[j].gfn;
	rwmapj.pte = rwmap[j].pte;
	rwmap[i].gfn = rwmapj.gfn;
	rwmap[i].pte = rwmapj.pte;
	rwmap[j].gfn = rwmapi.gfn;
	rwmap[j].pte = rwmapi.pte;
}

static void
update_shadow (u64 gfn0, u64 *pte, bool needrw)
{
	struct cpu_mmu_spt_data *spt;
	struct sptlist *listspt;
	bool rw = true;
	int i, j;
	u64 key, keytmp;

	key = gfn0 << KEY_GFN_SHIFT;
	LIST1_FOREACH (list1_spt, listspt) {
		spt = listspt->spt;
		spinlock_lock (&spt->shadow1_lock);
		j = spt->shadow1_normal;
		for (i = j; i != spt->shadow1_free;
		     i = (i + 1) % NUM_OF_SPTSHADOW1) {
			keytmp = spt->shadow1[i].key;
			if (keytmp & KEY_LARGEPAGE)
				continue;
			if ((keytmp & KEY_GFNMASK) != key)
				continue;
			if (!needrw) {
				rw = false;
				continue;
			}
			if (i != j)
				swap_shadow (spt->shadow1, i, j);
			j = (j + 1) % NUM_OF_SPTSHADOW1;
		}
		spt->shadow1_normal = j;
		spinlock_unlock (&spt->shadow1_lock);
		spinlock_lock (&spt->shadow2_lock);
		j = spt->shadow2_normal;
		for (i = j; i != spt->shadow2_free;
		     i = (i + 1) % NUM_OF_SPTSHADOW2) {
			keytmp = spt->shadow2[i].key;
			if (keytmp & KEY_LARGEPAGE)
				continue;
			if ((keytmp & KEY_GFNMASK) != key)
				continue;
			if (!needrw) {
				rw = false;
				continue;
			}
			if (i != j)
				swap_shadow (spt->shadow2, i, j);
			j = (j + 1) % NUM_OF_SPTSHADOW2;
		}
		spt->shadow2_normal = j;
		spinlock_unlock (&spt->shadow2_lock);
	}
	if (!rw)
		pte_and (pte, ~PTE_RW_BIT);
}

static bool
is_shadow1_pde_ok (u64 pde, u64 key)
{
	unsigned int i, j;
	u64 phys;

	i = current->spt.shadow1_modified;
	j = current->spt.shadow1_free;
	phys = pde & current->pte_addr_mask;
	while (i != j) {
		if (key == current->spt.shadow1[i].key &&
		    phys == current->spt.shadow1[i].phys) {
			current->spt.shadow1[i].cleared = false;
			return true;
		}
		i = (i + 1) % NUM_OF_SPTSHADOW1;
	}
	return false;
}

static bool
is_shadow2_pdpe_ok (u64 pdpe, u64 key)
{
	unsigned int i, j;
	u64 phys;

	i = current->spt.shadow2_modified;
	j = current->spt.shadow2_free;
	phys = pdpe & current->pte_addr_mask;
	while (i != j) {
		if (key == current->spt.shadow2[i].key &&
		    phys == current->spt.shadow2[i].phys) {
			current->spt.shadow2[i].cleared = false;
			return true;
		}
		i = (i + 1) % NUM_OF_SPTSHADOW2;
	}
	return false;
}

static u64
find_shadow1 (u64 key)
{
	unsigned int i, j, k, l;

	j = current->spt.shadow1_modified;
	k = current->spt.shadow1_normal;
	l = current->spt.shadow1_free;
	i = k;
	while (i != l) {
		if (key == current->spt.shadow1[i].key)
			goto found;
		i = (i + 1) % NUM_OF_SPTSHADOW1;
	}
	return 0;
found:
	if (i != k)
		swap_shadow (current->spt.shadow1, i, k);
	i = k;
	k = (k + 1) % NUM_OF_SPTSHADOW1;
	if (i != j)
		swap_shadow (current->spt.shadow1, i, j);
	i = j;
	j = (j + 1) % NUM_OF_SPTSHADOW1;
	if (true)		/* always i != l */
		swap_shadow (current->spt.shadow1, i, l);
	i = l;
	l = (l + 1) % NUM_OF_SPTSHADOW1;
	current->spt.shadow1_modified = j;
	current->spt.shadow1_normal = k;
	current->spt.shadow1_free = l;
	current->spt.shadow1[i].cleared = false;
	return current->spt.shadow1[i].phys | PDE_P_BIT |
		((key & KEY_LARGEPAGE) ? PDE_AVAILABLE1_BIT : 0);
}

static u64
find_shadow2 (u64 key)
{
	unsigned int i, j, k, l;

	j = current->spt.shadow2_modified;
	k = current->spt.shadow2_normal;
	l = current->spt.shadow2_free;
	i = k;
	while (i != l) {
		if (key == current->spt.shadow2[i].key)
			goto found;
		i = (i + 1) % NUM_OF_SPTSHADOW2;
	}
	return 0;
found:
	if (i != k)
		swap_shadow (current->spt.shadow2, i, k);
	i = k;
	k = (k + 1) % NUM_OF_SPTSHADOW2;
	if (i != j)
		swap_shadow (current->spt.shadow2, i, j);
	i = j;
	j = (j + 1) % NUM_OF_SPTSHADOW2;
	if (true)		/* always i != l */
		swap_shadow (current->spt.shadow2, i, l);
	i = l;
	l = (l + 1) % NUM_OF_SPTSHADOW2;
	current->spt.shadow2_modified = j;
	current->spt.shadow2_normal = k;
	current->spt.shadow2_free = l;
	current->spt.shadow2[i].cleared = false;
	return current->spt.shadow2[i].phys | PDE_P_BIT |
		((key & KEY_LARGEPAGE) ? PDE_AVAILABLE1_BIT : 0);
}

static void
clear_shadow (struct cpu_mmu_spt_shadow *shadow)
{
	if (!shadow->cleared) {
		memset (shadow->virt, 0, PAGESIZE);
		shadow->cleared = true;
	}
}

static void
clean_modified_shadow2 (bool freeflag)
{
	unsigned int i, j;
	
	i = current->spt.shadow2_modified;
	j = current->spt.shadow2_normal;
	if (i == j)
		return;
	while (i != j) {
		clear_shadow (&current->spt.shadow2[i]);
		i = (i + 1) % NUM_OF_SPTSHADOW2;
	}
	if (freeflag)
		current->spt.shadow2_modified = i;
}

static void
clean_modified_shadow1 (bool freeflag)
{
	unsigned int i, j;

	/* freeflag = true; */	/* FIXME: deleting this, more freezes */
	i = current->spt.shadow1_modified;
	j = current->spt.shadow1_normal;
	if (i == j)
		return;
	while (i != j) {
		clear_shadow (&current->spt.shadow1[i]);
		i = (i + 1) % NUM_OF_SPTSHADOW1;
	}
	if (freeflag) {
		current->spt.shadow1_modified = i;
		spinlock_lock (&current->spt.shadow2_lock);
		current->spt.shadow2_normal = current->spt.shadow2_free;
		clean_modified_shadow2 (false);
		spinlock_unlock (&current->spt.shadow2_lock);
	}
}

static bool
new_shadow1 (u64 key, u64 *pde, unsigned int *ii)
{
	unsigned int i, j, k, l;

	j = current->spt.shadow1_modified;
	k = current->spt.shadow1_normal;
	l = current->spt.shadow1_free;
	for (i = j; i != k; i = (i + 1) % NUM_OF_SPTSHADOW1) {
		if (key == current->spt.shadow1[i].key) {
			clear_shadow (&current->spt.shadow1[i]);
			if (i != j)
				swap_shadow (current->spt.shadow1, i, j);
			i = j;
			j = (j + 1) % NUM_OF_SPTSHADOW1;
			swap_shadow (current->spt.shadow1, i, l);
			i = l;
			l = (l + 1) % NUM_OF_SPTSHADOW1;
			current->spt.shadow1_modified = j;
			current->spt.shadow1_free = l;
			STATUS_UPDATE (asm_lock_incl (&stat_ptfoundcnt));
			goto found;
		}
		
	}
	i = l;
	l = (l + 1) % NUM_OF_SPTSHADOW1;
	if (l == current->spt.shadow1_modified) {
		if (k != i) {
			k = (k + 1) % NUM_OF_SPTSHADOW1;
			current->spt.shadow1_normal = k;
		}
		clean_modified_shadow1 (true);
		if (l == current->spt.shadow1_modified)
			panic ("new_shadow fatal error");
		STATUS_UPDATE (asm_lock_incl (&stat_ptfullcnt));
		return false;
	}
	STATUS_UPDATE (asm_lock_incl (&stat_ptnewcnt));
	current->spt.shadow1[i].key = key;
	current->spt.shadow1_free = l;
found:
	current->spt.shadow1[i].cleared = false;
	*pde = current->spt.shadow1[i].phys | PDE_P_BIT |
		((key & KEY_LARGEPAGE) ? PDE_AVAILABLE1_BIT : 0);
	*ii = i;
	return true;
}

static bool
new_shadow2 (u64 key, u64 *pde, unsigned int *ii)
{
	unsigned int i, j, k, l;

	j = current->spt.shadow2_modified;
	k = current->spt.shadow2_normal;
	l = current->spt.shadow2_free;
	for (i = j; i != k; i = (i + 1) % NUM_OF_SPTSHADOW2) {
		if (key == current->spt.shadow2[i].key) {
			clear_shadow (&current->spt.shadow2[i]);
			if (i != j)
				swap_shadow (current->spt.shadow2, i, j);
			i = j;
			j = (j + 1) % NUM_OF_SPTSHADOW2;
			swap_shadow (current->spt.shadow2, i, l);
			i = l;
			l = (l + 1) % NUM_OF_SPTSHADOW2;
			current->spt.shadow2_modified = j;
			current->spt.shadow2_free = l;
			STATUS_UPDATE (asm_lock_incl (&stat_pdfoundcnt));
			goto found;
		}
		
	}
	i = l;
	l = (l + 1) % NUM_OF_SPTSHADOW2;
	if (l == current->spt.shadow2_modified) {
		if (k != i) {
			k = (k + 1) % NUM_OF_SPTSHADOW2;
			current->spt.shadow2_normal = k;
		}
		clean_modified_shadow2 (true);
		if (l == current->spt.shadow2_modified)
			panic ("new_shadow fatal error");
		STATUS_UPDATE (asm_lock_incl (&stat_pdfullcnt));
		return false;
	}
	STATUS_UPDATE (asm_lock_incl (&stat_pdnewcnt));
	current->spt.shadow2[i].key = key;
	current->spt.shadow2_free = l;
found:
	current->spt.shadow2[i].cleared = false;
	*pde = current->spt.shadow2[i].phys | PDE_P_BIT;
	*ii = i;
	return true;
}

static bool
makerdonly (u64 key)
{
	struct cpu_mmu_spt_data *spt;
	struct sptlist *listspt;
	u64 gfn, newpte;
	bool r = true;
	unsigned int i, j, k, l;

	if (key & KEY_LARGEPAGE)
		return true;
	gfn = key >> KEY_GFN_SHIFT;
	LIST1_FOREACH (list1_spt, listspt) {
		spt = listspt->spt;
		spinlock_lock (&spt->rwmap_lock);
		j = spt->rwmap_fail;
		k = spt->rwmap_normal;
		l = spt->rwmap_free;
		for (i = k; i != l; i = (i + 1) % NUM_OF_SPTRWMAP) {
			if (spt->rwmap[i].gfn == gfn) {
				if (i != k)
					swap_rwmap (spt->rwmap, i, k);
				k = (k + 1) % NUM_OF_SPTRWMAP;
			}
		}
		for (i = j; i != k; i = (i + 1) % NUM_OF_SPTRWMAP) {
			if (spt->rwmap[i].gfn == gfn) {
				newpte = pte_and (spt->rwmap[i].pte,
						  ~PTE_RW_BIT);
				if (newpte & PTE_D_BIT) {
					if (spt == &current->spt) {
						pte_and (spt->rwmap[i].pte,
							 ~PTE_D_BIT);
					} else {
						r = false;
						continue;
					}
				}
				if (i != j)
					swap_rwmap (spt->rwmap, i, j);
				j = (j + 1) % NUM_OF_SPTRWMAP;
			}
		}
		spt->rwmap_fail = j;
		spt->rwmap_normal = k;
		spinlock_unlock (&spt->rwmap_lock);
	}	
	return r;
}

static void
modified_shadow1 (unsigned int i)
{
	unsigned int j;

	j = current->spt.shadow1_normal;
	if (i != j)
		swap_shadow (current->spt.shadow1, i, j);
	j = (j + 1) % NUM_OF_SPTSHADOW1;
	current->spt.shadow1_normal = j;
}

static void
modified_shadow2 (unsigned int i)
{
	unsigned int j;

	j = current->spt.shadow2_normal;
	if (i != j)
		swap_shadow (current->spt.shadow2, i, j);
	j = (j + 1) % NUM_OF_SPTSHADOW2;
	current->spt.shadow2_normal = j;
}

static bool
setpde (pmap_t *p, u64 pde, u64 key, u64 gfnw, struct map_page_data2 m)
{
	bool r = true;
	unsigned int i;
	u64 pdeflags, u;

	pdeflags = (m.rw ? PDE_RW_BIT : 0) |
		(m.us ? PDE_US_BIT : 0) |
		(m.nx ? PDE_NX_BIT : 0);
	u = PDE_AVAILABLE1_BIT | PDE_P_BIT;
	spinlock_lock (&current->spt.shadow1_lock);
	if (pde) {
		if (is_shadow1_pde_ok (pde, key)) {
			pmap_write (p, pde | pdeflags, u);
			STATUS_UPDATE (asm_lock_incl (&stat_ptgoodcnt));
			goto ret;
		}
	}
	pde = find_shadow1 (key);
	if (pde) {
		pmap_write (p, pde | pdeflags, u);
		STATUS_UPDATE (asm_lock_incl (&stat_pthitcnt));
		goto ret;
	}
	r = new_shadow1 (key, &pde, &i);
	if (!r)
		goto ret;
	pmap_write (p, pde | pdeflags, u);
	if (gfnw != GFN_UNUSED && !(key & KEY_LARGEPAGE) &&
	    gfnw == (key >> KEY_GFN_SHIFT)) {
		modified_shadow1 (i);
		goto ret;
	}
	spinlock_unlock (&current->spt.shadow1_lock);
	if (!makerdonly (key)) {
		STATUS_UPDATE (asm_lock_incl (&stat_ptnew2cnt));
		spinlock_lock (&current->spt.shadow1_lock);
		modified_shadow1 (i);
		spinlock_unlock (&current->spt.shadow1_lock);
	}
	return r;
ret:
	spinlock_unlock (&current->spt.shadow1_lock);
	return r;
}

static bool
setpdpe (pmap_t *p, u64 pdpe, u64 key, u64 gfnw)
{
	bool r = true;
	unsigned int i;

	spinlock_lock (&current->spt.shadow2_lock);
	if (pdpe) {
		if (is_shadow2_pdpe_ok (pdpe, key)) {
			pmap_write (p, pdpe, PDE_P_BIT);
			STATUS_UPDATE (asm_lock_incl (&stat_pdgoodcnt));
			goto ret;
		}
	}
	pdpe = find_shadow2 (key);
	if (pdpe) {
		pmap_write (p, pdpe, PDE_P_BIT);
		STATUS_UPDATE (asm_lock_incl (&stat_pdhitcnt));
		goto ret;
	}
	r = new_shadow2 (key, &pdpe, &i);
	if (!r)
		goto ret;
	pmap_write (p, pdpe, PDE_P_BIT);
	if (gfnw != GFN_UNUSED && gfnw == (key >> KEY_GFN_SHIFT)) {
		modified_shadow2 (i);
		goto ret;
	}
	spinlock_unlock (&current->spt.shadow2_lock);
	if (!makerdonly (key)) {
		STATUS_UPDATE (asm_lock_incl (&stat_pdnew2cnt));
		spinlock_lock (&current->spt.shadow2_lock);
		modified_shadow2 (i);
		spinlock_unlock (&current->spt.shadow2_lock);
	}
	return r;
ret:
	spinlock_unlock (&current->spt.shadow2_lock);
	return r;
}

static bool
mapsub (pmap_t *pp, u64 *entry, int level, bool clearall)
{
	u64 tmp;
	int l;

	if (clearall) {
		pmap_setlevel (pp, current->spt.levels);
		pmap_clear (pp);
		current->spt.cnt = 0;
	}
	pmap_setlevel (pp, level);
	tmp = pmap_read (pp);
	for (; (l = pmap_getreadlevel (pp)) > level; tmp = pmap_read (pp)) {
		pmap_setlevel (pp, l);
		if (current->spt.cnt == NUM_OF_SPTTBL)
			return false;
		pmap_write (pp, current->spt.tbl_phys[current->spt.cnt++] |
			    PDE_P_BIT, PDE_P_BIT);
		pmap_setlevel (pp, l - 1);
		pmap_clear (pp);
		pmap_setlevel (pp, level);
	}
	*entry = tmp;
	return true;
}

static void
map_page (u64 v, struct map_page_data1 m1, struct map_page_data2 m2[5],
	  u64 gfns[5], int glvl)
{
	u64 hphys;
	bool fakerom;
	pmap_t p;
	u64 tmp;
	u64 key1, key2 = 0;
	u64 gfnw;
	int levels;

	STATUS_UPDATE (asm_lock_incl (&stat_mapcnt));
	if (!current->spt.wp) {
		if (!m1.user && m1.write) {
			m2[0].us = 0;
			m2[0].rw = 1;
		}
	}
	if (glvl == 1) {
		key1 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LPMASK) |
			KEY_LARGEPAGE;
	} else if (glvl == 2) {
		if (gfns[1] == GFN_GLOBAL || gfns[1] == GFN_UNUSED) {
			key1 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LPMASK) |
				KEY_LARGEPAGE;
		} else {
			key1 = (gfns[1] << KEY_GFN_SHIFT) | KEY_GLVL2;
			key1 |= ((v >> 21) & 1) << KEY_2TO3_SHIFT;
		}
		key2 = (gfns[2] << KEY_GFN_SHIFT) | KEY_GLVL2;
		key2 |= ((v >> 30) & 3) << KEY_2TO3_SHIFT;
	} else if (glvl == 3 || glvl == 4) {
		if (gfns[1] == GFN_GLOBAL || gfns[1] == GFN_UNUSED) {
			key1 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LPMASK) |
				KEY_LARGEPAGE;
		} else {
			key1 = (gfns[1] << KEY_GFN_SHIFT) | KEY_GLVL3;
		}
		key2 = (gfns[2] << KEY_GFN_SHIFT) | KEY_GLVL3;
	} else {
		panic ("map_page: glvl error");
		for (;;);
	}
	gfnw = GFN_UNUSED;
	if (m1.write)
		gfnw = gfns[0];
	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	if (key2)
		levels = 3;
	else
		levels = 2;
	pmap_seek (&p, v, levels);
	if (!mapsub (&p, &tmp, levels, false)) {
		if (!mapsub (&p, &tmp, levels, true))
			panic ("mapsub failed 1");
	}
	if (levels == 3) {
		if (!setpdpe (&p, tmp, key2, gfnw)) {
			if (!mapsub (&p, &tmp, levels, true))
				panic ("mapsub failed 2");
			if (!setpdpe (&p, tmp, key2, gfnw))
				panic ("setpdpe failed 1");
		}
		pmap_setlevel (&p, 2);
		tmp = pmap_read (&p);
		if (!setpde (&p, tmp, key1, gfnw, m2[1])) {
			if (!mapsub (&p, &tmp, levels, true))
				panic ("mapsub failed 3");
			if (!setpdpe (&p, tmp, key2, gfnw))
				panic ("setpdpe failed 2");
			pmap_setlevel (&p, 2);
			tmp = pmap_read (&p);
			if (!setpde (&p, tmp, key1, gfnw, m2[1]))
				panic ("setpde failed 1");
		}		
	} else {	
		if (!setpde (&p, tmp, key1, gfnw, m2[1])) {
			if (!mapsub (&p, &tmp, levels, true))
				panic ("mapsub failed 4");
			if (!setpde (&p, tmp, key1, gfnw, m2[1]))
				panic ("setpde failed 2");
		}
	}
	pmap_setlevel (&p, 1);
	tmp = pmap_read (&p);
	hphys = current->gmm.gp2hp (gfns[0] << PAGESIZE_SHIFT, &fakerom);
	if (fakerom && m1.write)
		panic ("Writing to VMM memory.");
	if (fakerom)
		m2[0].rw = 0;
	pmap_write (&p, hphys | PTE_P_BIT |
		    (m2[0].rw ? PTE_RW_BIT : 0) |
		    (m2[0].us ? PTE_US_BIT : 0) |
		    (m2[0].nx ? PTE_NX_BIT : 0) |
		    (m1.pwt ? PTE_PWT_BIT : 0) |
		    (m1.pcd ? PTE_PCD_BIT : 0) |
		    (m1.pat ? PTE_PAT_BIT : 0),
		    PTE_P_BIT | PTE_RW_BIT | PTE_US_BIT |
		    PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT | PTE_D_BIT);
	if (m2[0].rw) {
		update_rwmap (gfns[0], pmap_pointer (&p));
		update_shadow (gfns[0], pmap_pointer (&p), m1.write);
	}
	pmap_close (&p);
}

static bool
extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	u64 *e, tmp, mask = p->pte_addr_mask;
	unsigned int n = 512;
	unsigned int i, j, k;

	start &= ~PAGESIZE_MASK;
	end |= PAGESIZE_MASK;
	/* lock is not needed because this is called by mmio_register */
	/* mmio_register uses another lock to avoid conflict */
	/* with cpu_mmu_spt_pagefault() */
	i = p->spt.shadow1_modified;
	k = p->spt.shadow1_free;
	while (i != k) {
		e = p->spt.shadow1[i].virt;
		for (j = 0; j < n; j++) {
			tmp = e[j] & mask;
			if ((e[j] & PTE_P_BIT) && start <= tmp && tmp <= end)
				return true;
		}
		i = (i + 1) % NUM_OF_SPTSHADOW1;
	}
	return false;
}

static void
clear_rwmap (void)
{
	spinlock_lock (&current->spt.rwmap_lock);
	current->spt.rwmap_fail = 0;
	current->spt.rwmap_normal = 0;
	current->spt.rwmap_free = 0;
	spinlock_unlock (&current->spt.rwmap_lock);
}

static void
clear_shadow1 (void)
{
	unsigned int i;

	spinlock_lock (&current->spt.shadow1_lock);
	for (i = 0; i < NUM_OF_SPTSHADOW1; i++)
		clear_shadow (&current->spt.shadow1[i]);
	current->spt.shadow1_modified = 0;
	current->spt.shadow1_normal = 0;
	current->spt.shadow1_free = 0;
	spinlock_unlock (&current->spt.shadow1_lock);
}

static void
clear_shadow2 (void)
{
	unsigned int i;

	spinlock_lock (&current->spt.shadow2_lock);
	for (i = 0; i < NUM_OF_SPTSHADOW2; i++)
		clear_shadow (&current->spt.shadow2[i]);
	current->spt.shadow2_modified = 0;
	current->spt.shadow2_normal = 0;
	current->spt.shadow2_free = 0;
	spinlock_unlock (&current->spt.shadow2_lock);
}

static void
update_cr3 (void)
{
	pmap_t p;
	ulong cr0;

	STATUS_UPDATE (asm_lock_incl (&stat_cr3cnt));
#ifdef CPU_MMU_SPT_USE_PAE
	current->spt.levels = guest64 () ? 4 : 3;
#else
	current->spt.levels = 2;
#endif
	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	pmap_clear (&p);
	pmap_close (&p);
	current->spt.cnt = 0;
	current->vmctl.spt_setcr3 (current->spt.cr3tbl_phys);
	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr0);
	if (current->spt.wp && !(cr0 & CR0_WP_BIT)) {
		current->spt.wp = false;
		clear_rwmap ();
		clear_shadow1 ();
		clear_shadow2 ();
		STATUS_UPDATE (asm_lock_incl (&stat_wpcnt));
	}
	if (!current->spt.wp && (cr0 & CR0_WP_BIT)) {
		current->spt.wp = true;
		clear_rwmap ();
		clear_shadow1 ();
		clear_shadow2 ();
		STATUS_UPDATE (asm_lock_incl (&stat_wpcnt));
	}
	update_rwmap (0, NULL);
	spinlock_lock (&current->spt.shadow1_lock);
	clean_modified_shadow1 (false);
	spinlock_unlock (&current->spt.shadow1_lock);
	spinlock_lock (&current->spt.shadow2_lock);
	clean_modified_shadow2 (true);
	spinlock_unlock (&current->spt.shadow2_lock);
}

static void
clear_all (void)
{
	pmap_t p;

	pmap_open_vmm (&p, current->spt.cr3tbl_phys, current->spt.levels);
	pmap_clear (&p);
	pmap_close (&p);
	current->spt.cnt = 0;
	current->vmctl.spt_setcr3 (current->spt.cr3tbl_phys);
	clear_rwmap ();
	clear_shadow1 ();
	clear_shadow2 ();
}

static bool
spt_tlbflush (void)
{
	return update_rwmap (0, NULL);
}

static char *
spt_status (void)
{
	static char buf[1024];

	snprintf (buf, 1024,
		  "MMU:\n"
		  " MOV CR3: %u INVLPG: %u\n"
		  " Map: %u WP: %u\n"
		  "Shadow page table:\n"
		  " Found: %u  Full: %u New: %u\n"
		  " Good: %u Hit: %u New2: %u\n"
		  "Shadow page directory:\n"
		  " Found: %u  Full: %u New: %u\n"
		  " Good: %u Hit: %u New2: %u\n"
		  , stat_cr3cnt, stat_invlpgcnt, stat_mapcnt, stat_wpcnt
		  , stat_ptfoundcnt, stat_ptfullcnt, stat_ptnewcnt
		  , stat_ptgoodcnt, stat_pthitcnt, stat_ptnew2cnt
		  , stat_pdfoundcnt, stat_pdfullcnt, stat_pdnewcnt
		  , stat_pdgoodcnt, stat_pdhitcnt, stat_pdnew2cnt);
	return buf;
}

static void
init_global (void)
{
	LIST1_HEAD_INIT (list1_spt);
	register_status_callback (spt_status);
}

static void
init_pcpu (void)
{
}

static int
init_vcpu (void)
{
	int i;
	struct sptlist *listspt;

	alloc_page (&current->spt.cr3tbl, &current->spt.cr3tbl_phys);
	for (i = 0; i < NUM_OF_SPTTBL; i++)
		alloc_page (&current->spt.tbl[i], &current->spt.tbl_phys[i]);
	current->spt.cnt = 0;
	memset (current->spt.cr3tbl, 0, PAGESIZE);
	for (i = 0; i < NUM_OF_SPTSHADOW1; i++) {
		alloc_page (&current->spt.shadow1[i].virt,
			    &current->spt.shadow1[i].phys);
		current->spt.shadow1[i].cleared = false;
	}
	for (i = 0; i < NUM_OF_SPTSHADOW2; i++) {
		alloc_page (&current->spt.shadow2[i].virt,
			    &current->spt.shadow2[i].phys);
		current->spt.shadow2[i].cleared = false;
	}
	spinlock_init (&current->spt.rwmap_lock);
	spinlock_init (&current->spt.shadow1_lock);
	spinlock_init (&current->spt.shadow2_lock);
	clear_rwmap ();
	clear_shadow1 ();
	clear_shadow2 ();
	current->spt.wp = false;
	listspt = alloc (sizeof *listspt);
	listspt->spt = &current->spt;
	LIST1_PUSH (list1_spt, listspt);
	return 0;
}
#endif /* CPU_MMU_SPT_2 */

#ifdef CPU_MMU_SPT_3
#include "list.h"

/* key:
   bit 63-12: gfn
   bit 11,10: 2to3
   bit 9-7:   glevels (not largepage)
   bit 6:     largepage
   bit 5:     wp=0
   bit 4:     nx=1
   bit 3:     us=0
   bit 2:     rw=0
   bit 0:     modified */

#define KEY_GFNMASK	0xFFFFFFFFFFFFF000ULL
#define KEY_GFN_SHIFT	12
#define KEY_2TO3_SHIFT	10
#define KEY_LARGEPAGE	0x40ULL
#define KEY_WP0		0x20ULL
#define KEY_NX		0x10ULL
#define KEY_US0		0x8ULL
#define KEY_RW0		0x4ULL
#define KEY_GLVL2	(2 << 7)
#define KEY_GLVL3	(3 << 7)
#define KEY_GLVL4	(4 << 7)
#define KEY_LPMASK	0xFFFFFFFFFFE00000ULL
#define KEY_LP2MASK	0xFFFFFFFFC0000000ULL
#define KEY_CMPMASK	0xFFFFFFFFFFFFFFFCULL
#define KEY_MODIFIED	0x1ULL
#define CLEARINFO1(v)	(((v) >> 12) & 0x1FF)
#define CLEARINFO2(v)	(((v) >> 21) & 0x1FF)

#define NUM_OF_SPTTBL		32
#define NUM_OF_SPTRWMAP 	4096
#define NUM_OF_SPTSHADOW1	2048
#define NUM_OF_SPTSHADOW2	512
#define HASHSIZE_OF_SPTRWMAP	4096
#define HASHSIZE_OF_SPTSHADOW1	2048
#define HASHSIZE_OF_SPTSHADOW2	1024
#define NUM_OF_SPTSHADOWOFF	31
#define NUM_OF_SPTSHADOW1MAP	512

struct cpu_mmu_spt_rwmap {
	LIST3_DEFINE (struct cpu_mmu_spt_rwmap, rwmap, short);
	LIST3_DEFINE (struct cpu_mmu_spt_rwmap, hash, short);
	u64 gfn;		/* bit 0 is fail flag */
	u64 *pte;
	u64 hphys;
};

struct cpu_mmu_spt_shadow;

struct cpu_mmu_spt_shadow1map {
	LIST3_DEFINE (struct cpu_mmu_spt_shadow1map, shadow1map, short);
	LIST3_DEFINE (struct cpu_mmu_spt_shadow1map, ref, short);
	struct cpu_mmu_spt_shadow *shadow1;
	u64 *pde;
};

struct cpu_mmu_spt_shadow {
	LIST3_DEFINE (struct cpu_mmu_spt_shadow, shadow, short);
	LIST3_DEFINE (struct cpu_mmu_spt_shadow, hash, short);
	u64 phys;
	u64 key;
	u8 clear_n;
	u8 clear_off[NUM_OF_SPTSHADOWOFF];
	u64 clear_area;
	LIST3_DEFINE_HEAD (shadow1map_ref, struct cpu_mmu_spt_shadow1map, ref);
};

struct cpu_mmu_spt_data_internal {
	void *cr3tbl;
	u64 cr3tbl_phys;
	void *tbl[NUM_OF_SPTTBL];
	u64 tbl_phys[NUM_OF_SPTTBL];
	int cnt;
	int levels;
	struct cpu_mmu_spt_rwmap rwmap[NUM_OF_SPTRWMAP];
	spinlock_t rwmap_lock;
	LIST3_DEFINE_HEAD (rwmap_fail, struct cpu_mmu_spt_rwmap, rwmap);
	LIST3_DEFINE_HEAD (rwmap_normal, struct cpu_mmu_spt_rwmap, rwmap);
	LIST3_DEFINE_HEAD (rwmap_free, struct cpu_mmu_spt_rwmap, rwmap);
	LIST3_DEFINE_HEAD (rwmap_hash[HASHSIZE_OF_SPTRWMAP],
			   struct cpu_mmu_spt_rwmap, hash);
	struct cpu_mmu_spt_shadow shadow1[NUM_OF_SPTSHADOW1];
	struct cpu_mmu_spt_shadow1map shadow1map[NUM_OF_SPTSHADOW1MAP];
	LIST3_DEFINE_HEAD (shadow1map_free, struct cpu_mmu_spt_shadow1map,
			   shadow1map);
	LIST3_DEFINE_HEAD (shadow1map_list, struct cpu_mmu_spt_shadow1map,
			   shadow1map);
	rw_spinlock_t shadow1_lock;
	LIST3_DEFINE_HEAD (shadow1_modified, struct cpu_mmu_spt_shadow,
			   shadow);
	LIST3_DEFINE_HEAD (shadow1_normal, struct cpu_mmu_spt_shadow, shadow);
	LIST3_DEFINE_HEAD (shadow1_free, struct cpu_mmu_spt_shadow, shadow);
	LIST3_DEFINE_HEAD (shadow1_hash[HASHSIZE_OF_SPTSHADOW1],
			   struct cpu_mmu_spt_shadow, hash);
	struct cpu_mmu_spt_shadow shadow2[NUM_OF_SPTSHADOW2];
	rw_spinlock_t shadow2_lock;
	LIST3_DEFINE_HEAD (shadow2_modified, struct cpu_mmu_spt_shadow,
			   shadow);
	LIST3_DEFINE_HEAD (shadow2_normal, struct cpu_mmu_spt_shadow, shadow);
	LIST3_DEFINE_HEAD (shadow2_free, struct cpu_mmu_spt_shadow, shadow);
	LIST3_DEFINE_HEAD (shadow2_hash[HASHSIZE_OF_SPTSHADOW2],
			   struct cpu_mmu_spt_shadow, hash);
	bool wp;
	ulong cr0, cr3, cr4;
	u64 efer;
};

struct sptlist {
	LIST1_DEFINE (struct sptlist);
	struct cpu_mmu_spt_data_internal *spt;
};

struct findshadow {
	struct cpu_mmu_spt_shadow *pn, *pm;
};

typedef struct cpu_mmu_spt_data_internal spt_t;

static LIST1_DEFINE_HEAD (struct sptlist, list1_spt);

static u32 stat_mapcnt = 0;
static u32 stat_cr3cnt = 0;
static u32 stat_invlpgcnt = 0;
static u32 stat_wpcnt = 0;
static u32 stat_ptfoundcnt = 0;
static u32 stat_ptfullcnt = 0;
static u32 stat_ptnewcnt = 0;
static u32 stat_pdfoundcnt = 0;
static u32 stat_pdfullcnt = 0;
static u32 stat_pdnewcnt = 0;
static u32 stat_ptgoodcnt = 0;
static u32 stat_pthitcnt = 0;
static u32 stat_ptnew2cnt = 0;
static u32 stat_pdgoodcnt = 0;
static u32 stat_pdhitcnt = 0;
static u32 stat_pdnew2cnt = 0;
static u32 stat_clrcnt = 0;
static u32 stat_clr2cnt = 0;

static void
get_cr0_cr3_cr4_and_efer (ulong *cr0, ulong *cr3, ulong *cr4, u64 *efer)
{
	spt_t *spt;

	spt = current->spt.data;
	*cr0 = spt->cr0;
	*cr3 = spt->cr3;
	*cr4 = spt->cr4;
	*efer = spt->efer;
}

static unsigned int
rwmap_hash_index (u64 gfn)
{
	return gfn & (HASHSIZE_OF_SPTRWMAP - 1);
}

static unsigned int
shadow1_hash_index (u64 key)
{
	return (key >> KEY_GFN_SHIFT) & (HASHSIZE_OF_SPTSHADOW1 - 1);
}

static unsigned int
shadow2_hash_index (u64 key)
{
	return (key >> KEY_GFN_SHIFT) & (HASHSIZE_OF_SPTSHADOW2 - 1);
}

static bool
update_rwmap (spt_t *cspt, u64 gfn, void *pte, u64 hphys)
{
	struct cpu_mmu_spt_rwmap *p;
	unsigned int hr, hrtmp;
	bool r = false;
	u64 oldpte;
	const u64 mask = current->pte_addr_mask;

	spinlock_lock (&cspt->rwmap_lock);
	while ((p = LIST3_POP (cspt->rwmap_fail, rwmap))) {
		hrtmp = rwmap_hash_index (p->gfn >> 1);
		LIST3_DEL (cspt->rwmap_hash[hrtmp], hash, p);
		oldpte = *p->pte;
		if ((oldpte & mask) == p->hphys) {
			*p->pte = oldpte & ~(PTE_D_BIT | PTE_RW_BIT);
			r = true;
		}
		LIST3_PUSH (cspt->rwmap_free, rwmap, p);
	}
	if (pte != NULL) {
		hr = rwmap_hash_index (gfn);
		LIST3_FOREACH (cspt->rwmap_hash[hr], hash, p) {
			if (p->pte == pte) {
				LIST3_DEL (cspt->rwmap_hash[hr], hash, p);
				LIST3_DEL (cspt->rwmap_normal, rwmap, p);
				goto found;
			}
		}
		p = LIST3_POP (cspt->rwmap_free, rwmap);
		if (p == NULL) {
			p = LIST3_POP (cspt->rwmap_normal, rwmap);
			hrtmp = rwmap_hash_index (p->gfn >> 1);
			LIST3_DEL (cspt->rwmap_hash[hrtmp], hash, p);
			oldpte = *p->pte;
			if ((oldpte & mask) == p->hphys)
				*p->pte = oldpte & ~(PTE_D_BIT | PTE_RW_BIT);
		}
		p->pte = pte;
	found:
		p->gfn = gfn << 1;
		p->hphys = hphys;
		LIST3_ADD (cspt->rwmap_normal, rwmap, p);
		LIST3_PUSH (cspt->rwmap_hash[hr], hash, p);
	}
	spinlock_unlock (&cspt->rwmap_lock);
	return r;
}

static void
update_shadow (spt_t *cspt, u64 gfn0, u64 *pte, bool needrw)
{
	spt_t *spt;
	struct sptlist *listspt;
	bool rw = true;
	u64 key, keytmp;
	struct cpu_mmu_spt_shadow *p, *pn;
	unsigned int hs, hr;
	struct cpu_mmu_spt_rwmap *q;

	key = gfn0 << KEY_GFN_SHIFT;
	LIST1_FOREACH (list1_spt, listspt) {
		spt = listspt->spt;
		hs = shadow1_hash_index (key);
		if (needrw)
			rw_spinlock_lock_ex (&spt->shadow1_lock);
		else
			rw_spinlock_lock_sh (&spt->shadow1_lock);
		LIST3_FOREACH_DELETABLE (spt->shadow1_hash[hs], hash, p, pn) {
			keytmp = p->key;
			if (keytmp & (KEY_LARGEPAGE | KEY_MODIFIED))
				continue;
			if ((keytmp & KEY_GFNMASK) != key)
				continue;
			if (!needrw) {
				rw = false;
				break;
			}
			LIST3_DEL (spt->shadow1_normal, shadow, p);
			LIST3_ADD (spt->shadow1_modified, shadow, p);
			p->key |= KEY_MODIFIED;
		}
		if (needrw)
			rw_spinlock_unlock_ex (&spt->shadow1_lock);
		else
			rw_spinlock_unlock_sh (&spt->shadow1_lock);
		if (!rw)
			break;
		hs = shadow2_hash_index (key);
		if (needrw)
			rw_spinlock_lock_ex (&spt->shadow2_lock);
		else
			rw_spinlock_lock_sh (&spt->shadow2_lock);
		LIST3_FOREACH_DELETABLE (spt->shadow2_hash[hs], hash, p, pn) {
			keytmp = p->key;
			if (keytmp & (KEY_LARGEPAGE | KEY_MODIFIED))
				continue;
			if ((keytmp & KEY_GFNMASK) != key)
				continue;
			if (!needrw) {
				rw = false;
				break;
			}
			LIST3_DEL (spt->shadow2_normal, shadow, p);
			LIST3_ADD (spt->shadow2_modified, shadow, p);
			p->key |= KEY_MODIFIED;
		}
		if (needrw)
			rw_spinlock_unlock_ex (&spt->shadow2_lock);
		else
			rw_spinlock_unlock_sh (&spt->shadow2_lock);
		if (!rw)
			break;
	}
	if (!rw) {
		hr = rwmap_hash_index (gfn0);
		spinlock_lock (&cspt->rwmap_lock);
		q = LIST3_POP (cspt->rwmap_hash[hr], hash);
		if (q && (q->gfn >> 1) == gfn0 && q->pte == pte) {
			if (q->gfn & 1)
				LIST3_DEL (cspt->rwmap_fail, rwmap, q);
			else
				LIST3_DEL (cspt->rwmap_normal, rwmap, q);
			LIST3_PUSH (cspt->rwmap_free, rwmap, q);
		} else if (q) {
			LIST3_PUSH (cspt->rwmap_hash[hr], hash, q);
		}
		spinlock_unlock (&cspt->rwmap_lock);
		*pte &= ~PTE_RW_BIT;
	}
}

static void
update_clearinfo (struct cpu_mmu_spt_shadow *shadow, unsigned int off)
{
	unsigned int i, j;

	j = off >> 1;
	i = shadow->clear_n;
	if (i < NUM_OF_SPTSHADOWOFF) {
		shadow->clear_off[i++] = j;
		shadow->clear_n = i;
	} else if (i == NUM_OF_SPTSHADOWOFF) {
		i++;
		shadow->clear_n = i;
	}
	shadow->clear_area |= 1ULL << (off >> 3);
}

static void
find_shadow1_from_hash (spt_t *cspt, u64 key, struct findshadow *q)
{
	unsigned int hs;
	struct cpu_mmu_spt_shadow *p;

	q->pn = NULL;
	q->pm = NULL;
	hs = shadow1_hash_index (key);
	LIST3_FOREACH (cspt->shadow1_hash[hs], hash, p) {
		if (key == (p->key & KEY_CMPMASK)) {
			if (p->key & KEY_MODIFIED)
				q->pm = p;
			else
				q->pn = p;
			return;
		}
	}
}

static void
find_shadow2_from_hash (spt_t *cspt, u64 key, struct findshadow *q)
{
	unsigned int hs;
	struct cpu_mmu_spt_shadow *p;

	q->pn = NULL;
	q->pm = NULL;
	hs = shadow2_hash_index (key);
	LIST3_FOREACH (cspt->shadow2_hash[hs], hash, p) {
		if (key == (p->key & KEY_CMPMASK)) {
			if (p->key & KEY_MODIFIED)
				q->pm = p;
			else
				q->pn = p;
			return;
		}
	}
}

static bool
is_shadow1_pde_ok (spt_t *cspt, u64 pde, u64 v, struct findshadow *fs,
		   u64 mask)
{
	u64 phys;
	struct cpu_mmu_spt_shadow *p;

	phys = pde & mask;
	p = fs->pm;
	if (p && phys == p->phys) {
		if (p->clear_n == 0)
			return false;
		update_clearinfo (p, CLEARINFO1 (v));
		return true;
	}
	p = fs->pn;
	if (p && phys == p->phys) {
		LIST3_DEL (cspt->shadow1_normal, shadow, p);
		LIST3_ADD (cspt->shadow1_normal, shadow, p);
		update_clearinfo (p, CLEARINFO1 (v));
		return true;
	}
	return false;
}

static bool
is_shadow2_pdpe_ok (spt_t *cspt, u64 pdpe, u64 v, struct findshadow *fs,
		    u64 mask)
{
	u64 phys;
	struct cpu_mmu_spt_shadow *p;

	phys = pdpe & mask;
	p = fs->pm;
	if (p && phys == p->phys) {
		if (p->clear_n == 0)
			return false;
		update_clearinfo (p, CLEARINFO2 (v));
		return true;
	}
	p = fs->pn;
	if (p && phys == p->phys) {
		LIST3_DEL (cspt->shadow2_normal, shadow, p);
		LIST3_ADD (cspt->shadow2_normal, shadow, p);
		update_clearinfo (p, CLEARINFO2 (v));
		return true;
	}
	return false;
}

static u64
find_shadow1 (spt_t *cspt, u64 key, u64 v, struct findshadow *fs)
{
	struct cpu_mmu_spt_shadow *p;

	p = fs->pn;
	if (p)
		goto found;
	return 0;
found:
	LIST3_DEL (cspt->shadow1_normal, shadow, p);
	LIST3_ADD (cspt->shadow1_normal, shadow, p);
	update_clearinfo (p, CLEARINFO1 (v));
	return p->phys | PDE_P_BIT |
		((key & KEY_LARGEPAGE) ? PDE_AVAILABLE1_BIT : 0);
}

static u64
find_shadow2 (spt_t *cspt, u64 v, struct findshadow *fs)
{
	struct cpu_mmu_spt_shadow *p;

	p = fs->pn;
	if (p)
		goto found;
	return 0;
found:
	LIST3_DEL (cspt->shadow2_normal, shadow, p);
	LIST3_ADD (cspt->shadow2_normal, shadow, p);
	update_clearinfo (p, CLEARINFO2 (v));
	return p->phys | PDE_P_BIT;
}

static void
clear_shadow (struct cpu_mmu_spt_shadow *shadow)
{
	unsigned int i, j, n;
	u64 *p, tmp;
	u8 *pp;

	n = shadow->clear_n;
	if (n) {
		p = (u64 *)phys_to_virt (shadow->phys);
		if (n < NUM_OF_SPTSHADOWOFF) {
			pp = shadow->clear_off;
			for (i = 0; i < n; i++) {
				j = *pp++;
				j <<= 1;
				p[j] = 0;
				p[j + 1] = 0;
			}
			STATUS_UPDATE (asm_lock_incl (&stat_clr2cnt));
		} else {
			tmp = shadow->clear_area;
			if (!tmp)
				panic ("clear_shadow");
			pp = (u8 *)p;
			if (tmp & 1)
				goto start1;
			do {
				do {
					tmp >>= 1;
					pp += 64;
				} while (!(tmp & 1));
			start1:
				j = 0;
				do {
					tmp >>= 1;
					j += 64;
				} while (tmp & 1);
				memset (pp, 0, j);
				pp += j;
			} while (tmp);
			STATUS_UPDATE (asm_lock_incl (&stat_clrcnt));
		}
		shadow->clear_n = 0;
		shadow->clear_area = 0;
	}
}

static void
clean_modified_shadow2 (spt_t *cspt, bool freeflag)
{
	struct cpu_mmu_spt_shadow *p, *n;
	unsigned int hs;

	LIST3_FOREACH_DELETABLE (cspt->shadow2_modified, shadow, p, n) {
		clear_shadow (p);
		if (freeflag) {
			LIST3_DEL (cspt->shadow2_modified, shadow, p);
			LIST3_PUSH (cspt->shadow2_free, shadow, p);
			hs = shadow2_hash_index (p->key);
			LIST3_DEL (cspt->shadow2_hash[hs], hash, p);
		}
	}
}

static void
clean_modified_shadow1 (spt_t *cspt, bool freeflag)
{
	struct cpu_mmu_spt_shadow *p;
	struct cpu_mmu_spt_shadow1map *q;
	unsigned int hs;

	if (freeflag) {
		while ((p = LIST3_POP (cspt->shadow1_modified, shadow))) {
			clear_shadow (p);
			while ((q = LIST3_POP (p->shadow1map_ref, ref))) {
				LIST3_DEL (cspt->shadow1map_list,
					   shadow1map, q);
				if ((*q->pde & PDE_ADDR_MASK64) == p->phys)
					*q->pde = 0;
				LIST3_PUSH (cspt->shadow1map_free,
					    shadow1map, q);
			}
			hs = shadow1_hash_index (p->key);
			LIST3_DEL (cspt->shadow1_hash[hs], hash, p);
			LIST3_PUSH (cspt->shadow1_free, shadow, p);
		}
	} else {
		LIST3_FOREACH (cspt->shadow1_modified, shadow, p)
			clear_shadow (p);
	}
}

static bool
new_shadow1 (spt_t *cspt, u64 key, u64 v, u64 *pde, struct findshadow *fs)
{
	struct cpu_mmu_spt_shadow *p;
	unsigned int hs;

	hs = shadow1_hash_index (key);
	if (fs->pm) {
		p = fs->pm;
		clear_shadow (p);
		LIST3_DEL (cspt->shadow1_modified, shadow, p);
		STATUS_UPDATE (asm_lock_incl (&stat_ptfoundcnt));
		goto found;
	}
	p = LIST3_POP (cspt->shadow1_free, shadow);
	if (p == NULL) {
		p = LIST3_POP (cspt->shadow1_modified, shadow);
		if (p) {
			LIST3_PUSH (cspt->shadow1_modified, shadow, p);
			goto clean;
		}
		p = LIST3_POP (cspt->shadow1_normal, shadow);
		if (p) {
			LIST3_ADD (cspt->shadow1_modified, shadow, p);
			p->key |= KEY_MODIFIED;
		}
		STATUS_UPDATE (asm_lock_incl (&stat_ptfullcnt));
	clean:
		clean_modified_shadow1 (cspt, true);
		p = LIST3_POP (cspt->shadow1_free, shadow);
	}
	STATUS_UPDATE (asm_lock_incl (&stat_ptnewcnt));
	p->key = key;
	LIST3_ADD (cspt->shadow1_hash[hs], hash, p);
found:
	LIST3_ADD (cspt->shadow1_normal, shadow, p);
	p->key &= ~KEY_MODIFIED;
	update_clearinfo (p, CLEARINFO1 (v));
	*pde = p->phys | PDE_P_BIT |
		((key & KEY_LARGEPAGE) ? PDE_AVAILABLE1_BIT : 0);
	fs->pn = p;
	fs->pm = NULL;
	return true;
}

static bool
new_shadow2 (spt_t *cspt, u64 key, u64 v, u64 *pde, struct findshadow *fs)
{
	struct cpu_mmu_spt_shadow *p;
	unsigned int hs;

	hs = shadow2_hash_index (key);
	if (fs->pm) {
		p = fs->pm;
		clear_shadow (p);
		LIST3_DEL (cspt->shadow2_modified, shadow, p);
		STATUS_UPDATE (asm_lock_incl (&stat_pdfoundcnt));
		goto found;
	}
	p = LIST3_POP (cspt->shadow2_free, shadow);
	if (p == NULL) {
		p = LIST3_POP (cspt->shadow2_modified, shadow);
		if (p) {
			LIST3_PUSH (cspt->shadow2_modified, shadow, p);
			goto clean_ret;
		}
		p = LIST3_POP (cspt->shadow2_normal, shadow);
		if (p) {
			LIST3_ADD (cspt->shadow2_modified, shadow, p);
			p->key |= KEY_MODIFIED;
		}
		STATUS_UPDATE (asm_lock_incl (&stat_pdfullcnt));
	clean_ret:
		clean_modified_shadow2 (cspt, true);
		return false;
	}
	STATUS_UPDATE (asm_lock_incl (&stat_pdnewcnt));
	p->key = key;
	LIST3_ADD (cspt->shadow2_hash[hs], hash, p);
found:
	LIST3_ADD (cspt->shadow2_normal, shadow, p);
	p->key &= ~KEY_MODIFIED;
	update_clearinfo (p, CLEARINFO2 (v));
	*pde = p->phys | PDE_P_BIT;
	fs->pn = p;
	fs->pm = NULL;
	return true;
}

static bool
makerdonly (spt_t *cspt, u64 key)
{
	spt_t *spt;
	struct sptlist *listspt;
	u64 gfn, newpte, oldpte;
	bool r = true;
	struct cpu_mmu_spt_rwmap *p, *pn;
	unsigned int hr;

	if (key & KEY_LARGEPAGE)
		return true;
	gfn = key >> KEY_GFN_SHIFT;
	LIST1_FOREACH (list1_spt, listspt) {
		spt = listspt->spt;
		spinlock_lock (&spt->rwmap_lock);
		hr = rwmap_hash_index (gfn);
		LIST3_FOREACH_DELETABLE (spt->rwmap_hash[hr], hash, p, pn) {
			if ((p->gfn >> 1) != gfn)
				continue;
			if (!(p->gfn & 1)) {
				LIST3_DEL (spt->rwmap_normal, rwmap, p);
				LIST3_ADD (spt->rwmap_fail, rwmap, p);
				p->gfn |= 1;
			}
			oldpte = *p->pte;
		pte_changed:
			if ((oldpte & PTE_ADDR_MASK64) != p->hphys) {
				/* Do nothing */
			} else if (spt == cspt) {
				*p->pte = oldpte & ~(PTE_D_BIT | PTE_RW_BIT);
			} else if (oldpte & PTE_D_BIT) {
				r = false;
				continue;
			} else if (oldpte & PTE_RW_BIT) {
				newpte = oldpte & ~(PTE_D_BIT | PTE_RW_BIT);
				if (asm_lock_cmpxchgq (p->pte, &oldpte, newpte))
					goto pte_changed;
			}
			LIST3_DEL (spt->rwmap_fail, rwmap, p);
			LIST3_PUSH (spt->rwmap_free, rwmap, p);
			LIST3_DEL (spt->rwmap_hash[hr], hash, p);
		}
		spinlock_unlock (&spt->rwmap_lock);
	}
	return r;
}

static void
modified_shadow1 (spt_t *cspt, struct cpu_mmu_spt_shadow *p)
{
	if (!(p->key & KEY_MODIFIED)) {
		LIST3_DEL (cspt->shadow1_normal, shadow, p);
		LIST3_ADD (cspt->shadow1_modified, shadow, p);
		p->key |= KEY_MODIFIED;
	}
}

static void
modified_shadow2 (spt_t *cspt, struct cpu_mmu_spt_shadow *p)
{
	if (!(p->key & KEY_MODIFIED)) {
		LIST3_DEL (cspt->shadow2_normal, shadow, p);
		LIST3_ADD (cspt->shadow2_modified, shadow, p);
		p->key |= KEY_MODIFIED;
	}
}

static void
add_shadow1map (spt_t *cspt, struct cpu_mmu_spt_shadow *shadow1, u64 *pde)
{
	struct cpu_mmu_spt_shadow1map *p;

	LIST3_FOREACH (shadow1->shadow1map_ref, ref, p) {
		if (p->pde == pde)
			return;
	}
	p = LIST3_POP (cspt->shadow1map_free, shadow1map);
	if (p == NULL) {
		p = LIST3_POP (cspt->shadow1map_list, shadow1map);
		if ((*p->pde & PDE_ADDR_MASK64) == p->shadow1->phys)
			*p->pde = 0;
		LIST3_DEL (p->shadow1->shadow1map_ref, ref, p);
	}
	p->pde = pde;
	p->shadow1 = shadow1;
	LIST3_ADD (cspt->shadow1map_list, shadow1map, p);
	LIST3_ADD (shadow1->shadow1map_ref, ref, p);
}

static bool
setpde (spt_t *cspt, pmap_t *p, u64 pde, u64 key, u64 gfnw, u64 v, u64 mask,
	struct map_page_data2 m)
{
	bool r = true;
	struct findshadow fs;
	u64 pdeflags, u;

	pdeflags = (m.rw ? PDE_RW_BIT : 0) |
		(m.us ? PDE_US_BIT : 0) |
		(m.nx ? PDE_NX_BIT : 0);
	u = PDE_AVAILABLE1_BIT | PDE_RW_BIT | PDE_US_BIT | PDE_P_BIT;
	if (pde) {
		rw_spinlock_lock_sh (&cspt->shadow1_lock);
		find_shadow1_from_hash (cspt, key, &fs);
		if (is_shadow1_pde_ok (cspt, pde, v, &fs, mask)) {
			rw_spinlock_unlock_sh (&cspt->shadow1_lock);
			pde &= ~(PDE_RW_BIT | PDE_US_BIT | PDE_NX_BIT);
			pmap_write (p, pde | pdeflags, u);
			STATUS_UPDATE (asm_lock_incl (&stat_ptgoodcnt));
			return r;
		}
		rw_spinlock_unlock_sh (&cspt->shadow1_lock);
		rw_spinlock_lock_ex (&cspt->shadow1_lock);
		if (fs.pn && (fs.pn->key & KEY_MODIFIED)) {
			fs.pm = fs.pn;
			fs.pn = NULL;
		}
	} else {
		rw_spinlock_lock_ex (&cspt->shadow1_lock);
		find_shadow1_from_hash (cspt, key, &fs);
	}
	pde = find_shadow1 (cspt, key, v, &fs);
	if (pde) {
		add_shadow1map (cspt, fs.pn, pmap_pointer (p));
		pmap_write (p, pde | pdeflags, u);
		STATUS_UPDATE (asm_lock_incl (&stat_pthitcnt));
		goto ret;
	}
	r = new_shadow1 (cspt, key, v, &pde, &fs);
	if (!r)
		goto ret;
	add_shadow1map (cspt, fs.pn, pmap_pointer (p));
	pmap_write (p, pde | pdeflags, u);
	if (gfnw != GFN_UNUSED && !(key & KEY_LARGEPAGE) &&
	    gfnw == (key >> KEY_GFN_SHIFT)) {
		modified_shadow1 (cspt, fs.pn);
		goto ret;
	}
	rw_spinlock_unlock_ex (&cspt->shadow1_lock);
	if (!makerdonly (cspt, key)) {
		STATUS_UPDATE (asm_lock_incl (&stat_ptnew2cnt));
		rw_spinlock_lock_ex (&cspt->shadow1_lock);
		modified_shadow1 (cspt, fs.pn);
		rw_spinlock_unlock_ex (&cspt->shadow1_lock);
	}
	return r;
ret:
	rw_spinlock_unlock_ex (&cspt->shadow1_lock);
	return r;
}

static bool
setpdpe (spt_t *cspt, pmap_t *p, u64 pdpe, u64 key, u64 gfnw, u64 v, u64 mask)
{
	bool r = true;
	struct findshadow fs;

	if (pdpe) {
		rw_spinlock_lock_sh (&cspt->shadow2_lock);
		find_shadow2_from_hash (cspt, key, &fs);
		if (is_shadow2_pdpe_ok (cspt, pdpe, v, &fs, mask)) {
			rw_spinlock_unlock_sh (&cspt->shadow2_lock);
			STATUS_UPDATE (asm_lock_incl (&stat_pdgoodcnt));
			return r;
		}
		rw_spinlock_unlock_sh (&cspt->shadow2_lock);
		rw_spinlock_lock_ex (&cspt->shadow2_lock);
		if (fs.pn && (fs.pn->key & KEY_MODIFIED)) {
			fs.pm = fs.pn;
			fs.pn = NULL;
		}
	} else {
		rw_spinlock_lock_ex (&cspt->shadow2_lock);
		find_shadow2_from_hash (cspt, key, &fs);
	}
	pdpe = find_shadow2 (cspt, v, &fs);
	if (pdpe) {
		pmap_write (p, pdpe, PDE_P_BIT);
		STATUS_UPDATE (asm_lock_incl (&stat_pdhitcnt));
		goto ret;
	}
	r = new_shadow2 (cspt, key, v, &pdpe, &fs);
	if (!r)
		goto ret;
	pmap_write (p, pdpe, PDE_P_BIT);
	if (gfnw != GFN_UNUSED && !(key & KEY_LARGEPAGE) &&
	    gfnw == (key >> KEY_GFN_SHIFT)) {
		modified_shadow2 (cspt, fs.pn);
		goto ret;
	}
	rw_spinlock_unlock_ex (&cspt->shadow2_lock);
	if (!makerdonly (cspt, key)) {
		STATUS_UPDATE (asm_lock_incl (&stat_pdnew2cnt));
		rw_spinlock_lock_ex (&cspt->shadow2_lock);
		modified_shadow2 (cspt, fs.pn);
		rw_spinlock_unlock_ex (&cspt->shadow2_lock);
	}
	return r;
ret:
	rw_spinlock_unlock_ex (&cspt->shadow2_lock);
	return r;
}

static bool
mapsub (spt_t *cspt, pmap_t *pp, u64 *entry, int level, bool clearall)
{
	u64 tmp;
	int l;

	if (clearall) {
		pmap_setlevel (pp, cspt->levels);
		pmap_clear (pp);
		cspt->cnt = 0;
	}
	pmap_setlevel (pp, level);
	tmp = pmap_read (pp);
	for (; (l = pmap_getreadlevel (pp)) > level; tmp = pmap_read (pp)) {
		pmap_setlevel (pp, l);
		if (cspt->cnt == NUM_OF_SPTTBL)
			return false;
		pmap_write (pp, cspt->tbl_phys[cspt->cnt++] |
			    PDE_P_BIT, PDE_P_BIT);
		pmap_setlevel (pp, l - 1);
		pmap_clear (pp);
		pmap_setlevel (pp, level);
	}
	*entry = tmp;
	return true;
}

static void
map_page (u64 v, struct map_page_data1 m1, struct map_page_data2 m2[5],
	  u64 gfns[5], int glvl)
{
	u64 hphys;
	bool fakerom;
	pmap_t p;
	u64 tmp;
	u64 key1, key2;
	u64 gfnw;
	static const int levels = 3;
	u64 pte;
	spt_t *const cspt = current->spt.data;
	const u64 mask = current->pte_addr_mask;

	STATUS_UPDATE (asm_lock_incl (&stat_mapcnt));
	if (glvl == 1) {
		key1 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LPMASK) |
			KEY_LARGEPAGE;
		key2 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LP2MASK) |
			KEY_LARGEPAGE;
	} else if (glvl == 2) {
		if (gfns[1] == GFN_GLOBAL || gfns[1] == GFN_UNUSED) {
			key1 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LPMASK) |
				KEY_LARGEPAGE;
		} else {
			key1 = (gfns[1] << KEY_GFN_SHIFT) | KEY_GLVL2;
			key1 |= ((v >> 21) & 1) << KEY_2TO3_SHIFT;
		}
		key2 = (gfns[2] << KEY_GFN_SHIFT) | KEY_GLVL2;
		key2 |= ((v >> 30) & 3) << KEY_2TO3_SHIFT;
	} else if (glvl == 3 || glvl == 4) {
		if (gfns[1] == GFN_GLOBAL || gfns[1] == GFN_UNUSED) {
			key1 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LPMASK) |
				KEY_LARGEPAGE;
		} else {
			key1 = (gfns[1] << KEY_GFN_SHIFT) | KEY_GLVL3;
		}
		if (gfns[2] == GFN_GLOBAL || gfns[2] == GFN_UNUSED) {
			key2 = ((gfns[0] << KEY_GFN_SHIFT) & KEY_LP2MASK) |
				KEY_LARGEPAGE;
		} else {
			key2 = (gfns[2] << KEY_GFN_SHIFT) | KEY_GLVL3;
		}
	} else {
		panic ("map_page: glvl error");
		for (;;);
	}
	if (!cspt->wp) {
		/* guest us,rw	user,write	shadow us,rw
			0,0	0,0		0,1	   [B]
			0,0	0,1		0,1	[A][B]
			0,1	*,*		0,1	   [B]
			1,0	0,0		1,0
			1,0	0,1		0,1	[A][B]
			1,0	1,0		1,0
			1,0	1,1		(page fault)
			1,1	*,*		1,1 */
		if (!m1.user && m1.write) {
			if (!m2[0].rw)
				m2[0].us = 0;	/* [A] */
			if (!m2[1].rw)
				m2[1].us = 0;	/* [A] */
		}
		if (!m2[0].us)
			m2[0].rw = 1;		/* [B] */
		if (!m2[1].us)
			m2[1].rw = 1;		/* [B] */
		key1 |= KEY_WP0;
		key2 |= KEY_WP0;
	}
	if (glvl >= 2) {
		if (m2[2].nx)
			key2 |= KEY_NX;
		if (!m2[2].us)
			key2 |= KEY_US0;
		if (!m2[2].rw)
			key2 |= KEY_RW0;
	}
	gfnw = GFN_UNUSED;
	if (m1.write)
		gfnw = gfns[0];
	pmap_open_vmm (&p, cspt->cr3tbl_phys, cspt->levels);
	pmap_seek (&p, v, levels);
	if (!mapsub (cspt, &p, &tmp, levels, false)) {
		if (!mapsub (cspt, &p, &tmp, levels, true))
			panic ("mapsub failed 1");
	}
	if (!setpdpe (cspt, &p, tmp, key2, gfnw, v, mask)) {
		if (!mapsub (cspt, &p, &tmp, levels, true))
			panic ("mapsub failed 2");
		if (!setpdpe (cspt, &p, tmp, key2, gfnw, v, mask))
			panic ("setpdpe failed 1");
	}
	pmap_setlevel (&p, 2);
	tmp = pmap_read (&p);
	if (!setpde (cspt, &p, tmp, key1, gfnw, v, mask, m2[1])) {
		if (!mapsub (cspt, &p, &tmp, levels, true))
			panic ("mapsub failed 3");
		if (!setpdpe (cspt, &p, tmp, key2, gfnw, v, mask))
			panic ("setpdpe failed 2");
		pmap_setlevel (&p, 2);
		tmp = pmap_read (&p);
		if (!setpde (cspt, &p, tmp, key1, gfnw, v, mask, m2[1]))
			panic ("setpde failed 1");
	}
	pmap_setlevel (&p, 1);
	tmp = pmap_read (&p);
	hphys = current->gmm.gp2hp (gfns[0] << PAGESIZE_SHIFT, &fakerom);
	if (fakerom && m1.write)
		panic ("Writing to VMM memory.");
	if (fakerom)
		m2[0].rw = 0;
	pte = hphys | PTE_P_BIT |
		(m2[0].rw ? PTE_RW_BIT : 0) |
		(m2[0].us ? PTE_US_BIT : 0) |
		(m2[0].nx ? PTE_NX_BIT : 0) |
		(m1.write ? PTE_D_BIT : 0) |
		(m1.pwt ? PTE_PWT_BIT : 0) |
		(m1.pcd ? PTE_PCD_BIT : 0) |
		(m1.pat ? PTE_PAT_BIT : 0);
	if ((tmp | PTE_A_BIT | PTE_D_BIT) == (pte | PTE_A_BIT | PTE_D_BIT))
		goto skip;
	pmap_write (&p, pte, PTE_P_BIT | PTE_RW_BIT | PTE_US_BIT |
		    PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT | PTE_D_BIT);
	if (m2[0].rw) {
		update_rwmap (cspt, gfns[0], pmap_pointer (&p), hphys);
		update_shadow (cspt, gfns[0], pmap_pointer (&p), m1.write);
	}
skip:
	pmap_close (&p);
}

static bool
extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	struct cpu_mmu_spt_shadow *q;
	u64 *e, tmp, mask = p->pte_addr_mask;
	unsigned int n = 512;
	unsigned int j;

	start &= ~PAGESIZE_MASK;
	end |= PAGESIZE_MASK;
	/* lock is not needed because this is called by mmio_register */
	/* mmio_register uses another lock to avoid conflict */
	/* with cpu_mmu_spt_pagefault() */
	LIST3_FOREACH (p->spt.data->shadow1_modified, shadow, q) {
		e = (u64 *)phys_to_virt (q->phys);
		for (j = 0; j < n; j++) {
			tmp = e[j] & mask;
			if ((e[j] & PTE_P_BIT) && start <= tmp && tmp <= end) {
				if (p != current)
					return true;
				e[j] = 0;
			}
		}
	}
	LIST3_FOREACH (p->spt.data->shadow1_normal, shadow, q) {
		e = (u64 *)phys_to_virt (q->phys);
		for (j = 0; j < n; j++) {
			tmp = e[j] & mask;
			if ((e[j] & PTE_P_BIT) && start <= tmp && tmp <= end) {
				if (p != current)
					return true;
				e[j] = 0;
			}
		}
	}
	return false;
}

static void
clear_rwmap (spt_t *cspt)
{
	int i;

	spinlock_lock (&cspt->rwmap_lock);
	LIST3_HEAD_INIT (cspt->rwmap_fail, rwmap);
	LIST3_HEAD_INIT (cspt->rwmap_normal, rwmap);
	LIST3_HEAD_INIT (cspt->rwmap_free, rwmap);
	for (i = 0; i < NUM_OF_SPTRWMAP; i++)
		LIST3_ADD (cspt->rwmap_free, rwmap, &cspt->rwmap[i]);
	for (i = 0; i < HASHSIZE_OF_SPTRWMAP; i++)
		LIST3_HEAD_INIT (cspt->rwmap_hash[i], hash);
	spinlock_unlock (&cspt->rwmap_lock);
}

static void
clear_shadow1 (spt_t *cspt)
{
	unsigned int i;

	rw_spinlock_lock_ex (&cspt->shadow1_lock);
	LIST3_HEAD_INIT (cspt->shadow1_modified, shadow);
	LIST3_HEAD_INIT (cspt->shadow1_normal, shadow);
	LIST3_HEAD_INIT (cspt->shadow1_free, shadow);
	for (i = 0; i < NUM_OF_SPTSHADOW1; i++) {
		clear_shadow (&cspt->shadow1[i]);
		LIST3_ADD (cspt->shadow1_free, shadow, &cspt->shadow1[i]);
	}
	for (i = 0; i < HASHSIZE_OF_SPTSHADOW1; i++)
		LIST3_HEAD_INIT (cspt->shadow1_hash[i], hash);
	rw_spinlock_unlock_ex (&cspt->shadow1_lock);
}

static void
clear_shadow1map (spt_t *cspt)
{
	unsigned int i;

	rw_spinlock_lock_ex (&cspt->shadow1_lock);
	LIST3_HEAD_INIT (cspt->shadow1map_free, shadow1map);
	LIST3_HEAD_INIT (cspt->shadow1map_list, shadow1map);
	for (i = 0; i < NUM_OF_SPTSHADOW1MAP; i++) {
		LIST3_ADD (cspt->shadow1map_free, shadow1map,
			   &cspt->shadow1map[i]);
	}
	rw_spinlock_unlock_ex (&cspt->shadow1_lock);
}

static void
clear_shadow2 (spt_t *cspt)
{
	unsigned int i;

	rw_spinlock_lock_ex (&cspt->shadow2_lock);
	LIST3_HEAD_INIT (cspt->shadow2_modified, shadow);
	LIST3_HEAD_INIT (cspt->shadow2_normal, shadow);
	LIST3_HEAD_INIT (cspt->shadow2_free, shadow);
	for (i = 0; i < NUM_OF_SPTSHADOW2; i++) {
		clear_shadow (&cspt->shadow2[i]);
		LIST3_ADD (cspt->shadow2_free, shadow, &cspt->shadow2[i]);
	}
	for (i = 0; i < NUM_OF_SPTSHADOW2; i++)
		clear_shadow (&cspt->shadow2[i]);
	for (i = 0; i < HASHSIZE_OF_SPTSHADOW2; i++)
		LIST3_HEAD_INIT (cspt->shadow2_hash[i], hash);
	rw_spinlock_unlock_ex (&cspt->shadow2_lock);
}

static void
invalidate_page (ulong v)
{
	pmap_t p;
	u64 tmp;
	spt_t *const cspt = current->spt.data;

	STATUS_UPDATE (asm_lock_incl (&stat_invlpgcnt));
	if (true) {		/* FIXME: clean* seems good but slow */
		rw_spinlock_lock_sh (&cspt->shadow1_lock);
		clean_modified_shadow1 (cspt, false);
		rw_spinlock_unlock_sh (&cspt->shadow1_lock);
		rw_spinlock_lock_sh (&cspt->shadow2_lock);
		clean_modified_shadow2 (cspt, false);
		rw_spinlock_unlock_sh (&cspt->shadow2_lock);
		return;
	}
	pmap_open_vmm (&p, cspt->cr3tbl_phys, cspt->levels);
	pmap_seek (&p, v, 2);
	tmp = pmap_read (&p);
	if (tmp & PDE_P_BIT) {
		if (tmp & PDE_AVAILABLE1_BIT) {
			pmap_write (&p, 0, 0xFFF);
			if (cspt->levels >= 3) {
				pmap_seek (&p, v ^ PAGESIZE2M, 2);
				tmp = pmap_read (&p);
				if (tmp & PDE_P_BIT)
					pmap_write (&p, 0, 0xFFF);
			}
		} else {
			pmap_setlevel (&p, 1);
			tmp = pmap_read (&p);
			pmap_write (&p, 0, 0xFFF);
		}
	}
	pmap_close (&p);
}

static void
update_cr3 (void)
{
	pmap_t p;
	ulong cr0, cr3, cr4;
	u64 efer;
	spt_t *const cspt = current->spt.data;

	STATUS_UPDATE (asm_lock_incl (&stat_cr3cnt));
	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr0);
	current->vmctl.read_control_reg (CONTROL_REG_CR3, &cr3);
	current->vmctl.read_control_reg (CONTROL_REG_CR4, &cr4);
	current->vmctl.read_msr (MSR_IA32_EFER, &efer);
	if (cr0 != cspt->cr0 || cr3 != cspt->cr3 ||
	    cr4 != cspt->cr4 || efer != cspt->efer) {
		cspt->cr0 = cr0;
		cspt->cr3 = cr3;
		cspt->cr4 = cr4;
		cspt->efer = efer;
	} else {
		/* fast path */
		rw_spinlock_lock_sh (&cspt->shadow1_lock);
		clean_modified_shadow1 (cspt, false);
		rw_spinlock_unlock_sh (&cspt->shadow1_lock);
		rw_spinlock_lock_sh (&cspt->shadow2_lock);
		clean_modified_shadow2 (cspt, false);
		rw_spinlock_unlock_sh (&cspt->shadow2_lock);
		return;
	}
#ifdef CPU_MMU_SPT_USE_PAE
	cspt->levels = guest64 () ? 4 : 3;
#else
	cspt->levels = 2;
#endif
	pmap_open_vmm (&p, cspt->cr3tbl_phys, cspt->levels);
	pmap_clear (&p);
	pmap_close (&p);
	cspt->cnt = 0;
	current->vmctl.spt_setcr3 (cspt->cr3tbl_phys);
	if (cspt->wp && !(cr0 & CR0_WP_BIT)) {
		cspt->wp = false;
		STATUS_UPDATE (asm_lock_incl (&stat_wpcnt));
	}
	if (!cspt->wp && (cr0 & CR0_WP_BIT)) {
		cspt->wp = true;
		STATUS_UPDATE (asm_lock_incl (&stat_wpcnt));
	}
	update_rwmap (cspt, 0, NULL, 0);
	rw_spinlock_lock_ex (&cspt->shadow1_lock);
	clean_modified_shadow1 (cspt, true);
	rw_spinlock_unlock_ex (&cspt->shadow1_lock);
	rw_spinlock_lock_ex (&cspt->shadow2_lock);
	clean_modified_shadow2 (cspt, true);
	rw_spinlock_unlock_ex (&cspt->shadow2_lock);
}

static void
clear_all (void)
{
	pmap_t p;
	struct cpu_mmu_spt_shadow *q, *qn;
	spt_t *const cspt = current->spt.data;

	pmap_open_vmm (&p, cspt->cr3tbl_phys, cspt->levels);
	pmap_clear (&p);
	pmap_close (&p);
	cspt->cnt = 0;
	current->vmctl.spt_setcr3 (cspt->cr3tbl_phys);
	rw_spinlock_lock_ex (&cspt->shadow1_lock);
	LIST3_FOREACH_DELETABLE (cspt->shadow1_normal, shadow, q, qn)
		modified_shadow1 (cspt, q);
	clean_modified_shadow1 (cspt, true);
	rw_spinlock_unlock_ex (&cspt->shadow1_lock);
	rw_spinlock_lock_ex (&cspt->shadow2_lock);
	LIST3_FOREACH_DELETABLE (cspt->shadow2_normal, shadow, q, qn)
		modified_shadow2 (cspt, q);
	clean_modified_shadow2 (cspt, true);
	rw_spinlock_unlock_ex (&cspt->shadow2_lock);
}

static bool
spt_tlbflush (void)
{
	return update_rwmap (current->spt.data, 0, NULL, 0);
}

static char *
spt_status (void)
{
	static char buf[1024];

	snprintf (buf, 1024,
		  "MMU:\n"
		  " MOV CR3: %u INVLPG: %u\n"
		  " Map: %u WP: %u Clear: %u, %u\n"
		  "Shadow page table:\n"
		  " Found: %u  Full: %u New: %u\n"
		  " Good: %u Hit: %u New2: %u\n"
		  "Shadow page directory:\n"
		  " Found: %u  Full: %u New: %u\n"
		  " Good: %u Hit: %u New2: %u\n"
		  , stat_cr3cnt, stat_invlpgcnt
		  , stat_mapcnt, stat_wpcnt, stat_clrcnt, stat_clr2cnt
		  , stat_ptfoundcnt, stat_ptfullcnt, stat_ptnewcnt
		  , stat_ptgoodcnt, stat_pthitcnt, stat_ptnew2cnt
		  , stat_pdfoundcnt, stat_pdfullcnt, stat_pdnewcnt
		  , stat_pdgoodcnt, stat_pdhitcnt, stat_pdnew2cnt);
	return buf;
}

static void
init_global (void)
{
	LIST1_HEAD_INIT (list1_spt);
	register_status_callback (spt_status);
}

static void
init_pcpu (void)
{
}

static int
init_vcpu (void)
{
	int i;
	struct sptlist *listspt;
	spt_t *cspt;

	cspt = alloc (sizeof *cspt);
	memset (cspt, 0, sizeof *cspt);
	current->spt.data = cspt;
	alloc_page (&cspt->cr3tbl, &cspt->cr3tbl_phys);
	for (i = 0; i < NUM_OF_SPTTBL; i++)
		alloc_page (&cspt->tbl[i], &cspt->tbl_phys[i]);
	cspt->cnt = 0;
	memset (cspt->cr3tbl, 0, PAGESIZE);
	for (i = 0; i < NUM_OF_SPTSHADOW1; i++) {
		alloc_page (NULL, &cspt->shadow1[i].phys);
		cspt->shadow1[i].key = 0;
		cspt->shadow1[i].clear_n = NUM_OF_SPTSHADOWOFF + 1;
		cspt->shadow1[i].clear_area = ~0ULL;
		LIST3_HEAD_INIT (cspt->shadow1[i].shadow1map_ref, ref);
	}
	for (i = 0; i < NUM_OF_SPTSHADOW2; i++) {
		alloc_page (NULL, &cspt->shadow2[i].phys);
		cspt->shadow2[i].key = 0;
		cspt->shadow2[i].clear_n = NUM_OF_SPTSHADOWOFF + 1;
		cspt->shadow2[i].clear_area = ~0ULL;
	}
	spinlock_init (&cspt->rwmap_lock);
	rw_spinlock_init (&cspt->shadow1_lock);
	rw_spinlock_init (&cspt->shadow2_lock);
	clear_rwmap (cspt);
	clear_shadow1 (cspt);
	clear_shadow1map (cspt);
	clear_shadow2 (cspt);
	cspt->wp = false;
	cspt->cr0 = ~0;
	cspt->cr3 = 0;
	cspt->cr4 = 0;
	cspt->efer = 0;
	listspt = alloc (sizeof *listspt);
	listspt->spt = cspt;
	LIST1_PUSH (list1_spt, listspt);
	return 0;
}
#endif /* CPU_MMU_SPT_3 */

static bool
guest64 (void)
{
	u64 efer;

	current->vmctl.read_msr (MSR_IA32_EFER, &efer);
	if (efer & MSR_IA32_EFER_LMA_BIT)
		return true;
	return false;
}

static void
generate_pf_noaccess (u32 err, ulong cr2)
{
	err |= PAGEFAULT_ERR_P_BIT;
	current->vmctl.generate_pagefault (err, cr2);
}

static void
generate_pf_nopage (u32 err, ulong cr2)
{
	err &= ~PAGEFAULT_ERR_P_BIT;
	current->vmctl.generate_pagefault (err, cr2);
}

static void
generate_pf_reserved (u32 err, ulong cr2)
{
	err |= PAGEFAULT_ERR_P_BIT;
	err |= PAGEFAULT_ERR_RSVD_BIT;
	current->vmctl.generate_pagefault (err, cr2);
}

static void
generate_pf_noexec (u32 err, ulong cr2)
{
	err |= PAGEFAULT_ERR_P_BIT;
	err |= PAGEFAULT_ERR_ID_BIT;
	current->vmctl.generate_pagefault (err, cr2);
}

/* this function is called when shadow page table entries can be cleared
   from TLB. */
void
cpu_mmu_spt_tlbflush (void)
{
	if (spt_tlbflush ())
		current->vmctl.spt_tlbflush ();
}

/* this function is called when a guest sets CR3 */
void
cpu_mmu_spt_updatecr3 (void)
{
	update_cr3 ();
	acpi_smi_hook ();
	current->vmctl.spt_tlbflush ();
}

/* this function is called by INVLPG in a guest */
/* deletes TLB entry regardless of G bit */
void
cpu_mmu_spt_invalidate (ulong virtual_addr)
{
	invalidate_page (virtual_addr);
	current->vmctl.spt_tlbflush ();
}

static void
set_m1 (u32 attr, bool write, bool user, bool wp, struct map_page_data1 *m1)
{
	m1->write = write;
	m1->user = user;
	m1->wp = wp;
	m1->pwt = !!(attr & PTE_PWT_BIT);
	m1->pcd = !!(attr & PTE_PCD_BIT);
	m1->pat = !!(attr & PTE_PAT_BIT);
}

static void
set_m2 (u64 entries[5], int levels, struct map_page_data2 m2[5])
{
	unsigned int rw = 1, us = 1, nx = 0;
	int i;
	u64 entry;

	for (i = levels; i >= 0; i--) {
		entry = entries[i];
		if (!(entry & PDE_RW_BIT))
			rw = 0;
		if (!(entry & PDE_US_BIT))
			us = 0;
		if (entry & PDE_NX_BIT)
			nx = 1;
		m2[i].rw = rw;
		m2[i].us = us;
		m2[i].nx = nx;
	}
	if (!(entries[0] & PTE_D_BIT))
		m2[0].rw = 0;
}

static void
set_gfns (u64 entries[5], int levels, u64 gfns[5])
{
	int i;
	u64 entry;
	const u64 mask = current->pte_addr_mask;

	for (i = levels; i >= 0; i--) {
		entry = entries[i];
		if ((i == 1 || i == 2) && (entry & PDE_PS_BIT)) {
			if (entry & PDE_G_BIT)
				gfns[i] = GFN_GLOBAL;
			else
				gfns[i] = GFN_UNUSED;
			continue;
		}
		gfns[i] = (entry & mask) >> PAGESIZE_SHIFT;
	}
}

/* handling a page fault of a guest */
void
cpu_mmu_spt_pagefault (ulong err, ulong cr2)
{
	bool map;
	int levels;
	enum vmmerr r;
	bool wr, us, ex, wp;
	ulong cr0, cr3, cr4;
	struct map_page_data1 m1;
	struct map_page_data2 m2[5];
	u64 efer, gfns[5], entries[5];
	u32 attr;

	get_cr0_cr3_cr4_and_efer (&cr0, &cr3, &cr4, &efer);
	wr = !!(err & PAGEFAULT_ERR_WR_BIT);
	us = !!(err & PAGEFAULT_ERR_US_BIT);
	ex = !!(err & PAGEFAULT_ERR_ID_BIT);
	wp = !!(cr0 & CR0_WP_BIT);
	r = cpu_mmu_get_pte (cr2, cr0, cr3, cr4, efer, wr, us, ex, entries,
			     &levels);
	switch (r) {
	case VMMERR_PAGE_NOT_PRESENT:
		generate_pf_nopage (err, cr2);
		break;
	case VMMERR_PAGE_NOT_ACCESSIBLE:
		generate_pf_noaccess (err, cr2);
		break;
	case VMMERR_PAGE_BAD_RESERVED_BIT:
		generate_pf_reserved (err, cr2);
		break;
	case VMMERR_PAGE_NOT_EXECUTABLE:
		generate_pf_noexec (err, cr2);
		break;
	case VMMERR_SUCCESS:
		set_gfns (entries, levels, gfns);
		attr = cache_get_attr (gfns[0] << PAGESIZE_SHIFT,
				       entries[0] & PTE_ATTR_MASK);
		set_m1 (attr, wr, us, wp, &m1);
		set_m2 (entries, levels, m2);
		map = false;
		mmio_lock ();
		if (!mmio_access_page (gfns[0] << PAGESIZE_SHIFT, true)) {
			map_page (cr2, m1, m2, gfns, levels);
			map = true;
		}
		mmio_unlock ();
		if (map)
			current->vmctl.spt_tlbflush ();
		break;
	default:
		panic ("unknown err");
	}
}

bool
cpu_mmu_spt_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end)
{
	return extern_mapsearch (p, start, end);
}

/* Workaround for freezing in some BIOSes (SMI related problem) */
void
cpu_mmu_spt_map_1mb (void)
{
	int levels;
	enum vmmerr r;
	bool wr, us, ex, wp;
	ulong cr0, cr3, cr4;
	struct map_page_data1 m1;
	struct map_page_data2 m2[5];
	u64 efer, gfns[5], entries[5];
	ulong cr2;
	bool map = false;
	u32 attr;

	get_cr0_cr3_cr4_and_efer (&cr0, &cr3, &cr4, &efer);
	wr = false;
	us = true;
	ex = false;
	wp = !!(cr0 & CR0_WP_BIT);
	for (cr2 = 0; cr2 < 0x100000; cr2 += PAGESIZE) {
		r = cpu_mmu_get_pte (cr2, cr0, cr3, cr4, efer, wr, us, ex,
				     entries, &levels);
		if (r != VMMERR_SUCCESS)
			continue;
		set_gfns (entries, levels, gfns);
		attr = cache_get_attr (gfns[0] << PAGESIZE_SHIFT,
				       entries[0] & PTE_ATTR_MASK);
		set_m1 (attr, wr, us, wp, &m1);
		set_m2 (entries, levels, m2);
		mmio_lock ();
		if (!mmio_access_page (gfns[0] << PAGESIZE_SHIFT, false)) {
			map_page (cr2, m1, m2, gfns, levels);
			map = true;
		}
		mmio_unlock ();
	}
	if (map)
		current->vmctl.spt_tlbflush ();
}

void
cpu_mmu_spt_clear_all (void)
{
	clear_all ();
}

static void
cpu_mmu_spt_init_global (void)
{
	init_global ();
}

static void
cpu_mmu_spt_init_pcpu (void)
{
	init_pcpu ();
}

int
cpu_mmu_spt_init (void)
{
	return init_vcpu ();
}

INITFUNC ("global4", cpu_mmu_spt_init_global);
INITFUNC ("pcpu0", cpu_mmu_spt_init_pcpu);
