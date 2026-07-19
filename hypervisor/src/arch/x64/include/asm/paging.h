#ifndef _HYVEMIND_X64_ASM_PAGING_H
#define _HYVEMIND_X64_ASM_PAGING_H

#include "types.h"
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

#define LVL4_PAGING_HIGHEST_TRANSLATED_BIT  47
#define LVL4_PAGING_ZERO_CANONICAL_MASK 0
/* Check that 17 consecutive bits (i.e. 63..47) are all 1 */
#define LVL4_PAGING_ONES_CANONICAL_MASK (U64_LSHIFT(1, 17) - 1)

#define IS_PAGE_ALIGNED(addr) (((val) & (PAGE_SIZE - 1)) == 0)
#define PAGE_ALIGN(addr) (((addr) >> PAGE_SHIFT) << PAGE_SHIFT)

extern bool supports_1gb_pages;
#define GB_PAGES_SUPPORTED() supports_1gb_pages

extern uint64_t early_direct_mapping_offset;
#define EARLY_DIRECT_MAPPING_OFFSET early_direct_mapping_offset

extern uint64_t hypervisor_direct_mapping_offset;
#define HYPERVISOR_DIRECT_MAPPING_START     0xFFFF800000000000
#define HYPERVISOR_DIRECT_MAPPING_END       (0xFFFFF80000000000 - 1)
#define HYPERVISOR_DIRECT_MAPPING_OFFSET    0xFFFF800000000000

#define __phys_to_virt(addr, offset) ((addr) + (offset))
#define phys_to_virt(addr) __phys_to_virt(addr, HYPERVISOR_DIRECT_MAPPING_OFFSET)
#define __virt_to_phys(addr, offset) ((addr) - (offset))
#define virt_to_phys(addr) __virt_to_phys(addr, HYPERVISOR_DIRECT_MAPPING_OFFSET)

#define phys_to_pfn(paddr)  (U64_RSHIFT(paddr, PAGE_SHIFT))
#define pfn_to_phys(pfn)    (U64_LSHIFT(pfn, PAGE_SHIFT))

/* @alignment ... must be a power of two */
static inline bool
is_aligned(const uint64_t val, const int alignment)
{
    return (val & (alignment - 1)) == 0;
}

static inline uint64_t
align_down(const uint64_t val, const int alignment)
{
    return (val >> alignment) << alignment;
}

static inline uint64_t
align_forward(const uint64_t val, const int alignment)
{
    int mod = val % alignment;
    if (mod == 0) {
        return val;
    }

    return val + (alignment - mod);
}

/**
 * Since we only support 4-level paging, we simply check 4-level paging
 * canonicality here.
 */
static inline bool
is_paging_canonical(const uint64_t vaddr)
{
    const uint64_t untranslated_bits = vaddr >> LVL4_PAGING_HIGHEST_TRANSLATED_BIT;
    return (untranslated_bits == LVL4_PAGING_ZERO_CANONICAL_MASK) ||
           (untranslated_bits == LVL4_PAGING_ONES_CANONICAL_MASK);
}

#endif /* _HYVEMIND_X64_ASM_PAGING_H */

