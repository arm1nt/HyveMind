#ifndef _HYVEMIND_MM_H
#define _HYVEMIND_MM_H

#include "types.h"
#include "limine/limine.h"

#include <stdint.h>

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

/* Marker for verbosity */
#define __directly_mapped

typedef uint64_t phys_addr_t;
typedef uint64_t virt_addr_t;

extern uint64_t direct_mapping_offset;
#define DIRECT_MAPPING_OFFSET direct_mapping_offset

#define phys_to_directly_mapped(addr) (addr + DIRECT_MAPPING_OFFSET)
#define directly_mapped_to_phys(virt) (virt - DIRECT_MAPPING_OFFSET)
#define phys_to_pfn(addr)   (U64_RSHIFT(addr, PAGE_SHIFT))
#define pfn_to_phys(pfn)    (U64_LSHIFT(pfn, PAGE_SHIFT))
#define directly_mapped_to_pfn(virt)    (phys_to_pfn(directly_mapped_to_phys(virt)))
#define pfn_to_directly_mapped(pfn)     (phys_to_directly_mapped(pfn_to_phys(pfn)))

/* Returns physical address of a free page */
int get_page_raw(phys_addr_t *pf_addr);
/* Returns physical address of first pf of a contigous block of size 'nr' */
int get_pages_raw(const uint64_t nr, phys_addr_t *start_pf_addr);
/* Get physical addr of pf that is aligned as requested */
int get_page_raw_aligned(phys_addr_t *pf_addr, const int alignment);
/* Get physical addr of pf of contigous block where addr of the first pf is aligned as requested */
int get_pages_raw_aligned(const uint64_t nr, phys_addr_t *start_pf_addr, const int alignment);

/* As above, but returns the directly mapped virtual addresses */
int get_page(virt_addr_t __directly_mapped *addr);
int get_pages(const uint64_t nr, virt_addr_t __directly_mapped *start_addr);
int get_page_aligned(virt_addr_t __directly_mapped *addr, const int alignment);
int get_pages_aligned(const uint64_t nr, virt_addr_t __directly_mapped *start_addr, const int alignment);

int free_page_raw(const phys_addr_t addr);
int free_pages_raw(const uint64_t nr, const phys_addr_t addr);
int free_page(const virt_addr_t __directly_mapped addr);
int free_pages(const uint64_t nr, const virt_addr_t __directly_mapped addr);

int early_init_page_frame_allocator(const struct limine_memmap_response *mem_map);

#endif /* _HYVEMIND_MM_H */

