# AArch64 implementation

This document provides an overview of AArch64 implementation.

## Current requirements

* Armv8.1-A: The implementation requires Virtualization Host Extension (VHE).

VHE is required to simplify the MMU implementation. Armv8-A originally have
only `TTBR0_EL2` for EL2. This means it will require a tricky implementation to
make BitVisor run its processes like on x86 implementation. BitVisor processes
are intended to run on the least privileged level (In case of AArch64, it is
EL0). I means that we need to find a way to enter EL1 first, and start a
process from EL1. When we take control of EL1, it also means that we need to
set up our EL1 system registers. We also have to deal with EL1 context save and
restore as it is also used by the guest OS.

With VHE, it greatly simplifies the implementation a lot. It is basically turns
EL2 system registers to behave like EL1. We have access to `TTBR0_EL2` and
`TTBR1_EL2` allowing us to have a clean separation between hypervisor memory
address space and process memory address space. It allows us to launch a
process on EL0 from EL2 directly.

* 64-bit mode only

We currently conceal the 32-bit feature. This is to simplify the load-store
emulation for initial implementation.

* Power State Coordination Interface (PSCI)

The current implementation assumes the running platform supports PSCI.

* UEFI environment

Original x86 BitVisor either starts from the BIOS or UEFI environment. For
AArch64, we currently require a UEFI environment.

* ACPI

As there is no devicetree on x86 environment, current AArch64 implementation
inherits the trait. We should support this as it is common on AArch64
environment once we have a stable AArch64 implementation.

* PCI initialization by firmware

We currently expect PCI to be initialized by the firmware. PCI initialization
is platform-specific. We generally have no access to documents related to the
process.

## Relocation

AArch64 bitvisor.elf is relocatable. It means that it can run on any address
space. We currently handle only `RELATIVE (1027)` type. The relocation table is
located in the `.rela.dyn` section of the elf file. Each entry in the table
contains the following structure:

```
struct rela_entry {
    u64 r_offset; /* Location to apply relocation */
    u64 r_info; /* Relocation type */
    u64 r_addend;
};
```

We get the location to apply relocation by:


```
target_loc = runtime_base_address + r_offset;
```

`Delta(S) + Addend` is the formula to calculate the value to write for
`RELATIVE` type. `Delta(S)` is the difference between static link address and
the runtime address. In our case, our static head address (the very first byte
of BitVisor binary) is 0. This turns `Delta(S)` into simply the runtime
address:

```
*target_loc = (runtime_base_address - 0) + r_addend;
```

For more information, see ELF AArch64 specification.

## From loader to BitVisor

Loader is responsible loading a portion of bitvisor.elf containing entire
`.entry` section. The initial loading size is currently hard-coded to be within
a certain size depending on the running architecture. Current AArch64 uses
512KB for entry size as the `.rela.dyn` section can be very large.

Once the loader jumps to the BitVisor's `entry` code. The first thing the code
does is saving current context registers. This includes callee-save registers,
and other system registers. They are necessary for returning back to the loader
on EL1.

As the code is relocatable, the next step is to apply relocation in order to
access global variables. At this stage, `apply_reloc_on_entry()` is called. It
applies relocation only within the `.entry` section. The code is currently
running on the UEFI environment. The memory address space is 1-to-1 mapping.
Applying relocation with the physical base address (`head`) is valid at this
point.

After the relocation, `uefi_entry_load()` is called. It returns the newly
loaded virtual memory base address of the entire BitVisor . The function is
responsible for loading the entire BitVisor through the platform independent
`uefi_load_bitvisor()` function, and preparing BitVisor virtual memory address
space with `mmu_setup_for_loaded_bitvisor()`. The MMU function constructs
BitVisor page tables for `TTBR1_EL2`, enables `HCR_EL2`, configures other MMU
related system registers, and returns virtual memory address base to the
caller.

After loading the entire BitVisor, we jump to the `_vmm_entry()` function with
our new virtual address. It does relocation, sets up CPU0’s stack, and calls
`vmm_entry()` for the rest of the initialization.

## AArch64 specific Initialization overview

This section provides an overview of major initialization needed for running on
AArch64.

The first thing BitVisor needs to do is to set up its own exception table, and
pcpu (physical CPU) related data structures. This allows other code to access
per-CPU data.

Next major set-up is heap allocation. Once it is up and running, we can
finalize other MMU related code including memory mapping, stage-2 memory
translation, and MMIO access interception. Memory mapping is for mapping
physical memory to virtual memory to access. Its initialization involves
preparing virtual memory space. We need this to access hardware registers and
guest memory. Stage-2 memory translation is for allowing the guest OS to run.
It involves translating intermediate physical address (IPA) to real physical
address (PA). The current implementation is identity-mapping. MMIO access
interception initialization involves preparing data structures for keeping
track of memory areas the hypervisor is intercepting. MMIO access interception
involves Stage-2 translation. The general idea is BitVisor unmaps the memory
area it wants to intercept from Stage-2 translation. This causes exceptions to
the hypervisor to handle the access.

BitVisor then looks for the GIC controller from ACPI. BitVisor initializes its
GIC para-passthrough driver for forwarding and injecting interrupts to the
guest. It also needs to be able to translate IO port memory space to MMIO to
handle guest PCI device access properly. This can be done by looking up from
ACPI DSDT, and translating accordingly.

The rest of the initialization is similar to the x86 implementation. The final
step is to jump back to the loader to return to the EFI environment in EL1.
This involves restoring registers we saved on entering BitVisor, and
initializing necessary EL1 registers.

## How the hypervisor works overview

How the hypervisor works heavily relies on exception mechanisms. IRQ/FIQ
exceptions regularly cause context-switching to the hypervisor. Synchronous
exceptions cause context-switching when the guest OS tries to perform actions
monitored by the hypervisor.

IRQ/FIQ exceptions are from interrupts. The current implementation requires
redirecting all interrupts to EL2 due to GICv3 design. This gives BitVisor
opportunities to run background tasks, like handling network connections. For
each interrupt, in general, BitVisor needs to inject its corresponding virtual
interrupt to the guest.

Synchronous exceptions happen either when the guest tries to execute monitored
instructions, or when it tries to access monitored system registers or memory
areas. BitVisor intercepts executing of certain instructions and accessing
certain system registers by configuring the `HCR_EL2` register. For example, it
traps `SMC` execution, and hijacks the execution if it is a PSCI call for
secondary core start-up. BitVisor unmaps Stage-2 translation of memory areas
through `mmio_register()` to monitor memory access from the guest. For example,
BitVisor handles access to the NIC register memory to make the guest think
that it is dealing with the virtio_net device instead of real hardware if the
driver is enabled.

## Secondary core start-up

BitVisor does not start secondary cores during its initialization. It is the
guest OS who starts them. The guest issues a PSCI compliant SMC call with
`CPU_ON` function. BitVisor intercepts the call, records the original entry
points, and issues its own `CPU_ON` function with its own entry point. Once a
secondary core starts, it enters the entry point specified by BitVisor.
BitVisor then initializes core specific system registers and data. Finally, it
enters EL1 with the saved entry point from the guest.
