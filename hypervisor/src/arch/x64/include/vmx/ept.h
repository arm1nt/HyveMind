#ifndef _HYVEMIND_X64_VMX_EPT_H
#define _HYVEMIND_X64_VMX_EPT_H

#include "string.h"
#include "asm/cpufeatures.h"
#include "asm/processor.h"
#include "vmx/ept_types.h"

#define EPT_PML4_INDEX_VADDR_START_POS  39
#define EPT_PML4_INDEX_VADDR_END_POS    47
#define EPT_PDPT_INDEX_VADDR_START_POS  30
#define EPT_PDPT_INDEX_VADDR_END_POS    38
#define EPT_PDT_INDEX_VADDR_START_POS   21
#define EPT_PDT_INDEX_VADDR_END_POS     29
#define EPT_PT_INDEX_VADDR_START_POS    12
#define EPT_PT_INDEX_VADDR_END_POS      20

static inline uint64_t
__get_ept_xtable_block_end(const uint64_t addr, const int mask_bits)
{
    const uint64_t block_end = addr | (U64_LSHIFT(1,  mask_bits) - 1);
    return block_end;
}

#define get_ept_pml4e_block_end(addr)   \
    __get_ept_xtable_block_end(addr, EPT_PML4_INDEX_VADDR_START_POS)
#define get_ept_pdpte_block_end(addr)   \
    __get_ept_xtable_block_end(addr, EPT_PDPT_INDEX_VADDR_START_POS)
#define get_ept_pd_block_end(addr)      \
    __get_ept_xtable_block_end(addr, EPT_PDT_INDEX_VADDR_START_POS)
#define get_ept_pt_block_end(addr)      \
    __get_ept_xtable_block_end(addr, EPT_PT_INDEX_VADDR_START_POS)

/**
 * @msb_pos ... bit index of the highest bit part of the index calculation
 */
static inline int
__get_ept_xentry_index(const uint64_t addr, const int msb_pos)
{
    return (addr << (63 - msb_pos)) >> 55;
}

#define get_ept_pml4_index(addr)    \
    __get_ept_xentry_index(addr, EPT_PML4_INDEX_VADDR_END_POS)
#define get_ept_pdpt_index(addr)    \
    __get_ept_xentry_index(addr, EPT_PDPT_INDEX_VADDR_END_POS)
#define get_ept_pd_index(addr)      \
    __get_ept_xentry_index(addr, EPT_PDT_INDEX_VADDR_END_POS)
#define get_ept_pt_index(addr)      \
    __get_ept_xentry_index(addr, EPT_PT_INDEX_VADDR_END_POS)

struct ept_mapping_info {
    gpaddr guest_paddr_start;
    uint64_t req_bytes;
    int64_t offset;

    uint64_t no_page_flags;
    uint64_t page_map_flags;

    bool use_gb_mappings;
    bool use_mb_mappings;
};

static inline struct ept_mapping_info
create_ept_mapping_info(
        const gpaddr guest_paddr_start,
        const uint64_t req_bytes,
        const phys_addr_t target_host_paddr
) {
    struct ept_mapping_info info;
    memset(&info, 0, sizeof(info));

    info.guest_paddr_start = guest_paddr_start;
    info.req_bytes = req_bytes;
    info.offset = target_host_paddr - guest_paddr_start;

    return info;
}

static inline void
try_use_gb_mappings(struct ept_mapping_info *info)
{
    bool supported = false;
    const uint64_t ept_cap_msr = read_msr(MSRX64_IA32_VMX_EPT_VPID_CAP);

    if (IS_SET(ept_cap_msr, U64_LSHIFT(1, EPT_VPID_CAP_GB_PAGES_BIT))) {
        supported = true;
    }

    info->use_gb_mappings = supported;
}

static inline void
try_use_mb_mappings(struct ept_mapping_info *info)
{
    bool supported = false;
    const uint64_t ept_cap_msr = read_msr(MSRX64_IA32_VMX_EPT_VPID_CAP);

    if (IS_SET(ept_cap_msr, U64_LSHIFT(1, EPT_VPID_CAP_MB_PAGES_BIT))) {
        supported = true;
    }

    info->use_mb_mappings = supported;
}

enum ept_error {
    EPT_SUCCESS,
    EPT_RANGE_ALREADY_MAPPED,
    EPT_PARTS_OF_RANGE_ALREADY_MAPPED,
    EPT_ERROR,
};

int create_ept_mapping(eptp_t *eptp, const struct ept_mapping_info *info);
int add_ept_mapping(const eptp_t *eptp, const struct ept_mapping_info *info);

#endif /* _HYVEMIND_X64_VMX_EPT_H */

