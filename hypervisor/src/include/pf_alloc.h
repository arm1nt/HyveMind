#ifndef _HYVEMIND_PF_ALLOC_H
#define _HYVEMIND_PF_ALLOC_H

#include "limine/limine.h"
#include "mm_types.h"

int get_page_raw(phys_addr_t *pf_addr);
/* Returns physically contigous block of 'nr' pages, if possible */
int get_pages_raw(const uint64_t nr, phys_addr_t *start_pf_addr);
/* Same as their '.._raw' counterparts, but returns directly mapped virtual addresses */
int get_page(virt_addr_t __directly_mapped *addr);
int get_pages(const uint64_t nr, virt_addr_t __directly_mapped *start_addr);

int free_page_raw(const phys_addr_t addr);
int free_pages_raw(const uint64_t nr, const phys_addr_t addr);
int free_page(const virt_addr_t __directly_mapped addr);
int free_pages(const uint64_t nr, const virt_addr_t __directly_mapped addr);

int
early_init_page_frame_allocator(
        const struct limine_memmap_response *mem_map,
        const uint64_t direct_mapping_offset
);

#endif /* _HYVEMIND_PF_ALLOC_H */

