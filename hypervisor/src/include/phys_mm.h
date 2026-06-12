#ifndef _HYVEMIND_PHYS_MM_H
#define _HYVEMIND_PHYS_MM_H

#include "limine/limine.h"
#include "mm_types.h"

extern uint8_t boot_mem_info_scratch[];

extern int __hypervisor_base;
extern int __hypervisor_text_start;
extern int __hypervisor_text_end;
extern int __hypervisor_rodata_start;
extern int __hypervisor_rodata_end;
extern int __hypervisor_data_start;
extern int __hypervisor_data_end;

enum memmap_region_type {
    MEMMAP_USABLE,
    MEMMAP_BOOTLOADER_RECLAIMABLE,
    MEMMAP_HYPERVISOR,
    MEMMAP_RESERVED
};
typedef enum memmap_region_type memmap_region_type_t;

struct memmap_entry {
    phys_addr_t start;
    phys_addr_t end;
    uint64_t length;
    memmap_region_type_t type;
};
typedef struct memmap_entry memmap_entry_t;

struct hyvemind_memmap {
    uint64_t nr_entries;
    memmap_entry_t entry[];
};
typedef struct hyvemind_memmap hyv_memmap_t;

/**
 * Wrapper/abstraction around Limine-specific types, so that we don't have
 * Limine types cluttered everywhere in the code.
 */
struct boot_mem_info {
    phys_addr_t mem_map;

    struct {
        phys_addr_t paddr_start;
        virt_addr_t vaddr_start;
        uint64_t length;
    } hypervisor_region;
};
typedef struct boot_mem_info boot_mem_info_t;

struct system_memory_info {
    uint64_t total_size;
    uint64_t early_usable_memory;
    uint64_t total_usable_memory;

    /* There can (and most certainly are) holes in-between */
    phys_addr_t lowest_mapped_addr;
    phys_addr_t highest_mapped_addr;

    uint64_t largest_mapped_pfn;
    /* That covers the area from first mapped addr to last mapped addr, including holes */
    uint64_t nr_of_pfns;
};
typedef struct system_memory_info sys_mem_info_t;

const sys_mem_info_t* init_system_memory_info(const boot_mem_info_t *info);

const sys_mem_info_t* get_system_memory_info(void);

boot_mem_info_t get_boot_mem_info(const limine_memmap_t *limine_mem_map);

#endif /* _HYVEMIND_PHYS_MM_H */

