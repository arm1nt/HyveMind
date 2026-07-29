#include "guest_config.h"

/* Hardcode for now, but change later */
#define GUEST_CFG_NUMBER_OF_GUESTS 1

static struct guest_config guest_configs[GUEST_CFG_NUMBER_OF_GUESTS] = {
    {
        .name = "vm1",
        .nr_vcpus = 1,
        .mem_size = 300,
        .mem_granularity = MB,
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

