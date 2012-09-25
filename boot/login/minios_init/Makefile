CFLAGS = -m32 -Wall -idirafter ../../../include -idirafter ../../login-simple
CFLAGS_IDMAN = -Wall \
	-DIDMAN_CLIENT -idirafter ../../../idman/lib/ccid \
	-idirafter ../../../idman/lib/iccard \
	-idirafter ../../../idman/lib/idman_pkcs11 \
	-idirafter ../../../idman/lib/pcsc \
	-idirafter ../../../idman/lib/pkcs11 \
	-idirafter ../../../idman/lib/standardio \
	-idirafter idmaninc
LDFLAGS = -m32 -static
LDLIBS = -lusb -lssl -lcrypto
OBJS   = boot.o minios_init.o passinput.o usbmem.o idman.o configparser.o \
	dl.o keymapload.o
BIN    = minios_init
INC    = ../../../include/core/config.h
IDMAN_SRC = ../../../idman/lib/ccid/IDMan_CcAtr.c \
	../../../idman/lib/ccid/IDMan_CcIfdhandler.c \
	../../../idman/lib/ccid/IDMan_CcInitCard.c \
	../../../idman/lib/ccid/IDMan_CcProtocol.c \
	../../../idman/lib/ccid/IDMan_CcReaderControl.c \
	../../../idman/lib/ccid/IDMan_CcUsb.c \
	../../../idman/lib/iccard/IDMan_ICSCard.c \
	../../../idman/lib/idman_pkcs11/IDMan_IPCommon.c \
	../../../idman/lib/idman_pkcs11/IDMan_IPgenerateSignature.c \
	../../../idman/lib/idman_pkcs11/IDMan_IPgetCertificate.c \
	../../../idman/lib/idman_pkcs11/IDMan_IPgetStaticPassword.c \
	../../../idman/lib/idman_pkcs11/IDMan_IPuserAuthPKCS11.c \
	../../../idman/lib/idman_pkcs11/IDMan_IPverifySignature.c \
	../../../idman/lib/pcsc/IDMan_PcEventhandler.c \
	../../../idman/lib/pcsc/IDMan_PcHotplug_libusb.c \
	../../../idman/lib/pcsc/IDMan_PcProthandler.c \
	../../../idman/lib/pcsc/IDMan_PcReaderfactory.c \
	../../../idman/lib/pcsc/IDMan_PcWinscard.c \
	../../../idman/lib/pkcs11/IDMan_PKCardAccess.c \
	../../../idman/lib/pkcs11/IDMan_PKCardData.c \
	../../../idman/lib/pkcs11/IDMan_PKList.c \
	../../../idman/lib/pkcs11/IDMan_PKPkcs11.c \
	../../../idman/lib/standardio/IDMan_StandardIo.c

.PHONY : all clean

all : $(BIN) keymapdump

clean :
	rm -f $(OBJS) $(BIN) core-idman.o idman.o

$(BIN) : $(OBJS)

$(OBJS) : $(INC)

idman.o : $(IDMAN_SRC)
	$(CC) $(CFLAGS) -c -o core-idman.o idmanread.c
	$(CC) $(CFLAGS_IDMAN) $(LDFLAGS) -nostdlib -Wl,-r -o idman.o \
		$(IDMAN_SRC) core-idman.o

configparser.o : ../../login-simple/configparser.c
	$(CC) $(CFLAGS) -c -o configparser.o ../../login-simple/configparser.c
