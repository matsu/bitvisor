CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN_PD) += -DIDMAN_PD
CONSTANTS-y += -DNTTCOM

CFLAGS += -Iidman/ccid -Iidman/iccard -Iidman/idman_pkcs11 -Iidman/pcsc
CFLAGS += -Iidman/pkcs11 -Iidman/standardio -Iidman/user

CFLAGS += -Idrivers/usb -Iidman

objs-y += user.o
subdirs-y += ccid iccard idman_pkcs11 pcsc pkcs11 standardio
