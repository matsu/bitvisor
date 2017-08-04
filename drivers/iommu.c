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

#include <common.h>
#include <core.h>
#include "passthrough/vtd.h"
#if defined(PAGESIZE)
#undef PAGESIZE
#endif
#include "../core/mm.h"
#include "passthrough/iodom.h"
#include "passthrough/intel-iommu.h"
#include "passthrough/dmar.h"

#include "pci.h"
extern struct list pci_device_list_head;

static inline void asm_wbinvd (void)
{
	asm volatile ("wbinvd");
}

u32 vmm_start_inf();
u32 vmm_term_inf();
struct acpi_drhd_u * matched_drhd_u();

extern struct list drhd_list_head;

/* DMA Remapping Information */
struct remap_inf {
	u8 bus;
	union {
		u8 devfn ;
		struct {
			unsigned int func_no: 3 ;
			unsigned int dev_no: 5 ;
		} df ;
	} ;
	int phys;  // by Page Frame Number (==physical add >> 12)
	int num_pages;
	int perm;  // READ (1), WRITE (2), RW (1&2)
	int dom;
};

static struct remap_inf rem[256];
static int num_remap=0;

static inline unsigned int cpuid_ebx(unsigned int op)
{
	unsigned int eax, ebx, ecx, edx;

	asm volatile ("cpuid"
		      : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
		      : "a" (op));
	return ebx;
}

unsigned int clflush_size;
#define clflush(a) asm volatile ("clflush (%0)" : : "r"(a))

static void clflush_seq(struct iommu *iommu, void *addr, int size)
{
	int i=0 ;
	
	if (ecap_c(iommu->ecap)) return; // 'Coherency' field of ECAP register
	
	while (i<size) {
		clflush(addr + i);
		i+=clflush_size ;
	} ;
}

#define inval_cache_dw(iommu, addr) clflush_seq(iommu, addr, 8)
#define inval_cache_pg(iommu, addr) clflush_seq(iommu, addr, PAGESIZE)

/* Setup device context information */
static struct context_entry *devid_to_context(struct iommu *iommu, u8 bus, u8 devfn)
{
	struct root_entry *root;
	struct context_entry *context;
	void *virt;
	phys_t phys;
	int ret;
	
	spinlock_lock(&iommu->unit_lock);
	root = &iommu->root_entry[bus];
	
	if (!root_entry_present(*root))
	{
		ret = alloc_page(&virt, &phys);
		if (ret != 0) {
			spinlock_unlock(&iommu->unit_lock);
			return NULL;
		}
		memset((void *)(virt) , 0, PAGESIZE);
		inval_cache_pg(iommu, (void *)(virt));
		set_ctp(*root, phys);
		set_root_present(*root);
		inval_cache_dw(iommu, root);
	}
	context = (struct context_entry *)mapmem_hphys((unsigned long) root_entry_ctp(*root), PAGESIZE, MAPMEM_PCD);
	spinlock_unlock(&iommu->unit_lock);
	
	return &context[devfn];
}

static phys_t buildup_iopt(struct domain *dom, u64 addr)
{
	struct acpi_drhd_u *drhd;
	struct iommu *iommu;
	int addr_width;
	struct iopt_entry *parent, *pte = NULL;
	int level;
	int offset;
	phys_t pgaddr ;
	u64 *vaddr = NULL;
	
	void *virt; 
	phys_t phys;
	int ret;
	
	level = dom->agaw + 2; // level of iommu page table
	addr_width = level * IOPT_LEVEL_STRIDE + 12; // guest address width
	
	drhd = drhd_list_head.next;
	iommu = drhd->iommu;
	
	addr &= (((typeof(addr))1) << addr_width) - 1;
	
	spinlock_lock(&dom->iopt_lock);
	
	if (!dom->pgd) // if NOT prepared ...
	{
		ret = alloc_page(&virt, &phys);
		if (ret!=0) {
			spinlock_unlock(&dom->iopt_lock);
			return 0x0;
		}
		
		vaddr = (u64 *)virt;
		memset(vaddr, 0, PAGESIZE);
		dom->pgd = (void *)(long)phys;
	}
	
	parent = mapmem_hphys((unsigned long)dom->pgd, PAGESIZE, MAPMEM_PCD);
	
	do {
		offset = iopt_level_offset(addr, level);
		pte = &parent[offset];
		
		if (get_pte_addr(*pte) == 0) { // if iommu page table NOT prepared ...
			ret = alloc_page(&virt, &phys);
			pgaddr = phys;
			vaddr = (u64 *)virt;
			if (ret!=0) {
				spinlock_unlock(&dom->iopt_lock);
				return 0x0;
			}
			
			memset(vaddr, 0, PAGESIZE);
			inval_cache_pg(iommu, vaddr);
			
			set_pte_addr(*pte, (pgaddr & PAGE_MASK));
			set_pte_perm(*pte, PERM_DMA_RW);
			
			inval_cache_dw(iommu, pte);
		} else { // if iommu page table ALREADY prepared ...
			pgaddr = get_pte_addr(*pte);
			vaddr = mapmem_hphys(pgaddr, PAGESIZE, MAPMEM_PCD);
			if (!vaddr) {
				spinlock_unlock(&dom->iopt_lock);
				return 0x0;
			}
		}
		
		if (parent != dom->pgd)
			unmapmem(parent, PAGESIZE);
		
		if (level == 2 && vaddr) {
			unmapmem(vaddr, PAGESIZE);
			break;
		}
		
		parent = (struct iopt_entry *)vaddr;
		vaddr = NULL;
	} while (--level > 1) ;
	spinlock_unlock(&dom->iopt_lock);
	return pgaddr;
}

static void gcmd_wbf(struct iommu *iommu)
{
	u32 val;
	
	if (!cap_rwbf(iommu->cap)) {
		// no need for write-buffer flushing to ensure changes to 
		// memory-resident structures are visible to hardware
		
		return;
	}
	val = iommu->gcmd | GCMD_WBF;
	
	spinlock_lock(&iommu->reg_lock);
	write_hphys_l(iommu->reg+ GCMD_REG, val, MAPMEM_PCD);
	
	// wait until completion
	for (;;) {
		read_hphys_l(iommu->reg+ GSTS_REG, &val, MAPMEM_PCD);		
		if (!(val & GSTS_WBFS))
			break;
		asm_rep_and_nop();
	}
	spinlock_unlock(&iommu->reg_lock);
}

// Context-Cache global invalidation
static int invalidate_context_cache(struct iommu *iommu)
{
	u64 val = CCMD_GLOBAL_INVL | CCMD_ICC;
	
	spinlock_lock(&iommu->reg_lock);
	write_hphys_q(iommu->reg+ CCMD_REG, val, MAPMEM_PCD);
	
	// wait until complettion
	for (;;) {
		read_hphys_q(iommu->reg+ CCMD_REG, &val, MAPMEM_PCD);		
		if (!(val & CCMD_ICC))
			break;
		asm_rep_and_nop();
	}
	spinlock_unlock(&iommu->reg_lock);
	
	return 0;
}

// IOTLB global invalidation
static int flush_iotlb_global(struct iommu *iommu)
{
	int iotlb_reg_offset = ecap_iro(iommu->ecap);
	u64 val = 0 ;
	
	// IOTLB global invalidation
	// Also DMA draining will be applied, if supported
	val = IOTLB_FLUSH_GLOBAL|IOTLB_IVT|IOTLB_DRAIN_READ|IOTLB_DRAIN_WRITE;
	
	spinlock_lock(&iommu->reg_lock);
	write_hphys_q(iommu->reg+ iotlb_reg_offset + 8, val, MAPMEM_PCD);
	
	// wait until completion
	for (;;) {
		read_hphys_q(iommu->reg+ iotlb_reg_offset + 8, &val, MAPMEM_PCD);		
		if (!(val & IOTLB_IVT))
			break;
		asm_rep_and_nop();
	}
	spinlock_unlock(&iommu->reg_lock);
	
	return 0;
}

static void flush_all(void)
{
	struct acpi_drhd_u *drhd;
	
	asm_wbinvd();
	LIST_FOREACH(drhd_list, drhd) {
		invalidate_context_cache(drhd->iommu);
		flush_iotlb_global(drhd->iommu);
	}
}

// Set Root-entry table address
static int gcmd_srtp(struct iommu *iommu)
{
	u32 cmd, stat;
	void *virt;
	int ret;
	
	if (iommu == NULL) {
		printf("gcmd_srtp: iommu == NULL\n");
		return -EINVAL;
	}
	
	if (!iommu->root_entry) {
		ret = alloc_page(&virt, &(iommu->root_entry_phys));
		if (ret!=0)
			return -ENOMEM;
		
		memset((u8*)virt, 0, PAGESIZE);
		inval_cache_pg(iommu, virt);
		
		iommu->root_entry = (struct root_entry *)virt;
	}
	
	spinlock_lock(&iommu->reg_lock);
	write_hphys_q(iommu->reg+ RTADDR_REG, iommu->root_entry_phys, MAPMEM_PCD);
	
	cmd = iommu->gcmd | GCMD_SRTP;
	write_hphys_l(iommu->reg+ GCMD_REG, cmd, MAPMEM_PCD);
	
	// wait until completion
	for (;;) {
		read_hphys_l(iommu->reg+ GSTS_REG, &stat, MAPMEM_PCD);		
		if (stat & GSTS_RTPS)
			break;
		asm_rep_and_nop();
	}
	spinlock_unlock(&iommu->reg_lock);
	
	return 0;
}

// Enable DMA Remapping
static int gcmd_te(struct iommu *iommu)
{
	u32 stat;
	
	spinlock_lock(&iommu->reg_lock);
	// Enable translation
	iommu->gcmd |= GCMD_TE;
	write_hphys_l(iommu->reg+ GCMD_REG, iommu->gcmd, MAPMEM_PCD);
	// Wait until completion
	for (;;) {
		read_hphys_l(iommu->reg+ GSTS_REG, &stat, MAPMEM_PCD);		
		if (stat & GSTS_TES)
			break;
		asm_rep_and_nop();
	}
	
	spinlock_unlock(&iommu->reg_lock);
	return 0;
}

static struct iommu *alloc_iommu(struct acpi_drhd_u *drhd)
{
	struct iommu *iommu;
	
	iommu = alloc(sizeof(struct iommu));
	if (!iommu)
		return NULL;
	memset(iommu, 0, sizeof(struct iommu));
	
	iommu->reg = drhd->address;
	
	read_hphys_q(iommu->reg+ CAP_REG, &iommu->cap, MAPMEM_PCD);	
	read_hphys_q(iommu->reg+ ECAP_REG, &iommu->ecap, MAPMEM_PCD);	
	
	spinlock_init(&iommu->unit_lock);
	spinlock_init(&iommu->reg_lock);
	
	drhd->iommu = iommu;
	return iommu;
}

static int gaw_to_agaw(int gaw) { // Just mentioned in chapter 3.4.2 in VT-d spec.
	int agaw, r;
	
	r = (gaw-12) % 9;
	if (r==0) {
		agaw = gaw;
	} else {
		agaw = gaw+9-r;
	}
	if (agaw > 64) {
		agaw = 64;
	}
	
	return agaw;
}

int iodom_init(struct domain *dom)
{
	struct iommu *iommu = NULL;
	int guest_width ;
	int adjust_width ;
	int agaw ;
	u64 sagaw;
	struct acpi_drhd_u *drhd;
	
	if (!iommu_detected || (drhd_list_head.next==NULL))
		return 0;
	
	LIST_FOREACH(drhd_list, drhd) {
		if (drhd->iommu)
			iommu = drhd->iommu ;
		else 
			iommu = alloc_iommu(drhd) ;
	}
	
	/* determine AGAW */
	guest_width = cap_mgaw(iommu->cap);
	adjust_width = gaw_to_agaw(guest_width);
	agaw = ((adjust_width-12)/IOPT_LEVEL_STRIDE)-2;
	sagaw = cap_sagaw(iommu->cap);
	
	if ((1UL << (agaw % 32) & sagaw)==0) {
                printf("Unsupported AGAW value.\n");
                return -ENODEV;
	}
	
	dom->agaw = agaw;
	
	spinlock_init(&dom->iopt_lock);
	
	return 0;
}

static int reg_context_entry(struct domain *dom, struct iommu *iommu, u8 bus, u8 devfn)
{
	struct context_entry *context;
	void *virt;
	phys_t phys;
	int ret = 0;
	
	context = devid_to_context(iommu, bus, devfn);
	if (!context) {
		printf("context mapping failed.\n");
		return -ENOMEM;
	}
	if (context_entry_present(*context)) {
		return 0;
	}
	
	spinlock_lock(&iommu->unit_lock);
	
	set_agaw(*context, dom->agaw);
	
	if (!dom->pgd) {
		ret = alloc_page(&virt, &phys);
		if (ret!=0) {
			spinlock_unlock(&dom->iopt_lock);
			return -ENOMEM;
		}
		memset(virt, 0, PAGESIZE);
		dom->pgd = (void *)(long)phys;
	}
	
	set_asr(*context, (unsigned long)dom->pgd);
	set_context_trans_type(*context, 0x0);     // means "ASR field points to a multi-level page-table,"
	
	enable_fault_handling(*context);
	set_context_present(*context);
	set_context_domid(*context, dom->domain_id);
	inval_cache_dw(iommu, context);
	
	gcmd_wbf(iommu);
	spinlock_unlock(&iommu->unit_lock);
	
	return ret;
}

static int dmar_map_page(struct domain *dom, unsigned long gfn, int perm)
{
	struct acpi_drhd_u *drhd;
	struct iommu *iommu;
	struct iopt_entry *pte = NULL;
	phys_t pgaddr=0x0 ;
	
	drhd = drhd_list_head.next;
	iommu = drhd->iommu;
	
	pgaddr = buildup_iopt(dom, (phys_t)gfn << PAGE_SHIFT);
	if (pgaddr==0x0)
		return -ENOMEM;
	pte = mapmem_hphys(pgaddr, PAGESIZE, MAPMEM_PCD);
	pte += gfn & IOPT_LEVEL_MASK;
	set_pte_addr(*pte, (phys_t)gfn << PAGE_SHIFT);
	switch (perm) {
	case PERM_DMA_NO:
	case PERM_DMA_RO:
	case PERM_DMA_WO:
	case PERM_DMA_RW:
		set_pte_perm(*pte, perm);
		break;
	default:
		break;
	}
	
	inval_cache_dw(iommu, pte);
	unmapmem(pte, PAGESIZE);
	
	LIST_FOREACH(drhd_list, drhd)
	{
		iommu = drhd->iommu;
		gcmd_wbf(iommu);
	}
	return 0;
}

static int search_remap(int bus, int dev, int func) {
	int remap;
	
	for (remap=0; remap<num_remap ; remap++) {
		if (rem[remap].bus==bus && rem[remap].df.dev_no==dev && rem[remap].df.func_no==func)
			return rem[remap].dom;
	}
	
	return 0;
}

static void setup_bitvisor_devs(void)
{
	struct acpi_drhd_u *drhd;
	int ret=0;
	u8 bus, devfn ;
	
	struct pci_device *devs;
	LIST_FOREACH(pci_device_list, devs) {
		if (search_remap(devs->address.bus_no, devs->address.device_no, devs->address.func_no)==0) {
			bus = devs->address.bus_no ;
			devfn = (devs->address.device_no << 3) | devs->address.func_no ;
			drhd = matched_drhd_u(bus, devfn) ;
			ret = reg_context_entry(dom_io[0], drhd->iommu, bus, devfn) ;
		}
		if (ret != 0)
			printf("reg_context_entry failed\n");
	}
}

static void mod_remap_conf(struct domain *dom, int bus, int devfn)
{
	struct acpi_drhd_u *drhd;
	int ret;
	
	drhd = matched_drhd_u((u8)bus, (u8)devfn);
	ret = reg_context_entry(dom, drhd->iommu, (u8)bus, (u8)devfn);
	
	if (ret != 0)
		printf("reg_context_entry failed\n");
}

static void clear_fault_bits(struct iommu *iommu)
{
	u64 val;
	u64 fro; // Fault Register Offset
	
	read_hphys_q(iommu->reg+CAP_REG, &val, MAPMEM_PCD);
	fro = cap_fro(val);
	read_hphys_q(iommu->reg+fro+0x8, &val, MAPMEM_PCD); // reading upper 64bits of 1st fault recording register
	write_hphys_q(iommu->reg+fro+0x8, val, MAPMEM_PCD);  // writing back to clear fault status
	
	write_hphys_l(iommu->reg+ FSTS_REG, FSTS_MASK, MAPMEM_PCD); // Clearing lower 7bits of Fault Status Register
}

static int init_iommu(void)
{
	struct acpi_drhd_u *drhd;
	struct iommu *iommu;
	int ret;
	
	LIST_FOREACH(drhd_list, drhd)
	{
		iommu = drhd->iommu;
		ret = gcmd_srtp(iommu);
		if (ret)
		{
			printf("IOMMU: set root entry failed\n");
			return -EIO;
		}
		clear_fault_bits(iommu);
		write_hphys_l(iommu->reg+ FECTL_REG, 0, MAPMEM_PCD);  /* clearing IM field */
	}
	return 0;
}

static int enable_dma_remapping(void)
{
	struct acpi_drhd_u *drhd;
	
	LIST_FOREACH(drhd_list, drhd)
	{
		if (gcmd_te(drhd->iommu))
			return -EIO;
	}
	return 0;
}

static int remap_preconf()
{
	int i;
	int bus, devfn;
	int cnt=0;
	int f=1;
	
	if (num_remap==0) return 0;
	
	while (f) {
		cnt++;
		f=0;
		for (i=0; i<num_remap ; i++) {
			if (rem[i].dom==0) {
				f=1;
				bus=rem[i].bus;
				devfn=rem[i].devfn;
				rem[i].dom=cnt;
				for (; i<num_remap; i++) {
					if (bus==rem[i].bus && devfn==rem[i].devfn) {
						rem[i].dom=cnt;
					}
				}
				goto next_loop;
			}
		}
	next_loop:;
	}
	
#ifdef VTD_DEBUG
	printf("(IOMMU) Following memory regions will be used for VMM-controlled DMA I/O.\n");
	for (i=0; i<num_remap ; i++) {
		printf("(IOMMU) %x:%x:%x - physFN %x, "
		       ,rem[i].bus, rem[i].df.dev_no, rem[i].df.func_no
		       ,rem[i].phys);
		printf("%x pages, perm : %x : [dom %x]\n"
		       , rem[i].num_pages, rem[i].perm, rem[i].dom);
	}
#endif // of VTD_DEBUG
	
	return cnt;
}

void iommu_setup(void) __initcode__
{
#ifdef VTD_TRANS
	
	struct acpi_drhd_u *drhd;
	unsigned long i;
	int remap, dom, ndom, f;
	
	if (!iommu_detected)
		return;
	
	clflush_size = ((cpuid_ebx(1) >> 8) & 0xff) * 8;
	drhd = drhd_list_head.next;
	
#ifdef VTD_DEBUG
	printf ("(IOMMU) VMM region(0x%08X-0x%08X) will be hidden for all pass-through devices\n"
		, vmm_start_inf(), vmm_term_inf());
#endif // of VTD_DEBUG
	
	ndom=remap_preconf();
	
	printf("(IOMMU) dom 0(PT Devs.) ");
	for (i = 0; i <= 0xfffff; i++) 
		dmar_map_page(dom_io[0], i, ((i>=vmm_start_inf() >> 12 && i<vmm_term_inf() >> 12) ? PERM_DMA_NO : PERM_DMA_RW));
	for (dom=1; dom<ndom ; dom++) {
		printf("%x",dom);
		for (i=0; i<num_remap ; i++) {
			if (rem[i].dom!=dom) continue;
			printf("(%x:%x:%x) ", rem[i].bus, rem[i].df.dev_no, rem[i].df.func_no);
			break;
		}
		for (i = 0; i <= 0xfffff; i++) { 
			f=0;
			for (remap=0; remap<num_remap ; remap++) {
				if (rem[remap].dom==dom && i>=rem[remap].phys && i<rem[remap].phys+rem[remap].num_pages) {
					f=rem[remap].perm;
				}
			}
			if (f) 
				dmar_map_page(dom_io[dom], i, f);
			else
				dmar_map_page(dom_io[dom], i, PERM_DMA_NO);
		}
	}
	printf("... Ready.\n");
	
	init_iommu();
	setup_bitvisor_devs();
	for (dom=1; dom<ndom ; dom++) {
		for (remap=0; remap<num_remap ; remap++) {
			if (rem[remap].dom==dom) goto mod_remap;
		}
	mod_remap:
		mod_remap_conf(dom_io[dom], rem[remap].bus, rem[remap].devfn) ;
	}
	
	flush_all();
	enable_dma_remapping();
	
	return;
	
#endif // of VTD_TRANS
}

// DMA remapping information such as PCI bus, slot, function, and
//      physical address, length (by number of page frames), and permission is registered.
// 
int add_remap(int bus, int dev, int func, int phys, int num_pages, int perm)
{
	if (!iommu_detected) {
		printf("add_remap() : cannot register DMA remapping.\n");
		return 0;
	}
	
	rem[num_remap].bus=bus;
	rem[num_remap].df.dev_no = dev ;
	rem[num_remap].df.func_no = func ;
	rem[num_remap].phys=phys;
	rem[num_remap].num_pages=num_pages;
	rem[num_remap].perm=perm;
	rem[num_remap].dom=0;
	num_remap++;
	
	return 1;
}

struct domain *dom_io[MAX_IO_DOM];
int num_dom;

struct domain *create_dom(unsigned short int domid)
{
#ifdef VTD_TRANS
	struct domain *dom;
	
	if ((dom = alloc(sizeof(struct domain))) == NULL)
		return NULL;
	
	memset(dom, 0, sizeof(*dom));
	dom->domain_id = domid;
	
	if (iodom_init(dom) != 0)
		return NULL; // Init. failed.
	
	return dom;
#endif // of VTD_TRANS
	if (0)			/* make gcc happy */
		printf ("%p%p%p%p%p%p%p", flush_all, dmar_map_page,
			setup_bitvisor_devs, mod_remap_conf, init_iommu,
			enable_dma_remapping, remap_preconf);
	return 0;
}

INITFUNC ("driver95", iommu_setup);
