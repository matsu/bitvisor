CONSTANTS-$(CONFIG_IEEE1394_CONCEALER) += -DIEEE1394_CONCEALER
CONSTANTS-$(CONFIG_FWDBG) += -DFWDBG
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS

CFLAGS += -Ivpn

subdirs-$(CONFIG_ATA_DRIVER) += ata
subdirs-$(CONFIG_USB_DRIVER) += usb
subdirs-$(CONFIG_VPN_DRIVER) += vpn
objs-1 += core.o dmar.o ieee1394.o iommu.o pci_conceal.o pci_core.o
objs-1 += pci_debug.o pci_init.o security.o
objs-$(CONFIG_VGA_INTEL_DRIVER) += vga_intel.o
