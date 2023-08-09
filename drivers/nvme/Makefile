CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS

CFLAGS += -Idrivers/include

objs-1 += nvme.o nvme_subm_handler.o nvme_comp_handler.o nvme_common.o \
	  nvme_io.o
objs-$(CONFIG_STORAGE) += nvme_crypt.o
objs-$(CONFIG_STORAGE_IO) += nvme_storage_io.o
