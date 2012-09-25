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

#include <common.h>
#include <core.h>
#include "passthrough/vtd.h"
#include "passthrough/dmar.h"

#include "pci.h"
extern struct list pci_device_list_head;

u8 iommu_detected;

#define MIN_DEV_SCOPE_LEN (sizeof(struct pci_path) + sizeof(struct dev_scope))

LIST_DEFINE_HEAD(drhd_list);

static int pci_device_match(struct pci_dev *devices, int cnt, u8 bus, u8 devfn)
{
	int i;

	for (i = 0; i < cnt; i++, devices++) {
		if (devfn != devices->devfn)
			continue ;
		if (bus != devices->bus)
			continue ;
		goto found ;
	}

	return 0;
found:
	return 1;
}

struct acpi_drhd_u * matched_drhd_u(u8 bus, u8 devfn)
{
	struct acpi_drhd_u *drhd;
	struct acpi_drhd_u *include_pci_all_drhd;

	include_pci_all_drhd = NULL;
	LIST_FOREACH(drhd_list, drhd) {
		if (drhd->include_pci_all) {
			include_pci_all_drhd = drhd;
			continue;
		}

		if (pci_device_match(drhd->devices, drhd->devices_cnt, bus, devfn)) {
			return drhd;
		}
	}

	if (include_pci_all_drhd) {
		return include_pci_all_drhd;
	}

	return NULL;
}

static int num_dev_scope_entry(void *start, void *end)
{
	struct dev_scope *scope;
	int count=0;
	
	while (start < end) {
		scope = start;
		if (scope->length < MIN_DEV_SCOPE_LEN) {
			printf("(DMAR) Device scope length mismatch.\n");
			return -EINVAL;
		}
		if (scope->dev_type > 0x4) {
			printf("(DMAR) Unknown device type.\n");
			return -EINVAL;
		}

		count+=(scope->length-6)/2;  // number of entries in the 'Path' field
		start += scope->length;       // go next entry
	}

	return count;
}

static int parse_dev_scope(void *start, void *end, struct acpi_drhd_u *drhd_u)
{

	struct dev_scope *scope;
	struct pci_path *path;
	struct pci_dev *pdev;
	struct pci_dev **devices = NULL;
	u8 bus;
	int *cnt = NULL;

	cnt = &(drhd_u->devices_cnt);
	devices = &(drhd_u->devices);
	
	*cnt = num_dev_scope_entry(start, end);
	if (*cnt == 0)
	{
#ifdef VTD_DEBUG
		printf("(DMAR) parse_dev_scope: no device\n");
#endif // of VTD_DEBUG
		return 0;
	}
	
	*devices = alloc(sizeof(struct pci_dev)*(*cnt));
	if (!*devices)
		return -ENOMEM;
	memset(*devices, 0, sizeof(struct pci_dev) * (*cnt));
	
	pdev = *devices;
	scope = start;
	while ((void *)scope < end)
	{
		path = (struct pci_path *)(scope + 1);
		bus = scope->start_bus;
		
		if (scope->dev_type == DEV_SCOPE_ENDPOINT || scope->dev_type == DEV_SCOPE_PCIBRIDGE) {
#ifdef VTD_DEBUG
			printf("(DMAR) including : bdf = %x:%x:%x\n", bus, path->dev, path->fn);
#endif // of VTD_DEBUG
			pdev->bus = bus;
			pdev->df.dev_no = path->dev ;
			pdev->df.func_no = path->fn ;
			pdev++;
		}
		scope = (struct dev_scope *)(((void *)scope)+scope->length);
	}
	
	return 0;
}

#ifdef VTD_TRANS
static int parse_drhd_u(struct acpi_ent_drhd *ent)
{
	struct acpi_ent_drhd *drhd = ent;
	struct acpi_drhd_u *drhd_u;
	static int include_pci_all;
	void *sp, *ep;
	int ret = 0;

	drhd_u = alloc(sizeof(struct acpi_drhd_u));
	if (!drhd_u)
		return -ENOMEM;
	memset(drhd_u, 0, sizeof(struct acpi_drhd_u));

	drhd_u->address = drhd->address;

#ifdef VTD_DEBUG
	printf("(DMAR) Register base address = %llx\n", drhd_u->address);
#endif // of VTD_DEBUG
	if (drhd->flags & 1) {
		drhd_u->include_pci_all = 1 ;
#ifdef VTD_DEBUG
		printf("(DMAR) Including ALL the other PCI devices.\n");
#endif // of VTD_DEBUG

		if (include_pci_all)
		{
			printf("(DMAR) There should be only one INCLUDE_PCI_ALL DRHD!!\n");
			ret = -EINVAL;
		}
		include_pci_all = 1;
	} else {
		drhd_u->include_pci_all = 0 ;
		sp = (void *)(drhd + 1);
		ep   = ((void *)drhd) + ent->length;
		ret = parse_dev_scope(sp, ep, drhd_u);
	}

	if (ret)
		free(drhd_u);
	else
		LIST_APPEND(drhd_list, drhd_u);

	return ret;
}
#endif

int  parse_dmar_bios_report(struct acpi_ent_dmar *dmar) 
{
#ifdef VTD_TRANS
	struct acpi_ent_drhd *ent;
	int ret = 0;
	
	unsigned long size;
	size = dmar->header.length;
	
	if (!dmar->haw) {
		printf("(DMAR) Invalid DMAR haw\n");
		return -EINVAL;
	}
	
#ifdef VTD_DEBUG
 	printf("(DMAR) Host address width %d\n", dmar->haw);
#endif // of VTD_DEBUG
	
	ent = (struct acpi_ent_drhd *)(dmar + 1) ;
	while (((unsigned long)ent) < (((unsigned long)dmar) + size)) {
		switch (ent->type) {
		case DRHD_STRUCT:
#ifdef VTD_DEBUG
			printf("(DMAR) found DRHD_STRUCT\n");
#endif // of VTD_DEBUG
			ret = parse_drhd_u(ent);
			break;
#ifdef VTD_DEBUG
		case RMRR_STRUCT:
			printf("(DMAR) found RMRR_STRUCT\n");
			break;
		case ATSR_STRUCT:
			printf("(DMAR) found ATSR_STRUCT\n");
			break;
		case RHSA_STRUCT:
			printf("(DMAR) found RHSA_STRUCT\n");
			break;
#endif // of VTD_DEBUG

		default:
#ifdef VTD_DEBUG
			printf("(DMAR) Unknown DMAR structure type\n");
#endif // of VTD_DEBUG
			ret = -EINVAL;
			break;
		}
		if (ret)
			break;
		
		ent = ((void *)ent + ent->length);
	}
	
	// Zap APCI 'DMAR' signature to avoid guest OS's detection
	dmar->header.signature[0] = '\0';
	
	return ret;
#endif // of VTD_TRANS
	if (0)			/* make gcc happy */
		printf ("%p", parse_dev_scope);
	return 0;
}
