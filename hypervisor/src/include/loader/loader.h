#ifndef _HYVEMIND_LOADER_LOADER_H
#define _HYVEMIND_LOADER_LOADER_H

#include "vm.h"

struct linux_load_info {
    gpaddr pm_kernel;
    uint64_t pm_kernel_size;
    gpaddr pm_kernel_entry_point;
    gpaddr gdt_page;
    gpaddr zero_page;
    gpaddr command_line_string;
};

int load_linux_32bit_direct_boot_for_vm(
        struct vm *vm,
        const struct guest_config *config,
        struct linux_load_info *load_info
);

#endif /* _HYVEMIND_LOADER_LOADER_H */

