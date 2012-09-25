MINGW_PREFIX_DEBIAN	= i586-mingw32msvc-
MINGW_PREFIX		= $(MINGW_PREFIX_DEBIAN)
EXE_CC			= $(MINGW_PREFIX)cc
RM			= rm -f

.PHONY : all
all : dbgsh dbgsh.exe

.PHONY : clean
clean :
	$(RM) dbgsh dbgsh.exe

dbgsh : dbgsh.c ../common/call_vmm.c ../common/call_vmm.h
	$(CC) -s -o dbgsh dbgsh.c ../common/call_vmm.c

dbgsh.exe : dbgsh.c ../common/call_vmm.c ../common/call_vmm.h
	$(EXE_CC) -s -o dbgsh.exe dbgsh.c ../common/call_vmm.c
