obj-m := logget-linux.o

CurrentKernel :
	make -C /lib/modules/`uname -r`/build SUBDIRS=`pwd` modules

clean :
	-rm -f Module.symvers *.o *.ko *.mod.c .*.cmd *~ .tmp_versions/''*
	-rmdir .tmp_versions

load :
	-rmmod logget-linux
	insmod logget-linux.ko

unload :
	rmmod logget-linux
