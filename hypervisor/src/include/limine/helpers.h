#ifndef _HYVEMIND_LIMINE_HELPERS_H
#define _HYVEMIND_LIMINE_HELPERS_H

#include "limine/limine.h"

static char* limine_mem_type_to_string[9] = {
    "USABLE                     ",
    "RESERVED                   ",
    "ACPI_RECLAIMABLE           ",
    "ACPI_NON_VOLATILE_STORAGE  ",
    "BAD_MEMORY                 ",
    "BOOTLOADER_RECLAIMABLE     ",
    "EXECUTABLE_AND_MODULES     ",
    "FRAMEBUFFER                ",
    "RESERVED_MAPPED            "
};

#endif /* _HYVEMIND_LIMINE_HELPERS_H */

