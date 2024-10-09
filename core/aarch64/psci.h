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

#ifndef _CORE_AARCH64_PSCI_H
#define _CORE_AARCH64__H

#include <bits.h>

enum hypercall_func {
	HC_FUNC_ARM,
	HC_FUNC_CPU,
	HC_FUNC_SiP,
	HC_FUNC_OEM,
	HC_FUNC_SM,
	HC_FUNC_HV,
	HC_FUNC_VEN_H,
};

#define HC_FUNC_FASTCALL   BIT (31)
#define HC_FUNC_64BIT      BIT (30)
#define HC_FUNC_SERVICE(v) BITFIELD ((v), 0x3F, 24)
#define HC_FUNC_ID(v)      BITFIELD ((v), 0xFFFF, 0)

#define HC_FAST_SM (HC_FUNC_FASTCALL | HC_FUNC_SERVICE (HC_FUNC_SM))

#define HC_FAST_SM_32BIT (HC_FAST_SM)
#define HC_FAST_SM_64BIT (HC_FAST_SM | HC_FUNC_64BIT)

#define PSCI_VERSION_32BIT (HC_FAST_SM_32BIT | HC_FUNC_ID (0x0))
#define PSCI_VERSION_MAJOR(val) (((val) >> 16) & 0xFFFF)
#define PSCI_VERSION_MINOR(val) ((val) & 0xFFFF)

#define PSCI_CPU_SUSPEND_32BIT (HC_FAST_SM_32BIT | HC_FUNC_ID (0x1))
#define PSCI_CPU_SUSPEND_64BIT (HC_FAST_SM_64BIT | HC_FUNC_ID (0x1))
#define PSCI_CPU_SUSPEND_OS_INITIATED_MODE_SUPPORT(val) !!((val) & BIT (0))
#define PSCI_CPU_SUSPEND_EXT_FORMAT(val)		!!((val) & BIT (1))

#define PSCI_CPU_ON_32BIT (HC_FAST_SM_32BIT | HC_FUNC_ID (0x3))
#define PSCI_CPU_ON_64BIT (HC_FAST_SM_64BIT | HC_FUNC_ID (0x3))

#define PSCI_SYS_RESET_64BIT (HC_FAST_SM_64BIT | HC_FUNC_ID (0x9))

#define PSCI_FEATURES_32BIT (HC_FAST_SM_32BIT | HC_FUNC_ID (0xA))

#define PSCI_ERR_SUCCESS	   0
#define PSCI_ERR_NOT_SUPPORTED	  -1
#define PSCI_ERR_INVALID_PARAMS	  -2
#define PSCI_ERR_DENIED		  -3
#define PSCI_ERR_ALREADY_ON	  -4
#define PSCI_ERR_ON_PENDING	  -5
#define PSCI_ERR_INTERNAL_FAILURE -6
#define PSCI_ERR_NOT_PRESENT	  -7
#define PSCI_ERR_DISABLED	  -8
#define PSCI_ERR_INVALID_ADDR	  -9

#endif
