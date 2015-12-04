MINGW1=$(shell which i586-mingw32msvc-cc 2> /dev/null)
MINGW2=$(shell which i686-w64-mingw32-gcc 2> /dev/null)
ifneq ("$(MINGW1)","")
	EXE_CC=$(MINGW1)
else ifneq ("$(MINGW2)","")
	EXE_CC=$(MINGW2)
else
$(error MinGW not found)
endif
RM			= rm -f

.PHONY : all
all : iccard.exe

.PHONY : clean
clean :
	$(RM) iccard.exe

iccard.exe : iccard.c ../common/call_vmm.c ../common/call_vmm.h
	$(EXE_CC) -s -mwindows -o iccard.exe iccard.c ../common/call_vmm.c
