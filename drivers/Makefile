CONSTANTS-$(CONFIG_IEEE1394_CONCEALER) += -DIEEE1394_CONCEALER
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS
CONSTANTS-$(CONFIG_DUMP_PCI_DEV_LIST) += -DDUMP_PCI_DEV_LIST

CONSTANTS-$(CONFIG_NET_PRO100) += -DNET_PRO100
CONSTANTS-$(CONFIG_NET_PRO1000) += -DNET_PRO1000
CONSTANTS-$(CONFIG_NET_RTL8169) += -DNET_RTL8169

CFLAGS += -Ivpn

subdirs-$(CONFIG_ATA_DRIVER) += ata
subdirs-$(CONFIG_USB_DRIVER) += usb
subdirs-$(CONFIG_NET_DRIVER) += net
objs-1 += core.o dmar.o ieee1394.o iommu.o pci_conceal.o pci_core.o
objs-1 += pci_debug.o pci_init.o pci_match.o pci_match_compat.o security.o
objs-$(CONFIG_LOG_TO_IEEE1394) += ieee1394log.o
objs-$(CONFIG_VGA_INTEL_DRIVER) += vga_intel.o
objs-$(CONFIG_TTY_X540) += x540.o
objs-$(CONFIG_PCI_MONITOR) += pci_monitor.o
