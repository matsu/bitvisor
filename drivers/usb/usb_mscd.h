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

#ifndef _USB_MSCD_H
#include <core.h>
#include "uhci.h"

#define USBMSC_LUN_MAX		15

struct usbmsc_device {
	spinlock_t lock;
	u32        tag;
	u8	   lun_max;
	u8	   lun;
	struct usbmsc_unit *unit[USBMSC_LUN_MAX + 1];
};

struct usbmsc_unit {
	u8         command;
	u32        n_blocks;
	u32        lba;
	u32        lba_max;
	u16        profile;
	size_t     length;
	struct storage_device *storage;
	int        storage_sector_size;
};

#define SCSI_OPID_MAX 0xc0
static const unsigned char scsi_op2str[SCSI_OPID_MAX][32] = {
	/* 34567890123456789012345678901 */
	"TEST UNIT READY                ", /* 0x00 */
	"(command(0x01) not supported)  ", /* 0x01 */
	"(command(0x02) not supported)  ", /* 0x02 */
	"REQUEST SENSE                  ", /* 0x03 */
	"FORMAT UNIT                    ", /* 0x04 */
	"(command(0x05) not supported)  ", /* 0x05 */
	"(command(0x06) not supported)  ", /* 0x06 */
	"(command(0x07) not supported)  ", /* 0x07 */
	"READ(6)                        ", /* 0x08 */
	"(command(0x09) not supported)  ", /* 0x09 */
	"WRITE(6)                       ", /* 0x0a */
	"(command(0x0b) not supported)  ", /* 0x0b */
	"(command(0x0c) not supported)  ", /* 0x0c */
	"(command(0x0d) not supported)  ", /* 0x0d */
	"(command(0x0e) not supported)  ", /* 0x0e */
	"(command(0x0f) not supported)  ", /* 0x0f */
	"(command(0x10) not supported)  ", /* 0x10 */
	"(command(0x11) not supported)  ", /* 0x11 */
	"INQUIRY                        ", /* 0x12 */
	"(command(0x13) not supported)  ", /* 0x13 */
	"(command(0x14) not supported)  ", /* 0x14 */
	"MODE SELECT(6)                 ", /* 0x15 */
	"(command(0x16) not supported)  ", /* 0x16 */
	"(command(0x17) not supported)  ", /* 0x17 */
	"(command(0x18) not supported)  ", /* 0x18 */
	"(command(0x19) not supported)  ", /* 0x19 */
	"MODE SENSE(06)                 ", /* 0x1a */
	"START STOP UNIT                ", /* 0x1b */
	"(command(0x1c) not supported)  ", /* 0x1c */
	"SEND DIAGNOSTIC                ", /* 0x1d */
	"PREVENT ALLOW MEDIUM REMOVAL   ", /* 0x1e */
	"(command(0x1f) not supported)  ", /* 0x1f */
	"(command(0x20) not supported)  ", /* 0x20 */
	"(command(0x21) not supported)  ", /* 0x21 */
	"(command(0x22) not supported)  ", /* 0x22 */
	"READ FORMAT CAPACITIES         ", /* 0x23 */
	"(command(0x24) not supported)  ", /* 0x24 */
	"READ CAPABILITY(10)            ", /* 0x25 */
	"(command(0x26) not supported)  ", /* 0x26 */
	"(command(0x27) not supported)  ", /* 0x27 */
	"READ(10)                       ", /* 0x28 */
	"(command(0x29) not supported)  ", /* 0x29 */
	"WRITE(10)                      ", /* 0x2a */
	"(command(0x2b) not supported)  ", /* 0x2b */
	"(command(0x2c) not supported)  ", /* 0x2c */
	"(command(0x2d) not supported)  ", /* 0x2d */
	"(command(0x2e) not supported)  ", /* 0x2e */
	"VERIFY(10)                     ", /* 0x2f */
	"(command(0x30) not supported)  ", /* 0x30 */
	"(command(0x31) not supported)  ", /* 0x31 */
	"(command(0x32) not supported)  ", /* 0x32 */
	"(command(0x33) not supported)  ", /* 0x33 */
	"(command(0x34) not supported)  ", /* 0x34 */
	"SYNCHRONIZE CACHE(10)          ", /* 0x35 */
	"(command(0x36) not supported)  ", /* 0x36 */
	"(command(0x37) not supported)  ", /* 0x37 */
	"(command(0x38) not supported)  ", /* 0x38 */
	"(command(0x39) not supported)  ", /* 0x39 */
	"(command(0x3a) not supported)  ", /* 0x3a */
	"(command(0x3b) not supported)  ", /* 0x3b */
	"(command(0x3c) not supported)  ", /* 0x3c */
	"(command(0x3d) not supported)  ", /* 0x3d */
	"(command(0x3e) not supported)  ", /* 0x3e */
	"(command(0x3f) not supported)  ", /* 0x3f */
	"(command(0x40) not supported)  ", /* 0x40 */
	"(command(0x41) not supported)  ", /* 0x41 */
	"READ SUBCHANNEL                ", /* 0x42 */
	"READ TOC/PMA/ATIP              ", /* 0x43 */
	"(command(0x44) not supported)  ", /* 0x44 */
	"(command(0x45) not supported)  ", /* 0x45 */
	"GET CONFIGURATION              ", /* 0x46 */
	"(command(0x47) not supported)  ", /* 0x47 */
	"(command(0x48) not supported)  ", /* 0x48 */
	"(command(0x49) not supported)  ", /* 0x49 */
	"GET EVENT/STATUS NOTIFICATION  ", /* 0x4a */
	"(command(0x4b) not supported)  ", /* 0x4b */
	"(command(0x4c) not supported)  ", /* 0x4c */
	"(command(0x4d) not supported)  ", /* 0x4d */
	"(command(0x4e) not supported)  ", /* 0x4e */
	"(command(0x4f) not supported)  ", /* 0x4f */
	"(command(0x50) not supported)  ", /* 0x50 */
	"READ DISC INFORMATION          ", /* 0x51 */
	"READ TRACK INFORMATION         ", /* 0x52 */
	"(command(0x53) not supported)  ", /* 0x53 */
	"(command(0x54) not supported)  ", /* 0x54 */
	"MODE SELECT(10)                ", /* 0x55 */
	"(command(0x56) not supported)  ", /* 0x56 */
	"(command(0x57) not supported)  ", /* 0x57 */
	"(command(0x58) not supported)  ", /* 0x58 */
	"(command(0x59) not supported)  ", /* 0x59 */
	"MODE SENSE(10)                 ", /* 0x5a */
	"(command(0x5b) not supported)  ", /* 0x5b */
	"READ BUFFER CAPACITY           ", /* 0x5c */
	"(command(0x5d) not supported)  ", /* 0x5d */
	"(command(0x5e) not supported)  ", /* 0x5e */
	"(command(0x5f) not supported)  ", /* 0x5f */
	"(command(0x60) not supported)  ", /* 0x60 */
	"(command(0x61) not supported)  ", /* 0x61 */
	"(command(0x62) not supported)  ", /* 0x62 */
	"(command(0x63) not supported)  ", /* 0x63 */
	"(command(0x64) not supported)  ", /* 0x64 */
	"(command(0x65) not supported)  ", /* 0x65 */
	"(command(0x66) not supported)  ", /* 0x66 */
	"(command(0x67) not supported)  ", /* 0x67 */
	"(command(0x68) not supported)  ", /* 0x68 */
	"(command(0x69) not supported)  ", /* 0x69 */
	"(command(0x6a) not supported)  ", /* 0x6a */
	"(command(0x6b) not supported)  ", /* 0x6b */
	"(command(0x6c) not supported)  ", /* 0x6c */
	"(command(0x6d) not supported)  ", /* 0x6d */
	"(command(0x6e) not supported)  ", /* 0x6e */
	"(command(0x6f) not supported)  ", /* 0x6f */
	"(command(0x70) not supported)  ", /* 0x70 */
	"(command(0x71) not supported)  ", /* 0x71 */
	"(command(0x72) not supported)  ", /* 0x72 */
	"(command(0x73) not supported)  ", /* 0x73 */
	"(command(0x74) not supported)  ", /* 0x74 */
	"(command(0x75) not supported)  ", /* 0x75 */
	"(command(0x76) not supported)  ", /* 0x76 */
	"(command(0x77) not supported)  ", /* 0x77 */
	"(command(0x78) not supported)  ", /* 0x78 */
	"(command(0x79) not supported)  ", /* 0x79 */
	"(command(0x7a) not supported)  ", /* 0x7a */
	"(command(0x7b) not supported)  ", /* 0x7b */
	"(command(0x7c) not supported)  ", /* 0x7c */
	"(command(0x7d) not supported)  ", /* 0x7d */
	"(command(0x7e) not supported)  ", /* 0x7e */
	"(command(0x7f) not supported)  ", /* 0x7f */
	"(command(0x80) not supported)  ", /* 0x80 */
	"(command(0x81) not supported)  ", /* 0x81 */
	"(command(0x82) not supported)  ", /* 0x82 */
	"(command(0x83) not supported)  ", /* 0x83 */
	"(command(0x84) not supported)  ", /* 0x84 */
	"(command(0x85) not supported)  ", /* 0x85 */
	"(command(0x86) not supported)  ", /* 0x86 */
	"(command(0x87) not supported)  ", /* 0x87 */
	"(command(0x88) not supported)  ", /* 0x88 */
	"(command(0x89) not supported)  ", /* 0x89 */
	"(command(0x8a) not supported)  ", /* 0x8a */
	"(command(0x8b) not supported)  ", /* 0x8b */
	"(command(0x8c) not supported)  ", /* 0x8c */
	"(command(0x8d) not supported)  ", /* 0x8d */
	"(command(0x8e) not supported)  ", /* 0x8e */
	"(command(0x8f) not supported)  ", /* 0x8f */
	"(command(0x90) not supported)  ", /* 0x90 */
	"(command(0x91) not supported)  ", /* 0x91 */
	"(command(0x92) not supported)  ", /* 0x92 */
	"(command(0x93) not supported)  ", /* 0x93 */
	"(command(0x94) not supported)  ", /* 0x94 */
	"(command(0x95) not supported)  ", /* 0x95 */
	"(command(0x96) not supported)  ", /* 0x96 */
	"(command(0x97) not supported)  ", /* 0x97 */
	"(command(0x98) not supported)  ", /* 0x98 */
	"(command(0x99) not supported)  ", /* 0x99 */
	"(command(0x9a) not supported)  ", /* 0x9a */
	"(command(0x9b) not supported)  ", /* 0x9b */
	"(command(0x9c) not supported)  ", /* 0x9c */
	"(command(0x9d) not supported)  ", /* 0x9d */
	"(command(0x9e) not supported)  ", /* 0x9e */
	"(command(0x9f) not supported)  ", /* 0x9f */
	"REPORT LUNS                    ", /* 0xa0 */
	"(command(0xa1) not supported)  ", /* 0xa1 */
	"(command(0xa2) not supported)  ", /* 0xa2 */
	"(command(0xa3) not supported)  ", /* 0xa3 */
	"REPORT KEY                     ", /* 0xa4 */
	"(command(0xa5) not supported)  ", /* 0xa5 */
	"(command(0xa6) not supported)  ", /* 0xa6 */
	"(command(0xa7) not supported)  ", /* 0xa7 */
	"READ(12)                       ", /* 0xa8 */
	"(command(0xa9) not supported)  ", /* 0xa9 */
	"WRITE(12)                      ", /* 0xaa */
	"(command(0xab) not supported)  ", /* 0xab */
	"GET PERFORMANCE                ", /* 0xac */
	"READ DISC STRUCTURE            ", /* 0xad */
	"(command(0xae) not supported)  ", /* 0xae */
	"(command(0xaf) not supported)  ", /* 0xaf */
	"(command(0xb0) not supported)  ", /* 0xb0 */
	"(command(0xb1) not supported)  ", /* 0xb1 */
	"(command(0xb2) not supported)  ", /* 0xb2 */
	"(command(0xb3) not supported)  ", /* 0xb3 */
	"(command(0xb4) not supported)  ", /* 0xb4 */
	"(command(0xb5) not supported)  ", /* 0xb5 */
	"SET STREAMING                  ", /* 0xb6 */
	"(command(0xb7) not supported)  ", /* 0xb7 */
	"(command(0xb8) not supported)  ", /* 0xb8 */
	"READ CD MSF                    ", /* 0xb9 */
	"(command(0xba) not supported)  ", /* 0xba */
	"(command(0xbb) not supported)  ", /* 0xbb */
	"(command(0xbc) not supported)  ", /* 0xbc */
	"(command(0xbd) not supported)  ", /* 0xbd */
	"READ CD                        ", /* 0xbe */
	"(command(0xbf) not supported)  ", /* 0xbf */
};

struct usb_msc_cbw {
	u32 dCBWSignature;
	u32 dCBWTag;
	u32 dCBWDataTransferLength;
	u8  bmCBWFlags;
	u8  bCBWLUN;			/* includes 4 reserved bits */
	u8  bCBWCBLength;		/* includes 3 reserved bits */
	u8 CBWCB[16];
} __attribute__ ((packed));

#define USBMSC_STAT_MAX 0x03
static const unsigned char st2str[USBMSC_STAT_MAX][16] = {
	/* 3456789012345 */
	"Command Passed ", /* 0x00 */
	"Command Failed ", /* 0x01 */
	"Phase Error    ", /* 0x02 */
};

struct usb_msc_csw {
	u32 dCSWSignature;
	u32 dCSWTag;
	u32 dCSWDataResidue;
	u8  bCSWStatus;
} __attribute__ ((packed));


/* media profile name */
#define	USBMSC_PROF_NOPROF	0x0000
#define	USBMSC_PROF_NORMDISK	0x0001
#define	USBMSC_PROF_RMDISK	0x0002
#define	USBMSC_PROF_MOERASE	0x0003
#define	USBMSC_PROF_MOWRONEC	0x0004
#define	USBMSC_PROF_ASMO	0x0005
#define	USBMSC_PROF_CDROM	0x0008
#define	USBMSC_PROF_CDR		0x0009
#define	USBMSC_PROF_CDRW	0x000a
#define	USBMSC_PROF_DVDROM	0x0010
#define	USBMSC_PROF_DVDR	0x0011
#define	USBMSC_PROF_DVDRAM	0x0012
#define	USBMSC_PROF_DVDRW_RO	0x0013
#define	USBMSC_PROF_DVDRW_SR	0x0014
#define	USBMSC_PROF_DVDR_DLSR	0x0015
#define	USBMSC_PROF_DVDR_DLJR	0x0016
#define	USBMSC_PROF_DVD_P_RW	0x001a
#define	USBMSC_PROF_DVD_P_R	0x001b
#define	USBMSC_PROF_BLURAYROM	0x0040
#define	USBMSC_PROF_BDR_SRM	0x0041
#define	USBMSC_PROF_BDR_RRM	0x0042
#define	USBMSC_PROF_BDR_RE	0x0043
#define	USBMSC_PROF_HDDVDROM	0x0050
#define	USBMSC_PROF_HDDVDR	0x0051
#define	USBMSC_PROF_HDDVDRAM	0x0052
#define	USBMSC_PROF_NOTCONFORM	0xffff

#endif /* _USB_MSCD_H */
