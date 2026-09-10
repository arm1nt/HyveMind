#ifndef _HYVEMIND_GUEST_CONFIG_H
#define _HYVEMIND_GUEST_CONFIG_H

#include "hyvstdlib.h"
#include "limine/limine.h"

#define T struct vm_config
#define PREFIX vm_config
#define VECTOR_DEFINITIONS
#include "lib/vector_template.h"

enum mem_granularity {
    BYTES,
    KB,
    MB,
    GB,
};

static const char *mem_granularity_strings[] = {
    [BYTES] = "bytes",
    [KB] = "kb",
    [MB] = "mb",
    [GB] = "gb",
    NULL,
};

enum guest_type {
    MIRROR_VMM,
    LINUX_DIRECT_BOOT_32BIT,
    /*GUEST_MIRROR_VMM,
    GUEST_LINUX,*/
};

static const char *vm_type_strings[] = {
    [MIRROR_VMM] = "MIRROR_VMM",
    [LINUX_DIRECT_BOOT_32BIT] = "LINUX_DIRECT_BOOT_32BIT",
    NULL,
};

struct file_info {
    uintptr_t addr;
    uint64_t size;
};

struct linux_boot_info {
    struct file_info bzImage;
    struct file_info initramfs;
    char *cmdline_str;
};

struct vm_config {
    char *name;
    enum guest_type type;

    unsigned int nr_vcpus;
    uint64_t mem_size;
    enum mem_granularity granularity;

    union {
        struct linux_boot_info linux;
    } boot_info;
};

struct vm_config_vector *get_vm_configs(const struct limine_module_response *mods);
void destroy_vm_configs(const struct vm_config_vector *configs);
uint64_t get_req_bytes(const struct vm_config *config);


//---------------------------------------------------------------

struct guest_config {
    char *name;

    enum guest_type guest_type;

    unsigned int nr_vcpus;
    uint64_t mem_size;
    enum mem_granularity mem_granularity;

    /* Names as specified in the limine config */
    char *bzImage_name;
    char *initramfs_name;

    void *bzImage_addr;
    uint64_t bzImage_size;
    void *initramfs_addr;
    uint64_t initramfs_size;

    char *command_line;
};
typedef struct guest_config guest_cfg_t;

struct guest_config_info {
    unsigned int nr_guests;
    guest_cfg_t *guest_configs;
};

struct guest_config_info get_guest_configs(const struct limine_module_response *mods);
uint64_t get_req_mem_size_bytes(const struct guest_config *config);

#endif /* _HYVEMIND_GUEST_CONFIG_H */

