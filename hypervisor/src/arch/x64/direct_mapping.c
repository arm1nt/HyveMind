#include "printf.h"
#include "phys_mm.h"
#include "pf_alloc.h"
#include "string.h"
#include "asm/paging.h"
#include "asm/pgtables.h"
#include "asm/direct_mapping.h"

static inline void
zero_pgtable_struct(const phys_addr_t paddr, const struct mapping_info *mapping_info)
{
    const virt_addr_t va = __phys_to_virt(paddr, mapping_info->curr_offset);
    memset((void *) va, 0, sizeof(pgtable_structure_t));
}

static inline int
_init_cr3(struct cr3 *cr3, const struct mapping_info *mapping_info)
{
    phys_addr_t paddr;
    if (get_page_raw(&paddr) != 0) {
        return -1;
    }

    zero_pgtable_struct(paddr, mapping_info);
    write_paddr_to_cr3(cr3, paddr);
    return 0;
}

static inline int
_init_pml4_entry(pgtable_entry_t *entry, const struct mapping_info *mapping_info)
{
    if (init_pml4_entry_raw(entry, mapping_info->nops_flags | PGTABLE_PRESENT) != 0) {
        return -1;
    }
    zero_pgtable_struct(read_paddr_from_pml4e(entry), mapping_info);
    return 0;
}

static inline int
_init_pdpte_nops_entry(pgtable_entry_t *entry, const struct mapping_info *mapping_info)
{
    if (init_pdpt_entry_nops_raw(entry, mapping_info->nops_flags | PGTABLE_PRESENT) != 0) {
        return -1;
    }
    zero_pgtable_struct(read_paddr_from_pdpte_nops(entry), mapping_info);
    return 0;
}

static inline int
_init_pdt_nops_entry(pgtable_entry_t *entry, const struct mapping_info *mapping_info)
{
    if (init_pdt_entry_nops_raw(entry, mapping_info->nops_flags | PGTABLE_PRESENT) != 0) {
        return -1;
    }
    zero_pgtable_struct(read_paddr_from_pdte_nops(entry), mapping_info);
    return 0;
}

static inline pgtable_entry_t *
get_pgtable_entry(
        const struct mapping_info *mapping_info,
        const phys_addr_t pgtable_addr,
        const int entry_index
)
{
    const phys_addr_t entry_paddr = pgtable_addr + (entry_index << 3);
    return (pgtable_entry_t *) __phys_to_virt(entry_paddr, mapping_info->curr_offset);
}

static int
pt_map_range(
        const struct mapping_info *mapping_info,
        const phys_addr_t pt_ptr,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    int ret = 0;

    const virt_addr_t vaddr_start = __phys_to_virt(start, mapping_info->target_offset);
    const virt_addr_t vaddr_end = __phys_to_virt(end, mapping_info->target_offset);
    virt_addr_t curr_vaddr_start = vaddr_start;
    virt_addr_t curr_vaddr_end;

    while (curr_vaddr_start <= vaddr_end) {
        curr_vaddr_end = get_pt_entry_block_end(curr_vaddr_start, vaddr_end);

        const int entry_index = pt_entry_index(curr_vaddr_start);
        pgtable_entry_t *entry = get_pgtable_entry(mapping_info, pt_ptr, entry_index);

        if (!pgtable_entry_present(entry)) {
            /* Since we currently don't use PAT, we can simply reuse the generic nops flags */
            entry->raw_entry |= mapping_info->nops_flags | PGTABLE_PRESENT;
            write_paddr_to_pte(entry, __virt_to_phys(curr_vaddr_start, mapping_info->target_offset));
        }

        curr_vaddr_start = curr_vaddr_end + 1;
    }

    return ret;
}

static int
pdt_map_range(
        const struct mapping_info *mapping_info,
        const phys_addr_t pdt_ptr,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    int ret = 0;

    const virt_addr_t vaddr_start = __phys_to_virt(start, mapping_info->target_offset);
    const virt_addr_t vaddr_end = __phys_to_virt(end, mapping_info->target_offset);
    virt_addr_t curr_vaddr_start = vaddr_start;
    virt_addr_t curr_vaddr_end;

    while (curr_vaddr_start <= vaddr_end) {
        curr_vaddr_end = get_pdt_entry_block_end(curr_vaddr_start, vaddr_end);

        const int entry_index = pdt_entry_index(curr_vaddr_start);
        pgtable_entry_t *entry = get_pgtable_entry(mapping_info, pdt_ptr, entry_index);

        if (pgtable_entry_present(entry) && pgtable_entry_maps_page(entry)) {
            goto skip_pdt_mapping;
        }

        if (!pgtable_entry_present(entry)
                && eligible_for_mb_mapping(curr_vaddr_start, curr_vaddr_end)) {
            goto do_mb_mapping;
        }

        if (!pgtable_entry_present(entry)) {
            ret = _init_pdt_nops_entry(entry, mapping_info);
            if (ret != 0) {
                return ret;
            }
        }

        const phys_addr_t pt_ptr = read_paddr_from_pdte_nops(entry);
        ret = pt_map_range(
                mapping_info,
                pt_ptr,
                __virt_to_phys(curr_vaddr_start, mapping_info->target_offset),
                __virt_to_phys(curr_vaddr_end, mapping_info->target_offset)
        );

        if (ret != 0) {
            return ret;
        }

        curr_vaddr_start = curr_vaddr_end + 1;
        continue;

do_mb_mapping:
        curr_vaddr_start = curr_vaddr_end + 1;
        continue;

skip_pdt_mapping:
        curr_vaddr_start = curr_vaddr_end + 1;
        continue;
    }

    return ret;
}

static int
pdpt_map_range(
        const struct mapping_info *mapping_info,
        const phys_addr_t pdpt_ptr,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    int ret = 0;

    const virt_addr_t vaddr_start = __phys_to_virt(start, mapping_info->target_offset);
    const virt_addr_t vaddr_end = __phys_to_virt(end, mapping_info->target_offset);
    virt_addr_t curr_vaddr_start = vaddr_start;
    virt_addr_t curr_vaddr_end;

    while (curr_vaddr_start <= vaddr_end) {
        curr_vaddr_end = get_pdpt_entry_block_end(curr_vaddr_start, vaddr_end);

        const int entry_index = pdpt_entry_index(curr_vaddr_start);
        pgtable_entry_t *entry = get_pgtable_entry(mapping_info, pdpt_ptr, entry_index);

        if (!supports_1gb_pages) {
            goto do_mapping_via_pdt;
        } else {

            if (pgtable_entry_present(entry) && pgtable_entry_maps_page(entry)) {
                goto skip_pdpt_mapping;
            }

            if (!pgtable_entry_present(entry)
                    && eligible_for_gb_mapping(curr_vaddr_start, curr_vaddr_end)) {
                goto do_gb_mapping;
            }

            goto do_mapping_via_pdt;
        }

do_mapping_via_pdt:
        if (!pgtable_entry_present(entry)) {
            ret = _init_pdpte_nops_entry(entry, mapping_info);
            if (ret != 0) {
                return ret;
            }
        }

        const phys_addr_t pdt_ptr = read_paddr_from_pdpte_nops(entry);
        ret = pdt_map_range(
                mapping_info,
                pdt_ptr,
                __virt_to_phys(curr_vaddr_start, mapping_info->target_offset),
                __virt_to_phys(curr_vaddr_end, mapping_info->target_offset)
        );

        if (ret != 0) {
            return ret;
        }

        curr_vaddr_start = curr_vaddr_end + 1;
        continue;

do_gb_mapping:
        curr_vaddr_start = curr_vaddr_end + 1;
        continue;

skip_pdpt_mapping:
        curr_vaddr_start = curr_vaddr_end + 1;
        continue;
    }

    return ret;
}

static int
pml4_map_range(
        const struct mapping_info *mapping_info,
        const phys_addr_t pml4_ptr,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    int ret = 0;

    const virt_addr_t vaddr_start = __phys_to_virt(start, mapping_info->target_offset);
    const virt_addr_t vaddr_end = __phys_to_virt(end, mapping_info->target_offset);
    virt_addr_t curr_vaddr_start = vaddr_start;
    virt_addr_t curr_vaddr_end;

    while (curr_vaddr_start <= vaddr_end) {
        curr_vaddr_end = get_pml4_entry_block_end(curr_vaddr_start, vaddr_end);

        const int entry_index = pml4_entry_index(curr_vaddr_start);
        pgtable_entry_t *entry = get_pgtable_entry(mapping_info, pml4_ptr, entry_index);
        if (!pgtable_entry_present(entry)) {
            ret = _init_pml4_entry(entry, mapping_info);
            if (ret != 0) {
                return ret;
            }
        }

        const phys_addr_t pdpt_ptr = read_paddr_from_pml4e(entry);
        ret = pdpt_map_range(
                mapping_info,
                pdpt_ptr,
                __virt_to_phys(curr_vaddr_start, mapping_info->target_offset),
                __virt_to_phys(curr_vaddr_end, mapping_info->target_offset)
        );

        if (ret != 0) {
            return ret;
        }

        curr_vaddr_start = curr_vaddr_end + 1;
    }

    return ret;
}

int
directly_map_phys_range(
        const struct mapping_info *mapping_info,
        const phys_addr_t start,
        const phys_addr_t end
)
{
    const phys_addr_t aligned_start = PAGE_ALIGN(start);

    phys_addr_t pml4_ptr = read_paddr_from_cr3(mapping_info->addr_space);
    if (!pml4_ptr) {
        if (_init_cr3(mapping_info->addr_space, mapping_info) != 0) {
            return -1;
        }
        pml4_ptr = read_paddr_from_cr3(mapping_info->addr_space);
    }

    return pml4_map_range(mapping_info, pml4_ptr, aligned_start, end);
}

int
raw_setup_direct_mapping_from_memmap(
        const struct mapping_info *mapping_info,
        const struct limine_memmap_response *mem_map
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

        ret = directly_map_phys_range(
                mapping_info,
                entry->base,
                entry->base + entry->length - 1
        );

        if (ret != 0) {
            break;
        }
    }

    return ret;
}

static inline bool
phys_memory_fits(void)
{
    const sys_mem_info_t *sys_mem_info = get_system_memory_info();
    /* Need to take into consideration that mapping are done on page-granularity */
    const uint64_t rounded_range = align_forward(
            sys_mem_info->total_usable_memory,
            PAGE_SIZE
    );
    const uint64_t direct_mapping_vaddr_range =
        HYPERVISOR_DIRECT_MAPPING_END - HYPERVISOR_DIRECT_MAPPING_START + 1;

    return rounded_range <= direct_mapping_vaddr_range;
}

int
setup_direct_mapping_from_memmap(
        const struct mapping_info *mapping_info,
        const struct limine_memmap_response *mem_map
)
{
    if (!phys_memory_fits()) {
        printf("Physical memory present on the system does not fit into the "
                "virtual address space block allocated for the direct mapping");
        return -1;
    }

    return raw_setup_direct_mapping_from_memmap(mapping_info, mem_map);
}

