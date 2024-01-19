# Getting started with BitVisor

This is an introductory guide on how to run BitVisor.

## Downloading BitVisor

BitVisor code is currently hosted on
https://sourceforge.net/p/bitvisor/code/ci/default/tree/.

It is recommended to download the source using
[Mercurial](https://www.mercurial-scm.org/).

```
hg clone http://hg.code.sf.net/p/bitvisor/code bitvisor-code
```

## Preparing syslog

(Based on https://qiita.com/hdk_2/items/2431710fcf257904fc8a)

It is a good idea to prepare receiving syslog from BitVisor so that we can trace
what is going on. We use `rsyslog` for this guide.

In `/etc/rsyslog.conf`, enable UDP reception by having the following
configuration. The syntax depends on the rsyslog version.

```
# provides UDP syslog reception (Legacy format)
$ModLoad imudp
$UDPServerRun 514
```

or

```
# Read syslog messages from UDP
module(load="imudp")
input(type="imudp" port="514")
```

Next, create `/etc/rsyslog.d/10-bitvisor.conf` with the following content to
redirect logs with `bitvisor:` in the tag to `/var/log/bitvisor.log`.

```
:syslogtag,isequal,"bitvisor:" /var/log/bitvisor.log
& ~
```

or

```
:syslogtag, isequal, "bitvisor:" {
	action(type="omfile" file="/var/log/bitvisor.log")
	stop
}
```

Then, restart rsyslogd (Change the following command according to the init
system).

```
# service rsyslog restart
```

Finally, confirm that the UDP port 514 is bound.

```
$ ss -uln | grep 514
UNCONN     0      0                         *:514                      *:*
UNCONN     0      0                        :::514                     :::*
```

## Configuration

Let's configure BitVisor to make it send logs over the network.

Run `make config` from the source directory to configure options. This generates
a default compile time configuration. Some options configuration might generate
warnings during compiling. There is no need to worry those warning messages at
the moment. They are either from external code or unmaintained code from
research in the past. The maintainer wants to leave them intact at the moment.

The following options are recommended to getting started with BitVisor:

```
- 64
- CPU_MMU_SPT_1
- CPU_MMU_SPT_USE_PAE
- DBGSH
- ACPI_DSDT
- DISABLE_SLEEP
- ENABLE_ASSERT
- NET_DRIVER
- One of [NET_PRO1000, NET_RE, NET_BNX, NET_AQ] depending on your NIC
- NET_VIRTIO_NET (To make guest see the NIC as virtio network device)
- ACPI_TIME_SOURCE
- TCG_BIOS
- BACKTRACE
- IP (For IP stack)
- THREAD_1CPU (For performance reason)
- ACPI_IGNORE_ERROR
- DISABLE_VTD
- DMAR_PASS_THROUGH
```

Next, it is time to configure runtime parameters with the `defconfig` file.
First we copy `defconfig.tmpl` to `defconfig`. Then, we configure necessary
parameters for getting logs over the network via syslog.

The config structure definition is located at `include/share/config.h`. The
syntax is the plain C struct.

In the `.vmm` section, the `.tty_mac_address` member contains the MAC address of
the receiver. Fill them byte by byte. Next is the `.tty_syslog` member. Set
`.enable` to 1, `.src_ipaddr` to the IP address we want to see in syslog,
`.dst_ipaddr` to the IP of the syslog receiver. Note that MAC address can be any
number if we run on QEMU user network. For simplicity, we can just use the MAC
address of the host's active network interface.

Next, we specify the driver we want to enable NIC driver in `.driver` member.

* Intel => pro1000
* Broadcom => bnx
* Realtek  => re
* Aquntia  => aq

Here is the example configuration for enabling `pro1000` network driver.

```
	.driver = {
		.pci = "driver=pro1000, net=ippass, tty=1, virtio=1",
	}
```

`driver=driver_name` tells BitVisor to enable a driver. We can enable multiple
drivers by conjugating with `and` like `dirver=driver_a,and,driver=driver_b`.

A driver may accept parameters. In case of network drivers, they accept `net`,
`tty`, and `virtio`.

`net` tells whether BitVisor should start its network stack (through lwip) or
not. Currently, we have:

* net=pass -> BitVisor doesn't run IP stack, traffic goes to the guest only.
* net=ip -> BitVisor runs IP stack, traffic does not go to the guest at all.
* net=ippass-> BitVisor runs IP stack, traffic goes to both the host and the
guest.
* net=ippassfilter -> Like ippass but it tries to separate traffic if possible.
* net=ippassfilter2 -> Like ippassfilter but the guest will receive at least
1 of 100 packets to prevent the spurious interrupt problem on Linux.

`ippassfilter2` is recommended for `bnx`, `re`, and `aq` as they are using pin
interrupts at the moment. `pro1000` uses MSI interrupts so it does not suffer
from spurious interrupt problem (On some platform, pin interrupt is not
available on Intel NIC).

BitVisor can only have one network stack. This means only an interface is
allowed to run with `net=ip*` option.

For more information on `ippassfilter`, see the
`ip: add net=ippassfilter, TCP/IP stack + pass-through + IP address based filter`
commit log.

`tty` tells BitVisor whether it should send logs over the network. Setting this
parameter is necessary.

`virtio` tells whether it should expose network device to the guest as virtio
network device. It is recommended to set `virtio=1`. `bnx`, `re`, and `aq`
require this option to be set.

Next is to configure the IP address of BitVisor by adding `.ip` member to the
config data.

```
struct config_data config = {
	...
	.ip = {
		.use_dhcp = 0,
		.ipaddr = {192, 168, 0, 7},
		.netmask = {255, 255, 255, 0},
		.gateway = {192, 168, 0, 1}
	}
}
```

For simplicity, we use a static IP address in this guide.

## Compiling BitVisor and UEFI loader

To compile BitVisor, go to the base directory, and run the `make` command. This
produces the `bitvisor.elf` on the project root directory.

Now, we have the BitVisor binary. Next is to compile the UEFI loader. Switch
directory to `boot/uefi-loader`, and run the `make` command. This produces the
`loadvmm.efi`.

## Running BitVisor

For running on real x86_64 machines, the simplest way is to start UEFI shell
from a thumbdrive, and launch BitVisor. Let's assume we have the following
environment.

```
[target machine]------[network]------[syslog receiver machine]
```

First thing to do is to prepare the UEFI shell. It can be built from EDK2 source
(https://github.com/tianocore/edk2). However, we can also get the pre-compiled
version from some Linux distribution like Archlinux
(https://wiki.archlinux.org/title/Unified_Extensible_Firmware_Interface#UEFI_Shell).
Make sure the shell EFI file is for x86_64 architecture.

Then, format the thumbdrive to FAT32, and structure the thumbdrive to look like
the following:

```
your_thumbdrive/
|
--- EFI/
|    |
|    --- boot/
|          |
|          --- bootx64.efi
|
--- bitvisor/
|    |
|    --- loadvmm.efi
|    |
|    --- bitvisor.elf
```

When we tell UEFI firmware to boot from an external drive, the firmware looks
for `EFI\boot\bootx64.efi` by default. We rename the shell EFI file to
`bootx64.efi`, and put it in the `EFI\boot` folder.

Plug in the thumbdrive we prepared to the machine we want to run BitVisor. Then,
choose to boot from the thumbdrive, and we should see the EFI shell.

On the screen, there should be information regarding available file system
information `fs0`, `fs1`, etc. Typically, the `fsX` order is internal drives
first, followed by external drives. Note the number following `fs` is a
hexadecimal number without `0x` prefix.

To switch between file system partition, type `fsX:`, where `X` is the number,
and enter. Once we enter a file system, we can use `ls` command to show current
directory content, and `cd` command for changing directory.

Switch to the thumbdrive file system. Then, enter the `bitvisor` folder. After
that, type `loadvmm.efi` to start BitVisor. We should see some logs on the
screen similar the following:

```
Copyright (c) 2007, 2008 University of Tsukuba
All rights reserved.
ACPI DMAR not found.
FACS address 0x7FBFD000
Module not found.
Processor 0 (BSP)
oooooooooooooooooooooooooooooooooooooooooooooooooo
Disable ACPI S3
Using VMX.
Processor 0 2592519392 Hz
Loading drivers.
...
Uninstall 2 protocol(s) successfully
[0000:00:02.0] Disconnected PCI device drivers
Wait for PHY reset and link setup completion.
PCI: 6 devices found
MCFG [0] 0000:00-FF (B0000000,10000000)
Starting a virtual machine.
```

On the syslog receiver side, we should see the logs when the network setup is
correct in `/var/log/bitvisor.log`.

```
Feb  3 17:30:45 x260 bitvisor: PCI: 6 devices found
Feb  3 17:30:45 x260 bitvisor: MCFG [0] 0000:00-FF (B0000000,10000000)
Feb  3 17:30:45 x260 bitvisor: Starting a virtual machine.
Feb  3 17:30:45 x260 bitvisor: IP address changed: 0.0.0.0 -> 192.168.1.7
Feb  3 17:30:45 x260 bitvisor: telnet server "dbgsh" ready
```

Once the control returns to the shell, it means BitVisor has been started
successfully. Right now, the machine is running in the guest mode. We can exit
the shell by typing `exit` command, and continue booting normally. Otherwise, we
can navigate other file system partition, and run boot an OS from its EFI loader
manually. For Linux, the kernel loader is usually at the
`EFI\distro_name\grubx64.efi`. For Windows, the loader is located at
`EFI\Microsoft\Boot\bootmgfw.efi`.

### Running BitVisor x86_64 on QEMU

We can try BitVisor quickly by using QEMU + Linux bootable ISO:

```
qemu-system-x86_64 -cpu host -enable-kvm -bios /path/to/OVMF.fd \
-drive file=fat:rw:~/x86_test/,format=raw -cdrom /path/to/bootable.iso \
-nic user,model=e1000e -M q35 -m 4096
```

`-bios` specifies which BIOS to use. In this case, it is `OVMF.fd` which is UEFI
firmware image. Put `loadvmm.efi` and `bitvisor.elf` in `~/x86_test` folder.
Don't forget to enable `pro1000` driver as `e1000e` is an emulation of Intel
NIC. We can also use a storage image file instead of a bootable ISO file.

Once we start the virtual machine, press the ESC key repeatedly to enter the
setup menu. You can add the `-boot menu=on` parameter to the QEMU command to add
more delay during booting in case it is too fast. From there, we can start the
EFI shell. Once we enter the shell, we can start BitVisor as previously
described. If tty is setup correctly, the host can also receive syslog from the
guest machine running BitVisor as well.

On recent Intel CPU, interrupts does not behave properly when kvm_intel's
`enable_apicv=Y` after launching BitVisor. It causes timer check panic or SMP
bring-up stall on Linux guest OS. It is recommended to turn apicv off at the
moment. You can check the current value of `enable_apicv` by running the
following command:

```
cat /sys/module/kvm_intel/parameters/enable_apicv
```

If it is enabled, you can turn it off temporarily by unloading and reloading the
module.

```
sudo modprobe -r kvm-intel && sudo modprobe kvm-intel enable_apicv=N
```

`enable_apicv=N` can be permanently set through modprobe configuration.

Regarding QEMU's SMP option, we still don't have proper support for AMD cpu.
While it should work on Intel cpu, the SMP bring-up is still unstable as it also
depends on the guest OS and the host scheduling. In case of Linux guest OS, SMP
bring-up is tricky because its SMP bring-up code has no delay between
INIT-deassert and SIPI. It takes some time for AP core in virtualized
environment to get the INIT signal related VMEXIT event. Since there is no
delay, there is a high chance that INIT and SIPI are pending at the same time
causing SMP bring-up to fail as SIPI was dropped by KVM in this case. The
similar problem might also occur with Windows as L2 guest under SMP environment.

Recent BitVisor has a workaround for SMP bring-up and APICV problem on QEMU +
KVM. It is possible to set `.localapic_intercept` to 1 to enable INIT-SIPI
emulation and other APIC access emulation. When the option is enabled, BitVisor
should run properly on QEMU + KVM.
