CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS

CFLAGS += -Idrivers/include

objs-$(CONFIG_DEBUG_ATA) += ata_debug.o
objs-y += ahci.o ata_bm.o ata_cmd.o ata_core.o ata_init.o ata_pci.o
objs-y += ata_vendor.o atapi.o packet.o
