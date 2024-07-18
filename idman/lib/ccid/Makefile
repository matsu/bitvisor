CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN_PD) += -DIDMAN_PD
CONSTANTS-y += -DNTTCOM

CFLAGS += -Iidman/lib/ccid -Iidman/lib/iccard -Iidman/lib/idman_pkcs11
CFLAGS += -Iidman/lib/pcsc -Iidman/lib/pkcs11 -Iidman/lib/standardio
CFLAGS += -Iidman/lib/user
CFLAGS += -Idrivers/include -Idrivers/usb

objs-y += IDMan_CcAtr.o IDMan_CcIfdhandler.o IDMan_CcInitCard.o
objs-y += IDMan_CcProtocol.o IDMan_CcReaderControl.o IDMan_CcUsb.o
