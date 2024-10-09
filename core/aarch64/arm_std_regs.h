/*
 * Copyright (c) 2022 Igel Co., Ltd
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

#ifndef _CORE_AARCH64_ARM_STD_REG_H
#define _CORE_AARCH64_ARM_STD_REG_H

#include <bits.h>
#include "sys_reg.h"

/* CNTHCTL when E2H=1 */
#define CNTHCTL_EL0PCTEN BIT (0)
#define CNTHCTL_EL0VCTEN BIT (1)
#define CNTHCTL_EL0VTEN	 BIT (8)
#define CNTHCTL_EL0PTEN	 BIT (9)
#define CNTHCTL_EL1PCTEN BIT (10)
#define CNTHCTL_EL1PTEN	 BIT (11)

/* ESR */

#define ESR_EC_TOTAL 0x40

#define ESR_ISS(esr)  ((esr) & 0x1FFFFFF)
#define ESR_IL(esr)   (((esr) >> 25) & 0x1)
#define ESR_EC(esr)   (((esr) >> 26) & 0x3F)
#define ESR_ISS2(esr) (((esr) >> 32) & 0x1F)

#define ESR_EC_UNKNOWN		  0b000000
#define ESR_EC_WF_FAMILY	  0b000001
#define ESR_EC_MCR_MRC_1111	  0b000011
#define ESR_EC_MCRR_MRRC_1111	  0b000100
#define ESR_EC_MCR_MRC_1110	  0b000101
#define ESR_EC_LDC_STC		  0b000110
#define ESR_EC_SVE_SIMD_FP	  0b000111
#define ESR_EC_VMRS		  0b001000
#define ESR_EC_PA		  0b001001
#define ESR_EC_LD_ST_64B	  0b001010
#define ESR_EC_MRRC_1110	  0b001100
#define ESR_EC_BRANCH_TARGET	  0b001101
#define ESR_EC_ILL_EXE_STATE	  0b001110
#define ESR_EC_SVC_A32		  0b010001
#define ESR_EC_HVC_A32		  0b010010
#define ESR_EC_SMC_A32		  0b010011
#define ESR_EC_SVC_A64		  0b010101
#define ESR_EC_HVC_A64		  0b010110
#define ESR_EC_SMC_A64		  0b010111
#define ESR_EC_MSR_MRS		  0b011000
#define ESR_EC_SVE		  0b011001
#define ESR_EC_ERET_FAMILY	  0b011010
#define ESR_EC_PA_FAIL		  0b011100
#define ESR_EC_INST_ABORT_LOWER	  0b100000
#define ESR_EC_INST_ABORT_CURRENT 0b100001
#define ESR_EC_PC_ALIGNMENT	  0b100010
#define ESR_EC_DATA_ABORT_LOWER	  0b100100
#define ESR_EC_DATA_ABORT_CURRENT 0b100101
#define ESR_EC_SP_ALIGNMENT	  0b100110
#define ESR_EC_FP_A32		  0b101000
#define ESR_EC_FP_A64		  0b101100
#define ESR_EC_SERROR		  0b101111
#define ESR_EC_BP_LOWER		  0b110000
#define ESR_EC_BP_CURRENT	  0b110001
#define ESR_EC_SW_STEP_LOWER	  0b110010
#define ESR_EC_SW_STEP_CURRENT	  0b110011
#define ESR_EC_WP_LOWER		  0b110100
#define ESR_EC_WP_UPPER		  0b110101
#define ESR_EC_BKPT_A32		  0b111000
#define ESR_EC_VEC_CATCH	  0b111010
#define ESR_EC_BRK		  0b111100

/* HCR_EL2 */
#define HCR_VM		BIT (0)
#define HCR_SWIO	BIT (1)
#define HCR_PTW		BIT (2)
#define HCR_FMO		BIT (3)
#define HCR_IMO		BIT (4)
#define HCR_AMO		BIT (5)
#define HCR_VF		BIT (6)
#define HCR_VI		BIT (7)
#define HCR_VSE		BIT (8)
#define HCR_FB		BIT (9)
#define HCR_BSU		BIT (11)
#define HCR_DC		BIT (12)
#define HCR_TWI		BIT (13)
#define HCR_TWE		BIT (14)
#define HCR_TID0	BIT (15)
#define HCR_TID1	BIT (16)
#define HCR_TID2	BIT (17)
#define HCR_TID3	BIT (18)
#define HCR_TSC		BIT (19)
#define HCR_TIDCP	BIT (20)
#define HCR_TACR	BIT (21)
#define HCR_TSW		BIT (22)
#define HCR_TPCP	BIT (23)
#define HCR_TPU		BIT (24)
#define HCR_TTLB	BIT (25)
#define HCR_TVM		BIT (26)
#define HCR_TGE		BIT (27)
#define HCR_TDZ		BIT (28)
#define HCR_HCD		BIT (29)
#define HCR_TRVM	BIT (30)
#define HCR_RW		BIT (31)
#define HCR_CD		BIT (32)
#define HCR_ID		BIT (33)
#define HCR_E2H		BIT (34)
#define HCR_TLOR	BIT (35)
#define HCR_TERR	BIT (36)
#define HCR_TEA		BIT (37)
#define HCR_MIOCNCE	BIT (38)
#define HCR_APK		BIT (40)
#define HCR_API		BIT (41)
#define HCR_NV		BIT (42)
#define HCR_NV1		BIT (43)
#define HCR_AT		BIT (44)
#define HCR_NV2		BIT (45)
#define HCR_FWB		BIT (46)
#define HCR_FIEN	BIT (47)
#define HCR_TID4	BIT (49)
#define HCR_TICAB	BIT (50)
#define HCR_AMVOFFEN	BIT (51)
#define HCR_TOCU	BIT (52)
#define HCR_EnSCXT	BIT (53)
#define HCR_TTLBIS	BIT (54)
#define HCR_TTLBOS	BIT (55)
#define HCR_ATA		BIT (56)
#define HCR_DCT		BIT (57)
#define HCR_TID5	BIT (58)
#define HCR_TWEDEn	BIT (59)
#define HCR_TWEDEL(val) (((u64)(val) & 0xF) << 60)

/* PAR */
#define PAR_F	    BIT (0)
#define PAR_PA_MASK ((BIT (40) - 1) << 12)

/* SCTLR */
#define SCTLR_M		  BIT (0)
#define SCTLR_A		  BIT (1)
#define SCTLR_C		  BIT (2)
#define SCTLR_SA	  BIT (3)
#define SCTLR_SA0	  BIT (4)
#define SCTLR_CP15BEN	  BIT (5)
#define SCTLR_nAA	  BIT (6)
#define SCTLR_ITD	  BIT (7)
#define SCTLR_SED	  BIT (8)
#define SCTLR_EnRCTX	  BIT (10)
#define SCTLR_EOS	  BIT (11)
#define SCTLR_I		  BIT (12)
#define SCTLR_EnDB	  BIT (13)
#define SCTLR_DZE	  BIT (14)
#define SCTLR_UCT	  BIT (15)
#define SCTLR_nTWI	  BIT (16)
#define SCTLR_nTWE	  BIT (18)
#define SCTLR_WXN	  BIT (19)
#define SCTLR_TSCXT	  BIT (20)
#define SCTLR_IESB	  BIT (21)
#define SCTLR_EIS	  BIT (22)
#define SCTLR_SPAN	  BIT (23)
#define SCTLR_E0E	  BIT (24)
#define SCTLR_EE	  BIT (25)
#define SCTLR_UCI	  BIT (26)
#define SCTLR_EnDA	  BIT (27)
#define SCTLR_nTLSMD	  BIT (28)
#define SCTLR_LSMAOE	  BIT (29)
#define SCTLR_EnIB	  BIT (30)
#define SCTLR_EnIA	  BIT (31)
#define SCTLR_BT0	  BIT (35)
#define SCTLR_BT	  BIT (36)
#define SCTLR_ITFSB	  BIT (37)
#define SCTLR_TCF0(val)	  (((u64)(val) & 0x3) << 38)
#define SCTLR_TCF(val)	  (((u64)(val) & 0x3) << 40)
#define SCTLR_ATA0	  BIT (42)
#define SCTLR_ATA	  BIT (43)
#define SCTLR_DSSBS	  BIT (44)
#define SCTLR_TWEDEn	  BIT (45)
#define SCTLR_TWEDEL(val) (((u64)(val) & 0xF) << 46)
#define SCTLR_EnASR	  BIT (54)
#define SCTLR_EnASR0	  BIT (55)
#define SCTLR_EnALS	  BIT (56)
#define SCTLR_EPAN	  BIT (57)

#define SCTLR_RES1 \
	(SCTLR_ITD | SCTLR_EOS | SCTLR_TSCXT | SCTLR_EIS | SCTLR_nTLSMD | \
	 SCTLR_LSMAOE)

/* SPSR */
#define SPSR_M_TOTAL 0x10

#define SPSR_INTR_MASK (0xFULL << 6)

#define SPSR_M(spsr)   (((spsr) >> 0) & 0xF)
#define SPSR_nRW(spsr) (((spsr) >> 4) & 0x1)
#define SPSR_F(spsr)   (((spsr) >> 6) & 0x1)
#define SPSR_I(spsr)   (((spsr) >> 7) & 0x1)
#define SPSR_A(spsr)   (((spsr) >> 8) & 0x1)
#define SPSR_D(spsr)   (((spsr) >> 9) & 0x1)
#define SPSR_IL(spsr)  (((spsr) >> 20) & 0x1)
#define SPSR_SS(spsr)  (((spsr) >> 21) & 0x1)
#define SPSR_PAN(spsr) (((spsr) >> 22) & 0x1)
#define SPSR_UAO(spsr) (((spsr) >> 23) & 0x1)
#define SPSR_DIT(spsr) (((spsr) >> 24) & 0x1)
#define SPSR_TCO(spsr) (((spsr) >> 25) & 0x1)
#define SPSR_V(spsr)   (((spsr) >> 28) & 0x1)
#define SPSR_C(spsr)   (((spsr) >> 29) & 0x1)
#define SPSR_Z(spsr)   (((spsr) >> 30) & 0x1)
#define SPSR_N(spsr)   (((spsr) >> 31) & 0x1)

#define SPSR_F_BIT   BIT (6)
#define SPSR_I_BIT   BIT (7)
#define SPSR_A_BIT   BIT (8)
#define SPSR_D_BIT   BIT (9)

/* TCR */
#define TCR_T0SZ(val)  ((val) & 0x3F)
#define TCR_EPD0       BIT (7)
#define TCR_IRGN0(val) (((val) & 0x3) << 8)
#define TCR_ORGN0(val) (((val) & 0x3) << 10)
#define TCR_SH0(val)   (((val) & 0x3) << 12)
#define TCR_TG0(val)   (((val) & 0x3) << 14)
#define TCR_T1SZ(val)  (((val) & 0x3F) << 16)
#define TCR_A1	       BIT (22)
#define TCR_EPD1       BIT (23)
#define TCR_IRGN1(val) (((val) & 0x3) << 24)
#define TCR_ORGN1(val) (((val) & 0x3) << 26)
#define TCR_SH1(val)   (((val) & 0x3) << 28)
#define TCR_TG1(val)   (((val) & 0x3) << 30)
#define TCR_IPS(val)   (((val) & 0x7) << 32)
#define TCR_AS	       BIT (36)

#define TCR_IRGN_NC	  0x0ULL
#define TCR_IRGN_WBRAWAC  0x1ULL
#define TCR_IRGN_WTRANWAC 0x2ULL
#define TCR_IRGN_WBRANWAC 0x3ULL

#define TCR_ORGN_NC	  0x0ULL
#define TCR_ORGN_WBRAWAC  0x1ULL
#define TCR_ORGN_WTRANWAC 0x2ULL
#define TCR_ORGN_WBRANWAC 0x3ULL

#define TCR_SH_NS 0x0ULL
#define TCR_SH_OS 0x1ULL
#define TCR_SH_IS 0x3ULL

#define TCR_TG0_4KB  0x0ULL
#define TCR_TG0_64KB 0x1ULL
#define TCR_TG0_16KB 0x2ULL

#define TCR_IPS_4GB   0x0ULL
#define TCR_IPS_64GB  0x1ULL
#define TCR_IPS_1TB   0x2ULL
#define TCR_IPS_4TB   0x3ULL
#define TCR_IPS_16TB  0x4ULL
#define TCR_IPS_256TB 0x5ULL
#define TCR_IPS_4PB   0x6ULL

#define TCR_TG1_16KB 0x1ULL
#define TCR_TG1_4KB  0x2ULL
#define TCR_TG1_64KB 0x3ULL

/* VTCR */
#define VTCR_T0SZ(val)	((val) & 0x3F)
#define VTCR_SL0(val)	(((val) & 0x3) << 6)
#define VTCR_IRGN0(val) (((val) & 0x3) << 8)
#define VTCR_ORGN0(val) (((val) & 0x3) << 10)
#define VTCR_SH0(val)	(((val) & 0x3) << 12)
#define VTCR_TG0(val)	(((val) & 0x3) << 14)
#define VTCR_PS(val)	(((val) & 0x7) << 16)
#define VTCR_VS		BIT (19)
#define VTCR_HA		BIT (21)
#define VTCR_HD		BIT (22)

#define VTCR_IRGN_NC	   0x0ULL
#define VTCR_IRGN_WBRAWAC  0x1ULL
#define VTCR_IRGN_WTRANWAC 0x2ULL
#define VTCR_IRGN_WBRANWAC 0x3ULL

#define VTCR_ORGN_NC	   0x0ULL
#define VTCR_ORGN_WBRAWAC  0x1ULL
#define VTCR_ORGN_WTRANWAC 0x2ULL
#define VTCR_ORGN_WBRANWAC 0x3ULL

#define VTCR_SH_NS 0x0ULL
#define VTCR_SH_OS 0x1ULL
#define VTCR_SH_IS 0x3ULL

#define VTCR_TG0_4KB  0x0ULL
#define VTCR_TG0_64KB 0x1ULL
#define VTCR_TG0_16KB 0x2ULL

#define VTCR_PS_4GB   0x0ULL
#define VTCR_PS_64GB  0x1ULL
#define VTCR_PS_1TB   0x2ULL
#define VTCR_PS_4TB   0x3ULL
#define VTCR_PS_16TB  0x4ULL
#define VTCR_PS_256TB 0x5ULL
#define VTCR_PS_4PB   0x6ULL

#define MPIDR_AFF_MASK	 0xFF
#define MPIDR_AFF0_SHIFT 0
#define MPIDR_AFF1_SHIFT 8
#define MPIDR_AFF2_SHIFT 16
#define MPIDR_AFF3_SHIFT 32

#define CPTR_ZEN(v)  (((v) & 0x3) << 16)
#define CPTR_FPEN(v) (((v) & 0x3) << 20)
#define CPTR_SMEN(v) (((v) & 0x3) << 24)

#define CPACR_ZEN(v)  CPTR_ZEN ((v))
#define CPACR_FPEN(v) CPTR_FPEN ((v))
#define CPACR_SMEN(v) CPTR_SMEN ((v))

/* Compiler may not recognize these registers as they are new */
#define ID_AA64ZFR0_EL1	 sys_reg (3, 0, 0, 4, 4)
#define ID_AA64ISAR2_EL1 sys_reg (3, 0, 0, 6, 2)
#define ID_AA64MMFR2_EL1 sys_reg (3, 0, 0, 7, 2)

#define ID_AA64PFR0_AA64_ONLY 0x1
#define ID_AA64PFR0_AA64_AA32 0x2

#define ID_AA64PFR0_GET_EL3(v) (((v) >> 12) & 0xF)
#define ID_AA64PFR0_GET_GIC(v) (((v) >> 24) & 0xF)

#define ID_AA64MMFR0_PA_48 0x5

#define ID_AA64MMFR0_TG16_SUPPORT    0x1
#define ID_AA64MMFR0_TG16_SUPPORT_52 0x2
#define ID_AA64MMFR0_TG4_SUPPORT     0x0
#define ID_AA64MMFR0_TG4_SUPPORT_52  0x1

#define ID_AA64MMFR0_TG16_2_SUPPORT    0x2
#define ID_AA64MMFR0_TG16_2_SUPPORT_52 0x3
#define ID_AA64MMFR0_TG4_2_SUPPORT     0x2
#define ID_AA64MMFR0_TG4_2_SUPPORT_52  0x3

#define ID_AA64MMFR0_GET_PA(v)	   ((v) & 0xF)
#define ID_AA64MMFR0_GET_TG16(v)   (((v) >> 20) & 0xF)
#define ID_AA64MMFR0_GET_TG4(v)	   (((v) >> 28) & 0xF)
#define ID_AA64MMFR0_GET_TG16_2(v) (((v) >> 32) & 0xF)
#define ID_AA64MMFR0_GET_TG4_2(v)  (((v) >> 40) & 0xF)

#define ID_AA64MMFR0_PA(v)     ((v) & 0xF)
#define ID_AA64MMFR0_TG16(v)   (((v) & 0xF) << 20)
#define ID_AA64MMFR0_TG4(v)    (((v) & 0xF) << 28)
#define ID_AA64MMFR0_TG16_2(v) (((v) & 0xF) << 32)
#define ID_AA64MMFR0_TG4_2(v)  (((v) & 0xF) << 40)

#endif
