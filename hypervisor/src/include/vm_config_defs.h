#ifndef _HYVEMIND_VM_CONFIG_DEFS_H
#define _HYVEMIND_VM_CONFIG_DEFS_H

#include "hyvstdlib.h"

#define HYVEMIND_CONFIG_FILE "hyvemind_config"

#define CONFIG_SECTION_START    "[start]"
#define CONFIG_SECTION_END      "[end]"
#define CONFIG_SECTION_DELIM    "="

const char *reserved_tokens[] = {
    CONFIG_SECTION_START,
    CONFIG_SECTION_END,
    CONFIG_SECTION_DELIM,
    NULL
};

#define ADD_NODE(parent, child) parent "." TO_STR(child)

#define VM_NODE             TO_STR(vm)
#define VM_MEM_NODE         ADD_NODE(VM_NODE, mem)
#define VM_BOOT_NODE        ADD_NODE(VM_NODE, boot)
#define VM_LINUX_BOOT_NODE  ADD_NODE(VM_BOOT_NODE, linux)

enum config_key {
    CONFIG_VM_NAME_KEY,
    CONFIG_VM_TYPE_KEY,
    CONFIG_VM_VCPUS_KEY,
    CONFIG_VM_MEM_SIZE_KEY,
    CONFIG_VM_MEM_GRANULARITY_KEY,

    CONFIG_VM_BOOT_LINUX_BZIMAGE_KEY,
    CONFIG_VM_BOOT_LINUX_INITRAMFS_KEY,
    CONFIG_VM_BOOT_LINUX_CMDLINE_KEY,

    __CONFIG_OPTIONS_NR,
    __CONFIG_INVALID_KEY,
};

const char *config_key_strings[] = {
    [CONFIG_VM_NAME_KEY] = ADD_NODE(VM_NODE, name),
    [CONFIG_VM_TYPE_KEY] = ADD_NODE(VM_NODE, type),
    [CONFIG_VM_VCPUS_KEY] = ADD_NODE(VM_NODE, vcpus),
    [CONFIG_VM_MEM_SIZE_KEY] = ADD_NODE(VM_MEM_NODE, size),
    [CONFIG_VM_MEM_GRANULARITY_KEY] = ADD_NODE(VM_MEM_NODE, granularity),
    [CONFIG_VM_BOOT_LINUX_BZIMAGE_KEY] = ADD_NODE(VM_LINUX_BOOT_NODE, bzimage),
    [CONFIG_VM_BOOT_LINUX_INITRAMFS_KEY] = ADD_NODE(VM_LINUX_BOOT_NODE, initramfs),
    [CONFIG_VM_BOOT_LINUX_CMDLINE_KEY] = ADD_NODE(VM_LINUX_BOOT_NODE, cmdline),
};

static bool
is_config_whitespace(const char c)
{
    return (c == ' ') || (c == '\r') || (c == '\t');
}

static bool
is_config_delim(const char c)
{
    return c == CONFIG_SECTION_DELIM[0];
}

static bool
is_valid_config_token_char(const char c)
{
    switch (c) {
        case 'A' ... 'Z':
        case 'a' ... 'z':
        case '0' ... '9':
        case '[':
        case ']':
        case '-':
        case '_':
        case '.':
            return true;
        default:
            return false;
    }
}

#endif /* _HYVEMIND_VM_CONFIG_DEFS_H */

