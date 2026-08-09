#include "fatal.h"
#include "types.h"
#include "guest_config.h"
#include "printf.h"
#include "string.h"

#define MEM_GRANULARITY_KB_SHIFT 10
#define MEM_GRANULARITY_MB_SHIFT 20
#define MEM_GRANULARITY_GB_SHIFT 30

/* Hardcode for now, but change later */
#define GUEST_CFG_NUMBER_OF_GUESTS 1

static struct guest_config guest_configs[GUEST_CFG_NUMBER_OF_GUESTS] = {
    {
        .name = "vm1",
        .nr_vcpus = 1,
        .mem_size = 1,
        .mem_granularity = GB,
        .guest_type = MIRROR_VMM,
        .bzImage_name = "vm1-bzImage",
        .bzImage_addr = NULL,
        .initramfs_name = "vm1-initramfs",
        .initramfs_addr = NULL,
    },
};

struct guest_config_info
get_guest_configs(const struct limine_module_response *mods)
{
    struct guest_config_info info;

    info.nr_guests = GUEST_CFG_NUMBER_OF_GUESTS;
    info.guest_configs = guest_configs;

    /* todo: !!!do properly, this is just for testing!!! */

    for (unsigned int i = 0; i < info.nr_guests; i++) {
        guest_cfg_t *guest = &guest_configs[i];

        if (guest->guest_type != LINUX_DIRECT_BOOT_32BIT) {
            continue;
        } else if (!mods) {
            die_reason("linux guest but no kernel image provided");
        }

        for (uint64_t j = 0; j < mods->module_count; j++) {
            struct limine_file *mod = mods->modules[j];

            if (strcmp(guest->bzImage_name, mod->string) == 0) {
                guest->bzImage_addr = mod->address;
                guest->bzImage_size = mod->size;
                continue;
            } else if (strcmp(guest->initramfs_name, mod->string) == 0) {
                guest->initramfs_addr = mod->address;
                guest->initramfs_size = mod->size;
                continue;
            }
        }

        if (!guest->bzImage_addr) {
            die_reason("No kernel image found");
        }
    }

    return info;
}

uint64_t
get_req_mem_size_bytes(const struct guest_config *config)
{
    switch (config->mem_granularity) {
        case BYTES:
            return config->mem_size;
        case KB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_KB_SHIFT);
        case MB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_MB_SHIFT);
        case GB:
            return U64_LSHIFT(config->mem_size, MEM_GRANULARITY_GB_SHIFT);
        default:
            pr_error("VM configuration specifies unsupported memory granularity");
            return 0;
    }
}

