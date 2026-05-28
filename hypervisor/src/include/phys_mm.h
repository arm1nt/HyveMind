#ifndef _HYVEMIND_PHYS_MM_H
#define _HYVEMIND_PHYS_MM_H

#include "limine/limine.h"
#include "mm_types.h"

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

const sys_mem_info_t* init_system_memory_info(const struct limine_memmap_response *mem_map);

inline const sys_mem_info_t* get_system_memory_info(void);

#endif /* _HYVEMIND_PHYS_MM_H */

