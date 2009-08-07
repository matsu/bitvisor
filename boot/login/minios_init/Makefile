CFLAGS = -Wall -idirafter ../../../include -idirafter ../../login-simple
CFLAGS_IDMAN = -Wall \
	-DIDMAN_CLIENT -idirafter ../../../idman/ccid \
	-idirafter ../../../idman/iccard \
	-idirafter ../../../idman/idman_pkcs11 -idirafter ../../../idman/pcsc \
	-idirafter ../../../idman/pkcs11 -idirafter ../../../idman/standardio \
	-idirafter idmaninc
LDFLAGS = -static -s
LDLIBS = -lusb -lssl -lcrypto
OBJS   = boot.o minios_init.o passinput.o usbmem.o idman.o configparser.o \
	dl.o keymapload.o
BIN    = minios_init
INC    = ../../../include/core/config.h
IDMAN_SRC = ../../../idman/ccid/IDMan_CcAtr.c \
	../../../idman/ccid/IDMan_CcIfdhandler.c \
	../../../idman/ccid/IDMan_CcInitCard.c \
	../../../idman/ccid/IDMan_CcProtocol.c \
	../../../idman/ccid/IDMan_CcReaderControl.c \
	../../../idman/ccid/IDMan_CcUsb.c \
	../../../idman/iccard/IDMan_ICSCard.c \
	../../../idman/idman_pkcs11/IDMan_IPCommon.c \
	../../../idman/idman_pkcs11/IDMan_IPgenerateSignature.c \
	../../../idman/idman_pkcs11/IDMan_IPgetCertificate.c \
	../../../idman/idman_pkcs11/IDMan_IPgetStaticPassword.c \
	../../../idman/idman_pkcs11/IDMan_IPuserAuthPKCS11.c \
	../../../idman/idman_pkcs11/IDMan_IPverifySignature.c \
	../../../idman/pcsc/IDMan_PcEventhandler.c \
	../../../idman/pcsc/IDMan_PcHotplug_libusb.c \
	../../../idman/pcsc/IDMan_PcProthandler.c \
	../../../idman/pcsc/IDMan_PcReaderfactory.c \
	../../../idman/pcsc/IDMan_PcWinscard.c \
	../../../idman/pkcs11/IDMan_PKCardAccess.c \
	../../../idman/pkcs11/IDMan_PKCardData.c \
	../../../idman/pkcs11/IDMan_PKList.c \
	../../../idman/pkcs11/IDMan_PKPkcs11.c \
	../../../idman/standardio/IDMan_StandardIo.c

.PHONY : all clean

all : $(BIN) keymapdump

clean :
	rm -f $(OBJS) $(BIN) core-idman.o idman.o

$(BIN) : $(OBJS)

$(OBJS) : $(INC)

idman.o : $(IDMAN_SRC)
	$(CC) $(CFLAGS) -c -o core-idman.o ../../../core/idman.c
	$(CC) $(CFLAGS_IDMAN) $(LDFLAGS) -nostdlib -Wl,-r -o idman.o \
		$(IDMAN_SRC) core-idman.o

configparser.o : ../../login-simple/configparser.c
	$(CC) $(CFLAGS) -c -o configparser.o ../../login-simple/configparser.c
