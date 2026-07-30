#ifndef _HYVEMIND_X64_VMX_EPT_TYPES_H
#define _HYVEMIND_X64_VMX_EPT_TYPES_H

#include "fatal.h"
#include "hyvstdlib.h"
#include "mm_types.h"
#include "types.h"
#include "asm/paging.h"
#include "asm/mm.h"

#define EPT_MEM_TYPE_UC (0)
#define EPT_MEM_TYPE_WB (6)

#define EPT_PAGE_WALK_LEN_4 (3)
#define EPT_PAGE_WALK_LEN_5 (4)

/* prob. move into vm.h header */
struct vm_addr_space {
    /**
     * We could also use non-contigous chunks instead of one large contigous
     * area.
     */
    virt_addr_t __directly_mapped host_start;
    virt_addr_t __directly_mapped host_end;
};

union eptp {
    uint64_t raw;
    struct {
        uint64_t mem_type                               : 3,
                 page_walk_len                          : 3,
                 enable_accessed_dirty_flags            : 1,
                 enforce_supervisor_ssp_access_rights   : 1,
                 reserved0                              : 4,
                 paddr                                  : 40,
                 reserved1                              : 12;
    };
};
typedef union eptp eptp_t;

union ept_entry_maps_page {
    uint64_t raw;
    struct {
        uint64_t read_access                    : 1,
                 write_access                   : 1,
                 execute_access                 : 1,
                 ept_mem_type                   : 3,
                 ignore_pat_type                : 1,
                 maps_page                      : 1, /* must be 1 */
                 accessed                       : 1,
                 dirty                          : 1,
                 user_mode_execute_access       : 1,
                 reserved0                      : 1,
                 paddr                          : 40,
                 reserved1                      : 5,
                 vrfy_guest_paging              : 1,
                 paging_write_access            : 1,
                 reserved2                      : 1,
                 supervisor_shadow_stack        : 1,
                 reserved3                      : 2,
                 suppress_ve                    : 1;
    };
};

union ept_entry_no_page {
    uint64_t raw;
    struct {
        uint64_t read_access                : 1,
                 write_access               : 1,
                 execute_access             : 1,
                 reserved0                  : 5,
                 accessed                   : 1,
                 reserved1                  : 1,
                 user_mode_execute_access   : 1,
                 reserved2                  : 1,
                 paddr                      : 40,
                 reserved3                  : 12;
    };
};

typedef union ept_entry_no_page ept_pml4e;

typedef union ept_entry_no_page ept_pdpte_no_page;
typedef union ept_entry_maps_page ept_pdpte_maps_page;

union ept_pdpt_entry {
    uint64_t raw;
    ept_pdpte_maps_page pdpte_page;
    ept_pdpte_no_page pdpte_no_page;
};
typedef union ept_pdpt_entry ept_pdpte;

typedef union ept_entry_no_page ept_pde_no_page;
typedef union ept_entry_maps_page ept_pde_maps_page;

union ept_pd_entry {
    uint64_t raw;
    ept_pde_maps_page pde_page;
    ept_pde_no_page pde_no_page;
};
typedef union ept_pd_entry ept_pde;

union ept_pt_entry {
    uint64_t raw;
    struct {
        uint64_t read_access                : 1,
                 write_access               : 1,
                 execute_access             : 1,
                 ept_mem_type               : 3,
                 ignore_pat_type            : 1,
                 ignored0                   : 1,
                 accessed                   : 1,
                 dirty                      : 1,
                 user_mode_execute_access   : 1,
                 ignored1                   : 1,
                 paddr                      : 40,
                 ignored2                   : 5,
                 vrfy_guest_paging          : 1,
                 paging_write_access        : 1,
                 ignored3                   : 1,
                 supervisor_shadow_stack    : 1,
                 sub_page_write_permissions : 1,
                 ignored4                   : 1,
                 suppress_ve                : 1;
    };
};
typedef union ept_pt_entry ept_pte;

static inline void
eptp_set_paddr(eptp_t *eptp, const phys_addr_t paddr)
{
    eptp->paddr = (paddr >> PAGE_SHIFT) & MAX_PHYS_ADDR;
}

static inline void
ept_pml4e_set_paddr(ept_pml4e *pml4e, const phys_addr_t paddr)
{
    pml4e->paddr = (paddr >> PAGE_SHIFT) & MAX_PHYS_ADDR;
}

static inline void
ept_pdpte_set_pf_paddr(ept_pdpte *pdpte, const phys_addr_t paddr)
{
    die_reason("ept pdpte set pf paddr not yet implemented");
}

static inline void
ept_pdpte_set_paddr(ept_pdpte *pdpte, const phys_addr_t paddr)
{
    pdpte->pdpte_no_page.paddr = (paddr >> PAGE_SHIFT) & MAX_PHYS_ADDR;
}

static inline void
ept_pde_set_pf_paddr(ept_pde *pde, const phys_addr_t paddr)
{
    die_reason("ept pde set pf paddr not yet implemented");
}

static inline void
ept_pde_set_paddr(ept_pde *pde, const phys_addr_t paddr)
{
    pde->pde_no_page.paddr = (paddr >> PAGE_SHIFT) & MAX_PHYS_ADDR;
}

static inline void
ept_pte_set_paddr(ept_pte *pte, const phys_addr_t paddr)
{
    pte->paddr = (paddr >> PAGE_SHIFT) & MAX_PHYS_ADDR;
}

static inline phys_addr_t
eptp_read_paddr(const eptp_t *eptp)
{
    return (eptp->paddr) << PAGE_SHIFT;
}

static inline phys_addr_t
ept_pml4e_read_paddr(const ept_pml4e *pml4e)
{
    return (pml4e->paddr) << PAGE_SHIFT;
}

static inline phys_addr_t
ept_pdpte_read_paddr(const ept_pdpte *pdpte)
{
    return (pdpte->pdpte_no_page.paddr)  << PAGE_SHIFT;
}

static inline phys_addr_t
ept_pdpte_read_pf_paddr(const ept_pdpte *pdpte)
{
    die_reason("ept pdpte read pf addr not yet implemented");
    return 0;
}

static inline phys_addr_t
ept_pde_read_paddr(const ept_pde *pde)
{
    return (pde->pde_no_page.paddr) << PAGE_SHIFT;
}

static inline phys_addr_t
ept_pde_read_pf_paddr(const ept_pde *pde)
{
    die_reason("ept pde read pf paddr not yet implemented");
    return 0;
}

static inline bool
is_ept_entry_present(const uint64_t raw_entry)
{
    /**
     * If any of the bits 2:0 is 1 or (if the mode-based execution control
     * is set) if bit 10 is 1, then the entry is present.
     */

    const bool lower_bits_zero = ((raw_entry & 7) == 0);
    const bool bit10_set = IS_SET(raw_entry, U64_LSHIFT(1,10));

    return (!lower_bits_zero) || bit10_set;
}

#define EPT_READ_ACCS_FLAG      (U64_LSHIFT(1, 0))
#define EPT_WRITE_ACCS_FLAG     (U64_LSHIFT(1, 1))
#define EPT_RW_FLAG             (EPT_READ_ACCS_FLAG | EPT_WRITE_ACCS_FLAG)
#define EPT_EXECUTE_ACCS_FLAG   (U64_LSHIFT(1, 2))
#define EPT_MEM_TYPE_WB_FLAG    (U64_LSHIFT(6, 3))

#endif /* _HYVEMIND_X64_VMX_EPT_TYPES_H */

