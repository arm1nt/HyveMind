# HyveMind

A bare-metal x86-64 hypervisor using Intel's Virtual Machine Extensions.

To run with information being printed:

```
make qemu_run_hdd_uefi QUIET=1 DEBUG_BUILD=1
```

### Currently working on:

- Per-vCPU APIC virtualization and vCPU scheduling

### Completed features:

- Checking CPU capabilities (VMX support, etc.) for the BSP and APs
- Bitmap page-frame allocator
- Power-of-two freelist heap allocator
- 4-level paging
    - Replacing bootloader-provided page tables with custom tables
- GDT/IDT/TSS setup
- UART driver for serial output
    - Including coloring via ANSI escape sequences
- Basic spinlock implementation
- Basic per-cpu variables support
- xAPIC/x2APIC support
    - Including support for computing the APIC timer frequency
- Basic HPET driver to support HPET-based busy sleeping
- Timer infrastructure
    - Multiple approaches for determining the TSC frequency
    - Support for one-shot and periodic timers
    - Min-heap implementation to manage software timers

*Virtualization specific:*

- Entering & leaving VMX operation on each logical processor
- Verifying VMX capabilities (e.g. EPT support)
- Creating and validating VMX policies that specify the features a VM requires
- Initializing VMCS execution control fields and the VMCS host state area
- Initializing register & non-register VMCS guest state according to the type of guest
- VM-entry and VM-exit handlers
- Creation of EPT mappings to virtualize guest physical memory
    - Including support to copy data into a VM's address space
- Parsing and validating `bzImage` kernel images
- Support for directly booting and running `bzImage` kernels via the 32-bit Linux x86 boot protocol
