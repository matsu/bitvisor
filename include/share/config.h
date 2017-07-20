/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2017 Igel Co., Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __SHARE_CONFIG_H
#define __SHARE_CONFIG_H

#define NUM_OF_STORAGE_KEYS 16
#define NUM_OF_STORAGE_KEYS_CONF 16
#define STORAGE_GUID_NULL {0, 0, 0, {0, 0, 0 ,0 ,0, 0, 0, 0}}
#define STORAGE_GUID_ANY {0xFFFFFFFF, 0xFFFF, 0xFFFF, \
			  {0xFF, 0xFF, 0xFF ,0xFF ,0xFF, 0xFF, 0xFF, 0xFF}}
#define STORAGE_DEVICE_ID_ANY	0xFFFF
#define STORAGE_HOST_ID_ANY	0xFF

enum storage_type {
	STORAGE_TYPE_NULL,
	STORAGE_TYPE_ATA,
	STORAGE_TYPE_ATAPI,
	STORAGE_TYPE_USB,
	STORAGE_TYPE_AHCI,
	STORAGE_TYPE_AHCI_ATAPI,
	STORAGE_TYPE_ANY = 0xFF,
};

enum idman_authmethod {
	IDMAN_AUTH_NONE,
	IDMAN_AUTH_PKI,
	IDMAN_AUTH_IDPASSWD,
	IDMAN_AUTH_PASSWD,
};

struct config_data_idman {
	char crl01[4096];
	char crl02[4096];
	char crl03[4096];
	char pkc01[4096];
	char pkc02[4096];
	char pkc03[4096];
	char pin[64];
	unsigned int randomSeedSize;
	unsigned int maxPinLen;
	unsigned int minPinLen;
	enum idman_authmethod authenticationMethod;
};

struct config_data_vpn {
	char mode[16];
	char virtualGatewayMacAddress[32];
	char bindV4[8];
	char guestIpAddressV4[32];
	char guestIpSubnetV4[32];
	char guestMtuV4[16];
	char guestVirtualGatewayIpAddressV4[32];
	char dhcpV4[8];
	char dhcpLeaseExpiresV4[16];
	char dhcpDnsV4[32];
	char dhcpDomainV4[256];
	char adjustTcpMssV4[16];
	char hostIpAddressV4[32];
	char hostIpSubnetV4[32];
	char hostMtuV4[16];
	char hostIpDefaultGatewayV4[32];
	char optionV4ArpExpires[16];
	char optionV4ArpDontUpdateExpires[8];
	char vpnGatewayAddressV4[32];
	char vpnAuthMethodV4[16];
	char vpnPasswordV4[1024];
	char vpnIdStringV4[1024];
	char vpnCertV4[4096];
	char vpnCaCertV4[4096];
	char vpnRsaKeyV4[4096];
	char vpnSpecifyIssuerV4[8];
	char vpnPhase1ModeV4[16];
	char vpnPhase1CryptoV4[16];
	char vpnPhase1HashV4[16];
	char vpnPhase1LifeSecondsV4[16];
	char vpnPhase1LifeKilobytesV4[16];
	char vpnWaitPhase2BlankSpanV4[16];
	char vpnPhase2CryptoV4[16];
	char vpnPhase2HashV4[16];
	char vpnPhase2LifeSecondsV4[16];
	char vpnPhase2LifeKilobytesV4[16];
	char vpnConnectTimeoutV4[16];
	char vpnIdleTimeoutV4[16];
	char vpnPingTargetV4[32];
	char vpnPingIntervalV4[16];
	char vpnPingMsgSizeV4[16];
	char bindV6[8];
	char guestIpAddressPrefixV6[64];
	char guestIpAddressSubnetV6[16];
	char guestMtuV6[16];
	char guestVirtualGatewayIpAddressV6[64];
	char raV6[8];
	char raLifetimeV6[16];
	char raDnsV6[64];
	char hostIpAddressV6[64];
	char hostIpAddressSubnetV6[16];
	char hostMtuV6[16];
	char hostIpDefaultGatewayV6[64];
	char optionV6NeighborExpires[16];
	char vpnGatewayAddressV6[64];
	char vpnAuthMethodV6[16];
	char vpnPasswordV6[1024];
	char vpnIdStringV6[1024];
	char vpnCertV6[4096];
	char vpnCaCertV6[4096];
	char vpnRsaKeyV6[4096];
	char vpnSpecifyIssuerV6[8];
	char vpnPhase1ModeV6[16];
	char vpnPhase1CryptoV6[16];
	char vpnPhase1HashV6[16];
	char vpnPhase1LifeSecondsV6[16];
	char vpnPhase1LifeKilobytesV6[16];
	char vpnWaitPhase2BlankSpanV6[16];
	char vpnPhase2CryptoV6[16];
	char vpnPhase2HashV6[16];
	char vpnPhase2LifeSecondsV6[16];
	char vpnPhase2LifeKilobytesV6[16];
	char vpnPhase2StrictIdV6[8];
	char vpnConnectTimeoutV6[16];
	char vpnIdleTimeoutV6[16];
	char vpnPingTargetV6[64];
	char vpnPingIntervalV6[16];
	char vpnPingMsgSizeV6[16];
} __attribute__ ((packed));

struct guid {
	u32 data1;
	u16 data2;
	u16 data3;
	u8  data4[8];
} __attribute__ ((packed));

struct storage_keys_conf {
	struct guid guid;
	enum storage_type type;
	u8 host_id;
	u16 device_id;
	u64 lba_low, lba_high;
	char crypto_name[8];
	u8 keyindex;
	u16 keybits;
	char extend[256];
} __attribute__ ((packed));

struct config_data_storage {
	u8 keys[NUM_OF_STORAGE_KEYS][32];
	struct storage_keys_conf keys_conf[NUM_OF_STORAGE_KEYS_CONF];
} __attribute__ ((packed));

struct config_data_vmm_driver_vpn {
	int PRO100;
	int PRO1000;
	int RTL8169;
	int ve;
};

struct config_data_vmm_driver_usb {
	int uhci;
	int ehci;
};

struct config_data_vmm_driver {
	int ata;
	struct config_data_vmm_driver_usb usb;
	int concealEHCI;
	int conceal1394;
	int concealPRO1000;
	int vga_intel;
	struct config_data_vmm_driver_vpn vpn;
	char pci_conceal[1024];
	char pci[2048];
};

struct config_data_vmm_iccard {
	int enable;
	int status;
};

struct config_data_vmm_tty_syslog {
	int enable;
	u8 src_ipaddr[4];
	u8 dst_ipaddr[4];
};

struct config_data_ip {
	u8 ipaddr[4];
	u8 netmask[4];
	u8 gateway[4];
	int use_dhcp;
};

struct config_data_vmm {
	char randomSeed[1024];
	int f11panic;
	int f12msg;
	int auto_reboot;
	int panic_reboot;
	int shell;
	int dbgsh;
	int status;
	int boot_active;
	int no_intr_intercept;
	int ignore_tsc_invariant;
	int unsafe_nested_virtualization;
	char tty_mac_address[6];
	int tty_pro1000;
	int tty_rtl8169;
	int tty_x540;
	int tty_ieee1394;
	struct config_data_vmm_driver driver;
	struct config_data_vmm_iccard iccard;
	struct config_data_vmm_tty_syslog tty_syslog;
};

struct config_data {
	int len;
	struct config_data_idman idman;
	struct config_data_vpn vpn;
	struct config_data_ip ip;
	struct config_data_storage storage;
	struct config_data_vmm vmm;
} __attribute__ ((packed));

#endif
