#ifndef _HYVEMIND_X64_VMX_EPT_H
#define _HYVEMIND_X64_VMX_EPT_H

#include "string.h"
#include "vmx/ept_types.h"

static inline uint64_t
__get_ept_xtable_block_end(const uint64_t addr, const int mask_bits)
{
    const uint64_t block_end = addr | (U64_LSHIFT(1,  mask_bits) - 1);
    return block_end;
}

/* todo: define macros for the mask bit nrs */
#define get_ept_pml4e_block_end(addr)   __get_ept_xtable_block_end(addr, 39)
#define get_ept_pdpte_block_end(addr)   __get_ept_xtable_block_end(addr, 30)
#define get_ept_pd_block_end(addr)     __get_ept_xtable_block_end(addr, 21)
#define get_ept_pt_block_end(addr)     __get_ept_xtable_block_end(addr, 12)

/**
 * @msb_pos ... bit index of the highest bit part of the index calculation
 */
static inline int
__get_ept_xentry_index(const uint64_t addr, const int msb_pos)
{
    return (addr << (63 - msb_pos)) >> 55;
}

/* also define macros for the nrs */
#define get_ept_pml4_index(addr)    __get_ept_xentry_index(addr, 47)
#define get_ept_pdpt_index(addr)    __get_ept_xentry_index(addr, 38)
#define get_ept_pd_index(addr)      __get_ept_xentry_index(addr, 29)
#define get_ept_pt_index(addr)      __get_ept_xentry_index(addr, 20)

struct ept_mapping_info {
    gpaddr guest_paddr_start;
    uint64_t req_bytes;
    int64_t offset;

    uint64_t pml4e_flags;
    uint64_t pdpte_flags;
    uint64_t pde_flags;
    uint64_t pte_flags;

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
    /**
     * TODO: Check if gb mappings are supported
     */

    info->use_gb_mappings = false;
}

static inline void
try_use_mb_mappings(struct ept_mapping_info *info)
{
    /**
     * TODO: check if mb mappings are supported
     */
    info->use_mb_mappings = false;
}


int create_ept_mapping(eptp_t *eptp, struct ept_mapping_info *info);

#endif /* _HYVEMIND_X64_VMX_EPT_H */

