CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS
CONSTANTS-$(CONFIG_NET_VIRTIO_NET) += -DVIRTIO_NET

CFLAGS += -Idrivers/include -Ivpn/lib

objs-$(CONFIG_NET_PRO100) += pro100.o
objs-$(CONFIG_NET_PRO1000) += pro1000.o
objs-$(CONFIG_NET_RTL8169) += rtl8169.o
objs-$(CONFIG_NET_RE) += re.o re_core.o
objs-$(CONFIG_NET_BNX) += bnx.o
objs-$(CONFIG_NET_AQ) += aq.o
objs-$(CONFIG_NET_VIRTIO_NET) += virtio_net.o
objs-$(CONFIG_NET_V_VIRTIO_NET) += virtual_virtio_net.o
