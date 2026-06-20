#include "printf.h"
#include "asm/processor.h"
#include "asm/pgtables.h"

static inline int
__identity_map_mmio_page(const struct mapping_info *info, const phys_addr_t addr)
{
    return directly_map_phys_range(info, addr, addr + PAGE_SIZE - 1);
}

int
identity_map_mmio_page(const phys_addr_t addr)
{
    const uint64_t raw_addr_space = read_cr3();
    return raw_identity_map_mmio_page((struct cr3 *) &raw_addr_space, addr);
}

int
raw_identity_map_mmio_page(struct cr3 *addr_space, const phys_addr_t addr)
{
    /**
     * Setting PWT & PCD for the mapping page to force the page's memory
     * type to be UC
     */
    const struct mapping_info info = {
        .addr_space = addr_space,
        .curr_offset = HYPERVISOR_DIRECT_MAPPING_OFFSET,
        .target_offset = 0,
        .nops_flags = PGTABLE_RW,
        .ps_flags = PGTABLE_RW | PGTABLE_PWT | PGTABLE_PCD
    };

    return __identity_map_mmio_page(&info, addr);
}

int
modify_page_attributes(
        const virt_addr_t page_start,
        const uint64_t attrs,
        page_attr_mod_t mod_op
)
{
    uint64_t raw_cr3;
    struct cr3 *addr_space;
    phys_addr_t pgtable_ptr;
    unsigned int index;
    pgtable_entry_t *entry;

    raw_cr3 = read_cr3();
    addr_space = (struct cr3 *) &raw_cr3;

    pgtable_ptr = read_paddr_from_cr3(addr_space);
    if (!pgtable_ptr) {
        goto unmapped_page_error_out;
    }

    index = pml4_entry_index(page_start);
    entry = get_pgtable_entry(pgtable_ptr, index);
    if (!pgtable_entry_present(entry)) {
        goto unmapped_page_error_out;
    }

    pgtable_ptr = read_paddr_from_pml4e(entry);
    index = pdpt_entry_index(page_start);
    entry = get_pgtable_entry(pgtable_ptr, index);
    if (!pgtable_entry_present(entry)) {
        goto unmapped_page_error_out;
    } else if (pgtable_entry_maps_page(entry)) {
        mod_op(&entry->raw_entry, attrs);
        return 0;
    }

    pgtable_ptr = read_paddr_from_pdpte_nops(entry);
    index = pdt_entry_index(page_start);
    entry = get_pgtable_entry(pgtable_ptr, index);
    if (!pgtable_entry_present(entry)) {
        goto unmapped_page_error_out;
    } else if (pgtable_entry_maps_page(entry)) {
        mod_op(&entry->raw_entry, attrs);
        return 0;
    }

    pgtable_ptr = read_paddr_from_pdte_nops(entry);
    index = pt_entry_index(page_start);
    entry = get_pgtable_entry(pgtable_ptr, index);
    if (!pgtable_entry_present(entry)) {
        goto unmapped_page_error_out;
    }

    mod_op(&entry->raw_entry, attrs);
    return 0;

unmapped_page_error_out:
    pr_warn("Cannot modify attributes of an unmapped page");
    return -1;
}

