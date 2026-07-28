#ifndef _HYVEMIND_LOADER_LINUX_H
#define _HYVEMIND_LOADER_LINUX_H

#include "hyvstdlib.h"

#define SETUP_HEADER_MAGIC_NUMBER       0xAA55
#define SETUP_HEADER_MAGIC_SIGNATURE    "HdrS"

/**
 * Struct for the setup header located at offset 0x1f1 in the kernel image.
 * We only consider boot protocol 2.15 and don't consider older versions.
 */
struct setup_header {
    uint8_t setup_sects;
    uint16_t root_flags;
    uint32_t syssize;
    uint16_t ram_size;
    uint16_t vid_mode;
    uint16_t root_dev;
    uint16_t boot_flag;
    uint16_t jump;
    uint32_t header;
    uint16_t version;
    uint32_t realmode_swtch;
    uint16_t start_sys_seg;
    uint16_t kernel_version;
    uint8_t type_of_loader;
    uint8_t loadflags;
    uint16_t setup_move_size;
    uint32_t code32_start;
    uint32_t ramdisk_image;
    uint32_t ramdisk_size;
    uint32_t bootsect_kludge;
    uint16_t heap_end_ptr;
    uint8_t ext_loader_ver;
    uint8_t ext_loader_type;
    uint32_t cmd_line_ptr;
    uint32_t initrd_addr_max;
    uint32_t kernel_alignment;
    uint8_t relocatable_kernel;
    uint8_t min_alignment;
    uint16_t xloadflags;
    uint32_t cmdldine_size;
    uint32_t hardware_subarch;
    uint64_t hardware_subarch_data;
    uint32_t payload_offset;
    uint32_t payload_length;
    uint64_t setup_data;
    uint64_t pref_address;
    uint32_t init_size;
    uint32_t handover_offset;
    uint32_t kernel_info_offset;
} __attribute__((packed));

/* Aka. zero-page */
struct boot_params {
    /* todo */
} __attribute__((packed));

#endif /* _HYVEMIND_LOADER_LINUX_H */

