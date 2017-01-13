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
EDK_DIR			= ../../edk

.PHONY : all
all : dbgsh-a.efi dbgsh-i.efi

.PHONY : clean
clean :
	$(RM) dbgsh-a.dll dbgsh-i.dll dbgsh-a.efi dbgsh-i.efi

dbgsh-a.efi : dbgsh-a.dll
	(dd bs=1 count=220 && echo && dd bs=1 count=1 of=/dev/null && cat) \
		< dbgsh-a.dll > dbgsh-a.efi

dbgsh-i.efi : dbgsh-i.dll
	(dd bs=1 count=220 && echo && dd bs=1 count=1 of=/dev/null && cat) \
		< dbgsh-i.dll > dbgsh-i.efi

dbgsh-a.dll : dbgsh-uefi.c
	$(EXE_CC) -shared -nostdlib -e efi_main@8 \
		-mno-red-zone -mno-sse -nostdinc -s -O \
		-ffreestanding -fno-builtin -fno-stack-protector \
		-fno-strict-aliasing \
		-I. \
		-I$(EDK_DIR)/Foundation/Efi/Include/ \
		-I$(EDK_DIR)/Foundation/Framework/Include/ \
		-I$(EDK_DIR)/Foundation/Include/ \
		-I$(EDK_DIR)/Foundation/Efi/ \
		-DVMMCALL=\"vmmcall\" \
		-o dbgsh-a.dll dbgsh-uefi.c

dbgsh-i.dll : dbgsh-uefi.c
	$(EXE_CC) -shared -nostdlib -e efi_main@8 \
		-mno-red-zone -mno-sse -nostdinc -s -O \
		-ffreestanding -fno-builtin -fno-stack-protector \
		-fno-strict-aliasing \
		-I. \
		-I$(EDK_DIR)/Foundation/Efi/Include/ \
		-I$(EDK_DIR)/Foundation/Framework/Include/ \
		-I$(EDK_DIR)/Foundation/Include/ \
		-I$(EDK_DIR)/Foundation/Efi/ \
		-DVMMCALL=\"vmcall\" \
		-o dbgsh-i.dll dbgsh-uefi.c
