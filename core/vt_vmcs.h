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

#ifndef _CORE_VT_VMCS_H
#define _CORE_VT_VMCS_H

#include "regs.h"

enum exit_qual_cr_lmsw {
	EXIT_QUAL_CR_LMSW_REGISTER = 0,
	EXIT_QUAL_CR_LMSW_MEMORY = 1,
};

enum exit_qual_cr_type {
	EXIT_QUAL_CR_TYPE_MOV_TO_CR = 0,
	EXIT_QUAL_CR_TYPE_MOV_FROM_CR = 1,
	EXIT_QUAL_CR_TYPE_CLTS = 2,
	EXIT_QUAL_CR_TYPE_LMSW = 3,
};

enum exit_qual_io_dir {
	EXIT_QUAL_IO_DIR_OUT = 0,
	EXIT_QUAL_IO_DIR_IN = 1,
};

enum exit_qual_io_op {
	EXIT_QUAL_IO_OP_DX = 0,
	EXIT_QUAL_IO_OP_IMMEDIATE = 1,
};

enum exit_qual_io_rep {
	EXIT_QUAL_IO_REP_NOT_REP = 0,
	EXIT_QUAL_IO_REP_REP = 1,
};

enum exit_qual_io_size {
	EXIT_QUAL_IO_SIZE_1BYTE = 0,
	EXIT_QUAL_IO_SIZE_2BYTE = 1,
	EXIT_QUAL_IO_SIZE_4BYTE = 3,
};

enum exit_qual_io_str {
	EXIT_QUAL_IO_STR_NOT_STRING = 0,
	EXIT_QUAL_IO_STR_STRING = 1,
};

enum exit_qual_ts_src {
	EXIT_QUAL_TS_SRC_CALL = 0,
	EXIT_QUAL_TS_SRC_IRET = 1,
	EXIT_QUAL_TS_SRC_JMP = 2,
	EXIT_QUAL_TS_SRC_INTR = 3,
};

enum intr_info_err {
	INTR_INFO_ERR_INVALID = 0,
	INTR_INFO_ERR_VALID = 1,
};

enum intr_info_type {
	INTR_INFO_TYPE_EXTERNAL = 0,
	INTR_INFO_TYPE_NMI = 2,
	INTR_INFO_TYPE_HARD_EXCEPTION = 3,
	INTR_INFO_TYPE_SOFT_INTR = 4,
	INTR_INFO_TYPE_PRIV_SOFT_EXCEPTION = 5,
	INTR_INFO_TYPE_SOFT_EXCEPTION = 6,
};

enum intr_info_valid {
	INTR_INFO_VALID_INVALID = 0,
	INTR_INFO_VALID_VALID = 1,
};

struct exit_qual_cr {
	enum control_reg num : 4;
	enum exit_qual_cr_type type : 2;
	enum exit_qual_cr_lmsw lmsw : 1;
	unsigned int reserved1 : 1;
	enum general_reg reg : 4;
	unsigned int reserved2 : 4;
	unsigned int lmsw_src : 16;
} __attribute__ ((packed));

struct exit_qual_io {
	enum exit_qual_io_size size : 3;
	enum exit_qual_io_dir dir : 1;
	enum exit_qual_io_str str : 1;
	enum exit_qual_io_rep rep : 1;
	enum exit_qual_io_op op : 1;
	unsigned int reserved1 : 9;
	unsigned int port : 16;
} __attribute__ ((packed));

struct exit_qual_ts {
	unsigned int sel : 16;
	unsigned int reserved1 : 14;
	enum exit_qual_ts_src src : 2;
	unsigned int reserved2 : 32;
};

struct intr_info {
	unsigned int vector : 8;
	enum intr_info_type type : 3;
	enum intr_info_err err : 1;
	unsigned int nmi : 1;
	unsigned int reserved : 18;
	enum intr_info_valid valid : 1;
} __attribute__ ((packed));

#endif
