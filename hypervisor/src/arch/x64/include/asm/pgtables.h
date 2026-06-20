#ifndef _HYVEMIND_X64_ASM_PGTABLES_H
#define _HYVEMIND_X64_ASM_PGTABLES_H

#include "mm_types.h"
#include "pf_alloc.h"
#include "asm/direct_mapping.h"
#include "asm/paging.h"
#include "asm/pgtable_types.h"

#include <stdbool.h>

struct mapping_info {
    struct cr3 *addr_space;
    uint64_t target_offset;
    uint64_t curr_offset;
    uint64_t ps_flags;
    uint64_t nops_flags;
};

extern uint64_t max_phys_addr;

static inline bool
pgtable_entry_present(const pgtable_entry_t *entry)
{
    return entry->raw_entry & 0x01;
}

static inline bool
pgtable_entry_maps_page(const pgtable_entry_t *entry)
{
    return entry->entry_ps.ps;
}

static inline bool
eligible_for_gb_mapping(const virt_addr_t start, const virt_addr_t end)
{
    if (!is_aligned(start, 1 << 30)) {
        return false;
    }

    if (end-start < (PDPT_ENTRY_RANGE_BYTES - 1)) {
        return false;
    }

    return true;
}

static inline bool
eligible_for_mb_mapping(const virt_addr_t start, const virt_addr_t end)
{
    if (!is_aligned(start, 1 << 21)) {
        return false;
    }

    if (end-start < (PDT_ENTRY_RANGE_BYTES - 1)) {
        return false;
    }

    /* todo: change back to true as soon as we implement 2mb mappings */
    return false;
}

static inline int
pml4_entry_index(const virt_addr_t addr)
{
    return (addr << 16) >> 55;
}

static inline int
pdpt_entry_index(const virt_addr_t addr)
{
    return (addr << 25) >> 55;
}

static inline int
pdt_entry_index(const virt_addr_t addr)
{
    return (addr << 34) >> 55;
}

static inline int
pt_entry_index(const virt_addr_t addr)
{
    return (addr << 43) >> 55;
}

static inline pgtable_entry_t *
__get_pgtable_entry(
        const struct mapping_info *mapping_info,
        const phys_addr_t pgtable_addr,
        const int entry_index
)
{
    const phys_addr_t entry_paddr = pgtable_addr + (entry_index << 3);
    return (pgtable_entry_t *) __phys_to_virt(entry_paddr, mapping_info->curr_offset);
}

static inline pgtable_entry_t *
get_pgtable_entry(const phys_addr_t pgtable_addr, const int entry_index)
{
    const phys_addr_t entry_paddr = pgtable_addr + (entry_index << 3);
    return (pgtable_entry_t *) phys_to_virt(entry_paddr);
}

static inline virt_addr_t
get_x_entry_block_end(
        const virt_addr_t start,
        const virt_addr_t end,
        const uint64_t entry_range_bytes
)
{
    const virt_addr_t last_in_entry = start | (entry_range_bytes - 1);
    return (end <= last_in_entry) ? end : last_in_entry;
}

#define get_pml4_entry_block_end(start, end)    \
    get_x_entry_block_end(start, end, PML4_ENTRY_RANGE_BYTES)

#define get_pdpt_entry_block_end(start, end)    \
    get_x_entry_block_end(start, end, PDPT_ENTRY_RANGE_BYTES)

#define get_pdt_entry_block_end(start, end)     \
    get_x_entry_block_end(start, end, PDT_ENTRY_RANGE_BYTES)

#define get_pt_entry_block_end(start, end)      \
    get_x_entry_block_end(start, end, PT_ENTRY_RANGE_BYTES)

static inline phys_addr_t
read_paddr_from_cr3(const struct cr3 *cr3)
{
    return cr3->paddr << PAGE_SHIFT;
}

static inline phys_addr_t
read_paddr_from_nops_entry(const pgtable_entry_t *entry)
{
    return entry->entry_nops.paddr << PAGE_SHIFT;
}

#define read_paddr_from_pml4e(pml4_entry)       \
    read_paddr_from_nops_entry(pml4_entry)

#define read_paddr_from_pdpte_nops(pdpt_entry)  \
    read_paddr_from_nops_entry(pdpt_entry)

#define read_paddr_from_pdte_nops(pdt_entry)    \
    read_paddr_from_nops_entry(pdt_entry)

static inline phys_addr_t
read_paddr_from_ps_entry(const pgtable_entry_t *entry)
{
    return entry->entry_ps.paddr << 13;
}

#define read_paddr_from_pdpte_ps(pdpt_entry)    \
    read_paddr_from_ps_entry(pdpt_entry)

#define read_paddr_from_pdte_ps(pdt_entry)      \
    read_paddr_from_ps_entry(pdt_entry)

static inline phys_addr_t
read_paddr_from_pte(const pgtable_entry_t *pt_entry)
{
    return pt_entry->pt_entry.paddr << PAGE_SHIFT;
}

static inline void
write_paddr_to_cr3(struct cr3 *cr3, const phys_addr_t paddr)
{
    cr3->paddr = (paddr & max_phys_addr) >> PAGE_SHIFT;
}

static inline void
write_paddr_to_pml4e(pgtable_entry_t *pml4_entry, const phys_addr_t paddr)
{
    pml4_entry->entry_nops.paddr = (paddr & max_phys_addr) >> PAGE_SHIFT;
}

static inline void
write_paddr_to_pdpte_ps(pgtable_entry_t *pdpt_entry, const phys_addr_t paddr)
{
    phys_addr_t scratch = (paddr & max_phys_addr) >> 13;
    scratch = (scratch >> 17) << 17;
    pdpt_entry->entry_ps.paddr = scratch;
}

static inline void
write_paddr_to_pdpte_nops(pgtable_entry_t *pdpt_entry, const phys_addr_t paddr)
{
    pdpt_entry->entry_nops.paddr = (paddr & max_phys_addr) >> PAGE_SHIFT;
}

static inline void
write_paddr_to_pdte_ps(pgtable_entry_t *pdt_entry, const phys_addr_t paddr)
{
    phys_addr_t scratch = (paddr & max_phys_addr) >> 13;
    scratch = (scratch >> 8) << 8;
    pdt_entry->entry_ps.paddr = scratch;
}

static inline void
write_paddr_to_pdte_nops(pgtable_entry_t *pdt_entry, const phys_addr_t paddr)
{
    pdt_entry->entry_nops.paddr = (paddr & max_phys_addr) >> PAGE_SHIFT;
}

static inline void
write_paddr_to_pte(pgtable_entry_t *pt_entry, const phys_addr_t paddr)
{
    pt_entry->pt_entry.paddr = (paddr & max_phys_addr) >> PAGE_SHIFT;
}

static inline int
generic_init_nops_entry_raw(
        pgtable_entry_t *entry,
        const uint64_t flags,
        void (*write_paddr)(pgtable_entry_t *, phys_addr_t)
)
{
    phys_addr_t paddr;
    if (get_page_raw(&paddr) != 0) {
        return -1;
    }

    entry->raw_entry |= flags;
    write_paddr(entry, paddr);
    return 0;
}

#define init_pml4_entry_raw(entry, flags)       \
    generic_init_nops_entry_raw(entry, flags, &write_paddr_to_pml4e)

#define init_pdpt_entry_nops_raw(entry, flags)  \
    generic_init_nops_entry_raw(entry, flags, &write_paddr_to_pdpte_nops)

#define init_pdt_entry_nops_raw(entry, flags)   \
    generic_init_nops_entry_raw(entry, flags, &write_paddr_to_pdte_nops)

/* Walk the page tables to resolve the physical address of a given virtual addr */
phys_addr_t resolve_virt_addr(const virt_addr_t vaddr);

int identity_map_mmio_page(const phys_addr_t addr);
int raw_identity_map_mmio_page(struct cr3 *addr_space, const phys_addr_t addr);

typedef void (*page_attr_mod_t)(uint64_t*, const uint64_t);

static void
page_mod_add_attrs(uint64_t *page_attrs, const uint64_t attrs)
{
    *page_attrs |= attrs;
}

static void
page_mod_replace_attrs(uint64_t *page_attrs, const uint64_t attrs)
{
    *page_attrs = attrs;
}

int
modify_page_attributes(
        const virt_addr_t page_start,
        const uint64_t attrs,
        page_attr_mod_t mod_op
);

#endif /* _HYVEMIND_X64_ASM_PGTABLES_H */

