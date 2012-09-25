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

#include "assert.h"
#include "constants.h"
#include "cpu_mmu.h"
#include "current.h"
#include "gmm_access.h"
#include "mm.h"
#include "panic.h"
#include "printf.h"
#include "string.h"

struct get_pte_data {
	unsigned int pg : 1;	/* PG (Paging): CR0 bit 31 */
	unsigned int wp : 1;	/* WP (Write Protect): CR0 bit 16 */
	unsigned int pse : 1;	/* PSE (Page Size Extensions): CR4 bit 4 */
	unsigned int pae : 1;	/* PAE (Physical Address Extensions):CR4bit5 */
	unsigned int lme : 1;	/* LME (Long Mode Enable): MSR(EFER) bit 8 */
	unsigned int nxe : 1;	/* NXE (No-Execute Enable):MSR(EFER) bit 10 */
	unsigned int write : 1;	/* Write access. Set D bit if 1 */
	unsigned int user : 1;	/* User access */
	unsigned int exec : 1;	/* Execution */
};

static u64 reserved_bit_table[2][3][6] = {
	/* NXE = 0 */
	{
		/* levels = 2 (32-bit, PAE = 0, PSE = 1) */
		{
			0x0000000000200000ULL, /* PDE, 4-MByte page */
			0x0000000000000000ULL, /* PTE */
			0x0000000000000000ULL, /* PDE, 4-KByte page */
			0xFFFFFFFFFFFFFFFFULL, /* PDPE */
			0xFFFFFFFFFFFFFFFFULL, /* PML4E */
			0xFFFFFFFFFFFFFFFFULL, /* PDPE+PS */
		},
		/* levels = 3 (32-bit, PAE = 1) */
		{
			0xFFFFFFF0001FE000ULL, /* PDE, 2-MByte page */
			0xFFFFFFF000000000ULL, /* PTE */
			0xFFFFFFF000000000ULL, /* PDE, 4-KByte page */
			0xFFFFFFF0000001E6ULL, /* PDPE */
			0xFFFFFFFFFFFFFFFFULL, /* PML4E */
			0xFFFFFFFFFFFFFFFFULL, /* PDPE+PS */
		},
		/* levels = 4 (64-bit) */
		{
			0x800FFFF0001FE000ULL, /* PDE, 2-MByte page */
			0x800FFFF000000000ULL, /* PTE */
			0x800FFFF000000000ULL, /* PDE, 4-KByte page */
			0x800FFFF000000100ULL, /* PDPE */
			0x800FFFF000000180ULL, /* PML4E */
			0x800FFFF03FFFE000ULL, /* PDPE+PS */
		},
	},
	/* NXE = 1 */
	{
		/* levels = 2 (32-bit, PAE = 0, PSE = 1) */
		{
			0x0000000000200000ULL, /* PDE, 4-MByte page */
			0x0000000000000000ULL, /* PTE */
			0x0000000000000000ULL, /* PDE, 4-KByte page */
			0xFFFFFFFFFFFFFFFFULL, /* PDPE */
			0xFFFFFFFFFFFFFFFFULL, /* PML4E */
			0xFFFFFFFFFFFFFFFFULL, /* PDPE+PS */
		},
		/* levels = 3 (32-bit, PAE = 1) */
		{
			0x7FFFFFF0001FE000ULL, /* PDE, 2-MByte page */
			0x7FFFFFF000000000ULL, /* PTE */
			0x7FFFFFF000000000ULL, /* PDE, 4-KByte page */
			0xFFFFFFF0000001E6ULL, /* PDPE */
			0xFFFFFFFFFFFFFFFFULL, /* PML4E */
			0xFFFFFFFFFFFFFFFFULL, /* PDPE+PS */
		},
		/* levels = 4 (64-bit) */
		{
			0x000FFFF0001FE000ULL, /* PDE, 2-MByte page */
			0x000FFFF000000000ULL, /* PTE */
			0x000FFFF000000000ULL, /* PDE, 4-KByte page */
			0x000FFFF000000100ULL, /* PDPE */
			0x000FFFF000000180ULL, /* PML4E */
			0x000FFFF03FFFE000ULL, /* PDPE+PS */
		},
	},
};		

static bool
test_pmap_entry_reserved_bit (u64 entry, int level, int levels,
			      struct get_pte_data d)
{
	u64 mask;

	if (!d.pae && !d.pse)	/* PAE = 0, PSE = 0 */
		return false;	/* No reserved bits checked */
	if (level == 2 && (entry & PDE_PS_BIT))
		level = 0;
	if (level == 3 && levels == 4 && (entry & PDE_PS_BIT))
		level = 5;
	mask = reserved_bit_table[d.nxe][levels - 2][level];
	mask ^= current->pte_addr_mask ^ PTE_ADDR_MASK64;
	ASSERT (mask != 0xFFFFFFFFFFFFFFFFULL);
	if (entry & mask)
		return true;
	return false;
}

static bool
set_ad_bit (pmap_t *m, u64 *entry, bool seta, bool setd)
{
	if (*entry & PDE_A_BIT)
		seta = false;
	if (*entry & PDE_D_BIT)
		setd = false;
	if (seta || setd) {
		if (seta)
			*entry |= PDE_A_BIT;
		if (setd)
			*entry |= PDE_D_BIT;
		return pmap_write (m, *entry, 0xFFF);
	} else {
		return false;
	}
}

/* simplify1: entries[0] & entries[1] are generated when paging is disabled */
/* simplify2: entries[2] (PDPE) |= RW|US|A when 32-bit PAE */
/* simplify3: entries[1] (PDE) &= ~PS when PSE is disabled */
/* simplify : entries[] are 64-bit when PAE is disabled */
/* simplify : entries[0] (PTE) will be generated when it is in a large page */
static enum vmmerr
get_pte_sub (ulong virt, ulong cr3, struct get_pte_data d, u64 entries[5],
	     int *plevels)
{
	pmap_t m;
	int levels;
	int i;
	u64 entry;
	enum vmmerr r;
	bool setd;

	if (!d.pg) {
		/* Paging disabled */
		/* simplify1 */
		entries[0] = (virt & current->pte_addr_mask) | PTE_P_BIT
			| PTE_RW_BIT | PTE_US_BIT | PTE_A_BIT | PTE_D_BIT;
		entries[1] = PDE_P_BIT | PDE_RW_BIT | PDE_US_BIT | PDE_A_BIT
			| PDE_D_BIT | PDE_PS_BIT;
		*plevels = 1;
		return VMMERR_SUCCESS;
	}
	/* Paging enabled */
	if (d.lme) {
		d.pae = 1;
		d.pse = 1;
		levels = 4;
	} else if (d.pae) {
		d.pse = 1;
		levels = 3;
	} else {
		levels = 2;
	}
	*plevels = levels;
	pmap_open_guest (&m, cr3, levels, true);
	pmap_seek (&m, virt, levels + 1);
	entry = pmap_read (&m);
	entry |= PDE_RW_BIT | PDE_US_BIT | PDE_A_BIT;
	entries[levels] = entry;
	for (i = levels; i >= 1; i--) {
		pmap_setlevel (&m, i);
	retry:
		entry = pmap_read (&m);
		if (!(entry & PDE_P_BIT))
			goto ret_nopage;
		if (test_pmap_entry_reserved_bit (entry, i, levels, d))
			goto ret_reserved;
		if (levels == 3 && i == 3) /* simplify2 */
			entry |= PDE_RW_BIT | PDE_US_BIT | PDE_A_BIT;
		if (!(entry & PDE_US_BIT) && d.user)
			goto ret_noaccess;
		if (!(entry & PDE_RW_BIT) && d.write && (d.wp || d.user))
			goto ret_noaccess;
		if ((entry & PDE_NX_BIT) && d.exec && d.nxe)
			goto ret_noexec;
		if (i == 2 && !d.pse) /* simplify3 */
			entry &= ~(u64)PDE_PS_BIT;
		setd = false;
		if ((i == 2 && (entry & PDE_PS_BIT)) || i == 1 ||
		    (i == 3 && (entry & PDE_PS_BIT)))
			setd = d.write;
		if (set_ad_bit (&m, &entry, true, setd))
			goto retry;
		entries[i - 1] = entry;
	}
	r = VMMERR_SUCCESS;
ret:
	pmap_close (&m);
	return r;
ret_nopage:
	r = VMMERR_PAGE_NOT_PRESENT;
	goto ret;
ret_reserved:
	r = VMMERR_PAGE_BAD_RESERVED_BIT;
	goto ret;
ret_noaccess:
	r = VMMERR_PAGE_NOT_ACCESSIBLE;
	goto ret;
ret_noexec:
	r = VMMERR_PAGE_NOT_EXECUTABLE;
	goto ret;
}

enum vmmerr
cpu_mmu_get_pte (ulong virt, ulong cr0, ulong cr3, ulong cr4, u64 efer,
		 bool write, bool user, bool exec, u64 entries[5],
		 int *plevels)
{
	struct get_pte_data d;

	d.pg = !!(cr0 & CR0_PG_BIT);
	d.wp = !!(cr0 & CR0_WP_BIT);
	d.pse = !!(cr4 & CR4_PSE_BIT);
	d.pae = !!(cr4 & CR4_PAE_BIT);
	d.lme = !!(efer & MSR_IA32_EFER_LME_BIT);
	d.nxe = !!(efer & MSR_IA32_EFER_NXE_BIT);
	d.write = write;
	d.user = user;
	d.exec = exec;
	return get_pte_sub (virt, cr3, d, entries, plevels);
}

static enum vmmerr
get_pte (ulong virt, bool wr, bool us, bool ex, u64 *pte)
{
	int levels;
	enum vmmerr r;
	u64 entries[5];
	u64 efer;
	ulong cr0, cr3, cr4;

	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr0);
	current->vmctl.read_control_reg (CONTROL_REG_CR3, &cr3);
	current->vmctl.read_control_reg (CONTROL_REG_CR4, &cr4);
	current->vmctl.read_msr (MSR_IA32_EFER, &efer);
	r = cpu_mmu_get_pte (virt, cr0, cr3, cr4, efer, wr, us, ex, entries,
			     &levels);
	if (r == VMMERR_SUCCESS)
		*pte = entries[0];
	return r;
}

enum vmmerr
write_linearaddr_ok_b (ulong linear)
{
	u64 pte;

	RIE (get_pte (linear, true, false /*FIXME*/, false /*FIXME*/, &pte));
	return VMMERR_SUCCESS;
}

enum vmmerr
write_linearaddr_ok_w (ulong linear)
{
	if ((linear & 0xFFE) <= 0xFFE) {
		RIE (write_linearaddr_ok_b (linear));
	} else {
		RIE (write_linearaddr_ok_b (linear));
		RIE (write_linearaddr_ok_b (linear + 1));
	}
	return VMMERR_SUCCESS;
}

enum vmmerr
write_linearaddr_ok_l (ulong linear)
{
	if ((linear & 0xFFC) <= 0xFFC) {
		RIE (write_linearaddr_ok_w (linear));
	} else {
		RIE (write_linearaddr_ok_w (linear));
		RIE (write_linearaddr_ok_w (linear + 2));
	}
	return VMMERR_SUCCESS;
}

enum vmmerr
write_linearaddr_b (ulong linear, u8 data)
{
	u64 pte;

	RIE (get_pte (linear, true, false /*FIXME*/, false /*FIXME*/, &pte));
	write_gphys_b ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		       pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

enum vmmerr
write_linearaddr_w (ulong linear, u16 data)
{
	u64 pte;

	if ((linear & 0xFFF) == 0xFFF) {
		RIE (write_linearaddr_b (linear, data));
		RIE (write_linearaddr_b (linear + 1, data >> 8));
		return VMMERR_SUCCESS;
	}
	RIE (get_pte (linear, true, false /*FIXME*/, false /*FIXME*/, &pte));
	write_gphys_w ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		       pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

enum vmmerr
write_linearaddr_l (ulong linear, u32 data)
{
	u64 pte;

	if ((linear & 0xFFF) >= 0xFFD) {
		RIE (write_linearaddr_w (linear, data));
		RIE (write_linearaddr_w (linear + 2, data >> 16));
		return VMMERR_SUCCESS;
	}
	RIE (get_pte (linear, true, false /*FIXME*/, false /*FIXME*/, &pte));
	write_gphys_l ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		       pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

enum vmmerr
write_linearaddr_q (ulong linear, u64 data)
{
	u64 pte;

	if ((linear & 0xFFF) >= 0xFF9) {
		RIE (write_linearaddr_l (linear, data));
		RIE (write_linearaddr_l (linear + 4, data >> 32));
		return VMMERR_SUCCESS;
	}
	RIE (get_pte (linear, true, false /*FIXME*/, false /*FIXME*/, &pte));
	write_gphys_q ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		       pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

enum vmmerr
read_linearaddr_b (ulong linear, void *data)
{
	u64 pte;

	RIE (get_pte (linear, false, false /*FIXME*/, false /*FIXME*/, &pte));
	read_gphys_b ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		      pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

enum vmmerr
read_linearaddr_w (ulong linear, void *data)
{
	u64 pte;

	if ((linear & 0xFFF) == 0xFFF) {
		RIE (read_linearaddr_b (linear, ((u8 *)data)));
		RIE (read_linearaddr_b (linear + 1, ((u8 *)data + 1)));
		return VMMERR_SUCCESS;
	}
	RIE (get_pte (linear, false, false /*FIXME*/, false /*FIXME*/, &pte));
	read_gphys_w ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		      pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

enum vmmerr
read_linearaddr_l (ulong linear, void *data)
{
	u64 pte;

	if ((linear & 0xFFF) >= 0xFFD) {
		RIE (read_linearaddr_w (linear, ((u8 *)data)));
		RIE (read_linearaddr_w (linear + 2, ((u8 *)data + 2)));
		return VMMERR_SUCCESS;
	}
	RIE (get_pte (linear, false, false /*FIXME*/, false /*FIXME*/, &pte));
	read_gphys_l ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		      pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

enum vmmerr
read_linearaddr_q (ulong linear, void *data)
{
	u64 pte;

	if ((linear & 0xFFF) >= 0xFF9) {
		RIE (read_linearaddr_l (linear, ((u8 *)data)));
		RIE (read_linearaddr_l (linear + 4, ((u8 *)data + 4)));
		return VMMERR_SUCCESS;
	}
	RIE (get_pte (linear, false, false /*FIXME*/, false /*FIXME*/, &pte));
	read_gphys_q ((pte & current->pte_addr_mask) | (linear & 0xFFF), data,
		      pte & (PTE_PWT_BIT | PTE_PCD_BIT | PTE_PAT_BIT));
	return VMMERR_SUCCESS;
}

/* access a TSS during a task switch */
/* According to the manual, a processor uses contiguous physical addresses */
/* to access a TSS during a task switch */ 
enum vmmerr
read_linearaddr_tss (ulong linear, void *tss, uint len)
{
	u64 pte;
	void *p;

	RIE (get_pte (linear, false, false, false, &pte));
	p = mapmem_gphys ((pte & current->pte_addr_mask) | (linear & 0xFFF),
			  len, 0);
	if (!p)
		return VMMERR_NOMEM;
	memcpy (tss, p, len);
	unmapmem (p, len);
	return VMMERR_SUCCESS;
}

enum vmmerr
write_linearaddr_tss (ulong linear, void *tss, uint len)
{
	u64 pte;
	void *p;

	RIE (get_pte (linear, true, false, false, &pte));
	p = mapmem_gphys ((pte & current->pte_addr_mask) | (linear & 0xFFF),
			  len, MAPMEM_WRITE);
	if (!p)
		return VMMERR_NOMEM;
	memcpy (p, tss, len);
	unmapmem (p, len);
	return VMMERR_SUCCESS;
}
