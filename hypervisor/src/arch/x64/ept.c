#include "pf_alloc.h"
#include "printf.h"
#include "string.h"
#include "asm/paging.h"
#include "vmx/ept.h"

static inline phys_addr_t
__allocate_ept_paging_structure(void)
{
    phys_addr_t ept_table_ptr;
    if (get_page_raw(&ept_table_ptr) != 0) {
        pr_warn("Failed to allocate page for ept paging structure");
        return 0;
    }

    memset((void *) __vaddr(ept_table_ptr), 0, PAGE_SIZE);
    return ept_table_ptr;
}

static inline int
__init_pml4_entry(ept_pml4e *entry)
{
    phys_addr_t ept_pdpt_ptr = __allocate_ept_paging_structure();
    if (!ept_pdpt_ptr) {
        pr_error("Failed to allocate memory for a ept pdpt table");
        return -1;
    }

    ept_pml4e_set_paddr(entry, ept_pdpt_ptr);
    entry->read_access = 1;
    entry->write_access = 1;
    entry->execute_access = 1;

    return 0;
}

int
do_ept_pdpt_mapping(
        const virt_addr_t pdpt,
        const gpaddr guest_start,
        const gpaddr guest_end,
        const struct ept_mapping_info *info
)
{
    return -1;
}

int
do_ept_pml4_mapping(
        const virt_addr_t pml4,
        const gpaddr guest_paddr_start,
        const gpaddr guest_paddr_end,
        const struct ept_mapping_info *info
)
{
    int ret = 0;
    gpaddr curr_start = guest_paddr_start;
    gpaddr curr_end;

    while (curr_start <= guest_paddr_end) {
        curr_end = MIN(guest_paddr_end, get_ept_pml4e_block_end(curr_start));

        const int block_index = get_ept_pml4_index(curr_start);

        ept_pml4e *entry = (ept_pml4e *) (pml4 + (block_index << 3));

        if (!is_ept_entry_present(entry->raw)) {
            ret = __init_pml4_entry(entry);
            if (ret != 0) {
                pr_error("Failed to initialize pml4 entry at index '%lu'", block_index);
                return ret;
            }
        }

        const virt_addr_t pdpt_ptr = __vaddr(entry->paddr >> PAGE_SHIFT);
        ret = do_ept_pdpt_mapping(pdpt_ptr, curr_start, curr_end, info);
        if (ret != 0) {
            pr_warn("Failed to map range (%lx, %lx) in pdpt", curr_start, curr_end);
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
            __vaddr(ept_pml4_ptr),
            PAGE_ALIGN(info->guest_paddr_start),
            info->guest_paddr_start + info->req_bytes,
            info
    );
}

/* todo: prob. move into vm.c */
void
phys_copy_to_vm(
        const struct vm_addr_space *vm_addr_space,
        const gpaddr guest_paddr,
        const void *value,
        const uint64_t value_size
)
{
}

void
copy_to_vm(
        const struct vm_addr_space *vm_addr_space,
        const gvaddr guest_vaddr,
        const void *value,
        const uint64_t value_size
)
{
}

