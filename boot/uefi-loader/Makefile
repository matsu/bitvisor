MINGW_PREFIX_DEBIAN	= amd64-mingw32msvc-
MINGW_PREFIX		= $(MINGW_PREFIX_DEBIAN)
EXE_CC			= $(MINGW_PREFIX)cc
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

loadvmm.dll : loadvmm.c
	$(EXE_CC) -shared -nostdlib -e efi_main@8 \
		-mno-red-zone -mno-sse -nostdinc -s -O \
		-ffreestanding -fno-builtin -fno-stack-protector \
		-fno-strict-aliasing \
		-I$(EDK_DIR)/Foundation/Efi/Include/ \
		-I$(EDK_DIR)/Foundation/Framework/Include/ \
		-I$(EDK_DIR)/Foundation/Include/ \
		-I$(EDK_DIR)/Foundation/Efi/ -o loadvmm.dll loadvmm.c
