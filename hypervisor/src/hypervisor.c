#include <stddef.h>
#include <stdbool.h>

#include "arch_init.h"
#include "fatal.h"
#include "halloc.h"
#include "limine/requests.h"
#include "pf_alloc.h"
#include "phys_mm.h"
#include "printf.h"
#include "asm/paging.h"

#ifdef HYVEMIND_GUEST_CONFIG_FROM_BOOTLOADER

static inline void
confirm_bootloader_guest_info(void)
{
    if (module_request.response == NULL || module_request.response->module_count < 1) {
        printf("The bootloader did not provide module information!");
        die();
    }

    printf("Received guest module information from the bootloader!");
}

#else

static inline void
confirm_bootloader_guest_info(void)
{
}

#endif /* HYVEMIND_GUEST_CONFIG_FROM_BOOTLOADER */

static void
confirm_bootloader_info(void)
{
    if (memmap_request.response == NULL || memmap_request.response->entry_count < 1) {
        printf("The bootloader did not provide a memory map!");
        die();
    }

    printf("Received a memory map from the bootloader!");

    confirm_bootloader_guest_info();

    if (exec_addr_request.response == NULL) {
        printf("The bootloader did not provide information about the executables' address");
        die();
    }

    printf("Received information about the executables' address from the bootloader!");

    if (hhdm_request.response == NULL) {
        printf("The bootloader did not provide HHDM information!");
        die();
    }

    printf("Received HHDM information from the bootloader!");
}

void
hypervisor_main(void)
{
    /* Ensure the bootloader actually understands our base revision */
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        die();
    }

    if (init_printf() != 0) {
        die();
    }
    printf("Successfully initialized printf!");

    confirm_bootloader_info();

    early_direct_mapping_offset = hhdm_request.response->offset;
    printf("Set 'early_direct_mapping_offset' to value: '0x%lx'", early_direct_mapping_offset);

    init_system_memory_info(memmap_request.response);

    if (early_init_page_frame_allocator(memmap_request.response,  early_direct_mapping_offset) != 0) {
        printf("Failed to initialize the page frame allocator");
        die();
    }
    printf("(Early-)Initialized the pageframe allocator");

    arch_init(memmap_request.response, exec_addr_request.response);

    if (init_ptfl_allocator() != 0) {
        printf("Failed to initialize the hypervisor's memory allocator");
        die();
    }
    printf("Successfully inintialized the memory allocator!");

    die();
}

