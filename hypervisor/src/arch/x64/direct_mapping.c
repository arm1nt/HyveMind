#include "printf.h"
#include "pf_alloc.h"
#include "string.h"
#include "asm/direct_mapping.h"
#include "asm/mm.h"
#include "asm/paging.h"

static inline bool
phys_memory_fits(void)
{
    /* TODO: Check that present memory fits into allocated VA block */
    return true;
}

static inline phys_addr_t
read_cr3_paddr(const struct cr3 *cr3)
{
    return (cr3->paddr << PAGE_SHIFT) & max_phys_addr;
}

static inline void
write_cr3_paddr(struct cr3 *cr3,  phys_addr_t paddr)
{
    /* TODO: Perhaps a warning and fail if the address is too large */
    cr3->paddr = (paddr & max_phys_addr) >> PAGE_SHIFT;
}

static inline phys_addr_t
allocate_paging_structure(const struct mapping_info *mapping_info)
{
    phys_addr_t raw_addr;
    if (get_page_raw(&raw_addr) != 0) {
        return 0;
    }

    virt_addr_t va = __phys_to_virt(raw_addr, mapping_info->curr_offset);
    memset((void *) va, 0, sizeof(pgtable_structure_t));
    return raw_addr;
}

static inline void *
get_pgtable_entry(
        const phys_addr_t pgtable_addr,
        const int entry_index,
        const struct mapping_info *mapping_info
)
{
    const phys_addr_t addr = pgtable_addr + (entry_index << 3);
    return (void *) __phys_to_virt(addr, mapping_info->curr_offset);
}

static inline void
init_pdpte_entry_no_ps(struct pdpt_entry_no_ps *entry, const struct mapping_info *mapping_info)
{

    phys_addr_t referenced_pgtable = allocate_paging_structure(mapping_info);
    if (!referenced_pgtable) {
        /* prob return error instead */
        return;
    }

    entry->paddr = (referenced_pgtable & max_phys_addr) >> PAGE_SHIFT;
    entry->present = 1;
    entry->rw = 1;
}

static inline void
init_pdt_entry_no_ps(struct pdt_entry_no_ps *entry, const struct mapping_info *mapping_info)
{
    //

    phys_addr_t referenced_pgtable = allocate_paging_structure(mapping_info);
    if (!referenced_pgtable) {
        /* again prob return error */
        return;
    }

    entry->paddr = (referenced_pgtable &max_phys_addr) >> PAGE_SHIFT;
    entry->present = 1;
    entry->rw = 1;
}

static inline void
init_pdpte_entry_ps(struct pdpt_entry_ps *entry)
{
    return;
}


/* TODO: we can make this generic for every level tbh */
static inline void
create_pml4_entry(pml4e_t *entry)
{
    /* todo: would need error handling and zero the allocated page */
    phys_addr_t pdpt_ptr;
    get_page_raw(&pdpt_ptr);
    entry->paddr = (pdpt_ptr & max_phys_addr) >> PAGE_SHIFT;
    entry->present = 1;
}

/* remove and use the generic version */
static inline virt_addr_t
get_block_end_pml4(const virt_addr_t start_vaddr, const virt_addr_t end_vaddr)
{
    const virt_addr_t last_in_entry = start_vaddr | (PML4_ENTRY_RANGE_BYTES -1);
    return (end_vaddr <= last_in_entry) ? end_vaddr : last_in_entry;
}

static inline virt_addr_t
get_block_end_pdpt(const virt_addr_t start_vaddr, const virt_addr_t end_vaddr)
{
    const virt_addr_t last_in_entry = start_vaddr | (PDPT_ENTRY_RANGE_BYTES -1);
    return (end_vaddr <= last_in_entry) ?  end_vaddr : last_in_entry;
}

static inline virt_addr_t
get_block_end_pdt(const virt_addr_t start_vaddr, const virt_addr_t end_vaddr)
{
    const virt_addr_t last_in_entry = start_vaddr | (PD_ENTRY_RANGE_BYTES -1);
    return (end_vaddr <= last_in_entry) ?  end_vaddr : last_in_entry;
}

static inline virt_addr_t
get_block_end_generic(
        const virt_addr_t start_vaddr,
        const virt_addr_t end_vaddr,
        const uint64_t entry_range_bytes
)
{
    const virt_addr_t last_in_entry = start_vaddr | (entry_range_bytes - 1);
    return (end_vaddr <= last_in_entry) ? end_vaddr : last_in_entry;
}

/**
 * TODO: Die namen von allen .._directly_map_range sind alle eine structure zu hoch
 * e.g. pdpt_directly_map sollte eig pdt_directly_map sein etc.
 */

/**
 * todo: need capability to set specific attributes
 */

static void
pdt_directly_map_range(
        struct cr3 *addr_space,
        const struct mapping_info *mapping_info,
        struct pdt_entry_no_ps *pdt_entry,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    /* create macro or function to compute this */
    phys_addr_t pt_ptr = (pdt_entry->paddr << PAGE_SHIFT) & max_phys_addr;

    const virt_addr_t vaddr_start = __phys_to_virt(start, mapping_info->target_offset);
    const virt_addr_t vaddr_end = __phys_to_virt(end, mapping_info->target_offset);
    virt_addr_t curr_vaddr_start = vaddr_start;
    virt_addr_t curr_end;

    while (curr_vaddr_start <= vaddr_end) {
        curr_end = get_block_end_generic(curr_vaddr_start, vaddr_end, PT_ENTRY_RANGE_BYTES);

        int index = pt_entry_index(curr_vaddr_start);
        pt_entry_t *pt_entry = get_pgtable_entry(pt_ptr, index, mapping_info);

        if (!pgtable_entry_present(pt_entry)) {
            pt_entry->present = 1;
            pt_entry->rw = 1;
            pt_entry->paddr = __virt_to_phys(curr_vaddr_start, mapping_info->target_offset) >> PAGE_SHIFT;
        }

        curr_vaddr_start = curr_end + 1;
    }

    return;
}

static inline bool
eligible_for_2mb_mapping(const virt_addr_t vaddr_start, const virt_addr_t vaddr_end)
{
    return false;
}

static void
pdpt_directly_map_range(
        struct cr3 *addr_space,
        const struct mapping_info *mapping_info,
        struct pdpt_entry_no_ps *pdpt_entry,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    phys_addr_t pdt_ptr = (pdpt_entry->paddr << PAGE_SHIFT) & max_phys_addr;

    const virt_addr_t vaddr_start = __phys_to_virt(start, mapping_info->target_offset);
    const virt_addr_t vaddr_end = __phys_to_virt(end, mapping_info->target_offset);
    virt_addr_t curr_vaddr_start = vaddr_start;
    virt_addr_t curr_end;

    while (curr_vaddr_start <= vaddr_end) {
        curr_end = get_block_end_generic(curr_vaddr_start, vaddr_end, PD_ENTRY_RANGE_BYTES);

        int index = pdt_entry_index(curr_vaddr_start);
        pdt_entry_t *pdt_entry = get_pgtable_entry(pdt_ptr, index, mapping_info);

        if (pgtable_entry_maps_page(pdt_entry)) {
            goto skip_2mb_mapping;
        }

        if (eligible_for_2mb_mapping(curr_vaddr_start, curr_end)) {
            goto do_2mb_mapping;
        }

do_mapping_via_pt:
        printf("Map (%lx - %lx) via next (page table) paging structure", curr_vaddr_start, curr_end);

        if (!pgtable_entry_present(pdt_entry)) {
            init_pdt_entry_no_ps((struct pdt_entry_no_ps *) pdt_entry, mapping_info);
        }

        pdt_directly_map_range(
                addr_space,
                mapping_info,
                (struct pdt_entry_no_ps *) pdt_entry,
                __virt_to_phys(curr_vaddr_start, mapping_info->target_offset),
                __virt_to_phys(curr_end, mapping_info->target_offset)
        );

        curr_vaddr_start = curr_end + 1;
        continue;

do_2mb_mapping:
        printf("Establishing a 2mb direct mapping for (%lx - %lx)", curr_vaddr_start, curr_end);

        curr_vaddr_start = curr_end + 1;
        continue;

skip_2mb_mapping:
        printf("Skipping establishing a mapping for (%lx - %lx)", curr_vaddr_start, curr_end);
        curr_vaddr_start = curr_end + 1;
        continue;
    }

    return;
}

static void
free_pd_table(void *table)
{
    /**
     * Recursively free all contained entries until we free ourselves
     */
}

static inline bool
eligible_for_gb_mapping(const virt_addr_t vaddr_start, const virt_addr_t vaddr_end)
{
    /**
     * Only eligible if the start vaddr is gb aligned and if this block covers
     * a 1gb addr space. If these reqs are not fullfilled, a gb mapping would map
     * (way) too many addresses.
     */

    const uint64_t mask = ~(PDPT_ENTRY_RANGE_BYTES-1);
    const bool is_gb_aligned = (vaddr_start == (vaddr_start & mask));

    const bool spans_gb_range = ((vaddr_end - vaddr_start) == (PDPT_ENTRY_RANGE_BYTES -1));

    return is_gb_aligned && spans_gb_range;
}

/* phys addr and directly mapped virt addr must have same alignment */
static void
pml4_directly_map_range(
        struct cr3 *addr_space,
        const struct mapping_info *mapping_info,
        pml4e_t *pml4_entry,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    phys_addr_t pdpt_ptr = (pml4_entry->paddr << PAGE_SHIFT) & max_phys_addr;

    const virt_addr_t __directly_mapped vaddr_start = __phys_to_virt(
            start,
            mapping_info->target_offset
    );
    const virt_addr_t __directly_mapped vaddr_end = __phys_to_virt(
            end,
            mapping_info->target_offset
    );
    virt_addr_t __directly_mapped curr_vaddr_start = vaddr_start;
    virt_addr_t __directly_mapped curr_end;

    while (curr_vaddr_start <= vaddr_end) {
        curr_end = get_block_end_pdpt(curr_vaddr_start, vaddr_end);

        const int index = pdpt_entry_index(curr_vaddr_start);
        pdpt_entry_t *pdpt_entry = get_pgtable_entry(pdpt_ptr, index, mapping_info);

        /**
         * We dont ensure here that the addr space remains consistent. If there is
         * e.g. already a gb mapping for a different offset, and we try to
         * establish a mapping for a 2mb or 4kb range with a different offset,
         * we'll assume that the currently installed mapping belongs to the same
         * mapping and re-use it.
         * Ensuring that the addr space remains consistent should be handled
         * before installing a new mapping
         */
        if (!supports_1gb_pages) {
            goto do_mapping_via_pdt;
        } else {

            if (pgtable_entry_maps_page(pdpt_entry)) {
                goto skip_mapping;
            }

            if (eligible_for_gb_mapping(curr_vaddr_start, curr_end)) {
                goto do_1gb_mapping;
            }

            goto do_mapping_via_pdt;
        }

do_mapping_via_pdt:
        printf("Map (%lx - %lx) via next (page directory table) paging structure", curr_vaddr_start, curr_end);

        if (!pgtable_entry_present(pdpt_entry)) {
           init_pdpte_entry_no_ps((struct pdpt_entry_no_ps *) pdpt_entry, mapping_info);
        }

        pdpt_directly_map_range(
                addr_space,
                mapping_info,
                (struct pdpt_entry_no_ps *) pdpt_entry,
                __virt_to_phys(curr_vaddr_start, mapping_info->target_offset),
                __virt_to_phys(curr_end, mapping_info->target_offset)
        );

        curr_vaddr_start = curr_end + 1;
        continue;

do_1gb_mapping:
        printf("Map range via a 1GB page");

        curr_vaddr_start = curr_end + 1;
        continue;

skip_mapping:
        printf("Range is already mapped");
        curr_vaddr_start = curr_end + 1;
        continue;
    }

    return;
}

/* phys addr and directly mapped virt addr must have same alignment */
int
directly_map_range(
        struct cr3 *addr_space,
        const struct mapping_info *mapping_info,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    const phys_addr_t aligned_start = PAGE_ALIGN(start);

    phys_addr_t pml4_ptr = read_cr3_paddr(addr_space);
    if (!pml4_ptr) {

        phys_addr_t raw_addr = allocate_paging_structure(mapping_info);
        if (!raw_addr) {
            printf("Failed to allocate a page for the lvl4 page table");
            return -1;
        }

        write_cr3_paddr(addr_space, raw_addr);
        pml4_ptr = raw_addr;
    }

    const virt_addr_t __directly_mapped vaddr_start = __phys_to_virt(aligned_start, mapping_info->target_offset);
    const virt_addr_t __directly_mapped vaddr_end = __phys_to_virt(end, mapping_info->target_offset);
    virt_addr_t __directly_mapped curr_vaddr_start = vaddr_start;
    virt_addr_t __directly_mapped curr_end;

    while (curr_vaddr_start <= vaddr_end) {
        const int index = pml4_entry_index(curr_vaddr_start);

        pml4e_t *pml4_entry = (pml4e_t *) get_pgtable_entry(pml4_ptr, index, mapping_info);
        if (!pgtable_entry_present(pml4_entry)) {
            pml4_entry->paddr = (allocate_paging_structure(mapping_info) & max_phys_addr) >> PAGE_SHIFT;
            pml4_entry->present = 1;
            pml4_entry->rw = 1;
        }

        curr_end = get_block_end_pml4(curr_vaddr_start, vaddr_end);
        pml4_directly_map_range(
                addr_space,
                mapping_info,
                pml4_entry,
                __virt_to_phys(curr_vaddr_start, mapping_info->target_offset),
                __virt_to_phys(curr_end, mapping_info->target_offset)
        );

        curr_vaddr_start = curr_end+1;
    }

    return 0;
}

int
directly_map_from_memory_map(
        struct cr3 *addr_space,
        const struct limine_memmap_response *mem_map,
        const struct mapping_info *mapping_info
)
{
    int ret = 0;
    struct limine_memmap_entry *entry;

    for (uint64_t i = 0; i < mem_map->entry_count; i++) {
        entry = mem_map->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE
                && entry->type != LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE
                && entry->type != LIMINE_MEMMAP_EXECUTABLE_AND_MODULES)
        {
            continue;
        }

        ret = directly_map_range(
                addr_space,
                mapping_info,
                entry->base,
                entry->base + entry->length -1
        );

        if (ret != 0)
            break;
    }

    return ret;
}

int
early_setup_direct_mapping(
        struct cr3 *addr_space,
        const struct limine_memmap_response *mem_map,
        const struct mapping_info *mapping_info
)
{
    if (!phys_memory_fits()) {
        printf("Physical memory present on the system does not fit into the "
                "virtual address space block allocated for the direct mapping");
        return -1;
    }

    return directly_map_from_memory_map(addr_space, mem_map, mapping_info);
}

