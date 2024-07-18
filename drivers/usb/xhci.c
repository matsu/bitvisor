/*
 * Copyright (c) 2016 Igel Co., Ltd
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
 * 3. Neither the name of the copyright holder nor the names of its
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

/**
 * @file	drivers/usb/xhci.c
 * @brief	Generic xHCI para pass-through driver
 * @author	Ake Koomsin
 */
#include <core.h>
#include <core/dres.h>
#include <core/mm.h>
#include <pci.h>
#include <pci_conceal.h>
#include "usb.h"
#include "usb_device.h"
#include "usb_hook.h"
#include "xhci.h"

static const char driver_name[] = "xhci";
static const char driver_longname[] = "xHCI para pass-through driver 0.1";

typedef u64 (*data_handler) (struct xhci_host *host,
			     u64 data,
			     void *handler_data);

struct write_info {
	u64  data;
	u64  len;

	u64  *target;	       /* Pointer to a guest data */
	u8   *written;	       /* Pointer to a data for LOWER_WRITE check */

	data_handler handler;  /* Pointer to a function that patch the data */
	void *handler_data;
};

static void
fill_write_info (struct write_info *wr_info, u64 data, u64 len,
		 u64 *target, u8 *written, data_handler handler,
		 void *handler_data)
{
	wr_info->data	 = data;
	wr_info->len	 = len;
	wr_info->target  = target;
	wr_info->written = written;
	wr_info->handler = handler;
	wr_info->handler_data = handler_data;
}

static u8
handle_reg64 (struct xhci_host *host, u8 reg_offset,
	      struct write_info *wr_info)
{
	u8 valid = 1;

	/* Invalid access */
	if (reg_offset == 4 && wr_info->len == 8) {
		valid = 0;
		goto end;
	}

	u64 data_tmp = wr_info->data;

	switch (wr_info->len) {
	case 4:
		if (reg_offset == 0) {
			*(wr_info->target)  = data_tmp;
			*(wr_info->written) = LOWER_WRITE;
			valid = 0;
			break;
		}

		if (*(wr_info->written) != LOWER_WRITE) {
			valid = 0;
			break;
		}

		/* Create complete 64 bit data */
		data_tmp  = *(wr_info->target);
		data_tmp += (wr_info->data << 32);
		*(wr_info->written)= 0; /* Clear */

		/* Fallthrough */
	case 8:
	default:
		/* Patch write info */
		wr_info->data = data_tmp;
		wr_info->len  = sizeof (u64);

		*(wr_info->target) = data_tmp;

		if (wr_info->handler) {
			u64 new_data;
			new_data = wr_info->handler (host, data_tmp,
						     wr_info->handler_data);
			wr_info->data = new_data;
		}

		break;
	}

end:
	return valid;
}

/* Used in MMIO handler functions only */
#define CHECK_LEN_VALID (valid = (len != 4) ? 0 : 1)

static bool
hc_running (struct xhci_host *host)
{
	struct xhci_regs *regs;
	u32 usbcmd;
	u64 offset;

	regs = host->regs;
	offset = regs->opr_offset + OPR_USBCMD_OFFSET;
	dres_reg_read32 (regs->r, offset, &usbcmd);
	return !!(usbcmd & USBCMD_RUN);
}

static bool
hc_halted (struct xhci_host *host)
{
	struct xhci_regs *regs;
	u32 usbsts;
	u64 offset;

	regs = host->regs;
	offset = regs->opr_offset + OPR_USBSTS_OFFSET;
	dres_reg_read32 (regs->r, offset, &usbsts);
	return !!(usbsts & USBSTS_HCH);
}

static void
reflect_state_if_halted (struct xhci_host *host)
{
	if (host->hc_state == XHCI_HC_STATE_SHUTTING_DOWN && hc_halted (host))
		host->hc_state = XHCI_HC_STATE_HALTED;
}

void
xhci_update_vmm_hc_state (struct xhci_host *host)
{
	switch (host->hc_state) {
	case XHCI_HC_STATE_RUNNING:
		if (hc_running (host))
			break;
		host->hc_state = XHCI_HC_STATE_SHUTTING_DOWN;
		/* Fallthrough */
	case XHCI_HC_STATE_SHUTTING_DOWN:
		if (!hc_halted (host))
			break;
		host->hc_state = XHCI_HC_STATE_HALTED;
		/* Fallthrough */
	case XHCI_HC_STATE_HALTED:
		break;
	}
}

bool
xhci_hc_halted (struct xhci_host *host)
{
	reflect_state_if_halted (host);
	return host->hc_state == XHCI_HC_STATE_HALTED;
}

bool
xhci_hc_running (struct xhci_host *host)
{
	reflect_state_if_halted (host);
	return host->hc_state == XHCI_HC_STATE_RUNNING;
}

/* Update event ring and dev ctx if below situations:
 * - Just after the controller is halted.
 * - The controller is not halted and:
 *   if eint is true:
 *     - Event Interrupt occurs, or
 *     - Interrupt is not enabled.(for UEFI driver)
 *   else:
 *     - Interrupt is not enabled.(for UEFI driver) */
void
xhci_update_er_dev_ctx_eint (struct xhci_host *host, bool eint)
{
	struct xhci_regs *regs;
	u64 offset;
	u32 usbsts;
	switch (host->hc_state) {
	case XHCI_HC_STATE_RUNNING:
	case XHCI_HC_STATE_SHUTTING_DOWN:
		regs = host->regs;
		offset = regs->opr_offset + OPR_USBSTS_OFFSET;
		dres_reg_read32 (regs->r, offset, &usbsts);
		if (usbsts & USBSTS_HCH) {
			xhci_update_er_and_dev_ctx (host);
			host->hc_state = XHCI_HC_STATE_HALTED;
			break;
		}
		if ((eint && (usbsts & USBSTS_EINT)) || !host->intr_enable)
			xhci_update_er_and_dev_ctx (host);
		break;
	case XHCI_HC_STATE_HALTED:
		break;
	}
}

/* ---------- Start register access related helper functions ---------- */

static void
xhci_reg_read (const struct dres_reg *r, u64 offset, void *buf, uint len)
{
	dres_reg_read (r, offset, buf, len);
}

static void
xhci_reg_write (const struct dres_reg *r, u64 offset, void *buf, uint len)
{
	dres_reg_write (r, offset, buf, len);
}

/* ---------- End register access related helper functions ---------- */

/* ---------- Start capability related functions ---------- */

static u32
initial_cap_reg_read (struct xhci_host *host, const struct dres_reg *r,
		      phys_t field_offset)
{
	struct xhci_regs *regs = host->regs;
	u32 buf32;

	dres_reg_read32 (r, regs->cap_offset + field_offset, &buf32);

	switch (field_offset) {
	case CAP_CAPLENGTH_OFFSET: /* CAPLENGTH and HCIVERSION */
		dprintft (REG_DEBUG_LEVEL, "CAPLENGTH & HCIVERSION = %08X\n",
			  buf32);
		break;
	case CAP_HCIVERSION_OFFSET:
		dprintft (REG_DEBUG_LEVEL, "HCIVERSION = %04X\n", buf32);
		break;
	case CAP_HCSPARAMS1_OFFSET: /* HCSPARAMS1 */
		/*.
		 * Report max interrupts - 1 to the guest.
		 * BitVisor is going to use the last interrupter
		 * register set for event polling.
		 */
		buf32 = SET_HCSPARAMS1 (host->max_ports,
					host->usable_intrs,
					host->max_slots);
		dprintft (REG_DEBUG_LEVEL, "HCSPARAMS1 = %08X\n", buf32);
		break;
	case CAP_HCSPARAMS2_OFFSET: /* HCSPARAMS2 */
		dprintft (REG_DEBUG_LEVEL, "HCSPARAMS2 = %08X\n", buf32);
		break;
	case CAP_HCSPARAMS3_OFFSET: /* HCSPARAMS3 */
		dprintft (REG_DEBUG_LEVEL, "HCSPARAMS3 = %08X\n", buf32);
		break;
	case CAP_HCCPARAMS1_OFFSET: /* HCCPARAMS1 */
		dprintft (REG_DEBUG_LEVEL, "HCCPARAMS1 = %08X\n", buf32);
		break;
	case CAP_DBOFF_OFFSET: /* Doorbell registers offset */
		dprintft (REG_DEBUG_LEVEL, "DB offset = %08X\n", buf32);
		break;
	case CAP_RTSOFF_OFFSET: /* Runtime registers offset */
		dprintft (REG_DEBUG_LEVEL, "RTS offset = %08X\n", buf32);
		break;
	case CAP_HCCPARAMS2_OFFSET: /* HCCPARAMS2 */
		dprintft (REG_DEBUG_LEVEL, "HCCPARAMS2 = %08X\n", buf32);
		break;
	default:
		dprintft (REG_DEBUG_LEVEL, "Unknown register\n");
		break;
	}

	return buf32;
}

static void
xhci_cap_reg_read (struct xhci_host *host, phys_t offset, void *buf, uint len)
{
	struct xhci_regs *regs = host->regs;
	phys_t field_offset = offset - regs->cap_offset;
	dprintft (REG_DEBUG_LEVEL,
		  "Read Cap Reg, offset: 0x%x, size: %d\n", field_offset, len);
	memcpy (buf, &regs->cap_reg_copy[field_offset], len);
}

/* ---------- End capability related functions ---------- */


/* ---------- Start operation related functions ---------- */

static u64
create_h_cmd_ring (struct xhci_host *host, u64 g_cmd_ring, void *handler_data)
{
	dprintft (REG_DEBUG_LEVEL, "Guest cmd_ring: 0x%016llX\n",
		  host->g_data.cmd_ring);

	/* If CRR is one, ignore as writing means nothing */
	if (CRCTRL_GET_CRR (g_cmd_ring)) {
		return g_cmd_ring;
	}

	/* Record the flags */
	u64 new_cmd_ring = CRCTRL_GET_FLAGS (g_cmd_ring);

	/*
	 * It is possible that the write contains only flags.
	 * In that case, there is nothing to do.
	 */

	phys_t g_cmd_ring_addr = CRCTRL_GET_ADDR (g_cmd_ring);

	if (g_cmd_ring_addr) {
		ASSERT (!(PAGESIZE % XHCI_TRB_NBYTES));
		ASSERT (!(g_cmd_ring_addr % XHCI_TRB_NBYTES));
		host->g_data.cmd_ring_addr = g_cmd_ring_addr;

		/* First, unmap existing guest's cmd TRBs */
		for (int i = 0; i < (sizeof host->g_data.cmd_trbs /
				     sizeof host->g_data.cmd_trbs[0]); i++) {
			if (host->g_data.cmd_trbs[i]) {
				unmapmem (host->g_data.cmd_trbs[i], PAGESIZE);
				host->g_data.cmd_trbs[i] = NULL;
			}
		}

		host->g_data.cmd_n_trbs = XHCI_MAX_CMD_N_TRBS;
		host->cmd_n_trbs	= XHCI_MAX_CMD_N_TRBS;

		host->cmd_current_idx = 0;
		host->cmd_toggle = CRCTRL_GET_RCS (g_cmd_ring);

		/* Then, set up host's cmd TRBs */
		size_t trb_nbytes = host->g_data.cmd_n_trbs * XHCI_TRB_NBYTES;

		size_t cb_nbytes = host->g_data.cmd_n_trbs * sizeof (after_cb);

		if (!host->cmd_trbs) {
			phys_t *h_cmd_ring_addr = &host->cmd_ring_addr;

			host->cmd_after_cbs = zalloc (cb_nbytes);
			host->cmd_trbs	    = zalloc2_align (trb_nbytes,
							     h_cmd_ring_addr,
							     XHCI_ALIGN_64);
		}

		/* Clear host's cmd TRBs */
		memset (host->cmd_trbs, 0, trb_nbytes);
		if (!host->cmd_toggle) {
			/* Set Cycle bit of each TRB */
			unsigned int i;
			for (i = 0; i < host->g_data.cmd_n_trbs; i++)
				host->cmd_trbs[i].ctrl.value = 1;
		}

		dprintft (REG_DEBUG_LEVEL, "host cmd ring base: 0x%016llX\n",
			  host->cmd_ring_addr);

		/* Finally, update the new_cmd_ring */
		new_cmd_ring |= host->cmd_ring_addr;
	}

	return new_cmd_ring;
}

static u64
create_h_dev_ctx (struct xhci_host *host, u64 g_dev_ctx, void *handler_data)
{
	if (!g_dev_ctx) {
		dprintft (1, "Device context address is 0\n");
		return g_dev_ctx;
	}

	uint slots = xhci_get_max_slots (host);
	uint slots_plus_one = slots + 1; /* For scratchpad or index 0 */

	/* Plus one for the scratchpad */
	uint dev_ctx_phys_nbytes = sizeof (phys_t) * slots_plus_one;

	if (host->g_data.dev_ctx_array) {
		unmapmem (host->g_data.dev_ctx_array, dev_ctx_phys_nbytes);
	}

	host->g_data.dev_ctx_array = mapmem_as (host->usb_host->as_dma,
						g_dev_ctx, dev_ctx_phys_nbytes,
						MAPMEM_WRITE);

	if (host->dev_ctx_array) {
		return host->dev_ctx_addr;
	}

	/*
	 * Allocate dev_ctx_addr_array, the array of physical address.
	 * Note that the first address is the address of scratchpad.
	 */
	host->dev_ctx_array = zalloc2_align (dev_ctx_phys_nbytes,
					     &host->dev_ctx_addr,
					     XHCI_ALIGN_64);

	dprintft (REG_DEBUG_LEVEL, "Host's dev_ctx_addr: 0x%016llX\n",
		  host->dev_ctx_addr);

	/*
	 * Allocate dev_ctx_array, the array of virtual pointers to
	 * device context info. We need this for cleaning up operations.
	 */
	uint dev_ctx_nbytes  = sizeof (struct xhci_dev_ctx *) * slots_plus_one;
	host->dev_ctx	     = zalloc (dev_ctx_nbytes);

	host->g_data.dev_ctx = zalloc (dev_ctx_nbytes);

	/* With memset(), host_ctrl flag is HOST_CTRL_INITIAL */
	uint slot_meta_nbytes  = XHCI_SLOT_META_NBYTES * slots_plus_one;
	host->slot_meta        = zalloc (slot_meta_nbytes);

	host->g_data.slot_meta = zalloc (slot_meta_nbytes);

	uint i;
	for (i = 1; i <= slots; i++) {
		struct xhci_slot_meta *h_slot_meta, *g_slot_meta;
		h_slot_meta = &host->slot_meta[i];
		g_slot_meta = &host->g_data.slot_meta[i];

		spinlock_init (&h_slot_meta->lock);
		spinlock_init (&g_slot_meta->lock);

		/* This will be used during slot initialization */
		struct xhci_input_dev_ctx *input_ctx;
		input_ctx = zalloc2_align (XHCI_INPUT_DEV_CTX_NBYTES,
					   &h_slot_meta->input_dev_ctx_addr,
					   XHCI_ALIGN_64);

		h_slot_meta->input_ctx = input_ctx;

		uint j;
		for (j = 0; j < MAX_EP; j++) {
			struct xhci_ep_tr *h_ep_tr, *g_ep_tr;
			h_ep_tr = &h_slot_meta->ep_trs[j];
			g_ep_tr = &g_slot_meta->ep_trs[j];

			spinlock_init (&h_ep_tr->lock);
			spinlock_init (&g_ep_tr->lock);
		}
	}

	/* Don't forget scratchpad */
	if (!host->scratchpad_addr) {
		uint nbytes = sizeof (phys_t) * host->max_scratchpad;
		host->scratchpad_array = zalloc2_align (nbytes,
							&host->scratchpad_addr,
							XHCI_ALIGN_64);

		uint i;
		for (i = 0; i < host->max_scratchpad; i++) {
			zalloc2 (host->scratchpad_pagesize,
				 &host->scratchpad_array[i]);
		}
	}

	host->dev_ctx_array[0] = host->scratchpad_addr;

	return host->dev_ctx_addr;
}

static phys_t get_host_erdp (struct xhci_host *host,
			     phys_t g_erst_dq_ptr,
			     void *handler_data);

struct erst_data {
	struct xhci_erst_data *h_erst_data;
	struct xhci_erst_data *g_erst_data;
};

static void
take_control_erst (struct xhci_host *host)
{
	struct xhci_regs *regs = host->regs;
	const struct mm_as *const as = host->usb_host->as_dma;
	const struct dres_reg *r = regs->r;

	struct erst_data erst_data;
	struct xhci_erst_data *g_erst_data, *h_erst_data;

	phys_t erstsz_offset, erdp_offset, erst_offset;
	u64 erdp;

	uint i;
	for (i = 0; i < host->usable_intrs; i++) {
		g_erst_data = &host->g_data.erst_data[i];
		h_erst_data = &host->erst_data[i];

		erst_data.h_erst_data = h_erst_data;
		erst_data.g_erst_data = g_erst_data;

		if (!g_erst_data->erst_size) {
			continue;
		}

		xhci_create_shadow_erst (as, h_erst_data, g_erst_data);

		erdp_offset = INTR_REG_OFFSET (regs, i) + RTS_ERDP_OFFSET;
		erst_offset = INTR_REG_OFFSET (regs, i) + RTS_ERSTBA_OFFSET;

		erdp = get_host_erdp (host, g_erst_data->erst_dq_ptr,
				      &erst_data);
		dres_reg_write64 (r, erdp_offset, erdp);
		dres_reg_write64 (r, erst_offset, h_erst_data->erst_addr);
	}

	/* Now the last interrupt register set */
	i = host->max_intrs - 1;
	h_erst_data = &host->erst_data[i];

	xhci_initialize_event_ring (h_erst_data);

	u32 erstsz;

	erstsz_offset = INTR_REG_OFFSET (regs, i) + RTS_ERSTSZ_OFFSET;
	erdp_offset = INTR_REG_OFFSET (regs, i) + RTS_ERDP_OFFSET;
	erst_offset = INTR_REG_OFFSET (regs, i) + RTS_ERSTBA_OFFSET;

	dres_reg_read32 (r, erstsz_offset, &erstsz);
	erstsz = (erstsz & ERSTSZ_PRESERVE_MASK) | h_erst_data->erst_size;
	dres_reg_write32 (r, erstsz_offset, erstsz);
	dres_reg_write64 (r, erdp_offset, h_erst_data->erst_dq_ptr);
	dres_reg_write64 (r, erst_offset, h_erst_data->erst_addr);
}

static u8
handle_usb_cmd_write (struct xhci_host *host, u64 cmd)
{
	struct xhci_regs *regs = host->regs;

	/* The guest wants xHC to save its state */
	if (cmd & USBCMD_CSS) {
		/*
		 * Save the latest event dequeue pointer for
		 * the last interrupt set register
		 */
		uint i = host->max_intrs - 1;
		struct xhci_erst_data *h_erst_data = &host->erst_data[i];

		u64 erdp_offset = INTR_REG_OFFSET (regs, i) + RTS_ERDP_OFFSET;
		dres_reg_read64 (regs->r, erdp_offset,
				 &h_erst_data->erst_dq_ptr);
	}

	/* The guest wants xHC to restore its state */
	if (cmd & USBCMD_CRS) {
		panic ("CRS handling is not implemented yet.");
	}

	if (cmd & USBCMD_RUN) {
		if (!xhci_hc_running (host)) {
			xhci_release_data (host);

			host->hc_state = XHCI_HC_STATE_RUNNING;
			take_control_erst (host);
		}
	} else {
		if (xhci_hc_running (host)) {
			host->hc_state = XHCI_HC_STATE_SHUTTING_DOWN;
			xhci_update_er_and_dev_ctx (host);
		}
	}

	host->intr_enable = !!(cmd & USBCMD_INTE);

	return 1;
}

static void
check_for_error_status (struct xhci_host *host, u32 status)
{
	if ((status & USBSTS_HSE) ||
	    (status & USBSTS_SRE) ||
	    (status & USBSTS_HCE)) {

		if (status & USBSTS_HSE) {
			dprintf (0, "Host System Error occurs\n");
		}

		if (status & USBSTS_SRE) {
			dprintf (0, "Save/Restore Error occurs\n");
		}

		if (status & USBSTS_HCE) {
			dprintf (0, "Host Controller Error occurs\n");
		}
	}
}

static void
xhci_opr_reg_read (struct xhci_host *host, const struct dres_reg *r,
		   phys_t offset, void *buf, uint len)
{
	phys_t field_offset = offset - host->regs->opr_offset;

	u8 valid = 1;

	u64 buf64 = 0;
	xhci_reg_read (r, offset, &buf64, len);

	switch (field_offset) {
	case OPR_USBCMD_OFFSET: /* USB command */
		CHECK_LEN_VALID;

		break;
	case OPR_USBSTS_OFFSET: /* USB status */
		CHECK_LEN_VALID;
		check_for_error_status (host, (u32)buf64);

		break;
	case OPR_PAGESIZE_OFFSET: /* Page size */
		CHECK_LEN_VALID;
		buf64 = host->opr_pagesize;

		break;
	case OPR_DNCTRL_OFFSET: /* Device notification control */
		CHECK_LEN_VALID;

		break;
	case OPR_CRCTRL_OFFSET: /* Command ring control pointer */
		break;
	case OPR_CRCTRL_OFFSET + 4:
		CHECK_LEN_VALID;

		break;
	case OPR_DCBAAP_OFFSET: /* Dev CTX base address pointer */
		buf64 = host->g_data.dev_ctx_addr;

		break;
	case OPR_DCBAAP_OFFSET + 4:
		CHECK_LEN_VALID;
		buf64 = host->g_data.dev_ctx_addr >> 32;

		break;
	case OPR_CONFIG_OFFSET: /* Configure */
		CHECK_LEN_VALID;

		break;
	default:
		/* PORTSC */
		if (field_offset >= OPR_PORTREG_OFFSET &&
		    (field_offset & 0xF) == 0) {
			u32 port_offset = field_offset - OPR_PORTREG_OFFSET;
			u32 portno = port_offset / OPR_PORTREG_NBYTES;

			u16 status = (OPR_PORTSC_GET_CSC (buf64) << 1) |
				     (OPR_PORTSC_GET_PRC (buf64) << 1) |
				     OPR_PORTSC_GET_CCS (buf64);

			if (host->portsc[portno] == status) {
				break;
			}

			usb_sc_lock (host->usb_host);
			host->portsc[portno] = status;
			handle_connect_status (host->usb_host,
					       portno, status);
			usb_sc_unlock (host->usb_host);
		}

		break;
	}

	if (valid) {
		memcpy (buf, &buf64, len);
	}
}

static void
xhci_opr_reg_write (struct xhci_host *host, const struct dres_reg *r,
		    phys_t offset, void *buf, uint len)
{
	phys_t field_offset = offset - host->regs->opr_offset;

	u8 reg_offset = 0;

	u8 valid   = 1;
	u8 write64 = 0;

	u64 buf64 = 0;
	memcpy (&buf64, buf, len);

	struct write_info wr_info = {buf64, len, NULL, NULL, NULL, NULL};

	switch (field_offset) {
	case OPR_USBCMD_OFFSET: /* USB command */
		buf64 &= 0xFFFFFFFF;
		valid = handle_usb_cmd_write (host, buf64);

		break;
	case OPR_USBSTS_OFFSET: /* USB status */
	case OPR_PAGESIZE_OFFSET: /* Page size */
	case OPR_DNCTRL_OFFSET: /* Device notification control */
		CHECK_LEN_VALID;

		break;
	case OPR_CRCTRL_OFFSET: /* Command ring control lower part */
	case OPR_CRCTRL_OFFSET + 4:
		write64 = 1;
		reg_offset = field_offset - OPR_CRCTRL_OFFSET;

		fill_write_info (&wr_info, buf64, len,
				 &host->g_data.cmd_ring,
				 &host->g_data.cmd_ring_written,
				 create_h_cmd_ring, NULL);

		break;
	case OPR_DCBAAP_OFFSET: /* Dev CTX base address array pointer */
	case OPR_DCBAAP_OFFSET + 4:
		write64 = 1;
		reg_offset = field_offset - OPR_DCBAAP_OFFSET;

		fill_write_info (&wr_info, buf64, len,
				 &host->g_data.dev_ctx_addr,
				 &host->g_data.dev_ctx_addr_written,
				 create_h_dev_ctx, NULL);

		break;
	case OPR_CONFIG_OFFSET: /* Configure */
		CHECK_LEN_VALID;
		host->max_slots_enabled = (valid) ?
					  (u8)buf64 :
					  host->max_slots_enabled;
		dprintft (REG_DEBUG_LEVEL, "Max slot enabled: %u\n",
			  host->max_slots_enabled);

		break;
	default:
		/* PORTSC */
		if (field_offset >= OPR_PORTREG_OFFSET &&
		    (field_offset & 0xF) == 0) {
			u32 port_offset = field_offset - OPR_PORTREG_OFFSET;
			u32 portno	= port_offset / OPR_PORTREG_NBYTES;

			handle_port_reset (host->usb_host, portno,
					   buf64, 4);
		}

		break;
	}

	if (write64) {
		offset -= reg_offset;
		valid = handle_reg64 (host, reg_offset, &wr_info);
	}

	if (valid)
		xhci_reg_write (r, offset, &wr_info.data, wr_info.len);
}

/* ---------- End operation related functions ---------- */

/* ---------- Start runtime related functions ---------- */

static phys_t
get_host_erdp (struct xhci_host *host,
	       phys_t g_erst_dq_ptr,
	       void *handler_data)
{
	struct erst_data *er_dq_data = (struct erst_data *)handler_data;

	struct xhci_erst_data *h_erst_data = er_dq_data->h_erst_data;
	struct xhci_erst_data *g_erst_data = er_dq_data->g_erst_data;

	phys_t new_er_dq_ptr = g_erst_dq_ptr;

	if (xhci_hc_halted (host))
		return h_erst_data->erst_dq_ptr;

	/* Calculate new dequeue pointer */
	struct xhci_erst *g_erst = g_erst_data->erst;

	u8 found = 0;

	uint i;
	for (i = 0; i < g_erst_data->erst_size; i++) {
		/*
		 * 1) Find out the current segment
		 * 2) Update h_erst_data->erst_dq_ptr with proper flags
		 */

		phys_t offset = g_erst_data->erst_dq_ptr - g_erst[i].trb_addr;

		if (offset < g_erst[i].n_trbs * XHCI_TRB_NBYTES) {
			/* We found the current segment */
			found = 1;

			new_er_dq_ptr = h_erst_data->erst[i].trb_addr + offset;

			h_erst_data->erst_dq_ptr = new_er_dq_ptr;
			break;
		}
	}

	if (!found)
		dprintft (0, "Fatal error in %s()\n", __func__);

	return new_er_dq_ptr;
}

static void
xhci_rts_reg_read (struct xhci_host *host, const struct dres_reg *r,
		   phys_t offset, void *buf, uint len)
{
	u8 valid = 1;

	u64 buf64 = 0;
	xhci_reg_read (r, offset, &buf64, len);

	/* From here, traverse to the target erst location */
	phys_t o = offset - host->regs->rts_offset;

	if (o < RTS_INTRS_OFFSET)
		goto end;

	/* Don't forget to subtract with RTS_INTRS_OFFSET */
	o -= RTS_INTRS_OFFSET;

	/* Get the index */
	uint index = o / XHCI_INTR_REG_NBYTES;

	/* The last interrupt register set belongs to BitVisor */
	if (index == host->max_intrs - 1) {
		valid = 0;
		goto end;
	}

	/* Get field offset */
	phys_t field_offset = o - (index * XHCI_INTR_REG_NBYTES);

	struct xhci_erst_data *g_erst_data = &host->g_data.erst_data[index];

	switch (field_offset) {
	case RTS_IMAN_OFFSET:
		CHECK_LEN_VALID;
		dprintft (REG_DEBUG_LEVEL, "Reading IMAN: 0x%X\n", buf64);

		break;
	case RTS_IMOD_OFFSET:
		CHECK_LEN_VALID;
		dprintft (REG_DEBUG_LEVEL, "Reading IMOD: 0x%X\n", buf64);

		break;
	case RTS_ERSTSZ_OFFSET:
		CHECK_LEN_VALID;
		buf64 = g_erst_data->erst_size;
		dprintft (REG_DEBUG_LEVEL, "Reading ERSTSZ: %u\n", buf64);

		break;
	case RTS_ERSTBA_OFFSET:
		buf64 = g_erst_data->erst_addr;

		break;
	case RTS_ERSTBA_OFFSET + 4:
		CHECK_LEN_VALID;
		buf64 = g_erst_data->erst_addr >> 32;

		break;
	case RTS_ERDP_OFFSET:
		buf64 = g_erst_data->erst_dq_ptr;

		break;
	case RTS_ERDP_OFFSET + 4:
		CHECK_LEN_VALID;
		buf64 = g_erst_data->erst_dq_ptr >> 32;

		break;
	default:
		valid = 0;
		dprintft (REG_DEBUG_LEVEL, "Read RTS??? field offset: 0x%X\n",
			  field_offset);

		break;
	}
end:
	if (valid) {
		memcpy (buf, &buf64, len);
	}
}

static void
xhci_rts_reg_write (struct xhci_host *host, const struct dres_reg *r,
		    phys_t offset, void *buf, uint len)
{
	u8 reg_offset = 0;

	u8 valid = 1;
	u8 write64 = 0;

	u64 buf64 = 0;
	memcpy (&buf64, buf, len);

	struct write_info wr_info = {buf64, len, NULL, NULL, NULL, NULL};

	/* From here, traverse to the target erst location */
	phys_t o = offset - host->regs->rts_offset;

	if (o < RTS_INTRS_OFFSET)
		goto end;

	/* Don't forget to subtract with RTS_INTRS_OFFSET */
	o -= RTS_INTRS_OFFSET;

	/* Get the index */
	uint index = o / XHCI_INTR_REG_NBYTES;

	/* The last interrupt register set belongs to BitVisor */
	if (index == host->max_intrs - 1) {
		valid = 0;
		goto end;
	}

	/* Get field offset */
	phys_t field_offset = o - (index * XHCI_INTR_REG_NBYTES);

	struct xhci_erst_data *g_erst_data = &host->g_data.erst_data[index];
	struct xhci_erst_data *h_erst_data = &host->erst_data[index];

	struct erst_data erst_data = {h_erst_data, g_erst_data};

	if (!xhci_hc_halted (host)) {
		if (field_offset != RTS_IMAN_OFFSET &&
		    field_offset != RTS_IMOD_OFFSET &&
		    field_offset != RTS_ERDP_OFFSET &&
		    field_offset != RTS_ERDP_OFFSET + 4) {
			dprintft (1, "Ignore invalid write. Offset: 0x%X\n",
				  field_offset);
			valid = 0;
			goto end;
		}
	}

	switch (field_offset) {
	case RTS_IMAN_OFFSET:
		CHECK_LEN_VALID;

		dprintft (REG_DEBUG_LEVEL, "Write to IMAN: 0x%X\n", buf64);

		break;
	case RTS_IMOD_OFFSET:
		CHECK_LEN_VALID;

		dprintft (REG_DEBUG_LEVEL, "Writing IMOD: 0x%X\n", buf64);

		break;
	case RTS_ERSTSZ_OFFSET:
		CHECK_LEN_VALID;
		/*
		 * Writing zero to the primary interrupter results in
		 * undefined behaviors.
		 */
		if (buf64 == 0x0 && index == 0) {
			valid = 0;
			break;
		}

		g_erst_data->erst_size = (valid) ?
					 (u16)buf64 :
					 g_erst_data->erst_size;
		dprintft (REG_DEBUG_LEVEL, "Writing ERST size: %u\n", buf64);

		break;
	case RTS_ERSTBA_OFFSET:
	case RTS_ERSTBA_OFFSET + 4:
		write64 = 1;
		reg_offset = field_offset - RTS_ERSTBA_OFFSET;
		xhci_reset_erst_current (g_erst_data);
		fill_write_info (&wr_info, buf64, len,
				 &g_erst_data->erst_addr,
				 &g_erst_data->erst_addr_written,
				 NULL, NULL);

		break;
	case RTS_ERDP_OFFSET:
	case RTS_ERDP_OFFSET + 4:
		write64 = 1;
		reg_offset = field_offset - RTS_ERDP_OFFSET;

		fill_write_info (&wr_info, buf64, len,
				 &g_erst_data->erst_dq_ptr,
				 &g_erst_data->erst_dq_ptr_written,
				 get_host_erdp, &erst_data);

		break;
	default:
		valid = 0;
		dprintft (REG_DEBUG_LEVEL, "Write RTS??? field offset: 0x%X\n",
			  field_offset);

		break;
	}

	if (write64) {
		offset -= reg_offset;
		valid = handle_reg64 (host, reg_offset, &wr_info);
	}

end:
	if (valid)
		xhci_reg_write (r, offset, &wr_info.data, wr_info.len);
}

/* ---------- End runtime related functions ---------- */

/* ---------- Start doorbell related functions ---------- */

static struct xhci_trb *
get_g_cmd_trb (struct xhci_host *host, u32 idx)
{
	phys_t g_cmd_ring_addr = host->g_data.cmd_ring_addr;
	phys_t g_cmd_trb_addr = g_cmd_ring_addr + idx * XHCI_TRB_NBYTES;
	unsigned int page_index;
	page_index = g_cmd_trb_addr / PAGESIZE - g_cmd_ring_addr / PAGESIZE;
	ASSERT (page_index < (sizeof host->g_data.cmd_trbs /
			      sizeof host->g_data.cmd_trbs[0]));
	struct xhci_trb *map = host->g_data.cmd_trbs[page_index];
	if (!map) {
		map = mapmem_as (host->usb_host->as_dma,
				 g_cmd_trb_addr - g_cmd_trb_addr % PAGESIZE,
				 PAGESIZE, 0);
		host->g_data.cmd_trbs[page_index] = map;
	}
	return &map[g_cmd_trb_addr % PAGESIZE / XHCI_TRB_NBYTES];
}

static void
handle_cmd_write (struct xhci_host *host)
{
	/* The copied Link TRB will get patched by process_cmd_trb() */
	struct xhci_trb *h_cmd_trbs = host->cmd_trbs;

	struct xhci_trb *g_current_trb, *h_current_trb;

	struct xhci_trb tmp_trb;

	while (g_current_trb = get_g_cmd_trb (host, host->cmd_current_idx),
	       XHCI_TRB_GET_C (g_current_trb) == host->cmd_toggle) {

		h_current_trb = &h_cmd_trbs[host->cmd_current_idx];

		tmp_trb = *g_current_trb;

		u8 toggle = xhci_process_cmd_trb (host,
						  &tmp_trb,
						  host->cmd_current_idx);

		*h_current_trb = tmp_trb;

		host->cmd_current_idx++;

		if (toggle) {
			host->cmd_current_idx  = 0;
			host->cmd_toggle      ^= 1;
		}
	}
}

static void
set_slot_owner (struct xhci_host *host, uint slot_id)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];

	struct xhci_dev_ctx *h_dev_ctx = host->dev_ctx[slot_id];

	u64 portno = xhci_get_portno (h_dev_ctx);

	struct usb_device *dev = get_device_by_port (host->usb_host,
						     portno);

	if (!dev) {
		dprintf(0, "dev is NULL in %s()\n", __FUNCTION__);
		return;
	}

	if (dev->ctrl_by_host) {
		h_slot_meta->host_ctrl = HOST_CTRL_YES;
		xhci_set_no_data_trb_hook (host, dev);
	} else {
		/*
		 * No USB device driver takes control this slot ID.
		 * Hand back all Endpoints to the guest.
		 */
		xhci_hand_eps_back_to_guest (host, slot_id);
		h_slot_meta->host_ctrl = HOST_CTRL_NO;
	}
}

static void
handle_slot_write (struct xhci_host *host, uint slot_id, uint ep_no)
{
	struct xhci_slot_meta *h_slot_meta = &host->slot_meta[slot_id];
	struct xhci_ep_tr     *h_ep_tr	   = &h_slot_meta->ep_trs[ep_no];

	spinlock_lock (&h_ep_tr->lock);

	struct usb_request_block *g_urb;
	g_urb = xhci_construct_gurbs (host, slot_id, ep_no);

	while (g_urb) {
		struct usb_request_block *h_urb = xhci_shadow_g_urb (g_urb);

		u8 ret = usb_hook_process (host->usb_host,
					   h_urb,
					   USB_HOOK_REQUEST);

		if (ret == USB_HOOK_DISCARD) {
			dprintft (1, "slot: %u ep_no: %u Request discarded\n",
				  slot_id, ep_no);

			struct usb_request_block *next_urb = g_urb->link_next;

			delete_urb_xhci (h_urb);
			delete_urb_xhci (g_urb);

			g_urb = next_urb;
			continue;
		}

		xhci_append_h_urb_to_ep (h_urb, host, slot_id, ep_no);

		/* Set URB status to URB_STATUS_RUN */
		g_urb->status = URB_STATUS_RUN;
		h_urb->status = URB_STATUS_RUN;

		g_urb = g_urb->link_next;
	}

	spinlock_unlock (&h_ep_tr->lock);
}

static void
xhci_db_reg_write (struct xhci_host *host, const struct dres_reg *r,
		   phys_t offset, void *buf)
{
	phys_t field_offset = offset - host->regs->db_offset;
	union xhci_db_reg *req = buf;
	u64 slot_id = field_offset / XHCI_DB_REG_NBYTES;
	uint ep_no;

	switch (slot_id) {
	case 0:
		handle_cmd_write (host);
		break;
	default:
		/* Our EP starts from 0 */
		ep_no = req->reg.db_target - 1;

		if (ep_no != 0 &&
		    host->slot_meta[slot_id].host_ctrl == HOST_CTRL_INITIAL) {
			set_slot_owner (host, slot_id);
		}

		handle_slot_write (host, slot_id, ep_no);
		break;
	}

	dres_reg_write32 (r, offset, req->raw_val);
}

/* ---------- End doorbell related functions ---------- */

#undef CHECK_LEN_VALID

static inline u8
range_check (u64 acc_start, u64 acc_end, u64 area_start, u64 area_end)
{
	return acc_start >= area_start && acc_end <= area_end;
}

#define RANGE_CHECK_CAP						\
	range_check (acc_start, acc_end,			\
		     regs->cap_start, regs->cap_end)
#define RANGE_CHECK_OPR						\
	range_check (acc_start, acc_end,			\
		     regs->opr_start, regs->opr_end)
#define RANGE_CHECK_RTS						\
	range_check (acc_start, acc_end,			\
		     regs->rts_start, regs->rts_end)
#define RANGE_CHECK_DB						\
	range_check (acc_start, acc_end,			\
		     regs->db_start,  regs->db_end)

static enum dres_reg_ret_t
xhci_reg_handler (const struct dres_reg *r, void *handle, phys_t offset,
		  bool wr, void *buf, uint len)
{
	struct xhci_data *xhci_data = (struct xhci_data *)handle;
	struct xhci_host *host = xhci_data->host;
	struct xhci_regs *regs = host->regs;

	spinlock_lock (&host->sync_lock);

	xhci_update_er_dev_ctx_eint (host, false);

	phys_t acc_start = offset;
	phys_t acc_end	 = acc_start + len;

	if (len != 4 && len != 8) {
		if (!RANGE_CHECK_CAP)
			goto end;
	}

	if (RANGE_CHECK_CAP) {

		if (!wr)
			xhci_cap_reg_read (host, offset, buf, len);

	} else if (RANGE_CHECK_OPR) {

		if (wr)
			xhci_opr_reg_write (host, r, offset, buf, len);
		else
			xhci_opr_reg_read (host, r, offset, buf, len);

	} else if (RANGE_CHECK_RTS) {

		if (wr)
			xhci_rts_reg_write (host, r, offset, buf, len);
		else
			xhci_rts_reg_read (host, r, offset, buf, len);

	} else if (RANGE_CHECK_DB) {

		/*
		 * We also need to check whether the guest has started the xHC.
		 * If not, ignore the access which is likely from firmwares,
		 * UEFI applications, or APIC programming from the guest OS.
		 */
		if (wr && len == 4 && !xhci_hc_halted (host))
			xhci_db_reg_write (host, r, offset, buf);

	} else if (acc_start >= regs->ext_offset) {

		struct xhci_ext_cap *blacklist_ext_cap;
		blacklist_ext_cap = regs->blacklist_ext_cap;

		u8 condition;
		u8 deny_wr = 0;
		while (blacklist_ext_cap) {
			/* With in the range */
			condition = acc_start >= blacklist_ext_cap->start &&
				    acc_start < blacklist_ext_cap->end;

			/* acc_end happens to be more than the start addr */
			condition = condition ||
				    acc_end > blacklist_ext_cap->start;

			if (condition) {
				deny_wr = 1;
				break;
			}

			blacklist_ext_cap = blacklist_ext_cap->next;
		}

		if (wr && !deny_wr)
			xhci_reg_write (r, offset, buf, len);
		else if (!wr)
			xhci_reg_read (r, offset, buf, len);
	}
end:
	spinlock_unlock (&host->sync_lock);
	return DRES_REG_RET_DONE;
}

#undef RANGE_CHECK_CAP
#undef RANGE_CHECK_OPR
#undef RANGE_CHECK_RTS
#undef RANGE_CHECK_DB

static void
unreghook (struct xhci_data *xhci_data)
{
	if (xhci_data->enabled) {
		struct dres_reg *r = xhci_data->host->regs->r;
		dres_reg_unregister_handler (r);
		dres_reg_free (r);
		xhci_data->enabled = 0;
	}
}

static void
reghook (struct xhci_data *xhci_data, struct pci_bar_info *bar)
{
	if (bar->type == PCI_BAR_INFO_TYPE_NONE ||
	    bar->type == PCI_BAR_INFO_TYPE_IO) {
		return;
	}

	unreghook (xhci_data);

	xhci_data->enabled = 0;

	struct xhci_regs *regs = xhci_data->host->regs;

	regs->iobase  = bar->base;
	regs->map_len = bar->len;
	regs->r = dres_reg_alloc (bar->base, bar->len, DRES_REG_TYPE_MM,
				  pci_dres_reg_translate,
				  xhci_data->host->dev, 0);
	if (!regs->r)
		panic ("Cannot dres_reg_alloc() xHC registers");

	enum dres_err_t err = dres_reg_register_handler (regs->r,
							 xhci_reg_handler,
							 xhci_data);
	if (err != DRES_ERR_NONE)
		panic ("Cannot dres_reg_register_handler() xHC registers");

	xhci_data->enabled = 1;
}

static int
xhci_config_read (struct pci_device *pci_device, u8 iosize, u16 offset,
		  union mem *data)
{
	return CORE_IO_RET_DEFAULT;
}

static int
xhci_config_write (struct pci_device *pci_device, u8 iosize, u16 offset,
		   union mem *data)
{
	/* XXX: A hack as guest OS write to BAR with iosize of 1.
	 * This would make pci_get_modifying_bar_info() calls panic().
	 */
	if (iosize != 4 &&
	    offset >= PCI_CONFIG_BASE_ADDRESS0 &&
	    offset <= PCI_CONFIG_BASE_ADDRESS1 + 0x4) {
		goto no_write;
	}

	struct xhci_data *xhci_data = pci_device->host;
	struct xhci_host *host = xhci_data->host;

	struct pci_bar_info bar_info;

	int i = pci_get_modifying_bar_info (pci_device,
					    &bar_info,
					    iosize,
					    offset,
					    data);

	if (i == 0 && host->regs->iobase != bar_info.base) {
		reghook (xhci_data, &bar_info);

		printf ("Setting up new XHCI handlers done\n");
	}

	return CORE_IO_RET_DEFAULT;
no_write:
	return CORE_IO_RET_DONE;
}

static void
xhci_construct_blacklist_ext_cap (struct xhci_regs *regs)
{
	const struct dres_reg *r = regs->r;
	struct xhci_ext_cap_reg ext_cap_reg;

	/* ext_offset is the shifted value already */
	u64 next_cap_offset = regs->ext_offset;

	phys_t current_offset = 0;

	struct xhci_ext_cap **blacklist_ext_cap;
	blacklist_ext_cap = &regs->blacklist_ext_cap;

	while (next_cap_offset) {
		current_offset += next_cap_offset;

		dres_reg_read32 (r, current_offset, &ext_cap_reg);

		u64 total_bytes = 0;

		dprintft (REG_DEBUG_LEVEL, "Capability ID: %d\n",
			  ext_cap_reg.cap_id);

		switch (ext_cap_reg.cap_id) {
		case XHCI_EXT_CAP_IO_VIRT:
			total_bytes = XHCI_EXT_CAP_IO_VIRT_NBYTES;
			break;
		case XHCI_EXT_CAP_LOCAL_MEM:
			/* (base + 0x4) contains size info in 1KB blocks */
			dres_reg_read32 (r, current_offset + 0x4,
					 &total_bytes);
			total_bytes *= 1024;
			/* First 8 bytes of the register */
			total_bytes += sizeof (u64);
			break;
		case XHCI_EXT_CAP_USB_DEBUG:
			total_bytes = XHCI_EXT_CAP_USB_DEBUG_NBYTES;
			break;
		default:
			break;
		}

		switch (ext_cap_reg.cap_id) {
		case XHCI_EXT_CAP_IO_VIRT:
		case XHCI_EXT_CAP_LOCAL_MEM:
		case XHCI_EXT_CAP_USB_DEBUG:
			*blacklist_ext_cap = zalloc (XHCI_EXT_CAP_NBYTES);

			(*blacklist_ext_cap)->type  = ext_cap_reg.cap_id;
			(*blacklist_ext_cap)->start = current_offset;
			(*blacklist_ext_cap)->end   = current_offset +
						      total_bytes;

			blacklist_ext_cap = &(*blacklist_ext_cap)->next;
			break;
		default:
			break;
		}

		/* Need to shift according to the specification */
		next_cap_offset = ext_cap_reg.next_cap << XHCI_EXT_CAP_SHIFT;
	}
}

static void
xhci_set_reg_offsets (struct xhci_regs *regs, struct xhci_cap_reg *cap_reg)
{
	/* Capability registers offset is zero */
	regs->cap_offset = 0;

	/* Read Operational registers offset */
	regs->opr_offset = cap_reg->cap_length;
	dprintft (REG_DEBUG_LEVEL, "Operational registers offset: 0x%X\n",
		  regs->opr_offset);

	/* Read Doorbell registers offset */
	regs->db_offset = cap_reg->db_offset;
	dprintft (REG_DEBUG_LEVEL, "Doorbell registers offset: 0x%X\n",
		  regs->db_offset);

	/* Read Runtime registers offset */
	regs->rts_offset = cap_reg->rts_offset;
	dprintft (REG_DEBUG_LEVEL, "Runtime registers offset: 0x%X\n",
		  regs->rts_offset);

	/* Read xHCI extended capabilities pointer */
	regs->ext_offset = CAP_CPARAM1_GET_EXT_OFFSET (cap_reg->hc_cparams1);
	dprintft (REG_DEBUG_LEVEL, "xECP offset: 0x%X\n",
		  regs->ext_offset);
}

static void
xhci_set_reg_range (struct xhci_host *host)
{
	struct xhci_regs *regs = host->regs;

	regs->cap_start = regs->cap_offset;
	regs->cap_end	= regs->cap_start + regs->opr_offset;

	regs->opr_start = regs->opr_offset;
	regs->opr_end	= regs->opr_start + OPR_PORTREG_OFFSET +
			  (OPR_PORTREG_NBYTES * host->max_ports);

	regs->rts_start = regs->rts_offset;
	regs->rts_end	= regs->rts_start + RTS_INTRS_OFFSET +
			  (XHCI_INTR_REG_NBYTES * host->max_intrs);

	/*
	 * 'max_slots' means 1 to max_slots. Don't forget that
	 * there is a slot 0 for xHCI commands.
	 */
	regs->db_start	= regs->db_offset;
	regs->db_end	= regs->db_start +
			  (XHCI_DB_REG_NBYTES * (host->max_slots + 1));
}

static void
xhci_set_erst_data (struct xhci_host *host)
{
	/*
	 * Allocate memory for erst_data. Zero memset means current_seg,
	 * and current_idx are zero also.
	 */
	size_t erst_data_nbytes = XHCI_ERST_DATA_NBYTES * host->max_intrs;
	host->g_data.erst_data	= zalloc (erst_data_nbytes);
	host->erst_data		= zalloc (erst_data_nbytes);

	uint i;
	for (i = 0; i < host->max_intrs; i++) {
		/* Set current toggle to 1 (Default start) for all erst_data */
		host->erst_data[i].current_toggle = 1;
		host->g_data.erst_data[i].current_toggle = 1;
	}

	/* Set up data for the last interrupt register set */
	struct xhci_erst_data *h_last_erst_data;
	h_last_erst_data = &host->erst_data[host->max_intrs - 1];

	h_last_erst_data->erst = zalloc2_align (XHCI_ERST_NBYTES,
						&h_last_erst_data->erst_addr,
						XHCI_ALIGN_64);

	h_last_erst_data->erst_size = 1;
	h_last_erst_data->trb_array = zalloc (sizeof (struct xhci_trb *));

	uint n_trbs = XHCI_HOST_N_TRBS;
	h_last_erst_data->erst[0].n_trbs = n_trbs;

	phys_t *h_erst_trb_addr = &h_last_erst_data->erst[0].trb_addr;

	size_t trb_nbytes = XHCI_TRB_NBYTES * n_trbs;
	h_last_erst_data->trb_array[0] = zalloc2_align (trb_nbytes,
							h_erst_trb_addr,
							XHCI_ALIGN_64);

	h_last_erst_data->erst_dq_ptr = h_last_erst_data->erst[0].trb_addr;
}

static u8
xhci_dev_addr (struct usb_host *usb_host, struct usb_request_block *h_urb)
{
	struct usb_host *usbhc = h_urb->host;
	struct xhci_host *host = (struct xhci_host *)usbhc->private;

	u32 slot_id = XHCI_URB_PRIVATE (h_urb->shadow)->slot_id;

	struct xhci_dev_ctx *h_dev_ctx = host->dev_ctx[slot_id];

	return XHCI_SLOT_CTX_USB_ADDR (h_dev_ctx->ctx);
}

static void
xhci_add_hc_data (struct usb_host *usbhc,
		  struct usb_device *dev,
		  struct usb_request_block *h_urb)
{
	dev->hc_specific_data[0] = XHCI_URB_PRIVATE (h_urb->shadow)->slot_id;
}

static struct usb_operations xhciop = {
	.shadow_buffer = xhci_shadow_trbs,
	.submit_control = xhci_submit_control,
	.submit_bulk = xhci_submit_bulk,
	.submit_interrupt = xhci_submit_interrupt,
	.check_advance = xhci_check_urb_advance,
	.deactivate_urb = xhci_deactivate_urb
};

static struct usb_init_dev_operations xhci_init_dev_op = {
	.dev_addr = xhci_dev_addr,
	.add_hc_specific_data = xhci_add_hc_data
};

/* Read all capability registers using 32-bit aligned access.  This is
 * to avoid unaligned access which might not work properly on emulated
 * xHCI.  In addition, we can avoid reading hc_sparams1 register
 * multiple times later by reading the copy of static read-only
 * capability registers instead. */
static void
read_cap_reg (const struct dres_reg *r, void *copy_to, size_t len)
{
	for (size_t i = 0; i < len; i += sizeof (u32))
		dres_reg_read32 (r, i, copy_to + i);
}

static void
xhci_new (struct pci_device *pci_device)
{
	pci_system_disconnect (pci_device);
	usb_set_debug (DEBUG_LEVEL);

	struct xhci_host *host = zalloc (XHCI_HOST_NBYTES);
	host->dev = pci_device;

	struct xhci_regs *regs = zalloc (XHCI_REGS_NBYTES);
	host->regs = regs;

	struct pci_bar_info bar_info;

	pci_get_bar_info (pci_device, 0, &bar_info);

	/* To avoid 0xFFFF..F in config_write */
	pci_device->driver->options.use_base_address_mask_emulation = 1;

	struct xhci_data *xhci_data = zalloc (XHCI_DATA_NBYTES);

	xhci_data->enabled = 0;
	xhci_data->host = host;
	reghook (xhci_data, &bar_info);

	pci_device->host = xhci_data;

	const struct dres_reg *r = regs->r;

	struct xhci_cap_reg cap_reg_copy;
	read_cap_reg (r, &cap_reg_copy, sizeof cap_reg_copy);
	xhci_set_reg_offsets (regs, &cap_reg_copy);

	xhci_construct_blacklist_ext_cap (regs);

	struct xhci_cap_reg *cap_reg = &cap_reg_copy;

	/* Read max device slots */
	host->max_slots = CAP_SPARAM1_GET_MAX_SLOTS (cap_reg->hc_sparams1);
	dprintft (REG_DEBUG_LEVEL, "Max device slots: 0x%X\n",
		  host->max_slots);

	/* Read max interrupters */
	host->max_intrs = CAP_SPARAM1_GET_MAX_INTRS (cap_reg->hc_sparams1);
	dprintft (REG_DEBUG_LEVEL, "Max interrupters: 0x%X\n",
		  host->max_intrs);

	if (host->max_intrs < 2) {
		panic ("Need more than 2 interrupt register set.\n");
	}

	/* Set usable interrupters */
	host->usable_intrs = host->max_intrs - 1;

	/* Read max ports */
	host->max_ports = CAP_SPARAM1_GET_MAX_PORTS (cap_reg->hc_sparams1);
	dprintft (REG_DEBUG_LEVEL, "Max ports: 0x%X\n", host->max_ports);

	/* Construct capability registers for showing to the guest. */
	/* Alloc "cap_length + 8" to deal with the out-of-bounds read. */
	regs->cap_reg_copy = zalloc (cap_reg->cap_length + 8);
	for (uint off = 0; off < cap_reg->cap_length; off += sizeof (u32)) {
		u32 *cap_reg_copy_off = (u32 *)&regs->cap_reg_copy[off];
		*cap_reg_copy_off = initial_cap_reg_read (host, r, off);
	}

	/* Port number starts from 1 */
	u64 last_port_number = host->max_ports + 1;

	/* For recording port status */
	host->portsc = zalloc (sizeof (u32) * last_port_number);

	/* Read PAE flag, necessary for URB completion check */
	host->pae = CAP_CPARAM1_GET_PAE (cap_reg->hc_cparams1);
	dprintft (REG_DEBUG_LEVEL, "PAE flag: %u\n", host->pae);

	/* Read max scratchpad */
	host->max_scratchpad =
		CAP_SPARAM2_GET_MAX_SCRATCHPAD (cap_reg->hc_sparams2);
	dprintft (REG_DEBUG_LEVEL, "Max scratchpad size: %u\n",
		  host->max_scratchpad);

	/* See which highest bit is set in PAGESIZE register */
	u32 buf;
	dres_reg_read32 (r, regs->opr_offset + OPR_PAGESIZE_OFFSET, &buf);

	/* We iterate only from 15 to 0 because the max page size is 128MB */
	uint pos;
	for (pos = 15; pos >= 0; pos--) {
		if (buf & (1 << pos)) {
			break;
		}
	}

	/*
	 * We need to fake pagesize bit position because of
	 * BitVisor's memory allocation
	 */
	pos = (pos > OPR_PAGESIZE_FLAG_POS_LIMIT) ?
	      OPR_PAGESIZE_FLAG_POS_LIMIT :
	      pos;
	host->opr_pagesize = buf & OPR_MAX_PAGESIZE_MASK;

	host->scratchpad_pagesize = OPR_PAGESIZE_FLAG_POS_TO_NBYTES (pos);
	dprintft (REG_DEBUG_LEVEL, "PAGESIZE reg: 0x%08X\n", buf);
	dprintft (REG_DEBUG_LEVEL, "Bit %u is set. So the page size is %u\n",
		  pos, host->scratchpad_pagesize);

	/* Set up erst_data */
	xhci_set_erst_data (host);

	/*
	 * Set up registers' start-end (For xhci_reg_handle()
	 * to dispatch RW properly)
	 */
	xhci_set_reg_range (host);

	host->usb_host = usb_register_host ((void *)host, &xhciop,
					    &xhci_init_dev_op,
					    pci_device->as_dma,
					    USB_HOST_TYPE_XHCI);
	ASSERT (host->usb_host != NULL);

	spinlock_lock (&host->usb_host->lock_hk);

	usb_hook_register (host->usb_host, USB_HOOK_REQUEST,
			   USB_HOOK_MATCH_ENDP,
			   0, 0, NULL, xhci_ep0_shadowing,
			   NULL, NULL);

	usb_hook_register (host->usb_host, USB_HOOK_REPLY,
			   USB_HOOK_MATCH_ENDP,
			   0, 0, NULL, xhci_ep0_copyback,
			   NULL, NULL);

	spinlock_unlock (&host->usb_host->lock_hk);

	/* Set up spinlock for state synchronization */
	spinlock_init (&host->sync_lock);

	extern void usbmsc_init_handle (struct usb_host *host);

	usbmsc_init_handle (host->usb_host);

	extern void usbhub_init_handle (struct usb_host *host);

	usbhub_init_handle (host->usb_host);

	pci_register_intr_callback (xhci_sync_state, xhci_data);

	printf ("xHCI controller initialized\n");
	return;
}


static struct pci_driver xhci_driver = {
	.name		= driver_name,
	.longname	= driver_longname,
	.device		= "class_code=0c0330",
	.new		= xhci_new,
	.config_read	= xhci_config_read,
	.config_write	= xhci_config_write,
};

/**
 * @brief	driver init function automatically called at boot time
 */
void
xhci_init (void) __initcode__
{
	pci_register_driver (&xhci_driver);
	return;
}

PCI_DRIVER_INIT (xhci_init);
