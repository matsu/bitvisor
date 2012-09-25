MINGW_PREFIX_DEBIAN	= i586-mingw32msvc-
MINGW_PREFIX		= $(MINGW_PREFIX_DEBIAN)
EXE_CC			= $(MINGW_PREFIX)cc
RM			= rm -f

.PHONY : all
all : iccard.exe

.PHONY : clean
clean :
	$(RM) iccard.exe

iccard.exe : iccard.c ../common/call_vmm.c ../common/call_vmm.h
	$(EXE_CC) -s -mwindows -o iccard.exe iccard.c ../common/call_vmm.c
