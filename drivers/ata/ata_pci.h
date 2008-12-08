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

#ifndef _ATA_PCI_H
#define _ATA_PCI_H

/* Compatiblity Mode I/O registers address */
#define ATA_MAX_COMPAT_HOSTS	4
#define ATA_COMPAT_PRI_CMD_BASE 0x01F0
#define ATA_COMPAT_PRI_CTL_BASE 0x03F6
#define ATA_COMPAT_SEC_CMD_BASE 0x0170
#define ATA_COMPAT_SEC_CTL_BASE 0x0376
#define ATA_COMPAT_TER_CMD_BASE 0x01E8
#define ATA_COMPAT_TER_CTL_BASE 0x03EE
#define ATA_COMPAT_QUA_CMD_BASE 0x0168
#define ATA_COMPAT_QUA_CTL_BASE 0x036E

/* PCI Configuration Space API */
#define ATA_NATIVE_PCI_ONLY	0x05
#define ATA_FIXED_MODE		0
#define ATA_COMPATIBILITY_MODE	0
#define ATA_NATIVE_PCI_MODE	1
typedef struct ata_api {
	union {
		u8 value;
		struct {
			unsigned int primary_mode:	1;
			unsigned int primary_fixed:	1;
			unsigned int secondary_mode:	1;
			unsigned int secondary_fixed:	1;
			unsigned int /* dummy */:	3;
			unsigned int bus_master:	1;
		} __attribute__ ((packed));
	};
} ata_api_t;

#endif
