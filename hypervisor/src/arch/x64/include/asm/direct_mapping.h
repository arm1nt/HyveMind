#ifndef _HYVEMIND_X64_ASM_DIRECT_MAPPING_H
#define _HYVEMIND_X64_ASM_DIRECT_MAPPING_H

#include "limine/limine.h"
#include "mm_types.h"
#include "asm/pgtable_types.h"

/**
 * Necessary to replace a direct mapping already in place
 */
struct mapping_info {
    uint64_t curr_offset;
    uint64_t target_offset;
};

int
directly_map_range(
        struct cr3 *addr_space,
        const struct mapping_info *mapping_info,
        const phys_addr_t start,
        const phys_addr_t end
);

int
directly_map_from_memory_map(
        struct cr3 *addr_space,
        const struct limine_memmap_response *mem_map,
        const struct mapping_info *mapping_info
);

int
early_setup_direct_mapping(
        struct cr3 *addr_space,
        const struct limine_memmap_response *mem_map,
        const struct mapping_info *mapping_info
);

#endif /* _HYVEMIND_X64_ASM_DIRECT_MAPPING_H */

