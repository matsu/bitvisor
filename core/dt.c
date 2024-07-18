/*
 * Copyright (c) 2024 Igel Co., Ltd.
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

#include <core/dt.h>
#include <core/initfunc.h>
#include <core/list.h>
#include <core/mm.h>
#include <core/panic.h>
#include <core/printf.h>
#include <core/string.h>
#include <libfdt.h>
#include <share/uefi_boot.h>
#include "dt.h"
#include "uefi_param_ext.h"

#define DT_PCI_RANGE_GET_SPACE_CODE(flag) (((flag) >> 24) & 0x3)

#define DT_PCI_CODE_IS_IO(code) ((code) == 0x1)
#define DT_PCI_CODE_IS_MM(code) ((code) == 0x2 || (code) == 0x3)

struct dt_pci_range {
	LIST1_DEFINE (struct dt_pci_range);
	u64 child_address;
	u64 parent_address;
	u64 len;
	u32 flags;
};

struct dt_pci_info {
	LIST1_DEFINE (struct dt_pci_info);
	LIST1_DEFINE_HEAD (struct dt_pci_range, ranges);
	phys_t reg;
	u64 len;
	u32 bus_start;
	u32 bus_end;
	u32 seg_domain;
};

struct dt_pci_mcfg_iterator {
	struct dt_pci_info *current_pci_info;
};

static struct uuid devtree_uuid = UEFI_BITVISOR_DEVTREE_UUID;

static LIST1_DEFINE_HEAD_INIT (struct dt_pci_info, pci_info_list);

static phys_t fdt_base;
static size_t fdt_size;
static void *fdt;
static bool no_more_fdt;

const void *
dt_get_fdt (void)
{
	/* We currently don't make a copy of FDT as it can be big */
	if (no_more_fdt)
		panic ("%s(): cannot use after starting the guest", __func__);
	if (!fdt)
		panic ("%s(): fdt is not currently mapped", __func__);
	return fdt;
}

void
dt_no_more_fdt (void)
{
	unmapmem (fdt, fdt_size);
	fdt = NULL;
	no_more_fdt = true;
}

enum dt_result_t
dt_helper_cur_node_reg_address_size_cell (const void *fdt, int nodeoffset,
					  int *address_cells, int *size_cells)
{
	int parentoffset, ac, sz;

	parentoffset = fdt_parent_offset (fdt, nodeoffset);
	if (parentoffset < 0) {
		printf ("%s(): cannot get parentoffset %d\n", __func__,
			parentoffset);
		return DT_RESULT_ERROR;
	}

	if (address_cells) {
		ac = fdt_address_cells (fdt, parentoffset);
		if (ac < 0) {
			printf ("%s(): parent #address-cells reading error"
				" %d\n",
				__func__, ac);
			return DT_RESULT_ERROR;
		}
		*address_cells = ac;
	}

	if (size_cells) {
		sz = fdt_size_cells (fdt, parentoffset);
		if (size_cells < 0) {
			printf ("%s(): parent #size-cells reading error %d\n",
				__func__, sz);
			return DT_RESULT_ERROR;
		}
		*size_cells = sz;
	}

	return DT_RESULT_OK;
}

enum dt_result_t
dt_helper_reg_extract (const void *buf, uint buflen, uint address_cells,
		       uint size_cells, struct dt_reg *dr, uint n_dr)
{
	const void *end;
	uint i;

	if (address_cells == 0 || address_cells > 2) {
		printf ("%s(): unexpected address_cells %u\n", __func__,
			address_cells);
		return DT_RESULT_ERROR;
	}

	if (size_cells > 2) {
		printf ("%s(): unexpected size_cells %u\n", __func__,
			size_cells);
		return DT_RESULT_ERROR;
	}

	end = buf + buflen;
	for (i = 0; i < n_dr && buf < end; i++) {
		dr[i].addr = 0;
		dr[i].len = 0;
		switch (address_cells) {
		case 1:
			dr[i].addr = fdt32_ld (buf);
			buf += sizeof (u32);
			break;
		case 2:
			dr[i].addr = fdt64_ld (buf);
			buf += sizeof (u64);
			break;
		default:
			return DT_RESULT_ERROR;
		}
		if (buf >= end)
			break;
		switch (address_cells) {
		case 1:
			dr[i].len = fdt32_ld (buf);
			buf += sizeof (u32);
			break;
		case 2:
			dr[i].len = fdt64_ld (buf);
			buf += sizeof (u64);
			break;
		default:
			return DT_RESULT_ERROR;
		}
	}

	return DT_RESULT_OK;
}

struct dt_pci_mcfg_iterator *
dt_pci_mcfg_iterator_alloc (void)
{
	struct dt_pci_mcfg_iterator *new_iter;

	new_iter = alloc (sizeof *new_iter);
	new_iter->current_pci_info = pci_info_list.next;

	return new_iter;
}

void
dt_pci_mcfg_iterator_free (struct dt_pci_mcfg_iterator *iter)
{
	free (iter);
}

bool
dt_pci_mcfg_get (struct dt_pci_mcfg_iterator *iter, u64 *base, u32 *seg_group,
		 u8 *bus_start, u8 *bus_end)
{
	struct dt_pci_info *cur;

	cur = iter->current_pci_info;
	if (!cur)
		return false;

	*base = cur->reg;
	*seg_group = cur->seg_domain;
	*bus_start = cur->bus_start;
	*bus_end = cur->bus_end;

	iter->current_pci_info = cur->next;

	return true;
}

bool
dt_pci_addr_translate (uint segment, phys_t addr, size_t len, bool is_io_addr,
		       phys_t *cpu_addr)
{
	struct dt_pci_info *p;
	struct dt_pci_range *r;

	LIST1_FOREACH (pci_info_list, p) {
		if (p->seg_domain != segment)
			continue;
		LIST1_FOREACH (p->ranges, r) {
			phys_t addr_end, r_start, r_end;
			u8 code;

			code = DT_PCI_RANGE_GET_SPACE_CODE (r->flags);
			if (is_io_addr) {
				if (!DT_PCI_CODE_IS_IO (code))
					continue;
			} else {
				if (!DT_PCI_CODE_IS_MM (code))
					continue;
			}

			addr_end = addr + len - 1;
			r_start = r->child_address;
			r_end = r_start + r->len - 1;
			if (addr < r_start || addr_end > r_end)
				continue;

			*cpu_addr = addr - r_start + r->parent_address;
			return true;
		}
	}

	return false;
}

static enum dt_result_t
dt_extract_pcie_info (const void *fdt)
{
	const char compat[] = "pci-host-ecam-generic";
	const void *reg, *bus_range, *ranges, *ranges_end, *domain, *status;
	struct dt_reg dt_pci_reg;
	int offset, reg_ac, reg_sz, lenp;
	int address_cells, size_cells;
	enum dt_result_t dt_error;

	/*
	 * It is not error if no PCI found from the beginning. It just means
	 * that there is no PCI or the running platform is not currently
	 * supported.
	 */
	dt_error = DT_RESULT_OK;
	offset = fdt_node_offset_by_compatible (fdt, -1, compat);
	while (offset >= 0) {
		dt_error = dt_helper_cur_node_reg_address_size_cell (fdt,
								     offset,
								     &reg_ac,
								     &reg_sz);
		if (dt_error) {
			printf ("%s(): cannot read register's"
				" address/size-cells\n",
				__func__);
			break;
		}

		/*
		 * It is considered error until we have all neccessary
		 * information. This requires reading various information from
		 * the devicetree.
		 */
		dt_error = DT_RESULT_ERROR;

		/*
		 * If status property does not exist, it is implicitlt "okay"
		 * according to the devicetree specification. We can continue
		 * normally.
		 */
		status = fdt_getprop (fdt, offset, "status", NULL);
		if (status) {
			int idx = fdt_stringlist_search (fdt, offset, "status",
							 "okay");
			/*
			 * There is only 1 string in status property. "okay"
			 * should be the only first string. If "okay" is not
			 * found, we skip the current node.
			 */
			if (idx != 0)
				goto next;
		}

		reg = fdt_getprop (fdt, offset, "reg", &lenp);
		if (!reg) {
			printf ("%s(): cannot get reg property\n", __func__);
			break;
		}

		/*
		 * TODO: each board can be different. The process of
		 * discovering ECAM register is likely to be board dependent.
		 * We have to come back later once we start testing on real
		 * hardware.
		 */
		dt_error = dt_helper_reg_extract (reg, lenp, reg_ac, reg_sz,
						  &dt_pci_reg, 1);
		if (dt_error) {
			printf ("%s(): extract PCI reg fails\n", __func__);
			break;
		}

		bus_range = fdt_getprop (fdt, offset, "bus-range", NULL);
		if (!bus_range) {
			printf ("%s(): cannot get bus-range property\n",
				__func__);
			break;
		}

		domain = fdt_getprop (fdt, offset, "linux,pci-domain", NULL);
		if (!domain) {
			printf ("%s(): cannot get linux,pci-domain property\n",
				__func__);
			break;
		}

		address_cells = fdt_address_cells (fdt, offset);
		if (address_cells < 0) {
			printf ("%s(): #address-cells reading error %d\n",
				__func__, address_cells);
			break;
		}

		size_cells = fdt_size_cells (fdt, offset);
		if (size_cells < 0) {
			printf ("%s(): #size-cells reading error %d\n",
				__func__, size_cells);
			break;
		}

		/*
		 * This is a sanity check. IEEE Std 1275-1994 specifies that
		 * #address-cells is 3 and #size-cells is 2. We currently don't
		 * expect other values.
		 */
		if (address_cells != 3 || size_cells != 2) {
			printf ("%s(): expect #address-cells 3 #size-cells 2"
				" but #address-cells %d #size-cells %d\n",
				__func__, address_cells, size_cells);
			break;
		}

		ranges = fdt_getprop (fdt, offset, "ranges", &lenp);
		if (!ranges) {
			printf ("%s(): cannot get ranges property\n",
				__func__);
			break;
		}
		ranges_end = ranges + lenp;

		dt_error = DT_RESULT_OK;

		struct dt_pci_info *new_pci_info;

		new_pci_info = alloc (sizeof *new_pci_info);
		LIST1_HEAD_INIT (new_pci_info->ranges);
		new_pci_info->reg = dt_pci_reg.addr;
		new_pci_info->len = dt_pci_reg.len;
		new_pci_info->bus_start = fdt32_ld (bus_range);
		new_pci_info->bus_end = fdt32_ld (bus_range + sizeof (u32));
		new_pci_info->seg_domain = fdt32_ld (domain);

		printf ("Scanning PCI segment %u resources\n",
			new_pci_info->seg_domain);

		while (ranges < ranges_end) {
			phys_t c_addr, p_addr;
			u64 len;
			u32 flags;
			flags = fdt32_ld (ranges);
			ranges += (1 * sizeof (u32));
			c_addr = fdt64_ld (ranges);
			ranges += (2 * sizeof (u32));
			/*
			 * The p_addr size is from parent's #address-cells.
			 * reg_ac is either 1 or 2 at this point. No need to
			 * check for other cases.
			 */
			p_addr = reg_ac == 1 ? fdt32_ld (ranges) :
					       fdt64_ld (ranges);
			ranges += (reg_ac * sizeof (u32));
			len = fdt64_ld (ranges);
			ranges += (2 * sizeof (u32));

			struct dt_pci_range *new_range;
			new_range = alloc (sizeof *new_range);
			new_range->child_address = c_addr;
			new_range->parent_address = p_addr;
			new_range->len = len;
			new_range->flags = flags;
			LIST1_ADD (new_pci_info->ranges, new_range);

			printf ("%s(): res 0x%llX->0x%llX code 0x%X\n",
				__func__, c_addr, p_addr,
				DT_PCI_RANGE_GET_SPACE_CODE (flags));
		}

		LIST1_ADD (pci_info_list, new_pci_info);
next:
		offset = fdt_node_offset_by_compatible (fdt, offset, compat);
	}

	return DT_RESULT_OK;
}

void
dt_init (void)
{
	struct bitvisor_devtree *bd;
	void *fdt_tmp;
	const char *model;
	phys_t bitvisor_devtree_addr;
	int error;
	enum dt_result_t dt_error;

	bitvisor_devtree_addr = uefi_param_ext_get_phys (&devtree_uuid);
	if (!bitvisor_devtree_addr)
		return;

	bd = mapmem_hphys (bitvisor_devtree_addr, sizeof *bd, 0);
	fdt_base = (ulong)bd->fdt_base;
	unmapmem (bd, sizeof *bd);

	/* Check header for actual fdt size */
	fdt_tmp = mapmem_hphys (fdt_base, sizeof (struct fdt_header), 0);
	error = fdt_check_header (fdt_tmp);
	if (error) {
		printf ("%s(): fdt_check_header() fail with %d\n", __func__,
			error);
		return;
	}
	fdt_size = fdt_totalsize (fdt_tmp);
	unmapmem (fdt_tmp, sizeof (struct fdt_header));

	fdt = mapmem_hphys (fdt_base, fdt_size, 0);
	if (!fdt) {
		printf ("%s(): cannot map fdt\n", __func__);
		return;
	}

	model = fdt_getprop (fdt, 0, "model", NULL);
	printf ("model: %s\n", model ? model : "(no model name)");

	dt_error = dt_extract_pcie_info (fdt);
	if (dt_error)
		panic ("%s(): dt_search_pcie() fails", __func__);
}
