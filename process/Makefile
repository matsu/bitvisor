CFLAGS += -Iprocess/lib
CONSTANTS-$(CONFIG_TLS) += -DMBEDTLS_VMM

bins-y += debug help init log panic recvexample sendexample sendint
bins-y += serialtest shell
bins-$(CONFIG_IDMAN) += idman
bins-$(CONFIG_STORAGE) += storage
bins-$(CONFIG_VPN) += vpn
asubdirs-y += lib
bins-$(CONFIG_IP) += echoctl
bins-$(CONFIG_TLS) += echoctl_tls
bins-$(CONFIG_PCI_MONITOR) += monitor
psubdirs-$(CONFIG_SQLITE) += sqlite

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
echoctl-objs = echoctl.o
echoctl-objs += echoctl_common.o
echoctl_tls-objs = echoctl_tls.o
echoctl_tls-objs += echoctl_common.o
monitor-objs = monitor.o

$(dir)sqlite/$(outp_p) : $(dir)lib/$(outa_p)
