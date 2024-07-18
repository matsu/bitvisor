CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_IDMAN_PD) += -DIDMAN_PD
CONSTANTS-y += -DNTTCOM

CFLAGS += -Iidman/lib/ccid -Iidman/lib/iccard -Iidman/lib/idman_pkcs11
CFLAGS += -Iidman/lib/pcsc -Iidman/lib/pkcs11 -Iidman/lib/standardio
CFLAGS += -Iidman/lib/user

objs-y += IDMan_IPCommon.o IDMan_IPgenerateSignature.o
objs-y += IDMan_IPgetCertificate.o IDMan_IPgetStaticPassword.o
objs-y += IDMan_IPuserAuthPKCS11.o IDMan_IPverifySignature.o
