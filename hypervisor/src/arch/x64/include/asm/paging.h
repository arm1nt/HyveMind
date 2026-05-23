#ifndef _HYVEMIND_X64_ASM_PAGING_H
#define _HYVEMIND_X64_ASM_PAGING_H

#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

#define IS_PAGE_ALIGNED(addr) (0)
#define PAGE_ALIGN(addr) (((addr) >> PAGE_SHIFT) << PAGE_SHIFT)

extern bool supports_1gb_pages;
#define GB_PAGES_SUPPORTED() supports_1gb_pages

extern uint64_t early_direct_mapping_offset;
#define EARLY_DIRECT_MAPPING_OFFSET early_direct_mapping_offset

extern uint64_t hypervisor_direct_mapping_offset;
#define HYPERVISOR_DIRECT_MAPPING_START     0xFFFF800000000000
#define HYPERVISOR_DIRECT_MAPPING_END       0xFFFFF80000000000
#define HYPERVISOR_DIRECT_MAPPING_OFFSET    0xFFFF800000000000

#define __phys_to_virt(addr, offset) ((addr) + (offset))
#define phys_to_virt(addr) __phys_to_virt(addr, HYPERVISOR_DIRECT_MAPPING_OFFSET)
#define __virt_to_phys(addr, offset) ((addr) - (offset))
#define virt_to_phys(addr) __virt_to_phys(addr, HYPERVISOR_DIRECT_MAPPING_OFFSET)

#endif /* _HYVEMIND_X64_ASM_PAGING_H */

