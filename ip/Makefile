LWIP_VERSION=1.4.1

CFLAGS += -I$(DIR)/include -I$(DIR)/include/arch -I$(DIR)/include/lwip \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include/ipv4 \

objs-1 += lwip-$(LWIP_VERSION)/src/core/def.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/dhcp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/dns.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/init.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/mem.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/memp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/netif.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/pbuf.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/raw.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/stats.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/sys.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp_in.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/tcp_out.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/udp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/timers.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/autoip.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/igmp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/inet_chksum.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/icmp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/inet.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip_addr.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip_frag.o
objs-1 += lwip-$(LWIP_VERSION)/src/netif/etharp.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/api_lib.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/api_msg.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/err.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/netbuf.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/netdb.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/netifapi.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/sockets.o
objs-1 += lwip-$(LWIP_VERSION)/src/api/tcpip.o

objs-1 += ip_sys.o arch/sys_arch.o
objs-1 += ip_main.o net_main.o
objs-1 += echo-server.o echo-client.o echoctl.o
