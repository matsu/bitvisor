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

#ifndef _CORE_SVM_REGS_H
#define _CORE_SVM_REGS_H

#include "asm.h"
#include "cpu_seg.h"
#include "regs.h"

void svm_read_general_reg (enum general_reg reg, ulong *val);
void svm_write_general_reg (enum general_reg reg, ulong val);
void svm_read_control_reg (enum control_reg reg, ulong *val);
void svm_write_control_reg (enum control_reg reg, ulong val);
void svm_read_sreg_sel (enum sreg s, u16 *val);
void svm_read_sreg_acr (enum sreg s, ulong *val);
void svm_read_sreg_base (enum sreg s, ulong *val);
void svm_read_sreg_limit (enum sreg s, ulong *val);
void svm_read_ip (ulong *val);
void svm_write_ip (ulong val);
void svm_read_flags (ulong *val);
void svm_write_flags (ulong val);
void svm_read_gdtr (ulong *base, ulong *limit);
void svm_write_gdtr (ulong base, ulong limit);
void svm_read_idtr (ulong *base, ulong *limit);
void svm_write_idtr (ulong base, ulong limit);
void svm_write_realmode_seg (enum sreg s, u16 val);
enum vmmerr svm_writing_sreg (enum sreg s);

#endif
