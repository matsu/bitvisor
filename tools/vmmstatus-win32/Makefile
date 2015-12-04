ifdef X64
	MINGW1_PREFIX=amd64-mingw32msvc-
	MINGW2_PREFIX=x86_64-w64-mingw32-
else
	MINGW1_PREFIX=i586-mingw32msvc-
	MINGW2_PREFIX=i686-w64-mingw32-
endif
MINGW1=$(shell which $(MINGW1_PREFIX)cc 2> /dev/null)
MINGW2=$(shell which $(MINGW2_PREFIX)gcc 2> /dev/null)
ifneq ("$(MINGW1)","")
	CC=$(MINGW1_PREFIX)cc
	WINDRES=$(MINGW1_PREFIX)windres
else ifneq ("$(MINGW2)","")
	CC=$(MINGW2_PREFIX)gcc
	WINDRES=$(MINGW2_PREFIX)windres
else
$(error MinGW not found)
endif
OUT_EXE			= vmmstatus.exe
OBJS			= main.o resource.o
RM			= rm -f

.PHONY : all
all : $(OUT_EXE)

.PHONY : clean
clean :
	$(RM) $(OBJS) $(OUT_EXE)

$(OUT_EXE) : $(OBJS) ../common/call_vmm.c ../common/call_vmm.h
	$(CC) -s -mwindows -o $(OUT_EXE) $(OBJS)  ../common/call_vmm.c \
		-lcomctl32

main.o : main.c resource.h ../common/call_vmm.h
	$(CC) -Wall -c -o main.o main.c

resource.o : resource.rc resource.h vmmstatus.exe.manifest
	$(WINDRES) -i resource.rc -o resource.o
