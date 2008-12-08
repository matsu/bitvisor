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

#ifndef _CORE_CPU_MMU_SPT_H
#define _CORE_CPU_MMU_SPT_H

#include "list.h"
#include "spinlock.h"
#include "types.h"

#ifdef CPU_MMU_SPT_1
#define NUM_OF_SPTTBL 32
struct cpu_mmu_spt_data {
	void *cr3tbl;
	u64 cr3tbl_phys;
	void *tbl[NUM_OF_SPTTBL];
	u64 tbl_phys[NUM_OF_SPTTBL];
	int cnt;
	int levels;
};
#endif /* CPU_MMU_SPT_1 */

#ifdef CPU_MMU_SPT_2
#define NUM_OF_SPTTBL		32
#define NUM_OF_SPTRWMAP 	256
#define NUM_OF_SPTSHADOW1	64
#define NUM_OF_SPTSHADOW2	32

#ifndef CPU_MMU_SPT_USE_PAE
#error PAE only
#endif

struct cpu_mmu_spt_rwmap {
	u64 gfn;
	u64 *pte;
};

struct cpu_mmu_spt_shadow {
	void *virt;
	u64 phys;
	u64 key;
	bool cleared;
};

struct cpu_mmu_spt_data {
	void *cr3tbl;
	u64 cr3tbl_phys;
	void *tbl[NUM_OF_SPTTBL];
	u64 tbl_phys[NUM_OF_SPTTBL];
	int cnt;
	int levels;
	struct cpu_mmu_spt_rwmap rwmap[NUM_OF_SPTRWMAP];
	spinlock_t rwmap_lock;
	unsigned int rwmap_fail, rwmap_normal, rwmap_free;
	struct cpu_mmu_spt_shadow shadow1[NUM_OF_SPTSHADOW1];
	spinlock_t shadow1_lock;
	unsigned int shadow1_modified, shadow1_normal, shadow1_free;
	struct cpu_mmu_spt_shadow shadow2[NUM_OF_SPTSHADOW2];
	spinlock_t shadow2_lock;
	unsigned int shadow2_modified, shadow2_normal, shadow2_free;
	bool wp;
};
#endif /* CPU_MMU_SPT_2 */

#ifdef CPU_MMU_SPT_3
#define NUM_OF_SPTTBL		32
#define NUM_OF_SPTRWMAP 	2048
#define NUM_OF_SPTSHADOW1	1024
#define NUM_OF_SPTSHADOW2	512
#define HASHSIZE_OF_SPTRWMAP	4096
#define HASHSIZE_OF_SPTSHADOW1N	2048
#define HASHSIZE_OF_SPTSHADOW1M	2048
#define HASHSIZE_OF_SPTSHADOW2N	1024
#define HASHSIZE_OF_SPTSHADOW2M	1024
#define NUM_OF_SPTSHADOWOFF	31

#ifndef CPU_MMU_SPT_USE_PAE
#error PAE only
#endif

struct cpu_mmu_spt_rwmap {
	LIST2_DEFINE (struct cpu_mmu_spt_rwmap, rwmap);
	LIST2_DEFINE (struct cpu_mmu_spt_rwmap, hash);
	u64 gfn;
	u64 *pte;
	bool fail;
};

struct cpu_mmu_spt_shadow {
	LIST2_DEFINE (struct cpu_mmu_spt_shadow, shadow);
	LIST2_DEFINE (struct cpu_mmu_spt_shadow, hash);
	u64 phys;
	u64 key;
	u8 clear_n;
	u8 clear_off[NUM_OF_SPTSHADOWOFF];
	u64 clear_area;
};

struct cpu_mmu_spt_data {
	void *cr3tbl;
	u64 cr3tbl_phys;
	void *tbl[NUM_OF_SPTTBL];
	u64 tbl_phys[NUM_OF_SPTTBL];
	int cnt;
	int levels;
	struct cpu_mmu_spt_rwmap rwmap[NUM_OF_SPTRWMAP];
	spinlock_t rwmap_lock;
	LIST2_DEFINE_HEAD (rwmap_fail, struct cpu_mmu_spt_rwmap, rwmap);
	LIST2_DEFINE_HEAD (rwmap_normal, struct cpu_mmu_spt_rwmap, rwmap);
	LIST2_DEFINE_HEAD (rwmap_free, struct cpu_mmu_spt_rwmap, rwmap);
	LIST2_DEFINE_HEAD (rwmap_hash[HASHSIZE_OF_SPTRWMAP],
			   struct cpu_mmu_spt_rwmap, hash);
	struct cpu_mmu_spt_shadow shadow1[NUM_OF_SPTSHADOW1];
	spinlock_t shadow1_lock;
	LIST2_DEFINE_HEAD (shadow1_modified, struct cpu_mmu_spt_shadow,
			   shadow);
	LIST2_DEFINE_HEAD (shadow1_normal, struct cpu_mmu_spt_shadow, shadow);
	LIST2_DEFINE_HEAD (shadow1_free, struct cpu_mmu_spt_shadow, shadow);
	LIST2_DEFINE_HEAD (shadow1_hashn[HASHSIZE_OF_SPTSHADOW1N],
			   struct cpu_mmu_spt_shadow, hash);
	LIST2_DEFINE_HEAD (shadow1_hashm[HASHSIZE_OF_SPTSHADOW1M],
			   struct cpu_mmu_spt_shadow, hash);
	struct cpu_mmu_spt_shadow shadow2[NUM_OF_SPTSHADOW2];
	spinlock_t shadow2_lock;
	LIST2_DEFINE_HEAD (shadow2_modified, struct cpu_mmu_spt_shadow,
			   shadow);
	LIST2_DEFINE_HEAD (shadow2_normal, struct cpu_mmu_spt_shadow, shadow);
	LIST2_DEFINE_HEAD (shadow2_free, struct cpu_mmu_spt_shadow, shadow);
	LIST2_DEFINE_HEAD (shadow2_hashn[HASHSIZE_OF_SPTSHADOW2N],
			   struct cpu_mmu_spt_shadow, hash);
	LIST2_DEFINE_HEAD (shadow2_hashm[HASHSIZE_OF_SPTSHADOW2M],
			   struct cpu_mmu_spt_shadow, hash);
	bool wp;
};
#endif /* CPU_MMU_SPT_3 */

struct vcpu;

bool cpu_mmu_spt_tlbflush (void);
void cpu_mmu_spt_updatecr3 (void);
void cpu_mmu_spt_invalidate (ulong virtual_addr);
void cpu_mmu_spt_pagefault (ulong err, ulong cr2);
bool cpu_mmu_spt_extern_mapsearch (struct vcpu *p, phys_t start, phys_t end);
int cpu_mmu_spt_init (void);

#endif
