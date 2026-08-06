# HyveMind

A bare-metal x86-64 hypervisor using Intel's Virtual Machine Extensions.

### Currently working on:

- Implementing support to directly boot Linux `bzImage` kernels via the 32-bit Linux
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
- Creating VMX policies that specify which features a VM wants to use
- Initializing VMCS execution control fields
- Initializing the VMCS host state area
- Initializing architectural & non-architectural VMCS guest state
- Creating an EPT mapping for a VM
