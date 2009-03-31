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

#include <core/spinlock.h>
#include "common/list.h"

/*
 * The PCI device ID encoding
 */

struct pci_dev {
        u8 bus;
        union {
                u8 devfn ;
                struct {
                        unsigned int func_no: 3 ;
                        unsigned int dev_no: 5 ;
                } df ;
        } ;
};

struct iommu {
        u64 reg;              /* register base address of the unit = drhd->address */
        u32 gcmd;             /* maintaining TE field */
        u64 cap;              /* capability register */
        u64 ecap;             /* extended capability register */
        spinlock_t unit_lock; /* iommu lock */
        spinlock_t reg_lock;  /* register operation lock */
        struct root_entry *root_entry; /* virtual address */
        u64 root_entry_phys ;/* physical address */
};

struct acpi_drhd_u {
	LIST_DEFINE(drhd_list) ;
	u64    address; /* register base address of the unit */
	struct    pci_dev *devices; /* target devices */
	int    devices_cnt;
	u8    include_pci_all;
	struct iommu *iommu;
};

/*
 * Error tags
 */
#define EIO              2      /* I/O error */
#define ENOMEM           3      /* Out of memory */
#define ENODEV           5      /* No such device */
#define EINVAL           7      /* Invalid argument */
