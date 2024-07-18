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

#include <bits.h>
#include <core/assert.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/types.h>
#include "acc_emu.h"
#include "acc_emu_asm.h"
#include "asm.h"
#include "exception.h"
#include "mmio.h"
#include "mmu.h"
#include "vm.h"

#define SIG_LDRx_LIT	 (BIT (28) | BIT (27))
#define SIG_IMM_UNSCALE	 (BIT (29) | BIT (28) | BIT (27))
#define SIG_IMM_POST	 (BIT (29) | BIT (28) | BIT (27) | BIT (10))
#define SIG_IMM_PRE	 (BIT (29) | BIT (28) | BIT (27) | BIT (11) | BIT (10))
#define SIG_REG		 (BIT (29) | BIT (28) | BIT (27) | BIT (21) | BIT (11))
#define SIG_IMM_UNSIGNED (BIT (29) | BIT (28) | BIT (27) | BIT (24))

#define SIGNED_EXT64(val, val_nbits) \
	(((i64)(val) << (64 - (val_nbits))) >> (64 - (val_nbits)))

#define OPT_UXTW 0x2
#define OPT_LSL	 0x3
#define OPT_SXTW 0x6
#define OPT_SXTX 0x7

union fp_regs {
	u64 d[2];
	u32 s[4];
	u16 h[8];
	u8 b[16];
};

static void
do_store (void *addr, u64 val, u32 size)
{
	switch (size) {
	case 1:
		*(u8 *)addr = val;
		break;
	case 2:
		*(u16 *)addr = val;
		break;
	case 4:
		*(u32 *)addr = val;
		break;
	case 8:
		*(u64 *)addr = val;
		break;
	default:
		panic ("%s(): impossible store size %u", __func__, size);
		break;
	};
}

static void
do_load (void *addr, u64 *data, u32 size, bool signed_ext)
{
	u64 val = 0;
	uint bits = 64;

	switch (size) {
	case 1:
		val = *(u8 *)addr;
		bits = 8;
		break;
	case 2:
		val = *(u16 *)addr;
		bits = 16;
		break;
	case 4:
		val = *(u32 *)addr;
		bits = 32;
		break;
	case 8:
		val = *(u64 *)addr;
		bits = 64;
		break;
	case 16:
		ASSERT (!signed_ext);
		data[0] = *(u64 *)addr;
		data[1] = *(u64 *)(addr + 8);
		return;
	default:
		panic ("%s(): impossible load size %u", __func__, size);
		break;
	};

	if (signed_ext)
		val = SIGNED_EXT64 (val, bits);

	*data = val;
}

static void
do_access (u64 addr, bool wr, u32 size, u64 *data, u64 flags, bool signed_ext)
{
	void *a;

	if (!mmio_call_handler (addr, wr, size, data, flags)) {
		a = mapmem_as (vm_get_current_as (), addr, size, flags);
		if (wr)
			do_store (a, *data, size);
		else
			do_load (a, data, size, signed_ext);
		unmapmem (a, size);
	}
}

static u64
vaddr_from_rn (u64 *regs, uint rn, uint el)
{
	u64 vaddr;

	if (rn == 31)
		vaddr = el == 1 ? mrs (SP_EL1) : mrs (SP_EL0);
	else
		vaddr = regs[rn];

	return vaddr;
}

static inline void
inst_decode_common (u32 inst, u32 *rt, u32 *rn, u32 *opc, u32 *v, u32 *s)
{
	*rt = inst & 0x1F;
	*rn = (inst >> 5) & 0x1F;
	*opc = (inst >> 22) & 0x3;
	*v = (inst >> 26) & 0x1;
	*s = (inst >> 30) & 0x3;
}

static inline u32
size_common (u32 s, u32 opc, u32 v)
{
	u32 shift;

	shift = s;
	if (v && !!(opc & 0x2))
		shift += 1;
	ASSERT (shift < 4);

	return 1 << shift; /* At most 16 bytes */
}

static inline bool
check_prefetch_common (u32 s, u32 opc, u32 v)
{
	return s == 3 && opc == 2 && v == 0;
}

int
acc_emu_emulate (union exception_saved_regs *r, u64 elr, bool wr, uint el)
{
	u64 vaddr, inst_ipa_addr, inst_flags, ipa_addr, flags, reg_offset;
	u64 *regs = r->regs;
	u32 *inst_p, inst;
	u32 rm, rn, rt, s, opc, size, v, imm, option, reg_shift;
	int error = 0;
	bool signed_ext = false;

	/*
	 * Translate the instruction address with read permission first. We
	 * return immediately if the translation fails.
	 */
	error = mmu_gvirt_to_ipa (elr, el, false, &inst_ipa_addr, &inst_flags);
	if (error) {
		printf ("%s(): ELR translation fault: 0x%llX EL: %u\n",
			__func__, elr, el);
		return error;
	}

	/*
	 * Read the instruction. We map the memory with the guest's memory
	 * space to avoid reading BitVisor memory.
	 */
	inst_p = mapmem_as (vm_get_current_as (), inst_ipa_addr,
			    sizeof *inst_p, inst_flags);
	inst = *inst_p;
	unmapmem (inst_p, sizeof *inst_p);

	/*
	 * We cannot obtain the fault address from FAR_EL2 directly. It is
	 * because of an unaligned access that crosses a page boundary. FAR_EL2
	 * reports only the start address of the fault. We need to obtain the
	 * actual beginning from decoding the instruction.
	 */

	if ((inst & SIG_REG) == SIG_REG) {
		inst_decode_common (inst, &rt, &rn, &opc, &v, &s);
		if (check_prefetch_common (s, opc, v)) {
			if (0)
				printf ("%s(): SIG_REG prefetch\n", __func__);
			goto end;
		}
		size = size_common (s, opc, v);
		reg_shift = !!((inst >> 12) & 1) ? s : 0;
		option = (inst >> 13) & 7;
		rm = (inst >> 16) & 0x1F;
		reg_offset = regs[rm] << reg_shift;
		switch (option) {
		case OPT_UXTW:
		case OPT_LSL /* Also known as UXTX */:
			/* Do nothing, unsigned extent case */
			break;
		case OPT_SXTW:
		case OPT_SXTX:
			reg_offset = SIGNED_EXT64 (reg_offset, size * 8);
			break;
		default:
			panic ("%s(): unhandled SIG_REG option 0x%X", __func__,
			       option);
			break;
		}
		vaddr = vaddr_from_rn (regs, rn, el) + reg_offset;
	} else if ((inst & SIG_IMM_UNSIGNED) == SIG_IMM_UNSIGNED) {
		inst_decode_common (inst, &rt, &rn, &opc, &v, &s);
		if (check_prefetch_common (s, opc, v)) {
			if (0)
				printf ("%s(): SIG_IMM_UNSIGNED prefetch\n",
					__func__);
			goto end;
		}
		size = size_common (s, opc, v);
		imm = (inst >> 10) & 0xFFF; /* 12 bits */
		vaddr = vaddr_from_rn (regs, rn, el) + ((u64)imm << s);
	} else if (((inst & SIG_IMM_PRE) == SIG_IMM_PRE) ||
		   ((inst & SIG_IMM_POST) == SIG_IMM_POST)) {
		inst_decode_common (inst, &rt, &rn, &opc, &v, &s);
		/* No prefetch for this varient */
		size = size_common (s, opc, v);
		imm = (inst >> 12) & 0x1FF; /* 9 bits */
		vaddr = vaddr_from_rn (regs, rn, el);
		/* Update base register immediately regardless of pre/post */
		if (rn == 31) {
			if (el == 1)
				msr (SP_EL1, vaddr + SIGNED_EXT64 (imm, 9));
			else
				msr (SP_EL0, vaddr + SIGNED_EXT64 (imm, 9));
		} else {
			regs[rn] += SIGNED_EXT64 (imm, 9);
		}
		/* For pre-increment, we need to imm to vaddr immediately */
		if (((inst & SIG_IMM_PRE) == SIG_IMM_PRE))
			vaddr += SIGNED_EXT64 (imm, 9);
	} else if ((inst & SIG_IMM_UNSCALE) == SIG_IMM_UNSCALE) {
		inst_decode_common (inst, &rt, &rn, &opc, &v, &s);
		if (check_prefetch_common (s, opc, v)) {
			if (0)
				printf ("%s(): SIG_IMM_UNSCALE prefetch\n",
					__func__);
			goto end;
		}
		size = size_common (s, opc, v);
		imm = (inst >> 12) & 0x1FF; /* 9 bits */
		vaddr = vaddr_from_rn (regs, rn, el) + SIGNED_EXT64 (imm, 9);
	} else if ((inst & SIG_LDRx_LIT) == SIG_LDRx_LIT) {
		ASSERT (!wr);
		opc = (inst >> 30) & 0x3;
		if (opc == 0x3) {
			if (0)
				printf ("%s(): SIG_LDRx_LIT prefetch\n",
					__func__);
			goto end; /* Prefetch, nothing to do */
		}
		rt = inst & 0x1F;
		imm = (inst >> 5) & 0x3FF; /* 19 bits */
		v = (inst >> 26) & 0x1;
		if (v)
			size = 4 << opc;
		else
			size = 1 << (2 + (opc & 0x1));
		/* vaddr = PC + offset. offset = sign_ext (imm * 4) */
		vaddr = r->reg.elr_el2 + SIGNED_EXT64 (imm << 2, 19 + 2);
	} else {
		printf ("%s(): unhandled load/store varient 0x%X\n", __func__,
			inst);
		error = -1;
		goto end;
	}

	/* For debugging */
	if (0 && vaddr != r->reg.far_el2)
		printf ("%s(): mismatch vaddr: 0x%llX FAR_EL2: 0x%llX\n",
			__func__, vaddr, r->reg.far_el2);

	/* Obtain the fault address, translate with the original permission */
	error = mmu_gvirt_to_ipa (vaddr, el, wr, &ipa_addr, &flags);
	if (error) {
		printf ("%s(): vaddr translation fault: 0x%llX EL: %u\n",
			__func__, vaddr, el);
		goto end;
	}

	if (v) {
		union fp_regs fpr[32];
		acc_emu_save_fp_regs (fpr);
		fpr[rt].d[1] = 0;
		do_access (ipa_addr, wr, size, fpr[rt].d, flags, signed_ext);
		acc_emu_restore_fp_regs (fpr);
	} else {
		signed_ext = !!(opc & 0x2);
		do_access (ipa_addr, wr, size, &regs[rt], flags, signed_ext);
	}
end:
	if (error) {
		printf ("%s(): error, dumping instruction context from guest"
			" physical memory\n",
			__func__);
		u64 e = elr - 32;
		u64 ia = inst_ipa_addr - 32;
		inst_p = mapmem_as (vm_get_current_as (), ia,
				    sizeof *inst_p * 16, inst_flags);
		for (int i = 0; i < 16; i++) {
			if (i == 8)
				printf (">>> ");
			printf ("0x%llX 0x%llX: 0x%08X\n", e + (i * 4),
				ia + (i * 4), inst_p[i]);
		}
		unmapmem (inst_p, sizeof *inst_p * 16);
	}

	return error;
}
