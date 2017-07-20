INC	 = ../../include/share/config.h
BOOTARGS = ../../include/share/uefi_boot.h

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
all : loadvmm.efi

.PHONY : clean
clean :
	$(RM) loadvmm.dll loadvmm.efi

loadvmm.efi : loadvmm.dll
	(dd bs=1 count=220 && echo && dd bs=1 count=1 of=/dev/null && cat) \
		< loadvmm.dll > loadvmm.efi

loadvmm.dll : loadvmm.c pass_auth.c pass_auth.h randseed.c randseed.h \
		$(INC) $(BOOTARGS)
	$(EXE_CC) -shared -nostdlib -e efi_main@8 \
		-mno-red-zone -mno-sse -nostdinc -mno-ms-bitfields -s -O \
		-ffreestanding -fno-builtin -fno-stack-protector \
		-fno-strict-aliasing \
		-I. \
		-I../../include/share/ \
		-I$(EDK_DIR)/Foundation/Efi/Include/ \
		-I$(EDK_DIR)/Foundation/Framework/Include/ \
		-I$(EDK_DIR)/Foundation/Include/ \
		-I$(EDK_DIR)/Foundation/Efi/ \
		-o loadvmm.dll loadvmm.c pass_auth.c randseed.c
