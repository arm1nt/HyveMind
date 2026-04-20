#include <stddef.h>
#include <stdbool.h>

#include "logger.h"
#include "limine/requests.h"

static void __attribute__((noreturn))
die(void)
{
    for (;;) {
        asm ("hlt");
    }
}

#ifdef HYVEMIND_GUEST_CONFIG_FROM_BOOTLOADER

static inline void
confirm_bootloader_guest_info(void)
{
    if (module_request.response == NULL || module_request.response->module_count < 1) {
        debug_printf("The bootloader did not provide module information!");
        die();
    }

    debug_printf("Received guest module information from the bootloader!");
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
        debug_printf("The bootloader did not provide a memory map!");
        die();
    }

    debug_printf("Received a memory map from the bootloader!");

    confirm_bootloader_guest_info();

    if (exec_addr_request.response == NULL) {
        debug_printf("The bootloader did not provide information about the executables' address");
        die();
    }

    debug_printf("Received information about the executables' address from the bootloader!");

    if (hhdm_request.response == NULL) {
        debug_printf("The bootloader did not provide HHDM information!");
        die();
    }

    debug_printf("Received HHDM information from the bootloader!");
}

void
hypervisor_main(void)
{
    /* Ensure the bootloader actually understands our base revision */
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        die();
    }

    if (init_debug_logging()) {
        die();
    }

    debug_printf("Debug logging successfully initialized!");

    confirm_bootloader_info();

    die();
}

