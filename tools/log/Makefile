obj-m := logget-linux.o

CurrentKernel :
	make -C /lib/modules/`uname -r`/build M=`pwd` modules

clean :
	-rm -f Module.symvers *.o *.ko *.mod.c .*.cmd *~ .tmp_versions/''* \
		modules.order *.mod
	-rmdir .tmp_versions

# GNU Make understands "load :" as a load directive.  Do not put
# spaces between "load" and ":" for workaround.
load:
	-rmmod logget-linux
	insmod logget-linux.ko

unload :
	rmmod logget-linux
