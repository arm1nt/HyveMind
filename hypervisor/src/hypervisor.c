#include <stddef.h>
#include <stdbool.h>

#include "limine/requests.h"
#include "printf.h"

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

    die();
}

