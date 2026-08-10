# HyveMind

A bare-metal x86-64 hypervisor using Intel's Virtual Machine Extensions.

To run with information being printed:

```
make qemu_run_hdd_uefi QUIET=1 DEBUG_BUILD=1
```

### Currently working on:

- Implementing support to directly boot and run Linux `bzImage` kernels via the 32-bit Linux
x86 boot protocol.

### Completed features:

- Checking CPU capabilities (VMX support, etc.) for the BSP and APs
- UART driver for serial output
    - including coloring via ANSI escape sequences
- Bitmap page-frame allocator
- Power-of-two freelist heap allocator
- 4-level paging
    - replacing boot loader provided page tables with custom tables
- x2apic initialization
- GDT/IDT/TSS setup
- Basic spinlock implementation
- Basic per-cpu variables support

*Virtualization specific:*
- Entering & leaving VMX operation
- Verifying VMX capabilities (e.g. EPT support)
- Creating and validating VMX policies that specify which features a VM wants to use
- Initializing VMCS execution control fields
- Initializing the VMCS host state area
- Initializing register & non-register VMCS guest state according to the type of guest
- Basic VM-entry and VM-exit handlers
- Creating an EPT mapping for a VM
    - Copying data into the guest addr space
- Parsing and validating a `bzImage` kernel image
- Loading the kernel image into a VM's memory
