# Writing a PCI driver

BitVisor, by design, does not intend to emulate hardware environments. Device
drivers in BitVisor are para-passthrough drivers. Para-passthrough drivers
mean drivers do not fully emulate the hardware. In general, while they take
control of the device, they also have to handle device access from the guest
OS. They let uninterested device access as it is (passthrough), and intercept
access otherwise.

During BitVisor initialization, it enumerates PCI bus, and initialize drivers
specified in `defconfig`'s `driver.pci` part. The following structure describes
a PCI driver. It is defined in every driver code.

```
static struct pci_driver a_driver = {
	.name		= "a_driver",
	.longname	= "A long driver name",
	.driver_options	= "option1,option2",
	.device		= "class_code=xxxxxx,id=aaaa:bbbb|cccc:dddd",
	.new		= drv_new,
	.config_read	= drv_pci_config_read,
	.config_write	= drv_pci_config_write
};
```

Each driver is expected to have at least one pci_driver object. The following
list describes each member of `pci_driver` structure.

* `name` is a driver name to be used in `defconfig`'s `driver.pci`.
* `longname` is a verbose driver name for meaningful description.
* `driver_options` contains a list of options the driver accepts. We will talk
more about options later.
* `device` tells driver enumeration code how to match the driver with devices.
Typically, you use `class_code` to match a certain device class, use `id` for
deivce's `vendor_id:device_id` to specifically match certain devices, or a
combination of both. It is possible to match multiple `vendor_id:device_id` by
concatenating them with `|`. You can specify empty match by default with `id=:`
if the driver is intended for force loading (Like the current implementation
of pci_monitor driver for example).
* `new` specifies an initialization function name. Driver enumeration code
calls the function when a device is matched based on `device` or force loading.
* `config_read` specifies a PCI configuration read handler function. The
handler function is called when the guest OS reads the device PCI configuration
space. The handler function decides what data to be returned to the guest OS.
* `config_write` specifies a PCI configuration write handler function. The
handler function is called when the guest OS writes data to the device PCI
configuration space. The handler function decides what data to be written to
the device finally.

A driver is registered with the following code example:

```
static void
a_driver_init (void)
{
	pci_register_driver (&a_driver);
}

PCI_DRIVER_INIT (a_driver_init);
```

`pci_register_driver()` registers `a_driver` object. `a_driver_init()` is called
during the PCI driver initialization phase by `PCI_DRIVER_INIT()`.

## Driver initialization

As mentioned earlier, `pci_driver.new()` is called for driver initialization if
matching conditions are met. The following steps are usually done by the
initialization function.

* Call `pci_system_disconnect()` in case the driver wants to completely control
the device. This is generally the case. During boot up, on the complex firmware
environments like UEFI, the system firmware owns the device initially. The
firmware typically gives up the device upon booting an operating system.
However, BitVisor is booted up before an operating system. So, we have to
explicitly disconnect the device we want to control from the firmware to avoid
access conflict. Depending on the system, you may need to enable bus master and
DMA memory again after disconnecting the device from the firmware.
* Set `pci_device->driver->options.use_base_address_mask_emulation = 1`. When a
guest OS enumerates PCI devices, it usually writes 0xFFFFFFFF to Base Address
Registers (BAR) to determine the size of memory map it needs to allocate. In
general, we don't want 0xFFFFFFFF value to actually be written to the PCI
configuration space as it can cause the hardware to temporarily stop working.
This can be a problem if the driver uses the device during guest OS PCI
enumeration. Enabling `use_base_address_mask_emulation` avoids the problem.
* Create a handle to access device registers. Device register addresses can be
found in BARs. The device's specification should describe BAR layout.
`pci_get_bar_info()` is used to obtain BAR information. `struct pci_bar_info`
contains register address and its memory type. A driver, in general, needs to
access them. To do that, the driver creates an abstraction to access device
registers. This can be done by calling `dres_reg_alloc()` with information from
BAR and `pci_dres_reg_translate()` as its translator function. Translator
function is needed because the address seen by devices can be different from
CPU point of view. This depends on the platform. Caller can access device
registers with `dres_reg_read*()/dres_reg_write*()`. However, if the driver
does not want the auto-mapping provided by `dres_reg_alloc()`, it can use
`dres_reg_nomap_alloc()` instead. The driver can extract base address seen by
CPU and its real memory type by using `dres_reg_nomap_cpu_addr_base()` and
`dres_reg_nomap_real_addr_type()` respectively for its customized access. Note
that the driver should not use `memcpy()` and `memset()` on MMIO registers
because the standard of these two functions does not guarantee access
alignment.
* Intercept access to device's registers. This can be done by registering
a handler functions to the created `dres_reg/dres_reg_nomap` handle
`dres_reg_register_handler()/dres_reg_nomap_register_handler()`.
* Initialize the device and driver's internal data structure.

## PCI configuration space access handling

```
static int
drv_pci_config_read (struct pci_device *dev, u8 iosize, u16 offset,
		     union mem *data)
{
	...
}

static int
drv_pci_config_write (struct pci_device *dev, u8 iosize, u16 offset,
		      union mem *data)
{
	...
}
```

Both functions return `CORE_IO_RET_DONE` if they handle the access completely.
They return `CORE_IO_RET_DEFAULT` to let the access handled by the system
default implementation.

`config_read` handler function is called when there is a read access from the
guest OS. Its implementation depends on the purpose of the driver. In case of
NIC driver, it can delegate handling to `virtio_net` subsystem so that the
guest thinks it is handling virtio network device instead of real hardware. In
case of other para-passthrough drivers, most read access can likely be
passthrough. The function can just return `CORE_IO_RET_DEFAULT` in case of
passthrough. If the function needs to manipulate some information. It can call
`pci_handle_default_config_read()` to get the actual data first. Then, it
checks for accessed location, and modify data read by
`pci_handle_default_config_read()` if needed. Finally, it returns
`CORE_IO_RET_DONE`.

`config_write` handler function is called when there is a write access from the
guest OS. While most of write can be passthrough, you likely have to keep track
of BAR address change. `pci_get_modifying_bar_info()` can be used to get the
index of BAR the guest OS is writing to. -1 means there is no BAR being
modified. The function returns the data written to the BAR through
`struct pci_bar_info` object. You need to refer to the device specification
whether a BAR is 64-bit to determine the final write event. Upon the final
write, the driver should unmap the old device register memory mapped during
device initialization, and remap with the new address.

## Device register access handling

Guest OS access to device registers are intercepted after
`dres_reg_register_handler()/dres_reg_nomap_register_handler()` is called on
`dres_reg/dres_reg_nomap` handle. In general, you need to read the device
specification, and design the handling model to suit the driver purpose.

The implementation can become tricky if both BitVisor and the guest OS
needs to submit commands through ring buffers to the device. In this case, the
driver needs to pass its ring buffers to the device, create a shadow ring
buffers for commands from the guest, and multiplex commands. The driver will
likely need to look for doorbell write event to inspect commands in the guest's
ring buffers. The driver must copy guest's original commands, and work on the
copy it needs to modify the commands. It is not a good idea to modify the guest
commands from the guest memory directly. The driver will also have to take
control of responses from the device. It needs to pass responses due to the
guest's commands back to the guest as well.

### NIC drivers and virtio_net

Each NIC specification is different. Having to handle access from the guest OS
for each NIC can be complex to implement. To simply handling access from the
guest OS, NIC drivers delegate handling to virtio_net subsystem. Drivers's
`new()` setup virtio_net with `virtio_net_init()` and
`virtio_net_set_pci_deivce()`. The driver calls
`virtio_net_handle_config_read()` and `virtio_net_handle_config_write()` for
PCI access handling. With virtio_net subsystem, the guest OS will think that it
is handling a virtio_net device instead of an actual hardware. The NIC driver
will only have to deal with controlling the hardware bases on the specification
when it utilizes virtio_net subsystem.

As mentioned earlier, a driver initially creates a `dres_reg` handle during its
initialization for register access. The driver transfers the `dres_reg` handle
ownership to virtio_net after `virtio_net_set_pci_device()`. The driver will
only hold a reference to the handle for accessing actual device registers after
the function call. Upon MMIO base address change event, virito_net notifies the
driver with `mmio_change()` callback to let the driver update its handle
reference. The driver calls `virtio_net_suspend()` to tell virito_net to stop
handling MMIO access, takes back the `dres_reg` handle ownership, and frees it
before suspending. The driver creates a new `dres_reg` handle, and transfers
the ownership to virtio_net by calling `virtio_net_resume()` on resuming.
