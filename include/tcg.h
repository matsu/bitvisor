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

#ifndef _TCG_H
#define _TCG_H

#define TPM_ALG_SHA 4

/* Return Codes */
#define TCG_PC_OK		0x0000
#define TCG_PC_TPMERROR(TPM_driver_error) \
	((TCG_PC_OK + 0x01) | ((TPM_driver_error) << 16))
#define TCG_PC_LOGOVERFLOW	(TCG_PC_OK + 0x02)
#define TCG_PC_UNSUPPORTED	(TCG_PC_OK + 0x03)

/* TCG_HashLogExtendEvent Input Parameter Block Format 1 */
struct TCG_HashLogExtendEvent_input_param_blk_1 {
	u16 IPBLength;
	u16 Reserved;
	u32 HashDataPtr;
	u32 HashDataLen;
	u32 PCRIndex;
	u32 LogDataPtr;
	u32 LogDataLen;
} __attribute__ ((packed));

/* TCG_HashLogExtendEvent Input Parameter Block Format 2 */
struct TCG_HashLogExtendEvent_input_param_blk_2 {
	u16 IPBLength;
	u16 Reserved1;
	u32 HashDataPtr;
	u32 HashDataLen;
	u32 PCRIndex;
	u32 Reserved2;
	u32 LogDataPtr;
	u32 LogDataLen;
} __attribute__ ((packed));

/* TCG_HashLogExtendEvent Input Parameter Block Format */
struct TCG_HashLogExtendEvent_input_param_blk {
	union {
		struct TCG_HashLogExtendEvent_input_param_blk_1 format_1;
		struct TCG_HashLogExtendEvent_input_param_blk_2 format_2;
	} u;
} __attribute__ ((packed));

/* TCG_HashLogExtendEvent Output Parameter Block */
struct TCG_HashLogExtendEvent_output_param_blk {
	u16 OPBLength;
	u16 Reserved;
	u32 EventNumber;
	u8 HashValue[1];
} __attribute__ ((packed));

/* TCG_PassThroughToTPM Input Parameter Block */
struct TCG_PassThroughToTPM_input_param_blk {
	u16 IPBLength;
	u16 Reserved1;
	u16 OPBLength;
	u16 Reserved2;
	u8 TPMOperandIn[1];
} __attribute__ ((packed));

/* TCG_PassThroughToTPM Output Parameter Block */
struct TCG_PassThroughToTPM_output_param_blk {
	u16 OPBLength;
	u16 Reserved;
	u8 TPMOperandOut[1];
} __attribute__ ((packed));

/* TCG_HashLogEvent Input Parameter Block */
struct TCG_HashLogEvent_input_param_blk {
	u16 IPBLength;
	u16 Reserved;
	u32 HashDataPtr;
	u32 HashDataLen;
	u32 PCRIndex;
	u32 LogEventType;
	u32 LogDataPtr;
	u32 LogDataLen;
} __attribute__ ((packed));

/* TCG_HashLogEvent Output Parameter Block */
struct TCG_HashLogEvent_output_param_blk {
	u16 OPBLength;
	u16 Reserved;
	u32 EventNumber;
} __attribute__ ((packed));

/* TCG_HashAll Input Parameter Block */
struct TCG_HashAll_input_param_blk {
	u16 IPBLength;
	u16 Reserved;
	u32 HashDataPtr;
	u32 HashDataLen;
	u32 AlgorithmID;
} __attribute__ ((packed));

/* TCG_TSS Input Parameter Block */
struct TCG_TSS_input_param_blk {
	u16 IPBLength;
	u16 Reserved1;
	u16 OPBLength;
	u16 Reserved2;
	u8 TSSOperandIn[1];
} __attribute__ ((packed));

/* TCG_TSS Output Parameter Block */
struct TCG_TSS_output_param_blk {
	u16 OPBLength;
	u16 Reserved;
	u8 TSSOperandOut[1];
} __attribute__ ((packed));

bool int1a_TCG_StatusCheck (u32 *return_code, u8 *major, u8 *minor,
			    u32 *feature_flags, u32 *event_log, u32 *edi);
bool int1a_TCG_HashLogExtendEvent (struct
				   TCG_HashLogExtendEvent_input_param_blk
				   *input,
				   struct
				   TCG_HashLogExtendEvent_output_param_blk
				   *output, u16 size, u32 *return_code);
bool int1a_TCG_PassThroughToTPM (struct TCG_PassThroughToTPM_input_param_blk
				 *input,
				 struct TCG_PassThroughToTPM_output_param_blk
				 *output, u16 size, u32 *return_code);
bool int1a_TCG_HashLogEvent (struct TCG_HashLogEvent_input_param_blk *input,
			     struct TCG_HashLogEvent_output_param_blk *output,
			     u32 *return_code);
bool int1a_TCG_HashAll (struct TCG_HashAll_input_param_blk *input,
			void *digest, int digest_len, u32 *return_code);
bool int1a_TCG_TSS (struct TCG_TSS_input_param_blk *input,
		    struct TCG_TSS_output_param_blk *output,
		    u16 size, u32 *return_code);
bool int1a_TCG_CompactHashLogExtendEvent (u32 data_addr, u32 data_len,
					  u32 esi, u32 pcr, u32 *return_code,
					  u32 *event_number);
void tcg_measure (void *virt, u32 len);

#endif
