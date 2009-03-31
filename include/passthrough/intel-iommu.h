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

/*
 * Intel VT-d register offset
 */
#define  CAP_REG    0x8     /* Capabilities, 64 bit */
#define  ECAP_REG   0x10    /* Extended-capabilities, 64 bit */
#define  GCMD_REG   0x18    /* Global command register, 32 bit */
#define  GSTS_REG   0x1c    /* Global status register, 32 bit */
#define  RTADDR_REG 0x20    /* Root-entry table address, 64 bit */
#define  CCMD_REG   0x28    /* Context command register, 64 bit*/
#define  FSTS_REG   0x34    /* Fault status register, 32 bit */
#define  FECTL_REG  0x38    /* Fault event control register, 32 bit */

/*
 * Decoding Capability Register
 */

#define cap_fro(c)    ((((c) >> 24) & 0x3ff) * 16)  /* Fault-recording register offset */
#define cap_mgaw(c)   ((((c) >> 16) & 0x3f) + 1) /* Maximum guest address width */
#define cap_sagaw(c)  (((c) >> 8) & 0x1f)       /* Supported adjusted guest address widths */
#define cap_rwbf(c)   (((c) >> 4) & 1)

/*
 * Decoding Extended Capability Register
 */
#define ecap_iro(e)   ((((e) >> 8) & 0x3ff) * 16)
#define ecap_c(e)     ((e >> 0) & 0x1)

#define PAGE_SHIFT (12)

#define PAGE_MASK  (((u64)-1) << PAGE_SHIFT)

/* IOTLB Invalidate Register Field Offset */
#define IOTLB_FLUSH_GLOBAL (((u64)1) << 60)
#define IOTLB_DRAIN_READ   (((u64)1) << 49)
#define IOTLB_DRAIN_WRITE  (((u64)1) << 48)
#define IOTLB_IVT          (((u64)1) << 63)

/*
 * Global Command Register Field Offset
 */
#define GCMD_TE     (((u64)1) << 31)
#define GCMD_SRTP   (((u64)1) << 30)
#define GCMD_WBF    (((u64)1) << 27)

/*
 * Global Status Register Field Offset
 */
#define GSTS_TES    (((u64)1) << 31)
#define GSTS_RTPS   (((u64)1) << 30)
#define GSTS_WBFS   (((u64)1) << 27)

/* 
 * Context Command Register Field Offset
 */
#define CCMD_ICC   (((u64)1) << 63)
#define CCMD_GLOBAL_INVL (((u64)1) << 61)

/* 
 * Decoding Fault Status Register
 */
#define FSTS_MASK   ((u64)0x7f)

// Translation Sturcture Format

/*
 * Root Entry Structure and Ops.
 */
struct root_entry {
	struct {
		unsigned p: 1 ;
		unsigned rsvd: 11 ;
		u64 ctp: 52 ;
	} v ;
	u64 rsvd;
} ;

#define root_entry_present(r)     ((r).v.p)
#define set_root_present(r) do {(r).v.p = 1;} while(0)
#define root_entry_ctp(r)          ((r).v.ctp << PAGE_SHIFT)
#define set_ctp(r, value)   do {(r).v.ctp = (value) >> PAGE_SHIFT;} while(0)

/*
 * Context Entry Structure and Ops.
 */
struct context_entry {
	struct {
		unsigned p:1 ;
		unsigned fpd:1 ;
		unsigned t:2 ;
		unsigned eh:1 ;
		unsigned alh:1 ;
		unsigned rsvd: 6 ;
		u64 asr: 52 ;
	} l ;
	struct {
		unsigned aw: 3 ;
		unsigned avail: 4 ;
		unsigned rsfc: 1 ;
		unsigned did:16 ;
		u64 rsvd: 40 ;
	} h ;
} ;

#define context_entry_present(c)          ((c).l.p)
#define set_context_present(c)      do {(c).l.p = 1;} while(0)
#define set_context_domid(c, d)     do {(c).h.did = (d) ;} while (0)
#define enable_fault_handling(c) do {(c).l.fpd = 0;} while(0)

#define set_context_trans_type(c, val) do {(c).l.t = val;} while(0)

#define set_asr(c, val)  do {(c).l.asr = (val) >> PAGE_SHIFT ;} while(0)
#define set_agaw(c, val) do {(c).h.aw = (val) & 7;} while(0)

/* IO pagetable walk */
#define IOPT_LEVEL_STRIDE     (9)
#define IOPT_LEVEL_MASK       ((1 << IOPT_LEVEL_STRIDE) - 1)

#define iopt_level_offset(addr, level)                               \
        ((addr >> (12 + (level - 1) * IOPT_LEVEL_STRIDE)) & IOPT_LEVEL_MASK)

/*
 * Page-Table Entry Structure and Ops.
 */
struct iopt_entry {
	unsigned r: 1 ;
	unsigned w: 1 ;
	unsigned avail1: 5 ;
	unsigned sp: 1;
	unsigned avail2: 3;
	unsigned snp: 1;
	u64 addr: 40 ;
	unsigned avail3: 10 ;
	unsigned tm: 1;
	unsigned avail4: 1 ;
} ;

#define set_pte_perm(p, prot)  do { (p).r = prot & 1; (p).w = (prot & 2) >> 1; } while (0)
#define get_pte_addr(p)        ((p).addr << PAGE_SHIFT)
#define set_pte_addr(p, address)  do {(p).addr = (address) >> PAGE_SHIFT ;} while(0)
