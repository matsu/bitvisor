CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN_PD) += -DIDMAN_PD
CONSTANTS-y += -DNTTCOM

CFLAGS += -Iidman/lib/ccid -Iidman/lib/iccard -Iidman/lib/idman_pkcs11
CFLAGS += -Iidman/lib/pcsc -Iidman/lib/pkcs11 -Iidman/lib/standardio
CFLAGS += -Iidman/lib/user
CFLAGS += -Idrivers/include -Idrivers/usb

objs-y += IDMan_PcEventhandler.o IDMan_PcHotplug_libusb.o
objs-y += IDMan_PcProthandler.o IDMan_PcReaderfactory.o IDMan_PcWinscard.o
