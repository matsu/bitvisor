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

#ifndef _CORE_VMMERR_H
#define _CORE_VMMERR_H

enum vmmerr {
	VMMERR_SUCCESS = 0,
	VMMERR_GUESTSEG_LOAD_FAILED,
	VMMERR_GUESTSEG_NOT_PRESENT,
	VMMERR_INVALID_GUESTSEG,
	VMMERR_PAGE_NOT_PRESENT,
	VMMERR_PAGE_NOT_ACCESSIBLE,
	VMMERR_PAGE_NOT_EXECUTABLE,
	VMMERR_PAGE_BAD_RESERVED_BIT,
	VMMERR_INSTRUCTION_TOO_LONG,
	VMMERR_UNIMPLEMENTED_OPCODE,
	VMMERR_UNSUPPORTED_OPCODE,
	VMMERR_EXCEPTION_UD,
	VMMERR_AVOID_COMPILER_WARNING,
	VMMERR_SW,
	VMMERR_NOMEM,
	VMMERR_MSR_FAULT,
};

#define RET_IF_ERR(func) do { \
	enum vmmerr err; \
	err = (func); \
	if (err) \
		return err; \
} while (0)

#define RIE RET_IF_ERR

#endif
