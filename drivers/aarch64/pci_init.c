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

#include <arch/pci_init.h>
#include <core/dt.h>
#include <core/mm.h>
#include "../pci_internal.h"

const struct mm_as *
pci_init_arch_as_dma (struct pci_device *dev, struct pci_device *pdev)
{
	return as_passvm;
}

const struct mm_as *
pci_init_arch_virtual_as_dma (struct pci_virtual_device *dev)
{
	return as_passvm;
}

uint
pci_init_arch_find_segment (
	void (*pci_record_segment) (struct pci_config_mmio_data *d))
{
#ifdef DEVICETREE
	uint n;
	struct pci_config_mmio_data d;
	struct dt_pci_mcfg_iterator *iter;

	iter = dt_pci_mcfg_iterator_alloc ();
	for (n = 0; dt_pci_mcfg_get (iter, &d.base, &d.seg_group, &d.bus_start,
				     &d.bus_end); n++)
		pci_record_segment (&d);
	dt_pci_mcfg_iterator_free (iter);

	return n;
#else
	return 0;
#endif
}
