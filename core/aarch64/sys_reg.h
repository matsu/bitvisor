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

#ifndef _CORE_AARCH64_SYS_REG_H
#define _CORE_AARCH64_SYS_REG_H

/* Indirection is necessary to for concatenation chaining */
#define __concat(a, b) a##b
#define _concat(a, b)  __concat (a, b)

#define _op2(op2) _concat (_, op2)
#define _crm(crm) _concat (_C, crm)
#define _crn(crn) _concat (_C, crn)
#define _op1(op1) _concat (_, op1)
#define _op0(op0) _concat (S, op0)

/*
 * This macro is intended to use with msr()/mrs(). There are times that the
 * compiler does not recognize register name or there is no official register
 * name. sys_reg(3, 7, 14, 2, 2) expands to S3_7_C14_C2_2 for example.
 */
#define sys_reg(op0, op1, crn, crm, op2) \
	_concat (_op0 (op0), \
		 _concat (_concat (_op1 (op1), _crn (crn)), \
			  _concat (_crm (crm), _op2 (op2))))

/*
 * This macro produces an encoding of a system register in integer. This is
 * useful for determining a trapped system register accessed by msr()/mrs().
 */
#define sys_reg_encode(op0, op1, crn, crm, op2) \
	(((op0) & 0x3) | (((op1) & 0x7) << 2) | (((crn) & 0xF) << 5) | \
	 (((crm) & 0xF) << 9) | (((op2) & 0x7) << 13))

#endif
