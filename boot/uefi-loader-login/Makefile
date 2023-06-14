INC_SHARE_DIR = ../../include/share
EDK2_INC      = ../../edk2/MdePkg/Include

include ../../Makefile.uefi.common

LOADVMM_OBJ = loadvmm.o pass_auth.o randseed.o

BSDRIVER_OBJ = bsdriver.o
BSDRIVER_SRC = ../uefi-loader/$(BSDRIVER_OBJ:.o=.c)

DEP = $(BSDRIVER_OBJ:.o=.d) $(LOADVMM_OBJ:.o=.d)

.PHONY : all
all : loadvmm.efi

.PHONY : clean
clean :
	$(RM) *.efi *.o *.d *.lib

$(LOADVMM_OBJ) : bsdriver.efi

loadvmm.efi : $(LOADVMM_OBJ)
	$(EXE_CC) $(LDFLAGS) $(LDFLAGS_EFI_APP) -o $@ $(LOADVMM_OBJ)
	$(STRIP) $@

bsdriver.efi : $(BSDRIVER_OBJ)
	$(EXE_CC) $(LDFLAGS) $(LDFLAGS_EFI_BSDRV) -o $@ $(BSDRIVER_OBJ)
	$(STRIP) $@

$(BSDRIVER_OBJ) : $(BSDRIVER_SRC)
	$(EXE_CC) $(CFLAGS) -c $(BSDRIVER_SRC) -o $@

-include $(DEP)
