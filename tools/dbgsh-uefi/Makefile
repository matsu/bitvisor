INC_SHARE_DIR = ../../include/share
EDK2_INC      = ../../edk2/MdePkg/Include

include ../../Makefile.uefi.common

DBGSH_UEFI_SRC = dbgsh-uefi.c

DBGSH_UEFI_AMD_OBJ = dbgsh-a.o
DBGSH_UEFI_INTEL_OBJ = dbgsh-i.o
DBGSH_UEFI_AARCH64_OBJ = dbgsh-aarch64.o

CFLAGS_AMD = $(CFLAGS) -DVMMCALL=\"vmmcall\"
CFLAGS_INTEL = $(CFLAGS) -DVMMCALL=\"vmcall\"
CFLAGS_AARCH64 = $(CFLAGS)

DEP_x86 = $(DBGSH_UEFI_AMD_OBJ:.o=.d) $(DBGSH_UEFI_INTEL_OBJ:.o=.d)
DEP_aarch64 = $(DBGSH_UEFI_AARCH64_OBJ:.o=.d)
DEP = $(DEP_$(ARCH))

OUTPUT_x86 = dbgsh-a.efi dbgsh-i.efi
OUTPUT_aarch64 = dbgsh-aarch64.efi
OUTPUT = $(OUTPUT_$(ARCH))

.PHONY : all
all : $(OUTPUT)

.PHONY : clean
clean :
	$(RM) *.efi *.o *.d *.lib

dbgsh-a.efi : $(DBGSH_UEFI_AMD_OBJ)
	$(EXE_CC) $(LDFLAGS) $(LDFLAGS_EFI_APP) -o $@ $(DBGSH_UEFI_AMD_OBJ)
	$(STRIP) $@

dbgsh-i.efi : $(DBGSH_UEFI_INTEL_OBJ)
	$(EXE_CC) $(LDFLAGS) $(LDFLAGS_EFI_APP) -o $@ $(DBGSH_UEFI_INTEL_OBJ)
	$(STRIP) $@

dbgsh-aarch64.efi : $(DBGSH_UEFI_AARCH64_OBJ)
	$(EXE_CC) $(LDFLAGS) $(LDFLAGS_EFI_APP) -o $@ $(DBGSH_UEFI_AARCH64_OBJ)
	$(STRIP) $@

$(DBGSH_UEFI_AMD_OBJ) : $(DBGSH_UEFI_SRC)
	$(EXE_CC) $(CFLAGS_AMD) -c $(DBGSH_UEFI_SRC) -o $@

$(DBGSH_UEFI_INTEL_OBJ) : $(DBGSH_UEFI_SRC)
	$(EXE_CC) $(CFLAGS_INTEL) -c $(DBGSH_UEFI_SRC) -o $@

$(DBGSH_UEFI_AARCH64_OBJ) : $(DBGSH_UEFI_SRC)
	$(EXE_CC) $(CFLAGS_AARCH64) -c $(DBGSH_UEFI_SRC) -o $@

-include $(DEP)
