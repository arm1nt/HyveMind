#include "fatal.h"
#include "halloc.h"
#include "pf_alloc.h"
#include "phys_mm.h"
#include "printf.h"
#include "sched.h"
#include "timer.h"
#include "vm.h"
#include "asm/setup.h"
#include "asm/paging.h"
#include "asm/vmm.h"
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

    pr_debug("Received all information requested from the bootloader");
}

void __no_return
hypervisor_main(void)
{
    /* Ensure the bootloader actually understands our base revision */
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false)
        die();

    if (init_printf() != 0) {
        die();
    }
    pr_debug("Successfully initialized printf!");

    confirm_bootloader_info();

    early_direct_mapping_offset = hhdm_request.response->offset;
    const boot_mem_info_t boot_mem_info = get_boot_mem_info(
            memmap_request.response,
            exec_addr_request.response
    );
    init_system_memory_info(&boot_mem_info);

    map_cpu_topology(mp_request.response);
    init_task_queues();

    const uint64_t curr_dm_offset = hhdm_request.response->offset;
    if (early_init_page_frame_allocator(memmap_request.response, curr_dm_offset) != 0) {
        die_reason("Failed to initialize the page frame allocator");
    }
    pr_debug("(Early-) initialized the page frame allocator");

    arch_setup_bsp();
    pr_debug("Done with arch setup of the bootstrap processor");

    if (init_halloc() != 0) {
        die_reason("Failed to initialize the hypervisor's heap allocator");
    }
    pr_debug("Successfully initialized the heap allocator!");

    if (init_timer_framework() != TIMER_SUCCESS) {
        die_reason("Failed to initialize timer framework");
    }

    arch_bringup_aps_limine(mp_request.response);

    const struct vm_config_vector *configs = get_vm_configs(module_request.response);
    if (!configs) {
        die_reason("Failed to retrieve VM configs from the hyvemind config file");
    }

    const int nr_configs = size_vm_config_vector(configs);
    pr_info("Done parsing hyvemind config. Found %lu entries", nr_configs);

    struct vm_config config;
    for (int i = 0; i < nr_configs; i++) {
        at_vm_config_vector(configs, &config, i);

        struct vm *vm = create_vm(&config);
        list_add_tail(&global_vm_list, &vm->vm_list);
    }
    destroy_vm_configs(configs);

    wait_until_aps_online();

    struct list_head *entry;
    list_for_each(entry, &global_vm_list) {
        struct vm *vm = list_entry(entry, struct vm, vm_list);
        sched_submit_vm(vm);
    }

    enter_schedule_loop();

    die_reason("unreachable");
}

