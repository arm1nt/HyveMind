#ifndef _HYVEMIND_X64_ASM_DIRECT_MAPPING_H
#define _HYVEMIND_X64_ASM_DIRECT_MAPPING_H

#include "phys_mm.h"

struct mapping_info;

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
        const hyv_memmap_t *mem_map
);

int
setup_direct_mapping_from_memmap(
        const struct mapping_info *mapping_info,
        const hyv_memmap_t *mem_map
);

#endif /* _HYVEMIND_X64_ASM_DIRECT_MAPPING_H */

