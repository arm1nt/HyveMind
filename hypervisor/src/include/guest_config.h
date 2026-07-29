#ifndef _HYVEMIND_GUEST_CONFIG_H
#define _HYVEMIND_GUEST_CONFIG_H

enum mem_granularity {
    BYTES,
    KB,
    MB,
    GB,
};

struct guest_config {
    char *name;

    unsigned int nr_vcpus;
    unsigned int mem_size;
    enum mem_granularity mem_granularity;

    /* Names as specified in the limine config */
    char *bzImageName;
    char *initramfsName;
};
typedef struct guest_config guest_cfg_t;

struct guest_config_info {
    unsigned int nr_guests;
    guest_cfg_t *guest_configs;
};

struct guest_config_info get_guest_configs(void);

#endif /* _HYVEMIND_GUEST_CONFIG_H */

