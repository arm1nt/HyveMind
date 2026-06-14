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

