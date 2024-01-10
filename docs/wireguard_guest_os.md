## Guide for establishing a WireGuard connection on BitVisor for guest OS

This guide describes how to create a WireGuard connection using BitVisor as a client with an external Linux server. Ensuring the guest OS remains unaware of traffic routing through a remote WireGuard server for IP packets.

---

**Prerequisites**

Learn more about WireGuard at https://www.wireguard.com/

WireGuard whitepaper pdf https://www.wireguard.com/papers/wireguard.pdf

---
<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Setup WireGuard on Linux server](#setup-wireguard-on-linux-server)
- [Setup WireGuard on BitVisor](#setup-wireguard-on-bitvisor)
- [Observe handshaking in the log](#observe-handshaking-in-the-log)
- [Observe the network traffic of the guest OS](#observe-the-network-traffic-of-the-guest-os)
<!-- END doctoc generated TOC please keep comment here to allow auto update -->
## Setup WireGuard on Linux server

You will need to install WireGuard on a Linux server, specifically in a demo environment running Ubuntu LTS:

1. To install WireGuard, execute the command `sudo apt install wireguard`.

2. Next, generate the configuration by following these steps:

- Go to the configuration generating website at https://www.wireguardconfig.com/.
- Fill in the required values, such as the **Listen Port** (e.g., 51820) and **CIDR** (Classless Inter-Domain Routing e.g., 192.168.3.0/24). In this case, the _Endpoint_ and _DNS_ fields are not needed.
- Modify the content in **Post-Up rule** and **Post-Down rule** if your network interface to the outer network is not named **eth0**.
- Enter '1' for the **Number of Clients**, as we only need one client for the BitVisor. Generate a keypair for both the Private Key and Public Key by pressing the button **Generate Config**.
- Below is an example server configuration. Please clear the contents of the file `/etc/wireguard/wg0.conf` and copy-paste the following content into the file.
- Remember to modify the `AllowedIPs` to match your specific configuration, such as `192.168.3.0/24`, as it represents a range of IP addresses. This configuration allows both the guest OS and BitVisor to establish connections based on your chosen IP range.

An exmaple of `wg0.conf`:
```
[Interface]
Address = 192.168.3.1/24
ListenPort = 51820
PrivateKey = ENzDMHIDSsdg/l9TyXxsM0KjpKAW0b+O7ic2O777pHE=
PostUp = iptables -A FORWARD -i %i -j ACCEPT; iptables -t nat -A POSTROUTING -o wlp0s20f3 -j MASQUERADE
PostDown = iptables -D FORWARD -i %i -j ACCEPT; iptables -t nat -D POSTROUTING -o wlp0s20f3 -j MASQUERADE

[Peer]
PublicKey = Oicka7Rvoy9PweKGCwFtU1Xb4Gs0fcqBwLFJmtGMpSE=
AllowedIPs = 192.168.3.0/24

```
3. Wake up the wg0 network interface using the command `wg-quick up wg0`. After executing the command, it starts running the netif up script. You should see the interface listed when running `ifconfig`, as shown below:
```
wg0: flags=209<UP,POINTOPOINT,RUNNING,NOARP>  mtu 1420
	inet 192.168.3.1  netmask 255.255.255.0  destination 192.168.3.1
	unspec 00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00 txqueuelen 1000 (UNSPEC)
	RX packets 0  bytes 0 (0.0 B)
	RX errors 0  dropped 0  overruns 0  frame 0
	TX packets 0  bytes 0 (0.0 B)
	TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```
4. Enable IP forwarding in the Linux kernel:

- Open the file /etc/sysctl.conf using a text editor. Find the line that says `net.ipv4.ip_forward = 0`.
- Uncomment the line by removing the "#" at the beginning, or if the line doesn't exist, add the following line: `net.ipv4.ip_forward = 1`.
- Save the changes and exit the text editor.
- Run the following command in the Linux command line: `sudo sysctl -p`, to apply the changes.

## Setup WireGuard on BitVisor

1. Go back to the configuration generating website and navigate to the **Client 1** section. Here is an example of the configuration for **Client 1**:
```
[Interface]
Address = 192.168.3.2/24
ListenPort = 51820
PrivateKey = eA9xQbuO91WFFpfPNLxkUDTsAaiZWP8ISjnZVYpL1lg=

[Peer]
PublicKey = L+9I+jLcfQVM6R/Aw77c7GIDoD/bTofMt5LK6wRqlmw=
AllowedIPs = 0.0.0.0/0, ::/0
```
2. Fill in the values for the **defconfig** in the BitVisor code, using the referenced values.
To configure the `.vmm` structure in this setup, you should include the `net=ipwggos` parameter within the `.vmm.driver.pci` structure as follows:
```
	.vmm = {
...
		.driver = {
			.pci = "driver=pro1000,net=ipwggos,tty=1,virtio=1",
		},
	},
...
```
The values of `.wg` structure:
- The `ipaddr_end_point` should be set as the server's external IP address. And the `peer_endpoint_port` is the server's listening port.
- Directly copy and paste the PrivateKey and PublicKey obtained from 1. as `wg_private_key` and `peer_public_key`.
- The `ipaddr` here is used for communication by BitVisor.
- If the value of `wg_listen_port` remains zero, it will be assigned a random value in the range of 49152 to 65535.

The values of `.wg_gos` structure:
- BitVisor will use the .wg_gos configuration to manage network settings for the guest operating system. When BitVisor intercepts a DHCP request from the guest OS, it responds with a customized DHCP offer and DHCP ACK. This process includes offering the IP address and DNS server specified in the `.wg_gos` structure.

- `ipaddr` (192.168.3.3) is used to assign an IP address to the guest OS, ensuring that it can communicate with the WireGuard server.
- `dns` (8.8.8.8) is used to specify the DNS server for the guest OS, allowing it to resolve domain names to IP addresses. This setting is crucial for enabling the guest OS to access the internet and other network services correctly.
- `mac_gateway` (02-48-84-76-71-00) This is the Ethernet MAC address of the virtual gateway network card. You can set it to the same MAC Address as the Linux server, or you can define it yourself according to the situation.
```
	.wg = {
		.ipaddr = {192, 168, 3, 2},
		.netmask = {255, 255, 255, 0},
		.gateway = {192, 168, 3, 1},
		.ipaddr_end_point = {10, 16, 165, 1},
		.peer_allowed_ip = {0, 0, 0, 0},
		.peer_allowed_mask = {0, 0, 0, 0},
		.peer_endpoint_port = 51820,
		.wg_listen_port = 12345,
		.wg_private_key = "eA9xQbuO91WFFpfPNLxkUDTsAaiZWP8ISjnZVYpL1lg=",
		.peer_public_key = "L+9I+jLcfQVM6R/Aw77c7GIDoD/bTofMt5LK6wRqlmw=",
	},
	.wg_gos = {
		.ipaddr = {192, 168, 3, 3},
		.dns = {8, 8, 8, 8},
		.mac_gateway = { 0x02, 0x48, 0x84, 0x76, 0x71, 0x00 },
	},
```

- `wg.ipaddr` and `wg_gos.ipaddr` must be two different IPs. Otherwise, packets may be received incorrectly.

3. Save the defconfig file and then navigate to the BitVisor source code directory. Run the command `make config` to access the configuration options. Ensure that 'WIREGUARD' is selected for building by marking it.
## Observe handshaking in the log
All the preparatory work is complete. Let's see what effects we have achieved.

1. To enable WireGuard dynamic_debug logging on the server, use the following commands:
```
$ sudo modprobe wireguard
$ echo module wireguard +p | sudo tee /sys/kernel/debug/dynamic_debug/control
```
2. Compile the BitVisor code and start running it as a client on Qemu.
3. To check the syslog on the server, use the command `dmesg -wT`. This command displays the system log messages in real-time. If you don't see the desired WireGuard-related information, you can try refreshing the WireGuard connection by executing the following command:
`wg-quick down wg0; wg-quick up wg0` and wait for approximately 1 minute for BitVisor to reconnect.
This command first brings down the WireGuard interface (wg0) and then brings it back up.
When the WireGuard module is initialized and the WireGuard interface starts up, you may see logging messages similar to the following:
```
[6/21 18:47:27 2023] wireguard: WireGuard 1.0.0 loaded. See www.wireguard.com for information.
[6/21 18:47:27 2023] wireguard: Copyright (C) 2015-2019 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
[6/21 18:48:02 2023] wireguard: wg0: Interface created
[6/21 18:48:02 2023] wireguard: wg0: Peer 1 created
[6/21 18:48:13 2023] wireguard: wg0: Receiving handshake initiation from peer 1 (10.16.129.151:52693)
[6/21 18:48:13 2023] wireguard: wg0: Sending handshake response to peer 1 (10.16.129.151:52693)
[6/21 18:48:13 2023] wireguard: wg0: Keypair 1 created for peer 1
[6/21 18:48:13 2023] wireguard: wg0: Receiving keepalive packet from peer 1 (10.16.129.151:52693)
[6/21 18:48:23 2023] wireguard: wg0: Receiving keepalive packet from peer 1 (10.16.129.151:52693)

```
The messages typically indicate the successful initialization of the WireGuard module, the initialization of the WireGuard interface (e.g., wg0), and the authentication and handshake processes with peers.

Note :
- Even if you're not actively sending data to the server, the WireGuard protocol sends keepalive packets every 10 seconds by default. This ensures that the connection remains active and allows for efficient detection of any connectivity issues.
- In the log messages, the client port number shown is randomly generated for each connection. This is a security measure to help protect against potential port scanning and improve the overall security of the connection.

## Observe the network traffic of the guest OS

You can enter the following command in the guest OS: `ifconfig`.
```
enp0s2: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
	inet 192.168.3.3  netmask 255.255.255.0  broadcast 192.168.3.255
	inet6 fe80::fd99:94ae:1e02:6666  prefixlen 64  scopeid 0x20<link>
	ether 52:54:00:12:34:56  txqueuelen 1000  (Ethernet)
	RX packets 425  bytes 301200 (301.2 KB)
	RX errors 0  dropped 0  overruns 0  frame 0
	TX packets 163  bytes 15289 (15.2 KB)
	TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0

lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65536
	inet 127.0.0.1  netmask 255.0.0.0
	loop  txqueuelen 1000  (Local Loopback)
	RX packets 112  bytes 9258 (9.2 KB)
	RX errors 0  dropped 0  overruns 0  frame 0
	TX packets 112  bytes 9258 (9.2 KB)
	TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```
- As you can see, the guest OS will receive an IP of 192.168.3.3 based on the DHCP offer and ARP response sent by BitVisor. It will treat 192.168.3.1 as its gateway. Therefore, all IP packets will be routed through the WireGuard server for forwarding.

- Similarly, the WireGuard server will also collect packets that belong to the guest OS and forward them. However, ARP packets will not pass through WireGuard.

You can also observe the encrypted traffic on your Qemu host or WireGuard server using the following command: `sudo tshark -i eth0`.
```
26218 1366.608523871 10.16.129.151 → 10.16.165.1  WireGuard 122 Transport Data, receiver=0x2E3403FF, counter=8, datalen=48
26219 1366.630763137  10.16.165.1 → 10.16.129.151 WireGuard 154 Transport Data, receiver=0x6D1C11B6, counter=3, datalen=80
26220 1366.711835840  10.16.165.1 → 10.16.129.151 WireGuard 138 Transport Data, receiver=0x6D1C11B6, counter=4, datalen=64
26221 1366.712224533 10.16.129.151 → 10.16.165.1  WireGuard 138 Transport Data, receiver=0x2E3403FF, counter=9, datalen=64
26222 1366.714007834 10.16.129.151 → 10.16.165.1  WireGuard 218 Transport Data, receiver=0x2E3403FF, counter=10, datalen=144
26223 1366.749727945 10.16.129.151 → 10.16.165.1  WireGuard 154 Transport Data, receiver=0x2E3403FF, counter=11, datalen=80
26224 1366.838573780 10.16.129.151 → 10.16.165.1  WireGuard 330 Transport Data, receiver=0x2E3403FF, counter=12, datalen=256
26225 1366.922162203  10.16.165.1 → 10.16.129.151 WireGuard 330 Transport Data, receiver=0x6D1C11B6, counter=5, datalen=256
26226 1366.922527905 10.16.129.151 → 10.16.165.1  WireGuard 138 Transport Data, receiver=0x2E3403FF, counter=13, datalen=64
26227 1366.922549249  10.16.165.1 → 10.16.129.151 WireGuard 138 Transport Data, receiver=0x6D1C11B6, counter=6, datalen=64
```

