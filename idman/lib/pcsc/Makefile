CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN_PD) += -DIDMAN_PD
CONSTANTS-1 += -DNTTCOM

CFLAGS += -Iidman/lib/ccid -Iidman/lib/iccard -Iidman/lib/idman_pkcs11
CFLAGS += -Iidman/lib/pcsc -Iidman/lib/pkcs11 -Iidman/lib/standardio
CFLAGS += -Iidman/lib/user
CFLAGS += -Idrivers -Idrivers/usb

objs-1 += IDMan_PcEventhandler.o IDMan_PcHotplug_libusb.o
objs-1 += IDMan_PcProthandler.o IDMan_PcReaderfactory.o IDMan_PcWinscard.o
