/*
 * Copyright (c) 2018 Igel Co., Ltd
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
/*
 * Copyright (c) 1997, 1998
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <core.h>
#include <core/mmio.h>
#include <core/time.h>
#include <core/thread.h>
#include <net/netapi.h>

#include <stdint.h>

#include "pci.h"

#include "re_core.h"

#include "re_mod_header.h"

#define __P(a) a
#define TUNABLE_INT(a, b)

#define RE_PCIER_LINK_CTL	0x10

#define RE_PCIER_LINK_CAP	0x0C
#define RE_PCIEM_LINK_CAP_ASPM	0x00000C00

#define RE_LOCK(_sc)
#define RE_UNLOCK(_sc)
#define RE_LOCK_ASSERT(_sc)

#define EOPNOTSUPP 45

#define DELAY(time) usleep (time)

/* BitVisor currently runs only on little-endian machines */
#define htole16(val) (val)
#define htole32(val) (val)

#define ntohs(val) (((((u16)(val) & 0xFF)) << 8) | \
		    (((u16)(val) & 0xFF00) >> 8))

#define CSR_WRITE_4(sc, reg, val) \
	write_io_reg ((sc), (reg), (val), 4)
#define CSR_WRITE_2(sc, reg, val) \
	write_io_reg ((sc), (reg), (val), 2)
#define CSR_WRITE_1(sc, reg, val) \
	write_io_reg ((sc), (reg), (val), 1)

#define CSR_READ_4(sc, reg) \
	((u32)read_io_reg ((sc), (reg), 4))
#define CSR_READ_2(sc, reg) \
	((u16)read_io_reg ((sc), (reg), 2))
#define CSR_READ_1(sc, reg) \
	((u8)read_io_reg ((sc), (reg), 1))

/* cmac write/read MMIO register */
#define RE_CMAC_WRITE_4(sc, reg, val) \
	cmac_write ((sc), (reg), (val), 4)
#define RE_CMAC_WRITE_2(sc, reg, val) \
	cmac_write ((sc), (reg), (val), 2)
#define RE_CMAC_WRITE_1(sc, reg, val) \
	cmac_write ((sc), (reg), (val), 1)

#define RE_CMAC_READ_4(sc, reg) \
	((u32)cmac_read ((sc), (reg), 4))
#define RE_CMAC_READ_2(sc, reg) \
	((u16)cmac_read ((sc), (reg), 2))
#define RE_CMAC_READ_1(sc, reg) \
	((u8)cmac_read ((sc), (reg), 1))

void usleep (u32);
static uint read_io_reg (struct re_softc *sc, uint reg_offset, uint nbytes);
static void write_io_reg (struct re_softc *sc, uint reg_offset, uint val,
			  uint nbytes);
static uint cmac_read (struct re_softc *sc, uint reg_offset, uint nbytes);
static void cmac_write (struct re_softc *sc,uint reg_offset, uint val,
			uint nbytes);
static u32 pci_read_config (device_t dev, u32 reg, uint len);
static void pci_write_config (device_t dev, u32 reg, u32 val, uint len);
static u32 pci_find_cap (device_t dev, u32 cap, int *cap_reg);

#include "re_mod_impl.h"

static uint
read_io_reg (struct re_softc *sc, uint reg_offset, uint nbytes)
{
	uint val = 0;

	if (sc->prohibit_access_reg)
		return 0xFFFFFFFF;

	switch (nbytes) {
	case 4:
		in32 (sc->io_port_base + reg_offset, &val);
		break;
	case 2:
		in16 (sc->io_port_base + reg_offset, (u16 *)&val);
		break;
	case 1:
		in8 (sc->io_port_base + reg_offset, (u8 *)&val);
		break;
	default:
		panic ("re: invalid io size");
	}

	return val;
}

static void
write_io_reg (struct re_softc *sc, uint reg_offset, uint val, uint nbytes)
{
	if (sc->prohibit_access_reg)
		return;

	switch (nbytes) {
	case 4:
		out32 (sc->io_port_base + reg_offset, val);
		break;
	case 2:
		out16 (sc->io_port_base + reg_offset, val);
		break;
	case 1:
		out8 (sc->io_port_base + reg_offset, val);
		break;
	default:
		panic ("re: invalid io size");
	}
}

static uint
cmac_read (struct re_softc *sc, uint reg_offset, uint nbytes)
{
	if (sc->prohibit_access_reg)
		return 0xFFFFFFFF;

	uint val = 0;

	if (sc->cmac_regs)
		memcpy (&val, sc->cmac_regs + reg_offset, nbytes);
	else
		val = read_io_reg (sc, reg_offset, nbytes);

	return val;
}

static void
cmac_write (struct re_softc *sc, uint reg_offset, uint val, uint nbytes)
{
	if (sc->prohibit_access_reg)
		return;

	if (sc->cmac_regs)
		memcpy (sc->cmac_regs + reg_offset, &val, nbytes);
	else
		write_io_reg (sc, reg_offset, val, nbytes);

}

static u32
pci_read_config (device_t dev, u32 reg, uint len)
{
	ASSERT (len <= 4);
	u32 val = 0;
	pci_config_read (dev, &val, len, reg);
	return val;
}

static void
pci_write_config (device_t dev, u32 reg, u32 val, uint len)
{
	ASSERT (len <= 4);
	pci_config_write (dev, &val, len, reg);
}

static u32
pci_find_cap (device_t dev, u32 cap, int *cap_reg)
{
	*cap_reg = pci_find_cap_offset (dev, cap);
	return !!(*cap_reg);
}

void
re_core_init (struct re_host *host)
{
	struct pci_device *dev = host->dev;

	u8 eaddr[ETHER_ADDR_LEN];
	struct re_softc *sc;
	int error = 0;
	int cap_offset;

	sc = alloc (sizeof (*sc));
	memset (sc, 0, sizeof (*sc));

	sc->dev = dev;
	sc->host = host;

	host->sc = sc;

	sc->re_device_id = dev->config_space.device_id;
	sc->re_revid = dev->config_space.revision_id;

	pci_get_bar_info (sc->dev, 0, &sc->reg_bar_io);

	ASSERT (sc->reg_bar_io.base);
	ASSERT (sc->reg_bar_io.type == PCI_BAR_INFO_TYPE_IO);

	sc->io_port_base = sc->reg_bar_io.base;

	error = re_check_mac_version (sc);

	if (error)
		panic ("re: re_check_mac_version() error, unknown device");

	re_init_software_variable (sc);

	cap_offset = pci_find_cap_offset (dev, PCI_CAP_PCIEXP);

	if (cap_offset) {
		sc->re_if_flags |= RL_FLAG_PCIE;
		sc->re_expcap = cap_offset;
	} else {
		sc->re_if_flags &= ~RL_FLAG_PCIE;
		sc->re_expcap = 0;
	}

	if (sc->re_type == MACFG_3) { /* Change PCI Latency time */
		u8 val = 0x40;
		pci_config_write (dev, &val, 1, RE_PCI_LATENCY_TIMER);
	}

	/* Get MAC address. */
	re_get_hw_mac_address (sc, eaddr);

	if (eaddr[0] == 0xff &&
	    eaddr[1] == 0xff &&
	    eaddr[2] == 0xff &&
	    eaddr[3] == 0xff &&
	    eaddr[4] == 0xff &&
	    eaddr[5] == 0xff)
		panic ("re: MAC Address invalid");

	memcpy (host->mac, eaddr, sizeof (host->mac));
}

void
re_core_start (struct re_host *host)
{
	struct pci_device *dev = host->dev;
	struct re_softc *sc = host->sc;
	u64 start_time;
	uint i;

	ASSERT (sc && dev);

	/* Disable ASPM L0S/L1 and Clock Request. */
	if (sc->re_expcap != 0) {
		uint cap, ctl;
		pci_config_read (dev,
				 &cap,
				 2,
				 sc->re_expcap + RE_PCIER_LINK_CAP);
		if ((cap & RE_PCIEM_LINK_CAP_ASPM) != 0) {
			pci_config_read (dev,
					 &ctl,
					 2,
					 sc->re_expcap + RE_PCIER_LINK_CTL);
			if ((ctl & 0x0103) != 0) {
				ctl &= ~0x0103;
				pci_config_write (dev,
						  &ctl,
						  2,
						  sc->re_expcap +
						  RE_PCIER_LINK_CTL);
				printf ("re: ASPM disabled\n");
			}
		} else {
			printf ("re: no ASPM capability\n");
		}
	}

        sc->prohibit_access_reg = 0;

	re_exit_oob (sc);

	re_hw_init (sc);

	re_reset (sc);

	re_phy_power_up (sc);

	re_hw_phy_config (sc);

	re_clrwol (sc);

	re_ifmedia_upd (sc);

	set_rxbufsize (sc);

	re_link_on_patch (sc);

	printf ("re: waiting for link up\n");

	start_time = get_time ();

	while (!re_link_ok (sc)) {
		if (get_time () - start_time > 5000000) {
			printf ("re: timeout, assume link is disconnected\n");
			break;
		}
		schedule ();
	}

	/* Save PCI config space */
	for (i = 0; i < PCI_CONFIG_REGS32_NUM; i++)
		pci_config_read (host->dev,
				 &host->regs_at_init[i],
				 sizeof (host->regs_at_init[i]),
				 sizeof (host->regs_at_init[i]) * i);

	host->ready = 1;
}

void
re_core_suspend (struct re_host *host)
{
	struct re_softc *sc = host->sc;

	host->ready = 0;

	re_stop (sc);

	re_hw_d3_para (sc);

        sc->prohibit_access_reg = 1;
}

void
re_core_resume (struct re_host *host)
{
	re_core_start (host);
}

void
re_core_handle_recv (struct re_host *host)
{
	static u64 prev_time;

	struct re_softc	*sc = host->sc;
	u64 time;
	u16 status = 0;

	if (!host->ready)
		return;

	spinlock_lock (&host->rx_lock);

	time = get_time ();

	/* Access registers too often leads to performance drop */
	if (time - prev_time > 100) {
		status = CSR_READ_2 (sc, RE_ISR);

		if (status)
			CSR_WRITE_2 (sc, RE_ISR, status & 0xffbf);

		prev_time = time;
	}

	re_rxeof (sc);

	if (sc->re_type == MACFG_21) {
		if (status & RE_ISR_FIFO_OFLOW) {
			sc->rx_fifo_overflow = 1;
			CSR_WRITE_2 (sc, 0x00e2, 0x0000);
			CSR_WRITE_4 (sc, 0x0048, 0x4000);
			CSR_WRITE_4 (sc, 0x0058, 0x4000);
		} else {
			sc->rx_fifo_overflow = 0;
			CSR_WRITE_4 (sc, RE_CPCR, 0x51512082);
		}

		if (status & RE_ISR_PCS_TIMEOUT) {
			if ((status & RE_ISR_FIFO_OFLOW) &&
			    (!(status & (RE_ISR_RX_OK |
					 RE_ISR_TX_OK |
					 RE_ISR_RX_OVERRUN))))
				printf ("re: RE_ISR_PCS_TIMEOUT error\n");
		}
	}

	spinlock_unlock (&host->rx_lock);

	spinlock_lock (&host->tx_lock);

	re_txeof (sc);

	switch(sc->re_type) {
	case MACFG_21:
	case MACFG_22:
	case MACFG_23:
	case MACFG_24:
		CSR_WRITE_1 (sc, RE_TPPOLL, RE_NPQ);
		break;
	default:
		break;
	}

	spinlock_unlock (&host->tx_lock);
}

void
re_core_handle_send (struct re_host *host,
		     uint n_packets,
		     void **packets,
		     uint *packet_sizes)
{
	struct re_softc *sc = host->sc;

	uint i, count_free;

	if (!host->ready)
		return;

	spinlock_lock (&host->tx_lock);

	if (sc->rx_fifo_overflow)
		goto end;

	count_free = CountFreeTxDescNum (&sc->re_desc);
	if (n_packets > count_free)
		n_packets = count_free;

	for (i = 0; i < n_packets; i++) {
		/* XXX: how should we do with re_coalesce_tx_pkt stuff? */
		/* XXX: vlan stuff */
		/*
		 * fs_flag and ls_flag are always one for now because
		 * each packet buffer contains all packet data.
		 */
		WritePacket (sc,
			     packets[i],
			     packet_sizes[i],
			     1,
			     1,
			     0,
			     0);
	}
end:
	spinlock_unlock (&host->tx_lock);
}

void
re_core_intr_clear (void *param)
{
	struct re_host *host = param;

	re_core_handle_recv (host);
}

void
re_core_intr_set (void *param)
{
	/* Do nothing */
}

void
re_core_intr_disable (void *param)
{
	struct re_host *host = param;
	struct re_softc *sc = host->sc;

	CSR_WRITE_2 (sc, RE_IMR, 0x0000);

	host->intr_enabled = 0;

	printf ("re: disable interrupt\n");
}

void
re_core_intr_enable (void *param)
{
	struct re_host *host = param;
	struct re_softc *sc = host->sc;

	if (host->intr_enabled)
		return;

	CSR_WRITE_2 (sc, RE_IMR, RE_INTRS);

	host->intr_enabled = 1;

	printf ("re: enable interrupt\n");
}
