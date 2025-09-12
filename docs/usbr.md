# Data Interception and Redirection for USB Mass Storage Devices with BitVisor

This guide describes how to enable BitVisor to intercept data from a USB mass storage device.
It details how to redirect reading and writing operations to a remote server.

---

**Prerequisites**

- [Host controller interface (USB, Firewire) - Wikipedia](https://en.wikipedia.org/wiki/Host_controller_interface_(USB,_Firewire))
- [9P (protocol) - Wikipedia](https://en.wikipedia.org/wiki/9P_(protocol))
- [Ubuntu Manpage: mount.diod - mount diod file systems](https://manpages.ubuntu.com/manpages/trusty/man8/diodmount.8.html)
---

**Table of Contents**

- [Setup 9p on Linux Server and Enable Diod Server](#setup-9p-on-linux-server-and-enable-diod-server)
- [Fill 9p Parameters on Bitvisor](#fill-9p-parameters-on-bitvisor)
- [Start Running the Guest OS](#start-running-the-guest-os)

## Setup 9p on Linux Server and Enable Diod Server

You will need to install diod on a Linux server.
For example, on a demo environment running Ubuntu LTS, you will need to install diod as follows:

1.  To install diod from a package, do `apt-get install diod`.
    To install it from source code, find the source code on <https://github.com/chaos/diod> and clone the master branch.
    Compile it and generate the diod executable file.

2.  Next, generate an empty FAT32 image file with a specified size.
    For example, let's generate a 2-GiB FAT32 image file and format it as a FAT32.
    ```
    mkdir /home/user/fat
    cd /home/user/fat
    dd if=/dev/zero of=image_2g.bin bs=1G count=2
    sudo losetup -fP image_2g.bin
    losetup -l
    ```

    Note the loop device name (e.g., /dev/loop14).
    Use fdisk to create a partition:
    ```
    sudo fdisk /dev/loop14
    ```

    Create a New Partition:

    1. Type **n** and press **Enter**.
    2. Select partition type: **p** for primary.
    3. Partition number: Press **Enter** to accept the default.
    4. First sector: Press **Enter** to accept the default.
    5. Last sector: Press **Enter** to use the remaining space.
    6. Change Partition Type to FAT32 (LBA). Follow the steps below:

        Type **t** and press **Enter**.
        Select partition number (usually 1): Press **Enter**.\
        Enter the type code: Type **c** and press **Enter** (code 'c' corresponds to W95 FAT32 (LBA)).

        Write Changes and Exit:\
        Type **w** and press **Enter**.

        Identify the partition device (e.g., /dev/loop14p1):
        ```
        ls /dev/loop14p*
        ```

        Format the partition with the FAT32 filesystem:
        ```
        sudo mkfs.vfat -F 32 /dev/loop14p1
        ```

3.  Mount the newly created image to your file system and copy the desired files into the image.
    ```
    mkdir fat32_mount
    sudo mount /dev/loop14p1 fat32_mount
    ```

4.  Copy files such as documents and pictures into the temp\_mnt directory.
    Then unmount the image from your file system.
    And detach the loop device:
    ```
    sudo cp sample.pdf sample.mp4 sample.jpg fat32_mount
    sudo umount fat32_mount
    rmdir fat32_mount
    sudo losetup -d /dev/loop14
    ```

5.  Enable the diod server and specify the directory:
    ```
    diod -f --listen 0.0.0.0:10564 -e /home/user/fat -n -d 1 --nwthreads 36
    ```

## Fill 9p Parameters on Bitvisor

1.  About defconfig file
    1.  Fill in the values for the defconfig in the BitVisor code, using the referenced format in the template.
        In this example, we want the guest OS to connect to image\_2g.bin for intercepting the first USB drive and fat1.RO (created in a similar way) for the second USB drive on the guest OS.
        You need to fill in the image names and separate them by commas.
        The uid can be found by command 'id -u' on the server.
        The value data\_limit\_9p is the maximum transfer size for each 9P packet, excluding the 9P header.
        ```
                .usbr = {
                        .img_path = "/home/user/demo",
                        .usbr_img = "usb.img, fat1.RO,,,",
                        .server_ip = {192, 168, 2, 100},
                        .server_port = 10564,
                        .uid = "1002"
                        .uname = "user"
                        .data_limit_9p = 65536,
                },
        ```

    2.  When using more than one file image, please do not create them by directly copying from another image.
        This is because the Disk Identifier will be the same, which may cause issues when mounting them together on certain operating systems.

    3.  The maximum filename length is 31.
        If an image file’s extension ends with “.RO” (case-sensitive), the system will treat that file as strictly read-only.
        In other words, the user is unable to make any modifications or write any data to that image, preventing all changes from being saved.

    4.  Some directly installed diod services may restrict the payload size to a maximum of 65512.
        If the configured data\_limit\_9p exceeds 65512, smaller limit will be used.

    5.  Ensure that the .pci and .usb driver sections for EHCI and xHCI are enabled and usbr\_mscd is enabled.
        ```
                        .driver = {
                                .pci = "driver=ehci,and,driver=xhci,and,driver=pro1000,net=ip,tty=1,virtio=1",
                                .usb = "driver=usbr_mscd",
                        },
        ```

    6.  Check if you have assigned an ip for BitVisor.
        In order for BitVisor to successfully connect to the host through QEMU, it is recommended to refer to the following values.
        Alternatively, you can directly set .use\_dhcp to 1.
        ```
                .ip = {
                        .use_dhcp = 0,
                        .ipaddr = {10, 0, 2, 10},
                        .netmask = {255, 255, 255, 0},
                        .gateway = {10, 0, 2, 2},
                },
        ```

2.  Type `make config`, goto the item Enable USB driver.
    Mark the `USB_MSCD_REDIRECT` option with the corresponding shadow option since the USB redirection operation is based on shadow technology.
    ```
    [*] Redirect USB mass storage class devices transfers
    ...
    ```

3.  Build BitVisor with the defconfig setup.

## Start Running the Guest OS

1.  Assume you're running BitVisor on QEMU.
    We simulate two local USB drives.
    1.  Using usb-storage virtual devices:
        Here's an example command:
        ```
        /home/user/qemu-7.2.0/build/qemu-system-x86_64 -cpu host \
        -enable-kvm -bios /usr/share/ovmf/OVMF.fd \
        -drive file=fat:rw:/temp/x86_test/,format=raw \
        -drive file=/home/user/ub2_gui.qcow2,format=raw \
        -nic user,model=e1000e -M q35 -m 4096 \
        -device usb-ehci \
        -device usb-storage,drive=u1 \
        -device usb-storage,drive=u2 \
        -drive id=u1,if=none,snapshot=on,format=raw,file=fat:/temp/x86_test/ \
        -drive id=u2,if=none,snapshot=on,format=raw,file=fat:/temp/x86_test/
        ```

    2.  Using physical USB flash disks (pass through the host USB devices):
        Suppose you already have two USB flash disks, you can type 'lsusb' to get the vendor and productid for QEMU emulation.
        Here's an example command:
        ```
        sudo /home/user/qemu-7.2.0/build/qemu-system-x86_64 -cpu host \
        -enable-kvm -bios /usr/share/ovmf/OVMF.fd \
        -drive file=fat:rw:/temp/x86_test/,format=raw \
        -drive file=/home/user/ub2_gui.qcow2,format=raw \
        -nic user,model=e1000e -M q35 -m 4096 \
        -device usb-ehci \
        -device usb-host,vendorid=0x05dc,productid=0xa81d \
        -device usb-host,vendorid=0x13fe,productid=0x4100
        ```

2.  BitVisor will start connecting to the 9P server once it detects a USB drive is inserted.
    You will see some data transaction logs on the remote server where you executed the diod command:
    ```
    Tversion tag 0 msize 65560 version '9P2000.L'
    Rversion tag 0 msize 65560 version '9P2000.L'
    Tattach tag 0 fid 0 afid -1 uname 'user' aname '/home/user/fat' n_uname 1000
    Rattach tag 0 qid (00000000014881bb 0 'd')
    Tgetattr tag 0 fid 0 request_mask 0x7ff
    Rgetattr tag 0 valid 0x7ff qid (00000000014881bb 0 'd') mode 040775 uid 1000 gid 1000 nlink 5 rdev 0 size 4096 blksize 4096 blocks 8 atime Thu Apr 24 15:51:49 2025 mtime Thu Apr 24 15:52:21 2025 ctime Thu Apr 24 15:52:21 2025 btime X gen X data_version X
    Twalk tag 0 fid 0 newfid 1 nwname 1 'image_2g.bin'
    Rwalk tag 0 nwqid 1 (0000000001488ac0 0 '')
    Twalk tag 0 fid 1 newfid 2 nwname 0
    Rwalk tag 0 nwqid 0
    Tlopen tag 0 fid 1 flags 02
    Rlopen tag 0 qid (0000000001488ac0 0 '') iounit 0
    Tgetattr tag 0 fid 2 request_mask 0x7ff
    Rgetattr tag 0 valid 0x7ff qid (0000000001488ac0 0 '') mode 0100664 uid 1000 gid 1000 nlink 1 rdev 0 size 2147483648 blksize 4096 blocks 4194312 atime Thu Apr 24 15:53:17 2025 mtime Thu Apr 24 15:52:24 2025 ctime Thu Apr 24 15:52:24 2025 btime X gen X data_version X
    Tread tag 0 fid 1 offset 0 count 512
    Rread tag 0 count 512
    00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    Tread tag 0 fid 1 offset 0 count 512
    Rread tag 0 count 512
    00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000
    ...
    ```

3.  Check the USB drive status using the `lsblk` command.
    ```
    /home/user# lsblk
    NAME   MAJ:MIN RM   SIZE RO TYPE MOUNTPOINTS
    ...
    sdc      8:32   0     2G  0 disk
     |_sdc1  8:32   0     2G  0 part /media/user/11EC-AAAA
    sdd      8:48   0     4G  0 disk
     |_sdd1  8:48   0     4G  0 part /media/user/11EF-BBBB
    ```

4.  Usually, the system will automatically mount them.
    However, you can also do this manually using the following command.
    ```
    sudo mount -t vfat /dev/sdc1 mnt
    ```

5.  After the mounting process is complete, you can check the mnt directory to verify if it contains the same files as in the remote image.

6.  Since xHCI implements USB redirection through NOOP, the patch supporting QEMU NOOP has not yet been merged into the official code.
    To test xHCI on QEMU with `-device qemu-xhci` option, you must modify QEMU using the patch which is available at: <https://lore.kernel.org/qemu-devel/20250502033047.102465-7-npiggin@gmail.com/>.
    You may manually apply the patch and use the `make` command to compile QEMU.
