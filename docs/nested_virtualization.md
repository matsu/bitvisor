# Nested Virtualization

This document describes `vmm.unsafe_nested_virtualization` parameter in `defconfig`/`bitvisor.conf`.

Nested virtualization means running a hypervisor on a virtual machine.
If nested virtualization is enabled, virtualization extension (VT-x/SVM) is available on the virtual machine.
Hypervisor running on a physical machine is called L1 hypervisor.
Hypervisor running on a virtual machine is called L2 hypervisor.
A processor switches to an L1 hypervisor when some instructions related to virtualization are executed on a virtual machine.
The L1 hypervisor needs to emulate such instructions.
Switching to a virtual machine on an L2 hypervisor is done by emulation of the L1 hypervisor i.e. L1 hypervisor really switches to the virtual machine while emulating VMENTRY and switches back to the L2 hypervisor while emulating VMEXIT.

- Hypervisors work like following normally:\
hypervisor -> (VMENTRY) -> guest -> (VMEXIT) -> hypervisor
- Hypervisors work like following during nested virtualization:\
L2 hypervisor = L1 guest -> (VMEXIT) -> L1 hypervisor -> (VMENTRY) -> L2 guest -> (VMEXIT) -> L1 hypervisor -> (VMENTRY) -> L2 hypervisor = L1 guest

In general, data structure created by an L2 hypervisor is not used directly by an L1 hypervisor.
The reason is that the data structure contains addresses of a virtual machine on the L1 hypervisor.
Such addresses need to be replaced with physical addresses of the underlying physical machine.
Especially, page tables for nested paging (EPT/RVI) have a lot of entries containing addresses to be converted.

## Parameter

BitVisor supports nested virtualization with some restrictions.
In this section, an L1 hypervisor is BitVisor.
`vmm.unsafe_nested_virtualization` parameter takes an integer value as follows:

- 0: Off
- 1: Unsafe mode
- 2: Partially safe mode

When `vmm.unsafe_nested_virtualization` parameter is set to 0, nested virtualization is disabled.
Virtualization extension is concealed.

When `vmm.unsafe_nested_virtualization` parameter is set to 1, nested virtualization is enabled in unsafe mode.
The data structure created by an L2 hypervisor is used directly with a little modification, without address conversion.
Using it directly is possible since BitVisor is a para pass-through hypervisor.
It is called **unsafe** because of no address conversion.
Unsafe consideration is described later.

When `vmm.unsafe_nested_virtualization` parameter is set to 2, nested virtualization is enabled in partially safe mode.
Processor needs to support EPT/RVI in this mode.
In this mode, page tables created by an L2 hypervisor for nested paging are not directly used.
If the L2 hypervisor does not enable nested paging, the L1 hypervisor forcefully enables it with page tables for a virtual machine on the L1 hypervisor.
If the L2 hypervisor enables nested paging, the L1 hypervisor creates "shadow" page tables containing validated entries only.
This mode is slower than unsafe mode, but can prevent some unexpected memory accesses.
Currently only EPT is supported for shadowing.
RVI for the L2 hypervisor is simply concealed in this mode.

## Consideration about Unsafe Cases

If an L2 hypervisor does full emulation without pass-through devices, it works on BitVisor nested virtualization with unsafe mode.
There are several cases considered as unsafe:

- Accessing BitVisor memory
- Bypassing para pass-through drivers
  - Memory space
  - I/O space
- Race condition on virtual machine data structures

### Accessing BitVisor Memory

BitVisor memory is allocated from random access memory and is reserved in memory map.
A guest operating system on BitVisor cannot access the BitVisor memory directly.
However, a guest operating system on an L2 hypervisor can access the BitVisor memory directly in unsafe mode.
This is considered as unsafe, because for example, accessing the memory can retrieve sensitive data or bypass BitVisor features such as encryption, VPN, etc.

In general, a hypervisor does not use reserved memory at all.
However, a malicious hypervisor or pass-through hypervisor can use reserved memory as memory of a virtual machine to access BitVisor memory.
Details are described later.

Partially safe mode prevents the access.

### Bypassing Para Pass-through Drivers

BitVisor para pass-through drivers handle device accesses of a virtual machine.
There are two types of address space for a device.
One is memory space (MMIO).
Another is I/O space (I/O port).

If an L2 hypervisor supports pass-through devices, such address spaces may be directly accessible from a guest operating system on the L2 hypervisor.
This is considered as unsafe, because for example, bypassing para pass-through drivers can access storage without encryption or transmit data without VPN.

Partially safe mode prevents memory space accesses which should be intercepted by para pass-through drivers, but does not prevent I/O space accesses.

### Race Condition on Virtual Machine Data Structures

Virtual machine data structures such as VMCS (VT-x) or VMCB (SVM) for a virtual machine on an L2 hypervisor exist on the memory accessible from the L2 hypervisor.
They are accessible even while BitVisor is emulating instructions related to virtualization.
Therefore, the L2 hypervisor can modify the structures from other processors.
On VT-x, accessing the structure needs to use `VMREAD`/`VMWRITE` instruction on the specific processor.
However, on SVM, accessing the structure just before executing `VMRUN` instruction in BitVisor probably can avoid protection of partially safe mode.
As a result, this is race condition.

## Partially Safe Mode Behavior

As described above, partially safe mode prevents some unexpected memory accesses from software on a virtual machine on an L2 hypervisor.
The result depends on the access type.

When the software reads from BitVisor memory, it always reads zero bytes.
This is same as when the L1 guest does.

When the software writes to BitVisor memory, BitVisor panics.
This is same as when the L1 guest does.

When the software accesses memory space which should be intercepted, BitVisor panics.
This is different from when the L1 guest does, because instruction emulation is not yet implemented for such nested virtualization case.
It will need additional address translation (L2 guest virtual address -> L2 guest physical address -> L1 guest physical address) to support the emulation.

## Unsafe Example

### BitVisor on BitVisor

Running another BitVisor on BitVisor is a simple way to bypass everything.
In case of unsafe mode, running BitVisor with no drivers enabled as an L2 hypervisor on the underlying BitVisor allows accessing underlying BitVisor memory easily from the guest operating system.
In addition, modifying the L2 BitVisor to avoid intercepting PCI configuration space bypasses all para pass-through drivers in the L1 BitVisor.

Partially safe mode prevents such memory accesses.
However, accessing I/O ports is not intercepted.
In addition, accessing PCI Express configuration space via MCFG probably causes panic, because L1 BitVisor intercepts PCI configuration space.

### Microsoft Hyper-V or Xen on BitVisor

Hyper-V or Xen acts like a pass-through hypervisor for a host operating system, which manages most physical devices, on a special domain (called as dom0 in Xen).
The design is different from x86 Linux KVM or VirtualBox, which changes a host operating system to a big hypervisor including all user processes.
Therefore, Hyper-V or Xen on BitVisor is not expected to work.

Partially safe mode probably does not work, unless the special domain does not access memory space or I/O space intercepted by BitVisor at all.
For example, if the domain accesses PCI Express configuration space via MCFG, BitVisor panics, because L1 BitVisor intercepts PCI configuration space.

### Linux KVM on BitVisor

Accessing underlying BitVisor memory from Linux is possible in case of unsafe mode as follows:

1. Create a virtual machine using KVM.
2. Use `mmap` to map the reserved memory containing BitVisor in `/dev/mem`.
3. Pass the mapped memory to `KVM_SET_USER_MEMORY_REGION ioctl` to map it somewhere on the virtual machine.
4. Start the virtual machine.
5. Access the memory from a program running on the virtual machine.

Users which have permission for `/dev/mem` and `/dev/kvm` can do this.
Probably bypassing para pass-through drivers is also possible with a similar way.

Partially safe mode prevents such accesses.
