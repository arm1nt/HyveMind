#ifndef _HYVEMIND_LOADER_LINUX_H
#define _HYVEMIND_LOADER_LINUX_H

#include "hyvstdlib.h"

#define SETUP_HEADER_OFFSET 0x1F1

#define SETUP_HEADER_MAGIC_NUMBER       0xAA55
#define SETUP_HEADER_MAGIC_SIGNATURE    "HdrS"

#define SETUP_PROTECTED_MODE_LOADED_LOW     0x10000
#define SETUP_PROTECTED_MODE_LOADED_HIGH    0x100000

#define SETUP_LOADFLAGS_LOADED_HIGH     (1)
#define SETUP_LOADFLAGS_KASLR           (1 << 1)
#define SETUP_LOADFLAGS_QUIET           (1 << 5)
#define SETUP_LOADFLAGS_CAN_USE_HEAP    (1 << 7)

#define SETUP_XLF_KERNEL_64                 (1)
#define SETUP_XLF_CAN_BE_LOADED_ABOVE_4G    (1 << 1)

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

#define BOOT_PARAMS_MAX_E820_ENTRIES 128

struct e820_entry {
    uint64_t addr;
    uint64_t size;
    uint32_t type;
} __attribute__((packed));

struct screen_info {
    uint8_t dummy[0x040];
};

struct apm_bios_info {
    uint8_t dummy[0x014];
};

struct ist_info {
    uint8_t dummy[0x010];
};

struct olpc_ofw_header {
    uint8_t dummy[16];
};

struct edid_info {
    uint8_t dummy[0x080];
};

struct efi_info {
    uint8_t dummy[0x020];
};

/* Aka. zero-page */
struct boot_params {
    struct screen_info screen_info;             /* 0x000 - 0x03F */
    struct apm_bios_info apm_bios_info;         /* 0x040 - 0x053 */
    uint8_t pad0[4];                            /* 0x054 - 0x057 */
    uint8_t tboot_addr;                         /* 0x058 - 0x05F */
    struct ist_info ist_info;                   /* 0x060 - 0x06F */
    uint64_t acpi_rsdp_addr;                    /* 0x070 - 0x077 */
    uint8_t pad1[8];                            /* 0x078 - 0x07F */
    uint8_t hd0_info[16];                       /* 0x080 - 0x08F */
    uint8_t hd1_info[16];                       /* 0x090 - 0x09F */
    uint8_t sys_desc_table[16];                 /* 0x0A0 - 0x0AF */
    struct olpc_ofw_header olpc_ofw_header;     /* 0x0B0 - 0x0BF */
    uint32_t ext_ramdisk_image;                 /* 0x0C0 - 0x0C3 */
    uint32_t ext_ramdisk_size;                  /* 0x0C4 - 0x0C7 */
    uint32_t ext_cmd_line_ptr;                  /* 0x0C8 - 0x0CB */
    uint8_t pad2[112];                          /* 0x0CC - 0x13B */
    uint32_t cc_blob_address;                   /* 0x13C - 0x13F */
    struct edid_info edid_info;                 /* 0x140 - 0x1BF */
    struct efi_info efi_info;                   /* 0x1C0 - 0x1DF */
    uint32_t alt_mem_k;                         /* 0x1E0 - 0x1E3 */
    uint32_t scratch;                           /* 0x1E4 - 0x1E7 */
    uint8_t e820_entries;                       /* 0x1E8 - 0x1E8 */
    uint8_t eddbuf_entries;                     /* 0x1E9 - 0x1E9 */
    uint8_t edd_mbr_sig_buf_entries;            /* 0x1EA - 0x1EA */
    uint8_t kbd_status;                         /* 0x1EB - 0x1EB */
    uint8_t secure_boot;                        /* 0x1EC - 0x1EC */
    uint8_t pad3[2];                            /* 0x1ED - 0x1EE */
    uint8_t sentinel;                           /* 0x1EF - 0x1EF */
    uint8_t pad4[1];                            /* 0x1F0 - 0x1F0 */
    struct setup_header hdr;                    /* 0x1F1 - hdr_size - 1*/
    uint8_t pad5[0x290 - 0x1f1 - sizeof(struct setup_header)];
    uint8_t edd_mbr_sig_buffer[0x040];          /* 0x290 - 0x2CF */
    struct e820_entry e820_table[BOOT_PARAMS_MAX_E820_ENTRIES]; /* 0x2D0 - 0xCCF */
    uint8_t pad6[48];                           /* 0xCD0 - 0xCFF */
    uint8_t eddbuf[0x1EC];                      /* 0xD00 - 0xEEB */
    uint8_t pad7[276];                          /* 0xEEC - 0xFFF */
} __attribute__((packed));

#endif /* _HYVEMIND_LOADER_LINUX_H */

