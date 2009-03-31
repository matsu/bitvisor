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

extern u8 iommu_detected;

#define PERM_DMA_NO 0  // 00b
#define PERM_DMA_RO 1  // 01b
#define PERM_DMA_WO 2  // 10b
#define PERM_DMA_RW 3  // 11b

int parse_dmar_bios_report() ;
void iommu_setup() ;

#define DRHD_STRUCT 0
#define RMRR_STRUCT 1
#define ATSR_STRUCT 2
#define RHSA_STRUCT 3

struct desc_header {
	u8 signature[4];
	u32 length;
	u8 revision;
	u8 checksum;
	u8 oemid[6];
	u8 oem_table_id[8];
	u8 oem_revision[4];
	u8 creator_id[4];
	u8 creator_revision[4];
} __attribute__ ((packed));

struct acpi_ent_dmar {
	struct desc_header       header;
	u8			      haw;    /* Host address Width */
	u8			      flags;
	u8			      reserved[10];
} __attribute__ ((packed));

struct acpi_ent_drhd {
	u16     type;
	u16     length;
	u8      flags;
	u8      reserved;
	u16     segment;
	u64     address; /* register base address for this drhd */
} __attribute__ ((packed));

enum dev_scope_type {
	DEV_SCOPE_ENDPOINT=0x01, /* PCI Endpoing device */
	DEV_SCOPE_PCIBRIDGE,     /* PCI-PCI Bridge */
	DEV_SCOPE_IOAPIC,	/* IOAPIC device*/
	DEV_SCOPE_MSIHPET,      /* MSI capable HPET*/
};

struct dev_scope {
	u8      dev_type;
	u8      length;
	u8      reserved[2];
	u8      enum_id;
	u8      start_bus;
} __attribute__((packed));

struct pci_path {
	u8      dev;
	u8      fn;
} __attribute__((packed));
