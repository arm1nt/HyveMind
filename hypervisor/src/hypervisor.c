#include <stddef.h>
#include <stdbool.h>

#include "fatal.h"
#include "halloc.h"
#include "pf_alloc.h"
#include "phys_mm.h"
#include "printf.h"
#include "vm.h"
#include "asm/setup.h"
#include "asm/paging.h"
#include "limine/requests.h"

static void
confirm_bootloader_info(void)
{
    if (memmap_request.response == NULL || memmap_request.response->entry_count < 1) {
        die_reason("Bootloader did not provide a memory map");
    }

    if (exec_addr_request.response == NULL) {
        die_reason("Bootloader did not provide information about the executables' address");
    }

    if (hhdm_request.response == NULL) {
        die_reason("The bootloader did not provide HHDM information!");
    }

    if (rsdp_request.response == NULL || rsdp_request.response->address == NULL) {
        die_reason("The bootloader did not provide the address of the RSDP table");
    }

    if (mp_request.response == NULL || mp_request.response->cpu_count < 1) {
        die_reason("Bootloader failed to bring up APs");
    }
}

void __no_return
hypervisor_main(void)
{
    /* Ensure the bootloader actually understands our base revision */
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        die();
    }

    if (init_printf() != 0) {
        die();
    }
    pr_debug("Successfully initialized printf!");

    confirm_bootloader_info();
    pr_debug("Received all information requested from the bootloader");

    early_direct_mapping_offset = hhdm_request.response->offset;

    const boot_mem_info_t boot_mem_info = get_boot_mem_info(
            memmap_request.response,
            exec_addr_request.response
    );
    init_system_memory_info(&boot_mem_info);
    pr_debug("Organized information about early boot memory layout");

    const uint64_t curr_dm_offset = hhdm_request.response->offset;
    if (early_init_page_frame_allocator(memmap_request.response, curr_dm_offset) != 0) {
        die_reason("Failed to initialize the page frame allocator");
    }
    pr_debug("(Early-) initialized the page frame allocator");

    virt_addr_t top_of_stack = arch_replace_stack(DEFAULT_HYV_THREAD_STACK_SIZE);
    if (!top_of_stack) {
        die_reason("Failed to switch BSP stack");
    }
    pr_debug("Switched BSP stack to %lx", top_of_stack);

    arch_setup_bsp();
    pr_debug("Done with arch setup of the bootstrap processor");

    if (init_halloc() != 0) {
        die_reason("Failed to initialize the hypervisor's internal heap allocator");
    }
    pr_debug("Successfully inintialized the memory allocator!");

    arch_bringup_aps_limine(mp_request.response);
    pr_debug("Initialized APs");

    /* wait_until_aps_online(); */

    const struct guest_config_info guest_info = get_guest_configs(module_request.response);

    for (unsigned int i = 0; i < guest_info.nr_guests; i++) {
        struct vm *vm = create_vm(&guest_info.guest_configs[i]);
        pr_info("Created VM: %s", vm->name);
        destroy_vm(vm);
    }

    /* todo: submit created vcpus to scheduler */

    die_reason("reached end of main");
}

