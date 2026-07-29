#ifndef _HYVEMIND_X64_VMX_EPT_H
#define _HYVEMIND_X64_VMX_EPT_H

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
#define get_ept_pde_block_end(addr)     __get_ept_xtable_block_end(addr, 21)
#define get_ept_pte_block_end(addr)     __get_ept_xtable_block_end(addr, 12)

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
};

static inline struct ept_mapping_info
create_ept_mapping_info(
        const gpaddr guest_paddr_start,
        const uint64_t req_bytes,
        const phys_addr_t target_host_paddr
) {
    struct ept_mapping_info info;

    info.guest_paddr_start = guest_paddr_start;
    info.req_bytes = req_bytes;

    info.offset = target_host_paddr - guest_paddr_start;

    return info;
}

int create_ept_mapping(eptp_t *eptp, struct ept_mapping_info *info);

#endif /* _HYVEMIND_X64_VMX_EPT_H */

