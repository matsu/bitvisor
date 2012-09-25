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

#include "asm.h"
#include "comphappy.h"
#include "constants.h"
#include "cpu_emul.h"
#include "cpu_interpreter.h"
#include "cpu_mmu.h"
#include "cpu_seg.h"
#include "cpu_stack.h"
#include "current.h"
#include "io_io.h"
#include "panic.h"
#include "printf.h"		/* DEBUG */

#define PREFIX_LOCK		0xF0
#define PREFIX_REPNE		0xF2
#define PREFIX_REPE		0xF3
#define PREFIX_ES		0x26
#define PREFIX_CS		0x2E
#define PREFIX_SS		0x36
#define PREFIX_DS		0x3E
#define PREFIX_FS		0x64
#define PREFIX_GS		0x65
#define PREFIX_OPERAND_SIZE	0x66
#define PREFIX_ADDRESS_SIZE	0x67
#define PREFIX_REX_MIN		0x40
#define PREFIX_REX_MAX		0x4F

#define OPCODE_0x0F			0x0F
#define OPCODE_0x0F_MOV_TO_CR		0x22
#define OPCODE_0x0F_MOV_FROM_CR		0x20
#define OPCODE_0x0F_MOV_TO_DR		0x23
#define OPCODE_0x0F_MOV_FROM_DR		0x21
#define OPCODE_0x0F_CLTS		0x06
#define OPCODE_HLT			0xF4
#define OPCODE_CLI			0xFA
#define OPCODE_STI			0xFB
#define OPCODE_PUSHF			0x9C
#define OPCODE_POPF			0x9D
#define OPCODE_IRET			0xCF
#define OPCODE_0x0F_INVD		0x08
#define OPCODE_0x0F_WBINVD		0x09
#define OPCODE_0x0F_0x01		0x01
#define OPCODE_0x0F_0x01_LGDT		0x2
#define OPCODE_0x0F_0x01_LIDT		0x3
#define OPCODE_0x0F_0x01_SMSW		0x4
#define OPCODE_0x0F_0x01_LMSW		0x6
#define OPCODE_0x0F_0x01_INVLPG		0x7
#define OPCODE_0x0F_RDMSR		0x32
#define OPCODE_0x0F_WRMSR		0x30
#define OPCODE_INT			0xCD
#define OPCODE_INT3			0xCC
#define OPCODE_INTO			0xCE
#define OPCODE_INB_IMM			0xE4
#define OPCODE_IN_IMM			0xE5
#define OPCODE_INB_DX			0xEC
#define OPCODE_IN_DX			0xED
#define OPCODE_INSB			0x6C
#define OPCODE_INS			0x6D
#define OPCODE_OUTB_IMM			0xE6
#define OPCODE_OUT_IMM			0xE7
#define OPCODE_OUTB_DX			0xEE
#define OPCODE_OUT_DX			0xEF
#define OPCODE_OUTSB			0x6E
#define OPCODE_OUTS			0x6F
#define OPCODE_MOV_FROM_SR		0x8C
#define OPCODE_MOV_TO_SR		0x8E
#define OPCODE_PUSH_ES			0x06
#define OPCODE_PUSH_CS			0x0E
#define OPCODE_PUSH_SS			0x16
#define OPCODE_PUSH_DS			0x1E
#define OPCODE_0x0F_PUSH_FS		0xA0
#define OPCODE_0x0F_PUSH_GS		0xA8
#define OPCODE_POP_ES			0x07
#define OPCODE_POP_CS			0x0F /* i8086 and i8088 only */
#define OPCODE_POP_SS			0x17
#define OPCODE_POP_DS			0x1F
#define OPCODE_0x0F_POP_FS		0xA1
#define OPCODE_0x0F_POP_GS		0xA9
#define OPCODE_JMP_FAR_DIRECT		0xEA
#define OPCODE_0xFF			0xFF
#define OPCODE_0xFF_JMP_FAR_INDIRECT	0x05
#define OPCODE_RETF			0xCB
#define OPCODE_0x0F_LSS			0xB2

/* --- for MMIO --- */
#define OPCODE_MOVSB			0xA4
#define OPCODE_MOVS			0xA5
#define OPCODE_STOSB			0xAA
#define OPCODE_STOS			0xAB
#define OPCODE_0x0F_MOVZX_RM8_TO_R	0xB6
#define OPCODE_0x0F_MOVZX_RM16_TO_R	0xB7
#define OPCODE_0xFF_PUSH		0x06
#define OPCODE_0x8F			0x8F
#define OPCODE_0x8F_POP			0x00
#define OPCODE_0x0F_0xBA		0xBA
#define OPCODE_0x0F_0xBA_BT		0x4

enum addrtype {
	ADDRTYPE_16BIT,
	ADDRTYPE_32BIT,
	ADDRTYPE_64BIT,
};

enum reptype {
	REPTYPE_16BIT,
	REPTYPE_32BIT,
	REPTYPE_64BIT,
};

enum reg {
	REG_NO = -1,
	REG_AX = 0,		/* AX/EAX/RAX */
	REG_CX = 1,		/* CX/ECX/RCX */
	REG_DX = 2,		/* DX/EDX/RDX */
	REG_BX = 3,		/* BX/EBX/RBX */
	REG_SP = 4,		/* SP/ESP/RSP */
	REG_BP = 5,		/* BP/EBP/RBP */
	REG_SI = 6,		/* SI/ESI/RSI */
	REG_DI = 7,		/* DI/EDI/RDI */
	REG_08 = 8,		/* R8W/R8D/R8 */
	REG_09 = 9,		/* R9W/R9D/R9 */
	REG_10 = 10,		/* R10W/R10D/R10 */
	REG_11 = 11,		/* R11W/R11D/R11 */
	REG_12 = 12,		/* R12W/R12D/R12 */
	REG_13 = 13,		/* R13W/R13D/R13 */
	REG_14 = 14,		/* R14W/R14D/R14 */
	REG_15 = 15,		/* R15W/R15D/R15 */
};

enum reg8 {
	REG_AL = 0,		/* AL */
	REG_CL = 1,		/* CL */
	REG_DL = 2,		/* DL */
	REG_BL = 3,		/* BL */
	REG_AH = 4,		/* AH (No REX prefixes) */
	REG_CH = 5,		/* CH (No REX prefixes) */
	REG_DH = 6,		/* DH (No REX prefixes) */
	REG_BH = 7,		/* BH (No REX prefixes) */
	REG_SPL = 4,		/* SPL (With a REX prefix) */
	REG_BPL = 5,		/* BPL (With a REX prefix) */
	REG_SIL = 6,		/* SIL (With a REX prefix) */
	REG_DIL = 7,		/* DIL (With a REX prefix) */
	REG_R8B = 8,		/* R8B */
	REG_R9B = 9,		/* R9B */
	REG_R10B = 10,		/* R10B */
	REG_R11B = 11,		/* R11B */
	REG_R12B = 12,		/* R12B */
	REG_R13B = 13,		/* R13B */
	REG_R14B = 14,		/* R14B */
	REG_R15B = 15,		/* R15B */
};

enum idata_type {
	I_ZERO,			/* not implemented */
	I_MODRM,		/* Mod R/M byte */
	I_IMME1,		/* Immediate 1 byte */
	I_IMME2,		/* Immediate 2/4 byte */
	I_MIMM1,		/* Mod R/M byte and Immediate 1 byte */
	I_MIMM2,		/* Mod R/M byte and Immediate 2/4 byte */
	I_MOFFS,		/* displacement */
	I_MGRP3,		/* Group 3 special */
	I_IMMED,		/* Immediate (for Group 3) */
	I_NOMOR,		/* there are no more bytes (for Group 3) */
};

enum idata_operand {
	O_ZERO,			/* no operands */
	O_M,			/* memory or register by Mod-R/M */
	O_R,			/* register by bit3-5 of Mod-R/M */
	O_A,			/* AL/AX/EAX register */
	O_I,			/* Immediate */
	O_1,			/* constant 1 (for bit shift) */
	O_C,			/* CL register (for bit shift) */
};

enum idata_function {
	F_ZERO,
	F_ADD,
	F_OR,
	F_ADC,
	F_SBB,
	F_AND,
	F_SUB,
	F_XOR,
	F_CMP,
	F_TEST,
	F_XCHG,
	F_MOV,
	F_NOT,
	F_NEG,
	F_ROL,
	F_ROR,
	F_RCL,
	F_RCR,
	F_SHL,
	F_SHR,
	F_SAR,
	F_GRP1,
	F_GRP2,
	F_GRP11,
};

struct prefix {
	enum sreg seg : 3;
	unsigned int lock : 1;
	unsigned int repne : 1;
	unsigned int repe : 1;
	unsigned int opsize : 1;
	unsigned int addrsize : 1;
	union {
		struct {
			unsigned int b : 1;
			unsigned int x : 1;
			unsigned int r : 1;
			unsigned int w : 1;
		} b;
		struct {
			unsigned int val : 8;
		} v;
	} rex;
};

struct modrm {
	unsigned int rm : 3;
	unsigned int reg : 3;
	unsigned int mod : 2;
};

struct sib {
	unsigned int base : 3;
	unsigned int index : 3;
	unsigned int scale : 2;
};

struct op {
	struct prefix prefix;
	struct modrm modrm;
	struct sib sib;
	i32 disp;		/* maximum size of a displacement is 32bit */
	u64 imm;
	enum reg modrm_brm, modrm_rreg;
	enum sreg modrm_seg;
	u64 modrm_addr;
	enum cpumode mode;
	enum addrtype addrtype;
	enum optype optype;
	enum reptype reptype;
	ulong ip;
	uint ip_off;
	struct realmode_sysregs *rsr;
	bool longmode;
	bool modrm_ripflag;
};

struct modrm_info {
	int displen;
	enum reg reg1, reg2;
	enum sreg defseg;
	unsigned int sibflag : 1;
	unsigned int ripflag : 1;
};

struct sibbase_info {
	int displen;
	enum reg reg;
	enum sreg defseg;
};

struct sibscale_info {
	enum reg reg;
};

struct execinst_io_data {
	enum iotype type;
};

struct idata {
	enum idata_type type : 4;
	int len : 4;
	enum idata_operand dst : 4;
	enum idata_operand src : 4;
	enum idata_function func : 16;
};

static struct modrm_info modrmmatrix16[3][8] = { /* [mod][rm] */
	/* displen, reg1, reg2, defseg, sibflag, ripflag */
	{
		{ 0, REG_BX, REG_SI, SREG_DS, 0, 0 },
		{ 0, REG_BX, REG_DI, SREG_DS, 0, 0 },
		{ 0, REG_BP, REG_SI, SREG_SS, 0, 0 },
		{ 0, REG_BP, REG_DI, SREG_SS, 0, 0 },
		{ 0, REG_SI, REG_NO, SREG_DS, 0, 0 },
		{ 0, REG_DI, REG_NO, SREG_DS, 0, 0 },
		{ 2, REG_NO, REG_NO, SREG_DS, 0, 0 },
		{ 0, REG_BX, REG_NO, SREG_DS, 0, 0 },
	},
	{
		{ 1, REG_BX, REG_SI, SREG_DS, 0, 0 },
		{ 1, REG_BX, REG_DI, SREG_DS, 0, 0 },
		{ 1, REG_BP, REG_SI, SREG_SS, 0, 0 },
		{ 1, REG_BP, REG_DI, SREG_SS, 0, 0 },
		{ 1, REG_SI, REG_NO, SREG_DS, 0, 0 },
		{ 1, REG_DI, REG_NO, SREG_DS, 0, 0 },
		{ 1, REG_BP, REG_NO, SREG_SS, 0, 0 },
		{ 1, REG_BX, REG_NO, SREG_DS, 0, 0 },
	},
	{
		{ 2, REG_BX, REG_SI, SREG_DS, 0, 0 },
		{ 2, REG_BX, REG_DI, SREG_DS, 0, 0 },
		{ 2, REG_BP, REG_SI, SREG_SS, 0, 0 },
		{ 2, REG_BP, REG_DI, SREG_SS, 0, 0 },
		{ 2, REG_SI, REG_NO, SREG_DS, 0, 0 },
		{ 2, REG_DI, REG_NO, SREG_DS, 0, 0 },
		{ 2, REG_BP, REG_NO, SREG_SS, 0, 0 },
		{ 2, REG_BX, REG_NO, SREG_DS, 0, 0 },
	},
};

static struct modrm_info modrmmatrix32[3][2][8] = { /* [mod][rex.b][rm] */
	/* displen, reg1, reg2, defseg, sibflag, ripflag */
	{
		{
			{ 0, REG_AX, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_CX, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_DX, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_BX, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_NO, REG_NO, SREG_DS, 1, 0 },
			{ 4, REG_NO, REG_NO, SREG_DS, 0, 1 },
			{ 0, REG_SI, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_DI, REG_NO, SREG_DS, 0, 0 },
		},
		{
			{ 0, REG_08, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_09, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_10, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_11, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_NO, REG_NO, SREG_DS, 1, 0 },
			{ 4, REG_NO, REG_NO, SREG_DS, 0, 1 },
			{ 0, REG_14, REG_NO, SREG_DS, 0, 0 },
			{ 0, REG_15, REG_NO, SREG_DS, 0, 0 },
		},
	},
	{
		{
			{ 1, REG_AX, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_CX, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_DX, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_BX, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_NO, REG_NO, SREG_DS, 1, 0 },
			{ 1, REG_BP, REG_NO, SREG_SS, 0, 0 },
			{ 1, REG_SI, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_DI, REG_NO, SREG_DS, 0, 0 },
		},
		{
			{ 1, REG_08, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_09, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_10, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_11, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_NO, REG_NO, SREG_DS, 1, 0 },
			{ 1, REG_13, REG_NO, SREG_SS, 0, 0 },
			{ 1, REG_14, REG_NO, SREG_DS, 0, 0 },
			{ 1, REG_15, REG_NO, SREG_DS, 0, 0 },
		},
	},
	{
		{
			{ 4, REG_AX, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_CX, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_DX, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_BX, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_NO, REG_NO, SREG_DS, 1, 0 },
			{ 4, REG_BP, REG_NO, SREG_SS, 0, 0 },
			{ 4, REG_SI, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_DI, REG_NO, SREG_DS, 0, 0 },
		},
		{
			{ 4, REG_08, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_09, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_10, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_11, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_NO, REG_NO, SREG_DS, 1, 0 },
			{ 4, REG_13, REG_NO, SREG_SS, 0, 0 },
			{ 4, REG_14, REG_NO, SREG_DS, 0, 0 },
			{ 4, REG_15, REG_NO, SREG_DS, 0, 0 },
		},
	},
};

static struct sibbase_info sibmatrix_base[3][2][8] = { /* [mod][rex.b][base] */
	/* displen (override), reg, defseg (override) */
	{
		{
			{ 0, REG_AX, SREG_DS },
			{ 0, REG_CX, SREG_DS },
			{ 0, REG_DX, SREG_DS },
			{ 0, REG_BX, SREG_DS },
			{ 0, REG_SP, SREG_SS },
			{ 4, REG_NO, SREG_DS }, /* [*] */
			{ 0, REG_SI, SREG_DS },
			{ 0, REG_DI, SREG_DS },
		},
		{
			{ 0, REG_08, SREG_DS },
			{ 0, REG_09, SREG_DS },
			{ 0, REG_10, SREG_DS },
			{ 0, REG_11, SREG_DS },
			{ 0, REG_12, SREG_SS },
			{ 4, REG_NO, SREG_DS }, /* [*] */
			{ 0, REG_14, SREG_DS },
			{ 0, REG_15, SREG_DS },
		},
	},
	{
		{
			{ 1, REG_AX, SREG_DS },
			{ 1, REG_CX, SREG_DS },
			{ 1, REG_DX, SREG_DS },
			{ 1, REG_BX, SREG_DS },
			{ 1, REG_SP, SREG_SS },
			{ 1, REG_BP, SREG_SS },
			{ 1, REG_SI, SREG_DS },
			{ 1, REG_DI, SREG_DS },
		},
		{
			{ 1, REG_08, SREG_DS },
			{ 1, REG_09, SREG_DS },
			{ 1, REG_10, SREG_DS },
			{ 1, REG_11, SREG_DS },
			{ 1, REG_12, SREG_SS },
			{ 1, REG_13, SREG_SS },
			{ 1, REG_14, SREG_DS },
			{ 1, REG_15, SREG_DS },
		},
	},		
	{
		{
			{ 4, REG_AX, SREG_DS },
			{ 4, REG_CX, SREG_DS },
			{ 4, REG_DX, SREG_DS },
			{ 4, REG_BX, SREG_DS },
			{ 4, REG_SP, SREG_SS },
			{ 4, REG_BP, SREG_SS },
			{ 4, REG_SI, SREG_DS },
			{ 4, REG_DI, SREG_DS },
		},
		{
			{ 4, REG_08, SREG_DS },
			{ 4, REG_09, SREG_DS },
			{ 4, REG_10, SREG_DS },
			{ 4, REG_11, SREG_DS },
			{ 4, REG_12, SREG_SS },
			{ 4, REG_13, SREG_SS },
			{ 4, REG_14, SREG_DS },
			{ 4, REG_15, SREG_DS },
		},
	},		
};

static struct sibscale_info sib_scale[2][8] = { /* [rex.x][index] */
	/* reg */
	{
		{ REG_AX },
		{ REG_CX },
		{ REG_DX },
		{ REG_BX },
		{ REG_NO },
		{ REG_BP },
		{ REG_SI },
		{ REG_DI },
	},
	{
		{ REG_08 },
		{ REG_09 },
		{ REG_10 },
		{ REG_11 },
		{ REG_12 },
		{ REG_13 },
		{ REG_14 },
		{ REG_15 },
	},
};

static struct idata idata[256] = {
	{ I_MODRM, 1, O_M, O_R, F_ADD }, /* 0x00 */
	{ I_MODRM, 2, O_M, O_R, F_ADD }, /* 0x01 */
	{ I_MODRM, 1, O_R, O_M, F_ADD }, /* 0x02 */
	{ I_MODRM, 2, O_R, O_M, F_ADD }, /* 0x03 */
	{ I_IMME1, 1, O_A, O_I, F_ADD }, /* 0x04 */
	{ I_IMME2, 2, O_A, O_I, F_ADD }, /* 0x05 */
	{ 0, 0, 0, 0, 0 },		 /* 0x06 */
	{ 0, 0, 0, 0, 0 },		 /* 0x07 */
	{ I_MODRM, 1, O_M, O_R, F_OR  }, /* 0x08 */
	{ I_MODRM, 2, O_M, O_R, F_OR  }, /* 0x09 */
	{ I_MODRM, 1, O_R, O_M, F_OR  }, /* 0x0A */
	{ I_MODRM, 2, O_R, O_M, F_OR  }, /* 0x0B */
	{ I_IMME1, 1, O_A, O_I, F_OR  }, /* 0x0C */
	{ I_IMME2, 2, O_A, O_I, F_OR  }, /* 0x0D */
	{ 0, 0, 0, 0, 0 },		 /* 0x0E */
	{ 0, 0, 0, 0, 0 },		 /* 0x0F */
	{ I_MODRM, 1, O_M, O_R, F_ADC }, /* 0x10 */
	{ I_MODRM, 2, O_M, O_R, F_ADC }, /* 0x11 */
	{ I_MODRM, 1, O_R, O_M, F_ADC }, /* 0x12 */
	{ I_MODRM, 2, O_R, O_M, F_ADC }, /* 0x13 */
	{ I_IMME1, 1, O_A, O_I, F_ADC }, /* 0x14 */
	{ I_IMME2, 2, O_A, O_I, F_ADC }, /* 0x15 */
	{ 0, 0, 0, 0, 0 },		 /* 0x16 */
	{ 0, 0, 0, 0, 0 },		 /* 0x17 */
	{ I_MODRM, 1, O_M, O_R, F_SBB }, /* 0x18 */
	{ I_MODRM, 2, O_M, O_R, F_SBB }, /* 0x19 */
	{ I_MODRM, 1, O_R, O_M, F_SBB }, /* 0x1A */
	{ I_MODRM, 2, O_R, O_M, F_SBB }, /* 0x1B */
	{ I_IMME1, 1, O_A, O_I, F_SBB }, /* 0x1C */
	{ I_IMME2, 2, O_A, O_I, F_SBB }, /* 0x1D */
	{ 0, 0, 0, 0, 0 },		 /* 0x1E */
	{ 0, 0, 0, 0, 0 },		 /* 0x1F */
	{ I_MODRM, 1, O_M, O_R, F_AND }, /* 0x20 */
	{ I_MODRM, 2, O_M, O_R, F_AND }, /* 0x21 */
	{ I_MODRM, 1, O_R, O_M, F_AND }, /* 0x22 */
	{ I_MODRM, 2, O_R, O_M, F_AND }, /* 0x23 */
	{ I_IMME1, 1, O_A, O_I, F_AND }, /* 0x24 */
	{ I_IMME2, 2, O_A, O_I, F_AND }, /* 0x25 */
	{ 0, 0, 0, 0, 0 },		 /* 0x26 */
	{ 0, 0, 0, 0, 0 },		 /* 0x27 */
	{ I_MODRM, 1, O_M, O_R, F_SUB }, /* 0x28 */
	{ I_MODRM, 2, O_M, O_R, F_SUB }, /* 0x29 */
	{ I_MODRM, 1, O_R, O_M, F_SUB }, /* 0x2A */
	{ I_MODRM, 2, O_R, O_M, F_SUB }, /* 0x2B */
	{ I_IMME1, 1, O_A, O_I, F_SUB }, /* 0x2C */
	{ I_IMME2, 2, O_A, O_I, F_SUB }, /* 0x2D */
	{ 0, 0, 0, 0, 0 },		 /* 0x2E */
	{ 0, 0, 0, 0, 0 },		 /* 0x2F */
	{ I_MODRM, 1, O_M, O_R, F_XOR }, /* 0x30 */
	{ I_MODRM, 2, O_M, O_R, F_XOR }, /* 0x31 */
	{ I_MODRM, 1, O_R, O_M, F_XOR }, /* 0x32 */
	{ I_MODRM, 2, O_R, O_M, F_XOR }, /* 0x33 */
	{ I_IMME1, 1, O_A, O_I, F_XOR }, /* 0x34 */
	{ I_IMME2, 2, O_A, O_I, F_XOR }, /* 0x35 */
	{ 0, 0, 0, 0, 0 },		 /* 0x36 */
	{ 0, 0, 0, 0, 0 },		 /* 0x37 */
	{ I_MODRM, 1, O_M, O_R, F_CMP }, /* 0x38 */
	{ I_MODRM, 2, O_M, O_R, F_CMP }, /* 0x39 */
	{ I_MODRM, 1, O_R, O_M, F_CMP }, /* 0x3A */
	{ I_MODRM, 2, O_R, O_M, F_CMP }, /* 0x3B */
	{ I_IMME1, 1, O_A, O_I, F_CMP }, /* 0x3C */
	{ I_IMME2, 2, O_A, O_I, F_CMP }, /* 0x3D */
	{ 0, 0, 0, 0, 0 },		 /* 0x3E */
	{ 0, 0, 0, 0, 0 },		 /* 0x3F */
	{ 0, 0, 0, 0, 0 },		 /* 0x40 */
	{ 0, 0, 0, 0, 0 },		 /* 0x41 */
	{ 0, 0, 0, 0, 0 },		 /* 0x42 */
	{ 0, 0, 0, 0, 0 },		 /* 0x43 */
	{ 0, 0, 0, 0, 0 },		 /* 0x44 */
	{ 0, 0, 0, 0, 0 },		 /* 0x45 */
	{ 0, 0, 0, 0, 0 },		 /* 0x46 */
	{ 0, 0, 0, 0, 0 },		 /* 0x47 */
	{ 0, 0, 0, 0, 0 },		 /* 0x48 */
	{ 0, 0, 0, 0, 0 },		 /* 0x49 */
	{ 0, 0, 0, 0, 0 },		 /* 0x4A */
	{ 0, 0, 0, 0, 0 },		 /* 0x4B */
	{ 0, 0, 0, 0, 0 },		 /* 0x4C */
	{ 0, 0, 0, 0, 0 },		 /* 0x4D */
	{ 0, 0, 0, 0, 0 },		 /* 0x4E */
	{ 0, 0, 0, 0, 0 },		 /* 0x4F */
	{ 0, 0, 0, 0, 0 },		 /* 0x50 */
	{ 0, 0, 0, 0, 0 },		 /* 0x51 */
	{ 0, 0, 0, 0, 0 },		 /* 0x52 */
	{ 0, 0, 0, 0, 0 },		 /* 0x53 */
	{ 0, 0, 0, 0, 0 },		 /* 0x54 */
	{ 0, 0, 0, 0, 0 },		 /* 0x55 */
	{ 0, 0, 0, 0, 0 },		 /* 0x56 */
	{ 0, 0, 0, 0, 0 },		 /* 0x57 */
	{ 0, 0, 0, 0, 0 },		 /* 0x58 */
	{ 0, 0, 0, 0, 0 },		 /* 0x59 */
	{ 0, 0, 0, 0, 0 },		 /* 0x5A */
	{ 0, 0, 0, 0, 0 },		 /* 0x5B */
	{ 0, 0, 0, 0, 0 },		 /* 0x5C */
	{ 0, 0, 0, 0, 0 },		 /* 0x5D */
	{ 0, 0, 0, 0, 0 },		 /* 0x5E */
	{ 0, 0, 0, 0, 0 },		 /* 0x5F */
	{ 0, 0, 0, 0, 0 },		 /* 0x60 */
	{ 0, 0, 0, 0, 0 },		 /* 0x61 */
	{ 0, 0, 0, 0, 0 },		 /* 0x62 */
	{ 0, 0, 0, 0, 0 },		 /* 0x63 */
	{ 0, 0, 0, 0, 0 },		 /* 0x64 */
	{ 0, 0, 0, 0, 0 },		 /* 0x65 */
	{ 0, 0, 0, 0, 0 },		 /* 0x66 */
	{ 0, 0, 0, 0, 0 },		 /* 0x67 */
	{ 0, 0, 0, 0, 0 },		 /* 0x68 */
	{ 0, 0, 0, 0, 0 },		 /* 0x69 */
	{ 0, 0, 0, 0, 0 },		 /* 0x6A */
	{ 0, 0, 0, 0, 0 },		 /* 0x6B */
	{ 0, 0, 0, 0, 0 },		 /* 0x6C */
	{ 0, 0, 0, 0, 0 },		 /* 0x6D */
	{ 0, 0, 0, 0, 0 },		 /* 0x6E */
	{ 0, 0, 0, 0, 0 },		 /* 0x6F */
	{ 0, 0, 0, 0, 0 },		 /* 0x70 */
	{ 0, 0, 0, 0, 0 },		 /* 0x71 */
	{ 0, 0, 0, 0, 0 },		 /* 0x72 */
	{ 0, 0, 0, 0, 0 },		 /* 0x73 */
	{ 0, 0, 0, 0, 0 },		 /* 0x74 */
	{ 0, 0, 0, 0, 0 },		 /* 0x75 */
	{ 0, 0, 0, 0, 0 },		 /* 0x76 */
	{ 0, 0, 0, 0, 0 },		 /* 0x77 */
	{ 0, 0, 0, 0, 0 },		 /* 0x78 */
	{ 0, 0, 0, 0, 0 },		 /* 0x79 */
	{ 0, 0, 0, 0, 0 },		 /* 0x7A */
	{ 0, 0, 0, 0, 0 },		 /* 0x7B */
	{ 0, 0, 0, 0, 0 },		 /* 0x7C */
	{ 0, 0, 0, 0, 0 },		 /* 0x7D */
	{ 0, 0, 0, 0, 0 },		 /* 0x7E */
	{ 0, 0, 0, 0, 0 },		 /* 0x7F */
	{ I_MIMM1, 1, O_M, O_I, F_GRP1 }, /* 0x80 */
	{ I_MIMM2, 2, O_M, O_I, F_GRP1 }, /* 0x81 */
	{ I_MIMM1, 1, O_M, O_I, F_GRP1 }, /* 0x82 */
	{ I_MIMM1, 2, O_M, O_I, F_GRP1 }, /* 0x83 */
	{ I_MODRM, 1, O_M, O_R, F_TEST }, /* 0x84 */
	{ I_MODRM, 2, O_M, O_R, F_TEST }, /* 0x85 */
	{ I_MODRM, 1, O_M, O_R, F_XCHG }, /* 0x86 */
	{ I_MODRM, 2, O_M, O_R, F_XCHG }, /* 0x87 */
	{ I_MODRM, 1, O_M, O_R, F_MOV }, /* 0x88 */
	{ I_MODRM, 2, O_M, O_R, F_MOV }, /* 0x89 */
	{ I_MODRM, 1, O_R, O_M, F_MOV }, /* 0x8A */
	{ I_MODRM, 2, O_R, O_M, F_MOV }, /* 0x8B */
	{ 0, 0, 0, 0, 0 },		 /* 0x8C */
	{ 0, 0, 0, 0, 0 },		 /* 0x8D */
	{ 0, 0, 0, 0, 0 },		 /* 0x8E */
	{ 0, 0, 0, 0, 0 },		 /* 0x8F */
	{ 0, 0, 0, 0, 0 },		 /* 0x90 */
	{ 0, 0, 0, 0, 0 },		 /* 0x91 */
	{ 0, 0, 0, 0, 0 },		 /* 0x92 */
	{ 0, 0, 0, 0, 0 },		 /* 0x93 */
	{ 0, 0, 0, 0, 0 },		 /* 0x94 */
	{ 0, 0, 0, 0, 0 },		 /* 0x95 */
	{ 0, 0, 0, 0, 0 },		 /* 0x96 */
	{ 0, 0, 0, 0, 0 },		 /* 0x97 */
	{ 0, 0, 0, 0, 0 },		 /* 0x98 */
	{ 0, 0, 0, 0, 0 },		 /* 0x99 */
	{ 0, 0, 0, 0, 0 },		 /* 0x9A */
	{ 0, 0, 0, 0, 0 },		 /* 0x9B */
	{ 0, 0, 0, 0, 0 },		 /* 0x9C */
	{ 0, 0, 0, 0, 0 },		 /* 0x9D */
	{ 0, 0, 0, 0, 0 },		 /* 0x9E */
	{ 0, 0, 0, 0, 0 },		 /* 0x9F */
	{ I_MOFFS, 1, O_A, O_M, F_MOV }, /* 0xA0 */
	{ I_MOFFS, 2, O_A, O_M, F_MOV }, /* 0xA1 */
	{ I_MOFFS, 1, O_M, O_A, F_MOV }, /* 0xA2 */
	{ I_MOFFS, 2, O_M, O_A, F_MOV }, /* 0xA3 */
	{ 0, 0, 0, 0, 0 },		 /* 0xA4 */
	{ 0, 0, 0, 0, 0 },		 /* 0xA5 */
	{ 0, 0, 0, 0, 0 },		 /* 0xA6 */
	{ 0, 0, 0, 0, 0 },		 /* 0xA7 */
	{ 0, 0, 0, 0, 0 },		 /* 0xA8 */
	{ 0, 0, 0, 0, 0 },		 /* 0xA9 */
	{ 0, 0, 0, 0, 0 },		 /* 0xAA */
	{ 0, 0, 0, 0, 0 },		 /* 0xAB */
	{ 0, 0, 0, 0, 0 },		 /* 0xAC */
	{ 0, 0, 0, 0, 0 },		 /* 0xAD */
	{ 0, 0, 0, 0, 0 },		 /* 0xAE */
	{ 0, 0, 0, 0, 0 },		 /* 0xAF */
	{ 0, 0, 0, 0, 0 },		 /* 0xB0 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB1 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB2 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB3 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB4 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB5 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB6 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB7 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB8 */
	{ 0, 0, 0, 0, 0 },		 /* 0xB9 */
	{ 0, 0, 0, 0, 0 },		 /* 0xBA */
	{ 0, 0, 0, 0, 0 },		 /* 0xBB */
	{ 0, 0, 0, 0, 0 },		 /* 0xBC */
	{ 0, 0, 0, 0, 0 },		 /* 0xBD */
	{ 0, 0, 0, 0, 0 },		 /* 0xBE */
	{ 0, 0, 0, 0, 0 },		 /* 0xBF */
	{ I_MIMM1, 1, O_M, O_I, F_GRP2 }, /* 0xC0 */
	{ I_MIMM1, 2, O_M, O_I, F_GRP2 }, /* 0xC1 */
	{ 0, 0, 0, 0, 0 },		 /* 0xC2 */
	{ 0, 0, 0, 0, 0 },		 /* 0xC3 */
	{ 0, 0, 0, 0, 0 },		 /* 0xC4 */
	{ 0, 0, 0, 0, 0 },		 /* 0xC5 */
	{ I_MIMM1, 1, O_M, O_I, F_GRP11 }, /* 0xC6 */
	{ I_MIMM2, 2, O_M, O_I, F_GRP11 }, /* 0xC7 */
	{ 0, 0, 0, 0, 0 },		 /* 0xC8 */
	{ 0, 0, 0, 0, 0 },		 /* 0xC9 */
	{ 0, 0, 0, 0, 0 },		 /* 0xCA */
	{ 0, 0, 0, 0, 0 },		 /* 0xCB */
	{ 0, 0, 0, 0, 0 },		 /* 0xCC */
	{ 0, 0, 0, 0, 0 },		 /* 0xCD */
	{ 0, 0, 0, 0, 0 },		 /* 0xCE */
	{ 0, 0, 0, 0, 0 },		 /* 0xCF */
	{ I_MODRM, 1, O_M, O_1, F_GRP2 }, /* 0xD0 */
	{ I_MODRM, 2, O_M, O_1, F_GRP2 }, /* 0xD1 */
	{ I_MODRM, 1, O_M, O_C, F_GRP2 }, /* 0xD2 */
	{ I_MODRM, 2, O_M, O_C, F_GRP2 }, /* 0xD3 */
	{ 0, 0, 0, 0, 0 },		 /* 0xD4 */
	{ 0, 0, 0, 0, 0 },		 /* 0xD5 */
	{ 0, 0, 0, 0, 0 },		 /* 0xD6 */
	{ 0, 0, 0, 0, 0 },		 /* 0xD7 */
	{ 0, 0, 0, 0, 0 },		 /* 0xD8 */
	{ 0, 0, 0, 0, 0 },		 /* 0xD9 */
	{ 0, 0, 0, 0, 0 },		 /* 0xDA */
	{ 0, 0, 0, 0, 0 },		 /* 0xDB */
	{ 0, 0, 0, 0, 0 },		 /* 0xDC */
	{ 0, 0, 0, 0, 0 },		 /* 0xDD */
	{ 0, 0, 0, 0, 0 },		 /* 0xDE */
	{ 0, 0, 0, 0, 0 },		 /* 0xDF */
	{ 0, 0, 0, 0, 0 },		 /* 0xE0 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE1 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE2 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE3 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE4 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE5 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE6 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE7 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE8 */
	{ 0, 0, 0, 0, 0 },		 /* 0xE9 */
	{ 0, 0, 0, 0, 0 },		 /* 0xEA */
	{ 0, 0, 0, 0, 0 },		 /* 0xEB */
	{ 0, 0, 0, 0, 0 },		 /* 0xEC */
	{ 0, 0, 0, 0, 0 },		 /* 0xED */
	{ 0, 0, 0, 0, 0 },		 /* 0xEE */
	{ 0, 0, 0, 0, 0 },		 /* 0xEF */
	{ 0, 0, 0, 0, 0 },		 /* 0xF0 */
	{ 0, 0, 0, 0, 0 },		 /* 0xF1 */
	{ 0, 0, 0, 0, 0 },		 /* 0xF2 */
	{ 0, 0, 0, 0, 0 },		 /* 0xF3 */
	{ 0, 0, 0, 0, 0 },		 /* 0xF4 */
	{ 0, 0, 0, 0, 0 },		 /* 0xF5 */
	{ I_MGRP3, 1, 0, 0, 0 },	 /* 0xF6 */
	{ I_MGRP3, 2, 0, 0, 0 },	 /* 0xF7 */
	{ 0, 0, 0, 0, 0 },		 /* 0xF8 */
	{ 0, 0, 0, 0, 0 },		 /* 0xF9 */
	{ 0, 0, 0, 0, 0 },		 /* 0xFA */
	{ 0, 0, 0, 0, 0 },		 /* 0xFB */
	{ 0, 0, 0, 0, 0 },		 /* 0xFC */
	{ 0, 0, 0, 0, 0 },		 /* 0xFD */
	{ 0, 0, 0, 0, 0 },		 /* 0xFE */
	{ 0, 0, 0, 0, 0 },		 /* 0xFF */
};

static struct idata grp3[8] = {
	{ I_IMMED, 0, O_M, O_I, F_TEST },
	{ 0, 0, 0, 0, 0 },
	{ I_NOMOR, 0, O_M, 0, F_NOT },
	{ I_NOMOR, 0, O_M, 0, F_NEG },
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 0 },
};

#define READ_NEXT_B(op,code) do { \
	enum vmmerr err; \
	err = read_next_b (op, (u8 *)(code)); \
	if (err) \
		return err; \
} while (0)

#define READ_NEXT_W(op,code) do { \
	READ_NEXT_B (op, ((u8 *)(code)) + 0); \
	READ_NEXT_B (op, ((u8 *)(code)) + 1); \
} while (0)

#define READ_NEXT_L(op,code) do { \
	READ_NEXT_W (op, ((u8 *)(code)) + 0); \
	READ_NEXT_W (op, ((u8 *)(code)) + 2); \
} while (0)

#define GET_MODRM(op) do { \
	enum vmmerr err; \
	err = get_modrm (op); \
	if (err) \
		return err; \
} while (0)

#define READ_MODRM_B(op) RIE (read_modrm_b (op))

#define UPDATE_IP(op) current->vmctl.write_ip (op->ip + op->ip_off)

#define LGUESTSEG_READ_B(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (read_linearaddr_b (base + (offset), (data))); \
	} else { \
		GUESTSEG_READ_B (p, offset, data); \
	} \
} while (0)

#define LGUESTSEG_READ_W(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (read_linearaddr_w (base + (offset), (data))); \
	} else { \
		GUESTSEG_READ_W (p, offset, data); \
	} \
} while (0)

#define LGUESTSEG_READ_L(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (read_linearaddr_l (base + (offset), (data))); \
	} else { \
		GUESTSEG_READ_L (p, offset, data); \
	} \
} while (0)

#define LGUESTSEG_READ_Q(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (read_linearaddr_q (base + (offset), (data))); \
	} else { \
		GUESTSEG_READ_Q (p, offset, data); \
	} \
} while (0)

#define LGUESTSEG_WRITE_B(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (write_linearaddr_b (base + (offset), (data))); \
	} else { \
		GUESTSEG_WRITE_B (p, offset, data); \
	} \
} while (0)

#define LGUESTSEG_WRITE_W(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (write_linearaddr_w (base + (offset), (data))); \
	} else { \
		GUESTSEG_WRITE_W (p, offset, data); \
	} \
} while (0)

#define LGUESTSEG_WRITE_L(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (write_linearaddr_l (base + (offset), (data))); \
	} else { \
		GUESTSEG_WRITE_L (p, offset, data); \
	} \
} while (0)

#define LGUESTSEG_WRITE_Q(op,p,offset,data) do { \
	if (op->longmode) { \
		ulong base; \
		current->vmctl.read_sreg_base (p, &base); \
		RIE (write_linearaddr_q (base + (offset), (data))); \
	} else { \
		GUESTSEG_WRITE_Q (p, offset, data); \
	} \
} while (0)

static enum vmmerr idata_rd (struct op *op, enum idata_operand operand,
			     int length, void *pointer);
static enum vmmerr idata_wr (struct op *op, enum idata_operand operand,
			     int length, void *pointer);
static enum vmmerr read_modrm16 (struct op *op, u8 o, u16 *d16);
static enum vmmerr read_modrm32 (struct op *op, u8 o, u32 *d);

static enum vmmerr
read_next_b (struct op *op, u8 *data)
{
	if (op->ip_off >= 15)
		return VMMERR_INSTRUCTION_TOO_LONG;
	return cpu_seg_read_b (SREG_CS, op->ip + op->ip_off++,
			       data);
}

static ulong
get_reg (struct op *op, enum reg reg)
{
	ulong tmp;

	switch (reg) {
	case REG_AX:
		current->vmctl.read_general_reg (GENERAL_REG_RAX, &tmp);
		break;
	case REG_CX:
		current->vmctl.read_general_reg (GENERAL_REG_RCX, &tmp);
		break;
	case REG_DX:
		current->vmctl.read_general_reg (GENERAL_REG_RDX, &tmp);
		break;
	case REG_BX:
		current->vmctl.read_general_reg (GENERAL_REG_RBX, &tmp);
		break;
	case REG_SP:
		current->vmctl.read_general_reg (GENERAL_REG_RSP, &tmp);
		break;
	case REG_BP:
		current->vmctl.read_general_reg (GENERAL_REG_RBP, &tmp);
		break;
	case REG_SI:
		current->vmctl.read_general_reg (GENERAL_REG_RSI, &tmp);
		break;
	case REG_DI:
		current->vmctl.read_general_reg (GENERAL_REG_RDI, &tmp);
		break;
	case REG_08:
		current->vmctl.read_general_reg (GENERAL_REG_R8, &tmp);
		break;
	case REG_09:
		current->vmctl.read_general_reg (GENERAL_REG_R9, &tmp);
		break;
	case REG_10:
		current->vmctl.read_general_reg (GENERAL_REG_R10, &tmp);
		break;
	case REG_11:
		current->vmctl.read_general_reg (GENERAL_REG_R11, &tmp);
		break;
	case REG_12:
		current->vmctl.read_general_reg (GENERAL_REG_R12, &tmp);
		break;
	case REG_13:
		current->vmctl.read_general_reg (GENERAL_REG_R13, &tmp);
		break;
	case REG_14:
		current->vmctl.read_general_reg (GENERAL_REG_R14, &tmp);
		break;
	case REG_15:
		current->vmctl.read_general_reg (GENERAL_REG_R15, &tmp);
		break;
	case REG_NO:
		tmp = 0;
		break;
	}
	return tmp;
}

static void
set_reg (struct op *op, enum reg reg, ulong data)
{
	switch (reg) {
	case REG_AX:
		current->vmctl.write_general_reg (GENERAL_REG_RAX, data);
		break;
	case REG_CX:
		current->vmctl.write_general_reg (GENERAL_REG_RCX, data);
		break;
	case REG_DX:
		current->vmctl.write_general_reg (GENERAL_REG_RDX, data);
		break;
	case REG_BX:
		current->vmctl.write_general_reg (GENERAL_REG_RBX, data);
		break;
	case REG_SP:
		current->vmctl.write_general_reg (GENERAL_REG_RSP, data);
		break;
	case REG_BP:
		current->vmctl.write_general_reg (GENERAL_REG_RBP, data);
		break;
	case REG_SI:
		current->vmctl.write_general_reg (GENERAL_REG_RSI, data);
		break;
	case REG_DI:
		current->vmctl.write_general_reg (GENERAL_REG_RDI, data);
		break;
	case REG_08:
		current->vmctl.write_general_reg (GENERAL_REG_R8, data);
		break;
	case REG_09:
		current->vmctl.write_general_reg (GENERAL_REG_R9, data);
		break;
	case REG_10:
		current->vmctl.write_general_reg (GENERAL_REG_R10, data);
		break;
	case REG_11:
		current->vmctl.write_general_reg (GENERAL_REG_R11, data);
		break;
	case REG_12:
		current->vmctl.write_general_reg (GENERAL_REG_R12, data);
		break;
	case REG_13:
		current->vmctl.write_general_reg (GENERAL_REG_R13, data);
		break;
	case REG_14:
		current->vmctl.write_general_reg (GENERAL_REG_R14, data);
		break;
	case REG_15:
		current->vmctl.write_general_reg (GENERAL_REG_R15, data);
		break;
	case REG_NO:
		break;
	}
}

static enum vmmerr
read_modrm_b (struct op *op)
{
	READ_NEXT_B (op, &op->modrm);
	op->modrm_rreg = op->modrm.reg + (op->prefix.rex.b.r ? 8 : 0);
	op->modrm_brm = op->modrm.rm + (op->prefix.rex.b.b ? 8 : 0);
	return VMMERR_SUCCESS;
}

static enum vmmerr
get_modrm (struct op *op)
{
	struct modrm_info *m;
	struct sibbase_info *sb;
	struct sibscale_info *ss;
	int displen;
	enum sreg defseg;
	u64 addr;
	i8 tmp1;
	i16 tmp2;

	if (op->modrm.mod == 3)
		return VMMERR_SUCCESS;
	op->modrm_brm = REG_NO;
	if (op->addrtype == ADDRTYPE_16BIT)
		m = &modrmmatrix16[op->modrm.mod][op->modrm.rm];
	else
		m = &modrmmatrix32[op->modrm.mod][op->prefix.rex.b.b]
			[op->modrm.rm];
	displen = m->displen;
	defseg = m->defseg;
	if (m->sibflag) {
		READ_NEXT_B (op, &op->sib);
		sb = &sibmatrix_base[op->modrm.mod][op->prefix.rex.b.b]
			[op->sib.base];
		ss = &sib_scale[op->prefix.rex.b.x][op->sib.index];
		displen = sb->displen;
		defseg = sb->defseg;
		addr = (get_reg (op, ss->reg) << op->sib.scale)
			+ get_reg (op, sb->reg);
	} else {
		addr = get_reg (op, m->reg1) + get_reg (op, m->reg2);
	}
	switch (displen) {
	case 1:
		READ_NEXT_B (op, &tmp1);
		op->disp = tmp1;
		break;
	case 2:
		READ_NEXT_W (op, &tmp2);
		op->disp = tmp2;
		break;
	case 4:
		READ_NEXT_L (op, &op->disp);
		break;
	default:
		op->disp = 0;
	}
	addr += op->disp;
	op->modrm_addr = addr;
	op->modrm_ripflag = (op->longmode && m->ripflag);
	if (op->prefix.seg != SREG_DEFAULT)
		op->modrm_seg = op->prefix.seg;
	else
		op->modrm_seg = defseg;
	return VMMERR_SUCCESS;
}

static enum vmmerr
read_moffs (struct op *op)
{
	u32 moffs = 0;

	op->modrm_brm = REG_NO;
	if (op->addrtype == ADDRTYPE_16BIT)
		READ_NEXT_W (op, &moffs);
	else
		READ_NEXT_L (op, &moffs);
	op->modrm_addr = moffs;
	op->modrm_ripflag = false;
	if (op->prefix.seg != SREG_DEFAULT)
		op->modrm_seg = op->prefix.seg;
	else
		op->modrm_seg = SREG_DS;
	return VMMERR_SUCCESS;
}

static u64
modrm_off (struct op *op, u8 o)
{
	u64 addr;

	addr = op->modrm_addr + o;
	if (op->modrm_ripflag)
		addr += op->ip + op->ip_off;
	if (op->addrtype == ADDRTYPE_16BIT)
		addr &= 0xFFFF;
	else if (op->addrtype == ADDRTYPE_32BIT)
		addr &= 0xFFFFFFFF;
	return addr;
}

static void
clear_prefix (struct prefix *p)
{
	p->seg = SREG_DEFAULT;
	p->lock = 0;
	p->repne = 0;
	p->repe = 0;
	p->opsize = 0;
	p->addrsize = 0;
	p->rex.v.val = 0;
}

#define DEBUG_UNIMPLEMENTED() do { \
	panic ("DEBUG : file %s line %d function %s NOT IMPLEMENTED", \
	       __FILE__, __LINE__, __FUNCTION__); \
} while (0)

static enum vmmerr
opcode_hlt (struct op *op)
{
	UPDATE_IP (op);
	cpu_emul_hlt ();
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_cli (struct op *op)
{
	UPDATE_IP (op);
	cpu_emul_cli ();
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_sti (struct op *op)
{
	UPDATE_IP (op);
	cpu_emul_sti ();
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_pushf (struct op *op)
{
	ulong rflags;
	struct cpu_stack st;

	if (op->longmode)
		panic ("64bit PUSHF not supported");
	RIE (cpu_stack_get (&st));
	current->vmctl.read_flags (&rflags);
	rflags &= ~(RFLAGS_VM_BIT | RFLAGS_RF_BIT);
	RIE (cpu_stack_push (&st, &rflags, op->optype));
	RIE (cpu_stack_set (&st));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_popf (struct op *op)
{
	ulong rflags, orflags;
	struct cpu_stack st;

	if (op->longmode)
		panic ("64bit POPF not supported");
	RIE (cpu_stack_get (&st));
	current->vmctl.read_flags (&rflags);
	orflags = rflags;
	RIE (cpu_stack_pop (&st, &rflags, op->optype));
	rflags &= RFLAGS_SYS_MASK | RFLAGS_NONSYS_MASK;
	rflags |= RFLAGS_ALWAYS1_BIT;
	rflags &= ~(RFLAGS_RF_BIT | RFLAGS_VM_BIT |
		    RFLAGS_VIF_BIT | RFLAGS_VIP_BIT |
		    RFLAGS_IOPL_MASK);
	rflags |= orflags & (RFLAGS_RF_BIT | RFLAGS_VM_BIT |
			     RFLAGS_VIF_BIT | RFLAGS_VIP_BIT |
			     RFLAGS_IOPL_MASK);
	current->vmctl.write_flags (rflags);
	UPDATE_IP (op);
	RIE (cpu_stack_set (&st));
	if (rflags & RFLAGS_IF_BIT) {
		/* FIXME: interrupts must be delayed */
		cpu_emul_sti ();
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_iret (struct op *op)
{
	ulong rip, rflags, orflags;
	u16 cs;
	struct cpu_stack st;

	if (op->longmode)
		panic ("64bit IRET not supported");
	if (op->mode == CPUMODE_REAL) {
		RIE (cpu_stack_get (&st));
		current->vmctl.read_flags (&rflags);
		orflags = rflags;
		rip = 0;
		cs = 0;
		RIE (cpu_stack_pop (&st, &rip, op->optype));
		RIE (cpu_stack_pop (&st, &cs, op->optype));
		RIE (cpu_stack_pop (&st, &rflags, op->optype));
		cs &= 0xFFFF;
		rflags &= RFLAGS_SYS_MASK | RFLAGS_NONSYS_MASK;
		rflags |= RFLAGS_ALWAYS1_BIT;
		rflags &= ~(RFLAGS_RF_BIT | RFLAGS_VM_BIT |
			    RFLAGS_VIF_BIT | RFLAGS_VIP_BIT |
			    RFLAGS_IOPL_MASK);
		rflags |= orflags & (RFLAGS_RF_BIT | RFLAGS_VM_BIT |
				     RFLAGS_VIF_BIT | RFLAGS_VIP_BIT |
				     RFLAGS_IOPL_MASK);
		current->vmctl.write_flags (rflags);
		current->vmctl.write_realmode_seg (SREG_CS, cs);
		current->vmctl.write_ip (rip);
		RIE (cpu_stack_set (&st));
		if (rflags & RFLAGS_IF_BIT) {
			cpu_emul_sti ();
		}
		return VMMERR_SUCCESS;
	}
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_int (struct op *op)
{
	op->imm &= 0xFF;
	if (op->mode == CPUMODE_REAL) {
		UPDATE_IP (op);
		RET_IF_ERR (cpu_emul_realmode_int (op->imm));
		return VMMERR_SUCCESS;
	}
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_int3 (struct op *op)
{
	if (op->mode == CPUMODE_REAL) {
		UPDATE_IP (op);
		RET_IF_ERR (cpu_emul_realmode_int (3));
		return VMMERR_SUCCESS;
	}
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_into (struct op *op)
{
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static bool
execinst_io (void *execdata, void *data, u32 len)
{
	struct execinst_io_data *e;
	ulong rdx;

	e = (struct execinst_io_data *)execdata;
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &rdx);
	switch (call_io (e->type, rdx & 0xFFFF, data)) {
	case IOACT_CONT:
		return true;
	case IOACT_RERUN:
		return false;
	}
	panic ("execinst_io: bad ioact");
	return false;
}

static enum vmmerr
string_instruction (struct op *op, bool rd, bool wr, u32 len, void *execdata,
		    bool (*execinst) (void *execdata, void *data, u32 len))
{
	u64 data;
	enum sreg segr, segw;
	ulong offr, offw;
	ulong rflags;
	ulong cnt;
	ulong regr, regw, rcx;
	enum general_reg regrn, regwn;
	int loopcount = 16;	/* no interrupts are accepted in this loop */

	VAR_IS_INITIALIZED (segr);
	VAR_IS_INITIALIZED (offr);
	VAR_IS_INITIALIZED (offw);
	current->vmctl.read_flags (&rflags);
	if (wr) {
		segw = SREG_ES;
		regwn = GENERAL_REG_RDI;
		current->vmctl.read_general_reg (regwn, &regw);
	}
	if (rd) {
		if (op->prefix.seg == SREG_DEFAULT)
			segr = SREG_DS;
		else
			segr = op->prefix.seg;
		regrn = GENERAL_REG_RSI;
		current->vmctl.read_general_reg (regrn, &regr);
	}
reploop:
	if (op->prefix.repe || op->prefix.repne) {
		current->vmctl.read_general_reg (GENERAL_REG_RCX, &rcx);
		cnt = rcx;
		if (op->reptype == REPTYPE_16BIT)
			cnt &= 0xFFFF;
		else if (op->reptype == REPTYPE_32BIT)
			cnt &= 0xFFFFFFFF;
		if (!cnt)
			goto rcxz;
		cnt--;
		if (op->reptype == REPTYPE_16BIT)
			(*(u16 *)&rcx) = cnt;
		else if (op->reptype == REPTYPE_32BIT)
			(*(u32 *)&rcx) = cnt;
		else
			rcx = cnt;
		current->vmctl.write_general_reg (GENERAL_REG_RCX, rcx);
	} else {
		cnt = 0;
	}
	if (rd) {
		if (op->addrtype == ADDRTYPE_16BIT)
			offr = (*(u16 *)&regr);
		else if (op->addrtype == ADDRTYPE_32BIT)
			offr = (*(u32 *)&regr);
		else
			offr = regr;
		switch (len) {
		case 1:
			LGUESTSEG_READ_B (op, segr, offr, (u8 *)&data);
			break;
		case 2:
			LGUESTSEG_READ_W (op, segr, offr, (u16 *)&data);
			break;
		case 4:
			LGUESTSEG_READ_L (op, segr, offr, (u32 *)&data);
			break;
		case 8:
			LGUESTSEG_READ_Q (op, segr, offr, (u64 *)&data);
			break;
		}
	}
	if (!execinst (execdata, &data, len))
		return VMMERR_SUCCESS;
	if (wr) {
		if (op->addrtype == ADDRTYPE_16BIT)
			offw = (*(u16 *)&regw);
		else if (op->addrtype == ADDRTYPE_32BIT)
			offw = (*(u32 *)&regw);
		else
			offw = regw;
		switch (len) {
		case 1:
			LGUESTSEG_WRITE_B (op, segw, offw, data);
			break;
		case 2:
			LGUESTSEG_WRITE_W (op, segw, offw, data);
			break;
		case 4:
			LGUESTSEG_WRITE_L (op, segw, offw, data);
			break;
		case 8:
			LGUESTSEG_WRITE_Q (op, segw, offw, data);
			break;
		}
	}
	if (rflags & RFLAGS_DF_BIT) {
		if (rd)
			offr -= len;
		if (wr)
			offw -= len;
	} else {
		if (rd)
			offr += len;
		if (wr)
			offw += len;
	}
	if (op->addrtype == ADDRTYPE_16BIT) {
		if (rd)
			(*(u16 *)&regr) = offr;
		if (wr)
			(*(u16 *)&regw) = offw;
	} else if (op->addrtype == ADDRTYPE_32BIT) {
		if (rd)
			(*(u32 *)&regr) = offr;
		if (wr)
			(*(u32 *)&regw) = offw;
	} else {
		if (rd)
			regr = offr;
		if (wr)
			regw = offw;
	}
	if (cnt && loopcount) {
		if (rflags & RFLAGS_IF_BIT)
			loopcount--;
		goto reploop;
	}
	if (rd)
		current->vmctl.write_general_reg (regrn, regr);
	if (wr)
		current->vmctl.write_general_reg (regwn, regw);
rcxz:
	if (!cnt)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
io_str (struct op *op, enum iotype type, u32 len)
{
	struct execinst_io_data d;
	bool wr;

	d.type = type;
	switch (type) {
	case IOTYPE_INB:
	case IOTYPE_INW:
	case IOTYPE_INL:
		wr = true;
		break;
	case IOTYPE_OUTB:
	case IOTYPE_OUTW:
	case IOTYPE_OUTL:
		wr = false;
		break;
	default:
		return VMMERR_AVOID_COMPILER_WARNING;
	}
	return string_instruction (op, !wr, wr, len, &d, execinst_io);
}

static enum vmmerr
opcode_inb_imm (struct op *op)
{
	ulong rax;
	enum ioact ioret;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->updateip = false;
	ioret = call_io (IOTYPE_INB, op->imm & 0xFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	current->vmctl.write_general_reg (GENERAL_REG_RAX, rax);
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_in_imm (struct op *op)
{
	ulong rax;
	enum ioact ioret;

	if (op->optype == OPTYPE_64BIT)
		op->optype = OPTYPE_32BIT;
	if (op->optype == OPTYPE_32BIT)
		rax = 0;
	else
		current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->updateip = false;
	if (op->optype == OPTYPE_16BIT)
		ioret = call_io (IOTYPE_INW, op->imm & 0xFF, &rax);
	else
		ioret = call_io (IOTYPE_INL, op->imm & 0xFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	current->vmctl.write_general_reg (GENERAL_REG_RAX, rax);
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_inb_dx (struct op *op)
{
	ulong rax, rdx;
	enum ioact ioret;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &rdx);
	current->updateip = false;
	ioret = call_io (IOTYPE_INB, rdx & 0xFFFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	current->vmctl.write_general_reg (GENERAL_REG_RAX, rax);
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_in_dx (struct op *op)
{
	ulong rax, rdx;
	enum ioact ioret;

	current->vmctl.read_general_reg (GENERAL_REG_RDX, &rdx);
	if (op->optype == OPTYPE_64BIT)
		op->optype = OPTYPE_32BIT;
	if (op->optype == OPTYPE_32BIT)
		rax = 0;
	else
		current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->updateip = false;
	if (op->optype == OPTYPE_16BIT)
		ioret = call_io (IOTYPE_INW, rdx & 0xFFFF, &rax);
	else
		ioret = call_io (IOTYPE_INL, rdx & 0xFFFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	current->vmctl.write_general_reg (GENERAL_REG_RAX, rax);
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_insb (struct op *op)
{
	return io_str (op, IOTYPE_INB, 1);
}

static enum vmmerr
opcode_ins (struct op *op)
{
	if (op->optype == OPTYPE_64BIT)
		op->optype = OPTYPE_32BIT;
	if (op->optype == OPTYPE_16BIT)
		return io_str (op, IOTYPE_INW, 2);
	else 
		return io_str (op, IOTYPE_INL, 4);
}

static enum vmmerr
opcode_outb_imm (struct op *op)
{
	ulong rax;
	enum ioact ioret;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->updateip = false;
	ioret = call_io (IOTYPE_OUTB, op->imm & 0xFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_out_imm (struct op *op)
{
	ulong rax;
	enum ioact ioret;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	if (op->optype == OPTYPE_64BIT)
		op->optype = OPTYPE_32BIT;
	current->updateip = false;
	if (op->optype == OPTYPE_16BIT)
		ioret = call_io (IOTYPE_OUTW, op->imm & 0xFF, &rax);
	else
		ioret = call_io (IOTYPE_OUTL, op->imm & 0xFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_outb_dx (struct op *op)
{
	ulong rax, rdx;
	enum ioact ioret;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &rdx);
	current->updateip = false;
	ioret = call_io (IOTYPE_OUTB, rdx & 0xFFFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_out_dx (struct op *op)
{
	ulong rax, rdx;
	enum ioact ioret;

	current->vmctl.read_general_reg (GENERAL_REG_RAX, &rax);
	current->vmctl.read_general_reg (GENERAL_REG_RDX, &rdx);
	if (op->optype == OPTYPE_64BIT)
		op->optype = OPTYPE_32BIT;
	current->updateip = false;
	if (op->optype == OPTYPE_16BIT)
		ioret = call_io (IOTYPE_OUTW, rdx & 0xFFFF, &rax);
	else
		ioret = call_io (IOTYPE_OUTL, rdx & 0xFFFF, &rax);
	switch (ioret) {
	case IOACT_CONT:
		break;
	case IOACT_RERUN:
		return VMMERR_SUCCESS;
	}
	if (!current->updateip)
		UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_outsb (struct op *op)
{
	return io_str (op, IOTYPE_OUTB, 1);
}

static enum vmmerr
opcode_outs (struct op *op)
{
	if (op->optype == OPTYPE_64BIT)
		op->optype = OPTYPE_32BIT;
	if (op->optype == OPTYPE_16BIT)
		return io_str (op, IOTYPE_OUTW, 2);
	else
		return io_str (op, IOTYPE_OUTL, 4);
}

static enum vmmerr
opcode_mov_to_cr (struct op *op)
{
	ulong cr;
	enum control_reg n;

	/* Ignore op->modrm.mod */
	cr = get_reg (op, op->modrm_brm);
	if (!op->longmode)
		cr &= 0xFFFFFFFF;
	n = op->modrm_rreg;
	current->vmctl.write_control_reg (n, cr);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_mov_from_cr (struct op *op)
{
	ulong cr;
	enum control_reg n;

	/* Ignore op->modrm.mod */
	n = op->modrm_rreg;
	current->vmctl.read_control_reg (n, &cr);
	if (!op->longmode)
		cr &= 0xFFFFFFFF;
	set_reg (op, op->modrm_brm, cr);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_mov_to_dr (struct op *op)
{
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_mov_from_dr (struct op *op)
{
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_clts (struct op *op)
{
	ulong val;

	current->vmctl.read_control_reg (CONTROL_REG_CR0, &val);
	val &= ~CR0_TS_BIT;
	current->vmctl.write_control_reg (CONTROL_REG_CR0, val);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_invd (struct op *op)
{
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_wbinvd (struct op *op)
{
	asm_wbinvd ();
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_rdmsr (struct op *op)
{
	if (cpu_emul_rdmsr ())
		return VMMERR_MSR_FAULT;
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_wrmsr (struct op *op)
{
	if (cpu_emul_wrmsr ())
		return VMMERR_MSR_FAULT;
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_lgdt (struct op *op)
{
	u16 limit;
	u32 base;

	if (op->modrm_brm != REG_NO)
		return VMMERR_EXCEPTION_UD;
	if (op->longmode)
		panic ("64bit LGDT not supported");
	RIE (read_modrm16 (op, 0, &limit));
	RIE (read_modrm32 (op, 2, &base));
	if (op->optype == OPTYPE_16BIT)
		base &= 0x00FFFFFF;
	current->vmctl.write_gdtr (base, limit);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_lidt (struct op *op)
{
	u16 limit;
	u32 base;

	if (op->modrm_brm != REG_NO)
		return VMMERR_EXCEPTION_UD;
	if (op->longmode)
		panic ("64bit LIDT not supported");
	RIE (read_modrm16 (op, 0, &limit));
	RIE (read_modrm32 (op, 2, &base));
	if (op->optype == OPTYPE_16BIT)
		base &= 0x00FFFFFF;
	current->vmctl.write_idtr (base, limit);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_smsw (struct op *op)
{
	ulong cr;

	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr);
	RIE (idata_wr (op, O_M, 2, &cr));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_lmsw (struct op *op)
{
	ulong data;
	ulong cr;

	op->optype = OPTYPE_16BIT;
	RIE (idata_rd (op, O_M, 2, &data));
	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr);
	cr &= ~0xFUL;
	cr |= data & 0xFUL;
	current->vmctl.write_control_reg (CONTROL_REG_CR0, cr);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_invlpg (struct op *op)
{
	ulong base;

	current->vmctl.read_sreg_base (op->modrm_seg, &base);
	current->vmctl.invlpg (base + modrm_off (op, 0));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static void
set_reg16 (struct op *op, enum reg reg, u16 d16)
{
	ulong dlong;

	dlong = get_reg (op, reg);
	*(u16 *)&dlong = d16;
	set_reg (op, reg, dlong);
}

static enum vmmerr
opcode_mov_from_sr (struct op *op)
{
	u16 sel;

	current->vmctl.read_sreg_sel ((enum sreg)op->modrm.reg, &sel);
	if (op->modrm_brm == REG_NO)
		op->optype = OPTYPE_16BIT;
	RIE (idata_wr (op, O_M, 2, &sel));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_mov_to_sr (struct op *op)
{
	enum vmmerr err;

	/* FIXME: interrupts must be delayed after writing SS register */
	err = current->vmctl.writing_sreg ((enum sreg)op->modrm.reg);
	if (err == VMMERR_SW)
		return err;
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_push_sr (struct op *op, enum sreg seg)
{
	u32 sel32;
	u16 sel16;
	struct cpu_stack st;

	if (op->longmode)
		panic ("64bit PUSH SREG not supported");
	current->vmctl.read_sreg_sel (seg, &sel16);
	sel32 = sel16;
	RIE (cpu_stack_get (&st));
	RIE (cpu_stack_push (&st, &sel32, op->optype));
	RIE (cpu_stack_set (&st));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_pop_sr (struct op *op, enum sreg seg)
{
	enum vmmerr err;

	/* FIXME: interrupts must be delayed after writing SS register */
	err = current->vmctl.writing_sreg (seg);
	if (err == VMMERR_SW)
		return err;
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_jmp_far_direct (struct op *op)
{
	enum vmmerr err;

	err = current->vmctl.writing_sreg (SREG_CS);
	if (err == VMMERR_SW)
		return err;
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_jmp_far_indirect (struct op *op)
{
	enum vmmerr err;

	err = current->vmctl.writing_sreg (SREG_CS);
	if (err == VMMERR_SW)
		return err;
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static enum vmmerr
opcode_retf (struct op *op)
{
	enum vmmerr err;

	err = current->vmctl.writing_sreg (SREG_CS);
	if (err == VMMERR_SW)
		return err;
	DEBUG_UNIMPLEMENTED ();
	return VMMERR_UNIMPLEMENTED_OPCODE;
}

static bool
execinst_stos (void *execdata, void *data, u32 len)
{
	current->vmctl.read_general_reg (GENERAL_REG_RAX, data);
	return true;
}

static bool
execinst_movs (void *execdata, void *data, u32 len)
{
	return true;
}

static enum vmmerr
opcode_stosb (struct op *op)
{
	return string_instruction (op, false, true, 1, NULL, execinst_stos);
}

static enum vmmerr
opcode_stos (struct op *op)
{
	u32 len;

	if (op->optype == OPTYPE_16BIT)
		len = 2;
	else if (op->optype == OPTYPE_32BIT)
		len = 4;
	else
		len = 8;
	return string_instruction (op, false, true, len, NULL, execinst_stos);
}

static enum vmmerr
opcode_movsb (struct op *op)
{
	return string_instruction (op, true, true, 1, NULL, execinst_movs);
}

static enum vmmerr
opcode_movs (struct op *op)
{
	u32 len;

	if (op->optype == OPTYPE_16BIT)
		len = 2;
	else if (op->optype == OPTYPE_32BIT)
		len = 4;
	else
		len = 8;
	return string_instruction (op, true, true, len, NULL, execinst_movs);
}

static enum general_reg
reg8_general_reg (enum reg8 r8)
{
	switch (r8) {
	case REG_AL:
	case REG_AH:
		return GENERAL_REG_RAX;
	case REG_CL:
	case REG_CH:
		return GENERAL_REG_RCX;
	case REG_DL:
	case REG_DH:
		return GENERAL_REG_RDX;
	case REG_BL:
	case REG_BH:
		return GENERAL_REG_RBX;
	default:
		break;
	}
	return 0;
}

static u8 *
reg8_dp (enum reg8 r8, ulong *data)
{
	switch (r8) {
	case REG_AL:
	case REG_CL:
	case REG_DL:
	case REG_BL:
		return &((u8 *)data)[0];
	case REG_AH:
	case REG_CH:
	case REG_DH:
	case REG_BH:
		return &((u8 *)data)[1];
	default:
		break;
	}
	return NULL;
}

static u8
get_reg8 (struct op *op, bool rex, enum reg8 r8)
{
	ulong dlong;
	enum general_reg reg;
	u8 *dp;

	if (rex) {
		reg = (enum general_reg)r8;
		dp = (u8 *)&dlong;
	} else {
		reg = reg8_general_reg (r8);
		dp = reg8_dp (r8, &dlong);
	}
	current->vmctl.read_general_reg (reg, &dlong);
	return *dp;
}

static void
set_reg8 (struct op *op, bool rex, enum reg8 r8, u8 d8)
{
	ulong dlong;
	enum general_reg reg;
	u8 *dp;

	if (rex) {
		reg = (enum general_reg)r8;
		dp = (u8 *)&dlong;
	} else {
		reg = reg8_general_reg (r8);
		dp = reg8_dp (r8, &dlong);
	}
	current->vmctl.read_general_reg (reg, &dlong);
	*dp = d8;
	current->vmctl.write_general_reg (reg, dlong);
}

static enum vmmerr
read_modrm8 (struct op *op, u8 o, u8 *d8)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_READ_B (op, op->modrm_seg, modrm_off (op, o), d8);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		*d8 = get_reg8 (op, !!op->prefix.rex.v.val, op->modrm_brm);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
write_modrm8 (struct op *op, u8 o, u8 d8)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_WRITE_B (op, op->modrm_seg, modrm_off (op, o), d8);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		set_reg8 (op, !!op->prefix.rex.v.val, op->modrm_brm, d8);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
read_modrm16 (struct op *op, u8 o, u16 *d16)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_READ_W (op, op->modrm_seg, modrm_off (op, o), d16);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		*d16 = get_reg (op, op->modrm_brm);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
read_modrm32 (struct op *op, u8 o, u32 *d)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_READ_L (op, op->modrm_seg, modrm_off (op, o), d);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		*d = get_reg (op, op->modrm_brm);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
read_modrm64 (struct op *op, u8 o, u64 *d)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_READ_Q (op, op->modrm_seg, modrm_off (op, o), d);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		*d = get_reg (op, op->modrm_brm);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
write_modrm16 (struct op *op, u8 o, u16 d16)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_WRITE_W (op, op->modrm_seg, modrm_off (op, o), d16);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		set_reg16 (op, op->modrm_brm, d16);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
write_modrm32 (struct op *op, u8 o, u32 d)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_WRITE_L (op, op->modrm_seg, modrm_off (op, o), d);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		set_reg (op, op->modrm_brm, d);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
write_modrm64 (struct op *op, u8 o, u64 d)
{
	if (op->modrm_brm == REG_NO) {
		LGUESTSEG_WRITE_Q (op, op->modrm_seg, modrm_off (op, o), d);
	} else {
		if (o)
			panic ("Offset must be zero for register access");
		set_reg (op, op->modrm_brm, d);
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_movzx_rm8_to_r (struct op *op)
{
	u8 d8;
	ulong d;

	RIE (read_modrm8 (op, 0, &d8));
	d = d8;
	RIE (idata_wr (op, O_R, 2, &d));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_movzx_rm16_to_r (struct op *op)
{
	u16 d16;
	ulong d;

	RIE (read_modrm16 (op, 0, &d16));
	d = d16;
	RIE (idata_wr (op, O_R, 2, &d));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_ff_push (struct op *op)
{
	u32 data;
	u16 data16;
	struct cpu_stack st;

	if (op->longmode)
		panic ("64bit PUSH not supported");
	if (op->optype == OPTYPE_16BIT) {
		RIE (read_modrm16 (op, 0, &data16));
		data = data16;
	} else {
		RIE (read_modrm32 (op, 0, &data));
	}
	RIE (cpu_stack_get (&st));
	RIE (cpu_stack_push (&st, &data, op->optype));
	RIE (cpu_stack_set (&st));
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_8f_pop (struct op *op)
{
	u32 data;
	u16 data16;
	struct cpu_stack st;

	if (op->longmode)
		panic ("64bit POP not supported");
	RIE (cpu_stack_get (&st));
	RIE (cpu_stack_pop (&st, &data, op->optype));
	if (op->optype == OPTYPE_16BIT) {
		data16 = data;
		RIE (write_modrm16 (op, 0, data16));
	} else {
		RIE (write_modrm32 (op, 0, data));
	}
	UPDATE_IP (op);
	RIE (cpu_stack_set (&st));
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_0f_lss (struct op *op)
{
	u32 data;
	u16 data16;
	u16 seg;

	if (op->longmode)
		panic ("64bit LSS(0F) not supported");
	if (op->modrm_brm != REG_NO)
		panic ("LSS(0F) reg");
	if (op->mode != CPUMODE_REAL)
		panic ("LSS(0F) protected mode");
	if (op->optype == OPTYPE_16BIT) {
		RIE (read_modrm16 (op, 0, &data16));
		RIE (read_modrm16 (op, 2, &seg));
		current->vmctl.write_realmode_seg (SREG_SS, seg);
		set_reg16 (op, op->modrm_rreg, data16);
	} else {
		RIE (read_modrm32 (op, 0, &data));
		RIE (read_modrm16 (op, 4, &seg));
		current->vmctl.write_realmode_seg (SREG_SS, seg);
		set_reg (op, op->modrm_rreg, data);
	}
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_bt_imm (struct op *op)
{
	ulong data, mask, rflags;

	RIE (idata_rd (op, O_M, 2, &data));
	if (op->optype == OPTYPE_16BIT)
		mask = 1 << (op->imm & 0xF);
	else if (op->optype == OPTYPE_32BIT)
		mask = 1 << (op->imm & 0x1F);
	else
		mask = 1 << (op->imm & 0x3F);
	current->vmctl.read_flags (&rflags);
	if (data & mask)
		rflags |= RFLAGS_CF_BIT;
	else
		rflags &= ~RFLAGS_CF_BIT;
	current->vmctl.write_flags (rflags);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

static enum vmmerr
idata_rd (struct op *op, enum idata_operand operand, int length, void *pointer)
{
	switch (operand) {
	case O_ZERO:
	default:
		panic ("idata_rd operand %d error!", operand);
	case O_M:
		if (length == 1)
			RIE (read_modrm8 (op, 0, (u8 *)pointer));
		else if (op->optype == OPTYPE_16BIT)
			RIE (read_modrm16 (op, 0, (u16 *)pointer));
		else if (op->optype == OPTYPE_32BIT)
			RIE (read_modrm32 (op, 0, (u32 *)pointer));
		else
			RIE (read_modrm64 (op, 0, (u64 *)pointer));
		break;
	case O_R:
		if (length == 1)
			*(u8 *)pointer = get_reg8 (op, !!op->prefix.rex.v.val,
						   (enum reg8)op->modrm_rreg);
		else if (op->optype == OPTYPE_16BIT)
			*(u16 *)pointer = get_reg (op, op->modrm_rreg);
		else if (op->optype == OPTYPE_32BIT)
			*(u32 *)pointer = get_reg (op, op->modrm_rreg);
		else
			*(u64 *)pointer = get_reg (op, op->modrm_rreg);
		break;
	case O_A:
		if (length == 1)
			*(u8 *)pointer = get_reg8 (op, false, REG_AL);
		else if (op->optype == OPTYPE_16BIT)
			*(u16 *)pointer = get_reg (op, REG_AX);
		else if (op->optype == OPTYPE_32BIT)
			*(u32 *)pointer = get_reg (op, REG_AX);
		else
			*(u64 *)pointer = get_reg (op, REG_AX);
		break;
	case O_I:
		if (length == 1)
			*(u8 *)pointer = op->imm;
		else if (op->optype == OPTYPE_16BIT)
			*(u16 *)pointer = op->imm;
		else if (op->optype == OPTYPE_32BIT)
			*(u32 *)pointer = op->imm;
		else
			*(u64 *)pointer = op->imm;
		break;
	case O_1:
		if (length == 1)
			*(u8 *)pointer = 1;
		else if (op->optype == OPTYPE_16BIT)
			*(u16 *)pointer = 1;
		else if (op->optype == OPTYPE_32BIT)
			*(u32 *)pointer = 1;
		else
			*(u64 *)pointer = 1;
		break;
	case O_C:
		if (length == 1)
			*(u8 *)pointer = get_reg8 (op, false, REG_CL);
		else
			panic ("read CX/RCX?");
		break;
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
idata_wr (struct op *op, enum idata_operand operand, int length, void *pointer)
{
	switch (operand) {
	case O_ZERO:
	case O_I:
	case O_1:
	case O_C:
	default:
		panic ("idata_wr operand %d error!", operand);
	case O_M:
		if (length == 1)
			RIE (write_modrm8 (op, 0, *(u8 *)pointer));
		else if (op->optype == OPTYPE_16BIT)
			RIE (write_modrm16 (op, 0, *(u16 *)pointer));
		else if (op->optype == OPTYPE_32BIT)
			RIE (write_modrm32 (op, 0, *(u32 *)pointer));
		else
			RIE (write_modrm64 (op, 0, *(u64 *)pointer));
		break;
	case O_R:
		if (length == 1)
			set_reg8 (op, !!op->prefix.rex.v.val,
				  (enum reg8)op->modrm_rreg, *(u8 *)pointer);
		else if (op->optype == OPTYPE_16BIT)
			set_reg16 (op, op->modrm_rreg, *(u16 *)pointer);
		else if (op->optype == OPTYPE_32BIT)
			set_reg (op, op->modrm_rreg, *(u32 *)pointer);
		else
			set_reg (op, op->modrm_rreg, *(u64 *)pointer);
		break;
	case O_A:
		if (length == 1)
			set_reg8 (op, false, REG_AL, *(u8 *)pointer);
		else if (op->optype == OPTYPE_16BIT)
			set_reg16 (op, REG_AX, *(u16 *)pointer);
		else if (op->optype == OPTYPE_32BIT)
			set_reg (op, REG_AX, *(u32 *)pointer);
		else
			set_reg (op, REG_AX, *(u64 *)pointer);
		break;
	}
	return VMMERR_SUCCESS;
}

static enum vmmerr
opcode_idata (struct op *op, struct idata id)
{
	enum idata_function grp1[8] = {
		F_ADD,
		F_OR,
		F_ADC,
		F_SBB,
		F_AND,
		F_SUB,
		F_XOR,
		F_CMP,
	};
	enum idata_function grp2[8] = {
		F_ROL,
		F_ROR,
		F_RCL,
		F_RCR,
		F_SHL,
		F_SHR,
		F_SHL,
		F_SAR,
	};
	ulong src, dst;
	static const ulong flagmask = RFLAGS_CF_BIT | RFLAGS_PF_BIT |
		RFLAGS_AF_BIT | RFLAGS_ZF_BIT | RFLAGS_SF_BIT |
		RFLAGS_DF_BIT | RFLAGS_OF_BIT;
	ulong flags, newflags = ~(0UL), tmp;
	bool update_flags = false;

#define DO_IDATA(op, rd1, rd2, doit, wr1, wr2) do { \
	ID_RD_##rd1 (op); \
	ID_RD_##rd2 (op); \
	ID_##doit; \
	ID_WR_##wr1 (op); \
	ID_WR_##wr2 (op); \
} while (0)
#define ID_RD_src(op) RIE (idata_rd (op, id.src, id.len, &src))
#define ID_RD_dst(op) RIE (idata_rd (op, id.dst, id.len, &dst))
#define ID_WR_src(op) RIE (idata_wr (op, id.src, id.len, &src))
#define ID_WR_dst(op) RIE (idata_wr (op, id.dst, id.len, &dst))
#define ID_RD_(op) do; while (0)
#define ID_WR_(op) do; while (0)
#ifdef __x86_64__
#	define ID_2OP_2(INSTRUCTION, src, dst) do { \
		current->vmctl.read_flags (&flags); \
		asm ("pushfq \n" \
		     "andq %2, (%%rsp) \n" \
		     "orq  %3, (%%rsp) \n" \
		     "popfq \n" \
		     INSTRUCTION " %4, %1 \n" \
		     "pushfq \n" \
		     "popq %0 \n" \
		     : "=&rm" (newflags) \
		     , "=&rm" (dst) \
		     : "i" (~flagmask) \
		     , "r" (flags & flagmask) \
		     , "r" (src) \
		     , "1" (dst) \
		     : "cc"); \
		newflags = (flags & ~flagmask) | (newflags & flagmask); \
		update_flags = true; \
	} while (0)
#else
#	define ID_2OP_2(INSTRUCTION, src, dst) do { \
		current->vmctl.read_flags (&flags); \
		asm ("pushfl \n" \
		     "andl %2, (%%esp) \n" \
		     "orl  %3, (%%esp) \n" \
		     "popfl \n" \
		     INSTRUCTION " %4, %1 \n" \
		     "pushfl \n" \
		     "popl %0 \n" \
		     : "=&rm" (newflags) \
		     , "=&rm" (dst) \
		     : "i" (~flagmask) \
		     , "r" (flags & flagmask) \
		     , "r" (src) \
		     , "1" (dst) \
		     : "cc"); \
		newflags = (flags & ~flagmask) | (newflags & flagmask); \
		update_flags = true; \
	} while (0)
#endif
#define ID_2OP(INSTRUCTION) do { \
	if (id.len == 1) \
		ID_2OP_2 (INSTRUCTION "b", *(u8 *)&src, *(u8 *)&dst); \
	else if (op->optype == OPTYPE_16BIT) \
		ID_2OP_2 (INSTRUCTION "w", *(u16 *)&src, *(u16 *)&dst); \
	else if (op->optype == OPTYPE_32BIT) \
		ID_2OP_2 (INSTRUCTION "l", *(u32 *)&src, *(u32 *)&dst); \
	else \
		ID_2OP_2 (INSTRUCTION, src, dst); \
} while (0)
#define ID_XCHG() do { \
	tmp = dst; \
	dst = src; \
	src = tmp; \
} while (0)
#define ID_MOV() dst = src

grp_special:
	switch (id.func) {
	case F_ZERO:
	case F_NOT:
	case F_NEG:
	case F_ROL:
	case F_ROR:
	case F_RCL:
	case F_RCR:
	case F_SHL:
	case F_SHR:
	case F_SAR:
	default:
		return VMMERR_UNSUPPORTED_OPCODE;
	case F_GRP1:
		id.func = grp1[op->modrm.reg];
		goto grp_special;
	case F_GRP2:
		id.func = grp2[op->modrm.reg];
		goto grp_special;
	case F_GRP11:
		if (op->modrm.reg == 0)
			id.func = F_MOV;
		else
			id.func = F_ZERO;
		goto grp_special;
	case F_ADD:  DO_IDATA (op, src, dst, 2OP ("add"),      , dst); break;
	case F_OR:   DO_IDATA (op, src, dst, 2OP ("or"),       , dst); break;
	case F_ADC:  DO_IDATA (op, src, dst, 2OP ("adc"),      , dst); break;
	case F_SBB:  DO_IDATA (op, src, dst, 2OP ("sbb"),      , dst); break;
	case F_AND:  DO_IDATA (op, src, dst, 2OP ("and"),      , dst); break;
	case F_SUB:  DO_IDATA (op, src, dst, 2OP ("sub"),      , dst); break;
	case F_XOR:  DO_IDATA (op, src, dst, 2OP ("xor"),      , dst); break;
	case F_CMP:  DO_IDATA (op, src, dst, 2OP ("cmp"),      ,    ); break;
	case F_TEST: DO_IDATA (op, src, dst, 2OP ("test"),     ,    ); break;
	case F_XCHG: DO_IDATA (op, src, dst, XCHG ()      ,src , dst); break;
	case F_MOV:  DO_IDATA (op, src,    , MOV ()       ,    , dst); break;
	}
	if (update_flags)
		current->vmctl.write_flags (newflags);
	UPDATE_IP (op);
	return VMMERR_SUCCESS;
}

enum vmmerr
cpu_interpreter (void)
{
	struct op *op, op1;
	u8 code;
	ulong acr;
	struct idata idat;
	ulong cr0;
	u64 efer;

	op = &op1;
	current->vmctl.read_control_reg (CONTROL_REG_CR0, &cr0);
	current->vmctl.read_msr (MSR_IA32_EFER, &efer);
	if (cr0 & CR0_PE_BIT)
		op->mode = CPUMODE_PROTECTED;
	else
		op->mode = CPUMODE_REAL;
	op->longmode = false;
	current->vmctl.read_ip (&op->ip);
	op->ip_off = 0;
	READ_NEXT_B (op, &code);
	clear_prefix (&op->prefix);
	for (;;) {
		switch (code) {
		case PREFIX_LOCK:
			op->prefix.lock = 1;
			panic ("LOCK");
			break;
		case PREFIX_REPNE:
			op->prefix.repne = 1;
			break;
		case PREFIX_REPE:
			op->prefix.repe = 1;
			break;
		case PREFIX_ES:
			op->prefix.seg = SREG_ES;
			break;
		case PREFIX_CS:
			op->prefix.seg = SREG_CS;
			break;
		case PREFIX_SS:
			op->prefix.seg = SREG_SS;
			break;
		case PREFIX_DS:
			op->prefix.seg = SREG_DS;
			break;
		case PREFIX_FS:
			op->prefix.seg = SREG_FS;
			break;
		case PREFIX_GS:
			op->prefix.seg = SREG_GS;
			break;
		case PREFIX_OPERAND_SIZE:
			op->prefix.opsize = 1;
			break;
		case PREFIX_ADDRESS_SIZE:
			op->prefix.addrsize = 1;
			break;
		default:
			goto parse_opcode;
		}
		READ_NEXT_B (op, &code);
	}
parse_opcode:
	current->vmctl.read_sreg_acr (SREG_CS, &acr);
	if ((efer & MSR_IA32_EFER_LMA_BIT) && (acr & ACCESS_RIGHTS_L_BIT)) {
		op->longmode = true;
		if (code >= PREFIX_REX_MIN && code <= PREFIX_REX_MAX) {
			op->prefix.rex.v.val = code;
			READ_NEXT_B (op, &code);
		}
		op->reptype = REPTYPE_64BIT;
		op->optype = op->prefix.rex.b.w ? OPTYPE_64BIT :
			!op->prefix.opsize ? OPTYPE_32BIT : OPTYPE_16BIT;
		op->addrtype =
			!op->prefix.addrsize ? ADDRTYPE_64BIT : ADDRTYPE_32BIT;
	} else if (acr & ACCESS_RIGHTS_D_B_BIT) {
		op->reptype = REPTYPE_32BIT;
		op->optype =
			!op->prefix.opsize ? OPTYPE_32BIT : OPTYPE_16BIT;
		op->addrtype =
			!op->prefix.addrsize ? ADDRTYPE_32BIT : ADDRTYPE_16BIT;
	} else {
		op->reptype = REPTYPE_16BIT;
		op->optype =
			!op->prefix.opsize ? OPTYPE_16BIT : OPTYPE_32BIT;
		op->addrtype =
			!op->prefix.addrsize ? ADDRTYPE_16BIT : ADDRTYPE_32BIT;
	}
	switch (code) {
	case OPCODE_0x0F:
		READ_NEXT_B (op, &code);
		goto parse_opcode_0x0f;
	case OPCODE_HLT:
		return opcode_hlt (op);
	case OPCODE_CLI:
		return opcode_cli (op);
	case OPCODE_STI:
		return opcode_sti (op);
	case OPCODE_PUSHF:
		return opcode_pushf (op);
	case OPCODE_POPF:
		return opcode_popf (op);
	case OPCODE_IRET:
		return opcode_iret (op);
	case OPCODE_INT:
		READ_NEXT_B (op, &op->imm);
		return opcode_int (op);
	case OPCODE_INT3:
		return opcode_int3 (op);
	case OPCODE_INTO:
		return opcode_into (op);
	case OPCODE_INB_IMM:
		READ_NEXT_B (op, &op->imm);
		return opcode_inb_imm (op);
	case OPCODE_IN_IMM:
		READ_NEXT_B (op, &op->imm);
		return opcode_in_imm (op);
	case OPCODE_INB_DX:
		return opcode_inb_dx (op);
	case OPCODE_IN_DX:
		return opcode_in_dx (op);
	case OPCODE_INSB:
		return opcode_insb (op);
	case OPCODE_INS:
		return opcode_ins (op);
	case OPCODE_OUTB_IMM:
		READ_NEXT_B (op, &op->imm);
		return opcode_outb_imm (op);
	case OPCODE_OUT_IMM:
		READ_NEXT_B (op, &op->imm);
		return opcode_out_imm (op);
	case OPCODE_OUTB_DX:
		return opcode_outb_dx (op);
	case OPCODE_OUT_DX:
		return opcode_out_dx (op);
	case OPCODE_OUTSB:
		return opcode_outsb (op);
	case OPCODE_OUTS:
		return opcode_outs (op);
	case OPCODE_MOV_FROM_SR:
		READ_MODRM_B (op);
		GET_MODRM (op);
		return opcode_mov_from_sr (op);
	case OPCODE_MOV_TO_SR:
		READ_MODRM_B (op);
		GET_MODRM (op);
		return opcode_mov_to_sr (op);
	case OPCODE_PUSH_ES:
		return opcode_push_sr (op, SREG_ES);
	case OPCODE_PUSH_CS:
		return opcode_push_sr (op, SREG_CS);
	case OPCODE_PUSH_SS:
		return opcode_push_sr (op, SREG_SS);
	case OPCODE_PUSH_DS:
		return opcode_push_sr (op, SREG_DS);
	case OPCODE_POP_ES:
		return opcode_pop_sr (op, SREG_ES);
	case OPCODE_POP_SS:
		return opcode_pop_sr (op, SREG_SS);
	case OPCODE_POP_DS:
		return opcode_pop_sr (op, SREG_DS);
	case OPCODE_JMP_FAR_DIRECT:
		return opcode_jmp_far_direct (op);
	case OPCODE_0xFF:
		READ_MODRM_B (op);
		GET_MODRM (op);
		goto parse_opcode_0xff;
	case OPCODE_0x8F:
		READ_MODRM_B (op);
		GET_MODRM (op);
		goto parse_opcode_0x8f;
	case OPCODE_RETF:
		return opcode_retf (op);
	case OPCODE_MOVSB:
		return opcode_movsb (op);
	case OPCODE_MOVS:
		return opcode_movs (op);
	case OPCODE_STOSB:
		return opcode_stosb (op);
	case OPCODE_STOS:
		return opcode_stos (op);
	}
	idat = idata[code];
grp_special:
	switch (idat.type) {
	case I_MODRM:
		READ_MODRM_B (op);
		GET_MODRM (op);
		return opcode_idata (op, idat);
	case I_MIMM1:
		READ_MODRM_B (op);
		GET_MODRM (op);
		/* fall through */
	case I_IMME1:
	grp_imme1:
		op->imm = 0;
		READ_NEXT_B (op, &op->imm);
		if (idat.len == 2 && (op->imm & 0x80))
			op->imm |= 0xFFFFFFFFFFFFFF00ULL;
		return opcode_idata (op, idat);
	case I_MIMM2:
		READ_MODRM_B (op);
		GET_MODRM (op);
		/* fall through */
	case I_IMME2:
	grp_imme2:
		op->imm = 0;
		if (op->optype == OPTYPE_16BIT)
			READ_NEXT_W (op, &op->imm);
		else
			READ_NEXT_L (op, &op->imm);
		if (op->optype == OPTYPE_64BIT && (op->imm & 0x80000000))
			op->imm |= 0xFFFFFFFF00000000ULL;
		return opcode_idata (op, idat);
	case I_MOFFS:
		RIE (read_moffs (op));
		return opcode_idata (op, idat);
	case I_MGRP3:
		READ_MODRM_B (op);
		GET_MODRM (op);
		idat.type = grp3[op->modrm.reg].type;
		idat.dst = grp3[op->modrm.reg].dst;
		idat.src = grp3[op->modrm.reg].src;
		idat.func = grp3[op->modrm.reg].func;
		goto grp_special;
	case I_IMMED:
		if (idat.len == 1)
			goto grp_imme1;
		else
			goto grp_imme2;
		break;
	case I_NOMOR:
		return opcode_idata (op, idat);
	case I_ZERO:
	default:
		break;
	}
	op->ip_off += 16;
	return VMMERR_UNSUPPORTED_OPCODE;
parse_opcode_0x0f:
	switch (code) {
	case OPCODE_0x0F_0x01:
		READ_MODRM_B (op);
		goto parse_opcode_0x0f_0x01;
	case OPCODE_0x0F_0xBA:
		READ_MODRM_B (op);
		goto parse_opcode_0x0f_0xba;
	case OPCODE_0x0F_MOV_TO_CR:
		READ_MODRM_B (op);
		return opcode_mov_to_cr (op);
	case OPCODE_0x0F_MOV_FROM_CR:
		READ_MODRM_B (op);
		return opcode_mov_from_cr (op);
	case OPCODE_0x0F_MOV_TO_DR:
		READ_MODRM_B (op);
		return opcode_mov_to_dr (op);
	case OPCODE_0x0F_MOV_FROM_DR:
		READ_MODRM_B (op);
		return opcode_mov_from_dr (op);
	case OPCODE_0x0F_CLTS:
		return opcode_clts (op);
	case OPCODE_0x0F_INVD:
		return opcode_invd (op);
	case OPCODE_0x0F_WBINVD:
		return opcode_wbinvd (op);
	case OPCODE_0x0F_RDMSR:
		return opcode_rdmsr (op);
	case OPCODE_0x0F_WRMSR:
		return opcode_wrmsr (op);
	case OPCODE_0x0F_MOVZX_RM8_TO_R:
		READ_MODRM_B (op);
		GET_MODRM (op);
		return opcode_movzx_rm8_to_r (op);
	case OPCODE_0x0F_MOVZX_RM16_TO_R:
		READ_MODRM_B (op);
		GET_MODRM (op);
		return opcode_movzx_rm16_to_r (op);
	}
	if (op->longmode)
		panic ("64bit instructions begin with 0x0F not supported");
	switch (code) {
	case OPCODE_0x0F_PUSH_FS:
		return opcode_push_sr (op, SREG_FS);
	case OPCODE_0x0F_PUSH_GS:
		return opcode_push_sr (op, SREG_GS);
	case OPCODE_0x0F_POP_FS:
		return opcode_pop_sr (op, SREG_FS);
	case OPCODE_0x0F_POP_GS:
		return opcode_pop_sr (op, SREG_GS);
	case OPCODE_0x0F_LSS:
		READ_MODRM_B (op);
		GET_MODRM (op);
		return opcode_0f_lss (op);
	}
	op->ip_off += 16;
	return VMMERR_UNSUPPORTED_OPCODE;
parse_opcode_0x0f_0x01:
	switch (op->modrm.reg) {
	case OPCODE_0x0F_0x01_LGDT:
		GET_MODRM (op);
		return opcode_lgdt (op);
	case OPCODE_0x0F_0x01_LIDT:
		GET_MODRM (op);
		return opcode_lidt (op);
	case OPCODE_0x0F_0x01_SMSW:
		GET_MODRM (op);
		return opcode_smsw (op);
	case OPCODE_0x0F_0x01_LMSW:
		GET_MODRM (op);
		return opcode_lmsw (op);
	case OPCODE_0x0F_0x01_INVLPG:
		GET_MODRM (op);
		return opcode_invlpg (op);
	}
	op->ip_off += 16;
	return VMMERR_UNSUPPORTED_OPCODE;
parse_opcode_0x0f_0xba:
	switch (op->modrm.reg) {
	case OPCODE_0x0F_0xBA_BT:
		GET_MODRM (op);
		READ_NEXT_B (op, &op->imm);
		return opcode_bt_imm (op);
	}
	return VMMERR_UNSUPPORTED_OPCODE;
parse_opcode_0xff:
	switch (op->modrm.reg) {
	case OPCODE_0xFF_JMP_FAR_INDIRECT:
		return opcode_jmp_far_indirect (op);
	case OPCODE_0xFF_PUSH:
		return opcode_ff_push (op);
	}
	op->ip_off += 16;
	return VMMERR_UNSUPPORTED_OPCODE;
parse_opcode_0x8f:
	switch (op->modrm.reg) {
	case OPCODE_0x8F_POP:
		return opcode_8f_pop (op);
	}
	op->ip_off += 16;
	return VMMERR_UNSUPPORTED_OPCODE;
}
