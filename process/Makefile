CFLAGS += -Iprocess/lib

bins-1 += debug help init log panic recvexample sendexample sendint
bins-1 += serialtest shell
bins-$(CONFIG_IDMAN) += idman
bins-$(CONFIG_STORAGE) += storage
bins-$(CONFIG_VPN) += vpn
asubdirs-1 += lib

debug-objs = debug.o
help-objs = help.o
idman-objs = idman.o
idman-libs = idman/lib/$(outa) crypto/$(outa)
init-objs = init.o
log-objs = log.o
panic-objs = panic.o
recvexample-objs = recvexample.o
sendexample-objs = sendexample.o
sendint-objs = sendint.o
serialtest-objs = serialtest.o
shell-objs = shell.o
storage-objs = storage.o
storage-libs = storage/lib/$(outa) crypto/$(outa)
vpn-objs = vpn.o
vpn-libs = vpn/lib/$(outa) crypto/$(outa)
