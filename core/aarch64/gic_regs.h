
/*
 * Copyright (c) 2007, 2008 University of Tsukuba
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

#ifndef _CORE_AARCH64_GIC_REGS_H
#define _CORE_AARCH64_GIC_REGS_H

#include <bits.h>
#include "sys_reg.h"

#define GIC_ICC_SGI1R_EL1_ENCODE  sys_reg_encode (3, 0, 12, 11, 5)
#define GIC_ICC_ASGI1R_EL1_ENCODE sys_reg_encode (3, 0, 12, 11, 6)
#define GIC_ICC_SGI0R_EL1_ENCODE  sys_reg_encode (3, 0, 12, 11, 7)

#define GIC_ICC_PMR_EL1	    sys_reg (3, 0, 4, 6, 0)
#define GIC_ICC_IAR0_EL1    sys_reg (3, 0, 12, 8, 0)
#define GIC_ICC_EOIR0_EL1   sys_reg (3, 0, 12, 8, 1)
#define GIC_ICC_HPPIR0_EL1  sys_reg (3, 0, 12, 8, 2)
#define GIC_ICC_BPR0_EL1    sys_reg (3, 0, 12, 8, 3)
#define GIC_ICC_AP0R0_EL1   sys_reg (3, 0, 12, 8, 4)
#define GIC_ICC_AP0R1_EL1   sys_reg (3, 0, 12, 8, 5)
#define GIC_ICC_AP0R2_EL1   sys_reg (3, 0, 12, 8, 6)
#define GIC_ICC_AP0R3_EL1   sys_reg (3, 0, 12, 8, 7)
#define GIC_ICC_AP1R0_EL1   sys_reg (3, 0, 12, 9, 0)
#define GIC_ICC_AP1R1_EL1   sys_reg (3, 0, 12, 9, 1)
#define GIC_ICC_AP1R2_EL1   sys_reg (3, 0, 12, 9, 2)
#define GIC_ICC_AP1R3_EL1   sys_reg (3, 0, 12, 9, 3)
#define GIC_ICC_DIR_EL1	    sys_reg (3, 0, 12, 11, 1)
#define GIC_ICC_RPR_EL1	    sys_reg (3, 0, 12, 11, 3)
#define GIC_ICC_SGI1R_EL1   sys_reg (3, 0, 12, 11, 5)
#define GIC_ICC_ASGI1R_EL1  sys_reg (3, 0, 12, 11, 6)
#define GIC_ICC_SGI0R_EL1   sys_reg (3, 0, 12, 11, 7)
#define GIC_ICC_IAR1_EL1    sys_reg (3, 0, 12, 12, 0)
#define GIC_ICC_EOIR1_EL1   sys_reg (3, 0, 12, 12, 1)
#define GIC_ICC_HPPIR1_EL1  sys_reg (3, 0, 12, 12, 2)
#define GIC_ICC_BPR1_EL1    sys_reg (3, 0, 12, 12, 3)
#define GIC_ICC_CTLR_EL1    sys_reg (3, 0, 12, 12, 4)
#define GIC_ICC_SRE_EL1	    sys_reg (3, 0, 12, 12, 5)
#define GIC_ICC_IGRPEN0_EL1 sys_reg (3, 0, 12, 12, 6)
#define GIC_ICC_IGRPEN1_EL1 sys_reg (3, 0, 12, 12, 7)
#define GIC_ICC_SRE_EL2	    sys_reg (3, 4, 12, 9, 5)

#define GIC_ICH_AP0R0_EL2 sys_reg (3, 4, 12, 8, 0)
#define GIC_ICH_AP0R1_EL2 sys_reg (3, 4, 12, 8, 1)
#define GIC_ICH_AP0R2_EL2 sys_reg (3, 4, 12, 8, 2)
#define GIC_ICH_AP0R3_EL2 sys_reg (3, 4, 12, 8, 3)
#define GIC_ICH_AP1R0_EL2 sys_reg (3, 4, 12, 9, 0)
#define GIC_ICH_AP1R1_EL2 sys_reg (3, 4, 12, 9, 1)
#define GIC_ICH_AP1R2_EL2 sys_reg (3, 4, 12, 9, 2)
#define GIC_ICH_AP1R3_EL2 sys_reg (3, 4, 12, 9, 3)
#define GIC_ICH_HCR_EL2	  sys_reg (3, 4, 12, 11, 0)
#define GIC_ICH_VTR_EL2	  sys_reg (3, 4, 12, 11, 1)
#define GIC_ICH_MISR_EL2  sys_reg (3, 4, 12, 11, 2)
#define GIC_ICH_EISR_EL2  sys_reg (3, 4, 12, 11, 3)
#define GIC_ICH_ELRSR_EL2 sys_reg (3, 4, 12, 11, 5)
#define GIC_ICH_VMCR_EL2  sys_reg (3, 4, 12, 11, 7)
#define GIC_ICH_LR0_EL2	  sys_reg (3, 4, 12, 12, 0)
#define GIC_ICH_LR1_EL2	  sys_reg (3, 4, 12, 12, 1)
#define GIC_ICH_LR2_EL2	  sys_reg (3, 4, 12, 12, 2)
#define GIC_ICH_LR3_EL2	  sys_reg (3, 4, 12, 12, 3)
#define GIC_ICH_LR4_EL2	  sys_reg (3, 4, 12, 12, 4)
#define GIC_ICH_LR5_EL2	  sys_reg (3, 4, 12, 12, 5)
#define GIC_ICH_LR6_EL2	  sys_reg (3, 4, 12, 12, 6)
#define GIC_ICH_LR7_EL2	  sys_reg (3, 4, 12, 12, 7)
#define GIC_ICH_LR8_EL2	  sys_reg (3, 4, 12, 12, 0)
#define GIC_ICH_LR9_EL2	  sys_reg (3, 4, 12, 13, 1)
#define GIC_ICH_LR10_EL2  sys_reg (3, 4, 12, 13, 2)
#define GIC_ICH_LR11_EL2  sys_reg (3, 4, 12, 13, 3)
#define GIC_ICH_LR12_EL2  sys_reg (3, 4, 12, 13, 4)
#define GIC_ICH_LR13_EL2  sys_reg (3, 4, 12, 13, 5)
#define GIC_ICH_LR14_EL2  sys_reg (3, 4, 12, 13, 6)
#define GIC_ICH_LR15_EL2  sys_reg (3, 4, 12, 13, 7)

#endif
