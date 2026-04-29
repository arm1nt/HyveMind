#ifndef _HYVEMIND_MM_H
#define _HYVEMIND_MM_H

#include "types.h"
#include "limine/limine.h"

#include <stdint.h>

/* Marker for verbosity */
#define __directly_mapped

typedef uint64_t virt_addr_t;
typedef uint64_t phys_addr_t;

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

#define phys_addr_to_pfn(addr)  (U64_RSHIFT(addr, PAGE_SHIFT))
#define pfn_to_phys_addr(pfn)   (U64_LSHIFT(pfn, PAGE_SHIFT))

int early_init_page_frame_allocator(
        struct limine_memmap_response * mem_map,
        const uint64_t direct_mapping_offset
);

#endif /* _HYVEMIND_MM_H */

