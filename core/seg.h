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

#ifndef _CORE_SEG_H
#define _CORE_SEG_H

#include "types.h"

#define SEG_SEL_NULL		0
#define SEG_SEL_CODE32		( 1 * 8) /* SYSENTER_CS+0 */
#define SEG_SEL_DATA32		( 2 * 8) /* SYSENTER_CS+8 */
#define SEG_SEL_CODE32U		( 3 * 8 + 3) /* SYSENTER_CS+16, SYSRET_CS+0 */
#define SEG_SEL_DATA32U		( 4 * 8 + 3) /* SYSENTER_CS+24, SYSRET_CS+8 */
#define SEG_SEL_CODE64U		( 5 * 8 + 3) /* SYSRET_CS+16 */
#define SEG_SEL_CODE16		( 6 * 8)
#define SEG_SEL_DATA16		( 7 * 8)
#define SEG_SEL_PCPU32		( 8 * 8)
#define SEG_SEL_CALLGATE32	( 9 * 8)
#define SEG_SEL_CODE64		(10 * 8)
#define SEG_SEL_DATA64		(11 * 8)
#define SEG_SEL_TSS32		(12 * 8)
#define SEG_SEL_TSS64		(13 * 8)
#define SEG_SEL_TSS64_		(14 * 8) /* Reserved */
#define SEG_SEL_PCPU64		(15 * 8)

void get_seg_base (ulong gdtbase, u16 ldtr, u16 sel, ulong *segbase);
u32 get_seg_access_rights (u16 sel);
void segment_wakeup (bool bsp);
void segment_init_ap (int cpunum);

#endif
