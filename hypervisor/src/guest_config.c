#include "types.h"
#include "guest_config.h"
#include "printf.h"

#define MEM_GRANULARITY_KB_SHIFT 10
#define MEM_GRANULARITY_MB_SHIFT 20
#define MEM_GRANULARITY_GB_SHIFT 30

/* Hardcode for now, but change later */
#define GUEST_CFG_NUMBER_OF_GUESTS 1

static struct guest_config guest_configs[GUEST_CFG_NUMBER_OF_GUESTS] = {
    {
        .name = "vm1",
        .nr_vcpus = 1,
        .mem_size = 1024,
        .mem_granularity = GB,
        .bzImageName = "vm1-bzImage",
        .initramfsName = "vm1-initramfs",
    },
};

struct guest_config_info
get_guest_configs(void)
{
    struct guest_config_info info;
    info.nr_guests = GUEST_CFG_NUMBER_OF_GUESTS;
    info.guest_configs = guest_configs;
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

