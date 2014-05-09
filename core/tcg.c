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

#ifdef TCG_BIOS
#include "callrealmode.h"
#include "mm.h"
#include "printf.h"
#include "string.h"
#include "uefi.h"
#include <tcg.h>

#define TCPA 0x41504354
#define FUNC_TCG_StatusCheck			0x00
#define FUNC_TCG_HashLogExtendEvent		0x01
#define FUNC_TCG_PassThroughToTPM		0x02
#define FUNC_TCG_HashLogEvent			0x04
#define FUNC_TCG_HashAll			0x05
#define FUNC_TCG_TSS				0x06
#define FUNC_TCG_CompactHashLogExtendEvent	0x07
#define INPUT_PARAM_BLK_ADDR 0x10000
#define OUTPUT_PARAM_BLK_ADDR 0x20000

static void
copy_input_buffer (void *input, u16 length)
{
	void *p;

	if (!length)
		return;
	p = mapmem_hphys (INPUT_PARAM_BLK_ADDR, length, MAPMEM_WRITE);
	memcpy (p, input, length);
	unmapmem (p, length);
}

static void
copy_output_buffer (void *output, u16 length)
{
	void *p;

	if (!length)
		return;
	p = mapmem_hphys (OUTPUT_PARAM_BLK_ADDR, length, 0);
	memcpy (output, p, length);
	unmapmem (p, length);
}

static void
copy_output_param_blk (void *output, u16 size)
{
	void *p;
	u16 *length;

	p = mapmem_hphys (OUTPUT_PARAM_BLK_ADDR, 0x10000, 0);
	length = p;
	if (size > *length)
		size = *length;
	if (size)
		memcpy (output, p, size);
	unmapmem (p, 0x10000);
}

bool
int1a_TCG_StatusCheck (u32 *return_code, u8 *major, u8 *minor,
		       u32 *feature_flags, u32 *event_log, u32 *edi)
{
	struct tcgbios_args args;

	if (uefi_booted)
		return false;
	args.in_ebx = 0;
	args.in_ecx = 0;
	args.in_edx = 0;
	args.in_esi = 0;
	args.in_edi = 0;
	args.in_es = 0;
	args.in_ds = 0;
	callrealmode_tcgbios (FUNC_TCG_StatusCheck, &args);
	*return_code = args.out_eax;
	if (args.out_eax)
		return false;
	if (args.out_ebx != TCPA)
		return false;
	*major = (u8)(args.out_ecx >> 8);
	*minor = (u8)args.out_ecx;
	*feature_flags = args.out_edx;
	*event_log = args.out_esi;
	*edi = args.out_edi;
	return true;
}

bool
int1a_TCG_HashLogExtendEvent (struct TCG_HashLogExtendEvent_input_param_blk
			      *input,
			      struct TCG_HashLogExtendEvent_output_param_blk
			      *output, u16 size, u32 *return_code)
{
	struct tcgbios_args args;

	copy_input_buffer (input, input->u.format_1.IPBLength);
	args.in_ebx = TCPA;
	args.in_ecx = 0;
	args.in_edx = 0;
	args.in_esi = OUTPUT_PARAM_BLK_ADDR & 0xF;
	args.in_ds = OUTPUT_PARAM_BLK_ADDR >> 4;
	args.in_edi = INPUT_PARAM_BLK_ADDR & 0xF;
	args.in_es = INPUT_PARAM_BLK_ADDR >> 4;
	callrealmode_tcgbios (FUNC_TCG_HashLogExtendEvent, &args);
	*return_code = args.out_eax;
	if (args.out_eax)
		return false;
	copy_output_param_blk (output, size);
	return true;
}

bool
int1a_TCG_PassThroughToTPM (struct TCG_PassThroughToTPM_input_param_blk *input,
			    struct TCG_PassThroughToTPM_output_param_blk
			    *output, u16 size, u32 *return_code)
{
	struct tcgbios_args args;

	copy_input_buffer (input, input->IPBLength);
	args.in_ebx = TCPA;
	args.in_ecx = 0;
	args.in_edx = 0;
	args.in_esi = OUTPUT_PARAM_BLK_ADDR & 0xF;
	args.in_ds = OUTPUT_PARAM_BLK_ADDR >> 4;
	args.in_edi = INPUT_PARAM_BLK_ADDR & 0xF;
	args.in_es = INPUT_PARAM_BLK_ADDR >> 4;
	callrealmode_tcgbios (FUNC_TCG_PassThroughToTPM, &args);
	*return_code = args.out_eax;
	if (args.out_eax)
		return false;
	copy_output_param_blk (output, size);
	return true;
}

bool
int1a_TCG_HashLogEvent (struct TCG_HashLogEvent_input_param_blk *input,
			struct TCG_HashLogEvent_output_param_blk *output,
			u32 *return_code)
{
	struct tcgbios_args args;

	copy_input_buffer (input, input->IPBLength);
	args.in_ebx = TCPA;
	args.in_ecx = 0;
	args.in_edx = 0;
	args.in_esi = OUTPUT_PARAM_BLK_ADDR & 0xF;
	args.in_ds = OUTPUT_PARAM_BLK_ADDR >> 4;
	args.in_edi = INPUT_PARAM_BLK_ADDR & 0xF;
	args.in_es = INPUT_PARAM_BLK_ADDR >> 4;
	callrealmode_tcgbios (FUNC_TCG_HashLogEvent, &args);
	*return_code = args.out_eax;
	if (args.out_eax)
		return false;
	copy_output_param_blk (output, sizeof *output);
	return true;
}

bool
int1a_TCG_HashAll (struct TCG_HashAll_input_param_blk *input, void *digest,
		   int digest_len, u32 *return_code)
{
	struct tcgbios_args args;

	copy_input_buffer (input, input->IPBLength);
	args.in_ebx = TCPA;
	args.in_ecx = 0;
	args.in_edx = 0;
	args.in_esi = OUTPUT_PARAM_BLK_ADDR & 0xF;
	args.in_ds = OUTPUT_PARAM_BLK_ADDR >> 4;
	args.in_edi = INPUT_PARAM_BLK_ADDR & 0xF;
	args.in_es = INPUT_PARAM_BLK_ADDR >> 4;
	callrealmode_tcgbios (FUNC_TCG_HashAll, &args);
	*return_code = args.out_eax;
	if (args.out_eax)
		return false;
	copy_output_buffer (digest, digest_len);
	return true;
}

bool
int1a_TCG_TSS (struct TCG_TSS_input_param_blk *input,
	       struct TCG_TSS_output_param_blk *output,
	       u16 size, u32 *return_code)
{
	struct tcgbios_args args;

	copy_input_buffer (input, input->IPBLength);
	args.in_ebx = TCPA;
	args.in_ecx = 0;
	args.in_edx = 0;
	args.in_esi = OUTPUT_PARAM_BLK_ADDR & 0xF;
	args.in_ds = OUTPUT_PARAM_BLK_ADDR >> 4;
	args.in_edi = INPUT_PARAM_BLK_ADDR & 0xF;
	args.in_es = INPUT_PARAM_BLK_ADDR >> 4;
	callrealmode_tcgbios (FUNC_TCG_TSS, &args);
	*return_code = args.out_eax;
	if (args.out_eax)
		return false;
	copy_output_param_blk (output, size);
	return true;
}

bool
int1a_TCG_CompactHashLogExtendEvent (u32 data_addr, u32 data_len, u32 esi,
				     u32 pcr, u32 *return_code,
				     u32 *event_number)
{
	struct tcgbios_args args;

	args.in_ebx = TCPA;
	args.in_ecx = data_len;
	args.in_edx = pcr;
	args.in_esi = esi;
	args.in_ds = 0;
	args.in_edi = data_addr & 0xF;
	args.in_es = data_addr >> 4;
	callrealmode_tcgbios (FUNC_TCG_CompactHashLogExtendEvent, &args);
	*return_code = args.out_eax;
	*event_number = args.out_edx;
	if (args.out_eax)
		return false;
	return true;
}

void
tcg_measure (void *virt, u32 len)
{
	struct TCG_HashLogExtendEvent_input_param_blk input;
	struct {
		struct TCG_HashLogExtendEvent_output_param_blk output;
		char buf[32];
	} s;
	u32 *log;
	u32 log_phys, phys;
	u32 ret, feat, event, edi;
	u8 major, minor;
	void *tmp;

	if (!len)
		return;
	if (!int1a_TCG_StatusCheck (&ret, &major, &minor, &feat, &event,
				    &edi))
		return;
	phys = 0x100000;
	log_phys = phys + len;
	log = mapmem_hphys (log_phys, 32, MAPMEM_WRITE);
	tmp = mapmem_hphys (phys, len, MAPMEM_WRITE);
	memcpy (tmp, virt, len);
	unmapmem (tmp, len);
	memset (&input, 0, sizeof input);
	input.u.format_2.IPBLength = sizeof input.u.format_2;
	input.u.format_2.HashDataPtr = phys;
	input.u.format_2.HashDataLen = len;
	input.u.format_2.PCRIndex = 4;
	input.u.format_2.LogDataPtr = log_phys;
	input.u.format_2.LogDataLen = 32;
	memset (log, 0, 32);
	log[0] = input.u.format_2.PCRIndex;
	log[1] = 13;
	if (!int1a_TCG_HashLogExtendEvent (&input, &s.output, sizeof s, &ret))
		printf ("TCG_HashLogExtendEvent error\n");
	else
		printf ("EventNumber = %u\n", s.output.EventNumber);
	unmapmem (log, 32);
}
#endif
