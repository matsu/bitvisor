CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN_PD) += -DIDMAN_PD
CONSTANTS-1 += -DNTTCOM

CFLAGS += -Iidman/lib/ccid -Iidman/lib/iccard -Iidman/lib/idman_pkcs11
CFLAGS += -Iidman/lib/pcsc -Iidman/lib/pkcs11 -Iidman/lib/standardio
CFLAGS += -Iidman/lib/user

objs-1 += IDMan_PKCardAccess.o IDMan_PKCardData.o IDMan_PKList.o
objs-1 += IDMan_PKPkcs11.o
