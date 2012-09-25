MINGW_PREFIX_DEBIAN	= i586-mingw32msvc-
MINGW_PREFIX		= $(MINGW_PREFIX_DEBIAN)
OUT_EXE			= vmmstatus.exe
OBJS			= main.o resource.o
WINDRES			= $(MINGW_PREFIX)windres
CC			= $(MINGW_PREFIX)cc
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
