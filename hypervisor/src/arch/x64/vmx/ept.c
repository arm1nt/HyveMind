#include "fatal.h"
#include "pf_alloc.h"
#include "printf.h"
#include "string.h"
#include "asm/paging.h"
#include "vmx/ept.h"

#define SCALE_BLOCK_INDEX(raw_index) ((raw_index) << 3)

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
    entry->raw |= info->pde_flags;

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
    entry->raw |= info->pdpte_flags;

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
    entry->raw |= info->pml4e_flags;

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

    curr_start = guest_start;

    while (curr_start <= guest_end) {
        curr_end = MIN(guest_end, get_ept_pt_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pt_index(curr_start));

        ept_pte *entry = (ept_pte *) (pt_table_ptr + block_index);

        if (!is_ept_entry_present(entry->raw)) {
            ept_pte_set_paddr(entry, curr_start + info->offset);
            entry->raw |= info->pte_flags;
            goto ept_pt_table_prepare_next_block;
        }

        die_reason("Unreachable");

ept_pt_table_prepare_next_block:
        curr_start = curr_end + 1;
    }

    return 0;
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

    curr_start = guest_start;

    while (curr_start <= guest_end) {
        curr_end = MIN(guest_end, get_ept_pd_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pd_index(curr_start));

        ept_pde *entry = (ept_pde *) (pd_table_ptr + block_index);

        if (!is_ept_entry_present(entry->raw) && info->use_mb_mappings) {
            NOT_YET_IMPLEMENTED;
        }

        if (!is_ept_entry_present(entry->raw)) {
            ret = __init_pd_table_entry(entry, info);
            if (ret != 0) {
                pr_error("Failed to initialize ept pd entry at index '%lu'", block_index);
                return -1;
            }

            ret = do_ept_pt_mapping(
                    phys_to_virt(ept_pde_read_paddr(entry)),
                    curr_start,
                    curr_end,
                    info
            );

            if (ret != 0) {
                pr_error(
                        "Mapping range (%lx, %lx) in pd entry at index '%lu' failed",
                        curr_start,
                        curr_end,
                        block_index
                );
                return -1;
            }

            goto ept_pd_table_prepare_next_block;
        }

        /**
         * Again, current assumption rn is that there is no existing mapping
         */
        die_reason("Unreachable");

ept_pd_table_prepare_next_block:
        curr_start = curr_end + 1;
    }

    return 0;
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

    curr_start = guest_start;

    while (curr_start <= guest_end) {
        curr_end = MIN(guest_end, get_ept_pdpte_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pdpt_index(curr_start));

        ept_pdpte *entry = (ept_pdpte *) (pdpt + block_index);

        if (!is_ept_entry_present(entry->raw) && info->use_gb_mappings) {
            /* Create a gb mapping */
            NOT_YET_IMPLEMENTED;
        }

        if (!is_ept_entry_present(entry->raw)) {
            /* Descend into the pd table referenced by the pdpt entry */

            ret = __init_pdpt_entry(entry, info);
            if (ret != 0) {
                pr_error("Failed to initialize ept pdpt entry at index: %lu", block_index);
                return -1;
            }

            ret = do_ept_pd_mapping(
                    phys_to_virt(ept_pdpte_read_paddr(entry)),
                    curr_start,
                    curr_end,
                    info
            );

            if (ret != 0) {
                pr_error(
                        "Mapping range (%lx, %lx) in pdpt entry at index '%lu' failed",
                        curr_start,
                        curr_end,
                        block_index
                );
                return -1;
            }

            goto ept_pdpt_prepare_next_block;
        }

        /**
         * The assumption currently is that we never encounter an already
         * present mapping. So this point should be unreachable.
         */
        die_reason("Unreachable");

ept_pdpt_prepare_next_block:
        curr_start = curr_end + 1;
    }

    return 0;
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

    curr_start = guest_paddr_start;

    while (curr_start <= guest_paddr_end) {
        curr_end = MIN(guest_paddr_end, get_ept_pml4e_block_end(curr_start));
        block_index = SCALE_BLOCK_INDEX(get_ept_pml4_index(curr_start));

        ept_pml4e *entry = (ept_pml4e *) (pml4 + block_index);
        if (!is_ept_entry_present(entry->raw)) {
            ret = __init_pml4_entry(entry, info);
            if (ret != 0) {
                pr_error("Failed to initialize ept pml4 entry for index '%lu'", block_index);
                return -1;
            }
        }

        ret = do_ept_pdpt_mapping(
                phys_to_virt(ept_pml4e_read_paddr(entry)),
                curr_start,
                curr_end,
                info
        );

        if (ret != 0) {
            pr_error(
                    "Mapping range (%lx, %lx) in pml4 entry at index '%lu' failed",
                    curr_start,
                    curr_end,
                    block_index
            );
            return -1;
        }

        curr_start = curr_end + 1;
    }

    return 0;
}

int
create_ept_mapping(eptp_t *eptp, struct ept_mapping_info *info)
{
    eptp->raw = 0;

    eptp->mem_type = EPT_MEM_TYPE_WB;
    eptp->page_walk_len = EPT_PAGE_WALK_LEN_4;

    const phys_addr_t ept_pml4_ptr = __allocate_ept_paging_structure();
    if (!ept_pml4_ptr) {
        pr_error("Failed to allocate ept pml4 table");
        return -1;
    }

    eptp_set_paddr(eptp, ept_pml4_ptr);

    return do_ept_pml4_mapping(
            phys_to_virt(eptp_read_paddr(eptp)),
            PAGE_ALIGN(info->guest_paddr_start),
            info->guest_paddr_start + info->req_bytes,
            info
    );
}

