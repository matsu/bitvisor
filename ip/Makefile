LWIP_VERSION=2.1.3
CONSTANTS-$(CONFIG_WIREGUARD) += -DWIREGUARD_VMM

CFLAGS += -I$(DIR)/include -I$(DIR)/include/arch -I$(DIR)/include/lwip \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include/lwip \
	-I$(DIR)/wireguard/include

objs-1 += lwip-$(LWIP_VERSION)/src/core/def.o
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
objs-1 += lwip-$(LWIP_VERSION)/src/core/timeouts.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ip.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/inet_chksum.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/autoip.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/dhcp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/etharp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/igmp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/icmp.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip4.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip4_addr.o
objs-1 += lwip-$(LWIP_VERSION)/src/core/ipv4/ip4_frag.o
objs-1 += lwip-$(LWIP_VERSION)/src/netif/ethernet.o
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
objs-1 += telnet-server.o telnet-dbgsh.o
objs-1 += string.o
objs-$(CONFIG_WIREGUARD) += wireguard/wg_main.o \
wireguard/wireguard-lwip/src/crypto/refc/blake2s.o \
wireguard/wireguard-lwip/src/crypto/refc/chacha20.o \
wireguard/wireguard-lwip/src/crypto/refc/chacha20poly1305.o \
wireguard/wireguard-lwip/src/crypto/refc/poly1305-donna.o \
wireguard/wireguard-lwip/src/crypto/refc/x25519.o \
wireguard/wireguard-lwip/src/wireguardif.o \
wireguard/wireguard-lwip/src/crypto.o \
wireguard/wireguard-lwip/src/wireguard.o wireguard/wireguard-platform.o \
wireguard/wireguard-setup.o
