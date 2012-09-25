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

#include "config.h"
#include "convert.h"
#include "initfunc.h"
#include "mm.h"
#include "printf.h"
#include "string.h"
#include <tcg.h>

static char *random_buf;

#ifdef TCG_BIOS
static u16
swap16 (u16 value)
{
	u8 l, h;

	conv16to8 (value, &l, &h);
	conv8to16 (h, l, &value);
	return value;
}

static u32
swap32 (u32 value)
{
	u16 l, h;

	conv32to16 (value, &l, &h);
	conv16to32 (swap16 (h), swap16 (l), &value);
	return value;
}
#endif

static unsigned int
tpm_getrandom (void *buf, unsigned int size)
{
#ifdef TCG_BIOS
	struct {
		u16 tag;
		u32 len;
		u32 command;
		u32 bytessize;
	} __attribute__ ((packed)) *tpminput;
	struct {
		u16 tag;
		u32 len;
		u32 result;
		u32 bytessize;
		u8 bytes[1];
	} __attribute__ ((packed)) *tpmoutput;
	struct TCG_PassThroughToTPM_input_param_blk *input;
	struct TCG_PassThroughToTPM_output_param_blk *output;
	u32 ret;
	unsigned int inputlen, outputlen;
	static const u16 TPM_TAG_RQU_COMMAND = 193;
	static const u32 TPM_ORD_GetRandom = 0x46;

	inputlen = sizeof *input - sizeof input->TPMOperandIn
		+ sizeof *tpminput;
	outputlen = sizeof *output - sizeof output->TPMOperandOut
		+ sizeof *tpmoutput - sizeof tpmoutput->bytes + size;
	input = alloc (inputlen);
	output = alloc (outputlen);
	memset (input, 0, inputlen);
	input->IPBLength = inputlen;
	input->OPBLength = outputlen;
	tpminput = (void *)input->TPMOperandIn;
	tpminput->tag = swap16 (TPM_TAG_RQU_COMMAND);
	tpminput->len = swap32 (sizeof *tpminput);
	tpminput->command = swap32 (TPM_ORD_GetRandom);
	tpminput->bytessize = swap32 (size);
	memset (output, 0, outputlen);
	if (!int1a_TCG_PassThroughToTPM (input, output, outputlen, &ret))
		return 0;
	tpmoutput = (void *)output->TPMOperandOut;
	if (tpmoutput->result != TCG_PC_OK)
		return 0;
	if (size > tpmoutput->bytessize)
		size = tpmoutput->bytessize;
	memcpy (buf, tpmoutput->bytes, size);
	free (input);
	free (output);
	return size;
#else
	return 0;
#endif
}

static void
random_init_config0 (void)
{
	int i;

	if (random_buf) {
		for (i = 0; i < sizeof config.vmm.randomSeed; i++)
			config.vmm.randomSeed[i] ^= random_buf[i];
		free (random_buf);
		random_buf = NULL;
	}
}

static void
random_init_global (void)
{
	char *p;
	int size;
	unsigned int r;
	bool tcg = false;
#ifdef TCG_BIOS
	u32 ret, feat, event, edi;
	u8 major, minor;

	if (int1a_TCG_StatusCheck (&ret, &major, &minor, &feat, &event, &edi))
		tcg = true;
#endif
	if (!tcg) {
		random_buf = NULL;
		return;
	}
	size = sizeof config.vmm.randomSeed;
	random_buf = p = alloc (size);
	while (size > 0) {
		r = tpm_getrandom (p, size > 64 ? 64 : size);
		if (!r)
			break;
		p += r;
		size -= r;
	}
	size = p - random_buf;
	if (size > 0) {
		printf ("%d bytes from TPM random number generator.\n", size);
		memcpy (config.vmm.randomSeed, random_buf, size);
	} else {
		free (random_buf);
		random_buf = NULL;
	}
}

INITFUNC ("bsp0", random_init_global);
INITFUNC ("config00", random_init_config0);
