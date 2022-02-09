INC_SHARE_DIR = ../../include/share

MINGW1=$(shell which amd64-mingw32msvc-cc 2> /dev/null)
MINGW2=$(shell which x86_64-w64-mingw32-gcc 2> /dev/null)
ifneq ("$(MINGW1)","")
	EXE_CC=$(MINGW1)
else ifneq ("$(MINGW2)","")
	EXE_CC=$(MINGW2)
else ifndef EXE_CC
$(error MinGW not found)
endif
RM			= rm -f
EDK2_INC		= ../../edk2/MdePkg/Include

.PHONY : all
all : dbgsh-a.efi dbgsh-i.efi

.PHONY : clean
clean :
	$(RM) dbgsh-a.efi dbgsh-i.efi

dbgsh-a.efi : dbgsh-uefi.c
	$(EXE_CC) -shared -nostdlib -e efi_main@8 \
		-mno-red-zone -mno-sse -nostdinc -s -O \
		-ffreestanding -fno-builtin -fno-stack-protector \
		-fno-strict-aliasing \
		-Wl,--subsystem,10 \
		-I. \
		-I$(INC_SHARE_DIR) \
		-I$(EDK2_INC) \
		-I$(EDK2_INC)/X64 \
		-DVMMCALL=\"vmmcall\" \
		-o dbgsh-a.efi dbgsh-uefi.c

dbgsh-i.efi : dbgsh-uefi.c
	$(EXE_CC) -shared -nostdlib -e efi_main@8 \
		-mno-red-zone -mno-sse -nostdinc -s -O \
		-ffreestanding -fno-builtin -fno-stack-protector \
		-fno-strict-aliasing \
		-Wl,--subsystem,10 \
		-I. \
		-I$(INC_SHARE_DIR) \
		-I$(EDK2_INC) \
		-I$(EDK2_INC)/X64 \
		-DVMMCALL=\"vmcall\" \
		-o dbgsh-i.efi dbgsh-uefi.c
