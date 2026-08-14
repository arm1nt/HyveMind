#include "pf_alloc.h"
#include "printf.h"
#include "string.h"
#include "asm/paging.h"
#include "vmx/ept.h"

#define SCALE_BLOCK_INDEX(raw_index) ((raw_index) << 3)

static inline bool
should_do_gb_mapping(
        const gpaddr gpa_start,
        const gpaddr gpa_end,
        const struct ept_mapping_info *info
) {
    if (!info->use_gb_mappings) {
        return false;
    }

    /* The range must really cover the entire 1gb space */
    if ((gpa_end - gpa_start + 1) < U64(1 << 30)) {
        return false;
    }

    /* We require the start addr to be 1gb page aligned */
    if ((gpa_start % U64(1 << 30)) != 0) {
        return false;
    }

    if (((gpa_start + info->offset) % U64(1 << 30)) != 0) {
        return false;
    }

    return true;
}

static inline bool
should_do_mb_mapping(
        const gpaddr gpa_start,
        const gpaddr gpa_end,
        const struct ept_mapping_info *info
) {
    if (!info->use_mb_mappings) {
        return false;
    }

    if ((gpa_end - gpa_start + 1) < U64(1 << 20)) {
        return false;
    }

    if ((gpa_start % U64(1 << 20)) != 0) {
        return false;
    }

    if (((gpa_start + info->offset) % U64(1 << 20)) != 0) {
        return false;
    }

    return true;
}

static inline phys_addr_t
__allocate_ept_paging_structure(void)
{
    phys_addr_t ept_table_ptr;
    if (get_page_raw(&ept_table_ptr) != 0) {
        pr_warn("Failed to allocate page for ept paging structure");
        return 0;
    }

    memset((void *) phys_to_virt(ept_table_ptr), 0, PAGE_SIZE);
    return ept_table_ptr;
}

static inline int
__init_pd_table_entry(ept_pde *entry, const struct ept_mapping_info *info)
{
    const phys_addr_t ept_pt_table_ptr = __allocate_ept_paging_structure();
    if (!ept_pt_table_ptr) {
        pr_error("Failed to allocate memory for a ept pt table");
        return -1;
    }

    ept_pde_set_paddr(entry, ept_pt_table_ptr);
    entry->raw |= info->no_page_flags;

    return 0;
}

static inline int
__init_pdpt_entry(ept_pdpte *entry, const struct ept_mapping_info *info)
{
    const phys_addr_t ept_pd_table_ptr = __allocate_ept_paging_structure();
    if (!ept_pd_table_ptr) {
        pr_error("Failed to allocate memory for a ept pd table");
        return -1;
    }

    ept_pdpte_set_paddr(entry, ept_pd_table_ptr);
    entry->raw |= info->no_page_flags;

    return 0;
}

static inline int
__init_pml4_entry(ept_pml4e *entry, const struct ept_mapping_info *info)
{
    const phys_addr_t ept_pdpt_ptr = __allocate_ept_paging_structure();
    if (!ept_pdpt_ptr) {
        pr_error("Failed to allocate memory for a ept pdpt table");
        return -1;
    }

    ept_pml4e_set_paddr(entry, ept_pdpt_ptr);
    entry->raw |= info->no_page_flags;

    return 0;
}

static int
do_ept_pt_mapping(
        const virt_addr_t pt_table_ptr,
        const gpaddr guest_start,
        const gpaddr guest_end,
        const struct ept_mapping_info *info
) {
    int block_index;
    gpaddr curr_start, curr_end;
    ept_pte *entry;

    curr_start = guest_start;

    while (curr_start <= guest_end) {
        curr_end = MIN(guest_end, get_ept_pt_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pt_index(curr_start));

        entry = (ept_pte *) (pt_table_ptr + block_index);

        if (is_ept_entry_present(entry->raw)) {
            pr_warn("Page for addr range (0x%lx, 0x%lx) is already mapped",
                    curr_start,
                    curr_end
            );
            return EPT_RANGE_ALREADY_MAPPED;
        }

        ept_pte_set_paddr(entry, curr_start + info->offset);
        entry->raw |= info->page_map_flags;

        curr_start = curr_end + 1;
    }

    return EPT_SUCCESS;
}

static int
do_ept_pd_mapping(
        const virt_addr_t pd_table_ptr,
        const gpaddr guest_start,
        const gpaddr guest_end,
        const struct ept_mapping_info *info
) {
    int ret, block_index;
    gpaddr curr_start, curr_end;
    ept_pde *entry;
    bool is_present;
    virt_addr_t pt_ptr;

    curr_start = guest_start;

    while (curr_start <= guest_end) {
        curr_end = MIN(guest_end, get_ept_pd_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pd_index(curr_start));

        entry = (ept_pde *) (pd_table_ptr + block_index);
        is_present = is_ept_entry_present(entry->raw);

        if (is_present && entry->pde_page.maps_page) {
            pr_warn("Range (0x%lx, 0x%lx) is already being mapped by a mb page",
                    curr_start,
                    curr_end
            );
            return EPT_RANGE_ALREADY_MAPPED;
        } else if (is_present && should_do_mb_mapping(curr_start, curr_end, info)) {
            pr_warn("Parts of the range between (0x%lx, 0x%lx) are already being mapped",
                    curr_start,
                    curr_end
            );
            return EPT_PARTS_OF_RANGE_ALREADY_MAPPED;
        } else if (!is_present && should_do_mb_mapping(curr_start, curr_end, info)) {
            pr_debug("Create a MB page for range (0x%lx, 0x%lx)", curr_start, curr_end);
            ept_pde_set_pf_paddr(entry, curr_start + info->offset);
            entry->raw |= info->page_map_flags;
            goto do_next_block;
        }

        if (!is_present) {
            if (__init_pd_table_entry(entry, info) != 0) {
                pr_error("Failed to init pd entry at index '%lu'", block_index);
                return EPT_ERROR;
            }
        }

        pt_ptr = phys_to_virt(ept_pde_read_paddr(entry));
        ret = do_ept_pt_mapping(pt_ptr, curr_start, curr_end, info);
        if (ret != EPT_SUCCESS) {
            pr_error("Mapping range (0x%lx, 0x%lx) in pd entry at index '%lu' failed",
                    curr_start,
                    curr_end,
                    block_index
            );
            return ret;
        }

do_next_block:
        curr_start = curr_end + 1;
    }

    return EPT_SUCCESS;
}

static int
do_ept_pdpt_mapping(
        const virt_addr_t pdpt,
        const gpaddr guest_start,
        const gpaddr guest_end,
        const struct ept_mapping_info *info
) {
    int ret, block_index;
    gpaddr curr_start, curr_end;
    bool is_present;
    ept_pdpte *entry;
    virt_addr_t pd_ptr;

    curr_start = guest_start;

    while (curr_start <= guest_end) {
        curr_end = MIN(guest_end, get_ept_pdpte_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pdpt_index(curr_start));

        entry = (ept_pdpte *) (pdpt + block_index);
        is_present = is_ept_entry_present(entry->raw);

        if (is_present && entry->pdpte_page.maps_page) {
            pr_warn("Range (0x%lx, 0x%lx) is already being mapped by a gb page",
                    curr_start,
                    curr_end
            );
            return EPT_RANGE_ALREADY_MAPPED;
        } else if (is_present && should_do_gb_mapping(curr_start, curr_end, info)) {
            pr_warn("Parts of the range between (0x%lx, 0x%lx) is already being mapped",
                    curr_start,
                    curr_end
            );
            return EPT_PARTS_OF_RANGE_ALREADY_MAPPED;
        } else if (!is_present && should_do_gb_mapping(curr_start, curr_end, info)) {
            pr_debug("Create GB page for range (0x%lx, 0x%lx)", curr_start, curr_end);
            ept_pdpte_set_pf_paddr(entry, curr_start + info->offset);
            entry->raw |= info->page_map_flags;
            goto do_next_block;
        }

        if (!is_present) {
            if (__init_pdpt_entry(entry, info) != 0) {
                pr_error("Failed to init pdpt entry at index '%lu'", block_index);
                return EPT_ERROR;
            }
        }

        pd_ptr = phys_to_virt(ept_pdpte_read_paddr(entry));
        ret = do_ept_pd_mapping(pd_ptr, curr_start, curr_end, info);
        if (ret != EPT_SUCCESS) {
            pr_error("Mapping range (0x%lx, 0x%lx) in pdpt entry at index '%lu' failed",
                    curr_start,
                    curr_end,
                    block_index
            );
            return ret;
        }

do_next_block:
        curr_start = curr_end + 1;
    }

    return EPT_SUCCESS;
}

static int
do_ept_pml4_mapping(
        const virt_addr_t pml4,
        const gpaddr guest_paddr_start,
        const gpaddr guest_paddr_end,
        const struct ept_mapping_info *info
) {
    int ret, block_index;
    gpaddr curr_start, curr_end;
    ept_pml4e *entry;
    virt_addr_t pdpt_ptr;

    curr_start = guest_paddr_start;

    while (curr_start <= guest_paddr_end) {
        curr_end = MIN(guest_paddr_end, get_ept_pml4e_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pml4_index(curr_start));

        entry = (ept_pml4e *) (pml4 + block_index);
        if (!is_ept_entry_present(entry->raw)) {
            if ((ret = __init_pml4_entry(entry, info)) != 0) {
                pr_error("Failed to init ept pml4 entry for index '%lu'",
                        block_index
                );
                return EPT_ERROR;
            }
        }

        pdpt_ptr = phys_to_virt(ept_pml4e_read_paddr(entry));
        ret = do_ept_pdpt_mapping(pdpt_ptr, curr_start, curr_end, info);
        if (ret != EPT_SUCCESS) {
            pr_error("Mapping range (0x%lx, 0x%lx) in pml4 entry at index '%lu' failed",
                    curr_start,
                    curr_end,
                    block_index
            );
            return ret;
        }

        curr_start = curr_end + 1;
    }

    return EPT_SUCCESS;
}

int
add_ept_mapping(const eptp_t *eptp, const struct ept_mapping_info *info)
{
    const virt_addr_t pml4_ptr = phys_to_virt(eptp_read_paddr(eptp));
    const gpaddr gpa_start = PAGE_ALIGN(info->guest_paddr_start);
    const gpaddr gpa_end = info->guest_paddr_start + info ->req_bytes;

    return do_ept_pml4_mapping(pml4_ptr, gpa_start, gpa_end, info);
}

int
create_ept_mapping(eptp_t *eptp, const struct ept_mapping_info *info)
{
    phys_addr_t pml4_ptr;

    if (!eptp) {
        pr_error("Cannot create eptp mapping as ept ptr is null");
        return EPT_ERROR;
    }

    eptp->raw = 0;
    eptp->mem_type = EPT_MEM_TYPE_WB;
    eptp->page_walk_len = EPT_PAGE_WALK_LEN_4;

    pml4_ptr = __allocate_ept_paging_structure();
    if (!pml4_ptr) {
        pr_error("Failed to allocate a pml4 table for the eptp");
        return EPT_ERROR;
    }

    eptp_set_paddr(eptp, pml4_ptr);

    return add_ept_mapping(eptp, info);
}

