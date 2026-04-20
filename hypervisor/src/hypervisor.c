#include <stddef.h>
#include <stdbool.h>

#include "limine/requests.h"
#include "logger.h"

static void __attribute__((noreturn))
die(void)
{
    for (;;) {
        asm ("hlt");
    }
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

    /* Ensure that we get a memory map from the bootloader */
    if (memmap_request.response == NULL || memmap_request.response->entry_count < 1) {
        debug_printf("The bootloader did not provide a memory map!");
        die();
    }

    die();
}

