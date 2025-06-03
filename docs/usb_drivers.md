# USB Drivers

USB consists of USB host controllers and USB devices.

## USB Host Controllers

BitVisor has para pass-through drivers for the following USB host controllers:

- Universal Host Controller Interface (UHCI)
- Enhanced Host Controller Interface (EHCI)
- Extensible Host Controller Interface (xHCI)

Each driver can be enabled via the `vmm.driver.pci` configuration parameter (set in `defconfig` or `bitvisor.conf`):

- UHCI: `driver=uhci`
- EHCI: `driver=ehci`
- xHCI: `driver=xhci`

BitVisor does not support Open Host Controller Interface (OHCI).

EHCI conceal driver, `driver=ehci_conceal`, conceals EHCI controller and sets the port route to a companion host controller.
This was previously used when a computer had UHCI as a companion controller to EHCI, allowing devices to access through the UHCI controller to circumvent EHCI driver limitations.

### Limitations

The UHCI driver slows down the system.
Errors sometimes occurred due to timing issues caused by the driver polling data structures created by a guest driver.
The UHCI driver supports submitting control, bulk, and interrupt transfer commands, so it can be used by the ID manager implementation in the `idman/` directory.

The EHCI driver is similar to UHCI, but interrupt transfers (like mouse or keyboard) are never intercepted, which eases the timing issue.
However, the EHCI driver does not support submitting interrupt transfer commands via the `submit_interrupt` function.

The xHCI driver currently does not support submitting bulk or interrupt transfer commands via the `submit_bulk` or `submit_interrupt` function.

## USB Devices

BitVisor has para pass-through drivers for the following USB devices:

- Chip card interface device (CCID)
- Mass Storage Class Device (MSCD)

Each driver can be enabled via the `vmm.driver.usb` configuration:

- CCID Class Conceal Driver (for UHCI host controller only): `driver=ccid`
- MSCD Driver (for storage encryption): `driver=mscd`

In addition, the following USB devices are treated specially:

- Hub
- Human Interface Device (HID)

Since a hub has multiple USB ports, BitVisor monitors device connection/disconnection events for these ports.
HID needs to be treated specially for the UHCI host controller because of the driver implementation.

## USB Driver Implementation

A USB driver registers itself as follows:

```
#include "usb.h"
#include "usb_device.h"
#include "usb_driver.h"

static void
usb_foo_new (struct usb_host *host, struct usb_device *dev)
{
        dev->ctrl_by_host = 1;
        (snip)
}

static struct usb_driver usb_foo_driver = {
        .name = "foo",
        .longname = "Foo Driver",
        .device = "class=ff,id=1234:5678",
        .new = usb_foo_new,
};

static void
usb_foo_init (void)
{
        usb_register_driver (&usb_foo_driver);
}

USB_DRIVER_INIT (usb_foo_init);
```

If the driver is enabled via the `vmm.driver.usb=driver=foo` configuration, it is equivalent to `vmm.driver.usb=class=ff,id=1234:5678,driver=foo`, based on the `device` field in the `struct usb_driver`.
The `new` callback is then invoked when a USB device of class 0xff, vendor ID 0x1234, and device ID 0x5678 is connected.
This callback can register hooks using `usb_hook_register()` to intercept its communications.

`ctrl_by_host` is referenced by the xHCI driver.
If its value is zero, device communication will be completely passed through.

`struct usb_driver` has a `compat` boolean member.
Older versions of BitVisor did not have the `vmm.driver.usb` configuration; at that time, all compiled drivers were enabled.
Only the CCID and MSCD drivers use the `compat` member to enable them when the `vmm.driver.usb` configuration is empty, for compatibility reasons.
New drivers should omit `compat` member initialization.

## Configuration Examples

To enable EHCI, xHCI, and MSCD drivers:

```
vmm.driver.pci=driver=ehci,and,driver=xhci
vmm.driver.usb=driver=mscd
```

To enable UHCI, MSCD, and CCID drivers:

```
vmm.driver.pci=driver=uhci,and,driver=ehci_conceal
vmm.driver.usb=driver=mscd,and,driver=ccid
```
