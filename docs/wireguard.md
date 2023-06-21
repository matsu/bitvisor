## Guide to Create a WireGuard Connection on BitVisor

This guide describes how to create a WireGuard connection using BitVisor as a client with an external Linux server.

---

**Prerequisites**

Learn more about Wireguard at https://www.wireguard.com/

Wireguard whitepaper pdf https://www.wireguard.com/papers/wireguard.pdf

---

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**

- [Setup wireguard on linux server](#setup-wireguard-on-linux-server)
- [Setup wireguard on bitvisor](#setup-wireguard-on-bitvisor)
- [Start handshaking](#start-handshaking)
- [Sending message by telnet application](#sending-message-by-telnet-application)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->




## Setup Wireguard on linux server




You will need to install WireGuard on a Linux server, specifically in a demo environment running Ubuntu LTS<br>

1. To install WireGuard, execute the command `sudo apt install wireguard`.

2. Next, generate the configuration by following these steps:

- Go to the configuration generating website at https://www.wireguardconfig.com/.
- Fill in the required values, such as the **Listen Port** (e.g., 51820)  and **CIDR** (Classless Inter-Domain Routing e.g., 192.168.3.0/24) . In this case, the _Endpoint_ and _DNS_ fields are not needed.
- Modify the content in **Post-Up rule** if your network interface to the outer network is not named **eth0**.
- Enter '1' for the **Number of Clients**, as we only need one client for the BitVisor. Generate a keypair for both the Private Key and Public Key by pressing the button **Generate Config**.
- Below is an example server configuration. Please clear the contents of the file `/etc/wireguard/wg0.conf` and copy-paste the following content into the file:

```
[Interface]
Address = 192.168.3.1/24
ListenPort = 51820
PrivateKey = ENzDMHIDSsdg/l9TyXxsM0KjpKAW0b+O7ic2O777pHE=
PostUp = iptables -A FORWARD -i %i -j ACCEPT; iptables -t nat -A POSTROUTING -o wlp0s20f3 -j MASQUERADE
PostDown = iptables -D FORWARD -i %i -j ACCEPT; iptables -t nat -D POSTROUTING -o wlp0s20f3 -j MASQUERADE

[Peer]
PublicKey = Oicka7Rvoy9PweKGCwFtU1Xb4Gs0fcqBwLFJmtGMpSE=
AllowedIPs = 192.168.3.2/32

```
3. Wake up the wg0 network interface using the command `wg-quick up wg0`. After executing the command, it starts running the netif up script. You should see the interface listed when running `ifconfig`, as shown below:
```
wg0: flags=209<UP,POINTOPOINT,RUNNING,NOARP>  mtu 1420
        inet 192.168.3.1  netmask 255.255.255.0  destination 192.168.3.1
        unspec 00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00  txqueuelen 1000  (UNSPEC)
        RX packets 0  bytes 0 (0.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 0  bytes 0 (0.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0

```
4. Enable IP forwarding in the Linux kernel:

- Open the file /etc/sysctl.conf using a text editor. Find the line that says net.ipv4.ip_forward = 0.
- Uncomment the line by removing the "#" at the beginning, or if the line doesn't exist, add the following line: net.ipv4.ip_forward = 1.
- Save the changes and exit the text editor.

## Setup Wireguard on Bitvisor

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
2. Fill in the values for the **defconfig** in the BitVisor code, using the referenced values above. The `ipaddr_end_point` should be set as the server's external IP address. And the `peer_endint_port` is the server's listening port.

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

        }
```
#If the value of wg_listen_port remains zero, it will be assigned a random value in the range of 49152 to 65535.

3. Save the defconfig file and then navigate to the BitVisor source code directory. Run the command `make config` to access the configuration options. Ensure that 'WIREGUARD' is selected for building by marking it.
## Start handshaking

1. To enable WireGuard dynamic_debug logging on the server, use the following commands:
```
$ sudo modprobe wireguard
$ echo module wireguard +p | sudo tee /sys/kernel/debug/dynamic_debug/control
```
2. Compile the Bitvisor code and start running it as a client on Qemu.
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
## Sending message by telnet application
Now that the WireGuard connection has been established, we can utilize it to transmit TCP/IP packets.

**Server**

Method to set up a TCP echo server using a pre-existing Linux package:
 - Install "ncat" by running the command: `sudo apt install ncat`.
 - Then, execute `ncat -l -p 10007 -e /bin/cat` to open port 10007 for the echo server.


**Client**

1. Assume you're running the Bitvisor on Qemu. Try to add `hostfwd=tcp::11123-:23` in the parameter to the Qemu command line to set up port forwarding. Here's an example command:
`./qemu-system-x86_64 -cpu host -enable-kvm -bios ./OVMF.fd  -drive file=fat:rw:~/x86_test/,format=raw -drive file=ub2.qcow2 -nic user,model=e1000e,hostfwd=tcp::11123-:23 -M q35 -m 4096`
2. To enable telnet-dbgsh, you need to write this option in your **defconfig** file.
```
	.vmm = {
		...
		.telnet_dbgsh = 1,
	...
	},
```

3. To connect to the BitVisor telnet application from another terminal on the client machine, use the following command: `telnet localhost -- -11123` This command establishes a telnet connection to the local machine (localhost) on port 11123. The -- -11123 option tells telnet to connect to port 11123 on the local machine, which is being forwarded to port 23 on the BitVisor virtual machine.

Once you have successfully connected to the BitVisor telnet application using the command , you should see the "**>**" prompt indicating that you are now interacting with the BitVisor command line interface.
```
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
>
```

Under the BitVisor telnet prompt, you can enter the command `echoctl` to access the Echo Control functionality. Once you have entered echoctl, you can then enter `help` to see the usage and available options for the Echo Control feature.
```
> echoctl
echoctl> help
usage:
  client connect <ipaddr> <port> [netif]  Connect to echo server.
  client send                             Send a message to client.
  server start <port> [netif]             Start echo server.


```
To initiate the WireGuard connection, execute the following command: `client connect <ipaddr> <port> [netif]`.

Replace <ipaddr> with the server's IP address based on the following scenarios:

- If you are using the server's WireGuard-assigned IP address (e.g., 192.168.3.1), there is no need to specify the netif.
- If you are using the server's external IP address (e.g., 10.16.165.1), make sure to specify netif as wg1.

Then, enter `client send`. These commands will establish the WireGuard connection and send a hello message over the connection. If the server sends an echo message, you will receive it accordingly.
```
echoctl> client connect 192.168.3.1 10007
Connecting to server at 192.168.3.1:10007 (c0a80301)
Done.
echoctl> client send
Sending a message...
Done.
echoctl>
```
**Output result**

After executing the `client send` command, you can check the server to see if it has received any messages from BitVisor over the WireGuard connection, provided that the server has sent some echo messages.

Bitvisor log:
```
Jun 22 11:16:41 bitvisor: Connecting...
Jun 22 11:16:41 bitvisor: Connection established!
Jun 22 11:16:55 bitvisor: Hello, BitVisor!
```

**Transferring large files by TCP/IP**

When BitVisor acts as the file receiver, it is necessary to adjust the TCP/IP parameters of LWIP to improve throughput. You need to modify the content of ip/include/lwip/lwipopts.h in bitvisor source code and add the following lines:
```
#define TCP_MSS 1460
#define TCP_WND (16 * TCP_MSS)
```

These changes will optimize the performance when bitvisor is receiving files.
