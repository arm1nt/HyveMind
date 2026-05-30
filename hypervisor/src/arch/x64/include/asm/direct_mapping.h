#ifndef _HYVEMIND_X64_ASM_DIRECT_MAPPING_H
#define _HYVEMIND_X64_ASM_DIRECT_MAPPING_H

#include "limine/limine.h"
#include "mm_types.h"
#include "asm/pgtable_types.h"

struct mapping_info {
    struct cr3 *addr_space;
    uint64_t target_offset;
    uint64_t curr_offset;
    uint64_t ps_flags;
    uint64_t nops_flags;
};

int
directly_map_phys_range(
        const struct mapping_info *mapping_info,
        const phys_addr_t start,
        const phys_addr_t end
);

/**
 * Does not check whether the available physical memory fits into the virtual
 * address space range allocated for the direct mapping
 */
int
raw_setup_direct_mapping_from_memmap(
        const struct mapping_info *mapping_info,
        const struct limine_memmap_response *mem_map
);

int
setup_direct_mapping_from_memmap(
        const struct mapping_info *mapping_info,
        const struct limine_memmap_response *mem_map
);

#endif /* _HYVEMIND_X64_ASM_DIRECT_MAPPING_H */

