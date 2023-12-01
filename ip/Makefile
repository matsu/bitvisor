LWIP_VERSION=2.1.3
MBEDTLS_VERSION=2.28.6
CONSTANTS-$(CONFIG_TLS) += -DMBEDTLS_VMM
CONSTANTS-$(CONFIG_WIREGUARD) += -DWIREGUARD_VMM

CFLAGS += -I$(DIR)/include -I$(DIR)/include/arch -I$(DIR)/include/lwip \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/include/lwip \
	-I$(DIR)/wireguard/include

CFLAGS-TLS-$(CONFIG_TLS) += -I$(DIR)/include/mbedtls \
	-I$(DIR)/mbedtls/mbedtls-$(MBEDTLS_VERSION)/include \
	-I$(DIR)/mbedtls/mbedtls-$(MBEDTLS_VERSION)/library \
	-I$(DIR)/lwip-$(LWIP_VERSION)/src/apps/altcp_tls \
	-DMBEDTLS_CONFIG_FILE='<config_mbedtls_2.h>'
CFLAGS += $(CFLAGS-TLS-1)

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
objs-$(CONFIG_TLS) += tls-echo-client.o \
tls-echo-server.o \
lwip-$(LWIP_VERSION)/src/core/altcp.o \
lwip-$(LWIP_VERSION)/src/core/altcp_alloc.o \
lwip-$(LWIP_VERSION)/src/core/altcp_tcp.o \
lwip-$(LWIP_VERSION)/src/apps/altcp_tls/altcp_tls_mbedtls_mem.o \
mbedtls/altcp_tls_mbedtls.o mbedtls/mbedtls_helpers.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/aes.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/aesni.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/aria.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/asn1parse.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/asn1write.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/base64.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/bignum.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/camellia.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ccm.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/chacha20.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/chachapoly.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/cipher_wrap.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/cipher.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/cmac.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/constant_time.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ctr_drbg.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/debug.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/des.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/dhm.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ecdh.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ecdsa.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ecjpake.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ecp_curves.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ecp.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/entropy_poll.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/entropy.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/error.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/gcm.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/hkdf.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/hmac_drbg.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/md.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/md5.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/memory_buffer_alloc.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/mps_reader.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/mps_trace.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/nist_kw.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/oid.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/pem.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/pk_wrap.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/pk.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/pkcs5.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/pkcs12.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/pkparse.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/pkwrite.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/platform_util.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/platform.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/poly1305.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_aead.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_cipher.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_client.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_driver_wrappers.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_ecp.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_hash.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_mac.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_rsa.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_se.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_slot_management.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto_storage.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/psa_crypto.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ripemd160.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/rsa.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/sha1.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/sha256.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/sha512.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_cache.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_ciphersuites.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_cookie.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_msg.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_ticket.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_tls.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_tls13_keys.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/timing.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/version_features.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/version.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/x509_create.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/x509_crl.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/x509_crt.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/x509_csr.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/x509.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/rsa_internal.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_cli.o \
mbedtls/mbedtls-$(MBEDTLS_VERSION)/library/ssl_srv.o \
